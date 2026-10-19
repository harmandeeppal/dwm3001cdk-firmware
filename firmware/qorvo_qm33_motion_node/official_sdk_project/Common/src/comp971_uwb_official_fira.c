/*
 * COMP971 fixed two-board FiRa adapter.
 *
 * This is deliberately a small application-level adapter over Qorvo's
 * unmodified FiRa, UWB-MAC and DW3110 layers. Its ordering is taken from
 * Qorvo's Src/Apps/Src/fira/fira_app.c:
 * UWB-MAC init -> helper open -> scheduler -> measurement sequence -> session
 * init -> parameters -> controlee -> UWB-MAC start -> session start.
 *
 * All settings remain in RAM. This file contains no raw UWB frame handling,
 * no antenna delay, OTP, UICR, NVM, calibration or runtime flash operation.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "common_fira.h"
#include "deca_device_api.h"
#include "deca_error.h"
#include "fira_default_params.h"
#include "nrf_gpio.h"
#include "persistent_time.h"
#include "qtime.h"
#include "task.h"
#include "net/fira_region_params.h"
#include "uwbmac/fira_helper.h"
#include "uwbmac/uwbmac.h"

#include "comp971_ble_pilot.h"
#include "comp971_flash_guard.h"
#include "comp971_telemetry_protocol.h"
#include "comp971_uwb_two_node.h"
#include "comp971_config_shared.h"
#include "comp971_node_identity.h"

#define COMP971_UWB_PAIR_INDEX 0U
#define COMP971_UWB_SESSION_ID COMP971_CONFIG_SESSION_ID

/* C1-C5 are successful lifecycle milestones. D1-D9 identify the first
 * official API call that failed, so a zero-attempt result is diagnosable. */
#define STAGE_MAC_INIT       COMP971_ERR_MAC_INIT
#define STAGE_HELPER_OPEN    COMP971_ERR_HELPER_OPEN
#define STAGE_SCHEDULER      COMP971_ERR_SCHEDULER
#define STAGE_MEASUREMENT    COMP971_ERR_MEASUREMENT
#define STAGE_SESSION_INIT   COMP971_ERR_SESSION_INIT
#define STAGE_SESSION_PARAMS COMP971_ERR_SESSION_PARAMS
#define STAGE_ADD_CONTROLEE  COMP971_ERR_ADD_CONTROLEE
#define STAGE_MAC_START      COMP971_ERR_MAC_START
#define STAGE_SESSION_START  COMP971_ERR_SESSION_START

#define PROGRESS_MAC_READY      COMP971_PROG_MAC_READY
#define PROGRESS_SCHEDULE_READY COMP971_PROG_SCHED_READY
#define PROGRESS_SESSION_READY  COMP971_PROG_SESSION_READY
#define PROGRESS_MAC_STARTED    COMP971_PROG_MAC_STARTED
#define PROGRESS_SESSION_STARTED COMP971_PROG_SESSION_STARTED

__attribute__((section(".retained_data"))) volatile struct comp971_retained_block g_comp971_retained;
__attribute__((section(".retained_data"))) volatile struct comp971_frozen_record g_comp971_frozen;
__attribute__((section(".retained_data"))) volatile struct comp971_frozen_delivery_meta g_comp971_frozen_meta;

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

#if COMP971_UWB_ROLE != COMP971_UWB_DISABLED
static struct uwbmac_context *m_uwbmac_ctx;
static struct fira_context m_fira_ctx;
static struct session_parameters m_session;
static uint32_t m_session_handle;
static bool m_platform_ready;
static bool m_prepared;
static bool m_start_requested;
static bool m_failure_pending;
static uint8_t m_failure_stage;
static int32_t m_failure_detail;
static bool m_progress_pending;
static uint8_t m_progress_stage;
static volatile enum quwbs_fbs_session_state m_session_state = QUWBS_FBS_SESSION_STATE_INIT;
static volatile enum quwbs_fbs_reason_code m_last_reason_code = QUWBS_FBS_REASON_CODE_SUCCESS;
static volatile bool m_session_status_pending;
static volatile uint32_t m_cb_call_count;
static volatile uint32_t m_cb_last_type;
static volatile int32_t m_cb_last_status = -999;
static volatile int32_t m_cb_last_n_meas = -999;
static volatile uint8_t m_cb_last_slot_index = 0xFF;
static volatile uint8_t m_cb_last_stopped = 0xFF;
static volatile uint16_t m_cb_last_short_addr = 0xFFFF;
static volatile uint32_t m_cb_last_block_index = 0xFFFFFFFF;
static volatile int32_t m_cb_last_diag_nb_reports = -1;
static volatile uint8_t m_cb_last_diag_msg_id = 0xFF;
static volatile uint8_t m_cb_last_diag_action = 0xFF;
static volatile uint16_t m_cb_last_diag_extra_status = 0xFFFF;
static volatile uint8_t m_cb_last_diag_ant_set = 0xFF;
static volatile uint8_t m_cb_last_diag_msg_id_1 = 0xFF;
static volatile uint8_t m_cb_last_diag_action_1 = 0xFF;
static volatile uint16_t m_cb_last_diag_extra_status_1 = 0xFFFF;

extern volatile uint32_t g_dw3000_tx_count;
extern volatile uint32_t g_dw3000_tx_hpdwarn_count;
extern volatile int32_t g_dw3000_last_tx_diff;
extern volatile int32_t g_dw3000_min_tx_diff;
extern volatile uint32_t g_dw3000_late_tx_count;
extern volatile uint8_t g_dw3000_last_tx_status;
extern volatile uint8_t  g_dw3000_last_tx_fastcmd;
extern volatile uint8_t  g_dw3000_last_tx_mode;
extern volatile uint8_t  g_dw3000_last_tx_result;
extern volatile uint8_t  g_dw3000_last_tx_hpdwarn_seen;
extern volatile uint32_t g_dw3000_last_tx_sys_state;
extern volatile uint32_t g_dw3000_last_tx_seq;
extern volatile uint32_t g_interface_tx_count;
extern volatile uint32_t g_interface_rx_count;

#define COMP971_STATUS_TIMELINE_LEN 350U
volatile uint8_t g_fira_status_timeline[COMP971_STATUS_TIMELINE_LEN];
volatile uint16_t g_fira_timeline_count = 0;
volatile uint32_t g_fira_first_block_index = 0xFFFFFFFF;
volatile uint32_t g_fira_last_block_index = 0;
volatile uint32_t g_fira_hist_total_blocks = 0;
volatile uint32_t g_fira_hist_success = 0;      /* Status 0: QUWBS_FBS_STATUS_RANGING_SUCCESS */
volatile uint32_t g_fira_hist_tx_failed = 0;    /* Status 32: QUWBS_FBS_STATUS_RANGING_TX_FAILED */
volatile uint32_t g_fira_hist_rx_timeout = 0;   /* Status 33: QUWBS_FBS_STATUS_RANGING_RX_TIMEOUT */
volatile uint32_t g_fira_hist_rx_phy_dec = 0;   /* Status 34: QUWBS_FBS_STATUS_RANGING_RX_PHY_DEC_FAILED */
volatile uint32_t g_fira_hist_other = 0;

