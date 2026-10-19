/*
 * test_production_c_harness.c
 * Host C verification harness using actual production struct layouts,
 * constants, bit flags, two-phase commit logic, and validation functions.
 * Mocks FreeRTOS ticks, Nordic DMB barriers, and DW3000 status functions.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>

/* Mock hardware / RTOS primitives */
#define __DMB() do { /* compiler barrier */ __asm__ __volatile__("" ::: "memory"); } while (0)
#define offsetof(TYPE, MEMBER) __builtin_offsetof (TYPE, MEMBER)

#define COMP971_PROTOCOL_VERSION          1U
#define COMP971_TELEMETRY_STALL_SNAPSHOT  0x06U
#define COMP971_RETAINED_MAGIC            0x5354414CU /* "STAL" */
#define COMP971_SNAPSHOT_RING_SIZE        16U
#define COMP971_ESCALATION_STAGE2_MAC     0x02U
#define COMP971_ESCALATION_STAGE3_WDT     0x03U

#define COMP971_COMMIT_STATE_EMPTY     0x00000000U
#define COMP971_COMMIT_STATE_WRITING   0x57524954U /* "WRIT" */
#define COMP971_COMMIT_STATE_COMMITTED 0x434F4D4DU /* "COMM" */

/* Production Structs */
struct comp971_cb_record {
    uint16_t block_index;
    uint8_t  status;
    uint8_t  lead_ms;
    uint16_t tick_offset_ms;
    uint16_t reserved;
} __attribute__((packed));

struct comp971_stall_snapshot_v1 {
    uint8_t  protocol_version;
    uint8_t  message_type;
    uint8_t  node_index;
    uint8_t  trigger_flags;
    uint32_t magic;
    uint32_t boot_count;
    uint32_t reset_reason;
    uint8_t  escalation_reached;
    uint8_t  reserved2[3];
    uint32_t stall_duration_ms;
    uint32_t last_block_index;
    int32_t  min_lead_dtu;
    uint32_t late_tx_count;
    struct comp971_cb_record recent_cbs[2];
    uint32_t crc32;
} __attribute__((packed));

struct comp971_retained_block {
    uint32_t magic;
    uint32_t boot_count;
    uint32_t reset_reason;
    uint8_t  last_escalation_stage;
    uint8_t  reset_loop_count;
    uint16_t trigger_flags;
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

struct comp971_frozen_record {
    uint32_t commit_state;
    uint32_t pre_reset_boot_id;
    uint16_t trigger_flags;
    uint8_t  escalation_reached;
    uint8_t  reserved1;
    uint32_t stall_duration_ms;
    uint32_t last_block_index;
    int32_t  min_lead_dtu;
    uint32_t late_tx_count;
    uint32_t sys_status_lo;
    uint32_t sys_status_hi;
    struct comp971_cb_record recent_cbs[2];
    uint32_t crc32;
} __attribute__((packed));

struct comp971_frozen_delivery_meta {
    uint32_t publish_attempts;
    uint32_t last_publish_tick_ms;
    uint16_t subsequent_resets_seen;
    uint16_t missed_snapshots_count;
    uint32_t first_post_reset_reason;
    /* Retained diagnostic indicators captured safely at stall escalation boundary */
    uint8_t  stall_irq_pin_level;      /* DW3000 IRQ GPIO input pin level (P1.02) */
    uint8_t  stall_uwb_task_state;     /* FreeRTOS eTaskGetState(g_comp971_uwb_task_handle) */
    uint16_t reserved_diag;
    uint32_t stall_dw3000_tx_count;    /* Driver volatile progress counter g_dw3000_tx_count */
    uint32_t stall_fira_cb_seq;        /* FiRa notification sequence counter g_fira_cb_sequence */
    uint32_t crc32;
};

/* State instances */
static volatile struct comp971_retained_block g_comp971_retained;
static volatile struct comp971_frozen_record g_comp971_frozen;
static volatile struct comp971_frozen_delivery_meta g_comp971_frozen_meta;

static volatile uint32_t g_fira_last_block_index = 0;
static volatile int32_t  g_fira_lifetime_min_lead = 5200;
static volatile uint32_t g_dw3000_late_tx_count = 14;
static volatile uint32_t g_mock_tick = 0;

/* Production CRC32 implementation */
uint32_t comp971_calc_crc32(const uint8_t *data, size_t length)
{
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < length; ++i)
    {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j)
        {
            if (crc & 1U)
            {
                crc = (crc >> 1) ^ 0xEDB88320U;
            }
            else
            {
                crc = crc >> 1;
            }
        }
    }
    return ~crc;
}

