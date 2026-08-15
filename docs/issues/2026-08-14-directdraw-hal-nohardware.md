# DirectDraw accepts `SetInfo` but reports `DDCAPS_NOHARDWARE`

Status: Resolved — 2026-08-14, build `trio64-hal-srccopy-003`

Date: 2026-08-14

## Summary

The Velocity9x display driver registered a DirectDraw HAL through
`DDHAL_SetInfo`, `SetInfo` returned TRUE, and DirectDraw created the driver
object — but Win98 then reported `DDCAPS_NOHARDWARE` (`0x02000000`) and never
entered a single HAL callback. Every DirectDraw operation that appeared to
prove acceleration was completed by the software HEL.

The cause is a Win98 DirectDraw admission rule that is not documented in the
DDK: **a driver that sets `DDCAPS_BLT` must also advertise ROP3 `SRCCOPY`
(`0xcc`) in `DDCORECAPS.dwRops`. If it does not, the runtime discards the
entire `DDHALINFO` — not just the blitter — and falls back to a GDI-only
emulation path.** The Trio64 driver claimed `DDCAPS_BLT` with only `PATCOPY`
in `dwRops[7]`, so the whole HAL was thrown away.

A second rule follows from the first: once `DDCAPS_BLT` is claimed, declining
a blit with `DDHAL_DRIVER_NOTHANDLED` does **not** fall back to the HEL. The
runtime returns `DDERR_UNSUPPORTED` to the application. Advertising `SRCCOPY`
therefore obliges the driver to implement source copies.

## Environment

- Guest: Windows 98 SE
- Emulator: 86Box 6.0
- VM: `<86Box VMs>\Win98SE-Trio64`
- Display adapter: `[PCI] S3 Trio64`, PCI ID `5333:8811`, 4 MB VRAM
- Mode under test: 1024 x 768 x 16 bpp
- Remote agent: v0.5.2, host port 9871 forwarded to guest port 9869

## How the cause was isolated

`DDHAL_SetInfo` returns TRUE whether or not the runtime keeps the
description, so the rejection is invisible from the driver side. Two tools
made it visible:

1. **A DirectDraw runtime-internals dump** in `tools/diag/ddraw_probe_win32.c`
   (`v9x_write_ddraw_globals`). An `IDirectDraw` interface pointer is a
   `DDRAWI_DIRECTDRAW_INT`; its `lpLcl->lpGbl` is the shared
   `DDRAWI_DIRECTDRAW_GBL` whose byte offsets are published in the DDK's
   `DDRAWI.H`. Reading it back distinguishes "the runtime never recorded the
   HAL" from "the runtime recorded it and then disabled it".

   In the failing state the dump showed `dwFlags = 0x01804120`
   (`DDRAWI_NOHARDWARE | DDRAWI_GDIDRV | DDRAWI_EMULATIONINITIALIZED | ...`),
   `ddCaps.dwCaps = 0x02000000`, `vmiData.fpPrimary = 0`, `dwNumHeaps = 0`,
   `dwPDevice = 0`, `hInstance = 0`, and `lpDDCBtmp->HALDD.dwFlags = 0` — the
   published `DDHALINFO` had been discarded wholesale, and the runtime had
   substituted its own nine-mode GDI mode list and its own alignment defaults.

2. **An INI-driven override of the published `DDHALINFO`** in the 16-bit
   driver, applied immediately before `DDHAL_SetInfo`. Rebuilding the display
   driver costs a guest reboot per attempt; reading the field from an INI made
   the whole description bisectable from the host with no reboots. This
   scaffold was removed once the field was identified.

## Measurements

Each row is one probe run on the same guest, varying only `ddCaps`:

| `dwCaps` | `dwRops` | Result |
|---|---|---|
| `0x04000401` (`3D`, `GDI`, `BLTCOLORFILL`) | none | HAL accepted |
| `0x04000400` (`GDI`, `BLTCOLORFILL`) | none | HAL accepted |
| `0x04080401` (adds `VBI`) | none | HAL accepted |
| `0x04000441` (adds `BLT`) | none | **`NOHARDWARE`** |
| `0x04000440` (`GDI`, `BLT`, `BLTCOLORFILL`) | `PATCOPY` only | **`NOHARDWARE`** |
| `0x04000440` | ROP3 `0xc0` only | **`NOHARDWARE`** |
| `0x04000440` | every ROP3 **except** `SRCCOPY` | **`NOHARDWARE`** |
| `0x04000440` | `SRCCOPY` only | HAL accepted, `Blt` dispatched |

