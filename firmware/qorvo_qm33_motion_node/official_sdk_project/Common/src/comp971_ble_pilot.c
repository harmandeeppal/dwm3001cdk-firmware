#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "sdk_config.h"
#include "app_error.h"
#include "app_util.h"
#include "ble.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_srv_common.h"
#include "nrf_ble_gatt.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_sdh_freertos.h"

#include "FreeRTOS.h"
#include "task.h"
#include "nrf_drv_clock.h"

#include "comp971_ble_pilot.h"
#include "comp971_telemetry_protocol.h"
#include "timers.h"

#define COMP971_BLE_CONN_CFG_TAG 1U
#define COMP971_BLE_OBSERVER_PRIO 3U
#define COMP971_ADV_INTERVAL 160U /* 100 ms */
#define COMP971_ADV_DURATION 0U

/* The two bytes at offset 12 are supplied by the 16-bit service or
 * characteristic value below. This produces the existing COMP971 UUIDs:
 * 6e2fd6a4-6a4c-4b64-b389-139718b02410, then ...a5 and ...a6. */
static ble_uuid128_t const comp971_base_uuid = {
    {0x10, 0x24, 0xB0, 0x18, 0x97, 0x13, 0x89, 0xB3,
     0x64, 0x4B, 0x4C, 0x6A, 0x00, 0x00, 0x2F, 0x6E}};

BLE_ADVERTISING_DEF(m_advertising);
NRF_BLE_GATT_DEF(m_gatt);

static uint8_t m_uuid_type;
static uint16_t m_service_handle;
static ble_gatts_char_handles_t m_telemetry_handles;
static ble_gatts_char_handles_t m_heartbeat_handles;
static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;
volatile uint32_t g_ble_force_disconnect = 0;
volatile uint32_t g_ble_connect_count = 0;
volatile uint32_t g_ble_disconnect_count = 0;
volatile uint32_t g_ble_queue_items_flushed_on_disconnect = 0;
volatile uint32_t g_fira_connect_block_index = 0xFFFFFFFF;
extern volatile uint32_t g_fira_last_block_index;
static bool m_notifications_enabled;
static bool m_allow_status_notify = true;
static bool m_allow_diag_notify = true;
static bool m_allow_range_notify = true;
static bool m_allow_imu_notify = true;
static uint8_t m_node_index;
static bool m_icm_ready;
static bool m_uwb_started;
static uint32_t m_imu_samples_acquired;
static uint32_t m_imu_queue_put_ok;
static uint32_t m_ble_imu_notify_ok;
static uint32_t m_ble_imu_notify_failed;
static uint32_t m_ble_uwb_notify_failed;
static uint32_t m_uwb_range_attempts;
static uint32_t m_uwb_range_valid;
static uint32_t m_uwb_range_invalid;

void comp971_ble_pilot_set_allow_notifications(bool allow_status, bool allow_diag, bool allow_range, bool allow_imu)
{
    m_allow_status_notify = allow_status;
    m_allow_diag_notify = allow_diag;
    m_allow_range_notify = allow_range;
    m_allow_imu_notify = allow_imu;
}

#define COMP971_PENDING_NOTIF_QUEUE_SIZE 48U
#define COMP971_PENDING_NOTIF_MAX_LEN 160U

struct comp971_pending_notif {
    uint8_t len;
    uint8_t data[COMP971_PENDING_NOTIF_MAX_LEN];
};

static struct comp971_pending_notif m_pending_notifs[COMP971_PENDING_NOTIF_QUEUE_SIZE];
static uint8_t m_pending_head;
static uint8_t m_pending_tail;
static uint8_t m_pending_count;
static uint32_t m_pending_queue_high_water;
static uint32_t m_pending_queue_full_drops;
static bool publish_small_notification(const void *payload, uint16_t payload_length);

/* Enqueue histogram: tracks pending queue depth when a new item is queued:
 * bin 0: depth 0
 * bin 1: depth 1..4
 * bin 2: depth 5..8
 * bin 3: depth 9..12
 * bin 4: depth 13..16
 * bin 5: depth 17..24
 * bin 6: depth 25..36
 * bin 7: depth 37..48 */
