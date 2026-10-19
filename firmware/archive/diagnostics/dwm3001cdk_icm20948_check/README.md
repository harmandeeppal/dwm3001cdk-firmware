# DWM3001CDK ICM-20948 identity check

This is the first controlled firmware test for the dissertation hardware. It
uses the DWM3001CDK nRF52833 to read the ICM-20948 `WHO_AM_I` register over the
J10 Raspberry Pi header.

## Wiring contract

| DWM3001CDK connection | nRF52833 | ICM-20948 STEMMA QT |
|---:|---|---|
| J2 current-jumper metal test point, 3.3 V | Supply | VIN, red |
| J10 pin 3, SDA_RPI | P0.26 | SDA, blue |
| J10 pin 5, SCL_RPI | P0.23 | SCL, yellow |
| J10 pin 6, GND | Ground | GND, black |

J10 pins 2 and 4 are `VRPI` power inputs to the DWM3001CDK, not accessory
power outputs. A high-impedance meter can show an unloaded voltage there when
the board is USB-powered, but it collapses under sensor load. Do not power the
ICM-20948 from J10 pin 2 or pin 4.

For the controlled bench test, keep the J2 jumper installed and attach the red
micro test hook to the exposed metal test point on top of the jumper. This
measured 3.26 V on the board that was then named DWM-01-HEAD. That historical
bench observation predates the reassignment of its serial to DWM-05 left
ankle; it must not be treated as a current device identity or deployment
record. Use a mechanically secure regulated 3.3 V
connection for the final wearable assembly; the micro hook is temporary.

The application tests the default I2C address `0x69` and alternate address
`0x68`. A valid ICM-20948 must return `0xEA` from register `0x00`.

The board's default Zephyr I2C pins serve its onboard LIS2DH12. The overlay
disables that node and remaps I2C0 to J10 at 100 kHz for this isolated test.

## Build

Run from an nRF Connect SDK v3.4.0 toolchain shell:

```powershell
west build -p always -b decawave_dwm3001cdk .
```

## Flash and observe

Connect the laptop to J9, then:

```powershell
west flash
```

Open the J-Link CDC UART port at 115200 baud. Success is:

```text
Device at 0x69 returned WHO_AM_I=0xEA.
SUCCESS: ICM-20948 detected at address 0x69.
```

The application also turns on green LED D9 after a successful identity check.
