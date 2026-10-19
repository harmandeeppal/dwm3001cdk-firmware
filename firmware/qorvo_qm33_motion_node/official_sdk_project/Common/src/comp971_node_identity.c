#include "comp971_node_identity.h"
#include "comp971_config_shared.h"
#include "comp971_uwb_two_node.h"
#include "nrf.h"
#include <string.h>

volatile uint8_t  g_comp971_node_index = 0xFFU;
volatile uint8_t  g_comp971_uwb_role = COMP971_UWB_DISABLED;
volatile uint16_t g_comp971_short_addr = 0xFFFFU;
const char       *g_comp971_device_name = "COMP971-UNKNOWN";
volatile uint16_t g_comp971_restart_jitter_ms = 0U;
volatile bool     g_comp971_identity_valid = false;
volatile uint32_t g_comp971_config_hash = 0U;

void comp971_identify_node(void)
{
    /* 1. Calculate configuration CRC32 hash across all session parameters and controlee addresses */
    struct comp971_session_config_manifest manifest;
    memset(&manifest, 0, sizeof(manifest));
    manifest.session_id = COMP971_CONFIG_SESSION_ID;
    manifest.channel_number = COMP971_CONFIG_CHANNEL_NUMBER;
    manifest.preamble_code_index = COMP971_CONFIG_PREAMBLE_CODE_INDEX;
    manifest.slot_duration_rstu = COMP971_CONFIG_SLOT_DURATION_RSTU;
    manifest.block_duration_ms = COMP971_CONFIG_BLOCK_DURATION_MS;
    manifest.round_duration_slots = COMP971_CONFIG_ROUND_DURATION_SLOTS;
    manifest.sfd_id = COMP971_CONFIG_SFD_ID;
    manifest.rframe_config = COMP971_CONFIG_RFRAME_CONFIG;
    manifest.preamble_duration = COMP971_CONFIG_PREAMBLE_DURATION;
    manifest.sts_segments = COMP971_CONFIG_STS_SEGMENTS;
    manifest.sts_length = COMP971_CONFIG_STS_LENGTH;
    manifest.controller_short_addr = COMP971_CONTROLLER_SHORT_ADDR;
    for (size_t i = 0; i < COMP971_MAX_STAR_PAIRS; ++i)
    {
        manifest.controlee_addrs[i] = g_controlee_addrs[i];
    }
    g_comp971_config_hash = comp971_calc_crc32((const uint8_t *)&manifest, sizeof(manifest));

    /* 2. Read factory read-only FICR registers */
    uint32_t dev_id0 = NRF_FICR->DEVICEID[0];
    uint32_t dev_id1 = NRF_FICR->DEVICEID[1];

    /* 3. Look up hardware entry in table */
    for (size_t i = 0; i < COMP971_NODE_TABLE_COUNT; ++i)
    {
        if ((g_comp971_node_table[i].deviceid_0 == dev_id0) &&
            (g_comp971_node_table[i].deviceid_1 == dev_id1))
        {
            g_comp971_node_index = g_comp971_node_table[i].node_index;
            g_comp971_uwb_role = g_comp971_node_table[i].role;
            g_comp971_short_addr = g_comp971_node_table[i].short_addr;
            g_comp971_device_name = g_comp971_node_table[i].ble_adv_name;
            g_comp971_restart_jitter_ms = g_comp971_node_table[i].restart_jitter_ms;
            g_comp971_identity_valid = true;
            return;
        }
    }

    /* Fallback: Unknown board, disable UWB */
    g_comp971_node_index = 0xFFU;
    g_comp971_uwb_role = COMP971_UWB_DISABLED;
    g_comp971_short_addr = 0xFFFFU;
    g_comp971_device_name = "COMP971-UNKNOWN";
    g_comp971_restart_jitter_ms = 0U;
    g_comp971_identity_valid = false;
}

