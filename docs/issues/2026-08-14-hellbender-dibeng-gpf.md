# Hellbender faults in `DIBENG.DLL` leaving the intro cinematic

Status: Resolved — 2026-08-15, build `selector-stable-001`

Date: 2026-08-14

## Summary

Hellbender now gets substantially further than the state recorded in
`docs/handoffs/2026-08-13-hellbender-d3d-review.md`. It accepts the Velocity9x
Direct3D HAL, reaches Quick Configuration, starts a new game, switches to
640 x 480 x 16 and plays the Terminal Reality intro cinematic correctly. The
previously recorded hard wedge — a black client frame immediately after New
Game with no further progress — no longer reproduces.

Leaving the intro it faulted, and that fault is the subject of this issue:

```text
HELLBEND caused a general protection fault
in module DIBENG.DLL at 0001:00000932.
EAX=000004a0 CS=0357 EIP=00000932 EFLGS=00000212
EBX=00000020 SS=3037 ESP=00008506 EBP=00008518
ECX=00000060 DS=03df ESI=00001980 FS=203f
EDX=00000022 ES=2077 EDI=0001f030 GS=05bf
```

The fault is in the Windows DIB Engine, not in `V9XHAL.DLL`. This is the same
module implicated in the pre-existing fault class recorded in section 4.1 of
the handoff.

It was caused by the driver freeing and reallocating its framebuffer selector
across a `Disable`/`Enable` cycle while the DIB Engine still held the old
value. With the selector made stable, Hellbender reaches gameplay.

## Environment

- Windows 98 SE, 86Box 6.0
- ViRGE VM (host port 9869), driver build `virge-bitblt-001`, boot 167,
  `useDirect3D=1`
- Trio64 VM (host port 9871), driver build `trio64-bitblt-001`, boot 43,
  `useDirect3D=0` — that target does not advertise a Direct3D HAL
- Hellbender installed at `C:\Program Files\Microsoft Games\Hellbender`

## Observations

### ViRGE

1. The 3D Adapter Warning appears — "your 3D adapter will not allow you to see
   fog … but the game will still run". The HAL passed Hellbender's capability
   filter; only fog is refused.
2. Quick Configuration renders correctly.
3. New Game switches the display to 640 x 480 x 16 and the intro cinematic
   plays with correct colour and no corruption.
4. Leaving the intro, the GPF above appears. `C:\V9XTRACE.INI` was not
   created, so the HAL's unhandled-exception filter did not fire.
5. The trace ring's final entries are three consecutive
   `Dd16CreateObject enter 0x00000001` / `exit 0x00000001` pairs — Enable or
   ReEnable refreshing the HAL across rapid mode changes. No HAL callback was
   in flight.
6. The agent stayed responsive. Closing the dialog and setting the mode back
   to 1024 x 768 x 16 restored a working desktop with no reboot.

### Trio64

1. No adapter warning, as expected with `useDirect3D=0`.
2. New Game switches to 640 x 480 x 16 and the intro plays correctly.
3. Left completely untouched, the guest hard-wedged during the intro: a
   partially restored desktop with a striped band, no dialog, and an
   unresponsive agent. The emulator had to be restarted.
4. After the cold boot there was no `C:\V9XTRACE.INI`, and `C:\V9XBOOT.INI`
   reported `enable-ok`.

The Trio64 run received no injected input at all, so the wedge is not caused
by the keystrokes used to skip the intro on the ViRGE. Whether the two
failures share a root cause is not yet established: one is a caught user-mode
GPF in `DIBENG.DLL` and the other is a hard wedge with no dialog.

## What this rules out

- **The blitter work is not implicated.** `CountBlt` did not move at all
  during either Hellbender run — 7 before and after on the ViRGE, 6 on the
  Trio64, all from the preceding probe. Hellbender presents through
  `Lock`/`Unlock` and `Flip`, never through `Blt`, so the DirectDraw blitter
  and both engine BitBLT paths were never exercised by the game.
- **The engines were healthy.** `EngineFifoTimeouts`, `EngineIdleTimeouts`
  and `EngineResets` were zero in every snapshot on both targets.
- **The driver survives the failure.** After the ViRGE GPF, with no reboot,
  the full probe still passed: `GblNoHardware=0`, `D3DHalFound=1`,
  `D3DTrianglePixelOk=1`, `D3DContextCycleOk=1`, all fill, source-copy and
  overlap pixel checks, and a GDI PASS. The Trio64 was equally healthy after
  its cold boot.
- **No new Direct3D work reached the driver.** The Direct3D counters were
  unchanged from the probe baseline throughout — one context create/destroy,
  nine render states, seven render primitives, two texture create/destroys,
  one swap. Hellbender never created its own device context, so the fault is
  still ahead of any Direct3D rasterisation the driver would perform.

## Where the fault is

The faulting instruction decodes from the reported bytes as
`rep movsb` with a 32-bit address-size override and an FS source override:
96 bytes (`ECX`) from `FS:ESI` = `203f:00001980` to `ES:EDI` =
`2077:0001f030`.