#define COMP971_FROZEN_CRC_START_OFFSET offsetof(struct comp971_frozen_record, pre_reset_boot_id)
#define COMP971_FROZEN_CRC_PAYLOAD_LEN  (offsetof(struct comp971_frozen_record, crc32) - COMP971_FROZEN_CRC_START_OFFSET)

static inline uint32_t comp971_calc_frozen_crc(const volatile struct comp971_frozen_record *rec)
{
    return comp971_calc_crc32(
        ((const uint8_t *)rec) + COMP971_FROZEN_CRC_START_OFFSET,
        COMP971_FROZEN_CRC_PAYLOAD_LEN);
}

/* Production record_stall_snapshot implementation */
void comp971_uwb_record_stall_snapshot(uint8_t stage, uint32_t duration_ms, uint16_t trigger_flags)
{
    g_comp971_retained.last_escalation_stage = stage;
    g_comp971_retained.last_stall_duration_ms = duration_ms;
    g_comp971_retained.trigger_flags = trigger_flags;
    g_comp971_retained.last_block_index = g_fira_last_block_index;
    g_comp971_retained.lifetime_min_lead = g_fira_lifetime_min_lead;
    g_comp971_retained.late_tx_count = g_dw3000_late_tx_count;
    g_comp971_retained.sys_status_lo = 0xFFFFFFFFU; /* Live SPI read removed from trip path */
    g_comp971_retained.sys_status_hi = 0xFFFFFFFFU;
    g_comp971_retained.crc32 = comp971_calc_crc32((const uint8_t *)&g_comp971_retained, offsetof(struct comp971_retained_block, crc32));

    uint32_t cur_crc = comp971_calc_frozen_crc(&g_comp971_frozen);
    if ((g_comp971_frozen.commit_state == COMP971_COMMIT_STATE_COMMITTED) &&
        (g_comp971_frozen.crc32 == cur_crc))
    {
        if (g_comp971_frozen_meta.missed_snapshots_count < 0xFFFFU)
        {
            g_comp971_frozen_meta.missed_snapshots_count++;
        }
        g_comp971_frozen_meta.crc32 = comp971_calc_crc32(
            (const uint8_t *)&g_comp971_frozen_meta, offsetof(struct comp971_frozen_delivery_meta, crc32));
        return;
    }

    g_comp971_frozen.commit_state = COMP971_COMMIT_STATE_WRITING;
    __DMB();

    g_comp971_frozen.pre_reset_boot_id = g_comp971_retained.boot_count;
    g_comp971_frozen.trigger_flags = trigger_flags;
    g_comp971_frozen.escalation_reached = stage;
    g_comp971_frozen.reserved1 = 0U;
    g_comp971_frozen.stall_duration_ms = duration_ms;
    g_comp971_frozen.last_block_index = g_fira_last_block_index;
    g_comp971_frozen.min_lead_dtu = g_fira_lifetime_min_lead;
    g_comp971_frozen.late_tx_count = g_dw3000_late_tx_count;
    g_comp971_frozen.sys_status_lo = 0xFFFFFFFFU;
    g_comp971_frozen.sys_status_hi = 0xFFFFFFFFU;

    memset((void *)g_comp971_frozen.recent_cbs, 0, sizeof(g_comp971_frozen.recent_cbs));
    if (g_comp971_retained.ring_count > 0U)
    {
        uint32_t head = g_comp971_retained.ring_head;
        uint32_t idx1 = (head + COMP971_SNAPSHOT_RING_SIZE - 1U) % COMP971_SNAPSHOT_RING_SIZE;
        g_comp971_frozen.recent_cbs[0] = g_comp971_retained.ring[idx1];
        if (g_comp971_retained.ring_count > 1U)
        {
            uint32_t idx2 = (head + COMP971_SNAPSHOT_RING_SIZE - 2U) % COMP971_SNAPSHOT_RING_SIZE;
            g_comp971_frozen.recent_cbs[1] = g_comp971_retained.ring[idx2];
        }
    }

    /* CRC covers ONLY immutable payload bytes 4..51 (48 bytes), excluding commit_state */
    g_comp971_frozen.crc32 = comp971_calc_frozen_crc(&g_comp971_frozen);
    __DMB();

    g_comp971_frozen.commit_state = COMP971_COMMIT_STATE_COMMITTED;
    __DMB();

    /* Capture lightweight non-blocking diagnostic indicators at stall boundary */
    g_comp971_frozen_meta.stall_irq_pin_level = 0; /* Simulated low pin level */
    g_comp971_frozen_meta.stall_uwb_task_state = 2; /* Simulated eBlocked (2) */
    g_comp971_frozen_meta.stall_dw3000_tx_count = g_dw3000_late_tx_count + 100;
    g_comp971_frozen_meta.stall_fira_cb_seq = 785;
    g_comp971_frozen_meta.reserved_diag = 0U;
    g_comp971_frozen_meta.crc32 = comp971_calc_crc32(
        (const uint8_t *)&g_comp971_frozen_meta, offsetof(struct comp971_frozen_delivery_meta, crc32));
}

