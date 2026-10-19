/*
 * External Adafruit ICM-20948 reader, 60 Hz, on J10 SDA=P0.26/SCL=P0.23.
 * Raw units match the verified standalone diagnostic stream.
 */
#include <errno.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "icm20948_stream.h"

#define ICM_ADDRESS 0x69U
#define REG_BANK_SEL 0x7FU
#define REG_WHO_AM_I 0x00U
#define REG_LP_CONFIG 0x05U
#define REG_PWR_MGMT_1 0x06U
#define REG_PWR_MGMT_2 0x07U
#define REG_ACCEL_XOUT_H 0x2DU
#define REG_GYRO_CONFIG_1 0x01U
#define REG_ACCEL_CONFIG 0x14U
#define ICM_RATE_HZ 60U

static const struct device *const i2c_device = DEVICE_DT_GET(DT_NODELABEL(i2c0));
static int current_bank = -1;
static icm20948_sample_callback_t sample_callback;

static int select_bank(uint8_t bank)
{
	int result;
	if (bank > 3U) { return -EINVAL; }
	if (current_bank == bank) { return 0; }
	result = i2c_reg_write_byte(i2c_device, ICM_ADDRESS, REG_BANK_SEL,
				    (uint8_t)(bank << 4));
	if (result == 0) { current_bank = bank; }
	return result;
}

static int write_register(uint8_t bank, uint8_t reg, uint8_t value)
{
	int result = select_bank(bank);
	return result == 0 ? i2c_reg_write_byte(i2c_device, ICM_ADDRESS, reg, value) : result;
}

static int read_register(uint8_t bank, uint8_t reg, uint8_t *value)
{
	int result = select_bank(bank);
	return result == 0 ? i2c_reg_read_byte(i2c_device, ICM_ADDRESS, reg, value) : result;
}

static int read_registers(uint8_t bank, uint8_t reg, uint8_t *data, size_t length)
{
	int result = select_bank(bank);
	return result == 0 ? i2c_burst_read(i2c_device, ICM_ADDRESS, reg, data, length) : result;
}

static int16_t decode_be(const uint8_t *data)
{
	return (int16_t)(((uint16_t)data[0] << 8) | data[1]);
}

static int initialize_icm20948(void)
{
	uint8_t value = 0U;
	int result;

	current_bank = -1;
	result = read_register(0, REG_WHO_AM_I, &value);
	if (result != 0 || value != 0xEAU) { return result != 0 ? result : -ENODEV; }
	result = write_register(0, REG_PWR_MGMT_1, 0x80U);
	if (result != 0) { return result; }
	current_bank = -1;
	k_msleep(100);
	if ((result = write_register(0, REG_PWR_MGMT_1, 0x01U)) != 0 ||
	    (result = write_register(0, REG_PWR_MGMT_2, 0x00U)) != 0 ||
	    (result = write_register(0, REG_LP_CONFIG, 0x00U)) != 0 ||
	    (result = write_register(2, REG_ACCEL_CONFIG, 0x00U)) != 0 ||
	    (result = write_register(2, REG_GYRO_CONFIG_1, 0x00U)) != 0) {
		return result;
	}
	return select_bank(0);
}

static void icm_thread(void *a, void *b, void *c)
{
	uint8_t frame[14];
	uint32_t sequence = 0U;
	int64_t start_us = k_ticks_to_us_floor64(k_uptime_ticks());
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

	while (true) {
		struct icm20948_sample sample;
		int result = read_registers(0, REG_ACCEL_XOUT_H, frame, sizeof(frame));
		if (result == 0 && sample_callback) {
			uint32_t sample_time_us = (uint32_t)k_ticks_to_us_floor64(k_uptime_ticks());
			sample.sequence = sequence;
			sample.timestamp_us = sample_time_us;
			sample.timestamp_ms = sample_time_us / 1000U;
			sample.ax = decode_be(&frame[0]); sample.ay = decode_be(&frame[2]); sample.az = decode_be(&frame[4]);
			sample.gx = decode_be(&frame[6]); sample.gy = decode_be(&frame[8]); sample.gz = decode_be(&frame[10]);
			sample.temperature = decode_be(&frame[12]);
			sample_callback(&sample);
		}
#if defined(CONFIG_PRINTK) && CONFIG_PRINTK
		else if (result != 0 && (sequence % ICM_RATE_HZ) == 0U) {
			printk("ICM_READ_ERROR=%d\n", result);
		}
#endif
		++sequence;
		int64_t target_us = start_us + ((int64_t)sequence * 1000000LL) / ICM_RATE_HZ;
		int64_t now_us = k_ticks_to_us_floor64(k_uptime_ticks());
		if (target_us > now_us) { k_usleep((int32_t)(target_us - now_us)); }
	}
}

/* Keep the fusion sensor cadence ahead of the continuously running UWB loop. */
K_THREAD_DEFINE(icm_reader_thread, 1536, icm_thread, NULL, NULL, NULL, -1, 0,
		SYS_FOREVER_MS);

int icm20948_start(icm20948_sample_callback_t callback)
{
	int result;
	if (!device_is_ready(i2c_device)) { return -ENODEV; }
	result = initialize_icm20948();
	if (result != 0) { return result; }
	sample_callback = callback;
	k_thread_start(icm_reader_thread);
	return 0;
}
