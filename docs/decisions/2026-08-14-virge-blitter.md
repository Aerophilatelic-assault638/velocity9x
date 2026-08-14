# ViRGE DirectDraw blitter (build `virge-blt-003`)

Date: 2026-08-14

Target: Windows 98SE, 86Box S3 ViRGE/DX 86C375, 4 MiB VRAM

## Decision

Advertise `DDCAPS_BLT` on the ViRGE target so the DirectDraw runtime actually
dispatches the `Blt` callback, and accept the obligations that come with it.

The ViRGE HAL published `DDCAPS_BLTCOLORFILL` without `DDCAPS_BLT`. That
combination is inert: the runtime never dispatches `Blt` at all. The guest
baseline confirmed it — `CountBlt = 0` — so the bounded solid-colour fill
added in `2026-08-11-virge-engine-foundation.md` had never executed once, and
every DirectDraw blit on this driver was a HEL copy.

The admission rule and its consequences were established on the Trio64 target
first; see
[docs/issues/2026-08-14-directdraw-hal-nohardware.md](../issues/2026-08-14-directdraw-hal-nohardware.md).

## Mechanism

- `DriverInit` publishes `DDCAPS_BLT | DDCAPS_BLTCOLORFILL` plus
  `dwRops[6] = SRCCOPY` and `dwRops[7] = PATCOPY`. The `SRCCOPY` entry is not
  optional: without it Win98 discards the entire `DDHALINFO` and reports
  `DDCAPS_NOHARDWARE`. The Trio64 clamp in `dd16.c` now only removes the
  Direct3D bit and inherits the blitter caps and ROP table from here, so there
  is one source of truth.
- Every blit the driver admits is completed by the driver. `DDHAL_DRIVER_NOTHANDLED`
  is reported to the application as `DDERR_UNSUPPORTED` rather than being
  emulated, so `v9x_colorfill_body` falls back to a CPU fill through the
  mapped aperture when the engine declines a shape, and `v9x_srccopy_body`
  serves source copies. Both engine paths (`v9x_trio_fill`, `v9x_virge_fill`)
  now return an outcome — done, busy, or declined — instead of refusing the
  callback.
- Engine-status validation happens on the blit path. It used to be latched
  only by `GetBltStatus(DDGBS_CANBLT)` and the Direct3D draw callbacks, so an
  application that blits without polling could never reach the engine. That
  was harmless while `DDCAPS_BLT` was unset and would have been a correctness
  bug the moment it was set. The check is now a single
  `v9x_engine_validate_status` helper used by all five call sites, and it
  re-samples the status register a bounded number of times: the validated bit
  is cleared on every Enable/ReEnable, and a single unlucky sample right after
  a mode set otherwise sent that mode's first fill down the CPU fallback.

## Fixed along the way

`V9X_DD_ENGINE_STATUS_VALIDATED` and `V9X_DD_ENGINE_S3_TRIO64` were both
`0x00000004`. Validating the ViRGE engine status therefore set the Trio64
identity bit, which made `v9x_trio_engine_ready()` true on a ViRGE. With the
blitter enabled that would have issued the Trio64 port-I/O rectangle-fill
command sequence to ViRGE hardware. `STATUS_VALIDATED` moved to `0x00000008`.

## Guest results (boots 160-164)

Baseline before the change, then after, on the same guest:

| | before | after |
|---|---|---|
| `CountBlt` | 0 | 3 per probe run |
| `CountBltEngine` | 0 | 1 per probe run |
| `BltFillMs` | 16 | 1 |
| `GetCaps` `dwCaps` | `0x04000401` | `0x04000441` |

- `GblNoHardware = 0` throughout: the HAL is still accepted with the blitter
  claimed.
- `D3DHalFound = 1`, four Direct3D devices enumerated, and the Direct3D
  callback counters are identical to the baseline (1 context create/destroy,
  9 render states, 7 render primitives, 2 texture creates/destroys, 1 swap).
  Enabling the blitter did not disturb the Direct3D path.
- `BltFillPixelOk = 1` and `SrcCopyPixelOk = 1` on every run.
- Stretched and colour-keyed blits still return `S_OK`. The trace ring shows
  they are served by the HEL through `Lock`/`Unlock` and never reach the
  driver's `Blt`, because `DDCAPS_BLTSTRETCH` and `DDCAPS_COLORKEY` are not
  advertised. This is the safe-fallback behaviour the driver depends on for
  everything it does not implement.
- Three consecutive runs produced identical results (`BltFillMs` 1, 1, 3),
  confirming the bounded re-sample made acceleration deterministic.
- GDI test PASS, 10/10 live mode-switch cycles, reboot clean, no engine FIFO
  or idle timeouts and no engine resets in any run.

The Trio64 target was rebuilt and re-verified against the same shared
`V9XHAL.DLL` changes: `CountBlt = 2` with `CountBltEngine = 1` (engine fill
plus CPU source copy), correct pixels, GDI PASS, 10/10 mode cycles.

## Ironfield RTS, both targets