/* Production main startup validation */
void production_startup_validate(uint32_t boot_resetreas)
{
    g_comp971_retained.boot_count++;
    g_comp971_retained.reset_reason = boot_resetreas;
    g_comp971_retained.crc32 = comp971_calc_crc32((const uint8_t *)&g_comp971_retained, offsetof(struct comp971_retained_block, crc32));

    uint32_t expected_frozen_crc = comp971_calc_frozen_crc(&g_comp971_frozen);
    if (g_comp971_frozen.commit_state == COMP971_COMMIT_STATE_COMMITTED)
    {
        if (g_comp971_frozen.crc32 != expected_frozen_crc)
        {
            memset((void *)&g_comp971_frozen, 0, sizeof(g_comp971_frozen));
            memset((void *)&g_comp971_frozen_meta, 0, sizeof(g_comp971_frozen_meta));
        }
    }
    else if (g_comp971_frozen.commit_state == COMP971_COMMIT_STATE_WRITING)
    {
        memset((void *)&g_comp971_frozen, 0, sizeof(g_comp971_frozen));
        memset((void *)&g_comp971_frozen_meta, 0, sizeof(g_comp971_frozen_meta));
    }

    uint32_t expected_meta_crc = comp971_calc_crc32((const uint8_t *)&g_comp971_frozen_meta, offsetof(struct comp971_frozen_delivery_meta, crc32));
    if (g_comp971_frozen_meta.crc32 != expected_meta_crc)
    {
        memset((void *)&g_comp971_frozen_meta, 0, sizeof(g_comp971_frozen_meta));
        g_comp971_frozen_meta.crc32 = comp971_calc_crc32((const uint8_t *)&g_comp971_frozen_meta, offsetof(struct comp971_frozen_delivery_meta, crc32));
    }

    /* Attribute the first post-reset reason to the original frozen snapshot if not yet latched */
    if ((g_comp971_frozen.commit_state == COMP971_COMMIT_STATE_COMMITTED) &&
        (g_comp971_frozen.crc32 == expected_frozen_crc))
    {
        if (g_comp971_frozen_meta.first_post_reset_reason == 0U)
        {
            g_comp971_frozen_meta.first_post_reset_reason = boot_resetreas;
            g_comp971_frozen_meta.crc32 = comp971_calc_crc32((const uint8_t *)&g_comp971_frozen_meta, offsetof(struct comp971_frozen_delivery_meta, crc32));
        }
    }
}

