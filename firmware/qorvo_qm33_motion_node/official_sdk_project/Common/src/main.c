#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include "HAL_cpu.h"
#include "HAL_error.h"
#include "HAL_watchdog.h"
#include "nrf_drv_clock.h"
#include "nrfx_clock.h"
#include "nrfx_nvmc.h"
#include "nrf_gpio.h"
#include "persistent_time.h"
#include "qos.h"

#include "comp971_ble_pilot.h"
#include "comp971_icm20948.h"
#include "comp971_node_identity.h"
#include "comp971_telemetry_protocol.h"
#include "comp971_uwb_two_node.h"

#define COMP971_IMU_SAMPLE_RATE_HZ 60U
#define COMP971_IMU_BATCH_SAMPLES 6U
#define COMP971_UWB_TASK_STACK_WORDS 1536U /* Expanded from 4 KiB to 6 KiB for safety margin. */
#define COMP971_UWB_START_DELAY_MS 50U

volatile TaskHandle_t g_comp971_status_task_handle = NULL;
volatile TaskHandle_t g_comp971_imu_task_handle = NULL;
volatile TaskHandle_t g_comp971_uwb_task_handle = NULL;
volatile TaskHandle_t g_comp971_supervisor_task_handle = NULL;

static volatile uint32_t g_boot_resetreas = 0U;
static volatile uint32_t g_imu_progress_counter = 0U;
static volatile uint32_t g_status_progress_counter = 0U;

static uint8_t s_burst_record_idx = 0U;

static bool send_frozen_record(uint8_t record_idx)
{
    if (record_idx == 0U)
    {
        struct comp971_stall_snapshot_v1 snap;
        memset(&snap, 0, sizeof(snap));
        snap.protocol_version = COMP971_PROTOCOL_VERSION;
        snap.message_type = COMP971_TELEMETRY_STALL_SNAPSHOT;
        snap.node_index = (uint8_t)g_comp971_node_index;
        snap.trigger_flags = (uint8_t)g_comp971_frozen.trigger_flags;
        snap.magic = COMP971_RETAINED_MAGIC;
        snap.boot_count = g_comp971_frozen.pre_reset_boot_id;
        snap.reset_reason = (g_comp971_frozen_meta.first_post_reset_reason != 0U) ?
                            g_comp971_frozen_meta.first_post_reset_reason : g_boot_resetreas;
        snap.escalation_reached = g_comp971_frozen.escalation_reached;
        snap.stall_duration_ms = g_comp971_frozen.stall_duration_ms;
        snap.last_block_index = g_comp971_frozen.last_block_index;
        snap.min_lead_dtu = g_comp971_frozen.min_lead_dtu;
        snap.late_tx_count = g_comp971_frozen.late_tx_count;
        snap.recent_cbs[0] = g_comp971_frozen.recent_cbs[0];
        snap.recent_cbs[1] = g_comp971_frozen.recent_cbs[1];
        snap.crc32 = comp971_calc_crc32((const uint8_t *)&snap, offsetof(struct comp971_stall_snapshot_v1, crc32));

        return comp971_ble_pilot_publish_stall_snapshot(&snap);
    }
    else if (record_idx <= 12U)
    {
        uint16_t stall_blk_lo = (uint16_t)(g_comp971_frozen.last_block_index & 0xFFFFU);
        uint32_t stall_boot = g_comp971_frozen.pre_reset_boot_id;
        uint8_t  node_idx = (uint8_t)g_comp971_node_index;

        static const uint8_t stage_codes[12] = {
            COMP971_DIAG_STALL_IRQ_PIN,
            COMP971_DIAG_STALL_TASK_STATE,
            COMP971_DIAG_STALL_TX_COUNT,
            COMP971_DIAG_STALL_CB_SEQ,
            COMP971_DIAG_STALL_IRQ_COUNT,
            COMP971_DIAG_STALL_CB_LAST_IRQ,
            COMP971_DIAG_STALL_CB_LAST_TX,
            COMP971_DIAG_STALL_TX_INFO,
            COMP971_DIAG_STALL_TX_SYS_STATE,
            COMP971_DIAG_STALL_TX_STATUS,
            COMP971_DIAG_STALL_TX_DIFF,
            COMP971_DIAG_STALL_TX_SEQ
        };
        const int32_t details[12] = {
            (int32_t)g_comp971_frozen_meta.stall_irq_pin_level,
            (int32_t)g_comp971_frozen_meta.stall_uwb_task_state,
            (int32_t)g_comp971_frozen_meta.stall_dw3000_tx_count,
            (int32_t)g_comp971_frozen_meta.stall_fira_cb_seq,
            (int32_t)g_comp971_frozen_meta.stall_dw3000_irq_count,
            (int32_t)g_comp971_frozen_meta.stall_cb_last_irq_count,
            (int32_t)g_comp971_frozen_meta.stall_cb_last_tx_count,
            (int32_t)g_comp971_frozen_meta.stall_last_tx_info,
            (int32_t)g_comp971_frozen_meta.stall_last_tx_sys_state,
            (int32_t)g_comp971_frozen_meta.stall_last_tx_status,
            (int32_t)g_comp971_frozen_meta.stall_last_tx_diff,
            (int32_t)g_comp971_frozen_meta.stall_last_tx_seq
        };

        uint8_t d = record_idx - 1U;
        return comp971_ble_pilot_publish_uwb_diagnostic_ext(
            node_idx, stage_codes[d], stall_blk_lo, details[d], stall_boot);
    }
    return false;
}

