/*
 * COMP971 two-node FiRa pilot using Qorvo's QANI task.
 *
 * Qorvo owns the UWB task, signal queue, UWB-MAC setup and FiRa session
 * lifecycle. This file is only the project-level adapter: it supplies the
 * fixed two-board identity and forwards Qorvo's RAM-only results to BLE.
 * It contains no OTP, UICR, antenna-delay, calibration, NVM or flash write.
 */
#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "HAL_crypto.h"
#include "common_fira.h"
#include "deca_error.h"
#include "fira_app_config.h"
#include "fira_niq.h"
#include "niq.h"
#include "persistent_time.h"
#include "task.h"
#include "uwbmac/fira_helper.h"
#include "uwbmac/uwbmac.h"

#include "comp971_ble_pilot.h"
#include "comp971_telemetry_protocol.h"
#include "comp971_uwb_two_node.h"

#ifndef COMP971_UWB_ROLE
#error "COMP971_UWB_ROLE must be supplied by project_COMP971.cmake"
#endif

#define COMP971_UWB_DISABLED   0U
#define COMP971_UWB_CONTROLLER 1U
#define COMP971_UWB_RESPONDER  2U
#define COMP971_UWB_PAIR_INDEX 0U

#define COMP971_UWB_STAGE_QANI_CREATE 0xD1U
#define COMP971_UWB_STAGE_NIQ_INIT 0xD2U
#define COMP971_UWB_PROGRESS_QANI_START_REQUESTED 0xC2U
#define COMP971_UWB_PROGRESS_QANI_SESSION_STARTED 0xC5U

#if COMP971_UWB_ROLE != COMP971_UWB_DISABLED
/* Qorvo's QANI source normally receives this object from its Apple Nearby
 * Interaction BLE demo. COMP971 supplies the equivalent fixed two-board
 * configuration locally. Qorvo copies it into the live FiRa session in RAM
 * when StartUWB is raised. */
fira_device_configure_t fira_config = {
    .role = QUWBS_FBS_DEVICE_ROLE_INITIATOR,
    .enc_payload = 1U,
    .Session_ID = 42U,
    .Ranging_Round_Usage = FIRA_RANGING_ROUND_USAGE_DSTWR_DEFERRED,
    .Multi_Node_Mode = FIRA_MULTI_NODE_MODE_UNICAST,
    .Rframe_Config = FIRA_RFRAME_CONFIG_SP3,
    .ToF_Report = 1U,
    .Block_Duration_ms = 200U,
    .Round_Duration_RSTU = 57600U,
    .Slot_Duration_RSTU = 2400U,
    .Channel_Number = 9U,
    .Preamble_Code = 11U,
    /* Qorvo's QANI BPRF set 2 selects SFD 2 in fira_set_params(). */
    .SP0_PHY_Set = 2U,
    .SRC_ADDR = {0U, 0U},
    .Number_of_Controlee = 1U,
    .DST_ADDR = {1U, 0U}
};

/* QANI's iOS-demo report callback calls this symbol. COMP971 forwards the
 * same result through comp971_qani_fira_range_report(), so no iOS payload is
 * required. */
void send_ios_notification(uint8_t distance, char *text)
{
    (void)distance;
    (void)text;
}

static bool m_qani_ready;
static bool m_start_requested;
static bool m_failure_pending;
static uint8_t m_failure_stage;
static int32_t m_failure_detail;
static bool m_progress_pending;
static uint8_t m_progress_stage;

#if COMP971_UWB_ROLE == COMP971_UWB_CONTROLLER
static uint32_t local_timestamp_us(void)
{
    return (uint32_t)(((uint64_t)xTaskGetTickCount() * 1000000U) /
                      configTICK_RATE_HZ);
}
#endif

static void record_failure(uint8_t stage, int32_t detail)
{
    m_failure_stage = stage;
    m_failure_detail = detail;
    m_failure_pending = true;
    comp971_ble_pilot_set_uwb_started(false);
}

static void record_progress(uint8_t stage)
{
    m_progress_stage = stage;
    m_progress_pending = true;
}

