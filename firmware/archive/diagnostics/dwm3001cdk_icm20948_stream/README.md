# DWM3001CDK ICM-20948 live stream diagnostic

This is a temporary bench diagnostic for one explicitly selected registered
device. It reads the external Adafruit ICM-20948 over J10 I2C and emits raw
accelerometer, gyroscope, magnetometer, and temperature channels at the rate
owned by `config/profiles/six_node_telemetry_v1.json`.

The C source requires a generated configuration header containing the permanent
identity, J-Link serial, serial rate, sample rate, I2C address, and expected
`WHO_AM_I`. Generate it with `scripts/diagnostics/prepare_icm20948_stream_config.py`;
there is no default physical target in the firmware.

## Wiring

| Selected DWM3001CDK device | ICM-20948 micro-hook cable |
|---|---|
| J2 jumper metal test point, measured 3.26 V | VIN, red |
| J10 pin 6, GND | GND, black |
| J10 pin 3, SDA_RPI / P0.26 | SDA, blue |
| J10 pin 5, SCL_RPI / P0.23 | SCL, yellow |

J10 pins 2 and 4 are power inputs and must not be used to power the sensor.
The micro hooks are suitable for this bench test only, not the final wearable.

## Stream format

The firmware sends compact raw records at the configuration-owned serial baud
and configured IMU rate on the J-Link CDC UART:

```text
ICM,seq,t_ms,ax_raw,ay_raw,az_raw,gx_raw,gy_raw,gz_raw,mx_raw,my_raw,mz_raw,temp_raw,mag_st1,mag_st2
```

The live viewer applies these scale factors:

- Accelerometer: +/-2 g, `raw / 16384 * 9.80665` m/s^2
- Gyroscope: +/-250 degrees/s, `raw / 131` degrees/s
- Magnetometer: `raw * 0.15` microtesla
- Temperature: `raw / 333.87 + 21` degrees C

The magnetometer runs at 100 Hz; the complete record is timestamped and
emitted at the configured IMU rate for the dissertation sensor-fusion
contract.
