/* BLE transport for the combined UWB + IMU firmware. */
#ifndef BLE_MOTION_TRANSPORT_H
#define BLE_MOTION_TRANSPORT_H

#include <stdint.h>
#include <stdbool.h>

#include "icm20948_stream.h"

int motion_ble_start(void);
void motion_ble_queue_imu(const struct icm20948_sample *sample);
void motion_ble_publish_range(uint8_t pair_index, int32_t range_cm);
void motion_ble_publish_uwb_diagnostic(uint8_t pair_index, uint8_t stage_code,
	int32_t detail);
void motion_ble_record_uwb_result(int32_t range_cm, bool result_timeout);

#endif /* BLE_MOTION_TRANSPORT_H */
