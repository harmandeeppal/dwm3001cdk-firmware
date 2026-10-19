#include <zephyr/bluetooth/bluetooth.h>
#include <errno.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

#include "ble_motion_transport.h"
#include "ble_telemetry_protocol.h"

#ifndef UWB_MESH_NODE_INDEX
#error "Combined firmware requires UWB_MESH_NODE_INDEX"
#endif

#define COMP971_SERVICE_UUID_VAL \
	BT_UUID_128_ENCODE(0x6e2fd6a4, 0x6a4c, 0x4b64, 0xb389, 0x139718b02410)
#define COMP971_IMU_UUID_VAL \
	BT_UUID_128_ENCODE(0x6e2fd6a5, 0x6a4c, 0x4b64, 0xb389, 0x139718b02410)
#define COMP971_HEARTBEAT_UUID_VAL \
	BT_UUID_128_ENCODE(0x6e2fd6a6, 0x6a4c, 0x4b64, 0xb389, 0x139718b02410)
#define NODE_STATUS_PERIOD_MS 1000U

static struct bt_uuid_128 service_uuid = BT_UUID_INIT_128(COMP971_SERVICE_UUID_VAL);
static struct bt_uuid_128 telemetry_uuid = BT_UUID_INIT_128(COMP971_IMU_UUID_VAL);
static struct bt_uuid_128 heartbeat_uuid = BT_UUID_INIT_128(COMP971_HEARTBEAT_UUID_VAL);
static struct bt_conn *gateway_connection;
static int64_t last_range_report_ms[15];
static int64_t last_gateway_heartbeat_ms;
static int64_t last_node_status_report_ms;
static struct k_work_delayable advertising_recovery;

/* These counters are deliberately independent of printk.  A status packet
 * makes dropped queue entries and failed BLE notification enqueues visible to
 * the laptop collector instead of silently hiding loss. */
static atomic_t imu_samples_acquired;
static atomic_t imu_queue_put_ok;
static atomic_t imu_queue_dropped;
static atomic_t imu_queue_high_water;
static atomic_t ble_imu_notify_ok;
static atomic_t ble_imu_notify_failed;
static atomic_t ble_uwb_notify_failed;
static atomic_t uwb_range_attempts;
static atomic_t uwb_range_valid;
static atomic_t uwb_range_invalid;
static atomic_t uwb_result_timeout;

K_MSGQ_DEFINE(imu_queue, sizeof(struct icm20948_sample), 36, 4);
K_THREAD_STACK_DEFINE(ble_worker_stack, 1536);
static struct k_thread ble_worker_thread;

static ssize_t gateway_heartbeat_write(struct bt_conn *connection,
	const struct bt_gatt_attr *attr, const void *buffer, uint16_t length,
	uint16_t offset, uint8_t flags)
{
	ARG_UNUSED(connection); ARG_UNUSED(attr); ARG_UNUSED(buffer); ARG_UNUSED(flags);
	if (offset != 0U || length != 1U) { return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN); }
	last_gateway_heartbeat_ms = k_uptime_get();
	return length;
}

BT_GATT_SERVICE_DEFINE(comp971_motion_service,
	BT_GATT_PRIMARY_SERVICE(&service_uuid.uuid),
	BT_GATT_CHARACTERISTIC(&telemetry_uuid.uuid, BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_NONE, NULL, NULL, NULL),
	BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	/* The central writes one byte each second. It allows this peripheral to
	 * detect a dead central even when a stale BLE link never raises a normal
	 * disconnect callback. */
	BT_GATT_CHARACTERISTIC(&heartbeat_uuid.uuid, BT_GATT_CHRC_WRITE_WITHOUT_RESP,
		BT_GATT_PERM_WRITE, NULL, gateway_heartbeat_write, NULL)
);

static const struct bt_data advertising_data[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
		sizeof(CONFIG_BT_DEVICE_NAME) - 1U),
};

/* Restart advertising after a normal disconnect. A one-shot advertising start
 * can fail while the controller is still finishing the old connection. Retry
 * until advertising is active; this needs no user reset or flash write. */
static void ensure_connectable_advertising(void)
{
	int result;

	if (gateway_connection != NULL) {
		return;
	}

	result = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, advertising_data,
		ARRAY_SIZE(advertising_data), NULL, 0U);
	if (result != 0 && result != -EALREADY) {
		(void)k_work_schedule(&advertising_recovery, K_SECONDS(1));
	}
}

