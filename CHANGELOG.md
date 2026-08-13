# Changelog

All notable Velocity9x changes are recorded here. The project uses semantic
version numbers for product milestones; diagnostic builds retain a separate
build identifier so exact guest-tested binaries remain traceable.

## Unreleased

### Added

- The DirectDraw HAL now writes its callback ring directly to
  `C:\V9XTRACE.INI` on an unhandled process fault or bounded ViRGE engine
  timeout, before recovery can discard the last useful callback history. The
  manual trace utility writes `C:\V9XSNAP.INI` so it cannot erase that evidence.

### Fixed

- Direct3D primary and flip-chain render targets now use the live scanout
  pitch, dimensions, and RGB565 description instead of potentially stale
  per-surface metadata. Target layout is included in the callback trace.

## 0.2 - 2026-08-11

### Added

- A flat 32-bit DirectDraw HAL with video-memory surfaces, vertical-blank
  services, CRTC display-start flipping, and bounded ViRGE solid-color fills.
- A minimal Direct3D HAL device and allocation-free context lifecycle.
- Pixel-verified S3D rendering for flat-color, pre-transformed/lit triangle
  lists through the legacy Direct3D v1 `RenderPrimitive` callback.
- DirectDraw, Direct3D, GDI, mode-switch, palette, power, and driver-stage
  guest diagnostics.
- A read-only Velocity9x page in Display Properties and a standalone settings
  utility showing hardware, mode, clock, framebuffer, test, version, and build
  information.

### Changed

- Same-depth resolution changes now apply live; color-depth changes remain
  reboot-selected.
- The supported framebuffer matrix covers 640x480, 800x600, and 1024x768 at
  8 and 16 bpp.

### Fixed

- Corrected the Windows 98 `DDHAL_FLIPTOGDISURFACEDATA` ABI layout and added
  an exclusive-mode lifecycle callback that restores CRTC display start when
  returning from flipped DirectDraw surfaces to the GDI desktop.
- Prevented unattended GDI validation from reporting false pixel failures
  when a boot-time utility dialog obscures the sampled client area.

### Known limitations

- S3D triangle output is native ZRGB1555 while the current 16-bpp Windows mode
  is RGB565; version 0.2 proves hardware execution but is not general Direct3D
  compatibility.
- Textures, Z buffering, blending, fog, lighting, transforms, clipping, lines,
  and indexed primitives are not supported.
- GDI acceleration and a hardware cursor are not yet advertised.

## 0.1 - 2026-08-08

### Added

- Initial repository structure, portable driver core, S3 ViRGE/DX device
  targeting, Win16 display-driver skeleton, mini-VDD lifecycle probe, host
  tests, diagnostics, packaging, and recovery documentation.
