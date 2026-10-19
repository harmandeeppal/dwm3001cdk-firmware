#ifndef COMP971_ICM20948_H
#define COMP971_ICM20948_H

#include <stdbool.h>
#include <stdint.h>

struct comp971_icm20948_sample {
    int16_t ax;
    int16_t ay;
    int16_t az;
    int16_t gx;
    int16_t gy;
    int16_t gz;
    int16_t temperature;
};

/* Uses J10 SDA P0.26 and SCL P0.23 through the unused TWIM1 peripheral. */
int comp971_icm20948_init(void);
int comp971_icm20948_read(struct comp971_icm20948_sample *sample);

#endif /* COMP971_ICM20948_H */
