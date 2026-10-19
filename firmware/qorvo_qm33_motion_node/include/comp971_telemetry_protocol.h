/*
 * COMP971 wearable-to-gateway binary protocol, version 1.
 *
 * Unified protocol header for firmware nodes, gateway receiver, and post-processors.
 * All wire structures are little endian and packed.
 */
#ifndef COMP971_TELEMETRY_PROTOCOL_H
#define COMP971_TELEMETRY_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#define COMP971_PROTOCOL_VERSION          1U
#define COMP971_IMU_BATCH_MAX_SAMPLES     9U
#define COMP971_IMU_BATCH_HEADER_SIZE     8U
#define COMP971_IMU_SAMPLE_WIRE_SIZE      22U
#define COMP971_IMU_BATCH_MAX_WIRE_SIZE   206U
#define COMP971_REQUIRED_ATT_MTU          209U

#define COMP971_SERVICE_UUID_16           0xD6A4U
#define COMP971_TELEMETRY_UUID_16         0xD6A5U
#define COMP971_HEARTBEAT_UUID_16         0xD6A6U
#define COMP971_GATEWAY_HEARTBEAT_VALUE   0xA5U

enum comp971_telemetry_message_type {
    COMP971_TELEMETRY_IMU_BATCH       = 0x01U,
    COMP971_TELEMETRY_CLOCK_STATUS    = 0x02U,
    COMP971_TELEMETRY_UWB_RANGE       = 0x03U,
    COMP971_TELEMETRY_NODE_STATUS     = 0x04U,
    COMP971_TELEMETRY_UWB_DIAGNOSTIC  = 0x05U,
    COMP971_TELEMETRY_STALL_SNAPSHOT  = 0x06U,
    COMP971_TELEMETRY_STAR_RANGES     = 0x07U,
};

/* Unified Progress Milestones (0xC1..0xC5) */
#define COMP971_PROG_MAC_READY            0xC1U
#define COMP971_PROG_SCHED_READY          0xC2U
#define COMP971_PROG_SESSION_READY        0xC3U
#define COMP971_PROG_MAC_STARTED          0xC4U
#define COMP971_PROG_SESSION_STARTED      0xC5U

/* Unified Failure / Error Stage Codes (0xD1..0xD9) - NO COLLISION with health */
#define COMP971_ERR_MAC_INIT              0xD1U
#define COMP971_ERR_HELPER_OPEN           0xD2U
#define COMP971_ERR_SCHEDULER             0xD3U
#define COMP971_ERR_MEASUREMENT           0xD4U
#define COMP971_ERR_SESSION_INIT          0xD5U
#define COMP971_ERR_SESSION_PARAMS        0xD6U
#define COMP971_ERR_ADD_CONTROLEE         0xD7U
#define COMP971_ERR_MAC_START             0xD8U
#define COMP971_ERR_SESSION_START         0xD9U

/* Unified Periodic Health Telemetry Step Codes (0xB0..0xB7, 0xC8..0xCF) */
#define COMP971_HEALTH_TX_COUNT           0xB0U
#define COMP971_HEALTH_INTERVAL_MIN_LEAD  0xB1U
#define COMP971_HEALTH_LATE_TX_COUNT      0xB2U
#define COMP971_HEALTH_STALL_RECOVERY_CNT 0xB3U
#define COMP971_HEALTH_STEADY_FAIL_CNT    0xB4U
#define COMP971_HEALTH_STEADY_TOTAL       0xB5U
#define COMP971_HEALTH_LAST_BLOCK_INDEX   0xB6U
#define COMP971_HEALTH_LIFETIME_MIN_LEAD  0xB7U
#define COMP971_HEALTH_DW3000_SYS_LO      0xB8U
#define COMP971_HEALTH_DW3000_SYS_HI      0xB9U
#define COMP971_HEALTH_CONFIG_HASH        0xBAU
#define COMP971_ERR_UNKNOWN_IDENTITY      0xDFU
#define COMP971_HEALTH_UWB_STACK_FREE     0xC8U
#define COMP971_HEALTH_IMU_STACK_FREE     0xC9U
#define COMP971_HEALTH_STATUS_STACK_FREE  0xCAU
#define COMP971_HEALTH_DISCONN_AND_FLUSH  0xCBU
#define COMP971_HEALTH_CONNECT_BLOCK_IDX  0xCCU
#define COMP971_HEALTH_FAIL_RING_BLOCK    0xCDU
#define COMP971_HEALTH_FAIL_RING_STATUS   0xCEU
#define COMP971_HEALTH_CONNECT_CNT_AND_Q  0xCFU

