# Final Reality 1.01 hardware Direct3D

Status: hardware enumeration and benchmark submission path working on VM 9869;
subpixel accuracy, specular Gouraud, and vertex fog working; texture mapping
and real Z-buffer state remain incomplete.

## 2026-08-14 baseline diagnosis

Final Reality 1.01 showed only `Direct3D Software` and reported `No Direct3D
hardware rendering platforms found!`. Its bundled `e2driver\d3d_mfc.dll`
contains explicit device-rejection paths for `failed: no zbuffer` and `failed:
no texture filters`.

The full-capability V9XDDP comparison found:

- Velocity9x: `HwTriFilter=0`, `HwZDepth=0`;
- stock ViRGE: `HwTriFilter=63`, `HwZDepth=1024` (16-bit).

The desktop and fullscreen modes were already 16-bpp, so this was not FR's
documented 32-bpp rejection case.

## Implemented

- Advertise 16-bit Z-buffer surfaces and point/linear texture filtering.
- Include Z-buffer surfaces in the DirectDraw surface capability set.
- Validate and accept an attached video-memory Z surface during D3D context
  creation and render-target changes.
- Expand V9XDDP enumeration output to record every D3D device and its complete
  hardware primitive capability block.
- Accept FR's 128-triangle RenderPrimitive batches.
- Use the full 16-bit D3D triangle-index contract instead of the former
  arbitrary 192/1024 vertex limits. FR uses one large transformed-vertex
  buffer and indices observed beyond `0x1000`.
- Clip guard-band triangles to the active viewport in software before writing
  the ViRGE's unsigned S11.20 X-start setup register.
- Add `D3dPrimitiveReject` trace events to distinguish index, input-coordinate,
  and post-clip hardware-submission failures.
- Advertise subpixel rasterization and exercise the fractional S11.20 triangle
  setup path in V9XDDP (`D3DSubpixelTriangleOk`).
- Retain `SPECULARENABLE`, `FOGENABLE`, and `FOGCOLOR` per D3D context.
- Saturating-add each transformed vertex's specular RGB to its diffuse colour,
  then apply vertex fog from the specular alpha/fog factor before the existing
  ViRGE Gouraud colour interpolation.
- Advertise specular Gouraud and vertex Gouraud fog, with independent
  pixel-verified V9XDDP tests (`D3DSpecularGouraudOk` and `D3DDepthFogOk`).

## Verified result

Installed build: `fr101-hardware-index16`, boot counter 145.

- FR's platform selector shows `Direct3D On-board Accelerator`.
- FR creates all hardware contexts and runs the 25-pixel benchmark.
- The focused test completes with 18.06 Kpolys/s and 0.58 marks.
- Its final 128-triangle batches return `S_OK` with no primitive-reject event.
- All FR test contexts and texture handles observed in the earlier broad run
  were destroyed cleanly.
- Final V9XDDP gate: `Result=COMPLETE`, `D3DHalFound=1`,
  `D3DCreateDeviceHr=0`, `D3DTrianglePixelOk=1`, `D3DContextCycleOk=1`, and
  `BltFillPixelOk=1`.
- Final trace: zero FIFO timeouts, idle timeouts, engine resets, or context
  rejects.

Evidence is under
`build\driver-results\fr101-hardware-index16-vm1`.

### Specular/fog verification

Installed build: `fr101-specular-fog`, boot counter 147.

- FR enables `Depth fog`, `Specular gouraud`, and `Subpixel accuracy` for the
  `Direct3D On-board Accelerator` platform.
- The focused 25-pixel hardware benchmark returns to the Advanced Options UI.
- V9XDDP reports both new state calls successful and both pixel checks equal to
  1, while retaining every prior mandatory DirectDraw/Direct3D gate.
- The post-FR trace records 6 clean context create/destroy pairs, 2,371
  primitive calls, 8 render-state calls, and zero FIFO timeouts, idle timeouts,
  engine resets, or context rejects.

Evidence is under
`build\driver-results\fr101-specular-fog-vm1`.

## Remaining correctness work

FR reports only 18.52% visual appearance. The current triangle path accepts
texture handles and render-state calls but still emits a flat-colour S3D
command. It also validates the attached Z surface without programming Z
coordinates, comparison mode, or updates. The next implementation slice is:

1. retain the current texture handle and relevant render states per context;
2. resolve the handle to its DirectDraw video-memory surface;
3. program TEX_BASE and the ViRGE U/V gradients for non-perspective RGB565
   texturing;
4. program Z_BASE/Z_STRIDE and Z gradients/comparison/update state;
5. add a pixel-verified textured-plus-Z probe before enabling further FR
   options such as mipmapping and alpha blending.
