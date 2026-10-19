/*
 * Configuration-labelled ICM-20948 9-axis live stream.
 *
 * Register setup follows the TDK ICM-20948 register map and the open-source
 * SparkFun ICM-20948 initialization sequence. The serial stream contains raw
 * sensor values; the PC viewer applies the documented scale factors.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "generated_icm_config.h"

#ifndef ICM_NODE_NAME
#error "ICM_NODE_NAME must be supplied by the selected hardware configuration"
#endif

#ifndef ICM_SAMPLE_RATE_HZ
#error "ICM_SAMPLE_RATE_HZ must be supplied by the selected hardware configuration"
#endif

#define I2C_NODE DT_NODELABEL(i2c0)

#define MAG_ADDRESS 0x0C
#define MAG_WIA1_VALUE 0x48
#define MAG_WIA2_VALUE 0x09

#define REG_BANK_SEL 0x7F

/* User bank 0. */
#define REG_WHO_AM_I 0x00
#define REG_USER_CTRL 0x03
#define REG_LP_CONFIG 0x05
#define REG_PWR_MGMT_1 0x06
#define REG_PWR_MGMT_2 0x07
#define REG_INT_PIN_CFG 0x0F
#define REG_I2C_MST_STATUS 0x17
#define REG_ACCEL_XOUT_H 0x2D

/* User bank 2. */
#define REG_GYRO_CONFIG_1 0x01
#define REG_ACCEL_CONFIG 0x14

/* User bank 3. */
#define REG_I2C_MST_CTRL 0x01
#define REG_I2C_SLV0_ADDR 0x03
#define REG_I2C_SLV0_REG 0x04
#define REG_I2C_SLV0_CTRL 0x05
#define REG_I2C_SLV4_ADDR 0x13
#define REG_I2C_SLV4_REG 0x14
#define REG_I2C_SLV4_CTRL 0x15
#define REG_I2C_SLV4_DO 0x16
#define REG_I2C_SLV4_DI 0x17

/* AK09916 registers. */
#define MAG_REG_WIA1 0x00
#define MAG_REG_WIA2 0x01
#define MAG_REG_ST1 0x10
#define MAG_REG_CNTL2 0x31
#define MAG_REG_CNTL3 0x32

#define FRAME_LENGTH 14U
#define MAG_FRAME_LENGTH 9U

static const struct device *const i2c_device = DEVICE_DT_GET(I2C_NODE);
static int current_bank = -1;

static int select_bank(uint8_t bank)
{
	int result;

	if (bank > 3U) {
		return -EINVAL;
	}
	if (current_bank == bank) {
		return 0;
	}

	result = i2c_reg_write_byte(i2c_device, ICM_I2C_ADDRESS, REG_BANK_SEL,
				    (uint8_t)(bank << 4));
	if (result == 0) {
		current_bank = bank;
	}
	return result;
}

static int write_register(uint8_t bank, uint8_t reg, uint8_t value)
{
	int result = select_bank(bank);

	if (result != 0) {
		return result;
	}
	return i2c_reg_write_byte(i2c_device, ICM_I2C_ADDRESS, reg, value);
}

static int read_register(uint8_t bank, uint8_t reg, uint8_t *value)
{
	int result = select_bank(bank);

	if (result != 0) {
		return result;
	}
	return i2c_reg_read_byte(i2c_device, ICM_I2C_ADDRESS, reg, value);
}

static int read_registers(uint8_t bank, uint8_t reg, uint8_t *data,
			  size_t length)
{
	int result = select_bank(bank);

	if (result != 0) {
		return result;
	}
	return i2c_burst_read(i2c_device, ICM_I2C_ADDRESS, reg, data, length);
}

static int magnetometer_write(uint8_t reg, uint8_t value)
{
	return i2c_reg_write_byte(i2c_device, MAG_ADDRESS, reg, value);
}

static int magnetometer_read(uint8_t reg, uint8_t *value)
{
	return i2c_reg_read_byte(i2c_device, MAG_ADDRESS, reg, value);
}

static int magnetometer_read_registers(uint8_t reg, uint8_t *data,
				       size_t length)
{
	return i2c_burst_read(i2c_device, MAG_ADDRESS, reg, data, length);
}

static int initialize_magnetometer(void)
{
	uint8_t int_pin_cfg;
	uint8_t user_ctrl;
	uint8_t wia1 = 0U;
	uint8_t wia2 = 0U;
	int result;

	result = read_register(0, REG_INT_PIN_CFG, &int_pin_cfg);
	if (result != 0) {
		return result;
	}
	/* Disable the ICM internal I2C master, then expose the integrated
	 * AK09916 directly on the primary bus at address 0x0C.
	 */
	result = read_register(0, REG_USER_CTRL, &user_ctrl);
	if (result != 0) {
		return result;
	}
	result = write_register(0, REG_USER_CTRL,
				(uint8_t)(user_ctrl & ~0x20U));
	if (result != 0) {
		return result;
	}
	result = write_register(0, REG_INT_PIN_CFG,
				(uint8_t)(int_pin_cfg | 0x02U));
	if (result != 0) {
		return result;
	}
	k_msleep(10);

	result = magnetometer_read(MAG_REG_WIA1, &wia1);
	if (result == 0) {
		result = magnetometer_read(MAG_REG_WIA2, &wia2);
	}
	if (result != 0 || wia1 != MAG_WIA1_VALUE ||
	    wia2 != MAG_WIA2_VALUE) {
		printk("ERROR,mag_identity,%d,0x%02X,0x%02X\n",
		       result, wia1, wia2);
		return result != 0 ? result : -ENODEV;
	}

	result = magnetometer_write(MAG_REG_CNTL3, 0x01U);
	if (result != 0) {
		return result;
	}
	k_msleep(50);

	/* Continuous mode 4: 100 Hz magnetic measurements. */
	result = magnetometer_write(MAG_REG_CNTL2, 0x08U);
	if (result != 0) {
		return result;
	}

	k_msleep(100);
	return select_bank(0);
}

