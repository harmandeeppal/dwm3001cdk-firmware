#ifndef COMP971_BLE_PILOT_H
#define COMP971_BLE_PILOT_H

#include <stdbool.h>
#include <stdint.h>

#include "comp971_telemetry_protocol.h"

void comp971_ble_pilot_init(uint8_t node_index, const char *device_name);
void comp971_ble_pilot_init_ext(uint8_t node_index, const char *device_name, bool start_advertising);
void comp971_ble_pilot_start_advertising(void);
void comp971_ble_pilot_set_allow_notifications(bool allow_status, bool allow_diag, bool allow_range, bool allow_imu);
bool comp971_ble_pilot_is_connected_and_notifying(void);
void comp971_ble_pilot_publish_status(void);
void comp971_ble_pilot_set_icm_ready(bool ready);
bool comp971_ble_pilot_publish_imu_batch(const struct comp971_imu_batch_v1 *batch);
void comp971_ble_pilot_set_uwb_started(bool started);
void comp971_ble_pilot_record_uwb_result(bool valid);
bool comp971_ble_pilot_publish_uwb_range(uint8_t pair_index, int32_t range_cm,
                                          uint16_t quality_flags,
                                          uint16_t status_code,
                                          uint32_t local_timestamp_us);
bool comp971_ble_pilot_publish_uwb_diagnostic(uint8_t pair_index,
                                               uint8_t stage_code,
                                               int32_t detail,
                                               uint32_t local_timestamp_us);
bool comp971_ble_pilot_publish_uwb_diagnostic_ext(uint8_t pair_index,
                                                   uint8_t stage_code,
                                                   uint16_t block_index_snapshot,
                                                   int32_t detail,
                                                   uint32_t local_timestamp_us);
bool comp971_ble_pilot_publish_stall_snapshot(const struct comp971_stall_snapshot_v1 *snapshot);
bool comp971_ble_pilot_publish_star_ranges(const struct comp971_star_ranges_v1 *star_ranges);

#endif /* COMP971_BLE_PILOT_H */