static bool publish_frozen_snapshot_step(void)
{
    uint32_t frozen_crc = comp971_calc_frozen_crc(&g_comp971_frozen);
    bool have_frozen = (g_comp971_frozen.commit_state == COMP971_COMMIT_STATE_COMMITTED) &&
                       (g_comp971_frozen.crc32 == frozen_crc);

    if (!have_frozen)
    {
        return false;
    }

    if (g_comp971_frozen_meta.publish_attempts >= 3U)
    {
        return true; /* Fully published target quota */
    }

    if (!comp971_ble_pilot_is_connected_and_notifying())
    {
        s_burst_record_idx = 0U; /* Reset in-flight record index on disconnect */
        return false; /* Link not ready, defer without blocking */
    }

    while (s_burst_record_idx < 13U)
    {
        if (!send_frozen_record(s_burst_record_idx))
        {
            /* SoftDevice buffer full (NRF_ERROR_RESOURCES) or busy.
             * Return false without blocking; s_burst_record_idx holds the pending record index. */
            return false;
        }
        s_burst_record_idx++;
    }

    /* All 13 records successfully accepted by SoftDevice queue */
    s_burst_record_idx = 0U;
    g_comp971_frozen_meta.publish_attempts++;
    g_comp971_frozen_meta.last_publish_tick_ms = (uint32_t)(((uint64_t)xTaskGetTickCount() * 33000ULL) / 32768ULL);
    g_comp971_frozen_meta.crc32 = comp971_calc_crc32(
        (const uint8_t *)&g_comp971_frozen_meta, offsetof(struct comp971_frozen_delivery_meta, crc32));

    return true;
}