/* Stall Diagnostic Indicator Stage Codes (0xE0..0xE6) - associated with frozen stall */
#define COMP971_DIAG_STALL_IRQ_PIN        0xE0U /* DW3000 IRQ GPIO level (P1.02) */
#define COMP971_DIAG_STALL_TASK_STATE     0xE1U /* FreeRTOS App UWB task state (0=Running, 1=Ready, 2=Blocked, 3=Suspended, 4=Deleted) */
#define COMP971_DIAG_STALL_TX_COUNT       0xE2U /* Driver TX progress counter at trip */
#define COMP971_DIAG_STALL_CB_SEQ         0xE3U /* FiRa callback sequence counter at trip */
#define COMP971_DIAG_STALL_IRQ_COUNT      0xE4U /* DW3000 ISR entry count at trip */
#define COMP971_DIAG_STALL_CB_LAST_IRQ    0xE5U /* DW3000 ISR entry count at last successful FiRa callback */
#define COMP971_DIAG_STALL_CB_LAST_TX     0xE6U /* Driver TX progress counter at last successful FiRa callback */
#define COMP971_DIAG_STALL_TX_INFO        0xE7U /* Packed [31:24]fastcmd [23:16]mode [15:8]result [7:0]hpdwarn */
#define COMP971_DIAG_STALL_TX_SYS_STATE   0xE8U /* DW3000 SYS_STATE_LO register captured at last TX */
#define COMP971_DIAG_STALL_TX_STATUS      0xE9U /* DW3000 SYS_STATUS byte 3 captured at last TX */
#define COMP971_DIAG_STALL_TX_DIFF        0xEAU /* Scheduled TX lead (dx_time - cur_sys) in DTU */
#define COMP971_DIAG_STALL_TX_SEQ         0xEBU /* Monotonic TX sequence number at last TX command */

/* Escalation Ladder Stages */
#define COMP971_ESCALATION_NONE           0x00U
#define COMP971_ESCALATION_STAGE1_SESSION 0x01U
#define COMP971_ESCALATION_STAGE2_MAC     0x02U
#define COMP971_ESCALATION_STAGE3_WDT     0x03U

/* Retained RAM Configuration */
#define COMP971_RETAINED_MAGIC            0x5354414CU /* "STAL" */
#define COMP971_SNAPSHOT_RING_SIZE        16U

/* Compact callback record (8 bytes) */
struct comp971_cb_record {
    uint16_t block_index;
    uint8_t  status;
    uint8_t  lead_ms;             /* lead time converted to ms (or clamped) */
    uint16_t tick_offset_ms;      /* delta ms before event/trip */
    uint16_t reserved;
} __attribute__((packed));

/* Dedicated stall snapshot frame (52 bytes total payload <= 64 bytes USB limit) */
struct comp971_stall_snapshot_v1 {
    uint8_t  protocol_version;    /* 1 */
    uint8_t  message_type;        /* COMP971_TELEMETRY_STALL_SNAPSHOT = 0x06 */
    uint8_t  node_index;
    uint8_t  trigger_flags;       /* Bit 0 = callback-age stall, Bit 1 = block-progress stall */
    uint32_t magic;               /* COMP971_RETAINED_MAGIC */
    uint32_t boot_count;
    uint32_t reset_reason;        /* NRF_POWER->RESETREAS */
    uint8_t  escalation_reached;  /* Stage reached before trip/reboot */
    uint8_t  reserved2[3];
    uint32_t stall_duration_ms;   /* Gap in ms before recovery/trip */
    uint32_t last_block_index;    /* Last block index seen in callback */
    int32_t  min_lead_dtu;        /* Lifetime minimum TX lead */
    uint32_t late_tx_count;       /* Total late TX count */
    /* 2 most recent callback records preserved on wire (2 * 8 = 16 bytes) */
    struct comp971_cb_record recent_cbs[2];
    uint32_t crc32;
} __attribute__((packed));