static int initialize_icm20948(void)
{
	uint8_t who_am_i = 0U;
	int result;

	current_bank = -1;
	result = read_register(0, REG_WHO_AM_I, &who_am_i);
	if (result != 0) {
		return result;
	}
	if (who_am_i != ICM_WHO_AM_I_VALUE) {
		return -ENODEV;
	}

	result = write_register(0, REG_PWR_MGMT_1, 0x80U);
	if (result != 0) {
		return result;
	}
	current_bank = -1;
	k_msleep(100);

	/* Wake, use the best available clock, enable all accel/gyro axes. */
	result = write_register(0, REG_PWR_MGMT_1, 0x01U);
	if (result != 0) {
		return result;
	}
	result = write_register(0, REG_PWR_MGMT_2, 0x00U);
	if (result != 0) {
		return result;
	}
	result = write_register(0, REG_LP_CONFIG, 0x00U);
	if (result != 0) {
		return result;
	}

	/* +/-2 g and +/-250 dps. DLPF bypassed; output rate is configuration-owned. */
	result = write_register(2, REG_ACCEL_CONFIG, 0x00U);
	if (result != 0) {
		return result;
	}
	result = write_register(2, REG_GYRO_CONFIG_1, 0x00U);
	if (result != 0) {
		return result;
	}

	return initialize_magnetometer();
}

static int16_t decode_be(const uint8_t *data)
{
	return (int16_t)(((uint16_t)data[0] << 8) | data[1]);
}

static int16_t decode_le(const uint8_t *data)
{
	return (int16_t)(((uint16_t)data[1] << 8) | data[0]);
}

int main(void)
{
	uint8_t frame[FRAME_LENGTH];
	uint8_t mag_frame[MAG_FRAME_LENGTH];
	uint32_t sequence = 0U;
	int64_t start_us;
	int result;

	if (!device_is_ready(i2c_device)) {
		printk("ERROR,i2c_not_ready\n");
		return -ENODEV;
	}

	result = initialize_icm20948();
	if (result != 0) {
		printk("ERROR,initialization,%d\n", result);
		return result;
	}

	printk("READY,%s,%s,ICM-20948,0x%02X,%uHz,JLINK=%s,CONFIG_SHA256=%s\n",
	       ICM_NODE_NAME, ICM_DEVICE_ID, ICM_I2C_ADDRESS, ICM_SAMPLE_RATE_HZ,
	       ICM_JLINK_SERIAL, ICM_CONFIG_SHA256);
	printk("FIELDS,seq,t_ms,ax_raw,ay_raw,az_raw,gx_raw,gy_raw,gz_raw,"
	       "mx_raw,my_raw,mz_raw,temp_raw,mag_st1,mag_st2\n");

	start_us = k_ticks_to_us_floor64(k_uptime_ticks());

	while (true) {
		int16_t ax;
		int16_t ay;
		int16_t az;
		int16_t gx;
		int16_t gy;
		int16_t gz;
		int16_t mx;
		int16_t my;
		int16_t mz;
		int16_t temperature;
		int64_t target_us;
		int64_t now_us;

		result = read_registers(0, REG_ACCEL_XOUT_H, frame,
					FRAME_LENGTH);
		if (result != 0) {
			printk("ERROR,read,%d\n", result);
			k_msleep(100);
			continue;
		}
		result = magnetometer_read_registers(MAG_REG_ST1, mag_frame,
						       MAG_FRAME_LENGTH);
		if (result != 0) {
			printk("ERROR,mag_read,%d\n", result);
			k_msleep(100);
			continue;
		}

		ax = decode_be(&frame[0]);
		ay = decode_be(&frame[2]);
		az = decode_be(&frame[4]);
		gx = decode_be(&frame[6]);
		gy = decode_be(&frame[8]);
		gz = decode_be(&frame[10]);
		temperature = decode_be(&frame[12]);
		mx = decode_le(&mag_frame[1]);
		my = decode_le(&mag_frame[3]);
		mz = decode_le(&mag_frame[5]);

		printk("ICM,%u,%u,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%u,%u\n",
		       sequence, k_uptime_get_32(), ax, ay, az, gx, gy, gz,
		       mx, my, mz, temperature, mag_frame[0], mag_frame[8]);

		++sequence;
		target_us = start_us +
			((int64_t)sequence * 1000000LL) / ICM_SAMPLE_RATE_HZ;
		now_us = k_ticks_to_us_floor64(k_uptime_ticks());
		if (target_us > now_us) {
			k_usleep((int32_t)(target_us - now_us));
		}
	}

	return 0;
}