static void supervisor_task(void *argument)
{
    (void)argument;

    /* Feed watchdog during boot grace period (5000 ms) while FreeRTOS tasks and FiRa session initialize */
    for (int boot_slice = 0; boot_slice < 50; ++boot_slice)
    {
        Watchdog.refresh();
        vTaskDelay(pdMS_TO_TICKS(100U));
    }

    /* On startup, emit retained stall snapshot: prefer immutable frozen record */
    uint32_t frozen_crc = comp971_calc_frozen_crc(&g_comp971_frozen);
    bool have_frozen = (g_comp971_frozen.commit_state == COMP971_COMMIT_STATE_COMMITTED) &&
                       (g_comp971_frozen.crc32 == frozen_crc);

    if (have_frozen)
    {
        /* Track subsequent resets seen across boots in delivery metadata */
        if (g_comp971_frozen_meta.subsequent_resets_seen < 0xFFFFU)
        {
            g_comp971_frozen_meta.subsequent_resets_seen++;
        }
        g_comp971_frozen_meta.crc32 = comp971_calc_crc32(
            (const uint8_t *)&g_comp971_frozen_meta, offsetof(struct comp971_frozen_delivery_meta, crc32));

        /* Attempt early publication bursts if BLE is already connected */
        for (uint32_t burst = 0; burst < 3U; ++burst)
        {
            if (g_comp971_frozen_meta.publish_attempts >= 3U)
            {
                break;
            }
            if (!publish_frozen_snapshot_step())
            {
                break; /* Not connected yet or buffer full; deferred publication in main loop will handle it */
            }
            vTaskDelay(pdMS_TO_TICKS(100U));
            Watchdog.refresh();
        }
    }
    else if (g_comp971_retained.magic == COMP971_RETAINED_MAGIC)
    {
        /* Fallback if no frozen record committed */
        struct comp971_stall_snapshot_v1 snap;
        memset(&snap, 0, sizeof(snap));
        snap.protocol_version = COMP971_PROTOCOL_VERSION;
        snap.message_type = COMP971_TELEMETRY_STALL_SNAPSHOT;
        snap.node_index = (uint8_t)g_comp971_node_index;
        snap.trigger_flags = (uint8_t)g_comp971_retained.trigger_flags;
        snap.magic = g_comp971_retained.magic;
        snap.boot_count = g_comp971_retained.boot_count;
        snap.reset_reason = g_boot_resetreas;
        snap.escalation_reached = g_comp971_retained.last_escalation_stage;
        snap.stall_duration_ms = g_comp971_retained.last_stall_duration_ms;
        snap.last_block_index = g_comp971_retained.last_block_index;
        snap.min_lead_dtu = g_comp971_retained.lifetime_min_lead;
        snap.late_tx_count = g_comp971_retained.late_tx_count;

        /* Take 2 most recent callbacks from ring */
        if (g_comp971_retained.ring_count > 0U)
        {
            uint32_t head = g_comp971_retained.ring_head;
            uint32_t idx1 = (head + COMP971_SNAPSHOT_RING_SIZE - 1U) % COMP971_SNAPSHOT_RING_SIZE;
            snap.recent_cbs[0] = g_comp971_retained.ring[idx1];
            if (g_comp971_retained.ring_count > 1U)
            {
                uint32_t idx2 = (head + COMP971_SNAPSHOT_RING_SIZE - 2U) % COMP971_SNAPSHOT_RING_SIZE;
                snap.recent_cbs[1] = g_comp971_retained.ring[idx2];
            }
        }
        snap.crc32 = comp971_calc_crc32((const uint8_t *)&snap, offsetof(struct comp971_stall_snapshot_v1, crc32));

        for (int retry = 0; retry < 100; ++retry)
        {
            if (comp971_ble_pilot_publish_stall_snapshot(&snap))
            {
                break;
            }
            Watchdog.refresh();
            vTaskDelay(pdMS_TO_TICKS(100U));
        }
    }

    TickType_t stall_start_tick = 0;
    uint32_t window_start_block = 0;
    TickType_t window_start_tick = 0;

    for (;;)
    {
        /* Run supervisor loop at 10 Hz (100 ms period) */
        vTaskDelay(pdMS_TO_TICKS(100U));

        /* Deferred frozen-snapshot publication (Task A):
         * If an unacknowledged frozen snapshot exists and target quota has not yet been satisfied (< 3),
         * periodically attempt publication whenever BLE connection and notifications are active.
         * Rate-limited to 1 burst per 1000 ms between completed bursts.
         * When in the middle of a burst (s_burst_record_idx > 0), check every supervisor loop (100 ms)
         * to quickly resume when SoftDevice buffers become free. */
        if (g_comp971_frozen_meta.publish_attempts < 3U)
        {
            static TickType_t last_burst_completed_tick = 0;
            TickType_t cur_tick = xTaskGetTickCount();
            if ((s_burst_record_idx > 0U) ||
                ((cur_tick - last_burst_completed_tick) >= pdMS_TO_TICKS(1000U)))
            {
                if (publish_frozen_snapshot_step())
                {
                    last_burst_completed_tick = cur_tick;
                }
            }
        }

        if (g_comp971_uwb_role == COMP971_UWB_CONTROLLER)
        {
            TickType_t now_tick = xTaskGetTickCount();
            uint32_t last_cb_tick = comp971_uwb_get_last_cb_tick();
            uint32_t cur_block = g_fira_last_block_index;

            if (last_cb_tick != 0U)
            {
                uint32_t cb_delta_ticks = (uint32_t)(now_tick - last_cb_tick);
                uint32_t cb_delta_ms = (uint32_t)(((uint64_t)cb_delta_ticks * 33000ULL) / 32768ULL);

                if (window_start_tick == 0)
                {
                    window_start_tick = now_tick;
                    window_start_block = cur_block;
                }

                uint32_t win_delta_ticks = (uint32_t)(now_tick - window_start_tick);
                uint32_t win_delta_ms = (uint32_t)(((uint64_t)win_delta_ticks * 33000ULL) / 32768ULL);

                bool block_progress_stalled = false;
                if (win_delta_ms >= 2000U)
                {
                    /* Over 2000 ms window, block index must advance by >= 3 blocks in steady state */
                    if ((cur_block >= 10U) && ((cur_block - window_start_block) < 3U))
                    {
                        block_progress_stalled = true;
                    }
                    else
                    {
                        window_start_tick = now_tick;
                        window_start_block = cur_block;
                    }
                }

                bool is_stalled = (cb_delta_ms >= 2000U) || block_progress_stalled;

                if (is_stalled)
                {
                    if (stall_start_tick == 0)
                    {
                        stall_start_tick = now_tick;
                    }

                    uint32_t stall_ticks = (uint32_t)(now_tick - stall_start_tick);
                    uint32_t stall_ms = (uint32_t)(((uint64_t)stall_ticks * 33000ULL) / 32768ULL);

                    /* Skip soft recovery stages and trigger clean reset after ~2.0 seconds */
                    if (stall_ms >= 2000U)
                    {
                        g_fira_stage3_recovery_count++;

                        /* Record snapshot with retained DW3000 register state and trigger flags */
                        uint16_t trig_flags = 0U;
                        if (cb_delta_ms >= 2000U) trig_flags |= 0x01U; /* Bit 0: callback age stall */
                        if (block_progress_stalled) trig_flags |= 0x02U; /* Bit 1: block progress stall */
                        comp971_uwb_record_stall_snapshot(COMP971_ESCALATION_STAGE3_WDT, stall_ms, trig_flags);

                        /* Reset loop guard: check resets in the last 60 seconds */
                        uint32_t now_uptime_ms = (uint32_t)(((uint64_t)now_tick * 33000ULL) / 32768ULL);
                        if ((g_comp971_retained.last_reset_tick_ms != 0U) &&
                            ((now_uptime_ms - g_comp971_retained.last_reset_tick_ms) < 60000U))
                        {
                            g_comp971_retained.reset_loop_count++;
                        }
                        else
                        {
                            g_comp971_retained.reset_loop_count = 1U;
                        }
                        g_comp971_retained.last_reset_tick_ms = now_uptime_ms;
                        g_comp971_retained.crc32 = comp971_calc_crc32((const uint8_t *)&g_comp971_retained, offsetof(struct comp971_retained_block, crc32));

                        if (g_comp971_retained.reset_loop_count > 3U)
                        {
                            /* Guard tripped: stay up, feed watchdog, and publish failure telemetry */
                            Watchdog.refresh();
                        }
                        else
                        {
                            NVIC_SystemReset();
                        }
                    }
                    else
                    {
                        Watchdog.refresh();
                    }
                }
                else
                {
                    stall_start_tick = 0;
                    Watchdog.refresh();
                }
            }
            else
            {
                Watchdog.refresh();
            }
        }
        else
        {
            /* Controlee role or disabled: keep watchdog fed */
            Watchdog.refresh();
        }
    }
}