volatile uint32_t g_ble_queue_depth_hist[8];

static void record_queue_depth_hist(uint8_t count)
{
    if (count == 0U)
    {
        g_ble_queue_depth_hist[0]++;
    }
    else if (count <= 4U)
    {
        g_ble_queue_depth_hist[1]++;
    }
    else if (count <= 8U)
    {
        g_ble_queue_depth_hist[2]++;
    }
    else if (count <= 12U)
    {
        g_ble_queue_depth_hist[3]++;
    }
    else if (count <= 16U)
    {
        g_ble_queue_depth_hist[4]++;
    }
    else if (count <= 24U)
    {
        g_ble_queue_depth_hist[5]++;
    }
    else if (count <= 36U)
    {
        g_ble_queue_depth_hist[6]++;
    }
    else
    {
        g_ble_queue_depth_hist[7]++;
    }
}

static void drain_pending_notifications(void)
{
    if ((m_conn_handle == BLE_CONN_HANDLE_INVALID) || !m_notifications_enabled)
    {
        return;
    }

    for (;;)
    {
        struct comp971_pending_notif item;
        bool has_item = false;

        taskENTER_CRITICAL();
        if (m_pending_count > 0U)
        {
            item = m_pending_notifs[m_pending_head];
            has_item = true;
        }
        taskEXIT_CRITICAL();

        if (!has_item)
        {
            break;
        }

        ble_gatts_hvx_params_t notification;
        uint16_t length = item.len;
        ret_code_t result;

        memset(&notification, 0, sizeof(notification));
        notification.handle = m_telemetry_handles.value_handle;
        notification.type = BLE_GATT_HVX_NOTIFICATION;
        notification.p_data = item.data;
        notification.p_len = &length;

        result = sd_ble_gatts_hvx(m_conn_handle, &notification);
        if (result == NRF_SUCCESS)
        {
            taskENTER_CRITICAL();
            m_pending_head = (m_pending_head + 1U) % COMP971_PENDING_NOTIF_QUEUE_SIZE;
            --m_pending_count;
            taskEXIT_CRITICAL();
        }
        else if (result == NRF_ERROR_RESOURCES)
        {
            /* SoftDevice buffer is full. Wait for next BLE_GATTS_EVT_HVN_TX_COMPLETE */
            break;
        }
        else
        {
            /* Dropped due to disconnect or invalid state */
            taskENTER_CRITICAL();
            m_pending_head = (m_pending_head + 1U) % COMP971_PENDING_NOTIF_QUEUE_SIZE;
            --m_pending_count;
            if (item.data[1] == COMP971_TELEMETRY_IMU_BATCH)
            {
                ++m_ble_imu_notify_failed;
            }
            else
            {
                ++m_ble_uwb_notify_failed;
            }
            taskEXIT_CRITICAL();
            break;
        }
    }
}

static TimerHandle_t m_adv_retry_timer = NULL;

static void adv_retry_timer_callback(TimerCallbackFunction_t xTimer)
{
    (void)xTimer;
    ret_code_t res = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
    if ((res != NRF_SUCCESS) &&
        (res != NRF_ERROR_INVALID_STATE) &&
        (res != NRF_ERROR_CONN_COUNT))
    {
        /* Retry again in 100 ms */
        if (m_adv_retry_timer != NULL)
        {
            (void)xTimerStart(m_adv_retry_timer, 0);
        }
    }
}

static void advertising_start(void *argument)
{
    ret_code_t result;

    (void)argument;
    result = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
    if (result == NRF_SUCCESS ||
        result == NRF_ERROR_INVALID_STATE ||
        result == NRF_ERROR_CONN_COUNT)
    {
        /* Expected or harmless benign states */
        return;
    }

    /* Recover from temporary radio or driver busy errors via FreeRTOS timer retry */
    if (m_adv_retry_timer == NULL)
    {
        m_adv_retry_timer = xTimerCreate("adv_retry", pdMS_TO_TICKS(100U), pdFALSE, NULL, (TimerCallbackFunction_t)adv_retry_timer_callback);
    }
    if (m_adv_retry_timer != NULL)
    {
        (void)xTimerStart(m_adv_retry_timer, 0);
    }
}

