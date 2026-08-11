# Direct3D phase 3: first hardware triangle

Date: 2026-08-11

Target: Windows 98SE DirectX 6.1, S3 ViRGE/DX 86C375

## Scope

Phase 3 adds one pixel-verified S3D rendering path to the phase-2 device and
context foundation:

- legacy Direct3D HAL v1 `RenderPrimitive` execute-buffer parsing;
- `D3DOP_TRIANGLE` records referencing pre-transformed/lit vertices;
- flat RGB, non-indexed triangle-list capability advertisement;
- target offset, pitch, dimensions, and clipping derived from the context's
  video-memory render target;
- bounded FIFO/idle waits and S3D autoexecute triangle-register submission;
- x87 state preservation around floating-point edge setup without adding DLL
  imports.

The runtime requires an `IDirect3DViewport2` to be created, added, and made
current before `DrawPrimitive`. The guest probe performs that sequence, draws
a red triangle at `(8,8)`, `(56,8)`, `(8,56)`, and reads a covered target pixel
back through DirectDraw.

## ABI decision

The DirectX 5 callbacks2 route was investigated. Advertising
`DDHALINFO_GETDRIVERINFOSET` together with a callbacks2 payload caused the
Windows 98 runtime to reject the hardware HAL. The accepted implementation
therefore leaves `GetDriverInfo` disabled in the published HAL flags and uses
the v1 `RenderPrimitive` callback that the runtime actually invokes. The
callbacks2 implementation remains non-advertised pending a separately verified
ABI correction.

## Capability boundary

The device advertises RGB color, floating-point TL vertices, system-memory TL
vertices, draw-primitive TL vertices, 16-bpp render targets, no culling, and
flat RGB triangle shading. It does not advertise or implement:

- texture formats or texture sampling;
- Z buffering, alpha blending, fog, or color-key blending;
- transforms, lighting, or clipping;
- lines, indexed primitives, or arbitrary render-state handling.

The ViRGE triangle command used here produces native ZRGB1555. The active
Windows display mode remains RGB565, so the verified red sample is raw
`0x7C00`, not RGB565 red `0xF800`. This proves that the S3D engine executed and
wrote the selected video-memory target; it does not yet establish correct
color presentation in the current mode. A 15-bpp target/mode path or a safe
format strategy is required before broadening the compatibility claim.

## Guest result

Build `d3d-phase3-accepted` passed on the Win98SE guest at boot counter 111:

- Direct3D query, HAL enumeration, target/device/viewport creation,
  `BeginScene`, `DrawPrimitive`, and `EndScene` all returned `DD_OK`;
- descriptor flags were `0xC3` and render depth was `DDBD_16`;
- the covered triangle pixel was raw `0x7C00` and
  `D3DTrianglePixelOk=1`;
- `D3DContextCycleOk=1`;
- ten consecutive complete Direct3D probe runs passed;
- DirectDraw hardware fill and completion/pixel checks remained clean;
- the final GDI framebuffer regression returned `PASS` at 800x600x16 with the
  same `d3d-phase3-accepted` build ID.

The DirectDraw probe still records `FlipPixelOk=0`: GDI readback after a real
display-start flip is a known limitation of that diagnostic and is not a
triangle-rendering regression. Result files and the final desktop capture are
retained under `build/driver-results/d3d-phase3-accepted`.
