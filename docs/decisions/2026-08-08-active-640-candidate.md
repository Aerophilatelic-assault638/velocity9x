# Active 640x480x8 bring-up candidate

Status: host-audited; guest activation pending cold backup  
Recorded: 2026-08-08

## Decision

Advance from inactive lifecycle probes to one recoverable active display-driver
candidate. The first activation is limited to Windows 98SE on the S3 ViRGE/DX
86C375 (`PCI\VEN_5333&DEV_8A01`) at 640x480, 8 bpp, 60 Hz.

No acceleration, high-color mode, DDC, hardware cursor, DirectDraw HAL, or
extended mode switching is advertised. Rendering and the software cursor are
delegated to the Windows DIB Engine.

## Implementation boundary

The Win16 DRV now:

- checks for the exact PCI vendor/device through the read-only PCI BIOS find
  function before changing video state;
- requests VBE mode `0x101` with the linear-framebuffer flag;
- reads and validates the S3 CR59/CR5A aperture base;
- maps a four-megabyte aperture through DPMI and binds it to one LDT selector;
- constructs the 640x480x8 `GDIINFO`, bitmap information, palette, and DIB
  Engine PDevice;
- forwards software rendering, palette translation, and cursor operations to
  DIBENG using PDevice-aware thunks;
- registers its fixed reset callback and visible framebuffer size with the
  Windows master VDD, records the post-mode state, and unregisters before VGA
  trapping resumes during disable or failed activation;
- programs only the VGA palette DAC after mode entry;
- releases the physical mapping and selector on disable;
- emits bounded COM1 checkpoints before and during activation.

The mini-VDD verifies that the master VDD dispatch table is present, logs its
build identity, and deliberately installs zero callbacks. Unfilled entries keep
the master VDD's default behavior and avoid callback lifetime hazards.

## Package and recovery

`scripts/build-active-package.ps1` produces a quarantined package under
`build/win98se-active`. Its INF is restricted to the exact PCI ID and exposes
only 640x480x8 plus the standard-VGA fallback entry.

The package contains explicit install and recovery instructions and a read-only
Windows 98 settings/status utility. Its adjacent GDI smoke test exercises the
screen-driver fill, line, text, blit, pixel-write, and pixel-read paths before
reporting a tolerant readback result. `scripts/backup-86box-profile.ps1`
refuses to run while any 86Box process exists and copies only the active
configured VHD, configuration, and NVR directory with a SHA-256 manifest.

## Remaining gate

The package is not accepted as a working driver until all of these occur:

1. 86Box and its manager are fully closed.
2. A cold backup of `Win98HDD.vhd`, `86box.cfg`, and `nvr` completes and hashes.
3. COM1 capture is armed before the first boot.
4. The INF is installed once and the VM receives one cold boot attempt.
5. Serial logs show mini-VDD initialization and DRV `enable-ok`.
6. The desktop renders, the software cursor moves, and standard-VGA rollback is
   demonstrated before the backup is considered replaceable.

## First guest activation evidence

Recorded: 2026-08-09

The cold-backed-up `active-640-vdd1` package passed its first activation boot.
Windows selected the Velocity9x S3 ViRGE/DX adapter and listed both
`C:\WINDOWS\SYSTEM\V9XDISP.DRV` and `V9XMINI.VXD` with the system VDD/VFlatD.
The 640x480x8 desktop rendered normally.

`V9XGDI.EXE` reported PASS after exercising display writes, solid palette
colors, lines, text, BitBlt, StretchBlt, SetPixel, and tolerant GetPixel
readback. The submitted visual evidence has SHA-256
`00392A7B8CA8A21519F9006B42E3FE9F87D7D639EACDC9E781A5D099348E6392`.

The first-boot serial capture contains the mini-VDD initialization and
zero-callback success checkpoints. It does not contain the Win16 DRV's direct
UART checkpoints even though the active desktop and GDI readback prove that
the DRV path ran. Treat reliable Win16 logging after VCOMM ownership as a
separate diagnostics defect; it is not evidence of an activation failure. The
93-byte capture has SHA-256
`B8B0D11006BA65D216474C877F2FA5AF2CEA03A700B6E28312C16F1BAC27BAB0`.

The software cursor moved across the test pattern without trails, corruption,
or disappearance, and the guest then completed a clean full shutdown.

Still pending: a repeat cold boot and demonstrated standard-VGA recovery.
