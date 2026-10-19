/* BLE central acquisition gateway: preserves every COMP971 telemetry record. */
#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include <sample_usbd.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/time_units.h>
#include <zephyr/usb/usbd.h>

#include "gateway_acquisition_stream.h"
#include "ble_telemetry_protocol.h"

#define WEARABLE_NAME_PREFIX "COMP971-MOTION-"
#define SCAN_INTERVAL 0x0060U /* 60 ms */
#define SCAN_WINDOW   0x0030U /* 30 ms */
#define CONN_INTERVAL 0x0018U /* 30 ms (24 * 1.25 ms) for balanced telemetry and UWB margin */
#define CONNECT_ATTEMPT_TIMEOUT_MS 3000U
#define GATEWAY_STATUS_PERIOD_MS 1000U

#define COMP971_SERVICE_UUID_VAL \
	BT_UUID_128_ENCODE(0x6e2fd6a4, 0x6a4c, 0x4b64, 0xb389, 0x139718b02410)
#define COMP971_IMU_UUID_VAL \
	BT_UUID_128_ENCODE(0x6e2fd6a5, 0x6a4c, 0x4b64, 0xb389, 0x139718b02410)
#define GATEWAY_HEARTBEAT_VALUE 0xA5U

struct telemetry_client {
	struct bt_conn *conn;
	struct bt_gatt_exchange_params mtu_exchange;
	struct bt_gatt_discover_params discovery;
	struct bt_gatt_subscribe_params subscription;
	uint16_t service_end_handle;
	uint16_t imu_value_handle;
	uint16_t heartbeat_value_handle;
	int64_t last_packet_ms;
	uint32_t packets_received;
	uint32_t bad_packets;
	uint32_t sequence_gaps;
	uint32_t last_sequence;
	bool have_last_sequence;
};

static struct bt_uuid_128 comp971_service_uuid = BT_UUID_INIT_128(COMP971_SERVICE_UUID_VAL);
static struct bt_uuid_128 comp971_imu_uuid = BT_UUID_INIT_128(COMP971_IMU_UUID_VAL);
static struct bt_conn *connecting;
static int64_t connecting_started_ms;
static uint8_t connected_count;
static struct usbd_context *usb_context;
static struct telemetry_client clients[CONFIG_BT_MAX_CONN];
static struct k_work_delayable connection_watchdog;
static bool scan_active;
static int64_t last_gateway_status_ms;
static uint16_t g_gateway_connects;
static uint16_t g_gateway_disconnects;
static uint8_t g_last_disconnect_reason;

static void start_scan(void);
static void start_telemetry_discovery(struct bt_conn *conn);
static uint8_t discover_telemetry(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				  struct bt_gatt_discover_params *params);

static uint64_t gateway_timestamp_us(void)
{
	/* The nRF52840 system timer exposes 32,768 uptime ticks/s, but no
	 * native 64-bit cycle counter. Convert the 64-bit uptime ticks directly
	 * so the gateway receive timestamps stay monotonic and do not wrap. */
	return k_ticks_to_us_floor64(k_uptime_ticks());
}

static void publish_gateway_status(void)
{
	struct gateway_transport_status_v1 status = {
		.protocol_version = BLE_TELEMETRY_PROTOCOL_VERSION,
		.record_type = GATEWAY_USB_RECORD_GATEWAY_STATUS,
		.node_index = 0xFFU,
		.last_disconnect_reason = g_last_disconnect_reason,
		.gateway_connects = g_gateway_connects,
		.gateway_disconnects = g_gateway_disconnects,
		.connected_count = connected_count,
		.scan_active = (uint8_t)scan_active,
		.reserved2 = (connecting != NULL) ? 1U : 0U,
	};

	for (uint8_t index = 0U; index < CONFIG_BT_MAX_CONN; ++index) {
		status.gateway_packets_received += clients[index].packets_received;
		status.gateway_bad_packets += clients[index].bad_packets;
		status.gateway_sequence_gaps += clients[index].sequence_gaps;
	}
	status.usb_records_dropped = gateway_stream_dropped_records();
	status.usb_queue_high_water = gateway_stream_queue_high_water();
	(void)gateway_stream_enqueue(GATEWAY_USB_RECORD_GATEWAY_STATUS, &status,
		sizeof(status));
}

static struct telemetry_client *client_for_conn(struct bt_conn *conn)
{
	uint8_t index = bt_conn_index(conn);
	if (index >= CONFIG_BT_MAX_CONN) {
		return NULL;
	}
	return &clients[index];
}