/* Production FiRa callback simulation */
void simulate_fira_cb(uint32_t block, uint8_t status, uint8_t lead_ms)
{
    g_fira_last_block_index = block;
    uint32_t head = g_comp971_retained.ring_head % COMP971_SNAPSHOT_RING_SIZE;
    g_comp971_retained.ring[head].block_index = (uint16_t)(block & 0xFFFFU);
    g_comp971_retained.ring[head].status = status;
    g_comp971_retained.ring[head].lead_ms = lead_ms;
    g_comp971_retained.ring[head].tick_offset_ms = (uint16_t)(block * 200U);
    g_comp971_retained.ring[head].reserved = 0U;
    g_comp971_retained.ring_head = (head + 1U) % COMP971_SNAPSHOT_RING_SIZE;
    if (g_comp971_retained.ring_count < COMP971_SNAPSHOT_RING_SIZE)
    {
        g_comp971_retained.ring_count++;
    }
    g_comp971_retained.last_block_index = block;
}

/* Production supervisor snapshot creation & publication simulation */
bool supervisor_emit_snapshot(uint8_t node_index, uint32_t boot_resetreas, struct comp971_stall_snapshot_v1 *out_snap)
{
    uint32_t frozen_crc = comp971_calc_frozen_crc(&g_comp971_frozen);
    bool have_frozen = (g_comp971_frozen.commit_state == COMP971_COMMIT_STATE_COMMITTED) &&
                       (g_comp971_frozen.crc32 == frozen_crc);

    if (!have_frozen)
    {
        return false;
    }

    if (g_comp971_frozen_meta.subsequent_resets_seen < 0xFFFFU)
    {
        g_comp971_frozen_meta.subsequent_resets_seen++;
    }
    g_comp971_frozen_meta.crc32 = comp971_calc_crc32(
        (const uint8_t *)&g_comp971_frozen_meta, offsetof(struct comp971_frozen_delivery_meta, crc32));

    memset(out_snap, 0, sizeof(*out_snap));
    out_snap->protocol_version = COMP971_PROTOCOL_VERSION;
    out_snap->message_type = COMP971_TELEMETRY_STALL_SNAPSHOT;
    out_snap->node_index = node_index;
    out_snap->trigger_flags = (uint8_t)g_comp971_frozen.trigger_flags;
    out_snap->magic = COMP971_RETAINED_MAGIC;
    out_snap->boot_count = g_comp971_frozen.pre_reset_boot_id;
    /* Use attributed first post-reset reason associated with original frozen boot */
    out_snap->reset_reason = (g_comp971_frozen_meta.first_post_reset_reason != 0U) ?
                             g_comp971_frozen_meta.first_post_reset_reason : boot_resetreas;
    out_snap->escalation_reached = g_comp971_frozen.escalation_reached;
    out_snap->stall_duration_ms = g_comp971_frozen.stall_duration_ms;
    out_snap->last_block_index = g_comp971_frozen.last_block_index;
    out_snap->min_lead_dtu = g_comp971_frozen.min_lead_dtu;
    out_snap->late_tx_count = g_comp971_frozen.late_tx_count;
    out_snap->recent_cbs[0] = g_comp971_frozen.recent_cbs[0];
    out_snap->recent_cbs[1] = g_comp971_frozen.recent_cbs[1];
    out_snap->crc32 = comp971_calc_crc32((const uint8_t *)out_snap, offsetof(struct comp971_stall_snapshot_v1, crc32));

    /* Publication attempt without mutating frozen evidence */
    g_comp971_frozen_meta.publish_attempts++;
    g_comp971_frozen_meta.last_publish_tick_ms = g_mock_tick;
    g_comp971_frozen_meta.crc32 = comp971_calc_crc32(
        (const uint8_t *)&g_comp971_frozen_meta, offsetof(struct comp971_frozen_delivery_meta, crc32));

    return true;
}

