# Trace8c clock, palette, and repeat-matrix result

Date: 2026-08-09

Build: `trace8c-clocks-palette`

Target: Windows 98SE, 86Box S3 ViRGE/DX 86C375, 4 MiB VRAM

## Hardware diagnostics

The active driver published a valid `s3-virge-pll-v1` result after mode entry:

- core / graphics engine: 56.079 MHz;
- memory: 56.079 MHz;
- relationship: engine clock shared with MCLK;
- boot stage: `enable-ok`.

The settings utility consumed the generic diagnostics contract and displayed
those values without direct hardware access. Evidence is retained in
`build/driver-results/trace8c-clocks/SETTINGS.BMP`.

## Palette test

`V9XPAL.EXE /auto` creates and realizes a 256-entry logical palette, draws a
reserved palette index, animates that entry without redrawing, and checks both
the logical palette and screen-pixel readback. It records machine-readable
results in `C:\V9XPAL.INI`. The first standalone 800x600x8 run passed.

## Repeated matrix

Two complete six-mode passes succeeded on boots 36 through 47. Every case
reached its exact requested resolution and depth, recorded `enable-ok`, and
passed the GDI framebuffer test. The palette test additionally passed all six
8-bit cases (three resolutions across two passes). The 16-bit cases correctly
record palette testing as not applicable.

The JSON summary and twelve screenshots are under
`build/driver-results/trace8c-repeat-matrix`. This increases the Phase 3
evidence to repeated reboot-selected mode activation, but does not replace the
larger final reliability gate.