#define COMP971_FAIL_RING_SIZE 32U
volatile uint32_t g_fira_fail_block_indices[COMP971_FAIL_RING_SIZE];
volatile uint8_t g_fira_fail_statuses[COMP971_FAIL_RING_SIZE];
volatile uint16_t g_fira_fail_ring_count = 0;

/* Steady-state window tracking (Blocks 10..N-15) and session timing */
volatile uint32_t g_fira_session_start_tick = 0;
volatile uint32_t g_fira_last_cb_tick = 0;
volatile uint32_t g_fira_stall_recovery_count = 0;
volatile uint32_t g_fira_stage1_recovery_count = 0;
volatile uint32_t g_fira_stage2_recovery_count = 0;
volatile uint32_t g_fira_stage3_recovery_count = 0;
volatile uint32_t g_fira_stall_failed_restart_count = 0;
volatile uint32_t g_fira_watchdog_backoff_ms = 0;
volatile uint32_t g_fira_force_stall = 0;
volatile uint32_t g_fira_steady_total = 0;
volatile uint32_t g_fira_steady_fail_count = 0;
volatile uint32_t g_fira_stop_at_block = 0xFFFFFFFF;
volatile int32_t g_fira_lifetime_min_lead = 0x7FFFFFFF;
volatile int32_t g_fira_last_interval_min_lead = 0x7FFFFFFF;

/* Recovery state machine (Version 6.7) - owned exclusively by uwb_task */
enum comp971_rec_state {
    COMP971_REC_IDLE = 0,
    COMP971_REC_COOLDOWN,
    COMP971_REC_BACKOFF,
    COMP971_REC_HOLD
};

#define COMP971_REC_COOLDOWN_MS  5000U
#define COMP971_REC_BACKOFF_MS   2000U
#define COMP971_REC_HOLD_MS      30000U
#define COMP971_REC_MAX_ATTEMPTS 3U

static enum comp971_rec_state m_rec_state = COMP971_REC_IDLE;
static TickType_t m_rec_timer_tick = 0;
static TickType_t m_rec_start_tick = 0;
static uint8_t    m_rec_attempts = 0;
static uint32_t   m_rec_start_seq = 0;
static uint8_t    m_rec_consecutive_valid = 0;
static uint32_t   m_rec_session_cookie = 100U;
static uint32_t   m_rec_last_consumed_valid_seq = 0;

/* Callback Progress Shared Variables (written by fira_notification_cb, read by uwb_task) */
volatile uint32_t g_fira_cb_sequence = 0U;
volatile uint32_t g_fira_valid_sequence = 0U;
volatile uint32_t g_fira_cb_valid_streak = 0U;
volatile uint32_t g_fira_last_cb_cookie = 0U;
volatile uint32_t g_fira_last_valid_cookie = 0U;

/* Passive FiRa Progress Instrumentation (Task B) */
volatile uint32_t g_dw3000_irq_count = 0U;
volatile uint32_t g_fira_cb_last_irq_count = 0U;
volatile uint32_t g_fira_cb_last_tx_count = 0U;

/* Intercept DW3000 ISR entry to count hardware interrupt events */
extern void __real_dwt_isr(void);
void __wrap_dwt_isr(void)
{
    g_dw3000_irq_count++;
    __real_dwt_isr();
}

extern volatile TaskHandle_t g_comp971_status_task_handle;
extern volatile TaskHandle_t g_comp971_imu_task_handle;
extern volatile TaskHandle_t g_comp971_uwb_task_handle;
extern volatile uint32_t g_dw3000_tx_count;
extern volatile uint32_t g_ble_connect_count;
extern volatile uint32_t g_ble_disconnect_count;
extern volatile uint32_t g_ble_queue_items_flushed_on_disconnect;
extern volatile uint32_t g_fira_connect_block_index;
extern volatile uint32_t g_ble_queue_depth_hist[8];

static int32_t get_task_free_bytes(TaskHandle_t handle)
{
    if (handle == NULL)
    {
        return 0;
    }
    return (int32_t)(uxTaskGetStackHighWaterMark(handle) * (UBaseType_t)sizeof(StackType_t));
}

struct comp971_pair_stats {
    uint32_t attempts;
    uint32_t valid;
    uint32_t tx_failed;   /* Status 32 */
    uint32_t rx_timeout;  /* Status 33 */
    uint32_t rx_phy_dec;  /* Status 34 */
    uint32_t other;
};
volatile struct comp971_pair_stats g_star_pair_stats[COMP971_MAX_STAR_PAIRS];

static struct comp971_star_ranges_v1 m_pending_star_ranges;
static volatile bool m_star_ranges_pending = false;

static uint32_t local_timestamp_us(void)
{
    return (uint32_t)(((uint64_t)xTaskGetTickCount() * 1000000U) /
                      configTICK_RATE_HZ);
}

static void record_failure(uint8_t stage, int32_t detail)
{
    m_failure_stage = stage;
    m_failure_detail = detail;
    m_failure_pending = true;
    m_start_requested = false;
    comp971_ble_pilot_set_uwb_started(false);
}

static void record_progress(uint8_t stage)
{
    m_progress_stage = stage;
    m_progress_pending = true;
}

void comp971_uwb_two_node_platform_init(void)
{
#if COMP971_UWB_ROLE != COMP971_UWB_DISABLED
    enum qerr result;

    if (m_platform_ready)
    {
        return;
    }

    result = fira_uwb_mcps_init(&m_uwbmac_ctx);
    if (result != QERR_SUCCESS)
    {
        record_failure(STAGE_MAC_INIT, (int32_t)result);
        return;
    }
    m_platform_ready = true;
#endif
}

/* This is the RAM equivalent of Qorvo fira_set_default_params(). Only the
 * two fixed short addresses differ for the PELVIS/HEAD pilot. */
static void configure_official_defaults(void)
{
    uint8_t index;

    memset(&m_session, 0, sizeof(m_session));

    m_session.rframe_config = FIRA_RFRAME_CONFIG_SP1;
    m_session.sfd_id = FIRA_DEFAULT_SFD_ID;
    m_session.slot_duration_rstu = FIRA_DEFAULT_SLOT_DURATION_RSTU;
    m_session.block_duration_ms = FIRA_DEFAULT_BLOCK_DURATION_MS;
    m_session.round_duration_slots = FIRA_DEFAULT_ROUND_DURATION_SLOTS;
    m_session.ranging_round_usage = FIRA_RANGING_ROUND_USAGE_DSTWR_NON_DEFERRED;
    m_session.multi_node_mode = FIRA_MULTI_NODE_MODE_ONE_TO_MANY;
    m_session.round_hopping = FIRA_DEFAULT_ROUND_HOPPING;
    m_session.schedule_mode = FIRA_SCHEDULE_MODE_TIME_SCHEDULED;
    m_session.preamble_code_index = FIRA_DEFAULT_PREAMBLE_CODE_INDEX;
    m_session.preamble_duration = FIRA_PREAMBLE_DURATION_64;
    m_session.number_of_sts_segments = FIRA_STS_SEGMENTS_1;
    m_session.sts_length = FIRA_STS_LENGTH_64;
    m_session.max_rr_retry = 0U;
    m_session.phr_data_rate = FIRA_PRF_MODE_BPRF;
    m_session.channel_number = FIRA_DEFAULT_CHANNEL_NUMBER;
    m_session.result_report_config =
        fira_helper_bool_to_result_report_config(true, false, false, false);
    m_session.ranging_round_control =
        fira_helper_bool_to_ranging_round_control(true, false);
    m_session.enable_diagnostics = false;
    for (index = 0U; index < FIRA_VUPPER64_SIZE; ++index)
    {
        m_session.vupper64[index] = (uint8_t)(index + 1U);
    }

    if (g_comp971_uwb_role == COMP971_UWB_CONTROLLER)
    {
        m_session.n_destination_short_address = 0U;
        m_session.short_addr = COMP971_CONTROLLER_SHORT_ADDR;
        m_session.device_type = QUWBS_FBS_DEVICE_TYPE_CONTROLLER;
        m_session.device_role = QUWBS_FBS_DEVICE_ROLE_INITIATOR;
    }
    else
    {
        m_session.n_destination_short_address = 1U;
        m_session.destination_short_address[0] = COMP971_CONTROLLER_SHORT_ADDR;
        m_session.short_addr = g_comp971_short_addr;
        m_session.device_type = QUWBS_FBS_DEVICE_TYPE_CONTROLEE;
        m_session.device_role = QUWBS_FBS_DEVICE_ROLE_RESPONDER;
    }
}

