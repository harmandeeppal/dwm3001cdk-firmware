/*
 * Shared BLE telemetry packet contract, version 1.
 *
 * This header is intentionally transport-only. It does not enable Bluetooth
 * and is not included by the currently flashed UWB test firmware. Firmware for
 * all six DWM boards and the nRF52840 Dongle must use this exact contract.
 */
#ifndef BLE_TELEMETRY_PROTOCOL_H
#define BLE_TELEMETRY_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#include <zephyr/sys/util.h>

#define BLE_TELEMETRY_PROTOCOL_VERSION 1U
#define BLE_IMU_BATCH_MAX_SAMPLES       9U
#define BLE_ATT_NOTIFICATION_OVERHEAD    3U

enum ble_telemetry_message_type {
	BLE_TELEMETRY_IMU_BATCH = 0x01U,
	BLE_TELEMETRY_CLOCK_STATUS = 0x02U,
	BLE_TELEMETRY_UWB_RANGE = 0x03U,
	BLE_TELEMETRY_NODE_STATUS = 0x04U,
	BLE_TELEMETRY_UWB_DIAGNOSTIC = 0x05U,
	BLE_TELEMETRY_STALL_SNAPSHOT = 0x06U,
	BLE_TELEMETRY_STAR_RANGES = 0x07U,
};

enum ble_telemetry_flags {
	BLE_TELEMETRY_SYNC_VALID = 0x01U,
	BLE_TELEMETRY_UWB_VALID = 0x01U,
	BLE_TELEMETRY_UWB_NLOS_SUSPECTED = 0x02U,
	BLE_TELEMETRY_UWB_DROPPED = 0x04U,
};

/* All wire structures are little endian and packed. A 32-bit microsecond
 * timestamp wraps every ~71.6 minutes; the laptop expands it using sequence
 * continuity and sync_epoch. */
struct ble_imu_sample_v1 {
	uint32_t sequence;
	uint32_t local_timestamp_us;
	int16_t ax;
	int16_t ay;
	int16_t az;
	int16_t gx;
	int16_t gy;
	int16_t gz;
	int16_t temperature;
} __attribute__((packed));

struct ble_imu_batch_v1 {
	uint8_t protocol_version;
	uint8_t message_type;
	uint8_t node_index;
	uint8_t sample_count; /* 1..BLE_IMU_BATCH_MAX_SAMPLES */
	uint32_t sync_epoch;
	struct ble_imu_sample_v1 samples[BLE_IMU_BATCH_MAX_SAMPLES];
} __attribute__((packed));

struct ble_clock_status_v1 {
	uint8_t protocol_version;
	uint8_t message_type;
	uint8_t node_index;
	uint8_t flags; /* BLE_TELEMETRY_SYNC_VALID when mapped */
	uint32_t sync_epoch;
	uint32_t local_timestamp_us;
	int32_t pelvis_offset_us;
	int32_t drift_ppb;
	uint32_t uncertainty_us;
} __attribute__((packed));

struct ble_uwb_range_v1 {
	uint8_t protocol_version;
	uint8_t message_type;
	uint8_t source_node; /* PELVIS for final BLE reporting */
	uint8_t pair_index;
	uint32_t pelvis_timestamp_us;
	int32_t range_cm; /* negative only when BLE_TELEMETRY_UWB_VALID is clear */
	uint16_t quality_flags;
	uint16_t reserved;
} __attribute__((packed));

/* Event-only UWB diagnostics. These are emitted on failures, not as a second
 * per-slot stream. detail carries the driver status or a small stage-specific
 * value; local_timestamp_us is the reporting node's monotonic uptime. */
struct ble_uwb_diagnostic_v1 {
	uint8_t protocol_version;
	uint8_t message_type;
	uint8_t node_index;
	uint8_t pair_index;
	uint8_t stage_code;
	uint8_t reserved[3];
	uint32_t local_timestamp_us;
	int32_t detail;
} __attribute__((packed));

