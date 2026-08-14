# S3 Trio64 conservative framebuffer bring-up

Date: 2026-08-14

Target: 86Box PCI S3 Trio32/64 86C764 (`5333:8811`), 4 MiB VRAM, Windows 98 SE

## Decision

Add a separate `-S3Trio64` build target that reuses the proven S3 VBE mode
entry, linear-aperture mapping, DIB Engine, palette, and mini-VDD paths. Keep
ViRGE-only new-MMIO enablement and the DirectDraw/S3D HAL unadvertised.

This is intentionally a software-GDI baseline. It proves stable display
ownership and mode handling before any Trio64 2D engine work is attempted.

The package builder emits `build/win98se-trio64` and generates an INF matching
only `PCI\VEN_5333&DEV_8811`. The original ViRGE package remains strict to
`PCI\VEN_5333&DEV_8A01`.

## Guest result

The first association was made against the cloned VM's existing
`Display\0002` Trio64 node after a cold disk/config/NVR backup. Build
`trio64-001` reached `enable-ok` on its first boot and published:

- adapter `S3 Trio32/64 86C764`;
- vendor/device `5333:8811`;
- active linear-aperture mapping;
- valid shared core/memory clock of 69.800 MHz in 86Box;
- live same-depth mode switching.

The standalone settings utility and native Display Properties Velocity9x tab
both display those facts. The machine-specific property-sheet handler tag was
validated against the existing Windows 98 handlers.

## Verified baseline

All six framebuffer modes pass the unattended GDI drawing and pixel-readback
test:

- 640x480x8, 800x600x8, 1024x768x8;
- 640x480x16, 800x600x16, 1024x768x16.

Resolution switching within one color depth succeeds live. The 8-bpp palette
animation and screen-readback test passes. Switching color depth is not live;
updating the hardware-profile depth and rebooting succeeds in both directions.
The VM was left at 1024x768x16 with a final GDI PASS.

## Current boundary

- No Trio64 hardware acceleration is advertised.
- The ViRGE DirectDraw HAL, new-MMIO window, and S3D path are disabled.
- Monitor-power behavior has not yet been included in this Trio64 baseline.
- The remote agent's INFO `BitsPerPixel` field reports zero on this guest even
  though GDI and the Velocity9x settings contract report the correct depth.

Future Trio64 work should add one bounded 2D primitive at a time, beginning
with engine identification and idle-status proof, while retaining this
software-GDI configuration as the recovery baseline.
