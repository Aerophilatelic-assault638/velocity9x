# Millennium II Candidate 8 physical activation

Status: rejected physical result; stock restored  
Target: physical Matrox Millennium II `102B:051B`, subsystem `1200:102B`

## Result

Build `mga2-640x480x16-modeprep8` passed its inactive query and reached a
ready 640x480x16 desktop on the physical MGA-2164W at boot counter 19.
`C:\V9XBOOT.INI` reported `Stage=enable-ok`, and the active files had the
expected Candidate 8 sizes: 9,726-byte `MGAPDX64.DRV` and 5,324-byte
`MGAPDX64.VXD`.

The physical framebuffer was not usable. The first desktop capture already
showed repeated and striped regions across the screen. The independent GDI
probe exited with code 3 and recorded:

```
[Velocity9xGDI]
Result=FAIL
Build=powerfix4-test
Width=640
Height=480
BitsPerPixel=16
```

Candidate 8 is therefore rejected on physical hardware. Its two consecutive
86Box GDI passes remain useful emulator results, but they do not clear the
physical surface-write checkpoint.

## Recovery

The rollback guard was deliberately left armed after the failure. The
prescribed second reboot restored the stock Matrox files before Windows loaded
at boot counter 20. The machine returned to a ready, visually clean
1024x768x24 desktop and `LASTBOOT.TXT` reported `ROLLED-BACK`. Neither
`ARMED.1` nor `ARMED.2` remained.

The restored files matched the pre-test physical files byte-for-byte:

- `MGAPDX64.DRV`: SHA-256
  `1BB171D0C32C6825B539486FA4BD58FA6ABBC686B2FFC21789C100C23BBC9E56`;
- `MGAPDX64.VXD`: SHA-256
  `8B765ABF772AE25C3B7BDF979CA6A9ABEE0366A636DEC5487F786961C56C15EC`.

Evidence is retained under `build/physical-mga-candidate8`:

- `candidate8-desktop.bmp` (corrupt Candidate 8 framebuffer);
- `stock-restored-desktop.bmp` (clean stock desktop after rollback);
- the pre-test, guard-copy and post-rollback stock driver pairs.

## Consequence

The NUL-safe guard installer worked correctly on the physical system, and the
strict PCI match, guarded copy path, mode entry and driver enable path remain
valid. Candidate 8 does not fix the real MGA-2164W framebuffer/surface-write
failure. Candidate 9 subsequently isolated the failure to replacement of the
stock Matrox mini-VDD. Preserving that component cleared the physical
framebuffer and GDI checkpoint; see
`docs/decisions/2026-08-11-millennium2-physical-candidate9.md`.