static void fira_notification_cb(enum fira_helper_cb_type type,
                                 const void *content, void *user_data)
{
    const struct fira_twr_ranging_results *results;

    (void)user_data;
#if defined(COMP971_TEST_HOOKS) && (COMP971_TEST_HOOKS != 0)
    if (g_fira_force_stall != 0)
    {
        /* Forced stall test hook: do not update g_fira_last_cb_tick to simulate stalled callbacks */
        return;
    }
#endif
    m_cb_call_count++;
    nrf_gpio_pin_toggle(NRF_GPIO_PIN_MAP(0, 4));
    m_cb_last_type = (uint32_t)type;

    if (type == FIRA_HELPER_CB_TYPE_SESSION_STATUS_NTF && content != NULL)
    {
        const struct fbs_helper_session_status_ntf *ntf =
            (const struct fbs_helper_session_status_ntf *)content;
        m_session_state = ntf->state;
        m_last_reason_code = ntf->reason_code;
        /* Stage 0xEx where x = state, detail = reason_code */
        (void)comp971_ble_pilot_publish_uwb_diagnostic(
            COMP971_UWB_PAIR_INDEX, (uint8_t)(0xE0U | (ntf->state & 0x0FU)),
            (int32_t)ntf->reason_code, local_timestamp_us());
        return;
    }

    if (type != FIRA_HELPER_CB_TYPE_TWR_RANGE_NTF || content == NULL)
    {
        return;
    }

    /* Valid ranging report: update coordinator/controlee last callback tick */
    g_fira_last_cb_tick = xTaskGetTickCount();
    g_fira_cb_sequence++;
    g_fira_last_cb_cookie = m_rec_session_cookie;
    g_fira_cb_last_irq_count = g_dw3000_irq_count;
    g_fira_cb_last_tx_count = g_dw3000_tx_count;

    results = (const struct fira_twr_ranging_results *)content;
    m_cb_last_n_meas = (int32_t)results->n_measurements;
    if (results->info != NULL)
    {
        uint32_t cur_block = results->info->block_index;
        m_cb_last_block_index = cur_block;
        if (g_fira_first_block_index == 0xFFFFFFFF)
        {
            g_fira_first_block_index = cur_block;
        }
        g_fira_last_block_index = cur_block;

        if (results->info->diagnostic != NULL)
        {
            m_cb_last_diag_nb_reports = (int32_t)results->info->diagnostic->nb_reports;
            struct frame_report *rep = results->info->diagnostic->reports;
            if (rep != NULL)
            {
                m_cb_last_diag_msg_id = rep->msg_id;
                m_cb_last_diag_action = rep->action;
                m_cb_last_diag_extra_status = rep->extra_status;
                m_cb_last_diag_ant_set = rep->antenna_set;
                if (rep->next != NULL)
                {
                    m_cb_last_diag_msg_id_1 = rep->next->msg_id;
                    m_cb_last_diag_action_1 = rep->next->action;
                    m_cb_last_diag_extra_status_1 = rep->next->extra_status;
                }
            }
        }
    }

    g_fira_hist_total_blocks++;
    uint8_t blk_status = 0xFE;
    if (results->n_measurements > 0)
    {
        m_cb_last_status = (int32_t)results->measurements[0].status;
        m_cb_last_slot_index = results->measurements[0].slot_index;
        m_cb_last_stopped = results->measurements[0].stopped ? 1U : 0U;
        m_cb_last_short_addr = results->measurements[0].short_addr;
        blk_status = (uint8_t)results->measurements[0].status;
    }

    if (blk_status == 0)
    {
        g_fira_hist_success++;
    }
    else
    {
        if (blk_status == 32)
        {
            g_fira_hist_tx_failed++;
        }
        else if (blk_status == 33)
        {
            g_fira_hist_rx_timeout++;
        }
        else if (blk_status == 34)
        {
            g_fira_hist_rx_phy_dec++;
        }
        else
        {
            g_fira_hist_other++;
        }

        uint16_t ring_idx = g_fira_fail_ring_count % COMP971_FAIL_RING_SIZE;
        g_fira_fail_block_indices[ring_idx] = (results->info != NULL) ? results->info->block_index : 0U;
        g_fira_fail_statuses[ring_idx] = blk_status;
        g_fira_fail_ring_count++;
    }

    /* Steady-state window tracking: predefined as block_index >= 10 */
    if (results->info != NULL && results->info->block_index >= 10U)
    {
        g_fira_steady_total++;
        if (blk_status != 0)
        {
            g_fira_steady_fail_count++;
        }
    }

    if (g_fira_timeline_count < COMP971_STATUS_TIMELINE_LEN)
    {
        g_fira_status_timeline[g_fira_timeline_count++] = blk_status;
    }

    /* Record in retained RAM 16-entry callback ring buffer */
    {
        uint32_t head = g_comp971_retained.ring_head % COMP971_SNAPSHOT_RING_SIZE;
        g_comp971_retained.ring[head].block_index = (uint16_t)(g_fira_last_block_index & 0xFFFFU);
        g_comp971_retained.ring[head].status = blk_status;
        g_comp971_retained.ring[head].lead_ms = (g_dw3000_last_tx_diff > 0) ? (uint8_t)(g_dw3000_last_tx_diff / 15600) : 0U;
        g_comp971_retained.ring[head].tick_offset_ms = (uint16_t)(((xTaskGetTickCount() * 33000ULL) / 32768ULL) & 0xFFFFU);
        g_comp971_retained.ring[head].reserved = 0U;
        g_comp971_retained.ring_head = (head + 1U) % COMP971_SNAPSHOT_RING_SIZE;
        if (g_comp971_retained.ring_count < COMP971_SNAPSHOT_RING_SIZE)
        {
            g_comp971_retained.ring_count++;
        }
        g_comp971_retained.last_block_index = g_fira_last_block_index;
        g_comp971_retained.lifetime_min_lead = g_fira_lifetime_min_lead;
        g_comp971_retained.late_tx_count = g_dw3000_late_tx_count;
        g_comp971_retained.crc32 = comp971_calc_crc32((const uint8_t *)&g_comp971_retained, offsetof(struct comp971_retained_block, crc32));
    }

    if (g_comp971_uwb_role == COMP971_UWB_CONTROLLER)
    {
        m_pending_star_ranges.protocol_version = COMP971_PROTOCOL_VERSION;
        m_pending_star_ranges.message_type = COMP971_TELEMETRY_STAR_RANGES;
        m_pending_star_ranges.node_index = (uint8_t)g_comp971_node_index;
        m_pending_star_ranges.reserved = 0U;
        m_pending_star_ranges.pelvis_timestamp_us = local_timestamp_us();
        m_pending_star_ranges.block_index = (results->info != NULL) ? results->info->block_index : 0U;

        /* Initialize all 5 pairs to unmeasured / omitted from SDK callback.
         * status = 0xFF indicates no result was returned by Qorvo SDK for this pair.
         * flags bit 0 (0x01) = valid distance (status == 0)
         * flags bit 1 (0x02) = measurement was present in SDK callback measurements[]
         * If the SDK omits a controlee, flags bit 1 remains 0 and status remains 0xFF.
         * If the SDK returns an explicit timeout, status will be 33 and flags bit 1 will be 1.
         */
        for (uint8_t p = 0; p < COMP971_MAX_STAR_PAIRS; p++)
        {
            m_pending_star_ranges.pairs[p].range_cm = -1;
            m_pending_star_ranges.pairs[p].status = 0xFFU; /* 0xFF = OMITTED by SDK callback */
            m_pending_star_ranges.pairs[p].flags = 0U;
        }

        bool any_valid = false;
        for (int i = 0; i < results->n_measurements && i < FIRA_RESPONDERS_MAX; i++)
        {
            const struct fira_twr_measurements *meas = &results->measurements[i];
            uint8_t p_idx = 0xFF;
            if (meas->short_addr == 0x0002U)
            {
                p_idx = 0U; /* HEAD (Pair 0) */
            }
            else if (meas->short_addr >= 0x0003U && meas->short_addr <= 0x0006U)
            {
                p_idx = (uint8_t)(meas->short_addr - 0x0002U);
            }

            if (p_idx < COMP971_MAX_STAR_PAIRS)
            {
                uint8_t pair_flags = 0x02U; /* Bit 1: Present in Qorvo SDK callback */
                bool is_valid = (meas->status == QUWBS_FBS_STATUS_RANGING_SUCCESS);
                if (is_valid)
                {
                    any_valid = true;
                    m_pending_star_ranges.pairs[p_idx].range_cm = (int16_t)meas->distance_cm;
                    pair_flags |= 0x01U; /* Bit 0: Valid distance */
                }
                else
                {
                    m_pending_star_ranges.pairs[p_idx].range_cm = -1;
                }
                m_pending_star_ranges.pairs[p_idx].status = (uint8_t)meas->status;
                m_pending_star_ranges.pairs[p_idx].flags = pair_flags;
            }
        }

        /* Update cumulative per-pair counters */
        for (uint8_t p = 0; p < COMP971_MAX_STAR_PAIRS; p++)
        {
            g_star_pair_stats[p].attempts++;
            if ((m_pending_star_ranges.pairs[p].flags & 1U) != 0U)
            {
                g_star_pair_stats[p].valid++;
            }
            else
            {
                uint8_t st = m_pending_star_ranges.pairs[p].status;
                if (st == 32U)
                {
                    g_star_pair_stats[p].tx_failed++;
                }
                else if (st == 33U)
                {
                    g_star_pair_stats[p].rx_timeout++;
                }
                else if (st == 34U)
                {
                    g_star_pair_stats[p].rx_phy_dec++;
                }
                else
                {
                    g_star_pair_stats[p].other++;
                }
            }
        }

    comp971_ble_pilot_record_uwb_result(any_valid);
    m_star_ranges_pending = true;
    if (any_valid)
    {
        g_fira_valid_sequence++;
        g_fira_cb_valid_streak++;
        g_fira_last_valid_cookie = m_rec_session_cookie;
        nrf_gpio_pin_toggle(NRF_GPIO_PIN_MAP(0, 22));
    }
    else
    {
        if (results->n_measurements > 0 &&
            results->measurements[0].status == QUWBS_FBS_STATUS_RANGING_SUCCESS)
        {
            g_fira_valid_sequence++;
            g_fira_cb_valid_streak++;
            g_fira_last_valid_cookie = m_rec_session_cookie;
            nrf_gpio_pin_toggle(NRF_GPIO_PIN_MAP(0, 22));
        }
        else
        {
            /* Explicit invalid round or timeout: immediately break callback success streak */
            g_fira_cb_valid_streak = 0U;
        }
    }
}
}
#endif

