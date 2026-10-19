/* External Adafruit ICM-20948 reader for DWM3001CDK J10 I2C. */
#ifndef ICM20948_STREAM_H
#define ICM20948_STREAM_H

#include <stdint.h>

struct icm20948_sample {
	uint32_t sequence;
	uint32_t timestamp_us;
	uint32_t timestamp_ms;
	int16_t ax;
	int16_t ay;
	int16_t az;
	int16_t gx;
	int16_t gy;
	int16_t gz;
	int16_t temperature;
};

typedef void (*icm20948_sample_callback_t)(const struct icm20948_sample *sample);

/* Starts the ICM-20948 reader at 60 Hz. Returns zero on successful startup. */
int icm20948_start(icm20948_sample_callback_t callback);

#endif