static void on_ble_event(ble_evt_t const *event, void *context)
{
    ret_code_t result;
    ble_gatts_evt_write_t const *write;

    (void)context;
    switch (event->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            m_conn_handle = event->evt.gap_evt.conn_handle;
            m_notifications_enabled = false;
            g_ble_connect_count++;
            g_fira_connect_block_index = g_fira_last_block_index;
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            m_conn_handle = BLE_CONN_HANDLE_INVALID;
            m_notifications_enabled = false;
            taskENTER_CRITICAL();
            g_ble_queue_items_flushed_on_disconnect += m_pending_count;
            m_pending_head = 0U;
            m_pending_tail = 0U;
            m_pending_count = 0U;
            taskEXIT_CRITICAL();
#if defined(COMP971_TEST_HOOKS) && (COMP971_TEST_HOOKS != 0)
            extern volatile uint32_t g_ble_disconnect_stress_target;
            extern volatile uint32_t g_ble_disconnect_stress_completed;
            if (g_ble_disconnect_stress_target > g_ble_disconnect_stress_completed)
            {
                g_ble_disconnect_stress_completed++;
            }
#endif
            advertising_start(NULL);
            break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
            ble_gap_phys_t phys = {
                .rx_phys = BLE_GAP_PHY_AUTO,
                .tx_phys = BLE_GAP_PHY_AUTO,
            };
            result = sd_ble_gap_phy_update(event->evt.gap_evt.conn_handle, &phys);
            APP_ERROR_CHECK(result);
            break;
        }

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            result = sd_ble_gatts_sys_attr_set(event->evt.gatts_evt.conn_handle, NULL, 0U, 0U);
            APP_ERROR_CHECK(result);
            break;

        case BLE_GATTS_EVT_WRITE:
            write = &event->evt.gatts_evt.params.write;
            if ((write->handle == m_telemetry_handles.cccd_handle) && (write->len == 2U))
            {
                m_notifications_enabled = ble_srv_is_notification_enabled(write->data);
            }
            else if ((write->handle == m_heartbeat_handles.value_handle) &&
                     (write->len == 1U) &&
                     (write->data[0] == COMP971_GATEWAY_HEARTBEAT_VALUE))
            {
                /* The expected gateway heartbeat was received. The pilot does
                 * not need to persist it; normal connection state is enough. */
            }
            break;

        case BLE_GATTS_EVT_HVN_TX_COMPLETE:
            drain_pending_notifications();
            break;

        default:
            break;
    }
}

NRF_SDH_BLE_OBSERVER(m_comp971_ble_observer, COMP971_BLE_OBSERVER_PRIO, on_ble_event, NULL);

static void ble_stack_init(void)
{
    ret_code_t result;
    uint32_t ram_start = 0U;

    nrf_sdh_disable_request();
    result = nrf_sdh_enable_request();
    APP_ERROR_CHECK(result);
    result = nrf_sdh_ble_default_cfg_set(COMP971_BLE_CONN_CFG_TAG, &ram_start);
    APP_ERROR_CHECK(result);
    result = nrf_sdh_ble_enable(&ram_start);
    APP_ERROR_CHECK(result);

    nrf_drv_clock_hfclk_request(NULL);
    while (!nrf_drv_clock_hfclk_is_running())
    {
    }
}

static void gap_init(char const *device_name)
{
    ret_code_t result;
    ble_gap_conn_sec_mode_t security_mode;
    ble_gap_conn_params_t connection_params;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&security_mode);
    result = sd_ble_gap_device_name_set(&security_mode,
                                        (uint8_t const *)device_name,
                                        (uint16_t)strlen(device_name));
    APP_ERROR_CHECK(result);

    memset(&connection_params, 0, sizeof(connection_params));
    connection_params.min_conn_interval = MSEC_TO_UNITS(30U, UNIT_1_25_MS);
    connection_params.max_conn_interval = MSEC_TO_UNITS(60U, UNIT_1_25_MS);
    connection_params.slave_latency = 0U;
    connection_params.conn_sup_timeout = MSEC_TO_UNITS(4000U, UNIT_10_MS);
    result = sd_ble_gap_ppcp_set(&connection_params);
    APP_ERROR_CHECK(result);
}