static bool advertised_name_matches(struct bt_data *data, void *user_data)
{
	bool *matches = user_data;
	size_t prefix_length = strlen(WEARABLE_NAME_PREFIX);
	if ((data->type == BT_DATA_NAME_COMPLETE || data->type == BT_DATA_NAME_SHORTENED) &&
		data->data_len >= prefix_length &&
		memcmp(data->data, WEARABLE_NAME_PREFIX, prefix_length) == 0) {
		*matches = true;
		return false;
	}
	return true;
}

static uint8_t received_imu_notification(struct bt_conn *conn,
					  struct bt_gatt_subscribe_params *params,
					  const void *data, uint16_t length)
{
	struct telemetry_client *client = client_for_conn(conn);
	const uint8_t *raw = data;
	uint8_t sample_count;
	uint8_t node_index;
	uint16_t expected_length;
	uint32_t sync_epoch;
	uint64_t received_us;

	ARG_UNUSED(params);
	if (data == NULL || client == NULL) {
		return BT_GATT_ITER_STOP;
	}
	if (length < BLE_IMU_BATCH_HEADER_SIZE) {
		++client->bad_packets;
		return BT_GATT_ITER_CONTINUE;
	}
	client->last_packet_ms = k_uptime_get();
	received_us = gateway_timestamp_us();
	if (raw[0] == BLE_TELEMETRY_PROTOCOL_VERSION &&
		raw[1] == BLE_TELEMETRY_UWB_RANGE) {
		struct ble_uwb_range_v1 range;
		struct gateway_uwb_record_v1 record;
		if (length != sizeof(range)) {
			++client->bad_packets;
			return BT_GATT_ITER_CONTINUE;
		}
		memcpy(&range, raw, sizeof(range));
		record.range = range;
		record.gateway_receive_timestamp_us = received_us;
		++client->packets_received;
		(void)gateway_stream_enqueue(BLE_TELEMETRY_UWB_RANGE, &record,
			sizeof(record));
		return BT_GATT_ITER_CONTINUE;
	}
	if (raw[0] == BLE_TELEMETRY_PROTOCOL_VERSION &&
		raw[1] == BLE_TELEMETRY_UWB_DIAGNOSTIC) {
		struct ble_uwb_diagnostic_v1 diagnostic;
		struct gateway_uwb_diagnostic_record_v1 record;

		if (length != sizeof(diagnostic)) {
			++client->bad_packets;
			return BT_GATT_ITER_CONTINUE;
		}
		memcpy(&diagnostic, raw, sizeof(diagnostic));
		record.diagnostic = diagnostic;
		record.gateway_receive_timestamp_us = received_us;
		++client->packets_received;
		(void)gateway_stream_enqueue(BLE_TELEMETRY_UWB_DIAGNOSTIC, &record,
			sizeof(record));
		return BT_GATT_ITER_CONTINUE;
	}
	if (raw[0] == BLE_TELEMETRY_PROTOCOL_VERSION &&
		raw[1] == BLE_TELEMETRY_NODE_STATUS) {
		struct ble_node_status_v1 status;
		struct gateway_node_status_record_v1 record;

		if (length != sizeof(status)) {
			++client->bad_packets;
			return BT_GATT_ITER_CONTINUE;
		}
		memcpy(&status, raw, sizeof(status));
		record.status = status;
		record.gateway_receive_timestamp_us = received_us;
		++client->packets_received;
		(void)gateway_stream_enqueue(BLE_TELEMETRY_NODE_STATUS, &record,
			sizeof(record));
		return BT_GATT_ITER_CONTINUE;
	}
	if (raw[0] == BLE_TELEMETRY_PROTOCOL_VERSION &&
		raw[1] == BLE_TELEMETRY_STALL_SNAPSHOT) {
		struct ble_stall_snapshot_v1 snapshot;
		struct gateway_stall_snapshot_record_v1 record;

		if (length != sizeof(snapshot)) {
			++client->bad_packets;
			return BT_GATT_ITER_CONTINUE;
		}
		memcpy(&snapshot, raw, sizeof(snapshot));
		record.snapshot = snapshot;
		record.gateway_receive_timestamp_us = received_us;
		++client->packets_received;
		(void)gateway_stream_enqueue(BLE_TELEMETRY_STALL_SNAPSHOT, &record,
			sizeof(record));
		return BT_GATT_ITER_CONTINUE;
	}
	if (raw[0] == BLE_TELEMETRY_PROTOCOL_VERSION &&
		raw[1] == BLE_TELEMETRY_STAR_RANGES) {
		struct ble_star_ranges_v1 star_ranges;
		struct gateway_star_ranges_record_v1 record;

		if (length != sizeof(star_ranges)) {
			++client->bad_packets;
			return BT_GATT_ITER_CONTINUE;
		}
		memcpy(&star_ranges, raw, sizeof(star_ranges));
		record.ranges = star_ranges;
		record.gateway_receive_timestamp_us = received_us;
		++client->packets_received;
		(void)gateway_stream_enqueue(BLE_TELEMETRY_STAR_RANGES, &record,
			sizeof(record));
		return BT_GATT_ITER_CONTINUE;
	}
	sample_count = raw[3];
	node_index = raw[2];
	expected_length = BLE_IMU_BATCH_WIRE_SIZE(sample_count);
	if (raw[0] != BLE_TELEMETRY_PROTOCOL_VERSION ||
		raw[1] != BLE_TELEMETRY_IMU_BATCH ||
		sample_count == 0U || sample_count > BLE_IMU_BATCH_MAX_SAMPLES ||
		length != expected_length) {
		++client->bad_packets;
		return BT_GATT_ITER_CONTINUE;
	}
	memcpy(&sync_epoch, raw + offsetof(struct ble_imu_batch_v1, sync_epoch),
		sizeof(sync_epoch));
	++client->packets_received;
	for (uint8_t sample_index = 0U; sample_index < sample_count; ++sample_index) {
		struct gateway_imu_record_v1 record = {
			.protocol_version = BLE_TELEMETRY_PROTOCOL_VERSION,
			.message_type = BLE_TELEMETRY_IMU_BATCH,
			.node_index = node_index,
			.reserved = 0U,
			.sync_epoch = sync_epoch,
			.gateway_receive_timestamp_us = received_us,
		};
		memcpy(&record.sample, raw + BLE_IMU_BATCH_HEADER_SIZE +
			((uint16_t)sample_index * sizeof(record.sample)), sizeof(record.sample));
		if (client->have_last_sequence &&
			record.sample.sequence != client->last_sequence + 1U &&
			record.sample.sequence > client->last_sequence) {
			client->sequence_gaps += record.sample.sequence - client->last_sequence - 1U;
		}
		client->last_sequence = record.sample.sequence;
		client->have_last_sequence = true;
		(void)gateway_stream_enqueue(BLE_TELEMETRY_IMU_BATCH, &record,
			sizeof(record));
	}
	return BT_GATT_ITER_CONTINUE;
}

