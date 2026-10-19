#ifndef COMP971_UWB_TWO_NODE_H
#define COMP971_UWB_TWO_NODE_H

#include <stdbool.h>
#include <stdint.h>
#include "comp971_telemetry_protocol.h"

#include "comp971_config_shared.h"

/* Structure stored in retained RAM across soft watchdog/system resets */
struct comp971_retained_block {
    uint32_t magic;
    uint32_t boot_count;
    uint32_t reset_reason;
    uint8_t  last_escalation_stage;
    uint8_t  reset_loop_count;
    uint16_t trigger_flags;        /* Bit 0 = callback_age, Bit 1 = block_progress */
    uint32_t last_reset_tick_ms;
    uint32_t last_stall_duration_ms;
    uint32_t last_block_index;
    int32_t  lifetime_min_lead;
    uint32_t late_tx_count;
    uint32_t sys_status_lo;
    uint32_t sys_status_hi;
    uint32_t ring_head;
    uint32_t ring_count;
    struct comp971_cb_record ring[COMP971_SNAPSHOT_RING_SIZE];
    uint32_t crc32;
};

#define COMP971_COMMIT_STATE_EMPTY     0x00000000U
#define COMP971_COMMIT_STATE_WRITING   0x57524954U /* "WRIT" */
#define COMP971_COMMIT_STATE_COMMITTED 0x434F4D4DU /* "COMM" */

/* Bounded retention policy: snapshot must be published at least this many times before being eligible for overwrite */
#define COMP971_FROZEN_RETENTION_MIN_PUBLISH_COUNT 2U

/* Immutable frozen pre-reset snapshot record (56 bytes, covers pre-reset failure) */
struct comp971_frozen_record {
    uint32_t commit_state;        /* Bytes 0..3: commit state word EXCLUDED from CRC */
    uint32_t pre_reset_boot_id;   /* Byte 4: start of CRC-protected immutable payload */
    uint16_t trigger_flags;       /* Bit 0 = callback_age, Bit 1 = block_progress */
    uint8_t  escalation_reached;
    uint8_t  reserved1;
    uint32_t stall_duration_ms;
    uint32_t last_block_index;
    int32_t  min_lead_dtu;
    uint32_t late_tx_count;
    uint32_t sys_status_lo;
    uint32_t sys_status_hi;
    struct comp971_cb_record recent_cbs[2]; /* 16 bytes */
    uint32_t crc32;               /* Bytes 52..55: CRC covers bytes 4..51 (48 bytes) */
} __attribute__((packed));

#define COMP971_FROZEN_CRC_START_OFFSET offsetof(struct comp971_frozen_record, pre_reset_boot_id)
#define COMP971_FROZEN_CRC_PAYLOAD_LEN  (offsetof(struct comp971_frozen_record, crc32) - COMP971_FROZEN_CRC_START_OFFSET)

uint32_t comp971_calc_crc32(const uint8_t *data, size_t length);

static inline uint32_t comp971_calc_frozen_crc(const volatile struct comp971_frozen_record *rec)
{
    return comp971_calc_crc32(
        ((const uint8_t *)rec) + COMP971_FROZEN_CRC_START_OFFSET,
        COMP971_FROZEN_CRC_PAYLOAD_LEN);
}

/* Separate delivery and retention metadata (never modifies frozen evidence) */
struct comp971_frozen_delivery_meta {
    uint32_t publish_attempts;
    uint32_t last_publish_tick_ms;
    uint16_t subsequent_resets_seen;
    uint16_t missed_snapshots_count;
    uint32_t first_post_reset_reason; /* Captured ONLY on first boot following original freeze */
    /* Retained diagnostic indicators captured safely at stall escalation boundary */
    uint8_t  stall_irq_pin_level;      /* DW3000 IRQ GPIO input pin level (P1.02) */
    uint8_t  stall_uwb_task_state;     /* FreeRTOS eTaskGetState(g_comp971_uwb_task_handle) */
    uint16_t reserved_diag;
    uint32_t stall_dw3000_tx_count;    /* Driver volatile progress counter g_dw3000_tx_count */
    uint32_t stall_fira_cb_seq;        /* FiRa notification sequence counter g_fira_cb_sequence */
    uint32_t stall_dw3000_irq_count;   /* DW3000 ISR entry count at trip */
    uint32_t stall_cb_last_irq_count;  /* DW3000 ISR entry count at last successful FiRa callback */
    uint32_t stall_cb_last_tx_count;   /* Driver TX progress counter at last successful FiRa callback */
    /* Extended TX-path results captured at existing driver processing boundary */
    uint32_t stall_last_tx_info;        /* Packed [31:24]fastcmd [23:16]mode [15:8]result [7:0]hpdwarn */
    uint32_t stall_last_tx_sys_state;   /* DW3000 SYS_STATE_LO register captured at last TX */
    uint32_t stall_last_tx_status;      /* DW3000 SYS_STATUS byte 3 captured at last TX */
    int32_t  stall_last_tx_diff;        /* Scheduled TX lead (dx_time - cur_sys) in DTU */
    uint32_t stall_last_tx_seq;         /* Monotonic TX sequence number at last TX command */
    uint32_t crc32;
};

extern volatile struct comp971_retained_block g_comp971_retained;
extern volatile struct comp971_frozen_record g_comp971_frozen;
extern volatile struct comp971_frozen_delivery_meta g_comp971_frozen_meta;
extern volatile uint32_t g_dw3000_irq_count;
extern volatile uint32_t g_fira_cb_last_irq_count;
extern volatile uint32_t g_fira_cb_last_tx_count;
extern volatile uint32_t g_fira_last_block_index;
extern volatile uint32_t g_fira_stall_recovery_count;
extern volatile uint32_t g_fira_stage1_recovery_count;
extern volatile uint32_t g_fira_stage2_recovery_count;
extern volatile uint32_t g_fira_stage3_recovery_count;

void comp971_uwb_two_node_platform_init(void);
void comp971_uwb_two_node_init(void);
void comp971_uwb_two_node_start(void);
void comp971_uwb_two_node_publish_pending_event(void);

uint32_t comp971_calc_crc32(const uint8_t *data, size_t length);
bool comp971_uwb_stage1_restart(void);
bool comp971_uwb_stage2_mac_restart(void);
uint32_t comp971_uwb_get_last_cb_tick(void);
bool comp971_uwb_is_active(void);
void comp971_uwb_record_stall_snapshot(uint8_t stage, uint32_t duration_ms, uint16_t trigger_flags);

#endif /* COMP971_UWB_TWO_NODE_H */