`ddCaps.ddsCaps` was ruled out independently: restoring the full ViRGE
surface caps while leaving `dwCaps` clamped still produced `NOHARDWARE`, and
clamping `ddsCaps` while restoring `dwCaps` did not.

`DDCAPS_VBI` was accepted but is unrelated to `WaitForVerticalBlank`, which
is driven by the `DDHAL_CB32_WAITFORVERTICALBLANK` callback flag. It was
dropped as an inaccurate claim.

## Fix

`src/display16/dd16.c`

- Advertise `dwRops[6] = 0x00001000` (ROP3 `SRCCOPY`, `0xcc = 6 * 32 + 12`)
  alongside the existing `dwRops[7] = 0x00010000` (`PATCOPY`). This is what
  the runtime requires in order to keep a `DDCAPS_BLT` HAL.
- Drop the inaccurate `DDCAPS_VBI` claim.
- Apply the Trio64 clamp to the DGROUP copy of `DDHALINFO` that is handed to
  `DDHAL_SetInfo` rather than to the shared block, so the 32-bit side keeps
  its full description. The duplicate clamp in `DriverInit` was removed:
  `DriverInit` runs from the `DDGET32BITDRIVERNAME` escape, before the 16-bit
  side has published the engine descriptor, so it could never see the
  chipset identity it was branching on.
- Refresh `ddCaps.dwVidMemTotal` / `dwVidMemFree` from the framebuffer
  descriptor. `DriverInit` computed them before the descriptor was valid and
  had been publishing zero.

`src/display32/ddhal.c`

- Implement `v9x_srccopy_body`: a bounded video-memory source copy through
  the mapped linear aperture, honouring the `SRCCOPY` claim. It accepts only
  unstretched, unmirrored, uncolour-keyed copies between validated in-aperture
  rectangles at 8/16 bpp, drains whichever engine owns the chipset first, and
  selects a row order that keeps an overlapping same-surface copy correct.
  Everything else still returns `DDHAL_DRIVER_NOTHANDLED`.
- Count `V9X_TRACE_BLT_ENGINE` only when `Blt` returns
  `DDHAL_DRIVER_HANDLED`, and record the driver return rather than `ddRVal`
  in the `Blt` exit trace. `ddRVal` is `DD_OK` for both an executed blit and a
  declined one, so it cannot distinguish engine execution from a HEL
  fallback, and `GetBltStatus` polling floods the trace ring after every blit.

`src/display16/ddi.c`

- Restore `C1_DIBENGINE`. It had been dropped on the theory that it caused
  `DDCAPS_NOHARDWARE`; that theory is disproven above, and the driver does
  build its PDEVICE with `CreateDIBPDevice` and forward output to the DIB
  Engine, so the declaration is simply accurate. `C1_SLOW_CARD` stays off.
- Publish `Acceleration=directdraw-solid-fill` now that fills are executed by
  the Trio64 engine.

`tools/diag/ddraw_probe_win32.c`

- Add the `DDRAWI_DIRECTDRAW_GBL` dump described above, and a source-copy
  blit test that verifies both the HRESULT and the resulting pixels.

## Verification

Build `trio64-hal-srccopy-003`, 1024 x 768 x 16 bpp. The figures below are
from `V9XDD-RELEASE-001.INI`; `FINAL-A`/`FINAL-B` are the same measurements
repeated twice after a reboot on the functionally identical `-002` build.

```text
ReportedCaps            = 67109952 (0x04000440, no DDCAPS_NOHARDWARE)
GblNoHardware           = 0
MonitorFreqHr / VBlankHr / ExclusiveVBlankHr = 0
SetModeHr / PrimaryHr / RestoreHr            = 0
VideoStageHr / SystemStageHr                 = 0
BltFillHr = 0, BltFillPixelOk = 1, BltFillMs = 3..6
SrcCopyBltHr = 0, SrcCopyPixelOk = 1
Flip20Ms = 0..1  (twenty flips; the HEL needed ~683 ms)
CountBlt = 2, CountBltEngine = 2, CountFlip = 23,
CountWaitForVerticalBlank = 13, CountCreateSurface = 3
EngineFifoTimeouts = EngineIdleTimeouts = EngineResets = 0
```