static void configure_qorvo_two_board_session(void)
{
    /* These are Qorvo QANI configuration fields. They are copied into a
     * RAM session when the official QANI task receives StartUWB(). */
    fira_config.Session_ID = 42U;
    fira_config.Channel_Number = 9U;
    fira_config.Block_Duration_ms = 200U;
    fira_config.Slot_Duration_RSTU = 2400U;
    fira_config.Round_Duration_RSTU = 57600U;
    fira_config.SRC_ADDR[1] = 0U;
    fira_config.DST_ADDR[1] = 0U;

#if COMP971_UWB_ROLE == COMP971_UWB_CONTROLLER
    fira_config.role = QUWBS_FBS_DEVICE_ROLE_INITIATOR;
    fira_config.SRC_ADDR[0] = 0U;
    fira_config.DST_ADDR[0] = 1U;
    fira_config.Number_of_Controlee = 1U;
#else
    fira_config.role = QUWBS_FBS_DEVICE_ROLE_RESPONDER;
    fira_config.SRC_ADDR[0] = 1U;
    fira_config.DST_ADDR[0] = 0U;
    fira_config.Number_of_Controlee = 0U;
#endif
}

/* These two hooks are called by Qorvo's QANI implementation. They do not
 * change the Qorvo ranging algorithm or radio configuration. */
void comp971_qani_fira_range_report(const struct fira_twr_ranging_results *results)
{
#if COMP971_UWB_ROLE != COMP971_UWB_CONTROLLER
    (void)results;
    return;
#else
    uint32_t index;

    if (results == NULL)
    {
        return;
    }
    for (index = 0U; index < results->n_measurements; ++index)
    {
        const struct fira_twr_measurements *measurement =
            (const struct fira_twr_measurements *)&results->measurements[index];
        const bool valid =
            measurement->status == QUWBS_FBS_STATUS_RANGING_SUCCESS;

        comp971_ble_pilot_record_uwb_result(valid);
        (void)comp971_ble_pilot_publish_uwb_range(
            COMP971_UWB_PAIR_INDEX,
            valid ? (int32_t)measurement->distance_cm : -1,
            valid ? COMP971_UWB_RANGE_VALID : 0U, local_timestamp_us());
        if (!valid)
        {
            (void)comp971_ble_pilot_publish_uwb_diagnostic(
                COMP971_UWB_PAIR_INDEX, 0x42U, (int32_t)measurement->status,
                local_timestamp_us());
        }
    }
#endif
}

void comp971_qani_fira_start_result(bool started, uint8_t stage, int32_t detail)
{
    comp971_ble_pilot_set_uwb_started(started);
    if (started)
    {
        record_progress(COMP971_UWB_PROGRESS_QANI_SESSION_STARTED);
    }
    else
    {
        record_failure(stage, detail);
    }
}
#endif

void comp971_uwb_two_node_init(void)
{
#if COMP971_UWB_ROLE != COMP971_UWB_DISABLED
    int niq_result;
    error_e result;

    m_qani_ready = false;
    m_start_requested = false;
    m_failure_pending = false;
    m_progress_pending = false;
    comp971_ble_pilot_set_uwb_started(false);
    configure_qorvo_two_board_session();

    /* Qorvo's own QANI main initialises this time base before creating its
     * FiRa task. It then registers the official NIQ callbacks before it
     * creates the QANI task. The COMP971 application supplies fixed local
     * session values instead of values from an iPhone, but the Qorvo startup
     * contract remains the same. */
    persistent_time_init(0);
    niq_result = niq_init(StartUWB, StopUWB, (const void *)crypto_init,
                          (const void *)crypto_deinit,
                          (const void *)crypto_get_random_vector);
    if (niq_result != 0)
    {
        record_failure(COMP971_UWB_STAGE_NIQ_INIT, niq_result);
        return;
    }
    result = CreateQaniTask();
    if (result != _NO_ERR)
    {
        record_failure(COMP971_UWB_STAGE_QANI_CREATE, (int32_t)result);
    }
    else
    {
        m_qani_ready = true;
    }
#endif
}

void comp971_uwb_two_node_start(void)
{
#if COMP971_UWB_ROLE != COMP971_UWB_DISABLED
    /* The existing status task runs after the scheduler and BLE are live.
     * It will raise Qorvo's official start signal on its first pass. */
    m_start_requested = m_qani_ready;
#endif
}

void comp971_uwb_two_node_publish_pending_event(void)
{
#if COMP971_UWB_ROLE != COMP971_UWB_DISABLED
    if (m_start_requested)
    {
        m_start_requested = false;
        record_progress(COMP971_UWB_PROGRESS_QANI_START_REQUESTED);
        StartUWB(&fira_config, NULL);
    }
#endif

#if COMP971_UWB_ROLE == COMP971_UWB_CONTROLLER
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
#endif
}