void comp971_uwb_two_node_init(void)
{
#if COMP971_UWB_ROLE != COMP971_UWB_DISABLED
    enum qerr result;
    struct fbs_session_init_rsp response;

    m_prepared = false;
    m_start_requested = false;
    m_failure_pending = false;
    m_progress_pending = false;
    comp971_ble_pilot_set_uwb_started(false);
    configure_official_defaults();

    if (!m_platform_ready)
    {
        comp971_uwb_two_node_platform_init();
        if (!m_platform_ready)
        {
            return;
        }
    }
    record_progress(PROGRESS_MAC_READY);

    uwbmac_set_short_addr(m_uwbmac_ctx, m_session.short_addr);
    result = fira_helper_open(&m_fira_ctx, m_uwbmac_ctx, fira_notification_cb,
                              "endless", 0U, NULL);
    if (result != QERR_SUCCESS)
    {
        record_failure(STAGE_HELPER_OPEN, (int32_t)result);
        return;
    }
    result = fira_helper_set_scheduler(&m_fira_ctx);
    if (result != QERR_SUCCESS)
    {
        record_failure(STAGE_SCHEDULER, (int32_t)result);
        return;
    }
    record_progress(PROGRESS_SCHEDULE_READY);

    /* Prepare measurement sequence matching stock fira_app.c (report_required = true) */
    result = fira_prepare_measurement_sequence(m_uwbmac_ctx, &m_session, true);
    if (result != QERR_SUCCESS)
    {
        record_failure(STAGE_MEASUREMENT, (int32_t)result);
        return;
    }
    result = fira_helper_init_session(
        &m_fira_ctx, COMP971_UWB_SESSION_ID,
        QUWBS_FBS_SESSION_TYPE_RANGING_NO_IN_BAND_DATA, &response);
    if (result != QERR_SUCCESS)
    {
        record_failure(STAGE_SESSION_INIT, (int32_t)result);
        return;
    }
    m_session_handle = response.session_handle;
    result = fira_set_session_parameters(&m_fira_ctx, m_session_handle,
                                         &m_session);
    if (result != QERR_SUCCESS)
    {
        record_failure(STAGE_SESSION_PARAMS, (int32_t)result);
        return;
    }

    if (g_comp971_uwb_role == COMP971_UWB_CONTROLLER)
    {
        for (size_t i = 0; i < COMP971_MAX_STAR_PAIRS; i++)
        {
            struct controlee_parameters cp;
            memset(&cp, 0, sizeof(cp));
            cp.address = g_controlee_addrs[i];
            int controlee_result = fira_helper_add_controlee(&m_fira_ctx, m_session_handle, &cp);
            if (controlee_result < 0)
            {
                record_failure(STAGE_ADD_CONTROLEE, (int32_t)controlee_result);
                return;
            }
        }
    }
    m_prepared = true;
    record_progress(PROGRESS_SESSION_READY);
#endif
}