/* Periodic wearable health snapshot.  Counters are monotonically increasing
 * within one boot.  A later protocol revision will add an explicit boot ID
 * and clock mapping; this Phase-A format deliberately makes present transport
 * loss visible without changing IMU sample semantics. */
struct ble_node_status_v1 {
	uint8_t protocol_version;
	uint8_t message_type;
	uint8_t node_index;
	uint8_t flags;
	uint32_t sync_epoch;
	uint32_t imu_samples_acquired;
	uint32_t imu_queue_put_ok;
	uint32_t imu_queue_dropped;
	uint32_t imu_queue_high_water;
	uint32_t ble_imu_notify_ok;
	uint32_t ble_imu_notify_failed;
	uint32_t ble_uwb_notify_failed;
	uint32_t uwb_range_attempts;
	uint32_t uwb_range_valid;
	uint32_t uwb_range_invalid;
	uint32_t uwb_result_timeout;
} __attribute__((packed));

struct ble_cb_record {
	uint16_t block_index;
	uint8_t  status;
	uint8_t  lead_ms;
	uint16_t tick_offset_ms;
	uint16_t reserved;
} __attribute__((packed));

struct ble_stall_snapshot_v1 {
	uint8_t  protocol_version;
	uint8_t  message_type;
	uint8_t  node_index;
	uint8_t  reserved;
	uint32_t magic;
	uint32_t boot_count;
	uint32_t reset_reason;
	uint8_t  escalation_reached;
	uint8_t  reserved2[3];
	uint32_t stall_duration_ms;
	uint32_t last_block_index;
	int32_t  min_lead_dtu;
	uint32_t late_tx_count;
	struct ble_cb_record recent_cbs[2];
	uint32_t crc32;
} __attribute__((packed));

struct gateway_stall_snapshot_record_v1 {
	struct ble_stall_snapshot_v1 snapshot;
	uint64_t gateway_receive_timestamp_us;
} __attribute__((packed));

/* The IMU batch has a variable on-wire length.  Keep its header calculation
 * coupled to the actual structure so changing the protocol cannot leave a
 * stale literal such as "8U" in a parser. */
#define BLE_IMU_BATCH_HEADER_SIZE \
	((uint16_t)offsetof(struct ble_imu_batch_v1, samples))
#define BLE_IMU_BATCH_WIRE_SIZE(sample_count) \
	((uint16_t)(BLE_IMU_BATCH_HEADER_SIZE + \
	((uint16_t)(sample_count) * sizeof(struct ble_imu_sample_v1))))
#define BLE_IMU_BATCH_MAX_WIRE_SIZE \
	BLE_IMU_BATCH_WIRE_SIZE(BLE_IMU_BATCH_MAX_SAMPLES)
#define BLE_REQUIRED_ATT_MTU \
	((uint16_t)(BLE_IMU_BATCH_MAX_WIRE_SIZE + BLE_ATT_NOTIFICATION_OVERHEAD))

/* Gateway-to-laptop records are binary records transported over USB CDC. The
 * gateway emits one IMU record per acquired IMU sample, rather than selecting
 * the newest sample from a BLE batch. */
struct gateway_imu_record_v1 {
	uint8_t protocol_version;
	uint8_t message_type;
	uint8_t node_index;
	uint8_t reserved;
	uint32_t sync_epoch;
	struct ble_imu_sample_v1 sample;
	uint64_t gateway_receive_timestamp_us;
} __attribute__((packed));

struct gateway_uwb_record_v1 {
	struct ble_uwb_range_v1 range;
	uint64_t gateway_receive_timestamp_us;
} __attribute__((packed));

struct gateway_uwb_diagnostic_record_v1 {
	struct ble_uwb_diagnostic_v1 diagnostic;
	uint64_t gateway_receive_timestamp_us;
} __attribute__((packed));