static void advertising_recovery_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	ensure_connectable_advertising();
}

static void copy_sample(struct ble_imu_sample_v1 *out,
	const struct icm20948_sample *in)
{
	out->sequence = in->sequence;
	out->local_timestamp_us = in->timestamp_us;
	out->ax = in->ax; out->ay = in->ay; out->az = in->az;
	out->gx = in->gx; out->gy = in->gy; out->gz = in->gz;
	out->temperature = in->temperature;
}

static void update_queue_high_water(uint32_t depth)
{
	atomic_val_t observed = atomic_get(&imu_queue_high_water);

	while (depth > (uint32_t)observed) {
		if (atomic_cas(&imu_queue_high_water, observed, (atomic_val_t)depth)) {
			break;
		}
		observed = atomic_get(&imu_queue_high_water);
	}
}

static void publish_node_status(void)
{
	struct ble_node_status_v1 status = {
		.protocol_version = BLE_TELEMETRY_PROTOCOL_VERSION,
		.message_type = BLE_TELEMETRY_NODE_STATUS,
		.node_index = UWB_MESH_NODE_INDEX,
		.flags = 0U,
		.sync_epoch = 0U,
		.imu_samples_acquired = (uint32_t)atomic_get(&imu_samples_acquired),
		.imu_queue_put_ok = (uint32_t)atomic_get(&imu_queue_put_ok),
		.imu_queue_dropped = (uint32_t)atomic_get(&imu_queue_dropped),
		.imu_queue_high_water = (uint32_t)atomic_get(&imu_queue_high_water),
		.ble_imu_notify_ok = (uint32_t)atomic_get(&ble_imu_notify_ok),
		.ble_imu_notify_failed = (uint32_t)atomic_get(&ble_imu_notify_failed),
		.ble_uwb_notify_failed = (uint32_t)atomic_get(&ble_uwb_notify_failed),
		.uwb_range_attempts = (uint32_t)atomic_get(&uwb_range_attempts),
		.uwb_range_valid = (uint32_t)atomic_get(&uwb_range_valid),
		.uwb_range_invalid = (uint32_t)atomic_get(&uwb_range_invalid),
		.uwb_result_timeout = (uint32_t)atomic_get(&uwb_result_timeout),
	};

	if (gateway_connection == NULL ||
		(sizeof(status) + BLE_ATT_NOTIFICATION_OVERHEAD) >
		bt_gatt_get_mtu(gateway_connection)) {
		return;
	}
	(void)bt_gatt_notify(gateway_connection, &comp971_motion_service.attrs[2],
		&status, sizeof(status));
}

static void connected(struct bt_conn *connection, uint8_t error)
{
	if (error == 0U && gateway_connection == NULL) {
		gateway_connection = bt_conn_ref(connection);
		last_gateway_heartbeat_ms = k_uptime_get();
	}
}

static void disconnected(struct bt_conn *connection, uint8_t reason)
{
	ARG_UNUSED(reason);
	if (gateway_connection == connection) {
		bt_conn_unref(gateway_connection);
		gateway_connection = NULL;
		ensure_connectable_advertising();
	}
}

