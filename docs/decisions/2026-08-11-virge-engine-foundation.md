# ViRGE engine foundation (engine-1)

Date: 2026-08-11

Target: Windows 98SE, 86Box S3 ViRGE/DX 86C375

## Scope

The flat DirectDraw HAL now has the bounded graphics-engine plumbing needed
before Direct3D command submission can be added:

- the cross-bitness shared block describes the existing 64-MiB DPMI mapping
  separately from the 4-MiB allocatable VRAM heap;
- MMIO reads and writes use the ViRGE register window in that mapping;
- FIFO-space and engine-idle waits have finite spin limits;
- a timeout records a diagnostic counter and toggles CR66 bit 1 to reset the
  ViRGE/DX graphics engine;
- CPU surface locks and page flips serialize with outstanding engine work;
- DirectDraw advertises and implements hardware solid-colour `Blt`, with
  `GetBltStatus` reporting FIFO availability and completion;
- the guest DirectDraw probe records `BltFillHr`, `BltFillDoneHr`,
  `BltFillMs`, and a locked-surface `BltFillPixelOk` readback independently
  of its raw CPU-fill timing.

The implementation follows the register and FIFO contracts documented by the
Windows 98 DDK S3 ViRGE sample, but is original Velocity9x code. Capability
flags remain deliberately narrow: no screen copy, stretch, colour key, alpha,
overlay, or Direct3D support is advertised.

## Important correction

The project-owned ABI header previously assigned `DDCAPS_GDI` the value
`0x00000200`. The Windows 98 value is `0x00000400`; `0x00000200` is
`DDCAPS_BLTSTRETCH`. Engine-1 corrects the constant, removing the accidental
stretch-blit claim while adding the explicit `DDCAPS_BLT` and
`DDCAPS_BLTCOLORFILL` claims implemented by the HAL.

## Validation state

The host suite, 16-bit DDI build, import-free fixed-base HAL build, and
runtime-free DirectDraw probe build pass. Hardware execution remains
quarantined until a new 86Box candidate runs the updated probe and confirms
the fill colour, completion status, and mode-switch regression. Timeout and
reset counters remain in the shared block for debugger inspection.
