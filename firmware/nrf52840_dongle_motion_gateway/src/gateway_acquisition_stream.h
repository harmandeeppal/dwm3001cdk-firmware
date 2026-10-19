/* Bounded binary USB output for the COMP971 acquisition gateway. */
#ifndef GATEWAY_ACQUISITION_STREAM_H
#define GATEWAY_ACQUISITION_STREAM_H

#include <stdint.h>

#include <zephyr/usb/usbd.h>

int gateway_stream_start(void);
void gateway_stream_handle_usbd_message(const struct usbd_msg *message);
int gateway_stream_enqueue(uint8_t record_type, const void *payload,
			   uint16_t payload_length);
uint32_t gateway_stream_dropped_records(void);
uint32_t gateway_stream_queue_high_water(void);

#endif /* GATEWAY_ACQUISITION_STREAM_H */
