/* One-board BLE IMU test for DWM-02-PELVIS. UWB is deliberately disabled. */
#include <string.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "ble_telemetry_protocol.h"
#include "icm20948_stream.h"

#define COMP971_SERVICE_UUID_VAL \
	BT_UUID_128_ENCODE(0x6e2fd6a4, 0x6a4c, 0x4b64, 0xb389, 0x139718b02410)
#define COMP971_IMU_UUID_VAL \
	BT_UUID_128_ENCODE(0x6e2fd6a5, 0x6a4c, 0x4b64, 0xb389, 0x139718b02410)

static struct bt_uuid_128 comp971_service_uuid = BT_UUID_INIT_128(COMP971_SERVICE_UUID_VAL);
static struct bt_uuid_128 comp971_imu_uuid = BT_UUID_INIT_128(COMP971_IMU_UUID_VAL);

K_MSGQ_DEFINE(imu_queue, sizeof(struct icm20948_sample), 24, 4);
static struct bt_conn *gateway_connection;

BT_GATT_SERVICE_DEFINE(comp971_service,
	BT_GATT_PRIMARY_SERVICE(&comp971_service_uuid.uuid),
	BT_GATT_CHARACTERISTIC(&comp971_imu_uuid.uuid,
		BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE, NULL, NULL, NULL),
	BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE)
);

static const struct bt_data advertising_data[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
		sizeof(CONFIG_BT_DEVICE_NAME) - 1U),
};

static void queue_icm_sample(const struct icm20948_sample *sample)
{
	if ((sample->sequence % 60U) == 0U) {
		printk("PELVIS_ICM_SAMPLE seq=%u ax=%d ay=%d az=%d gx=%d gy=%d gz=%d\n",
			sample->sequence, sample->ax, sample->ay, sample->az,
			sample->gx, sample->gy, sample->gz);
	}
	(void)k_msgq_put(&imu_queue, sample, K_NO_WAIT);
}

static void copy_sample(struct ble_imu_sample_v1 *destination,
			const struct icm20948_sample *source)
{
	destination->sequence = source->sequence;
	destination->local_timestamp_us = source->timestamp_us;
	destination->ax = source->ax;
	destination->ay = source->ay;
	destination->az = source->az;
	destination->gx = source->gx;
	destination->gy = source->gy;
	destination->gz = source->gz;
	destination->temperature = source->temperature;
}

static void gateway_connected(struct bt_conn *connection, uint8_t error)
{
	if (error == 0U && gateway_connection == NULL) {
		gateway_connection = bt_conn_ref(connection);
		printk("PELVIS_BLE_CONNECTED\n");
	}
}

static void gateway_disconnected(struct bt_conn *connection, uint8_t reason)
{
	ARG_UNUSED(reason);
	if (gateway_connection == connection) {
		bt_conn_unref(gateway_connection);
		gateway_connection = NULL;
		printk("PELVIS_BLE_DISCONNECTED\n");
	}
}

BT_CONN_CB_DEFINE(gateway_connection_callbacks) = {
	.connected = gateway_connected,
	.disconnected = gateway_disconnected,
};

int main(void)
{
	int result = bt_enable(NULL);
	if (result != 0) {
		printk("PELVIS_BLE_INIT_ERROR=%d\n", result);
		return result;
	}
	result = icm20948_start(queue_icm_sample);
	if (result != 0) {
		printk("PELVIS_ICM_INIT_ERROR=%d\n", result);
		return result;
	}
	printk("PELVIS_ICM_READY\n");
	result = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, advertising_data,
		ARRAY_SIZE(advertising_data), NULL, 0U);
	if (result != 0) {
		printk("PELVIS_ADV_START_ERROR=%d\n", result);
		return result;
	}
	printk("PELVIS_ADV_READY\n");

	while (true) {
		struct ble_imu_batch_v1 packet = {
			.protocol_version = BLE_TELEMETRY_PROTOCOL_VERSION,
			.message_type = BLE_TELEMETRY_IMU_BATCH,
			.node_index = CONFIG_COMP971_NODE_INDEX,
			.sample_count = 0U,
			.sync_epoch = 0U,
		};
		struct icm20948_sample sample;
		/* Send at 20 Hz: the normal 60 Hz input supplies three samples per
		 * notification, which avoids exhausting Bluetooth transmit buffers. */
		k_msleep(50);
		if (k_msgq_get(&imu_queue, &sample, K_NO_WAIT) != 0) {
			continue;
		}
		copy_sample(&packet.samples[packet.sample_count++], &sample);
		while (packet.sample_count < BLE_IMU_BATCH_MAX_SAMPLES &&
			k_msgq_get(&imu_queue, &sample, K_NO_WAIT) == 0) {
			copy_sample(&packet.samples[packet.sample_count++], &sample);
		}
		uint16_t length = 8U +
			((uint16_t)packet.sample_count * sizeof(struct ble_imu_sample_v1));
		if (gateway_connection != NULL) {
			result = bt_gatt_notify(gateway_connection, &comp971_service.attrs[2],
						&packet, length);
			if (result != 0 && (packet.samples[0].sequence % 60U) == 0U) {
				printk("PELVIS_NOTIFY_ERROR=%d\n", result);
			}
		}
	}
}
