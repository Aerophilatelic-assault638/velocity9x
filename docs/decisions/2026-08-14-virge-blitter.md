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

## Deliberate scope limits

- No stretching, colour keying, mirroring, or ROPs other than `SRCCOPY` and
  `PATCOPY`. Those caps must stay unadvertised until implemented, because
  advertising a capability makes the driver responsible for completing it.
- Source copies are CPU copies through the linear aperture on both targets.
  The ViRGE BitBLT engine and the Trio32/64 screen-to-screen BitBLT are the
  natural next primitives.
- Not yet exercised against Hellbender or Ironfield RTS; the automated probe
  is the only blit coverage so far.
