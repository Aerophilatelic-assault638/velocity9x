# DirectDraw HAL (builds ddhal-1/ddhal-2)

Date: 2026-08-11

Target: Windows 98SE, 86Box S3 ViRGE/DX 86C375, 4 MiB VRAM

## Summary

Velocity9x now exposes a DirectDraw HAL: video-memory surfaces allocated by
the DirectDraw runtime from a real VRAM heap, page flipping by programming
the S3 CRTC display-start registers, and real vertical-blank services from
the emulated timing. The design is 32-bit-first: all DirectDraw content and
every runtime callback live in the new flat `V9XHAL.DLL`
(`src/display32/ddhal.c`); the 16-bit driver contains only ABI glue
(`src/display16/dd16.c`) — the DCICOMMAND escape dispatch, the DPMI
allocation of the cross-bitness shared block, 16:16 pointer stamping, and
the `lpDDHAL_SetInfo` call, all of which the Windows 9x ABI requires to be
16-bit. No HAL blits, palettes, overlays, or Direct3D are advertised; the
HEL provides those over the HAL's video-memory surfaces.

Architecture follows the MIT vmdisp9x/vmhal9x pair; the 98DDK s3v sample
was used as documentation for the ViRGE display-start register layout. All
code is original; DirectDraw structure layouts live in the project-owned
`include/velocity9x/win9x_ddraw_abi.h` with size guards compiled by both
wcc and wcc386.

## Mechanism

- `Control` (ordinal 3) moved from a pure DIBENG forward to C: answers
  `QUERYESCSUPPORT(DCICOMMAND)` with the HAL version and dispatches
  `DDCREATEDRIVEROBJECT` / `DDGET32BITDRIVERNAME` / `DDNEWCALLBACKFNS` /
  `DDVERSIONINFO`; everything else still forwards to `DIB_Control`.
- The shared `V9X_DD_SHARED` block is DPMI-allocated (globally visible
  linear memory + one LDT alias descriptor, `V9XDDSHAREDALLOC` in
  runtime.asm); its linear address is the `dwContext` DDRAW passes to
  `V9XHAL.DLL!DriverInit` in every DirectDraw process.
- `V9XHAL.DLL` is linked at fixed base 0xB0400000 inside the Win9x shared
  arena with every PE section marked `IMAGE_SCN_MEM_SHARED` by a post-link
  patcher in `scripts/build-ddraw-hal-dll.ps1`; it has zero imports and no
  CRT. Verified in-guest: it loads at exactly 0xB0400000.
- `DriverInit` builds all DDHALINFO content in place: the six-mode table,
  minimal caps (`DDCAPS_GDI` only; surface caps
  `OFFSCREENPLAIN|FLIP|PRIMARYSURFACE`), one `VIDMEM_ISLINEAR` heap from
  the end of the visible screen to the 4 MiB aperture end, and the
  callback tables (32-bit `Flip`, `GetFlipStatus`, `Lock`/`Unlock`
  pass-throughs, `WaitForVerticalBlank`, `FlipToGDISurface`).
- The 16-bit `V9xDdCreateDriverObject(bReset)` runs at the tail of every
  successful `v9x_build_pdevice` (boot Enable and live ReEnable), refreshes
  the framebuffer descriptor and mode-dependent fields, and calls SetInfo -
  so mode switches keep the HAL current.
- `Flip` programs the display start in doubleword units: CR0D = bits 7:0,
  CR0C = bits 15:8, CR69 low nibble = bits 19:16 (high nibble preserved).
  Ring-3 port I/O; the VDD registration already stopped VGA trapping.
  `WaitForVerticalBlank` polls 3DA bit 3 with bounded spins.
  `FlipToGDISurface` returns the scanout to page 0 on exclusive-mode exit
  (added in ddhal-2 after the first guest round).

## Guest results (boots 69-70)

- Boot regression clean (`enable-ok`); desktop, GDI test, palette test,
  and 10/10 live mode-switch cycles unaffected.
- V9XDDP: `MonitorFreqHr=0` with `MonitorFreq=60` (was E_NOTIMPL),
  `VBlankHr=0` with 10 vblank waits = 166 ms (exactly 60 Hz),
  **`VideoStageHr=0x00000000`** (video-memory surfaces exist), and
  **`Flip20Ms=1`** - twenty flips in one millisecond, versus ~700 ms of
  HEL copies before the HAL.
- Ground-truth flip verification: V9XDDP `/hold` holds each verification
  color on screen; host-side captures of the 86Box window show the full
  blue page, a mid-latch frame (red above the scanline, blue below - the
  signature of a real scanout flip), and a clean desktop afterwards.
- The probe's `FlipPixelOk` GDI readback is not meaningful under real
  flipping: GDI always reads the fixed GDI page, so it reports 0 even when
  the flip is correct. The host-window capture is the authoritative check.

## Deliberate scope limits

Update: engine-1 later added the narrowly scoped, bounded solid-colour `Blt`
path described in `2026-08-11-virge-engine-foundation.md`. The original
ddhal-1/ddhal-2 state below is retained as the milestone record.

- No HAL `Blt`: caps deliberately do not claim `DDCAPS_BLT`; the HEL
  performs all blits (CPU) on HAL video-memory surfaces. The ViRGE BitBLT
  engine is the natural next acceleration step.
- No DirectDraw palette callbacks (HEL/GDI handle palettes; vmdisp9x
  precedent), no overlays, no Direct3D.
- VRAM size is the driver's existing fixed 4 MiB assumption; CR36 sizing
  remains future work.
- Surface invalidation across mode changes relies on DDRAW's surface-loss
  mechanics, as in both reference implementations.
