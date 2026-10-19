#ifndef COMP971_CONFIG_SHARED_H
#define COMP971_CONFIG_SHARED_H

#include <stdint.h>
#include <stdbool.h>

/* FiRa Session Constants matching validated build exactly */
#define COMP971_CONFIG_SESSION_ID            42U
#define COMP971_CONFIG_CHANNEL_NUMBER        9U
#define COMP971_CONFIG_PREAMBLE_CODE_INDEX   10U
#define COMP971_CONFIG_SLOT_DURATION_RSTU    2400U /* 2.0 ms */
#define COMP971_CONFIG_BLOCK_DURATION_MS     200U  /* 5.0 Hz */
#define COMP971_CONFIG_ROUND_DURATION_SLOTS  25U   /* 50 ms active */
#define COMP971_CONFIG_SFD_ID                2U
#define COMP971_CONFIG_RFRAME_CONFIG         1U    /* SP1 */
#define COMP971_CONFIG_PREAMBLE_DURATION     64U
#define COMP971_CONFIG_STS_SEGMENTS          1U
#define COMP971_CONFIG_STS_LENGTH            64U
#define COMP971_CONFIG_VUPPER64_LEN          8U

#define COMP971_MAX_STAR_PAIRS               5U
#define COMP971_CONTROLLER_SHORT_ADDR        0x0001U

/* Macro role definitions */
#define COMP971_UWB_DISABLED                 0U
#define COMP971_UWB_CONTROLLER               1U
#define COMP971_UWB_RESPONDER                2U

/* Hardware device identity entry */
struct comp971_device_id_entry {
    uint32_t deviceid_0;
    uint32_t deviceid_1;
    uint8_t  node_index;
    uint8_t  role;              /* COMP971_UWB_CONTROLLER or COMP971_UWB_RESPONDER */
    uint16_t short_addr;
    const char *node_name;
    const char *ble_adv_name;
    uint16_t restart_jitter_ms; /* Non-colliding controlee timeout jitter */
};

/* Device ID table for the 6 boards */
static const struct comp971_device_id_entry g_comp971_node_table[] = {
    { 0x0CF7323CU, 0x9B303470U, 0U, COMP971_UWB_RESPONDER,  0x0002U, "HEAD",        "COMP971-MOTION-HEAD",        0U },
    { 0x8BF1BFD6U, 0xDD79EFD4U, 1U, COMP971_UWB_CONTROLLER, 0x0001U, "PELVIS",      "COMP971-MOTION-PELVIS",      0U },
    { 0x641B24BEU, 0x3A7A30ECU, 2U, COMP971_UWB_RESPONDER,  0x0003U, "LEFT-WRIST",  "COMP971-MOTION-LEFT-WRIST",  500U },
    { 0xE901480FU, 0xD0AE5C49U, 3U, COMP971_UWB_RESPONDER,  0x0004U, "RIGHT-WRIST", "COMP971-MOTION-RIGHT-WRIST", 750U },
    { 0x289AA3B2U, 0xF3FD23C3U, 4U, COMP971_UWB_RESPONDER,  0x0005U, "LEFT-ANKLE",  "COMP971-MOTION-LEFT-ANKLE",  1000U },
    { 0x04A5CFAAU, 0xDCF3A587U, 5U, COMP971_UWB_RESPONDER,  0x0006U, "RIGHT-ANKLE", "COMP971-MOTION-RIGHT-ANKLE", 1250U }
};

#define COMP971_NODE_TABLE_COUNT (sizeof(g_comp971_node_table) / sizeof(g_comp971_node_table[0]))

/* Controlee short addresses in fixed stack order */
static const uint16_t g_controlee_addrs[COMP971_MAX_STAR_PAIRS] = {
    0x0002U, /* HEAD (Slot 1 in session) */
    0x0003U, /* LEFT-WRIST (Slot 2 in session) */
    0x0004U, /* RIGHT-WRIST (Slot 3) */
    0x0005U, /* LEFT-ANKLE (Slot 4) */
    0x0006U  /* RIGHT-ANKLE (Slot 5) */
};

/* Compile-time/runtime configuration structure for CRC verification */
struct comp971_session_config_manifest {
    uint32_t session_id;
    uint8_t  channel_number;
    uint8_t  preamble_code_index;
    uint16_t slot_duration_rstu;
    uint16_t block_duration_ms;
    uint8_t  round_duration_slots;
    uint8_t  sfd_id;
    uint8_t  rframe_config;
    uint8_t  preamble_duration;
    uint8_t  sts_segments;
    uint8_t  sts_length;
    uint16_t controller_short_addr;
    uint16_t controlee_addrs[COMP971_MAX_STAR_PAIRS];
};

#endif /* COMP971_CONFIG_SHARED_H */