void comp971_uwb_two_node_start(void)
{
#if COMP971_UWB_ROLE != COMP971_UWB_DISABLED
    enum qerr result;

    if (!m_prepared)
    {
        return;
    }

    /* Start MAC and FiRa session synchronously without FreeRTOS tick delay,
     * exactly mirroring stock fira_app_process_start(). */
    result = uwbmac_start(m_uwbmac_ctx);
    if (result != QERR_SUCCESS)
    {
        record_failure(STAGE_MAC_START, (int32_t)result);
        return;
    }
    record_progress(PROGRESS_MAC_STARTED);

    result = fira_helper_start_session(&m_fira_ctx, m_session_handle);
    if (result != QERR_SUCCESS)
    {
        record_failure(STAGE_SESSION_START, (int32_t)result);
        return;
    }

    m_session_state = QUWBS_FBS_SESSION_STATE_ACTIVE;
    m_rec_session_cookie++;
    m_rec_state = COMP971_REC_IDLE;
    m_rec_attempts = 0;
    m_rec_consecutive_valid = 0;
    m_rec_last_consumed_valid_seq = g_fira_valid_sequence;
    g_fira_session_start_tick = xTaskGetTickCount();
    g_fira_last_cb_tick = g_fira_session_start_tick;
    comp971_ble_pilot_set_uwb_started(true);
    nrf_gpio_pin_write(NRF_GPIO_PIN_MAP(0, 22), 0);
    nrf_gpio_pin_write(NRF_GPIO_PIN_MAP(0, 4), 0);
    record_progress(PROGRESS_SESSION_STARTED);

    /* Diagnostic 0xCF: log (result_report_config << 8) | ranging_round_control */
    (void)comp971_ble_pilot_publish_uwb_diagnostic(
        COMP971_UWB_PAIR_INDEX, 0xCFU,
        (int32_t)(((uint32_t)m_session.result_report_config << 8) |
                  (uint32_t)m_session.ranging_round_control),
        local_timestamp_us());
#endif
}

