/*
 * test_c_retention_harness.c
 * Standalone offline test proving:
 * 1. The original overwrite defect (live g_comp971_retained overwritten by new-boot callbacks).
 * 2. The fix: immutable frozen pre-reset record (g_comp971_frozen) survives 60 post-boot callbacks intact.
 * 3. Exact 56-byte layout and binary generation for host decoder verification.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>

#define COMP971_PROTOCOL_VERSION          1U
#define COMP971_TELEMETRY_STALL_SNAPSHOT  0x06U
#define COMP971_RETAINED_MAGIC            0x5354414CU /* "STAL" */
#define COMP971_SNAPSHOT_RING_SIZE        16U
#define COMP971_FROZEN_VALID_MARKER       0xA55A0001U
#define COMP971_ESCALATION_STAGE3_WDT     0x03U

struct comp971_cb_record {
    uint16_t block_index;
    uint8_t  status;
    uint8_t  lead_ms;
    uint16_t tick_offset_ms;
    uint16_t reserved;
} __attribute__((packed));

struct comp971_stall_snapshot_v1 {
    uint8_t  protocol_version;    /* offset 0 */
    uint8_t  message_type;        /* offset 1 */
    uint8_t  node_index;          /* offset 2 */
    uint8_t  trigger_flags;       /* offset 3 */
    uint32_t magic;               /* offset 4 */
    uint32_t boot_count;          /* offset 8 */
    uint32_t reset_reason;        /* offset 12 */
    uint8_t  escalation_reached;  /* offset 16 */
    uint8_t  reserved2[3];        /* offset 17..19 */
    uint32_t stall_duration_ms;   /* offset 20 */
    uint32_t last_block_index;    /* offset 24 */
    int32_t  min_lead_dtu;        /* offset 28 */
    uint32_t late_tx_count;       /* offset 32 */
    struct comp971_cb_record recent_cbs[2]; /* offset 36..51 (16 bytes) */
    uint32_t crc32;               /* offset 52..55 */
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
    uint32_t valid_marker;
    uint32_t pre_reset_boot_id;
    uint16_t trigger_flags;
    uint8_t  escalation_reached;
    uint8_t  published;
    uint32_t stall_duration_ms;
    uint32_t last_block_index;
    int32_t  min_lead_dtu;
    uint32_t late_tx_count;
    uint32_t sys_status_lo;
    uint32_t sys_status_hi;
    struct comp971_cb_record recent_cbs[2];
    uint32_t crc32;
} __attribute__((packed));

/* Standard CRC32/ISO-HDLC */
static uint32_t calc_crc32(const uint8_t *data, size_t length)
{
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < length; ++i) {
        crc ^= (uint32_t)data[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc >> 1) ^ (0xEDB88320U & (-(int)(crc & 1U)));
        }
    }
    return ~crc;
}

/* Simulated RAM state */
static struct comp971_retained_block g_retained;
static struct comp971_frozen_record   g_frozen;
static uint32_t g_fira_last_block_index = 0;
static int32_t  g_fira_lifetime_min_lead = 5200;
static uint32_t g_dw3000_late_tx_count = 14;

static void simulate_fira_callback(uint32_t block, uint8_t status, uint8_t lead_ms)
{
    g_fira_last_block_index = block;

    /* Original code writing into live g_retained */
    uint32_t head = g_retained.ring_head % COMP971_SNAPSHOT_RING_SIZE;
    g_retained.ring[head].block_index = (uint16_t)(block & 0xFFFFU);
    g_retained.ring[head].status = status;
    g_retained.ring[head].lead_ms = lead_ms;
    g_retained.ring[head].tick_offset_ms = (uint16_t)(block * 200U);
    g_retained.ring[head].reserved = 0U;
    g_retained.ring_head = (head + 1U) % COMP971_SNAPSHOT_RING_SIZE;
    if (g_retained.ring_count < COMP971_SNAPSHOT_RING_SIZE) {
        g_retained.ring_count++;
    }
    g_retained.last_block_index = block;
}

