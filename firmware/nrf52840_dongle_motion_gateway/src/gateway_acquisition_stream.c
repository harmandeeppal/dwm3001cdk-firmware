/*
 * The gateway's primary USB path is a framed binary stream, not printk.
 * USB bulk delivery is reliable, while this bounded queue makes an overload
 * explicit through a counter instead of silently replacing older samples.
 */
#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

#include "gateway_acquisition_stream.h"

#define GATEWAY_USB_MAGIC_0 0xC9U
#define GATEWAY_USB_MAGIC_1 0x71U
#define GATEWAY_USB_PROTOCOL_VERSION 1U
#define GATEWAY_USB_HEADER_SIZE 6U
#define GATEWAY_USB_MAX_PAYLOAD 72U
#define GATEWAY_USB_MAX_FRAME (GATEWAY_USB_HEADER_SIZE + GATEWAY_USB_MAX_PAYLOAD)
#define GATEWAY_USB_QUEUE_DEPTH 256U

struct gateway_usb_item {
	uint16_t length;
	uint8_t bytes[GATEWAY_USB_MAX_FRAME];
};

K_MSGQ_DEFINE(usb_output_queue, sizeof(struct gateway_usb_item),
	GATEWAY_USB_QUEUE_DEPTH, 4);
static const struct device *usb_uart;
static atomic_t usb_dtr_asserted;
static atomic_t usb_dropped_records;
static atomic_t usb_queue_high_water;
static bool worker_started;
static struct gateway_usb_item usb_current_item;
static volatile uint16_t usb_current_offset;
static volatile bool usb_current_valid;

/* CDC ACM next provides Zephyr's interrupt-driven UART API, not its async
 * UART API.  Feed complete framed records directly to that API so a record is
 * copied into the CDC TX FIFO in blocks instead of calling uart_poll_out()
 * once per byte. */
static void usb_uart_irq_callback(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);
	if (!uart_irq_update(dev) || !uart_irq_tx_ready(dev)) {
		return;
	}
	while (uart_irq_tx_ready(dev)) {
		if (!usb_current_valid) {
			if (k_msgq_get(&usb_output_queue, &usb_current_item,
					K_NO_WAIT) != 0) {
				uart_irq_tx_disable(dev);
				return;
			}
			usb_current_offset = 0U;
			usb_current_valid = true;
		}
		int written = uart_fifo_fill(dev,
			&usb_current_item.bytes[usb_current_offset],
			usb_current_item.length - usb_current_offset);
		if (written <= 0) {
			return;
		}
		usb_current_offset += (uint16_t)written;
		if (usb_current_offset == usb_current_item.length) {
			usb_current_valid = false;
		}
	}
}

static void update_queue_high_water(uint32_t depth)
{
	atomic_val_t observed = atomic_get(&usb_queue_high_water);

	while (depth > (uint32_t)observed) {
		if (atomic_cas(&usb_queue_high_water, observed, (atomic_val_t)depth)) {
			break;
		}
		observed = atomic_get(&usb_queue_high_water);
	}
}

int gateway_stream_start(void)
{
	uint32_t dtr = 0U;

	if (worker_started) {
		return 0;
	}
	usb_uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	if (!device_is_ready(usb_uart)) {
		return -ENODEV;
	}
	if (uart_irq_callback_user_data_set(usb_uart, usb_uart_irq_callback, NULL) != 0) {
		return -ENOTSUP;
	}
	uart_irq_tx_disable(usb_uart);
	if (uart_line_ctrl_get(usb_uart, UART_LINE_CTRL_DTR, &dtr) == 0) {
		atomic_set(&usb_dtr_asserted, dtr != 0U ? 1 : 0);
	}
	worker_started = true;
	if (atomic_get(&usb_dtr_asserted) != 0) {
		uart_irq_tx_enable(usb_uart);
	}
	return 0;
}

void gateway_stream_handle_usbd_message(const struct usbd_msg *message)
{
	uint32_t dtr = 0U;

	if (message->type != USBD_MSG_CDC_ACM_CONTROL_LINE_STATE ||
		message->dev == NULL) {
		return;
	}
	if (uart_line_ctrl_get(message->dev, UART_LINE_CTRL_DTR, &dtr) == 0) {
		atomic_set(&usb_dtr_asserted, dtr != 0U ? 1 : 0);
		if (dtr != 0U) {
			uart_irq_tx_enable(usb_uart);
		}
	}
}

int gateway_stream_enqueue(uint8_t record_type, const void *payload,
			   uint16_t payload_length)
{
	struct gateway_usb_item item = { 0 };
	int result;

	if (payload == NULL || payload_length > GATEWAY_USB_MAX_PAYLOAD) {
		return -EINVAL;
	}
	item.length = GATEWAY_USB_HEADER_SIZE + payload_length;
	item.bytes[0] = GATEWAY_USB_MAGIC_0;
	item.bytes[1] = GATEWAY_USB_MAGIC_1;
	item.bytes[2] = GATEWAY_USB_PROTOCOL_VERSION;
	item.bytes[3] = record_type;
	item.bytes[4] = (uint8_t)payload_length;
	item.bytes[5] = (uint8_t)(payload_length >> 8);
	memcpy(&item.bytes[GATEWAY_USB_HEADER_SIZE], payload, payload_length);

	result = k_msgq_put(&usb_output_queue, &item, K_NO_WAIT);
	if (result != 0) {
		atomic_inc(&usb_dropped_records);
		return result;
	}
	update_queue_high_water(k_msgq_num_used_get(&usb_output_queue));
	if (atomic_get(&usb_dtr_asserted) != 0) {
		uart_irq_tx_enable(usb_uart);
	}
	return 0;
}

uint32_t gateway_stream_dropped_records(void)
{
	return (uint32_t)atomic_get(&usb_dropped_records);
}

uint32_t gateway_stream_queue_high_water(void)
{
	return (uint32_t)atomic_get(&usb_queue_high_water);
}