void comp971_uwb_two_node_publish_pending_event(void)
{
#if COMP971_UWB_ROLE != COMP971_UWB_DISABLED

    if (m_failure_pending)
    {
        m_failure_pending = !comp971_ble_pilot_publish_uwb_diagnostic(
            COMP971_UWB_PAIR_INDEX, m_failure_stage, m_failure_detail,
            local_timestamp_us());
    }
    else if (m_progress_pending)
    {
        m_progress_pending = !comp971_ble_pilot_publish_uwb_diagnostic(
            COMP971_UWB_PAIR_INDEX, m_progress_stage, 0, local_timestamp_us());
    }

    if (g_comp971_uwb_role == COMP971_UWB_CONTROLLER)
    {
        if (m_star_ranges_pending)
        {
            if (comp971_ble_pilot_publish_star_ranges(&m_pending_star_ranges))
            {
                m_star_ranges_pending = false;
            }
        }
    }

    if (m_session_status_pending)
    {
        if (comp971_ble_pilot_publish_uwb_diagnostic(
                COMP971_UWB_PAIR_INDEX, 0xEEU, (int32_t)m_last_reason_code,
                local_timestamp_us()))
        {
            m_session_status_pending = false;
        }
    }

    /* Round-robin UWB health telemetry: 1 metric every 1000 ms (1.0 Hz deterministic cadence)
     * Cycle through 16 key metrics every 16 seconds without burst starvation. */
    static uint32_t last_health_report_us = 0;
    static uint8_t health_cycle_step = 0;
    uint32_t now_us = local_timestamp_us();

    if (now_us - last_health_report_us >= 1000000U)
    {
        last_health_report_us = now_us;
        uint16_t snap_block = (uint16_t)(g_fira_last_block_index & 0xFFFFU);
        switch (health_cycle_step)
        {
            case 0:
                (void)comp971_ble_pilot_publish_uwb_diagnostic_ext(
                    COMP971_UWB_PAIR_INDEX, COMP971_HEALTH_TX_COUNT, snap_block, (int32_t)g_dw3000_tx_count, now_us);
                break;
            case 1:
                /* Interval min lead (retained over rolling window) */
                (void)comp971_ble_pilot_publish_uwb_diagnostic_ext(
                    COMP971_UWB_PAIR_INDEX, COMP971_HEALTH_INTERVAL_MIN_LEAD, snap_block, (int32_t)g_dw3000_min_tx_diff, now_us);
                g_fira_last_interval_min_lead = g_dw3000_min_tx_diff;
                if (g_dw3000_min_tx_diff < g_fira_lifetime_min_lead)
                {
                    g_fira_lifetime_min_lead = g_dw3000_min_tx_diff;
                }
                break;
            case 2:
                (void)comp971_ble_pilot_publish_uwb_diagnostic_ext(
                    COMP971_UWB_PAIR_INDEX, COMP971_HEALTH_LATE_TX_COUNT, snap_block, (int32_t)g_dw3000_late_tx_count, now_us);
                break;
            case 3:
                (void)comp971_ble_pilot_publish_uwb_diagnostic_ext(
                    COMP971_UWB_PAIR_INDEX, COMP971_HEALTH_STALL_RECOVERY_CNT, snap_block, (int32_t)g_fira_stall_recovery_count, now_us);
                break;
            case 4:
                (void)comp971_ble_pilot_publish_uwb_diagnostic_ext(
                    COMP971_UWB_PAIR_INDEX, COMP971_HEALTH_STEADY_FAIL_CNT, snap_block, (int32_t)g_fira_steady_fail_count, now_us);
                break;
            case 5:
                (void)comp971_ble_pilot_publish_uwb_diagnostic_ext(
                    COMP971_UWB_PAIR_INDEX, COMP971_HEALTH_STEADY_TOTAL, snap_block, (int32_t)g_fira_steady_total, now_us);
                break;
            case 6:
                (void)comp971_ble_pilot_publish_uwb_diagnostic_ext(
                    COMP971_UWB_PAIR_INDEX, COMP971_HEALTH_LAST_BLOCK_INDEX, snap_block, (int32_t)g_fira_last_block_index, now_us);
                break;
            case 7:
                /* Lifetime min lead */
                (void)comp971_ble_pilot_publish_uwb_diagnostic_ext(
                    COMP971_UWB_PAIR_INDEX, COMP971_HEALTH_LIFETIME_MIN_LEAD, snap_block, (int32_t)g_fira_lifetime_min_lead, now_us);
                break;
            case 8:
                /* Free bytes in UWB task stack */
                (void)comp971_ble_pilot_publish_uwb_diagnostic_ext(
                    COMP971_UWB_PAIR_INDEX, COMP971_HEALTH_UWB_STACK_FREE, snap_block,
                    (int32_t)(uxTaskGetStackHighWaterMark(NULL) * (UBaseType_t)sizeof(StackType_t)),
                    now_us);
                break;
            case 9:
                /* Free bytes in IMU task stack */
                (void)comp971_ble_pilot_publish_uwb_diagnostic_ext(
                    COMP971_UWB_PAIR_INDEX, COMP971_HEALTH_IMU_STACK_FREE, snap_block,
                    get_task_free_bytes((TaskHandle_t)g_comp971_imu_task_handle), now_us);
                break;
            case 10:
                /* Free bytes in Status task stack */
                (void)comp971_ble_pilot_publish_uwb_diagnostic_ext(
                    COMP971_UWB_PAIR_INDEX, COMP971_HEALTH_STATUS_STACK_FREE, snap_block,
                    get_task_free_bytes((TaskHandle_t)g_comp971_status_task_handle), now_us);
                break;
            case 11:
                /* Disconnect count (high 16 bits) & flushed queue items count (low 16 bits) */
                (void)comp971_ble_pilot_publish_uwb_diagnostic_ext(
                    COMP971_UWB_PAIR_INDEX, COMP971_HEALTH_DISCONN_AND_FLUSH, snap_block,
                    (int32_t)(((g_ble_disconnect_count & 0xFFFFU) << 16) | (g_ble_queue_items_flushed_on_disconnect & 0xFFFFU)),
                    now_us);
                break;
            case 12:
                /* Connect block index (block when BLE connected) */
                (void)comp971_ble_pilot_publish_uwb_diagnostic_ext(
                    COMP971_UWB_PAIR_INDEX, COMP971_HEALTH_CONNECT_BLOCK_IDX, snap_block,
                    (int32_t)g_fira_connect_block_index, now_us);
                break;
            case 13:
            {
                /* Last failure block index from failure ring buffer */
                int32_t fail_block = -1;
                if (g_fira_fail_ring_count > 0U)
                {
                    uint16_t last_idx = (g_fira_fail_ring_count - 1U) % COMP971_FAIL_RING_SIZE;
                    fail_block = (int32_t)g_fira_fail_block_indices[last_idx];
                }
                (void)comp971_ble_pilot_publish_uwb_diagnostic_ext(
                    COMP971_UWB_PAIR_INDEX, COMP971_HEALTH_FAIL_RING_BLOCK, snap_block, fail_block, now_us);
                break;
            }
            case 14:
            {
                /* Last failure status code from failure ring buffer */
                int32_t fail_status = -1;
                if (g_fira_fail_ring_count > 0U)
                {
                    uint16_t last_idx = (g_fira_fail_ring_count - 1U) % COMP971_FAIL_RING_SIZE;
                    fail_status = (int32_t)g_fira_fail_statuses[last_idx];
                }
                (void)comp971_ble_pilot_publish_uwb_diagnostic_ext(
                    COMP971_UWB_PAIR_INDEX, COMP971_HEALTH_FAIL_RING_STATUS, snap_block, fail_status, now_us);
                break;
            }
            case 15:
                /* Connect count (high 16 bits) & queue histogram bins 1..7 non-zero count (low 16 bits) */
                (void)comp971_ble_pilot_publish_uwb_diagnostic_ext(
                    COMP971_UWB_PAIR_INDEX, COMP971_HEALTH_CONNECT_CNT_AND_Q, snap_block,
                    (int32_t)(((g_ble_connect_count & 0xFFFFU) << 16) | ((g_ble_queue_depth_hist[1] + g_ble_queue_depth_hist[2] + g_ble_queue_depth_hist[3] + g_ble_queue_depth_hist[4] + g_ble_queue_depth_hist[5] + g_ble_queue_depth_hist[6] + g_ble_queue_depth_hist[7]) & 0xFFFFU)),
                    now_us);
                break;
            case 16:
                /* Live DW3000 SYS_STATUS lo 32 bits */
                (void)comp971_ble_pilot_publish_uwb_diagnostic_ext(
                    COMP971_UWB_PAIR_INDEX, COMP971_HEALTH_DW3000_SYS_LO, snap_block,
                    (int32_t)dwt_readsysstatuslo(), now_us);
                break;
            case 17:
                /* Live DW3000 SYS_STATUS hi 16 bits */
                (void)comp971_ble_pilot_publish_uwb_diagnostic_ext(
                    COMP971_UWB_PAIR_INDEX, COMP971_HEALTH_DW3000_SYS_HI, snap_block,
                    (int32_t)dwt_readsysstatushi(), now_us);
                break;
            case 18:
                /* Shared session configuration CRC-32 hash */
                (void)comp971_ble_pilot_publish_uwb_diagnostic_ext(
                    COMP971_UWB_PAIR_INDEX, COMP971_HEALTH_CONFIG_HASH, snap_block,
                    (int32_t)g_comp971_config_hash, now_us);
                break;
            default:
                health_cycle_step = 0;
                break;
        }
        health_cycle_step = (uint8_t)((health_cycle_step + 1U) % 19U);
    }

    /* Bounded, decoupled recovery state machine (Version 6.7)
     * Sole owner: uwb_task (single-task context).
     * Callbacks only publish g_fira_cb_sequence and g_fira_valid_sequence atomically.
     */
    if (m_prepared && (m_session_state == QUWBS_FBS_SESSION_STATE_IDLE))
    {
        /* Auto-startup ONLY when not in recovery BACKOFF or HOLD */
        if (m_rec_state == COMP971_REC_IDLE)
        {
            enum qerr res = fira_helper_start_session(&m_fira_ctx, m_session_handle);
            if (res == QERR_SUCCESS)
            {
                m_session_state = QUWBS_FBS_SESSION_STATE_ACTIVE;
                m_rec_session_cookie++;
                m_rec_consecutive_valid = 0;
                m_rec_last_consumed_valid_seq = g_fira_valid_sequence;
                g_fira_last_cb_tick = xTaskGetTickCount();
                comp971_ble_pilot_set_uwb_started(true);
            }
        }
    }

    if ((g_comp971_uwb_role != COMP971_UWB_CONTROLLER) && m_prepared)
    {
        TickType_t now = xTaskGetTickCount();
        uint32_t cb_seq = g_fira_cb_sequence;
        uint32_t val_seq = g_fira_valid_sequence;
        uint32_t cb_streak = g_fira_cb_valid_streak;
        uint32_t cb_cookie = g_fira_last_cb_cookie;
        uint32_t val_cookie = g_fira_last_valid_cookie;
        uint32_t last_cb = g_fira_last_cb_tick;

        switch (m_rec_state)
        {
            case COMP971_REC_IDLE:
            {
                if (m_session_state == QUWBS_FBS_SESSION_STATE_ACTIVE)
                {
                    uint32_t delta_ticks = (uint32_t)(now - last_cb);
                    uint32_t delta_ms = (uint32_t)(((uint64_t)delta_ticks * 33000ULL) / 32768ULL);
                    uint32_t timeout_threshold_ms = 5000U + (uint32_t)g_comp971_restart_jitter_ms;

                    if (delta_ms >= timeout_threshold_ms)
                    {
                        m_rec_attempts++;
                        bool ok = comp971_uwb_stage2_mac_restart();
                        if (ok)
                        {
                            m_rec_state = COMP971_REC_COOLDOWN;
                            m_rec_timer_tick = now;
                            m_rec_start_tick = now;
                            m_rec_start_seq = cb_seq;
                            m_rec_consecutive_valid = 0;
                            m_rec_last_consumed_valid_seq = val_seq;
                        }
                        else
                        {
                            if (m_rec_attempts >= COMP971_REC_MAX_ATTEMPTS)
                            {
                                m_rec_state = COMP971_REC_HOLD;
                                m_rec_timer_tick = now;
                                (void)comp971_ble_pilot_publish_uwb_diagnostic(
                                    COMP971_UWB_PAIR_INDEX, 0xEDU, (int32_t)m_rec_attempts, local_timestamp_us());
                            }
                            else
                            {
                                m_rec_state = COMP971_REC_BACKOFF;
                                m_rec_timer_tick = now;
                            }
                        }
                    }
                }
                break;
            }

            case COMP971_REC_COOLDOWN:
            {
                /* Case 1 & Case 2: A new callback has arrived since last evaluation */
                if ((cb_seq > m_rec_start_seq) && (cb_cookie == m_rec_session_cookie))
                {
                    uint32_t new_cb_count = cb_seq - m_rec_start_seq;
                    uint32_t new_valid_count = (val_seq >= m_rec_last_consumed_valid_seq) ?
                                               (val_seq - m_rec_last_consumed_valid_seq) : 0U;

                    /* If the callback streak was broken, or any intervening failure arrived */
                    if ((cb_streak == 0U) || (new_valid_count < new_cb_count))
                    {
                        /* Case 2: New failed ranging event occurred -> reset success streak */
                        m_rec_consecutive_valid = 0;
                        m_rec_last_consumed_valid_seq = val_seq;
                    }
                    else if ((new_valid_count > 0U) && (val_cookie == m_rec_session_cookie))
                    {
                        /* Case 1: New valid ranging event(s) arrived with 0 intervening failures -> update success streak */
                        m_rec_consecutive_valid = (uint8_t)(m_rec_consecutive_valid + new_valid_count);
                        m_rec_last_consumed_valid_seq = val_seq;
                    }
                    m_rec_start_seq = cb_seq;

                    if ((m_rec_consecutive_valid >= 2U) && (cb_streak >= 2U))
                    {
                        /* Recovery verified: 2 distinct valid post-restart rounds with no intervening failures */
                        m_rec_state = COMP971_REC_IDLE;
                        m_rec_attempts = 0;
                        m_rec_consecutive_valid = 0;
                        break;
                    }
                }
                /* Case 3: No new ranging event arrived (cb_seq == m_rec_start_seq)
                 * Preserve m_rec_consecutive_valid without resetting */

                uint32_t cd_ticks = (uint32_t)(now - m_rec_timer_tick);
                uint32_t cd_ms = (uint32_t)(((uint64_t)cd_ticks * 33000ULL) / 32768ULL);
                if (cd_ms >= COMP971_REC_COOLDOWN_MS)
                {
                    m_rec_attempts++;
                    if (m_rec_attempts >= COMP971_REC_MAX_ATTEMPTS)
                    {
                        m_rec_state = COMP971_REC_HOLD;
                        m_rec_timer_tick = now;
                        (void)comp971_ble_pilot_publish_uwb_diagnostic(
                            COMP971_UWB_PAIR_INDEX, 0xEDU, (int32_t)m_rec_attempts, local_timestamp_us());
                    }
                    else
                    {
                        m_rec_state = COMP971_REC_BACKOFF;
                        m_rec_timer_tick = now;
                    }
                }
                break;
            }

            case COMP971_REC_BACKOFF:
            {
                uint32_t bo_ticks = (uint32_t)(now - m_rec_timer_tick);
                uint32_t bo_ms = (uint32_t)(((uint64_t)bo_ticks * 33000ULL) / 32768ULL);
                if (bo_ms >= COMP971_REC_BACKOFF_MS)
                {
                    bool ok = comp971_uwb_stage2_mac_restart();
                    if (ok)
                    {
                        m_rec_state = COMP971_REC_COOLDOWN;
                        m_rec_timer_tick = now;
                        m_rec_start_tick = now;
                        m_rec_start_seq = cb_seq;
                        m_rec_consecutive_valid = 0;
                        m_rec_last_consumed_valid_seq = val_seq;
                    }
                    else
                    {
                        /* SDK restart failed again; increment attempt for this failed restart invocation */
                        m_rec_attempts++;
                        if (m_rec_attempts >= COMP971_REC_MAX_ATTEMPTS)
                        {
                            m_rec_state = COMP971_REC_HOLD;
                            m_rec_timer_tick = now;
                            (void)comp971_ble_pilot_publish_uwb_diagnostic(
                                COMP971_UWB_PAIR_INDEX, 0xEDU, (int32_t)m_rec_attempts, local_timestamp_us());
                        }
                        else
                        {
                            m_rec_timer_tick = now;
                        }
                    }
                }
                break;
            }

            case COMP971_REC_HOLD:
            {
                uint32_t hold_ticks = (uint32_t)(now - m_rec_timer_tick);
                uint32_t hold_ms = (uint32_t)(((uint64_t)hold_ticks * 33000ULL) / 32768ULL);
                if (hold_ms >= COMP971_REC_HOLD_MS)
                {
                    m_rec_attempts = 0;
                    m_rec_state = COMP971_REC_IDLE;
                }
                break;
            }

            default:
                m_rec_state = COMP971_REC_IDLE;
                break;
        }
    }
#endif
}