Mapping `0001:00000932` through DIBENG.DLL's NE entry table places it in the
cursor region of segment 1, between the exported `DIB_MOVECURSOREXT`
(`0001:0245`) and `DIB_BEGINACCESS` (`0001:0f60`), with no exported symbol in
between. It is an internal cursor helper — the offsets and the 96-byte block
copy are consistent with the save-under buffer.

## Tested and rejected: cursor activity during the PDEVICE rebuild

The live mode-switch branch of `ReEnable` rebuilds the PDEVICE in place and
clears `DE_BUSY` afterwards without ever setting it first, unlike the
`Disable`-driven branch. Since `CreateDIBPDevice` replaces the DIB Engine's
cursor bookkeeping inside that same PDEVICE, the obvious theory was that
cursor activity during the rebuild window explained the fault.

`V9XMSW` gained a `/cursor` switch that moves the pointer and forces it to be
removed and redrawn immediately before and after every mode change, driven
from inside the guest process because the remote agent serialises its
connections and injected pointer input cannot overlap a mode switch issued
through the same agent.

The result does not support the theory: 12/12 cycles PASS with cursor stress
on both targets, no fault, no wedge. That path is now covered by a permanent
regression test, but it is not this bug. **Do not implement a `DE_BUSY` or
cursor-exclusion change on the strength of the fault location alone.**

## Root cause

The framebuffer selector was not stable across a `Disable`/`Enable` cycle.

Publishing the live selector through the HAL trace made this measurable.
`ES = 0x2077` in the fault is exactly the value the driver reported for its
own framebuffer selector. Driving Hellbender while sampling the trace then
caught the transition: `DisableCount` went from 0 to 1 and `ScreenSelector`
changed from `0x00002077` to `0x00003167`.

`V9xHardwareDisable` freed the LDT descriptor with DPMI 0001h and zeroed it,
so the next `Enable` allocated a different one. The DIB Engine caches the
selector inside the PDEVICE it builds and does not reacquire it, so after the
cycle it was writing through a descriptor that had been returned to the LDT
and could since belong to anything with any limit. That is why an offset of
`0x1f030` faulted even though the driver's own selector is mapped over the
full 64 MiB BAR: the descriptor being used was no longer the driver's.

This also explains why the mode-cycle repro could not reproduce it.
`ReEnable` takes `V9xHardwareEnable`'s reuse path and never frees the
selector — `DisableCount` stayed at 0 across every cycle test.

## Fix

`V9XHARDWAREDISABLE` in `src/display16/runtime.asm` now returns the adapter to
VGA text mode and keeps both the LDT descriptor and the linear mapping. One
selector serves the driver's lifetime. `Enable` already reuses a live selector
and re-enters the VBE mode and re-enables the linear aperture before reaching
that path, so nothing else had to change.

## Verification

Build `selector-stable-001` on both targets.

- Across a real `Disable` during Hellbender — `DisableCount` 0 to 1 — the
  selector held at `0x00002077`, where the same sequence previously moved it
  to `0x00003167`.
- **Hellbender reaches gameplay.** It plays the intro, leaves it without
  faulting, and renders the cockpit, HUD, radar, terrain and mission text.
  Both the black-frame hard wedge from the handoff and the `DIBENG.DLL` GPF
  are gone.
- Full probe passes on both targets after a reboot: `GblNoHardware=0`,
  ViRGE `D3DHalFound=1` and `D3DTrianglePixelOk=1`, and every fill,
  source-copy and overlap pixel check.
- GDI PASS and 10/10 live mode-switch cycles with cursor stress on both.

Hellbender's own rendering does not reach the Velocity9x HAL: the DirectDraw
and Direct3D callback counters were unchanged from the probe baseline
throughout gameplay. It presents through the software path, which is
consistent with the fault having been in `DIBENG.DLL`. Reaching gameplay is a
milestone for driver stability, **not** evidence that the hardware Direct3D
path is being exercised.

## Fixed along the way

`V9XMSW` lost the tail of its results file. Windows caches profile writes, and
a mode change immediately before process exit discarded everything written
after the last `ChangeDisplaySettings`: a run reported nine of ten cycles and
no verdict while still exiting zero. It now flushes with a null section before
exiting. This was a reporting defect in the test, not a driver regression —
the exit code, which is computed from the completed count, was correct — but
it briefly looked like one.

## Related

- [Hellbender Direct3D review and test handoff](../handoffs/2026-08-13-hellbender-d3d-review.md)
- [Hellbender hardware Direct3D compatibility plan](../plans/hellbender-hardware-d3d.md)
- [ViRGE DirectDraw blitter](../decisions/2026-08-14-virge-blitter.md)

Evidence (ignored build artifacts) under
`build/driver-results/hellbender-bitblt/`: `VIRGE-PRE-NEWGAME.INI`,
`VIRGE-INTRO.INI`, `VIRGE-GAME.INI`, `VIRGE-GPF.INI`, `VIRGE-POSTGPF.INI`,
`TRIO-PRE-NEWGAME.INI`, `TRIO-INTRO.INI`, `TRIO-POSTWEDGE.INI`.
