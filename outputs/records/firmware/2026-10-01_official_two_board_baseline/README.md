# Official two-board UCI baseline procedure

## Purpose

This controlled test separates the custom COMP971 ranging implementation from
the DWM3001CDK hardware and Qorvo's official low-level configuration.

It applies only to these two named boards:

| Role for this temporary test | Body label | J-Link serial |
|---|---|---:|
| UCI controller | PELVIS | `760224787` |
| UCI controlee | LEFT-WRIST | `760225868` |

No other wearable board or the nRF52840 gateway is involved.

## Exact official assets prepared

| Item | Location / result |
|---|---|
| Official UCI image selected for the baseline | `C:\COMP971_QORVO_SDK_1_1_1\OfficialBinaries\DWM3001CDK-UCI-FreeRTOS_QM33SDK-1.1.1_prebuilt.hex` |
| Image source | Qorvo's `DW3_QM33_SDK_1.1.1.zip`, `SDK/Binaries/DWM3001CDK/` |
| Image SHA-256 | `D79BFCE49136D9AAFFB3514E942BB045C76544A0630D20AF63DA1345BFA50A34` |
| Official UCI host tools | `C:\COMP971_QORVO_TOOLS_1_1_1` |
| UCI host runtime | isolated `C:\QorvoPython310` (Python 3.10.11) |
| UCI tool verification | `run_fira_twr`, `get_device_info`, and `get_cal` installed and their help screens verified |

The separately compiled official UCI image also built successfully, but its
binary hash differs from the packaged prebuilt image. The packaged prebuilt
Qorvo image above is selected for this baseline because it is the exact image
distributed with this SDK release.

## Non-negotiable safety rules

1. Make a new full flash, UICR, and FICR readback **immediately before**
   writing each named board. Read flash and UICR twice and compare hashes.
2. Identify the board by J-Link serial, never by COM port or physical position.
3. Do not use **Erase Chip**, `load_cal`, `set_cal`, `reset_calibration`, or
   any OTP programming command in this baseline.
4. Do not connect the nRF52840 gateway. Use **J9 only for J-Link flashing**.
   For the official UCI host tools, move the micro-USB cable to **J20 (nRF
   USB)**. The official UCI build uses USB CDC by default; its UART path is
   disabled by default.
5. Keep the new backup until the official test is complete and its fresh
   custom-firmware restoration has been verified.

## Test sequence

### A. Fresh backup and official UCI flash

Perform PELVIS first, then LEFT-WRIST. For each board:

1. Connect **only that named board** through J9.
2. Read full flash (`0x00000000`, 512 KiB), UICR (`0x10001000`, 4 KiB), and
   FICR identity (`0x10000000`, 4 KiB). Repeat the flash and UICR readback.
3. Confirm the two flash hashes match and the two UICR hashes match.
4. Program the selected official UCI `.hex`, using the J-Link serial shown
   above. No chip erase is allowed.
5. Record J-Link's Program and Verify result.

### B. Read-only official configuration check

After both boards run official UCI, connect **both J20 USB ports to the laptop**
(a powered USB hub is fine). Note each newly appeared USB COM port. Run these
read-only calls for each port:

```powershell
C:\QorvoPython310\Scripts\get_device_info.exe -p COMx
C:\QorvoPython310\Scripts\get_cal.exe -p COMx ant0.ch5.ant_delay ant1.ch5.ant_delay
C:\QorvoPython310\Scripts\get_cal.exe -p COMx ant0.ch9.ant_delay ant1.ch9.ant_delay
```

These calls retrieve data only. They do not change calibration.

### C. Official default ranging baseline

Place the two boards at a carefully measured **2.00 m** separation. Keep the
boards still, facing the same way, with their normal cases and battery setup.
That is important: this test measures the real wearable assembly, not an
unrepresentative bare-board setup.

Open two PowerShell windows. Use PELVIS as controller and LEFT-WRIST as
controlee. Replace `COM_PELVIS` and `COM_LEFT_WRIST` with the observed ports.