static void service_init(void)
{
    ret_code_t result;
    ble_uuid_t service_uuid;
    ble_add_char_params_t characteristic;

    result = sd_ble_uuid_vs_add(&comp971_base_uuid, &m_uuid_type);
    APP_ERROR_CHECK(result);
    service_uuid.type = m_uuid_type;
    service_uuid.uuid = COMP971_SERVICE_UUID_16;
    result = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &service_uuid, &m_service_handle);
    APP_ERROR_CHECK(result);

    memset(&characteristic, 0, sizeof(characteristic));
    characteristic.uuid = COMP971_TELEMETRY_UUID_16;
    characteristic.uuid_type = m_uuid_type;
    characteristic.max_len = COMP971_IMU_BATCH_MAX_WIRE_SIZE;
    characteristic.init_len = 1U;
    characteristic.is_var_len = true;
    characteristic.char_props.notify = 1U;
    characteristic.read_access = SEC_OPEN;
    characteristic.write_access = SEC_OPEN;
    characteristic.cccd_write_access = SEC_OPEN;
    result = characteristic_add(m_service_handle, &characteristic, &m_telemetry_handles);
    APP_ERROR_CHECK(result);

    memset(&characteristic, 0, sizeof(characteristic));
    characteristic.uuid = COMP971_HEARTBEAT_UUID_16;
    characteristic.uuid_type = m_uuid_type;
    characteristic.max_len = 1U;
    characteristic.init_len = 1U;
    characteristic.is_var_len = false;
    characteristic.char_props.write_wo_resp = 1U;
    characteristic.read_access = SEC_OPEN;
    characteristic.write_access = SEC_OPEN;
    result = characteristic_add(m_service_handle, &characteristic, &m_heartbeat_handles);
    APP_ERROR_CHECK(result);

}

static void gatt_init(void)
{
    ret_code_t result = nrf_ble_gatt_init(&m_gatt, NULL);
    APP_ERROR_CHECK(result);
    result = nrf_ble_gatt_att_mtu_periph_set(&m_gatt, 247U);
    APP_ERROR_CHECK(result);
}

static void advertising_init(void)
{
    ret_code_t result;
    ble_advertising_init_t advertising;

    memset(&advertising, 0, sizeof(advertising));
    /* The existing gateway uses passive scanning, so its discovery name must
     * be in the primary advertisement rather than the scan response. */
    advertising.advdata.name_type = BLE_ADVDATA_FULL_NAME;
    advertising.advdata.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    advertising.config.ble_adv_fast_enabled = true;
    advertising.config.ble_adv_fast_interval = COMP971_ADV_INTERVAL;
    advertising.config.ble_adv_fast_timeout = COMP971_ADV_DURATION;
    advertising.config.ble_adv_slow_enabled = false;
    advertising.config.ble_adv_on_disconnect_disabled = true;
    result = ble_advertising_init(&m_advertising, &advertising);
    APP_ERROR_CHECK(result);
    ble_advertising_conn_cfg_tag_set(&m_advertising, COMP971_BLE_CONN_CFG_TAG);
}

void comp971_ble_pilot_init_ext(uint8_t node_index, const char *device_name, bool start_advertising)
{
    m_node_index = node_index;
    m_conn_handle = BLE_CONN_HANDLE_INVALID;
    m_notifications_enabled = false;
    m_icm_ready = false;
    m_uwb_started = false;
    m_imu_samples_acquired = 0U;
    m_imu_queue_put_ok = 0U;
    m_ble_imu_notify_ok = 0U;
    m_ble_imu_notify_failed = 0U;
    m_ble_uwb_notify_failed = 0U;
    m_uwb_range_attempts = 0U;
    m_uwb_range_valid = 0U;
    m_uwb_range_invalid = 0U;
    m_pending_head = 0U;
    m_pending_tail = 0U;
    m_pending_count = 0U;
    memset(m_pending_notifs, 0, sizeof(m_pending_notifs));

    ble_stack_init();
    gap_init(device_name);
    service_init();
    gatt_init();
    advertising_init();
    nrf_sdh_freertos_init(start_advertising ? advertising_start : NULL, NULL);
}

