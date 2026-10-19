#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "generated_probe_config.h"

#define UWB_SPI_NODE DT_NODELABEL(spi3)
#define GPIO0_NODE DT_NODELABEL(gpio0)
#define PASS_LED_NODE DT_ALIAS(led0)
#define ERROR_LED_NODE DT_ALIAS(led1)
#define RUN_LED_NODE DT_ALIAS(led2)
#define BAD_ID_LED_NODE DT_ALIAS(led3)
#define UWB_RST_PIN 25U

static const struct device *const uwb_spi = DEVICE_DT_GET(UWB_SPI_NODE);
static const struct device *const gpio0_dev = DEVICE_DT_GET(GPIO0_NODE);
static const struct gpio_dt_spec uwb_cs =
	GPIO_DT_SPEC_GET_BY_IDX(UWB_SPI_NODE, cs_gpios, 0);
static const struct gpio_dt_spec pass_led = GPIO_DT_SPEC_GET(PASS_LED_NODE, gpios);
static const struct gpio_dt_spec error_led = GPIO_DT_SPEC_GET(ERROR_LED_NODE, gpios);
static const struct gpio_dt_spec run_led = GPIO_DT_SPEC_GET(RUN_LED_NODE, gpios);
static const struct gpio_dt_spec bad_id_led = GPIO_DT_SPEC_GET(BAD_ID_LED_NODE, gpios);

static const struct spi_config uwb_spi_config = {
	.frequency = 500000U,
	.operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_TRANSFER_MSB,
	.slave = 0U,
	.cs = NULL,
};

static void set_led(const struct gpio_dt_spec *led, int on)
{
	if (gpio_is_ready_dt(led)) {
		(void)gpio_pin_set_dt(led, on);
	}
}

static void configure_led(const struct gpio_dt_spec *led)
{
	if (gpio_is_ready_dt(led)) {
		(void)gpio_pin_configure_dt(led, GPIO_OUTPUT_INACTIVE);
	}
}

static void blink_run_forever(void)
{
	while (true) {
		set_led(&run_led, 1);
		k_sleep(K_MSEC(250));
		set_led(&run_led, 0);
		k_sleep(K_MSEC(750));
	}
}

static int read_device_id(uint32_t *device_id, uint8_t response[5])
{
	const uint8_t request[5] = { 0U, 0U, 0U, 0U, 0U };
	const struct spi_buf tx_buf = {
		.buf = (void *)request,
		.len = sizeof(request),
	};
	const struct spi_buf rx_buf = {
		.buf = response,
		.len = sizeof(request),
	};
	const struct spi_buf_set tx = {
		.buffers = &tx_buf,
		.count = 1U,
	};
	const struct spi_buf_set rx = {
		.buffers = &rx_buf,
		.count = 1U,
	};

	/* CS is active-low. gpio_pin_set_dt(..., 1) asserts it. */
	(void)gpio_pin_set_dt(&uwb_cs, 1);
	k_busy_wait(1U);
	const int error = spi_transceive(uwb_spi, &uwb_spi_config, &tx, &rx);
	k_busy_wait(1U);
	(void)gpio_pin_set_dt(&uwb_cs, 0);

	if (error != 0) {
		return error;
	}

	*device_id = ((uint32_t)response[1]) |
		     ((uint32_t)response[2] << 8) |
		     ((uint32_t)response[3] << 16) |
		     ((uint32_t)response[4] << 24);
	return 0;
}

int main(void)
{
	configure_led(&pass_led);
	configure_led(&error_led);
	configure_led(&run_led);
	configure_led(&bad_id_led);
	set_led(&run_led, 1);

	printk("DWM3001CDK DW3110 configuration-derived health probe\n");

	if (!device_is_ready(uwb_spi)) {
		printk("FAIL: SPI controller is not ready.\n");
		set_led(&error_led, 1);
		blink_run_forever();
	}

	if (!device_is_ready(gpio0_dev) || !gpio_is_ready_dt(&uwb_cs)) {
		printk("FAIL: UWB chip-select pin is not ready.\n");
		set_led(&error_led, 1);
		blink_run_forever();
	}

	if (gpio_pin_configure_dt(&uwb_cs, GPIO_OUTPUT_INACTIVE) != 0) {
		printk("FAIL: Could not configure UWB chip-select pin.\n");
		set_led(&error_led, 1);
		blink_run_forever();
	}

	/* Wake and hard-reset the DW3110 before its identity is read. */
	(void)gpio_pin_set_dt(&uwb_cs, 1);
	k_sleep(K_MSEC(1));
	(void)gpio_pin_set_dt(&uwb_cs, 0);
	k_sleep(K_MSEC(5));
	(void)gpio_pin_configure(gpio0_dev, UWB_RST_PIN, GPIO_OUTPUT_LOW);
	k_sleep(K_MSEC(2));
	(void)gpio_pin_configure(gpio0_dev, UWB_RST_PIN, GPIO_INPUT);
	k_sleep(K_MSEC(50));

	printk("READY,%s,%s,JLINK=%s,CONFIG_SHA256=%s,UART=%u\n",
	       DWM_PROBE_NODE_NAME, DWM_PROBE_DEVICE_ID,
	       DWM_PROBE_JLINK_SERIAL, DWM_PROBE_CONFIG_SHA256,
	       DWM_PROBE_SERIAL_BAUD);
	printk("DW3110 expected device ID: 0x%08X; radio_tx_enabled=false\n",
	       DWM_PROBE_EXPECTED_DEVICE_ID);
	k_sleep(K_MSEC(DWM_PROBE_BOOT_DELAY_MS));
	set_led(&run_led, 0);

	while (true) {
		uint8_t response[5] = { 0U };
		uint32_t device_id = 0U;
		const int error = read_device_id(&device_id, response);

		if (error != 0) {
			printk("FAIL: SPI read error %d\n", error);
			set_led(&error_led, 1);
			set_led(&bad_id_led, 0);
		} else {
			printk("SPI response: %02X %02X %02X %02X %02X\n",
			       response[0], response[1], response[2],
			       response[3], response[4]);
			printk("DW3110 device ID: 0x%08X\n", device_id);

			if (device_id == DWM_PROBE_EXPECTED_DEVICE_ID) {
				printk("PASS,%s,DW3110_SPI,0x%08X\n",
				       DWM_PROBE_NODE_NAME, device_id);
				set_led(&error_led, 0);
				set_led(&bad_id_led, 0);
				set_led(&run_led, 0);
				set_led(&pass_led, 1);
				while (true) {
					k_sleep(K_MSEC(DWM_PROBE_SUCCESS_STATUS_INTERVAL_MS));
					printk("STATUS,%s,DW3110_SPI,PASS\n",
					       DWM_PROBE_NODE_NAME);
				}
			}

			set_led(&error_led, 0);
			set_led(&bad_id_led, 1);
		}

		printk("Retrying after configured interval.\n\n");
		set_led(&run_led, 1);
		k_sleep(K_MSEC(200));
		set_led(&run_led, 0);
		k_sleep(K_MSEC(DWM_PROBE_RETRY_INTERVAL_MS));
	}

	return 0;
}
