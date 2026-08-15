# Hellbender renders in software despite accepting the Direct3D HAL

Status: Open — investigation

Date: 2026-08-15

## Summary

Hellbender now runs to gameplay on the Velocity9x driver, but it renders
entirely in software. It enumerates the Velocity9x Direct3D HAL device and
reads its capabilities — the fog warning proves that — and then creates no
context on it. Every Direct3D callback counter stays at the probe's own
baseline for the whole session:

```text
CountD3dContextCreate   = 1   # from V9XDDP, not the game
CountD3dRenderState     = 9   # ditto
CountD3dRenderPrimitive = 7   # ditto
CountD3dTextureCreate   = 2   # ditto
```

The same is true of DirectDraw: the game presents through `Lock`/`Unlock` and
`Flip` and never calls `Blt`.

## What the game asks for

`HELLBEND.EXE` contains only the DirectX 2/3-era interface GUIDs:

| GUID | interface |
|---|---|
| `3BBA0080-2421-11CF-A31A-00AA00B93356` | `IID_IDirect3D` (v1) |
| `4417C142-33AD-11CF-816F-0000C020156E` | `IID_IDirect3DExecuteBuffer` |
| `4417C144-33AD-11CF-816F-0000C020156E` | `IID_IDirect3DViewport` (v1) |
| `2CDCD9E0-25A0-11CF-A31A-00AA00B93356` | `IID_IDirect3DTexture` (v1) |

There is no `IDirect3D2`/`IDirect3D3`, no `IDirect3DDevice2`/`Device3`, and no
`DrawPrimitive`. It renders exclusively through execute buffers, and it
carries no hardcoded device GUID, so it selects a device from
`IDirect3D::EnumDevices`.

## Execute buffers are not the blocker

The Windows 98 DDK's own ViRGE Direct3D driver leaves the execute callbacks
null and says so explicitly:

```c
// Execution
NULL,                       // Optional.  Don't implement if just rasterization.
NULL,
myRenderState,              // Required if no Execute
myRenderPrimitive,          // Required if no Execute
```

The runtime parses the execute buffer and decomposes it into `RenderState`
and `RenderPrimitive` calls; the driver never sees the buffer. This driver has
exactly that shape, and the probe pixel-verifies that path
(`D3DTrianglePixelOk=1`). The earlier `D3DHalFound=0` results from publishing
`Execute` — recorded in section 4.4 of the 2026-08-13 handoff — were a dead
end, not a missing feature.

## Capability bits tested and rejected

Each was published correctly, verified in the probe's enumerated device caps,
and left the HAL accepted (`D3DHalFound=1`, `D3DCreateDeviceHr=0`,
`D3DTrianglePixelOk=1`). None caused Hellbender to create a context.

| claim | device caps before/after | result |
|---|---|---|
| `D3DDEVCAPS_EXECUTESYSTEMMEMORY` | `0x2441` to `0x2451` | no change |
| `D3DDEVCAPS_TEXTUREVIDEOMEMORY` | `0x2451` to `0x2651` | no change |
| `D3DDD_LINECAPS` + populated `dpcLineCaps` | flags `451` to `483` | no change |

`EXECUTESYSTEMMEMORY` is kept: the runtime satisfies it through
`RenderState`/`RenderPrimitive`, which this driver implements and the probe
verifies, and the DDK sample sets the same bit while leaving `Execute` null.

`TEXTUREVIDEOMEMORY` and the line caps were reverted. Texture sampling and
line rasterisation are not implemented, the claims bought nothing, and
advertising a capability the driver cannot honour is the same mistake that
produced the `DDCAPS_NOHARDWARE` and `DDERR_UNSUPPORTED` failures recorded in
`2026-08-14-directdraw-hal-nohardware.md`.

**The blocker is therefore not a missing capability bit in the device
description.**

## Leading hypothesis: the v1 device-creation path is untested

V9XDDP creates its Direct3D device through `IDirect3D2::CreateDevice`, after
`QueryInterface(IID_IDirect3D2)`. That path works: the probe creates a
context, renders a pixel-verified triangle, and cycles the context cleanly.

Hellbender cannot use that path — it has no `IDirect3D2` GUID. A v1
application creates its device by calling `QueryInterface` for the enumerated
device GUID **on the render-target surface**, which is a different code path
through the runtime and one this driver has never been tested against.

That is the largest untested difference between the working probe and the
failing game, and it is consistent with everything observed: enumeration
succeeds and caps are read (fog warning), but no context is ever created.

## Next investigation steps

1. Extend V9XDDP with a v1 device-creation path: `QueryInterface(IID_IDirect3D)`,
   `EnumDevices` to obtain the HAL device GUID, then `QueryInterface` for that
   GUID on the render-target surface. If that fails where the `IDirect3D2`
   path succeeds, the failure is reproduced in a two-minute probe instead of a
   ten-minute game run, and the HRESULT names the reason.
2. If v1 creation succeeds in the probe, the difference is in the game's
   selection logic rather than the runtime, and the next lever is the
   enumeration order and the device description the game compares against the
   software devices it also enumerates.
3. Only once a context is created on the HAL does texture sampling become the
   relevant problem.

## Related

- [Hellbender Direct3D review and test handoff](../handoffs/2026-08-13-hellbender-d3d-review.md)
- [Hellbender hardware Direct3D compatibility plan](../plans/hellbender-hardware-d3d.md)
- [DIBENG fault that previously stopped the game reaching gameplay](2026-08-14-hellbender-dibeng-gpf.md)

Evidence under `build/driver-results/hellbender-bitblt/`: `EXECCAPS-*.INI`,
`TEXCAPS-*.INI`, and `HELLBENDER-GAMEPLAY.INI`.
