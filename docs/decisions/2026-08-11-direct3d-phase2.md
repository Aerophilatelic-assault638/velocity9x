# Direct3D phase 2: device and context foundation

Date: 2026-08-11

Target: Windows 98SE DirectX 6.1, S3 ViRGE/DX 86C375

## Scope

Phase 2 connects the existing flat DirectDraw HAL to the legacy Direct3D HAL
v1 boundary without submitting S3D rendering commands:

- project-owned, size-guarded `D3DHAL_GLOBALDRIVERDATA`,
  `D3DDEVICEDESC_V1`, primitive-capability, and callback-table layouts;
- `DDCAPS_3D` and `DDSCAPS_3DDEVICE` only after the Direct3D pointers are
  populated;
- a 16-entry, allocation-free context pool with create, destroy, and
  process-wide destroy callbacks;
- context creation restricted to a non-null target in an active 16-bpp mode;
- diagnostic counters for accepted/rejected contexts and unexpected render
  callback calls;
- explicit `RenderState` and `RenderPrimitive` fallback callbacks, required by
  the v1 ABI when no Execute callback exists.

The device descriptor advertises RGB and 16-bpp render-target compatibility.
It intentionally advertises no line, triangle, texture, Z-buffer, transform,
lighting, or execute-buffer capability. Returning a Direct3D interface is not
treated as permission to touch S3D registers.

## Validation contract

The runtime-free guest probe now queries `IDirect3D2`, enumerates devices,
identifies `IID_IDirect3DHALDevice`, creates a small video-memory
`DDSCAPS_3DDEVICE` target, creates the HAL device, and releases it. Its result
file records:

- `D3DQueryHr` and `D3DEnumHr`;
- `D3DHalFound`, descriptor flags, and render depth;
- `D3DTargetHr`, `D3DCreateDeviceHr`, and `D3DContextCycleOk`.

The existing `/status-only` sequence remains the first guest activation gate,
so phase 2 can validate D3D enumeration/context lifetime and ViRGE MMIO status
without issuing a graphics-engine write. Full DirectDraw fill, flip, and GDI
regressions run only after that gate passes.

## Deferred

- line or triangle capability advertisement;
- render-state parsing;
- transformed/lit vertex submission;
- S3D triangle register programming;
- Z buffering, textures, fog, blending, and clipping.

Those features require independent pixel-verified milestones. Phase 2 must not
be described as hardware 3D rendering support.

## Guest result

Build `d3d-phase2-1` passed on the backed-up Win98SE guest at boot counter 93:

- `D3DQueryHr=0` and `D3DEnumHr=0`;
- `D3DHalFound=1`, descriptor flags `0x83`, render depth `DDBD_16`;
- `D3DTargetHr=0`, `D3DCreateDeviceHr=0`, and `D3DContextCycleOk=1`;
- 20 additional create/release iterations passed, exceeding the 16-slot pool
  and proving that the runtime reaches the destroy path;
- phase-1 regressions remained clean: hardware fill completion and pixel
  verification passed, the 20-flip batch completed in at most 1 ms, and the
  shell-launched GDI framebuffer test returned `PASS` at 800x600x16.

The activation used the cold backup
`Win86SE-pre-velocity9x-20260811-195326`. Result files and the post-test
screenshot are retained under `build/driver-results/d3d-phase2-1`.