volatile uint32_t g_ble_disconnect_stress_target = 0;
volatile uint32_t g_ble_disconnect_stress_completed = 0;

__attribute__((unused)) static void status_task(void *argument)
{
    (void)argument;

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000U));
        g_status_progress_counter++;
#if defined(COMP971_TEST_HOOKS) && (COMP971_TEST_HOOKS != 0)
        /* Injected disconnect stress test handler */
        if (g_ble_disconnect_stress_target > g_ble_disconnect_stress_completed)
        {
            extern volatile uint32_t g_ble_force_disconnect;
            if ((g_ble_force_disconnect == 0) && comp971_ble_pilot_is_connected_and_notifying())
            {
                g_ble_force_disconnect = 1;
            }
        }
#endif
        comp971_ble_pilot_publish_status();

        /* Emit retained stall state diagnostics periodically so post-boot telemetry is guaranteed */
        if (g_comp971_retained.magic == COMP971_RETAINED_MAGIC)
        {
            uint32_t now_us = (uint32_t)(((uint64_t)xTaskGetTickCount() * 33000ULL) / 32768ULL * 1000ULL);
            (void)comp971_ble_pilot_publish_uwb_diagnostic_ext(
                0, COMP971_HEALTH_STALL_RECOVERY_CNT, 
                (uint16_t)g_comp971_retained.boot_count,
                (int32_t)(((g_comp971_retained.reset_reason & 0xFFU) << 24) |
                          ((g_comp971_retained.last_escalation_stage & 0xFFU) << 16) |
                          (g_comp971_retained.last_stall_duration_ms & 0xFFFFU)),
                now_us);
            (void)comp971_ble_pilot_publish_uwb_diagnostic_ext(
                0, COMP971_HEALTH_DW3000_SYS_LO,
                (uint16_t)g_comp971_retained.boot_count,
                (int32_t)g_comp971_retained.sys_status_lo,
                now_us);
            (void)comp971_ble_pilot_publish_uwb_diagnostic_ext(
                0, COMP971_HEALTH_DW3000_SYS_HI,
                (uint16_t)g_comp971_retained.boot_count,
                (int32_t)g_comp971_retained.sys_status_hi,
                now_us);
        }
    }
}

