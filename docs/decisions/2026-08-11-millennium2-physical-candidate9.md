# Millennium II Candidate 9 physical activation

Status: accepted physical result  
Build: `mga2-640x480x16-stockvxd9`  
Target: physical Matrox Millennium II `102B:051B`, subsystem `1200:102B`

## Change

Candidate 9 isolates the component boundary exposed by Candidate 8. It installs
the Velocity9x `MGAPDX64.DRV` but preserves the target machine's validated
stock Matrox `MGAPDX64.VXD`. The package contains `KEEPVXD.TAG`, not a copy of
the proprietary mini-VDD. During activation the two-boot guard stages its own
saved stock VXD beside the candidate DRV.

This is a material hardware requirement. Every earlier physical candidate
replaced both files with Velocity9x components. The generic Velocity9x
mini-VDD lacks the MGA-2164W-specific early hardware/VDD setup that the real
board requires; 86Box did not expose that dependency.

## Qualification

The mixed candidate-DRV/stock-VXD pair first passed in the Millennium II 86Box
guest at boot counter 12:

- ready 640x480x16 desktop;
- `Stage=enable-ok`;
- active stock Matrox VXD size 51,059 bytes;
- generic GDI probe `Result=PASS`.

On the physical machine, the inactive query passed and the guard staged:

- Candidate 9 `MGAPDX64.DRV`: 9,726 bytes;
- physical stock `MGAPDX64.VXD`: 75,704 bytes.

Boot counter 21 reached a clean, ready 640x480x16 desktop with
`Stage=enable-ok`. The generic GDI probe passed and the post-test framebuffer
remained visually correct. The guard was disarmed, then re-armed from the
intact candidate and stock backups for a confirmation boot. Boot counter 22
again reached a clean 640x480x16 desktop and passed the same GDI probe. The
guard was disarmed after the second pass.

The installed pair was verified byte-for-byte:

- Candidate 9 DRV SHA-256:
  `DB83DBC54D477E5FD757D809FF0C73B8BDCD8DCAD92FF2B256BE8508FBB090B2`;
- preserved stock VXD SHA-256:
  `8B765ABF772AE25C3B7BDF979CA6A9ABEE0366A636DEC5487F786961C56C15EC`.

Evidence is retained under `build/physical-mga-candidate8`:

- `candidate9-vm-desktop.bmp` and `candidate9-vm-gdi.bmp`;
- `candidate9-physical-desktop.bmp`;
- `candidate9-physical-gdi-pass.bmp`;
- `candidate9-physical-confirm.bmp`;
- `C9-INSTALLED-MGAPDX64.DRV` and `C9-INSTALLED-MGAPDX64.VXD`.

## Recovery correction

Qualification also found that copying a loaded Win98 display DRV from
`RESTORE.BAT` can report a sharing violation without a reliable DOS errorlevel.
`RESTORE.BAT` now creates the second-boot marker and defers both stock copies to
`V9XGUARD.BAT` at DOS time, before Windows loads either file.

## Consequence

Candidate 9 clears the first repeatable physical desktop and software-GDI
checkpoint for the MGA-2164W. The current accepted installation boundary is a
Velocity9x 16-bit display DRV paired with the stock Matrox mini-VDD. Replacing
the mini-VDD remains a separate future milestone and must not be bundled into
mode or rendering work.