struct comp971_imu_sample_v1 {
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

struct comp971_imu_batch_v1 {
    uint8_t protocol_version;
    uint8_t message_type;
    uint8_t node_index;
    uint8_t sample_count;
    uint32_t sync_epoch;
    struct comp971_imu_sample_v1 samples[COMP971_IMU_BATCH_MAX_SAMPLES];
} __attribute__((packed));

/* A UWB result is always reported by the PELVIS coordinator.  pair_index is
 * the stable COMP971 matrix index (0 is HEAD <-> PELVIS).  A negative range
 * is an invalid ranging result and has COMP971_UWB_RANGE_VALID clear. */
#define COMP971_UWB_RANGE_VALID 0x0001U
struct comp971_uwb_range_v1 {
    uint8_t protocol_version;
    uint8_t message_type;
    uint8_t source_node;
    uint8_t pair_index;
    uint32_t pelvis_timestamp_us;
    int32_t range_cm;
    uint16_t quality_flags;
    uint16_t status_code;
} __attribute__((packed));

/* Event-only diagnostics */
struct comp971_uwb_diagnostic_v1 {
    uint8_t protocol_version;
    uint8_t message_type;
    uint8_t node_index;
    uint8_t pair_index;
    uint8_t stage_code;
    uint8_t reserved[3];
    uint32_t local_timestamp_us;
    int32_t detail;
} __attribute__((packed));

struct comp971_node_status_v1 {
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

#define COMP971_STAR_MAX_PAIRS 5U

struct comp971_star_range_entry_v1 {
    int16_t  range_cm;
    uint8_t  status;
    uint8_t  flags;
} __attribute__((packed));

struct comp971_star_ranges_v1 {
    uint8_t  protocol_version;
    uint8_t  message_type;
    uint8_t  node_index;
    uint8_t  reserved;
    uint32_t pelvis_timestamp_us;
    uint32_t block_index;
    struct comp971_star_range_entry_v1 pairs[COMP971_STAR_MAX_PAIRS];
} __attribute__((packed));

_Static_assert(sizeof(struct comp971_imu_sample_v1) == COMP971_IMU_SAMPLE_WIRE_SIZE,
               "COMP971 IMU sample ABI changed");
_Static_assert(offsetof(struct comp971_imu_batch_v1, samples) == COMP971_IMU_BATCH_HEADER_SIZE,
               "COMP971 IMU batch header ABI changed");
_Static_assert(sizeof(struct comp971_imu_batch_v1) == COMP971_IMU_BATCH_MAX_WIRE_SIZE,
               "COMP971 maximum IMU batch ABI changed");
_Static_assert(sizeof(struct comp971_uwb_range_v1) == 16U,
               "COMP971 UWB range ABI changed");
_Static_assert(sizeof(struct comp971_uwb_diagnostic_v1) == 16U,
               "COMP971 UWB diagnostic ABI changed");
_Static_assert(sizeof(struct comp971_node_status_v1) == 52U,
               "COMP971 node-status ABI changed");
_Static_assert(sizeof(struct comp971_star_ranges_v1) == 32U,
               "COMP971 star ranges ABI changed");
_Static_assert(sizeof(struct comp971_cb_record) == 8U,
               "COMP971 callback record ABI changed");
_Static_assert(sizeof(struct comp971_stall_snapshot_v1) == 56U,
               "COMP971 stall snapshot ABI changed");

#endif /* COMP971_TELEMETRY_PROTOCOL_H */
