# Trace8b Phase 3 mode-matrix result

Date: 2026-08-09

Build: `trace8b-mode-matrix-dibret`

Target: Windows 98SE, 86Box S3 ViRGE/DX 86C375, 4 MiB VRAM

## Outcome

The registry-selected display driver passed every advertised Phase 3 mode.
Each row represents a separate normal Windows reboot followed by an
Explorer-ready check, `C:\V9XBOOT.INI` verification, and the unattended GDI
framebuffer test.

| Mode | Boot | Driver trace | GDI result |
| --- | ---: | --- | --- |
| 640x480x8 | 27 | `enable-ok` | PASS |
| 800x600x8 | 28 | `enable-ok` | PASS |
| 1024x768x8 | 23 | `enable-ok` | PASS |
| 640x480x16 | 24 | `enable-ok` | PASS |
| 800x600x16 | 25 | `enable-ok` | PASS |
| 1024x768x16 | 26 | `enable-ok` | PASS |

The unattended test checks display writes, BitBlt, StretchBlt, SetPixel, and
tolerant GetPixel readback. It writes `Result`, `Build`, `Width`, `Height`, and
`BitsPerPixel` to `C:\V9XGDI.INI`, then exits without user input.

## Failure found and fixed

The first 1024x768x8 attempt reached the hardware mode and framebuffer mapping
but failed at `CreateDIBPDevice`, causing Windows to fall back to 640x480x4.
The DIB Engine API returns its 32-bit result in `EAX`, an exception to Open
Watcom's Win16 `DWORD` convention of `DX:AX`. The previous transparent thunk
therefore interpreted a valid return using a stale `DX` value.

The corrected thunk removes its own far return address before calling DIBENG,
restores it afterward, and converts `EAX` to `DX:AX`. The build audit now
requires that conversion sequence. With this fix, 1024x768x8 and all 16-bit
modes passed.

## Evidence

Screenshots and mode-specific registry inputs are under
`build/driver-results/trace8-mode-matrix`. The final matching DRV/mini-VDD pair
passed again at the 800x600x8 baseline on boot 29. The reusable matrix runner's
own end-to-end smoke run passed the same baseline on boot 31; the VM was left
running there.

This result proves reboot-selected fixed modes and software framebuffer GDI.
Palette animation and two complete repeat passes were subsequently proven by
the trace8c diagnostics run. Live mode switching, DOS full-screen transitions,
and the larger reliability counts required by the final Phase 3 gate remain.