static void subscribe_complete(struct bt_conn *conn, uint8_t error,
			       struct bt_gatt_subscribe_params *params)
{
	ARG_UNUSED(params);
	if (error == 0U) {
		struct telemetry_client *client = client_for_conn(conn);
		/* The wearable service declares telemetry, its CCC, then the writable
		 * heartbeat. This fixed project service layout makes the value handle
		 * exactly three handles after the telemetry value. */
		if (client != NULL) {
			client->heartbeat_value_handle = client->imu_value_handle + 3U;
		}
		/* The binary stream now carries acquisition data.  Console diagnostics
		 * are deliberately disabled so text cannot corrupt raw USB records. */
	} else {
		ARG_UNUSED(error);
	}
	start_scan();
}

static int discover_characteristic(struct bt_conn *conn, struct telemetry_client *client)
{
	memset(&client->discovery, 0, sizeof(client->discovery));
	client->discovery.uuid = &comp971_imu_uuid.uuid;
	client->discovery.func = discover_telemetry;
	client->discovery.start_handle = 1U;
	client->discovery.end_handle = client->service_end_handle;
	client->discovery.type = BT_GATT_DISCOVER_CHARACTERISTIC;
	return bt_gatt_discover(conn, &client->discovery);
}

static int subscribe_to_imu(struct bt_conn *conn, struct telemetry_client *client)
{
	memset(&client->subscription, 0, sizeof(client->subscription));
	client->subscription.notify = received_imu_notification;
	client->subscription.subscribe = subscribe_complete;
	client->subscription.value_handle = client->imu_value_handle;
	/* The COMP971 peripheral defines its CCC immediately after the notify
	 * value attribute. This avoids unreliable descriptor discovery observed
	 * during a multi-node connection. */
	client->subscription.ccc_handle = client->imu_value_handle + 1U;
	client->subscription.value = BT_GATT_CCC_NOTIFY;
	return bt_gatt_subscribe(conn, &client->subscription);
}