void comp971_ble_pilot_start_advertising(void)
{
    advertising_start(NULL);
}

void comp971_ble_pilot_init(uint8_t node_index, const char *device_name)
{
    comp971_ble_pilot_init_ext(node_index, device_name, true);
}

bool comp971_ble_pilot_is_connected_and_notifying(void)
{
    return (m_conn_handle != BLE_CONN_HANDLE_INVALID) && m_notifications_enabled;
}

void comp971_ble_pilot_publish_status(void)
{
    struct comp971_node_status_v1 status;

    if (g_ble_force_disconnect != 0)
    {
        if (m_conn_handle != BLE_CONN_HANDLE_INVALID)
        {
            g_ble_force_disconnect = 0;
            g_ble_disconnect_count++;
            (void)sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            return;
        }
    }

    if (!m_allow_status_notify || (m_conn_handle == BLE_CONN_HANDLE_INVALID) || !m_notifications_enabled)
    {
        return;
    }

    drain_pending_notifications();

    memset(&status, 0, sizeof(status));
    status.protocol_version = COMP971_PROTOCOL_VERSION;
    status.message_type = COMP971_TELEMETRY_NODE_STATUS;
    status.node_index = m_node_index;
    status.flags = (m_icm_ready ? 0x01U : 0x02U) |
                   (m_uwb_started ? 0x04U : 0x00U);
    status.imu_samples_acquired = m_imu_samples_acquired;
    status.imu_queue_put_ok = m_imu_queue_put_ok;
    status.imu_queue_dropped = m_pending_queue_full_drops;
    status.imu_queue_high_water = m_pending_queue_high_water;
    status.ble_imu_notify_ok = m_ble_imu_notify_ok;
    status.ble_imu_notify_failed = m_ble_imu_notify_failed;
    status.ble_uwb_notify_failed = m_ble_uwb_notify_failed;
    status.uwb_range_attempts = m_uwb_range_attempts;
    status.uwb_range_valid = m_uwb_range_valid;
    status.uwb_range_invalid = m_uwb_range_invalid;

    (void)publish_small_notification(&status, sizeof(status));
}

void comp971_ble_pilot_set_icm_ready(bool ready)
{
    m_icm_ready = ready;
}

bool comp971_ble_pilot_publish_imu_batch(const struct comp971_imu_batch_v1 *batch)
{
    ble_gatts_hvx_params_t notification;
    uint16_t length;
    ret_code_t result;

    if ((batch == NULL) || (batch->sample_count == 0U) ||
        (batch->sample_count > COMP971_IMU_BATCH_MAX_SAMPLES))
    {
        return false;
    }

    m_imu_samples_acquired += batch->sample_count;
    m_imu_queue_put_ok += batch->sample_count;
    if (!m_allow_imu_notify || (m_conn_handle == BLE_CONN_HANDLE_INVALID) || !m_notifications_enabled)
    {
        return false;
    }

    drain_pending_notifications();

    taskENTER_CRITICAL();
    const bool queue_empty = (m_pending_count == 0U);
    taskEXIT_CRITICAL();

    length = (uint16_t)(COMP971_IMU_BATCH_HEADER_SIZE +
                        ((uint16_t)batch->sample_count * COMP971_IMU_SAMPLE_WIRE_SIZE));

    if (queue_empty)
    {
        memset(&notification, 0, sizeof(notification));
        notification.handle = m_telemetry_handles.value_handle;
        notification.type = BLE_GATT_HVX_NOTIFICATION;
        notification.p_data = (uint8_t const *)batch;
        notification.p_len = &length;
        result = sd_ble_gatts_hvx(m_conn_handle, &notification);
        if (result == NRF_SUCCESS)
        {
            ++m_ble_imu_notify_ok;
            return true;
        }
        if (result != NRF_ERROR_RESOURCES)
        {
            ++m_ble_imu_notify_failed;
            return false;
        }
        /* If NRF_ERROR_RESOURCES, fall through to queue in pending FIFO */
    }

    taskENTER_CRITICAL();
    if ((m_pending_count < COMP971_PENDING_NOTIF_QUEUE_SIZE) &&
        (length <= COMP971_PENDING_NOTIF_MAX_LEN))
    {
        struct comp971_pending_notif *item = &m_pending_notifs[m_pending_tail];
        item->len = (uint8_t)length;
        memcpy(item->data, batch, length);
        m_pending_tail = (m_pending_tail + 1U) % COMP971_PENDING_NOTIF_QUEUE_SIZE;
        record_queue_depth_hist(m_pending_count);
        ++m_pending_count;
        if (m_pending_count > m_pending_queue_high_water)
        {
            m_pending_queue_high_water = (uint8_t)m_pending_count;
        }
        ++m_ble_imu_notify_ok;
        taskEXIT_CRITICAL();
        return true;
    }
    else
    {
        ++m_pending_queue_full_drops;
        ++m_ble_imu_notify_failed;
        taskEXIT_CRITICAL();
        return false;
    }
}