uint32_t comp971_uwb_get_last_cb_tick(void)
{
    return g_fira_last_cb_tick;
}

bool comp971_uwb_is_active(void)
{
#if COMP971_UWB_ROLE != COMP971_UWB_DISABLED
    return (m_prepared && (m_session_state == QUWBS_FBS_SESSION_STATE_ACTIVE));
#else
    return false;
#endif
}

bool comp971_uwb_stage1_restart(void)
{
#if COMP971_UWB_ROLE != COMP971_UWB_DISABLED
    g_fira_stall_recovery_count++;
    g_fira_stage1_recovery_count++;
    (void)fira_helper_stop_session(&m_fira_ctx, m_session_handle);
    vTaskDelay(pdMS_TO_TICKS(10U));
    enum qerr res = fira_helper_start_session(&m_fira_ctx, m_session_handle);
    if (res == QERR_SUCCESS)
    {
        m_session_state = QUWBS_FBS_SESSION_STATE_ACTIVE;
        nrf_gpio_pin_write(NRF_GPIO_PIN_MAP(0, 22), 0);
        return true;
    }
    g_fira_stall_failed_restart_count++;
    return false;
#else
    return false;
#endif
}

bool comp971_uwb_stage2_mac_restart(void)
{
#if COMP971_UWB_ROLE != COMP971_UWB_DISABLED
    g_fira_stall_recovery_count++;
    g_fira_stage2_recovery_count++;
    enum qerr res;

    /* Step 1: stop session first */
    res = fira_helper_stop_session(&m_fira_ctx, m_session_handle);
    if (res != QERR_SUCCESS)
    {
        g_fira_stall_failed_restart_count++;
        (void)comp971_ble_pilot_publish_uwb_diagnostic(
            COMP971_UWB_PAIR_INDEX, 0xE1U, (int32_t)res, local_timestamp_us());
        return false;
    }
    m_session_state = QUWBS_FBS_SESSION_STATE_IDLE;

    /* Step 2: stop MAC */
    res = uwbmac_stop(m_uwbmac_ctx);
    if (res != QERR_SUCCESS)
    {
        g_fira_stall_failed_restart_count++;
        (void)comp971_ble_pilot_publish_uwb_diagnostic(
            COMP971_UWB_PAIR_INDEX, 0xE2U, (int32_t)res, local_timestamp_us());
        return false;
    }

    /* Guard interval for MAC state machine stabilization */
    vTaskDelay(pdMS_TO_TICKS(10U));

    /* Step 3: start MAC */
    res = uwbmac_start(m_uwbmac_ctx);
    if (res != QERR_SUCCESS)
    {
        g_fira_stall_failed_restart_count++;
        (void)comp971_ble_pilot_publish_uwb_diagnostic(
            COMP971_UWB_PAIR_INDEX, 0xE3U, (int32_t)res, local_timestamp_us());
        return false;
    }

    /* Step 4: start session */
    res = fira_helper_start_session(&m_fira_ctx, m_session_handle);
    if (res != QERR_SUCCESS)
    {
        g_fira_stall_failed_restart_count++;
        (void)comp971_ble_pilot_publish_uwb_diagnostic(
            COMP971_UWB_PAIR_INDEX, 0xE4U, (int32_t)res, local_timestamp_us());
        return false;
    }

    m_session_state = QUWBS_FBS_SESSION_STATE_ACTIVE;
    m_rec_session_cookie++;
    nrf_gpio_pin_write(NRF_GPIO_PIN_MAP(0, 22), 0);
    return true;
#else
    return false;
#endif
}