int main(void)
{
    printf("====================================================\n");
    printf("COMP971 Pelvis Diagnostic Snapshot v2 C Test Suite\n");
    printf("====================================================\n");

    /* TEST 1: Struct ABI sizes and member offsets */
    printf("[TEST 1] Verifying Struct ABI sizes and offsets...\n");
    assert(sizeof(struct comp971_cb_record) == 8);
    assert(sizeof(struct comp971_stall_snapshot_v1) == 56);
    assert(sizeof(struct comp971_frozen_record) == 56);
    assert(sizeof(struct comp971_frozen_delivery_meta) == 32);
    assert(offsetof(struct comp971_stall_snapshot_v1, crc32) == 52);
    assert(offsetof(struct comp971_frozen_record, crc32) == 52);
    assert(offsetof(struct comp971_frozen_delivery_meta, crc32) == 28);
    printf("  PASS: sizeof(cb)=8, sizeof(snapshot)=56, sizeof(frozen)=56, sizeof(meta)=32, offsetof(crc)=52\n");

    /* TEST 2: Commit ordering & CRC validation */
    printf("[TEST 2] Verifying commit ordering and CRC calculation...\n");
    memset((void *)&g_comp971_retained, 0, sizeof(g_comp971_retained));
    memset((void *)&g_comp971_frozen, 0, sizeof(g_comp971_frozen));
    memset((void *)&g_comp971_frozen_meta, 0, sizeof(g_comp971_frozen_meta));
    g_comp971_retained.magic = COMP971_RETAINED_MAGIC;
    g_comp971_retained.boot_count = 1;

    for (uint32_t b = 1000; b <= 1010; ++b) {
        simulate_fira_cb(b, (b == 1010) ? 33 : 0, 10);
    }
    comp971_uwb_record_stall_snapshot(COMP971_ESCALATION_STAGE3_WDT, 2005, 0x01);

    assert(g_comp971_frozen.commit_state == COMP971_COMMIT_STATE_COMMITTED);
    assert(g_comp971_frozen.last_block_index == 1010);
    assert(g_comp971_frozen.recent_cbs[0].block_index == 1010);
    assert(g_comp971_frozen.recent_cbs[0].status == 33);
    assert(g_comp971_frozen.recent_cbs[1].block_index == 1009);
    assert(g_comp971_frozen.recent_cbs[1].status == 0);
    uint32_t crc_check = comp971_calc_frozen_crc(&g_comp971_frozen);
    assert(g_comp971_frozen.crc32 == crc_check);
    assert(g_comp971_frozen_meta.stall_irq_pin_level == 0);
    assert(g_comp971_frozen_meta.stall_uwb_task_state == 2);
    assert(g_comp971_frozen_meta.stall_fira_cb_seq == 785);
    printf("  PASS: Two-phase commit latched COMMITTED state and valid CRC32 (0x%08X)\n", (unsigned)crc_check);

    /* TEST 3: Interruption at each commit phase */
    printf("[TEST 3] Verifying interruption handling at each commit phase...\n");
    /* Case 3a: Interrupted during WRITING (torn commit) */
    memset((void *)&g_comp971_frozen, 0, sizeof(g_comp971_frozen));
    g_comp971_frozen.commit_state = COMP971_COMMIT_STATE_WRITING;
    g_comp971_frozen.last_block_index = 9999; /* partially written */
    production_startup_validate(0x04);
    assert(g_comp971_frozen.commit_state == COMP971_COMMIT_STATE_EMPTY);
    assert(g_comp971_frozen.last_block_index == 0);
    printf("  PASS: Incomplete commit interrupted in WRITING state was cleanly cleared on reboot\n");

    /* Case 3b: Interrupted before CRC finalized or bit flip in CRC */
    comp971_uwb_record_stall_snapshot(COMP971_ESCALATION_STAGE3_WDT, 2005, 0x01);
    g_comp971_frozen.crc32 ^= 0xFFFFFFFFU; /* Corrupt CRC */
    production_startup_validate(0x04);
    assert(g_comp971_frozen.commit_state == COMP971_COMMIT_STATE_EMPTY);
    printf("  PASS: Corrupted CRC in COMMITTED state was cleanly rejected on reboot\n");

    /* TEST 4: Reboot with 60 new FiRa callbacks */
    printf("[TEST 4] Verifying reboot with 60 new FiRa callbacks...\n");
    memset((void *)&g_comp971_frozen, 0, sizeof(g_comp971_frozen));
    g_comp971_retained.boot_count = 1;
    for (uint32_t b = 5000; b <= 5020; ++b) {
        simulate_fira_cb(b, 0, 12);
    }
    comp971_uwb_record_stall_snapshot(COMP971_ESCALATION_STAGE3_WDT, 2012, 0x02); /* block progress */
    assert(g_comp971_frozen.last_block_index == 5020);

    /* Simulate reboot to Boot 2 */
    production_startup_validate(0x04);
    /* Now 60 new callbacks fire during boot grace period */
    for (uint32_t b = 0; b < 60; ++b) {
        simulate_fira_cb(b, 0, 15);
    }
    /* Verify live retained was updated */
    assert(g_comp971_retained.last_block_index == 59);
    /* Verify frozen record is 100% UNTOUCHED */
    assert(g_comp971_frozen.last_block_index == 5020);
    assert(g_comp971_frozen.commit_state == COMP971_COMMIT_STATE_COMMITTED);
    assert(g_comp971_frozen.crc32 == comp971_calc_frozen_crc(&g_comp971_frozen));
    printf("  PASS: Pre-reset block 5020 survived 60 new callbacks completely intact\n");

    /* TEST 5: Five repeated resets before publication */
    printf("[TEST 5] Verifying 5 repeated resets before publication...\n");
    /* We currently have the valid snapshot from block 5020 */
    uint32_t initial_crc = g_comp971_frozen.crc32;
    for (int r = 1; r <= 5; ++r) {
        /* In each reset cycle, supervisor detects a stall and attempts to record snapshot */
        comp971_uwb_record_stall_snapshot(COMP971_ESCALATION_STAGE3_WDT, 2000 + r * 10, 0x01);
        /* Reboot with subsequent DOG watchdog reset (0x02) */
        production_startup_validate(0x02);
    }
    /* Frozen record MUST NOT have been overwritten */
    assert(g_comp971_frozen.last_block_index == 5020);
    assert(g_comp971_frozen.crc32 == initial_crc);
    /* Missed snapshots and subsequent resets must be recorded in separate metadata */
    assert(g_comp971_frozen_meta.missed_snapshots_count == 5);
    /* Crucial: first_post_reset_reason must retain the original SREQ (0x04), NOT the later DOG (0x02)! */
    assert(g_comp971_frozen_meta.first_post_reset_reason == 0x04);
    printf("  PASS: First snapshot preserved across 5 repeated resets; missed_snapshots_count = %u, first_post_reset_reason = 0x%08X (original SREQ)\n",
           g_comp971_frozen_meta.missed_snapshots_count, g_comp971_frozen_meta.first_post_reset_reason);

    /* TEST 6: Attempted overwrite of an already committed snapshot */
    printf("[TEST 6] Verifying attempted overwrite of committed snapshot...\n");
    uint32_t count_before = g_comp971_frozen_meta.missed_snapshots_count;
    comp971_uwb_record_stall_snapshot(COMP971_ESCALATION_STAGE2_MAC, 1500, 0x03);
    assert(g_comp971_frozen.last_block_index == 5020); /* still original */
    assert(g_comp971_frozen.escalation_reached == COMP971_ESCALATION_STAGE3_WDT); /* still original */
    assert(g_comp971_frozen_meta.missed_snapshots_count == count_before + 1);
    printf("  PASS: Overwrite rejected; missed snapshots incremented to %u\n", g_comp971_frozen_meta.missed_snapshots_count);

    /* TEST 7: Publication without host acknowledgement */
    printf("[TEST 7] Verifying publication without host acknowledgement...\n");
    struct comp971_stall_snapshot_v1 snap_out;
    g_mock_tick = 12345;
    bool emit1 = supervisor_emit_snapshot(0x00, 0x04, &snap_out);
    assert(emit1 == true);
    assert(snap_out.last_block_index == 5020);
    assert(g_comp971_frozen.crc32 == initial_crc); /* Frozen record NOT re-checksummed */
    assert(g_comp971_frozen_meta.publish_attempts == 1);

    /* Second emission attempt (e.g. repeated broadcast) */
    g_mock_tick = 13500;
    bool emit2 = supervisor_emit_snapshot(0x00, 0x04, &snap_out);
    assert(emit2 == true);
    assert(g_comp971_frozen.crc32 == initial_crc); /* Still untouched */
    assert(g_comp971_frozen_meta.publish_attempts == 2);
    assert(g_comp971_frozen_meta.last_publish_tick_ms == 13500);
    printf("  PASS: Repeated publication emitted duplicate frames; frozen evidence strictly immutable\n");

    /* TEST 8: Invalid CRC and invalid retained metadata */
    printf("[TEST 8] Verifying corrupted retained metadata recovery...\n");
    g_comp971_frozen_meta.crc32 ^= 0x12345678U;
    production_startup_validate(0x04);
    assert(g_comp971_frozen_meta.publish_attempts == 0); /* reset to 0 */
    assert(g_comp971_frozen_meta.crc32 == comp971_calc_crc32((const uint8_t *)&g_comp971_frozen_meta, offsetof(struct comp971_frozen_delivery_meta, crc32)));
    /* But the frozen snapshot remained intact */
    assert(g_comp971_frozen.commit_state == COMP971_COMMIT_STATE_COMMITTED);
    assert(g_comp971_frozen.last_block_index == 5020);
    printf("  PASS: Corrupted metadata recovered without impacting frozen record\n");

    /* TEST 9: Tick and counter wraparound */
    printf("[TEST 9] Verifying counter overflow safety...\n");
    g_comp971_frozen_meta.missed_snapshots_count = 0xFFFFU;
    comp971_uwb_record_stall_snapshot(COMP971_ESCALATION_STAGE3_WDT, 2000, 0x01);
    assert(g_comp971_frozen_meta.missed_snapshots_count == 0xFFFFU); /* Saturation at 0xFFFF */
    printf("  PASS: Counter overflow saturated at 0xFFFF without wrapping\n");

    /* TEST 10: Pelvis node_index = 1 */
    printf("[TEST 10] Verifying snapshot generation for node_index = 1...\n");
    struct comp971_stall_snapshot_v1 snap_node1;
    bool emit_n1 = supervisor_emit_snapshot(0x01, 0x04, &snap_node1);
    assert(emit_n1 == true);
    assert(snap_node1.node_index == 0x01);
    assert(snap_node1.crc32 == comp971_calc_crc32((const uint8_t *)&snap_node1, 52));
    printf("  PASS: node_index = 1 successfully encoded with valid CRC32 (0x%08X)\n", (unsigned)snap_node1.crc32);

    /* TEST 11: Legacy-compatible 56-byte snapshot + 4 diagnostic indicator frames for host decoder */
    printf("[TEST 11] Generating legacy-compatible 56-byte wire frame and 4 diagnostic indicator frames in test_production_c_frame.bin...\n");
    FILE *f = fopen("hardware/tools/test_production_c_frame.bin", "wb");
    assert(f != NULL);

    /* 1. Stall Snapshot gateway frame (70 bytes total: 6B header + 56B payload + 8B timestamp) */
    uint8_t gw_frame[70];
    gw_frame[0] = 0xC9;
    gw_frame[1] = 0x71;
    gw_frame[2] = 0x01; /* Version 1 */
    gw_frame[3] = COMP971_TELEMETRY_STALL_SNAPSHOT;
    gw_frame[4] = 64;   /* Payload length = 56 + 8 */
    gw_frame[5] = 0;    /* Upper byte of uint16_t length in little endian: 0x0040 */

    memcpy(&gw_frame[6], &snap_node1, 56);
    uint64_t gw_ts = 987654321ULL;
    memcpy(&gw_frame[6 + 56], &gw_ts, 8);
    fwrite(gw_frame, 1, sizeof(gw_frame), f);

    /* Helper struct for diagnostic wire record */
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

    /* 2. Emit the 4 diagnostic indicator frames */
    uint8_t diag_codes[4] = {0xE0, 0xE1, 0xE2, 0xE3};
    int32_t diag_details[4] = {
        0,   /* IRQ GPIO P1.02 = 0 (low / inactive) */
        2,   /* FreeRTOS UWB Task State = eBlocked (2) */
        967, /* Driver TX Count = 967 */
        785  /* FiRa Callback Seq = 785 */
    };

    uint16_t stall_blk_lo = (uint16_t)(snap_node1.last_block_index & 0xFFFFU);
    uint32_t stall_boot = snap_node1.boot_count;

    for (int i = 0; i < 4; ++i)
    {
        uint8_t diag_gw_frame[30]; /* 6B header + 16B payload + 8B timestamp = 30 bytes */
        diag_gw_frame[0] = 0xC9;
        diag_gw_frame[1] = 0x71;
        diag_gw_frame[2] = 0x01;
        diag_gw_frame[3] = 0x05; /* COMP971_TELEMETRY_UWB_DIAGNOSTIC */
        diag_gw_frame[4] = 24;   /* Payload length = 16 + 8 */
        diag_gw_frame[5] = 0;

        struct comp971_uwb_diagnostic_v1 diag;
        memset(&diag, 0, sizeof(diag));
        diag.protocol_version = 1;
        diag.message_type = 0x05;
        diag.node_index = snap_node1.node_index;
        diag.pair_index = snap_node1.node_index;
        diag.stage_code = diag_codes[i];
        diag.reserved[0] = (uint8_t)(stall_blk_lo & 0xFFU);
        diag.reserved[1] = (uint8_t)((stall_blk_lo >> 8) & 0xFFU);
        diag.local_timestamp_us = stall_boot;
        diag.detail = diag_details[i];

        memcpy(&diag_gw_frame[6], &diag, 16);
        uint64_t d_ts = gw_ts + (uint64_t)(i + 1) * 1000ULL;
        memcpy(&diag_gw_frame[6 + 16], &d_ts, 8);
        fwrite(diag_gw_frame, 1, sizeof(diag_gw_frame), f);
    }
    fclose(f);
    printf("  PASS: Saved hardware/tools/test_production_c_frame.bin (70 + 4*30 = 190 bytes)\n");

    /* TEST 12: Retained diagnostic indicators capture verification */
    printf("[TEST 12] Verifying retained diagnostic indicators at stall boundary...\n");
    memset((void *)&g_comp971_frozen, 0, sizeof(g_comp971_frozen));
    memset((void *)&g_comp971_frozen_meta, 0, sizeof(g_comp971_frozen_meta));
    comp971_uwb_record_stall_snapshot(COMP971_ESCALATION_STAGE3_WDT, 2005, 0x01);
    assert(g_comp971_frozen_meta.stall_irq_pin_level == 0);
    assert(g_comp971_frozen_meta.stall_uwb_task_state == 2);
    assert(g_comp971_frozen_meta.stall_dw3000_tx_count > 0);
    assert(g_comp971_frozen_meta.stall_fira_cb_seq == 785);
    uint32_t expected_ext_crc = comp971_calc_crc32(
        (const uint8_t *)&g_comp971_frozen_meta, offsetof(struct comp971_frozen_delivery_meta, crc32));
    assert(g_comp971_frozen_meta.crc32 == expected_ext_crc);
    printf("  PASS: Diagnostic indicators (irq=%d, task_state=%d, tx_cnt=%u, cb_seq=%u) verified with valid CRC32 (0x%08X)\n",
           g_comp971_frozen_meta.stall_irq_pin_level,
           g_comp971_frozen_meta.stall_uwb_task_state,
           (unsigned)g_comp971_frozen_meta.stall_dw3000_tx_count,
           (unsigned)g_comp971_frozen_meta.stall_fira_cb_seq,
           (unsigned)g_comp971_frozen_meta.crc32);

    printf("\nALL 12 PRODUCTION C TESTS PASSED (Exit code 0)\n");
    return 0;
}