void comp971_ble_pilot_set_uwb_started(bool started)
{
    m_uwb_started = started;
}

void comp971_ble_pilot_record_uwb_result(bool valid)
{
    ++m_uwb_range_attempts;
    if (valid)
    {
        ++m_uwb_range_valid;
    }
    else
    {
        ++m_uwb_range_invalid;
    }
}

static bool publish_small_notification(const void *payload, uint16_t payload_length)
{
    if ((payload == NULL) || (payload_length == 0U) ||
        (payload_length > COMP971_PENDING_NOTIF_MAX_LEN))
    {
        return false;
    }

    if ((m_conn_handle == BLE_CONN_HANDLE_INVALID) || !m_notifications_enabled)
    {
        return false;
    }

    drain_pending_notifications();

    taskENTER_CRITICAL();
    const bool queue_empty = (m_pending_count == 0U);
    taskEXIT_CRITICAL();

    if (queue_empty)
    {
        ble_gatts_hvx_params_t notification;
        uint16_t length = payload_length;
        ret_code_t result;

        memset(&notification, 0, sizeof(notification));
        notification.handle = m_telemetry_handles.value_handle;
        notification.type = BLE_GATT_HVX_NOTIFICATION;
        notification.p_data = (uint8_t const *)payload;
        notification.p_len = &length;

        result = sd_ble_gatts_hvx(m_conn_handle, &notification);
        if (result == NRF_SUCCESS)
        {
            return true;
        }
        if (result != NRF_ERROR_RESOURCES)
        {
            ++m_ble_uwb_notify_failed;
            return false;
        }
        /* If NRF_ERROR_RESOURCES, fall through to buffer in FIFO */
    }

    taskENTER_CRITICAL();
    if (m_pending_count < COMP971_PENDING_NOTIF_QUEUE_SIZE)
    {
        struct comp971_pending_notif *item = &m_pending_notifs[m_pending_tail];
        item->len = (uint8_t)payload_length;
        memcpy(item->data, payload, payload_length);
        m_pending_tail = (m_pending_tail + 1U) % COMP971_PENDING_NOTIF_QUEUE_SIZE;
        record_queue_depth_hist(m_pending_count);
        ++m_pending_count;
        if (m_pending_count > m_pending_queue_high_water)
        {
            m_pending_queue_high_water = (uint8_t)m_pending_count;
        }
        taskEXIT_CRITICAL();
        return true;
    }
    else
    {
        ++m_pending_queue_full_drops;
        ++m_ble_uwb_notify_failed;
        taskEXIT_CRITICAL();
        return false;
    }
}