void comp971_uwb_record_stall_snapshot(uint8_t stage, uint32_t duration_ms, uint16_t trigger_flags)
{
    /* Always update live retained state for post-mortem tracking.
     * Note: Live SPI bus transactions (e.g. dwt_readsysstatuslo/hi) are strictly forbidden
     * on the reset trip path to eliminate blocking risks. Hardware status fields are marked
     * unavailable (0xFFFFFFFF) when no safe in-memory cache exists. */
    g_comp971_retained.last_escalation_stage = stage;
    g_comp971_retained.last_stall_duration_ms = duration_ms;
    g_comp971_retained.trigger_flags = trigger_flags;
    g_comp971_retained.last_block_index = g_fira_last_block_index;
    g_comp971_retained.lifetime_min_lead = g_fira_lifetime_min_lead;
    g_comp971_retained.late_tx_count = g_dw3000_late_tx_count;
    g_comp971_retained.sys_status_lo = 0xFFFFFFFFU; /* No safe in-memory cache on trip path */
    g_comp971_retained.sys_status_hi = 0xFFFFFFFFU;
    g_comp971_retained.crc32 = comp971_calc_crc32((const uint8_t *)&g_comp971_retained, offsetof(struct comp971_retained_block, crc32));

    /* Check if an unacknowledged committed snapshot already exists using shared frozen CRC helper.
     * Bounded retention lifecycle policy:
     * - If a snapshot is COMMITTED and valid, and publish_attempts < COMP971_FROZEN_RETENTION_MIN_PUBLISH_COUNT,
     *   it has not yet been reliably ingested by the host. Preserve it immutably and count missed snapshots.
     * - Once published at least COMP971_FROZEN_RETENTION_MIN_PUBLISH_COUNT times, a new failure escalation
     *   is permitted to overwrite the snapshot so the system is not permanently locked to an old incident. */
    uint32_t cur_crc = comp971_calc_frozen_crc(&g_comp971_frozen);
    if ((g_comp971_frozen.commit_state == COMP971_COMMIT_STATE_COMMITTED) &&
        (g_comp971_frozen.crc32 == cur_crc) &&
        (g_comp971_frozen_meta.publish_attempts < COMP971_FROZEN_RETENTION_MIN_PUBLISH_COUNT))
    {
        /* Preserve the unhandled root-cause snapshot through repeated resets.
         * Record subsequent missed snapshots in separate metadata with overflow protection. */
        if (g_comp971_frozen_meta.missed_snapshots_count < 0xFFFFU)
        {
            g_comp971_frozen_meta.missed_snapshots_count++;
        }
        g_comp971_frozen_meta.crc32 = comp971_calc_crc32(
            (const uint8_t *)&g_comp971_frozen_meta, offsetof(struct comp971_frozen_delivery_meta, crc32));
        return;
    }

    /* Safe two-phase atomic commit sequence:
     * Phase 1: Mark WRITING before modifying any snapshot fields. */
    g_comp971_frozen.commit_state = COMP971_COMMIT_STATE_WRITING;
    __DMB();

    /* Populate immutable diagnostic fields (bytes 4..51) */
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

    /* Copy two most recent callbacks from ring */
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

    /* Phase 2: Compute and store CRC ONLY over immutable diagnostic fields (bytes 4..51, 48 bytes).
     * The commit-state word (bytes 0..3) is explicitly excluded from checksum coverage! */
    g_comp971_frozen.crc32 = comp971_calc_frozen_crc(&g_comp971_frozen);
    __DMB();

    /* Phase 3: Transition commit_state to COMMITTED only after payload & CRC are completely latched */
    g_comp971_frozen.commit_state = COMP971_COMMIT_STATE_COMMITTED;
    __DMB();

    /* Reset delivery metadata for the new failure event and capture new diagnostic indicators */
    g_comp971_frozen_meta.publish_attempts = 0U;
    g_comp971_frozen_meta.last_publish_tick_ms = 0U;
    g_comp971_frozen_meta.subsequent_resets_seen = 0U;
    g_comp971_frozen_meta.first_post_reset_reason = 0U;
    g_comp971_frozen_meta.stall_irq_pin_level = (uint8_t)nrf_gpio_pin_read(NRF_GPIO_PIN_MAP(1, 2));
    g_comp971_frozen_meta.stall_uwb_task_state = (g_comp971_uwb_task_handle != NULL) ?
                                                (uint8_t)eTaskGetState(g_comp971_uwb_task_handle) : 0xFFU;
    g_comp971_frozen_meta.stall_dw3000_tx_count = g_dw3000_tx_count;
    g_comp971_frozen_meta.stall_fira_cb_seq = g_fira_cb_sequence;
    g_comp971_frozen_meta.stall_dw3000_irq_count = g_dw3000_irq_count;
    g_comp971_frozen_meta.stall_cb_last_irq_count = g_fira_cb_last_irq_count;
    g_comp971_frozen_meta.stall_cb_last_tx_count = g_fira_cb_last_tx_count;
    g_comp971_frozen_meta.stall_last_tx_info = ((uint32_t)g_dw3000_last_tx_fastcmd << 24) |
                                               ((uint32_t)g_dw3000_last_tx_mode << 16) |
                                               ((uint32_t)g_dw3000_last_tx_result << 8) |
                                               ((uint32_t)g_dw3000_last_tx_hpdwarn_seen);
    g_comp971_frozen_meta.stall_last_tx_sys_state = g_dw3000_last_tx_sys_state;
    g_comp971_frozen_meta.stall_last_tx_status = (uint32_t)g_dw3000_last_tx_status;
    g_comp971_frozen_meta.stall_last_tx_diff = g_dw3000_last_tx_diff;
    g_comp971_frozen_meta.stall_last_tx_seq = g_dw3000_last_tx_seq;
    g_comp971_frozen_meta.reserved_diag = 0U;
    g_comp971_frozen_meta.crc32 = comp971_calc_crc32(
        (const uint8_t *)&g_comp971_frozen_meta, offsetof(struct comp971_frozen_delivery_meta, crc32));
}
