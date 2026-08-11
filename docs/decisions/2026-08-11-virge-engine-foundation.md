# ViRGE engine foundation (engine-1)

Date: 2026-08-11

Target: Windows 98SE, 86Box S3 ViRGE/DX 86C375

## Scope

The flat DirectDraw HAL now has the bounded graphics-engine plumbing needed
before Direct3D command submission can be added:

- the cross-bitness shared block describes the 64-MiB PCI BAR mapping
  separately from the 4-MiB allocatable VRAM heap;
- MMIO reads and writes use the 64-KiB ViRGE new-MMIO window at BAR + 16 MiB;
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
`DDCAPS_BLTSTRETCH`. Engine-1 corrects the constant and removes the accidental
stretch-blit claim. The HAL advertises `DDCAPS_BLTCOLORFILL`, but deliberately
does not advertise the broader `DDCAPS_BLT`: Win98 requires a generic Blt
driver to publish supported ROPs, while this milestone implements only the
independent colour-fill operation.

## Guest validation and recovery

The first activation exposed two independent integration defects. The DPMI
mapping still covered only 4 MiB, and the MMIO base was treated as BAR + 0
instead of BAR + 16 MiB. Consequently, engine offsets addressed visible VRAM
and corrupted the display. The VM was stopped and its active VHD restored
from the cold pre-activation backup; the restored SHA-256 matched the backup
manifest exactly. Subsequent builds first used a read-only `/status-only`
probe and a one-time status plausibility gate before permitting any engine
write.

Build `engine-6-full-bar`, boot counter 91, passed in the Win98SE guest:

- HAL active: `MonitorFreqHr=0`, `MonitorFreq=60`, `VBlankHr=0`;
- new-MMIO status accepted: `BltCanHr=0`;
- hardware fill: `BltFillHr=0`, `BltFillDoneHr=0`,
  `BltFillPixelOk=1`;
- presentation regression: `VideoStageHr=0`, `Flip20Ms=1`;
- GDI framebuffer regression: `Result=PASS` at 800x600x16 when launched
  through the normal shell path.

The DirectDraw probe's `FlipPixelOk=0` remains expected for the existing
fixed-GDI-page readback, as documented in the DirectDraw HAL milestone.
Timeout and reset counters remain in the shared block for debugger inspection.
