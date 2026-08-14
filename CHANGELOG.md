# Changelog

- Trace DirectDraw surface negotiation through `CanCreateSurface`,
  `CreateSurface`, `DestroySurface`, and `AddAttachedSurface`; enlarge the
  shared callback ring, correct Win16 exit bookkeeping, and honor the
  `GetDriverInfo` handled-return contract.
- Extend V9XDDP with an RGB565 Direct3D texture lifecycle and add V9XWND, a
  GDI-free top-level window inventory for diagnosing blocked fullscreen
  dialogs.
- Program ViRGE 8.7 color gradients for Gouraud-shaded triangles and expose
  the hardware's perspective-correction raster capability required by
  Hellbender's Direct3D device filter.
- Publish a coherent RGB565 Direct3D texture format and bounded legacy
  texture-handle lifecycle callbacks, with per-operation trace diagnostics.
- Add dormant legacy Direct3D execute-buffer parsing and DirectDraw
  pseudo-surface lifecycle tracing. Win98 rejects a HAL that publishes the
  obsolete `Execute` entry, so the valid DX5 callback path remains advertised.
- Guard the Win16 `SetCursor` and `MoveCursor` DIBENG extension thunks while
  the display PDEVICE is unavailable during mode teardown, preventing a null
  PDEVICE fault in `DIB_MOVECURSOREXT` observed when Hellbender exits a failed
  full-screen initialization; guarded Pascal returns discard their four bytes
  of original cursor arguments before returning to USER.
- Follow the Windows 98 DIBENGINE mini-driver ReEnable ordering by rebuilding
  the PDEVICE directly, without carrying a BeginAccess cursor exclusion across
  `CreateDIBPDevice`; the old exclusion state is invalid after the in-place
  PDEVICE rebuild and caused striped framebuffer writes plus a cursor fault.

All notable Velocity9x changes are recorded here. The project uses semantic
version numbers for product milestones; diagnostic builds retain a separate
build identifier so exact guest-tested binaries remain traceable.

## Unreleased

### Added

- Add a conservative S3 Trio32/64 86C764 (`5333:8811`) build target with
  strict INF matching, Trio-aware PCI discovery and hardware reporting, and
  no ViRGE-only DirectDraw/MMIO/S3D exposure. The 86Box target passes live
  640x480, 800x600, and 1024x768 switching plus GDI validation at both 8 and
  16 bpp, with palette validation at 8 bpp.
- Register and verify the Velocity9x native Display Properties page and
  standalone settings utility on the Trio64 target, including adapter, mode,
  framebuffer, clock, build, and last-test reporting.
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