static void record_stall_snapshot(uint8_t stage, uint32_t duration_ms, uint16_t trigger_flags)
{
    g_retained.last_escalation_stage = stage;
    g_retained.last_stall_duration_ms = duration_ms;
    g_retained.trigger_flags = trigger_flags;
    g_retained.last_block_index = g_fira_last_block_index;
    g_retained.lifetime_min_lead = g_fira_lifetime_min_lead;
    g_retained.late_tx_count = g_dw3000_late_tx_count;
    g_retained.sys_status_lo = 0x00000002U;
    g_retained.sys_status_hi = 0x00000008U;
    g_retained.crc32 = calc_crc32((const uint8_t *)&g_retained, offsetof(struct comp971_retained_block, crc32));

    /* Immutable frozen record two-phase commit */
    g_frozen.valid_marker = 0U;
    g_frozen.pre_reset_boot_id = g_retained.boot_count;
    g_frozen.trigger_flags = trigger_flags;
    g_frozen.escalation_reached = stage;
    g_frozen.published = 0U;
    g_frozen.stall_duration_ms = duration_ms;
    g_frozen.last_block_index = g_fira_last_block_index;
    g_frozen.min_lead_dtu = g_fira_lifetime_min_lead;
    g_frozen.late_tx_count = g_dw3000_late_tx_count;
    g_frozen.sys_status_lo = g_retained.sys_status_lo;
    g_frozen.sys_status_hi = g_retained.sys_status_hi;

    memset((void *)g_frozen.recent_cbs, 0, sizeof(g_frozen.recent_cbs));
    if (g_retained.ring_count > 0U) {
        uint32_t head = g_retained.ring_head;
        uint32_t idx1 = (head + COMP971_SNAPSHOT_RING_SIZE - 1U) % COMP971_SNAPSHOT_RING_SIZE;
        g_frozen.recent_cbs[0] = g_retained.ring[idx1];
        if (g_retained.ring_count > 1U) {
            uint32_t idx2 = (head + COMP971_SNAPSHOT_RING_SIZE - 2U) % COMP971_SNAPSHOT_RING_SIZE;
            g_frozen.recent_cbs[1] = g_retained.ring[idx2];
        }
    }
    g_frozen.valid_marker = COMP971_FROZEN_VALID_MARKER;
    g_frozen.crc32 = calc_crc32((const uint8_t *)&g_frozen, offsetof(struct comp971_frozen_record, crc32));
}