```powershell
# PELVIS - controller; Qorvo UCI defaults, including channel 9
C:\QorvoPython310\Scripts\run_fira_twr.exe -p COM_PELVIS -t 60 --stats --diag_dump

# LEFT-WRIST - controlee
C:\QorvoPython310\Scripts\run_fira_twr.exe -p COM_LEFT_WRIST -t 60 --controlee
```

The controller output reports the individual distance results and the final
statistics. `--diag_dump` saves the official diagnostic JSON. This first test
intentionally uses Qorvo's default session rather than forcing the COMP971
channel-5 settings. It answers the primary question: do these two assembled
boards range correctly under Qorvo's complete normal configuration?

### D. Optional channel-5 comparison

Only after the default baseline is recorded, run the same 60-second test on
channel 5. Both commands must use the same channel and preamble index:

```powershell
# PELVIS
C:\QorvoPython310\Scripts\run_fira_twr.exe -p COM_PELVIS -t 60 -c 5 --preamble-idx 9 --stats --diag_dump

# LEFT-WRIST
C:\QorvoPython310\Scripts\run_fira_twr.exe -p COM_LEFT_WRIST -t 60 -c 5 --preamble-idx 9 --controlee
```

This is a session setting, not a persistent calibration write. It provides a
closer comparison with the current COMP971 channel-5 mesh but does not replace
the default official baseline.

### E. Restore the two boards

Restore each board from the fresh readback made in step A, not from an older
image. Verify the restore. This returns both boards to exactly the firmware and
non-volatile state they had immediately before the official test.

## Decision after the baseline

- If the official default result is stable and close to 2.00 m, the main issue
  is in the current custom UWB implementation or its configuration. Continue
  with a staged port onto the official SDK.
- If the official default result is also inaccurate or unstable, investigate
  the physical assembly, antenna surroundings, board revision, and Qorvo's
  official calibration flow before changing custom firmware again.

No calibration value is written merely because this baseline is run.

## Actual progress - 1 October 2026

### PELVIS official-UCI installation

PELVIS (`760224787`) was selected by its J-Link serial number. Immediately
before writing, two full 512 KiB flash readbacks and two UICR readbacks were
made and compared. The matching SHA-256 values were:

| Readback | SHA-256 |
|---|---|
| Flash, both reads | `4E5EC80EEC845EBF8F7BD673171A2406D78600A2E05A2CA7C0858AC4476D6AB4` |
| UICR, both reads | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` |
| FICR identity read | `199C840A813AAC04F351683AE427EED55E5AE5D79DA7E84F8C46AED9BA8A893B` |

The official packaged UCI image was then programmed and independently verified
by J-Link. No chip erase, OTP write, or UCI calibration write was used.

### LEFT-WRIST official-UCI installation

LEFT-WRIST (`760225868`) was independently confirmed through `ShowEmuList`
before its backup. The immediate pre-write backups matched:

| Readback | SHA-256 |
|---|---|
| Flash, both reads | `3ED67B323828EA63714ADAAB731E8FEC127C4CB2525C4D239D44594657CC0B50` |
| UICR, both reads | `B380A6231DD01E4FD5C1CF7B316948ED7164D6D05E4358DE1CFEB69A5FB131C1` |
| FICR identity read | `33999BA266D84B2AF2B2D0D85C9BA56D92FFA6B5A1605EA20F00A99D57300362` |

J-Link then programmed the same official packaged UCI image used on PELVIS.
It reported `Program & Verify: O.K.` after affecting 262,144 bytes of normal
flash. No chip erase, OTP write, or UCI calibration write was used. The next
step is read-only J20 verification.

### Host-port diagnosis

While the cable was in **J9**, `COM6` appeared as the J-Link virtual UART port.
It opened but did not answer the UCI request, which is expected for the default
UCI configuration. `COM15` and `COM16` are a separate Nordic USB device and
are not the PELVIS UCI port. The next action is to move the existing PELVIS USB
cable from J9 to **J20**, then run `get_device_info` on the new J20 COM port.

### PELVIS J20 verification

After the cable was moved to J20, Windows enumerated PELVIS as `COM17`
(`USB\\VID_1915&PID_520F`). The official read-only `get_device_info` request
succeeded and reported UCI/MAC/PHY version `2.0.0`, UCI test and QMF version
`1.1.0`, and device ID `deca0302`.

The following official `get_cal` requests were also successful and read only:

| PELVIS stored setting | Value |
|---|---:|
| `ant0.ch5.ant_delay` / `ant1.ch5.ant_delay` | `16368` |
| `ant0.ch9.ant_delay` / `ant1.ch9.ant_delay` | `16375` |
| `xtal_trim` | `28` |

No calibration value was written or applied by this check.

### LEFT-WRIST J20 verification

After LEFT-WRIST was moved from J9 to J20, Windows enumerated it as `COM18`
(`USB\\VID_1915&PID_520F`). The official read-only `get_device_info` request
succeeded and reported UCI/MAC/PHY version `2.0.0`, UCI test and QMF version
`1.1.0`, and device ID `deca0302`.

| LEFT-WRIST stored setting | Value |
|---|---:|
| `ant0.ch5.ant_delay` / `ant1.ch5.ant_delay` | `16372` |
| `ant0.ch9.ant_delay` / `ant1.ch9.ant_delay` | `16376` |
| `xtal_trim` | `25` |

No calibration value was written or applied by this check. Both boards are now
prepared for the first official default-ranging baseline.

### Official default-ranging baseline at stated 100 cm

With both boards on J20 and connected to the official host tools, PELVIS ran
the controller on `COM17` and LEFT-WRIST ran the responder on `COM18`. This was
the Qorvo default session: channel 9, preamble index 10, deferred double-sided
TWR, 200 ms ranging interval. The board separation was stated by the operator
as 100 cm.

| Result | Value |
|---|---:|
| Controller range reports | 301 |
| Successful / total | 301 / 301 (100.0%) |
| All-60-second median / mean / sample SD | 107 / 108.02 / 4.30 cm |
| After first 20 seconds: median / mean / sample SD | 108 / 107.51 / 1.82 cm |
| After first 20 seconds: minimum / maximum | 99 / 113 cm |

The early portion contained a few high readings (maximum 145 cm); the later
40 seconds were much tighter. This run demonstrates a stable official radio
exchange on the assembled boards, but it does not authorize an offset or other
calibration change. The reported range is antenna-to-antenna; the operator's
100 cm reference may be measured from a different point on the enclosures.
The retained controller log and Qorvo diagnostic JSON are in
`official_default_100cm_20261001_173832/`.

### Official channel-5 comparison at the same stated 100 cm

Without moving either board, the same official UCI image was used for a second
60-second session. Both sessions set only temporary UCI session parameters;
nothing was written to calibration storage. This comparison used channel 5 and
preamble index 9.

| Result | Value |
|---|---:|
| Controller reports | 301 |
| Valid / invalid | 300 / 1 |
| Valid-range median / mean / sample SD | 130 / 130.97 / 2.59 cm |
| After first 20 seconds: median / mean / sample SD | 131 / 131.19 / 2.69 cm |
| After first 20 seconds: minimum / maximum | 126 / 140 cm |

The one invalid report was emitted as the `65535 cm` sentinel and was excluded
from all valid-range statistics. This result is stable but approximately 31 cm
longer than the stated 100 cm reference. In contrast, Qorvo's channel-9 default
session was stable at approximately 108 cm. The result proves that the boards
can exchange UWB ranges with the official firmware, but channel 5 must not be
treated as an accepted accurate configuration for this assembled pair.

The controller log and official diagnostic JSON are retained in
`official_channel5_100cm_20261001_1741/`.

### Official channel-9 check at stated 75 cm

With the same boards, J20 ports, orientation, and official default channel-9
session, the operator changed only the stated separation from 100 cm to 75 cm.

| Result | Value |
|---|---:|
| Controller reports | 301 |
| Valid / invalid | 300 / 1 |
| Valid-range median / mean / sample SD | 74 / 74.04 / 2.19 cm |
| After first 20 seconds: median / mean / sample SD | 74 / 74.23 / 2.16 cm |
| After first 20 seconds: minimum / maximum | 69 / 79 cm |
| Mean difference from stated 75 cm | -0.77 cm |

The one invalid report was the `65535 cm` sentinel and was excluded from valid
statistics. This is a stable, close result at 75 cm. The retained controller
log and Qorvo diagnostic JSON are in `official_default_75cm_20261001_174528/`.

### Official channel-9 check at stated 50 cm

With the same two boards, J20 ports, orientation, and official default
channel-9 session, the operator changed only the stated separation from 75 cm
to 50 cm.

| Result | Value |
|---|---:|
| Controller reports | 301 |
| Valid / invalid | 301 / 0 |
| Valid-range median / mean / sample SD | 52 / 52.29 / 2.03 cm |
| After first 20 seconds: median / mean / sample SD | 52 / 52.11 / 2.09 cm |
| After first 20 seconds: minimum / maximum | 47 / 58 cm |
| Mean difference from stated 50 cm | +2.11 cm |

This is a stable, close result at 50 cm. The retained controller log and Qorvo
diagnostic JSON are in `official_default_50cm_20261001_1748/`.

### Interim baseline conclusion

The official channel-9 sessions produced stable results at all three stated
distances: 52.11 cm at 50 cm, 74.23 cm at 75 cm, and 107.51 cm at 100 cm
(post-settling means). This establishes that the PELVIS and LEFT-WRIST radio
hardware, UCI transport, and official default channel-9 configuration work
reliably. It does **not** establish a one-number calibration correction: the
observed bias differs across the three positions. A repeat at 100 cm is the
next evidence needed before deciding whether that difference is repeatable or
comes from the physical distance reference/layout.

### Official channel-9 repeat at stated 100 cm

The operator returned the same boards to the same stated 100 cm separation and
ran one more unchanged official default channel-9 session.

| Result | Value |
|---|---:|
| Controller reports | 301 |
| Valid / invalid | 301 / 0 |
| Valid-range median / mean / sample SD | 106 / 106.08 / 1.49 cm |
| After first 20 seconds: median / mean / sample SD | 107 / 106.18 / 1.58 cm |
| After first 20 seconds: minimum / maximum | 101 / 110 cm |
| Mean difference from stated 100 cm | +6.18 cm |

The repeat is consistent with the first post-settling 100 cm result (107.51
cm mean). The 100 cm difference is therefore repeatable in this physical
layout, but is not a reason to write a fixed calibration value: the 50 cm and
75 cm biases differ.

After all sessions, `get_cal` was run again on both boards. The stored values
were unchanged from their pre-test reads:

| Setting | PELVIS | LEFT-WRIST |
|---|---:|---:|
| Channel-5 antenna delay | 16368 | 16372 |
| Channel-9 antenna delay | 16375 | 16376 |
| Crystal trim | 28 | 25 |

This verifies that all ranging tests changed temporary session settings only;
they did not write calibration or OTP. The retained controller log and Qorvo
diagnostic JSON are in `official_default_100cm_repeat_20261001_1751/`.

## Baseline decision

The PELVIS--LEFT-WRIST official-UCI baseline is **complete** for functionality
and repeatability at 50, 75, and 100 cm. The assembled hardware exchanged
ranges reliably on Qorvo's default channel 9 (1,203 valid results of 1,204
reported attempts across the four channel-9 runs). It is therefore not
supported by this evidence that an unreliable radio transaction or the two
boards themselves caused the earlier custom-firmware failures.

Channel 5 is explicitly **not accepted** for this pair: its official result at
the unchanged stated 100 cm position was stable but approximately 31 cm long.
The custom project must not simply retain its earlier fixed channel-5
configuration.

No calibration write is approved from this evidence. Before an official
calibration decision, measure from the UWB antenna reference point and run the
vendor-recommended known-distance procedure (approximately 2 m). That needs
two simultaneous J20 data connections, for example by placing the laptop or a
data USB hub between the boards. Until then, retain the current read-only
evidence and use channel 9 for the first official-SDK-based COMP971 port.