static uint8_t discover_telemetry(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				  struct bt_gatt_discover_params *params)
{
	struct telemetry_client *client = client_for_conn(conn);
	int result;

	if (client == NULL) {
		return BT_GATT_ITER_STOP;
	}
	if (attr == NULL) {
		return BT_GATT_ITER_STOP;
	}
	if (params->type == BT_GATT_DISCOVER_PRIMARY) {
		const struct bt_gatt_service_val *service = attr->user_data;
		if (bt_uuid_cmp(service->uuid, &comp971_service_uuid.uuid) != 0) {
			return BT_GATT_ITER_CONTINUE;
		}
		client->service_end_handle = service->end_handle;
		result = discover_characteristic(conn, client);
		ARG_UNUSED(result);
		return BT_GATT_ITER_STOP;
	}
	if (params->type == BT_GATT_DISCOVER_CHARACTERISTIC) {
		const struct bt_gatt_chrc *characteristic = attr->user_data;
		client->imu_value_handle = characteristic->value_handle;
		result = subscribe_to_imu(conn, client);
		ARG_UNUSED(result);
		return BT_GATT_ITER_STOP;
	}
	return BT_GATT_ITER_STOP;
}

static void start_telemetry_discovery(struct bt_conn *conn)
{
	struct telemetry_client *client = client_for_conn(conn);
	int result;
	if (client == NULL) {
		return;
	}
	memset(&client->discovery, 0, sizeof(client->discovery));
	/* Discover all primary services and compare UUIDs in the callback. This is
	 * more reliable during first-contact diagnosis than a filtered request. */
	client->discovery.uuid = NULL;
	client->discovery.func = discover_telemetry;
	client->discovery.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
	client->discovery.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
	client->discovery.type = BT_GATT_DISCOVER_PRIMARY;
	result = bt_gatt_discover(conn, &client->discovery);
	ARG_UNUSED(result);
}

static void mtu_exchange_complete(struct bt_conn *conn, uint8_t error,
				  struct bt_gatt_exchange_params *params)
{
	ARG_UNUSED(params);
	uint16_t mtu = bt_gatt_get_mtu(conn);

	ARG_UNUSED(params);
	if (error != 0U || mtu < BLE_REQUIRED_ATT_MTU) {
		/* A 74-byte IMU batch requires ATT MTU >= 77. Never subscribe to a
		 * link that would force silent truncation or failed notifications. */
		(void)bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
		start_scan();
		return;
	}
	start_telemetry_discovery(conn);
}

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			 struct net_buf_simple *ad)
{
	bool matches = false;
	struct bt_conn_le_create_param create_param = {
		.options = BT_CONN_LE_OPT_NONE,
		.interval = SCAN_INTERVAL,
		.window = SCAN_WINDOW,
		.interval_coded = 0U,
		.window_coded = 0U,
		/* Units are 10 ms. Do not wait indefinitely if a wearable disappears
		 * during a flash or restart. */
		.timeout = CONNECT_ATTEMPT_TIMEOUT_MS / 10U,
	};
	struct bt_le_conn_param connection_param = {
		.interval_min = CONN_INTERVAL,
		.interval_max = CONN_INTERVAL,
		.latency = 0U,
		.timeout = 400U,
	};
	int result;

	if (connecting != NULL || connected_count >= CONFIG_BT_MAX_CONN ||
		type != BT_GAP_ADV_TYPE_ADV_IND) {
		return;
	}
	bt_data_parse(ad, advertised_name_matches, &matches);
	if (!matches) {
		return;
	}
	ARG_UNUSED(rssi);
	result = bt_le_scan_stop();
	if (result != 0 && result != -EALREADY) {
		return;
	}
	scan_active = false;
	result = bt_conn_le_create(addr, &create_param, &connection_param, &connecting);
	if (result != 0) {
		connecting = NULL;
		connecting_started_ms = 0;
		start_scan();
	} else {
		connecting_started_ms = k_uptime_get();
	}
}

static void start_scan(void)
{
	struct bt_le_scan_param scan_param = {
		.type = BT_LE_SCAN_TYPE_PASSIVE,
		.options = BT_LE_SCAN_OPT_NONE,
		.interval = SCAN_INTERVAL,
		.window = SCAN_WINDOW,
	};
	int result;
	if (connected_count >= CONFIG_BT_MAX_CONN || connecting != NULL) {
		return;
	}
	result = bt_le_scan_start(&scan_param, device_found);
	if (result == 0 || result == -EALREADY) {
		scan_active = true;
	} else {
		scan_active = false;
		(void)bt_le_scan_stop();
		result = bt_le_scan_start(&scan_param, device_found);
		if (result == 0 || result == -EALREADY) {
			scan_active = true;
		}
	}
}