struct gateway_node_status_record_v1 {
	struct ble_node_status_v1 status;
	uint64_t gateway_receive_timestamp_us;
} __attribute__((packed));

#define BLE_STAR_MAX_PAIRS 5U

struct ble_star_range_entry_v1 {
	int16_t  range_cm;
	uint8_t  status;
	uint8_t  flags;
} __attribute__((packed));

struct ble_star_ranges_v1 {
	uint8_t  protocol_version;
	uint8_t  message_type;
	uint8_t  node_index;
	uint8_t  reserved;
	uint32_t pelvis_timestamp_us;
	uint32_t block_index;
	struct ble_star_range_entry_v1 pairs[BLE_STAR_MAX_PAIRS];
} __attribute__((packed));

struct gateway_star_ranges_record_v1 {
	struct ble_star_ranges_v1 ranges;
	uint64_t gateway_receive_timestamp_us;
} __attribute__((packed));

/* Gateway-local accounting for the USB acquisition path. This is emitted as
 * a USB record, not sent back over BLE. */
#define GATEWAY_USB_RECORD_GATEWAY_STATUS 0x80U
struct gateway_transport_status_v1 {
	uint8_t protocol_version;
	uint8_t record_type;
	uint8_t node_index; /* 0xFF means gateway-wide, rather than a wearable */
	uint8_t last_disconnect_reason;
	uint32_t gateway_packets_received;
	uint32_t gateway_bad_packets;
	uint32_t gateway_sequence_gaps;
	uint32_t usb_records_dropped;
	uint32_t usb_queue_high_water;
	uint16_t gateway_connects;
	uint16_t gateway_disconnects;
	uint8_t connected_count;
	uint8_t scan_active;
	uint16_t reserved2;
} __attribute__((packed));

BUILD_ASSERT(sizeof(struct ble_imu_sample_v1) == 22U,
	"BLE IMU sample wire ABI changed");
BUILD_ASSERT(BLE_IMU_BATCH_HEADER_SIZE == 8U,
	"BLE IMU batch header wire ABI changed");
BUILD_ASSERT(sizeof(struct ble_imu_batch_v1) == 206U,
	"BLE maximum IMU batch wire ABI changed");
BUILD_ASSERT(sizeof(struct ble_clock_status_v1) == 24U,
	"BLE clock-status wire ABI changed");
BUILD_ASSERT(sizeof(struct ble_uwb_range_v1) == 16U,
	"BLE UWB range wire ABI changed");
BUILD_ASSERT(sizeof(struct ble_uwb_diagnostic_v1) == 16U,
	"BLE UWB diagnostic wire ABI changed");
BUILD_ASSERT(sizeof(struct ble_node_status_v1) == 52U,
	"BLE node-status wire ABI changed");
BUILD_ASSERT(sizeof(struct ble_star_ranges_v1) == 32U,
	"BLE star-ranges wire ABI changed");
BUILD_ASSERT(sizeof(struct gateway_imu_record_v1) == 38U,
	"Gateway IMU record wire ABI changed");
BUILD_ASSERT(sizeof(struct gateway_uwb_record_v1) == 24U,
	"Gateway UWB record wire ABI changed");
BUILD_ASSERT(sizeof(struct gateway_uwb_diagnostic_record_v1) == 24U,
	"Gateway UWB diagnostic record wire ABI changed");
BUILD_ASSERT(sizeof(struct gateway_node_status_record_v1) == 60U,
	"Gateway node-status record wire ABI changed");
BUILD_ASSERT(sizeof(struct gateway_star_ranges_record_v1) == 40U,
	"Gateway star-ranges record wire ABI changed");
BUILD_ASSERT(sizeof(struct gateway_transport_status_v1) == 32U,
	"Gateway status record wire ABI changed");
BUILD_ASSERT(BLE_REQUIRED_ATT_MTU == 209U,
	"Maximum IMU notification must fit the required ATT MTU");

#endif /* BLE_TELEMETRY_PROTOCOL_H */