static void uwb_task(void *argument)
{
    (void)argument;

    /* If identity is unknown or UWB role is disabled, do not start UWB stack */
    if (g_comp971_uwb_role == COMP971_UWB_DISABLED)
    {
        /* Report unknown identity diagnostic once */
        comp971_ble_pilot_publish_uwb_diagnostic(0, COMP971_ERR_UNKNOWN_IDENTITY, 0, 0);
        for (;;)
        {
            vTaskDelay(pdMS_TO_TICKS(1000U));
        }
    }

    /* Give FreeRTOS scheduler a brief startup slice, then initialize MAC,
     * session parameters, and start ranging synchronously without intermediate delays. */
    vTaskDelay(pdMS_TO_TICKS(COMP971_UWB_START_DELAY_MS));
    comp971_uwb_two_node_platform_init();
    comp971_uwb_two_node_init();
    comp971_uwb_two_node_start();
    comp971_uwb_two_node_publish_pending_event();

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(100U));
        comp971_uwb_two_node_publish_pending_event();
    }
}

__attribute__((unused)) static void imu_task(void *argument)
{
    struct comp971_imu_batch_v1 batch;
    struct comp971_icm20948_sample sample;
    TickType_t start_tick;
    uint32_t sequence = 0U;
    uint32_t consecutive_errors = 0U;
    int result;

    (void)argument;
    for (;;)
    {
        result = comp971_icm20948_init();
        if (result == 0)
        {
            break;
        }
        comp971_ble_pilot_set_icm_ready(false);
        Watchdog.refresh();
        vTaskDelay(pdMS_TO_TICKS(1000U));
    }

    comp971_ble_pilot_set_icm_ready(true);
    memset(&batch, 0, sizeof(batch));
    batch.protocol_version = COMP971_PROTOCOL_VERSION;
    batch.message_type = COMP971_TELEMETRY_IMU_BATCH;
    batch.node_index = (uint8_t)g_comp971_node_index;
    start_tick = xTaskGetTickCount();

    for (;;)
    {
        TickType_t target_tick;
        uint8_t index = batch.sample_count;

        /* FreeRTOS RTC tick rate with prescaler 32 is 32768 / 33 = 992.969697 Hz.
         * Using exact integer fraction 32768 / (33 * 60) = 32768 / 1980 ticks per sample
         * delivers exact 60.000 Hz wall-clock IMU sampling rate (0.0 ppm error). */
        target_tick = start_tick + (TickType_t)(((uint64_t)sequence * 32768ULL) /
            (33ULL * COMP971_IMU_SAMPLE_RATE_HZ));
        if ((int32_t)(target_tick - xTaskGetTickCount()) > 0)
        {
            vTaskDelay(target_tick - xTaskGetTickCount());
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(1U));
            start_tick = xTaskGetTickCount();
            sequence = 0U;
        }

        result = comp971_icm20948_read(&sample);
        if (result != 0)
        {
            consecutive_errors++;
            comp971_ble_pilot_set_icm_ready(false);
            if (consecutive_errors >= 5U)
            {
                (void)comp971_icm20948_init();
                consecutive_errors = 0U;
            }
            vTaskDelay(pdMS_TO_TICKS(16U));
            continue;
        }
        consecutive_errors = 0U;
        comp971_ble_pilot_set_icm_ready(true);
        batch.samples[index].sequence = sequence;
        batch.samples[index].local_timestamp_us = (uint32_t)
            (((uint64_t)(xTaskGetTickCount() - start_tick) * 33000000ULL) /
             32768ULL);
        batch.samples[index].ax = sample.ax;
        batch.samples[index].ay = sample.ay;
        batch.samples[index].az = sample.az;
        batch.samples[index].gx = sample.gx;
        batch.samples[index].gy = sample.gy;
        batch.samples[index].gz = sample.gz;
        batch.samples[index].temperature = sample.temperature;
        ++batch.sample_count;
        ++sequence;
        g_imu_progress_counter++;

        if (batch.sample_count == COMP971_IMU_BATCH_SAMPLES)
        {
            (void)comp971_ble_pilot_publish_imu_batch(&batch);
            batch.sample_count = 0U;
        }
    }
}

