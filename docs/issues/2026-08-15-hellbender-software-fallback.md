# Hellbender renders in software despite accepting the Direct3D HAL

Status: Closed — 2026-08-15. Not a driver defect: Hellbender does not use
hardware Direct3D on this chipset even with the stock S3 ViRGE driver.

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

## Tested and rejected: the v1 device-creation path

V9XDDP creates its device through `IDirect3D2::CreateDevice`. A v1 application
cannot — it holds only `IID_IDirect3D` and creates the device by calling
`QueryInterface` for the enumerated device GUID **on the render-target
surface**. That path had never been exercised, and it was the largest
untested difference between the working probe and the failing game.

The probe now exercises it on its own render target, and it works:

```text
D3DV1InterfaceHr = 0x00000000    QueryInterface(IID_IDirect3D)
D3DV1TargetHr    = 0x00000000    3DDEVICE video-memory surface
D3DV1DeviceHr    = 0x00000000    QueryInterface(IID_IDirect3DHALDevice) on it
D3DV1DeviceOk    = 1
```

So the runtime will hand a DirectX 2/3-era application a hardware device on
this HAL. The rejection is in Hellbender's own device-selection logic, not in
the runtime and not in device creation.

## What the enumerated device actually reports

```text
TexFormatCount          = 1        only one texture format
TexFormat565            = 0        not RGB565
TexFormat1555           = 1        ARGB1555 only
D3DDevice2HwRenderDepth = 1024     DDBD_16
D3DDevice2HwZDepth      = 1024     DDBD_16
D3DDevice2HwMaxBuffer   = 0
D3DDevice2HwMaxVertices = 1024
```

The single texture format is the most suspicious remaining entry. The display
mode is RGB565 and the only advertised texture format is ARGB1555, so a
textured title has one format to choose from and it does not match the frame
buffer. The DDK ViRGE sample publishes more than one.

## Reference comparison: the stock S3 ViRGE driver

The reference VM on host port 9870 runs the retail S3 ViRGE driver on the same
emulated chip. Its Direct3D HAL is enumerated as "Microsoft Direct3D Hardware
acceleration through Direct3D HAL" and is materially richer than ours:

| HAL device field | ours | stock S3 | ours is missing |
|---|---|---|---|
| `dwFlags` | `0x1C3` | `0x1E3` | `D3DDD_LINECAPS` |
| `dwDevCaps` | `0x2451` | `0x2653` | `TEXTUREVIDEOMEMORY`, `SORTINCREASINGZ` |
| `dpcTriCaps.dwMiscCaps` | `0x10` | `0x70` | `CULLCW`, `CULLCCW` |
| `dwRasterCaps` | `0xB0` | `0xA1` | `DITHER` |
| `dwSrcBlendCaps` | `0x10` | `0x12` | `ONE` |
| `dwDestBlendCaps` | `0x20` | `0x21` | `ZERO` |
| `dwShadeCaps` | `0x8520A` | `0xC528A` | `FOGFLAT`, `SPECULARFLATRGB` |
| `dwTextureCaps` | `0x1` | `0x2F` | `POW2`, `SQUAREONLY`, others |
| `dwTextureBlendCaps` | `0x43` | `0xCF` | several |
| `dwDeviceRenderBitDepth` | `0x400` | `0x600` | `DDBD_24` |
| texture formats | 1 | 5 | four more |

Hellbender shows **no fog warning at all** on the stock driver, so the missing
`D3DPSHADECAPS_FOGFLAT` is what produces it here.

## Conclusion: the game does not use hardware Direct3D on this chipset

With `useDirect3D=1` and a driver whose HAL it accepts without a single
warning, Hellbender renders **identically** on the stock S3 ViRGE driver and
on Velocity9x. Comparing the 3D viewport of the two captures at the same game
state, 97.1% of sampled pixels match exactly; the remainder is animated rain
and a moving target marker between the two capture moments.

If the stock driver were giving the game hardware rasterisation and ours were
not, the two frames could not agree pixel-for-pixel. Both are the same
software renderer.

So the premise of this investigation was wrong. There is no capability this
driver can publish that moves Hellbender onto hardware Direct3D, because the
game does not take that path on this chipset even when a full retail driver
offers it. The earlier capability experiments were not merely unlucky guesses;
the target did not exist.

## What remains worth doing

1. **`D3DPSHADECAPS_FOGFLAT`.** The one difference with a visible symptom. The
   driver already implements fog as a colour blend, so flat fog is within what
   it can honour, and adding it removes the 3D Adapter Warning.
2. The rest of the table is a fidelity roadmap for Direct3D titles that *do*
   use the hardware path — culling, dithering, the four missing texture
   formats — and should be driven by a title that actually exercises them,
   not by Hellbender.
3. Do not spend further effort trying to move Hellbender onto the HAL.

## Related

- [Hellbender Direct3D review and test handoff](../handoffs/2026-08-13-hellbender-d3d-review.md)
- [Hellbender hardware Direct3D compatibility plan](../plans/hellbender-hardware-d3d.md)
- [DIBENG fault that previously stopped the game reaching gameplay](2026-08-14-hellbender-dibeng-gpf.md)

Evidence under `build/driver-results/hellbender-bitblt/`: `EXECCAPS-*.INI`,
`TEXCAPS-*.INI`, and `HELLBENDER-GAMEPLAY.INI`.