The first real DirectDraw workload to reach the blitter. Fullscreen
`-benchmark` runs at 640x480x16, 15 s each, three presentation paths:

| renderer | what it does | ViRGE | Trio64 | HAL `Blt` |
|---|---|---|---|---|
| Direct backbuffer | renders straight into the VRAM backbuffer | 19 FPS | 16 FPS | not used |
| System RAM | renders to system memory, HEL copies system to VRAM | 27 FPS | 23 FPS | not used |
| Video + BltFast | renders to a VRAM stage, `BltFast` copies VRAM to VRAM | 3 FPS | 3 FPS | one per frame |

Findings:

- `BltFast` is dispatched to the driver's `Blt` callback, one per frame. This
  path previously failed outright — the game's cached auto-renderer score for
  it was `4294967295` — so it is newly functional rather than newly slow.
- The other two paths never reach the driver's blitter, which is correct:
  `dwSVBCaps` is zero, so system-to-video blits stay with the HEL, and the
  direct path does not blit at all. Neither regressed.
- 3 FPS is the CPU source copy, not the game. A 640x480x16 frame is 614400
  bytes and the copy is VRAM to VRAM, so every byte crosses the aperture
  twice. The HEL's system-to-video copy in the System RAM path costs about
  10 ms per frame; ours costs about 300 ms. Reading back out of video memory
  is the expensive half, and it is exactly what a screen-to-screen BitBLT
  exists to avoid.
- Widening the CPU copy from bytes to dwords took it from 1 FPS to 3 FPS
  (25 to 53 frames), which confirms the loop itself was also part of the
  cost, but the aperture round trip dominates.
- A host-side capture of a live fullscreen frame shows correct rendering:
  HUD, sector map, command panel, terrain and water all clean, no smearing or
  tearing.
- Six fullscreen runs across both guests completed without a hang, GPF,
  engine FIFO or idle timeout, or engine reset.

The conclusion is that the ViRGE screen-to-screen BitBLT is no longer an
optimisation but the missing piece: `BltFast` presentation is the standard
way DirectDraw games present, and it cannot be served acceptably by the CPU.
That engine path was implemented next, below.

## Screen-to-screen BitBLT (build `trio64-bitblt-001`)

Both engines now execute source copies, with the CPU copy left as the
fallback for shapes they cannot express.

- **ViRGE.** Command 0 of the S3D 2D unit with ROP3 `SRCCOPY`, neither the
  mono-source nor image-data-source bit set so the source is read from
  display memory. It has per-surface base and stride registers
  (`0xa4d4`/`0xa4d8`, and `0xa4e4` carrying destination stride in the high
  word and source stride in the low word), so it can copy between surfaces of
  different pitches. Writing the command register with autoexecute clear is
  what starts the blit.
- **Trio64.** Opcode 6 of the 8514/A-compatible enhanced command set, with
  `FRGD_MIX` `0x0067` selecting a display-memory source and the SRC mix. This
  engine has no per-surface base or stride: it walks display memory as one
  surface at the display pitch from a common bank base, so a surface's
  position is folded into its y coordinate and both rectangles must sit on
  display-pitch scan lines. Anything else is declined to the CPU copy.
- Overlap is handled by scan direction rather than row order on both engines.
  The direction bits are derived from the rectangles, starting from the
  bottom row for a downward shift and the right column for a rightward one.

Ironfield `BltFast` at 640x480, same 15 s run:

| | byte copy | dword copy | engine BitBLT |
|---|---|---|---|
| ViRGE | 1 FPS | 3 FPS | **18 FPS** |
| Trio64 | - | 3 FPS | **16 FPS** |

Every frame's blit was engine-executed (`CountBltEngine` equal to `CountBlt`,
304 and 275 respectively) with no FIFO or idle timeout and no engine reset.
`BltFast` presentation is now level with the direct-backbuffer path (19 FPS
ViRGE, 16 FPS Trio64) rather than six times slower than it.

The probe gained a second overlap check on a display-pitch surface. The
original check used an offscreen surface with its own pitch, which only the
ViRGE engine can address, so the Trio64 engine copy had no pixel-verified
coverage at all — it silently fell back to the CPU for every probe blit.
Both checks now pass on both targets, and the Trio64 run shows the expected
split: display-pitch copies on the engine, small-pitch copies declined to the
CPU.

## Deliberate scope limits

- No stretching, colour keying, mirroring, or ROPs other than `SRCCOPY` and
  `PATCOPY`. Those caps must stay unadvertised until implemented, because
  advertising a capability makes the driver responsible for completing it.
- The Trio64 engine copy only serves display-pitch surfaces on scan-line
  boundaries, which is what its engine can address. Offscreen surfaces with
  their own pitch still take the CPU copy on that target.
- Not yet exercised against Hellbender, so the Direct3D milestone has only
  probe-level coverage of the blitter change.
- Colour fills still go through the mono-pattern path rather than the
  engine's rectangle-fill source, and stretching, colour keying and other
  ROPs remain unadvertised and therefore HEL work.