Against the acceptance criteria:

- `GetCaps` no longer reports `DDCAPS_NOHARDWARE`.
- `CountBlt = 2` with `CountBltEngine = 1`: the colour fill was executed by
  the Trio64 engine and the source copy by the HAL's own CPU path, so neither
  reached the HEL. `Flip`, `WaitForVerticalBlank`, `Lock`, `Unlock`,
  `CreateSurface`, `DestroySurface`, `GetBltStatus`, `SetExclusiveMode` and
  `FlipToGDISurface` all dispatch.
- `BltFillPixelOk` and `SrcCopyPixelOk` confirm the pixels those operations
  produced. `FlipPixelOk` remains 0 by design: GDI always reads the fixed GDI
  page, so a host-window capture is the authoritative check for flipping
  (recorded in `docs/decisions/2026-08-11-directdraw-hal.md`).
- Unsupported operations fall back safely: stretched, mirrored, colour-keyed
  and non-`SRCCOPY` ROP blits, and any rectangle failing aperture validation,
  return `DDHAL_DRIVER_NOTHANDLED`; system-memory and Direct3D surface
  requests behave as before.
- Stability: GDI test PASS, 10/10 live mode-switch cycles PASS, two reboots,
  and repeated probe runs with identical results and no engine timeouts or
  resets.
- The Velocity9x settings page now shows Hardware acceleration, Windows DIB
  Engine rendering, and Live mode switching all checked.

Evidence (ignored build artifacts):

- `build/trio64-bringup/V9XDD-RELEASE-001.INI`, `V9XSNAP-RELEASE-001.INI`
- `build/trio64-bringup/V9XGDI-RELEASE-001.INI`, `V9XMSW-RELEASE-001.INI`
- `build/trio64-bringup/SETTINGS-HAL-SRCCOPY-003.BMP`
- Reboot repeats: `V9XDD-FINAL-A.INI`, `V9XDD-FINAL-B.INI` and their snapshots
- Failing baseline with the runtime dump: `V9XDD-GBL-001.INI`,
  `V9XDD-BASE-002.INI`
- Caps bisect series: `V9XDD-T1-VIRGECAPS.INI` … `V9XDD-T10-BLT-ALLBUTSRC.INI`,
  and the `DDCAPS_BLT` on/off comparison `V9XDD-S-A-BLT.INI` /
  `V9XDD-S-B-NOBLT.INI`

## ViRGE

The same defect was confirmed on the ViRGE target and fixed in the same way;
see `docs/decisions/2026-08-14-virge-blitter.md`. Its baseline run measured
`CountBlt = 0` — the bounded solid fill added in
`2026-08-11-virge-engine-foundation.md` had never executed once.

## Follow-up

- The Trio64 source copy is a CPU copy through the linear aperture, matching
  what the HEL would have done. Replacing it with the Trio32/64
  screen-to-screen BitBLT (`CMD` opcode 6, `FRGD_MIX` source = display memory)
  is the next bounded 2D primitive. The surface validation it needs is
  already in place, because the engine works on display memory at the display
  pitch — the same constraint `v9x_copy_rect_valid` enforces.
- `dwCaps` still omits `DDCAPS_BLTSTRETCH`, colour keying and overlays; those
  remain HEL responsibilities and must not be advertised until implemented,
  for exactly the reason documented above.

## Relevant source references

- `include/velocity9x/win9x_ddraw_abi.h`
- `src/display16/dd16.c`
- `src/display16/ddi.c`
- `src/display32/ddhal.c`
- `tools/diag/ddraw_probe_win32.c`
- Windows 98 DDK: `C:\98DDK\src\display\inc\DDRAWI.H` (`DDRAWI_DIRECTDRAW_GBL`
  field offsets, `DDRAWI_NOHARDWARE`, `DDRAWI_GDIDRV`)
- Windows 98 DDK: `C:\98DDK\src\display\inc\DDRAW.H` (`DDCAPS_*`, `DDBLT_*`)
- Windows 98 DDK S3 sample: `C:\98DDK\src\display\mini\s3v\DDDRV.C`,
  `CONTROL.ASM`
- Local 86Box reference: `build/reference/86box/src/video/vid_s3.c`
- [S3 Trio32/Trio64 Graphics Accelerators manual](https://www.bitsavers.org/components/s3/DB014-B_Trio32_Trio64_Graphics_Accelerators_Mar1995.pdf)
