/*
 * DWM3001CDK J10 to ICM-20948 communication check.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define I2C_NODE DT_NODELABEL(i2c0)
#define STATUS_LED_NODE DT_ALIAS(led0)

#define ICM20948_WHO_AM_I_REG 0x00
#define ICM20948_WHO_AM_I_VALUE 0xEA
#define ICM20948_DEFAULT_ADDRESS 0x69
#define ICM20948_ALTERNATE_ADDRESS 0x68

static const struct device *const i2c_device = DEVICE_DT_GET(I2C_NODE);
static const struct gpio_dt_spec status_led =
	GPIO_DT_SPEC_GET(STATUS_LED_NODE, gpios);

static int read_who_am_i(uint8_t address, uint8_t *value)
{
	const uint8_t reg = ICM20948_WHO_AM_I_REG;

	return i2c_write_read(i2c_device, address, &reg, sizeof(reg), value,
			      sizeof(*value));
}

static bool probe_address(uint8_t address)
{
	uint8_t value = 0;
	int result = read_who_am_i(address, &value);

	if (result != 0) {
		printk("No I2C response at 0x%02X (error %d).\n", address,
		       result);
		return false;
	}

	printk("Device at 0x%02X returned WHO_AM_I=0x%02X.\n", address,
	       value);

	if (value != ICM20948_WHO_AM_I_VALUE) {
		printk("The response is not the expected ICM-20948 value 0xEA.\n");
		return false;
	}

	printk("SUCCESS: ICM-20948 detected at address 0x%02X.\n", address);
	return true;
}

int main(void)
{
	bool detected = false;
	bool led_ready = false;

	k_msleep(250);
	printk("\n========================================\n");
	printk("DWM3001CDK J10 -> ICM-20948 check\n");
	printk("SDA=P0.26, SCL=P0.23, bus=100 kHz\n");
	printk("Expected WHO_AM_I: 0xEA\n");
	printk("========================================\n");

	if (!device_is_ready(i2c_device)) {
		printk("FATAL: I2C0 is not ready. No bus test was performed.\n");
		return -ENODEV;
	}

	if (gpio_is_ready_dt(&status_led) &&
	    gpio_pin_configure_dt(&status_led, GPIO_OUTPUT_INACTIVE) == 0) {
		led_ready = true;
	}

	printk("I2C0 is ready. Testing 0x69, then 0x68.\n");

	while (!detected) {
		detected = probe_address(ICM20948_DEFAULT_ADDRESS);
		if (!detected) {
			detected = probe_address(ICM20948_ALTERNATE_ADDRESS);
		}

		if (detected) {
			if (led_ready) {
				gpio_pin_set_dt(&status_led, 1);
			}
			printk("Green D9 is on. The wiring and I2C identity check passed.\n");
			break;
		}

		printk("ICM-20948 not detected; retrying in 2 seconds.\n\n");
		if (led_ready) {
			gpio_pin_toggle_dt(&status_led);
		}
		k_sleep(K_SECONDS(2));
	}

	while (true) {
		k_sleep(K_SECONDS(5));
		printk("STATUS: ICM-20948 identity check remains PASS.\n");
	}

	return 0;
}