int main(void)
{
    /* 1. Assert struct ABI sizes and alignments */
    if (sizeof(struct comp971_cb_record) != 8) {
        fprintf(stderr, "FAIL: comp971_cb_record != 8\n");
        return 1;
    }
    if (sizeof(struct comp971_stall_snapshot_v1) != 56) {
        fprintf(stderr, "FAIL: comp971_stall_snapshot_v1 != 56 (is %u)\n", (unsigned)sizeof(struct comp971_stall_snapshot_v1));
        return 1;
    }
    if (sizeof(struct comp971_frozen_record) != 56) {
        fprintf(stderr, "FAIL: comp971_frozen_record != 56 (is %u)\n", (unsigned)sizeof(struct comp971_frozen_record));
        return 1;
    }
    if (offsetof(struct comp971_stall_snapshot_v1, crc32) != 52) {
        fprintf(stderr, "FAIL: snapshot crc32 offset != 52\n");
        return 1;
    }
    if (offsetof(struct comp971_frozen_record, crc32) != 52) {
        fprintf(stderr, "FAIL: frozen crc32 offset != 52\n");
        return 1;
    }

    printf("ABI checks passed: sizeof(snapshot)=56, sizeof(frozen)=56, offsetof(crc)=52\n");

    /* 2. Setup pre-reset state on Boot 1 */
    memset(&g_retained, 0, sizeof(g_retained));
    memset(&g_frozen, 0, sizeof(g_frozen));
    g_retained.magic = COMP971_RETAINED_MAGIC;
    g_retained.boot_count = 1;

    /* Simulate 50 ranging blocks prior to stall */
    for (uint32_t b = 1400; b <= 1450; ++b) {
        uint8_t status = (b == 1450) ? 33 : 0; /* timeout at 1450 */
        simulate_fira_callback(b, status, 12);
    }

    /* Verify pre-reset state at block 1450 */
    assert(g_retained.last_block_index == 1450);
    assert(g_retained.ring_count == 16);

    /* Stall occurs! Supervisor trips at 2000 ms gap */
    record_stall_snapshot(COMP971_ESCALATION_STAGE3_WDT, 2010, 0x01); /* Bit 0: cb age */

    /* Verify frozen contents immediately after commit */
    assert(g_frozen.valid_marker == COMP971_FROZEN_VALID_MARKER);
    assert(g_frozen.pre_reset_boot_id == 1);
    assert(g_frozen.last_block_index == 1450);
    assert(g_frozen.recent_cbs[0].block_index == 1450);
    assert(g_frozen.recent_cbs[0].status == 33);
    assert(g_frozen.recent_cbs[1].block_index == 1449);
    assert(g_frozen.recent_cbs[1].status == 0);

    /* 3. Simulate Reboot -> Boot 2 */
    /* Retained memory remains intact, global non-retained variables re-init */
    g_fira_last_block_index = 0;
    uint32_t boot2_resetreas = 0x00000004U; /* SREQ */

    /* During startup, main() increments boot count */
    g_retained.boot_count = 2;

    /* 4. During 5000 ms boot grace period, FiRa callbacks start running! */
    /* 5.0 Hz callback rate * 5.0 seconds = 25 callbacks. Let us simulate 60 callbacks. */
    for (uint32_t new_b = 0; new_b < 60; ++new_b) {
        simulate_fira_callback(new_b, 0, 15);
    }

    /* 5. PROVE THE DEFECT IN g_retained: */
    /* In naive architecture, g_retained was overwritten! */
    printf("[Proof of Defect] Naive g_retained.last_block_index is now: %u (Overwritten!)\n", (unsigned)g_retained.last_block_index);
    printf("[Proof of Defect] Naive g_retained.ring[0].block_index is now: %u (Overwritten!)\n", (unsigned)g_retained.ring[0].block_index);
    assert(g_retained.last_block_index == 59);

    /* 6. PROVE THE FIX IN g_frozen: */
    /* Frozen record was completely untouched by the 60 new callbacks! */
    uint32_t expected_frozen_crc = calc_crc32((const uint8_t *)&g_frozen, offsetof(struct comp971_frozen_record, crc32));
    assert(g_frozen.valid_marker == COMP971_FROZEN_VALID_MARKER);
    assert(g_frozen.crc32 == expected_frozen_crc);
    assert(g_frozen.pre_reset_boot_id == 1);
    assert(g_frozen.last_block_index == 1450);
    assert(g_frozen.recent_cbs[0].block_index == 1450);
    assert(g_frozen.recent_cbs[0].status == 33);
    assert(g_frozen.recent_cbs[1].block_index == 1449);
    assert(g_frozen.recent_cbs[1].status == 0);
    assert(g_frozen.trigger_flags == 0x01);
    assert(g_frozen.stall_duration_ms == 2010);
    assert(g_frozen.escalation_reached == COMP971_ESCALATION_STAGE3_WDT);

    printf("[Proof of Immunity] g_frozen.last_block_index remains: %u (IMMUTABLE)\n", (unsigned)g_frozen.last_block_index);
    printf("[Proof of Immunity] g_frozen.recent_cbs[0].block_index: %u (IMMUTABLE)\n", (unsigned)g_frozen.recent_cbs[0].block_index);
    printf("[Proof of Immunity] g_frozen CRC32 valid: 0x%08X\n", (unsigned)g_frozen.crc32);

    /* 7. Simulate supervisor publishing outgoing 56-byte snapshot frame */
    struct comp971_stall_snapshot_v1 snap;
    memset(&snap, 0, sizeof(snap));
    snap.protocol_version = COMP971_PROTOCOL_VERSION;
    snap.message_type = COMP971_TELEMETRY_STALL_SNAPSHOT;
    snap.node_index = 0x00U; /* Pelvis */
    snap.trigger_flags = (uint8_t)g_frozen.trigger_flags;
    snap.magic = COMP971_RETAINED_MAGIC;
    snap.boot_count = g_frozen.pre_reset_boot_id;
    snap.reset_reason = boot2_resetreas; /* SREQ without modifying frozen */
    snap.escalation_reached = g_frozen.escalation_reached;
    snap.stall_duration_ms = g_frozen.stall_duration_ms;
    snap.last_block_index = g_frozen.last_block_index;
    snap.min_lead_dtu = g_frozen.min_lead_dtu;
    snap.late_tx_count = g_frozen.late_tx_count;
    snap.recent_cbs[0] = g_frozen.recent_cbs[0];
    snap.recent_cbs[1] = g_frozen.recent_cbs[1];
    snap.crc32 = calc_crc32((const uint8_t *)&snap, offsetof(struct comp971_stall_snapshot_v1, crc32));

    /* Mark frozen as published without mutating evidence */
    g_frozen.published = 1U;
    g_frozen.crc32 = calc_crc32((const uint8_t *)&g_frozen, offsetof(struct comp971_frozen_record, crc32));

    printf("Generated 56-byte outgoing snapshot wire frame. CRC32=0x%08X\n", (unsigned)snap.crc32);

    /* 8. Write gateway frame to disk for decode_gateway_capture.py testing */
    /* Gateway frame format:
     * Header (6 bytes): Sync0 (0xAA), Sync1 (0x55), MsgType (0x06), Length (64 bytes = 56 + 8), Checksum_Hi, Checksum_Lo
     * Payload (64 bytes): 56 bytes snapshot + 8 bytes gateway timestamp
     */
    uint8_t gw_frame[70];
    gw_frame[0] = 0xAA;
    gw_frame[1] = 0x55;
    gw_frame[2] = 0x06;
    gw_frame[3] = 64; /* 56 + 8 */

    /* Payload */
    memcpy(&gw_frame[6], &snap, 56);
    uint64_t gw_ts = 123456789ULL;
    memcpy(&gw_frame[6 + 56], &gw_ts, 8);

    /* 16-bit Fletcher/Additive checksum over payload */
    uint16_t csum = 0;
    for (size_t i = 6; i < 70; ++i) {
        csum = (uint16_t)(csum + gw_frame[i]);
    }
    gw_frame[4] = (uint8_t)(csum >> 8);
    gw_frame[5] = (uint8_t)(csum & 0xFF);

    FILE *f = fopen("test_c_retention_frame.bin", "wb");
    if (!f) {
        fprintf(stderr, "Failed to open test_c_retention_frame.bin\n");
        return 1;
    }
    fwrite(gw_frame, 1, sizeof(gw_frame), f);
    fclose(f);

    printf("Saved 70-byte gateway frame to test_c_retention_frame.bin\n");
    return 0;
}

