# Millennium II Candidate 8 86Box activation

Status: accepted guest result  
Target: 86Box 6.0, Matrox Millennium II `102B:051B`, subsystem `2100:102B`

## Result

Build `mga2-640x480x16-modeprep8` passed the first valid guarded activation
against the Millennium II emulator. The guest reached a ready 640x480x16
desktop with `C:\V9XBOOT.INI` reporting `Stage=enable-ok`. The generic GDI
surface probe passed at 640x480x16. After the rollback guard was disarmed, a
second boot reached the same mode, reported `enable-ok`, and passed the GDI
probe again.

The installed `MGAPDX64.DRV` and `MGAPDX64.VXD` compared byte-for-byte with
the staged Candidate 8 package before the confirming boot. The agent remained
available through boot counters 9 and 10.

Evidence is retained under `build/driver-results`:

- `mga2c8-enable-ok-desktop.bmp`;
- `mga2c8-confirm-boot10.bmp`;
- `mga2c8-stock-matrox-16bpp.bmp`;
- `mga2c8-mode-640x480x16.reg`;
- `mga2c8-bind-051b-to-matrox.reg`.

The cold pre-activation VM backup is
`C:\86Box\vms\Velocity9x Backups\Win98SE-MillenniumII-pre-mga2c7-20260811-140651`.
Its VHD, configuration and three NVR files were hash-verified before the first
candidate activation.

## Environment corrections

The first two apparent candidate failures were invalid test configurations:

1. The cloned guest requested 640x480x4 while the guarded candidate exposes
   only 640x480x16. This caused Windows to stop after the GDI information query.
2. The live `102B:051B` device was still associated with the cloned S3
   `Display\0001` class. The stock Matrox `Display\0000` class was attached to
   a ghost `102B:0519` entry. Rebinding the live device to the stock Matrox
   class produced a healthy stock 640x480x16 baseline before the valid test.

The strict `102B:051B` driver match was not weakened.

## Recovery installer correction

The guest's original 186-byte `AUTOEXEC.BAT` ended with 54 NUL bytes. DOS
`FIND` returned success against that binary file even though the guard call was
absent, so the old `PREPARE.BAT` silently failed to install the two-boot guard.

Candidate 8 adds `V9XAUTO.EXE`, a no-runtime Win32 helper that:

- reads only `C:\AUTOEXEC.BAT`;
- treats the first NUL or DOS EOF marker as the logical end;
- performs a case-insensitive idempotence check before that boundary;
- rewrites the file with the guard call and flushes it;
- runs only after `PREPARE.BAT` saves `AUTOEXEC.BAK`, which is restored if the
  helper fails.

The helper converted the exact failing 186-byte input into the expected
166-byte file. Its output was byte-identical to the manually repaired guest
file, and a second preparation remained idempotent.

## Consequence

Candidate 8 clears the emulated Millennium II activation and software-GDI
checkpoint. It does not replace the pending guarded activation on the physical
subsystem `1200:102B` board.