BT_CONN_CB_DEFINE(motion_connection_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

static void ble_worker(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
	while (true) {
		struct ble_imu_batch_v1 packet = {
			.protocol_version = BLE_TELEMETRY_PROTOCOL_VERSION,
			.message_type = BLE_TELEMETRY_IMU_BATCH,
			.node_index = UWB_MESH_NODE_INDEX,
			.sample_count = 0U,
			.sync_epoch = 0U,
		};
		struct icm20948_sample sample;
		/* Nine samples every 150 ms retains the 60 Hz source stream while
		 * reducing six-node BLE notification traffic by roughly three times. */
		k_msleep(150);
		while (packet.sample_count < BLE_IMU_BATCH_MAX_SAMPLES &&
			k_msgq_get(&imu_queue, &sample, K_NO_WAIT) == 0) {
			copy_sample(&packet.samples[packet.sample_count++], &sample);
		}
		if (packet.sample_count != 0U && gateway_connection != NULL) {
			uint16_t length = BLE_IMU_BATCH_WIRE_SIZE(packet.sample_count);
			int result;

			if ((length + BLE_ATT_NOTIFICATION_OVERHEAD) >
				bt_gatt_get_mtu(gateway_connection)) {
				atomic_inc(&ble_imu_notify_failed);
			} else {
				result = bt_gatt_notify(gateway_connection,
					&comp971_motion_service.attrs[2], &packet, length);
				if (result == 0) {
					atomic_inc(&ble_imu_notify_ok);
				} else {
					atomic_inc(&ble_imu_notify_failed);
				}
			}
		}
		if (gateway_connection != NULL &&
			(k_uptime_get() - last_node_status_report_ms) >= NODE_STATUS_PERIOD_MS) {
			last_node_status_report_ms = k_uptime_get();
			publish_node_status();
		}
	}
}

int motion_ble_start(void)
{
	int result = bt_enable(NULL);
	if (result != 0) { return result; }
	k_work_init_delayable(&advertising_recovery, advertising_recovery_handler);
	ensure_connectable_advertising();
	k_thread_create(&ble_worker_thread, ble_worker_stack,
		K_THREAD_STACK_SIZEOF(ble_worker_stack), ble_worker,
		NULL, NULL, NULL, 5, 0, K_NO_WAIT);
	return 0;
}

void motion_ble_queue_imu(const struct icm20948_sample *sample)
{
	int result;

	atomic_inc(&imu_samples_acquired);
	result = k_msgq_put(&imu_queue, sample, K_NO_WAIT);
	if (result == 0) {
		atomic_inc(&imu_queue_put_ok);
		update_queue_high_water(k_msgq_num_used_get(&imu_queue));
	} else {
		atomic_inc(&imu_queue_dropped);
	}
}

void motion_ble_record_uwb_result(int32_t range_cm, bool result_timeout)
{
	atomic_inc(&uwb_range_attempts);
	if (range_cm >= 0) {
		atomic_inc(&uwb_range_valid);
	} else {
		atomic_inc(&uwb_range_invalid);
		if (result_timeout) {
			atomic_inc(&uwb_result_timeout);
		}
	}
}

void motion_ble_publish_range(uint8_t pair_index, int32_t range_cm)
{
	if (UWB_MESH_NODE_INDEX != 1U || gateway_connection == NULL) { return; }
	if (pair_index >= ARRAY_SIZE(last_range_report_ms)) { return; }
	int64_t now_ms = k_uptime_get();
	/* Per-pair BLE report limits: valid ranges at most 2 Hz, invalid ranges at
	 * most 1 Hz. Since the 15 pairs are measured sequentially, the complete
	 * matrix update rate is lower than either per-pair limit. */
	int64_t minimum_period_ms = range_cm >= 0 ? 500 : 1000;
	if ((now_ms - last_range_report_ms[pair_index]) < minimum_period_ms) { return; }
	last_range_report_ms[pair_index] = now_ms;
	struct ble_uwb_range_v1 report = {
		.protocol_version = BLE_TELEMETRY_PROTOCOL_VERSION,
		.message_type = BLE_TELEMETRY_UWB_RANGE,
		.source_node = UWB_MESH_NODE_INDEX,
		.pair_index = pair_index,
		.pelvis_timestamp_us = (uint32_t)(now_ms * 1000LL),
		.range_cm = range_cm,
		.quality_flags = range_cm >= 0 ? BLE_TELEMETRY_UWB_VALID : 0U,
		.reserved = 0U,
	};
	if (bt_gatt_notify(gateway_connection, &comp971_motion_service.attrs[2],
		&report, sizeof(report)) != 0) {
		atomic_inc(&ble_uwb_notify_failed);
	}
}

void motion_ble_publish_uwb_diagnostic(uint8_t pair_index, uint8_t stage_code,
	int32_t detail)
{
	int result;
	struct ble_uwb_diagnostic_v1 report = {
		.protocol_version = BLE_TELEMETRY_PROTOCOL_VERSION,
		.message_type = BLE_TELEMETRY_UWB_DIAGNOSTIC,
		.node_index = UWB_MESH_NODE_INDEX,
		.pair_index = pair_index,
		.stage_code = stage_code,
		.reserved = { 0U, 0U, 0U },
		.local_timestamp_us = (uint32_t)(k_uptime_get() * 1000LL),
		.detail = detail,
	};

	if (gateway_connection == NULL) { return; }
	/* One short notification per failure event; successes remain unreported. */
	result = bt_gatt_notify(gateway_connection, &comp971_motion_service.attrs[2],
		&report, sizeof(report));
	if (result != 0) {
		atomic_inc(&ble_uwb_notify_failed);
	}
}