/* A peripheral can remain electrically powered while a BLE connection gets
 * stuck and no longer delivers notifications.  Recover without requiring the
 * user to inspect LEDs or replug the gateway: drop the stale link and scan
 * for the board again. */
static void connection_watchdog_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	int64_t now_ms = k_uptime_get();
	if (connecting != NULL &&
		(now_ms - connecting_started_ms) > CONNECT_ATTEMPT_TIMEOUT_MS) {
		struct bt_conn *stuck = connecting;
		connecting = NULL;
		connecting_started_ms = 0;
		(void)bt_conn_disconnect(stuck, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
		bt_conn_unref(stuck);
		(void)bt_le_scan_stop();
		scan_active = false;
		start_scan();
	}
	for (uint8_t index = 0U; index < CONFIG_BT_MAX_CONN; ++index) {
		struct telemetry_client *client = &clients[index];
		if (client->conn != NULL &&
			(now_ms - client->last_packet_ms) > 10000) {
			/* Avoid issuing the same disconnect again before the callback. */
			client->last_packet_ms = now_ms;
			(void)bt_conn_disconnect(client->conn,
				BT_HCI_ERR_REMOTE_USER_TERM_CONN);
		}
	}
	if ((now_ms - last_gateway_status_ms) >= GATEWAY_STATUS_PERIOD_MS) {
		last_gateway_status_ms = now_ms;
		publish_gateway_status();
	}
	start_scan();
	(void)k_work_schedule(&connection_watchdog, K_SECONDS(1));
}

static void connected(struct bt_conn *conn, uint8_t error)
{
	if (error != 0U) {
		if (connecting != NULL) {
			bt_conn_unref(connecting);
			connecting = NULL;
		}
		connecting_started_ms = 0;
		(void)bt_le_scan_stop();
		scan_active = false;
		start_scan();
		return;
	}
	if (connecting != NULL) {
		bt_conn_unref(connecting);
		connecting = NULL;
	}
	connecting_started_ms = 0;
	++connected_count;
	++g_gateway_connects;
	struct telemetry_client *client = client_for_conn(conn);
	int result;
	if (client == NULL) {
		start_scan();
		return;
	}
	memset(client, 0, sizeof(*client));
	client->conn = bt_conn_ref(conn);
	client->last_packet_ms = k_uptime_get();
	client->mtu_exchange.func = mtu_exchange_complete;
	result = bt_gatt_exchange_mtu(conn, &client->mtu_exchange);
	if (result == -EALREADY) {
		mtu_exchange_complete(conn, 0U, &client->mtu_exchange);
	} else if (result != 0) {
		mtu_exchange_complete(conn, (uint8_t)-result, &client->mtu_exchange);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	struct telemetry_client *client = client_for_conn(conn);
	if (client != NULL && client->conn != NULL) {
		bt_conn_unref(client->conn);
		memset(client, 0, sizeof(*client));
	}
	if (connected_count > 0U) {
		--connected_count;
	}
	++g_gateway_disconnects;
	g_last_disconnect_reason = reason;
	(void)bt_le_scan_stop();
	scan_active = false;
	start_scan();
}

BT_CONN_CB_DEFINE(connection_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

static void usb_message_callback(struct usbd_context *const context,
				 const struct usbd_msg *message)
{
	gateway_stream_handle_usbd_message(message);
	if (usbd_can_detect_vbus(context) && message->type == USBD_MSG_VBUS_READY) {
		(void)usbd_enable(context);
	}
}

static int start_usb(void)
{
	usb_context = sample_usbd_init_device(usb_message_callback);
	if (usb_context == NULL) {
		return -1;
	}
	if (!usbd_can_detect_vbus(usb_context)) {
		return usbd_enable(usb_context);
	}
	return 0;
}

int main(void)
{
	int result = start_usb();
	if (result != 0) {
		return result;
	}
	result = gateway_stream_start();
	if (result != 0) {
		return result;
	}
	result = bt_enable(NULL);
	if (result != 0) {
		return result;
	}
	k_work_init_delayable(&connection_watchdog, connection_watchdog_handler);
	(void)k_work_schedule(&connection_watchdog, K_SECONDS(1));
	start_scan();
	return 0;
}
