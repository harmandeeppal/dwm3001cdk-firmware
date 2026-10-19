# DWM-05-LEFT-ANKLE combined UWB + ICM firmware

- Device: `DWM-05-LEFT-ANKLE`
- J-Link serial: `760225018`
- Role: UWB initiator
- Peer: `DWM-02-PELVIS` (`760224787`), responder
- ICM-20948: I2C address `0x69`, 60 Hz raw samples
- UWB delays: TX `16378`, RX `16378`, responder delay `1000 us`

`DWM-05-LEFT-ANKLE__uwb-initiator_icm-60hz__760225018.hex` was programmed and verified on 2026-09-26.

Live proof after flashing: 599 clean ICM frames in a 10 second capture (effectively 60 Hz), with no corrupt frames. PELVIS was off during this check, so no UWB range frames were expected.

This uses the recorded development UWB delays. The paired UWB distance must still be revalidated once PELVIS is restored and powered.