bool comp971_ble_pilot_publish_uwb_range(uint8_t pair_index, int32_t range_cm,
                                          uint16_t quality_flags,
                                          uint16_t status_code,
                                          uint32_t local_timestamp_us)
{
    struct comp971_uwb_range_v1 range;

    if (!m_allow_range_notify)
    {
        return false;
    }

    memset(&range, 0, sizeof(range));
    range.protocol_version = COMP971_PROTOCOL_VERSION;
    range.message_type = COMP971_TELEMETRY_UWB_RANGE;
    range.source_node = m_node_index;
    range.pair_index = pair_index;
    range.pelvis_timestamp_us = local_timestamp_us;
    range.range_cm = range_cm;
    range.quality_flags = quality_flags;
    range.status_code = status_code;
    return publish_small_notification(&range, sizeof(range));
}

bool comp971_ble_pilot_publish_star_ranges(const struct comp971_star_ranges_v1 *star_ranges)
{
    if (!m_allow_range_notify || (star_ranges == NULL))
    {
        return false;
    }
    return publish_small_notification(star_ranges, sizeof(*star_ranges));
}

bool comp971_ble_pilot_publish_uwb_diagnostic_ext(uint8_t pair_index,
                                                   uint8_t stage_code,
                                                   uint16_t block_index_snapshot,
                                                   int32_t detail,
                                                   uint32_t local_timestamp_us)
{
    struct comp971_uwb_diagnostic_v1 diagnostic;
    ble_gatts_hvx_params_t notification;
    uint16_t length;
    ret_code_t result;

    if (!m_allow_diag_notify)
    {
        return false;
    }

    if ((m_conn_handle == BLE_CONN_HANDLE_INVALID) || !m_notifications_enabled)
    {
        return false;
    }

    drain_pending_notifications();

    memset(&diagnostic, 0, sizeof(diagnostic));
    diagnostic.protocol_version = COMP971_PROTOCOL_VERSION;
    diagnostic.message_type = COMP971_TELEMETRY_UWB_DIAGNOSTIC;
    diagnostic.node_index = m_node_index;
    diagnostic.pair_index = pair_index;
    diagnostic.stage_code = stage_code;
    diagnostic.reserved[0] = (uint8_t)(block_index_snapshot & 0xFFU);
    diagnostic.reserved[1] = (uint8_t)((block_index_snapshot >> 8) & 0xFFU);
    diagnostic.reserved[2] = 0U;
    diagnostic.local_timestamp_us = local_timestamp_us;
    diagnostic.detail = detail;

    memset(&notification, 0, sizeof(notification));
    length = sizeof(diagnostic);
    notification.handle = m_telemetry_handles.value_handle;
    notification.type = BLE_GATT_HVX_NOTIFICATION;
    notification.p_data = (uint8_t const *)&diagnostic;
    notification.p_len = &length;

    /* Diagnostics are status updates: transmit immediately if SoftDevice has room,
     * but DO NOT buffer into the retry queue to protect IMU airtime. Latest wins. */
    result = sd_ble_gatts_hvx(m_conn_handle, &notification);
    return (result == NRF_SUCCESS);
}

bool comp971_ble_pilot_publish_uwb_diagnostic(uint8_t pair_index,
                                               uint8_t stage_code,
                                               int32_t detail,
                                               uint32_t local_timestamp_us)
{
    return comp971_ble_pilot_publish_uwb_diagnostic_ext(pair_index, stage_code, 0U, detail, local_timestamp_us);
}

bool comp971_ble_pilot_publish_stall_snapshot(const struct comp971_stall_snapshot_v1 *snapshot)
{
    ble_gatts_hvx_params_t notification;
    uint16_t length;
    ret_code_t result;

    if (snapshot == NULL)
    {
        return false;
    }

    if ((m_conn_handle == BLE_CONN_HANDLE_INVALID) || !m_notifications_enabled)
    {
        return false;
    }

    drain_pending_notifications();

    memset(&notification, 0, sizeof(notification));
    length = sizeof(struct comp971_stall_snapshot_v1);
    notification.handle = m_telemetry_handles.value_handle;
    notification.type = BLE_GATT_HVX_NOTIFICATION;
    notification.p_data = (uint8_t const *)snapshot;
    notification.p_len = &length;

    result = sd_ble_gatts_hvx(m_conn_handle, &notification);
    return (result == NRF_SUCCESS);
}