int main(void)
{
    ret_code_t result;

    /* Step 0: Capture and clear reset reason register immediately at boot */
    g_boot_resetreas = NRF_POWER->RESETREAS;
    NRF_POWER->RESETREAS = g_boot_resetreas;
#if defined(COMP971_TEST_HOOKS) && (COMP971_TEST_HOOKS != 0)
    /* Induce forced stall test if test flag 0xDEADBEEF is written to retained magic slot */
    if (NRF_POWER->GPREGRET == 0xAA)
    {
        NRF_POWER->GPREGRET = 0;
        extern volatile uint32_t g_fira_force_stall;
        g_fira_force_stall = 1;
    }
#endif

    /* Maintain retained block across boots with CRC32 verification */
    uint32_t expected_crc = comp971_calc_crc32((const uint8_t *)&g_comp971_retained, offsetof(struct comp971_retained_block, crc32));
    if ((g_comp971_retained.magic != COMP971_RETAINED_MAGIC) || (g_comp971_retained.crc32 != expected_crc))
    {
        memset((void *)&g_comp971_retained, 0, sizeof(g_comp971_retained));
        g_comp971_retained.magic = COMP971_RETAINED_MAGIC;
        g_comp971_retained.boot_count = 1U;
        g_comp971_retained.crc32 = comp971_calc_crc32((const uint8_t *)&g_comp971_retained, offsetof(struct comp971_retained_block, crc32));
    }
    else
    {
        g_comp971_retained.boot_count++;
    }
    g_comp971_retained.reset_reason = g_boot_resetreas;
    g_comp971_retained.crc32 = comp971_calc_crc32((const uint8_t *)&g_comp971_retained, offsetof(struct comp971_retained_block, crc32));

    /* Maintain frozen record across boots with commit state and CRC32 verification */
    uint32_t expected_frozen_crc = comp971_calc_frozen_crc(&g_comp971_frozen);
    if (g_comp971_frozen.commit_state == COMP971_COMMIT_STATE_COMMITTED)
    {
        if (g_comp971_frozen.crc32 != expected_frozen_crc)
        {
            /* Corrupted torn record: clear */
            memset((void *)&g_comp971_frozen, 0, sizeof(g_comp971_frozen));
            memset((void *)&g_comp971_frozen_meta, 0, sizeof(g_comp971_frozen_meta));
        }
    }
    else if (g_comp971_frozen.commit_state == COMP971_COMMIT_STATE_WRITING)
    {
        /* Incomplete commit interrupted by reset before completion: clear */
        memset((void *)&g_comp971_frozen, 0, sizeof(g_comp971_frozen));
        memset((void *)&g_comp971_frozen_meta, 0, sizeof(g_comp971_frozen_meta));
    }

    /* Validate delivery metadata */
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
            g_comp971_frozen_meta.first_post_reset_reason = g_boot_resetreas;
            g_comp971_frozen_meta.crc32 = comp971_calc_crc32((const uint8_t *)&g_comp971_frozen_meta, offsetof(struct comp971_frozen_delivery_meta, crc32));
        }
    }

    handle_cpu_protect();
    handle_fpu_irq();

    result = nrf_drv_clock_init();
    if ((result != NRF_SUCCESS) && (result != NRF_ERROR_MODULE_ALREADY_INITIALIZED))
    {
        error_handler(1, _ERR);
    }

    nrf_drv_clock_hfclk_request(NULL);
    while (!nrf_drv_clock_hfclk_is_running())
    {
    }

    nrfx_clock_lfclk_start();
    while (!nrfx_clock_lfclk_is_running())
    {
    }
    nrfx_nvmc_icache_enable();
    persistent_time_init(0);

    /* Configure Green LED (D11 / P0.22, active low) and Red LED (D9 / P0.04) */
    nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(0, 22));
    nrf_gpio_pin_write(NRF_GPIO_PIN_MAP(0, 22), 1);
    nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(0, 4));
    nrf_gpio_pin_write(NRF_GPIO_PIN_MAP(0, 4), 1);

    /* Auto-discover hardware identity and FiRa role from read-only FICR DEVICEID */
    comp971_identify_node();

    /* Step 2.6: Full system integration — 1 Hz status, 1 Hz round-robin health telemetry, IMU batches @ 60 Hz, and UWB range telemetry published over BLE. */
    comp971_ble_pilot_init_ext((uint8_t)g_comp971_node_index, g_comp971_device_name, true);
    comp971_ble_pilot_set_allow_notifications(true, true, true, true);
    if (xTaskCreate(status_task, "comp971stat", 256U, NULL, 2U, (TaskHandle_t *)&g_comp971_status_task_handle) != pdPASS)
    {
        error_handler(1, _ERR_Malloc_Failed);
    }
    if (xTaskCreate(imu_task, "comp971imu", 384U, NULL, 2U, (TaskHandle_t *)&g_comp971_imu_task_handle) != pdPASS)
    {
        error_handler(1, _ERR_Malloc_Failed);
    }
    if (xTaskCreate(uwb_task, "comp971uwb", COMP971_UWB_TASK_STACK_WORDS,
                    NULL, 2U, (TaskHandle_t *)&g_comp971_uwb_task_handle) != pdPASS)
    {
        error_handler(1, _ERR_Malloc_Failed);
    }
    if (xTaskCreate(supervisor_task, "comp971sup", 256U,
                    NULL, 3U, (TaskHandle_t *)&g_comp971_supervisor_task_handle) != pdPASS)
    {
        error_handler(1, _ERR_Malloc_Failed);
    }

    /* Initialize hardware watchdog with 8000 ms reload value (provides final hardware backstop) */
    Watchdog.init(8000);

    qos_start();
    for (;;)
    {
    }
}
