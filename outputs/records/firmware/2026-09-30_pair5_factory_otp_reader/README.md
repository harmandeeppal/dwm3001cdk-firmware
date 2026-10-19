# Pair-5 factory OTP reader — 30 September 2026

## Purpose

These are **temporary diagnostic images** for the current PELVIS--LEFT-WRIST
pair-5 test. They read each board's factory UWB antenna-delay word and report
it through the existing BLE diagnostic packet. They do not change a board's
OTP, flash configuration, antenna delay, UWB distance calculation, IMU
sampling, or BLE transport.

This is the safe next check because the pair-5 v3 timing change made ranging
reliable but the measured distance still had a large, non-constant error.

## What the reader reads

The active UWB profile is channel 5, 6.8 Mb/s, with PRF 64. Qorvo's official
OTP revision-1 map defines address `0x1A` as the four-byte channel-5,
PRF-64 antenna-delay word:

| Bits | Meaning |
|---|---|
| `[31:16]` | receive (RX) antenna delay |
| `[15:0]` | transmit (TX) antenna delay |

The firmware calls the read-only driver API `dwt_otpread(0x1A, ...)` once at
start-up. It then reports the unchanged raw word every 10 seconds as
diagnostic stage `0x50`. It **never** calls an OTP-write API and it does not
apply the read values. The 10-second interval allows the viewer's 20-second
old-data drain to finish before fresh evidence arrives.

Sources: Qorvo's [DWM3001CDK OTP guidance](https://forum.qorvo.com/t/how-to-reduce-distance-d-cm-error/20404)
and its linked OTP revision-1 map, plus the local `dwt_otpread` compatibility
implementation in `lib/dwt_uwb_driver/deca_compat.c`.

## Images

| Board | Registered serial | File | SHA-256 |
|---|---:|---|---|
| PELVIS | 760224787 | `DWM-02-PELVIS__pair5_factory_otp_reader__760224787.hex` | `FDBADB04C57A9A339BBA577FA8A86010812B564B6D4BB76E7A572264E8FE31E8` |
| LEFT-WRIST | 760225868 | `DWM-03-LEFT-WRIST__pair5_factory_otp_reader__760225868.hex` | `5F8A0592151D34DC7E0C049BF8DCA2E3AAA7165A41AA16B8AF38D2BCD242008D` |

Both images were built successfully with NCS v3.4.0 for `nrf52833dk/nrf52833`.
They retain the pair-5-only v3 configuration:

- PELVIS node index 1; LEFT-WRIST node index 2.
- Pair 5 only (`PELVIS <-> LEFT-WRIST`).
- PELVIS final delay 6 ms; LEFT-WRIST response delay 5 ms.
- UWB TX-status diagnostics enabled.
- `COMP971_FACTORY_OTP_REPORT` enabled only in these two images.

## Install and observe procedure

1. Before writing either image, make a fresh full-flash and UICR backup of
   the currently installed v3 pair-only image for that named board.
2. Install the PELVIS image only while registered PELVIS `760224787` is
   connected through J9. Verify the image after writing.
3. Install the LEFT-WRIST image only while registered LEFT-WRIST `760225868`
   is connected through J9. Verify the image after writing.
4. Put both boards on battery and start the unified viewer. After its 20-second
   drain, wait up to 15 seconds for the `Factory UWB calibration readout`
   table. Record the raw words and the RX/TX delays.
5. Compare the two factory values with the currently compiled fallback delay
   `16390`. Do not install those values or alter OTP until the comparison and
   a written calibration decision are reviewed.

The normal full-mesh firmware is not changed by this record. These images are
not a fleet release and must be replaced before returning to six-node testing.

## Install record

PELVIS `760224787` was connected through J9 and selected explicitly by its
registered J-Link serial. Before writing, a fresh duplicate-matched full-flash
and UICR backup was completed in
`hardware/backups/DWM3001CDK_760224787/2026-09-30_pre_factory_otp_reader_backup/`.

SEGGER J-Link Commander v9.72 then programmed
`DWM-02-PELVIS__pair5_factory_otp_reader__760224787.hex`. Its programming
report states `Program & Verify` completed successfully for three changed
flash ranges (135,168 bytes), followed by reset and run. OTP was not written.
The exact programming script is `program_pelvis_factory_otp_reader.jlink`.

LEFT-WRIST `760225868` was then connected through J9 and selected explicitly
by its registered J-Link serial. Its fresh duplicate-matched backup is in
`hardware/backups/DWM3001CDK_760225868/2026-09-30_pre_factory_otp_reader_backup/`.
SEGGER programmed
`DWM-03-LEFT-WRIST__pair5_factory_otp_reader__760225868.hex` and reported
`Program & Verify` success for three changed flash ranges (135,168 bytes),
followed by reset and run. OTP was not written. The exact script is
`program_left_wrist_factory_otp_reader.jlink`.

Both reader images are now installed. The next step is to run the unified
viewer with both boards on battery and record the two reported factory words.

## PELVIS default-delay restore

After the temporary runtime application of the OTP value failed the controlled
100 cm accuracy test, PELVIS `760224787` was restored to this reader image on
30 September 2026. A new full-flash and UICR backup was read twice first; both
copies matched exactly. Its flash backup hash is
`EC20AD66A3A9B0C4E612950EC0F6061C983CB568C15CF68972297E390D0462F2` and its
UICR hash is
`B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1`.

SEGGER J-Link Commander v9.24a then restored
`DWM-02-PELVIS__pair5_factory_otp_reader__760224787.hex` and reported
`Program & Verify` success for one changed 8 KiB flash range, followed by
reset/run. This returns PELVIS to default RX/TX antenna delay `16390`; it does
not write OTP. LEFT-WRIST is not changed by this PELVIS restore.

LEFT-WRIST `760225868` was restored next, after a new duplicate-matched
full-flash and UICR backup. Its flash backup hash is
`28C0ECA5C0BC2666EAF91D283DACDE65261E9FC6127B6BB51EA78B4949027855` and its
UICR hash is
`B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1`.
J-Link v9.24a restored
`DWM-03-LEFT-WRIST__pair5_factory_otp_reader__760225868.hex`, reported
`Program & Verify` success for one changed 4 KiB flash range, and reset/run.
It now also uses default RX/TX antenna delay `16390`; it does not write OTP.
