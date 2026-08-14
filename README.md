# Velocity9x

Velocity9x is a ground-up Windows 9x display-driver project. The primary target
is Windows 98 Second Edition on the S3 ViRGE/DX 86C375 (`5333:8A01`). A
conservative software-GDI target also supports the S3 Trio32/64 86C764
(`5333:8811`).

Current development version: **0.2**. See [CHANGELOG.md](CHANGELOG.md) for the
version history.

The focused compatibility plan for getting Hellbender menus and its first
mission onto the hardware Direct3D path is in
[docs/plans/hellbender-hardware-d3d.md](docs/plans/hellbender-hardware-d3d.md).

The repository contains the portable core, proven Phase 1 loader probes, and a
guest-proven Phase 3 framebuffer candidate covering 640x480, 800x600, and
1024x768 at 8 and 16 bpp. All six modes have passed cold-boot activation and
software-GDI framebuffer checks under 86Box, including two consecutive full
matrix repetitions. It is not a release driver.

## Current implementation

- strict matching for the first supported PCI device;
- decoded PCI framebuffer-resource validation and bounded VRAM overrides;
- overflow-safe framebuffer pitch and size calculation;
- a chipset-neutral backend contract;
- fixed-size diagnostic records suitable for a serial transport;
- lifecycle shells for the 16-bit display and 32-bit mini-VDD sides;
- a hardware-inert dynamic VxD lifecycle probe with bounded COM1 logging;
- a consolidated VxD-plus-Win16-DRV lifecycle test with one PASS/FAIL result;
- a boot-selected DIB Engine framebuffer candidate and default-handler mini-VDD;
- an unattended GDI framebuffer test with machine-readable Win98 results;
- an unattended 8-bit palette animation and screen-readback test;
- backend-neutral hardware diagnostics with S3 ViRGE PLL clock detection;
- a strict S3-only INF, recovery documentation, and read-only settings panel;
- a read-only "Velocity9x" page inside native Display Properties, installed as
  a shell property-sheet extension by the INF;
- a guest-proven Trio64 framebuffer/DirectDraw target with hardware diagnostics and the
  complete 640/800/1024 x 8/16-bpp software-GDI matrix;
- host tests and an Open Watcom build entry point.

The active candidate uses firmware mode entry and DPMI framebuffer mapping.
Same-depth resolution changes apply live through the ReEnable path;
colour-depth changes take effect after a reboot (Windows 9x never changes
depth dynamically). A DirectDraw HAL (V9XHAL.DLL, flat 32-bit) provides
video-memory surfaces, CRTC display-start page flipping, real vertical-blank
services, and bounded ViRGE solid-colour blits. The Direct3D HAL now advertises
and pixel-verifies one deliberately narrow hardware path: flat-colour,
pre-transformed/lit triangle lists submitted through the legacy v1
`RenderPrimitive` callback. Textures, Z buffering, blending, fog, lighting,
transforms, clipping, lines, and indexed primitives remain unsupported. The
ViRGE triangle engine writes native ZRGB1555 while the current 16-bpp display
mode is RGB565, so this is an S3D execution milestone rather than general
Direct3D compatibility. Palettes, stretching and colour-keyed blits remain
with the DirectDraw HEL. The Trio64 target provides the same DirectDraw
services on its own engine; GDI acceleration is not yet advertised on either.
Advertising the DirectDraw blitter is all-or-nothing on Win98 — a driver that
claims it must also advertise ROP3 SRCCOPY and must complete every blit it
admits, because a declined blit is not emulated. See
[docs/issues/2026-08-14-directdraw-hal-nohardware.md](docs/issues/2026-08-14-directdraw-hal-nohardware.md)
and [docs/decisions/2026-08-14-virge-blitter.md](docs/decisions/2026-08-14-virge-blitter.md).

## Host build

Install an Open Watcom v2 snapshot, initialize its environment, and run:

```powershell
./scripts/build-host.ps1
```

The script builds and runs `build/host/v9x-host-tests.exe`. A missing compiler is
reported as a prerequisite failure; the script does not download toolchains or
licensed SDK/DDK material.

The same suite can be built with a second compiler for independent warnings:

```powershell
./scripts/build-host-msvc.ps1
```

This locates MSVC (directly or via `vswhere`) and builds with `/W4 /WX` into
`build/host-msvc`. Both scripts default the embedded build identifier to the
current git revision, with a `-dirty` suffix for a modified tree.

The toolchain spike can also build the original Win16 NE loader shell:

```powershell
./scripts/build-win16-skeleton.ps1
```

This produces `build/win16/v9xdisp.drv` and verifies its DOS and NE signatures.
It proves the compiler/linker path only; the file is not an installable display
driver because the display DDI exports and DIB Engine contract are not complete.

With the external Windows 98 DDK installed at `C:\98DDK`, build the ABI/DDI
skeleton with:

```powershell
./scripts/build-win16-ddi-skeleton.ps1
```

That image exports the documented display ordinals and imports the system DIB
Engine. It now contains the quarantined 640/800/1024 x 8/16-bpp activation path;
build the full package and follow its recovery procedure rather than installing
this binary alone.

The same external DDK supplies the MASM and linker needed to prove the 32-bit
mini-VDD image path:

```powershell
./scripts/build-minivdd-skeleton.ps1
```

This produces `build/minivdd32/v9xmini.vxd`. It verifies the master VDD table,
logs its build, and installs audited legacy-VESA and Windows 98 monitor-power
callbacks. The driver advertises D0 only because the legacy BIOS resume path
does not reliably restore the active framebuffer.

Build the quarantined active package, Windows 98 settings/status panel, and
post-boot GDI and palette framebuffer tests:

```powershell
./scripts/build-active-package.ps1
```

The output is `build/win98se-active`, also staged as `build/vm-probe/ACTIVE`.
Read `FIRSTBOOT.TXT`, `INSTALL.TXT`, and `RECOVER.TXT`. Do not install it until
86Box is completely stopped and the cold profile backup has completed.

For the conservative S3 Trio32/64 PCI target, use:

```powershell
./scripts/build-active-package.ps1 -S3Trio64 -BootTrace
```

This produces `build/win98se-trio64`. It matches only PCI `5333:8811`, uses
the shared S3 VBE/linear-framebuffer path, and deliberately does not expose the
ViRGE DirectDraw, MMIO, or S3D engine. The verified bring-up and current limits
are recorded in [docs/decisions/2026-08-14-trio64-bringup.md](docs/decisions/2026-08-14-trio64-bringup.md).

To check the repository structure without a compiler:

```powershell
./scripts/check-tree.ps1
```

The inventoried local 86Box installations and the changes needed for an S3
ViRGE/DX test clone are recorded in `docs/vm-environment.md`.

After an already-associated candidate is installed and the remote agent is
online, run the complete reboot-selected mode matrix unattended with:

```powershell
./scripts/run-vm-mode-matrix.ps1
```

The runner refuses a mismatched installed DRV/VXD pair, verifies the requested
mode and `enable-ok` trace after every reboot, runs the machine-readable GDI
test, runs palette animation/readback in every 8-bit mode, and retains a
screenshot and JSON summary per mode. Use `-Repeat 2` (or higher) for repeated
reliability passes.

The settings panel reads the versioned `C:\V9XHW.INI` hardware-diagnostics
contract. The S3 ViRGE/DX and Trio64 targets decode MCLK from their shared S3
PLL registers and report the engine clock as shared with memory; unsupported
backends display `Unavailable` instead of guessing.

For a device already associated with Velocity9x, update a locked DRV/VXD pair
without SetupX media prompts using:

```powershell
./scripts/update-associated-driver.ps1
```

The updater verifies the existing class binding, runs the loader preflight,
refuses to overwrite unrelated pending `WININIT.INI` work, stages both files
for atomic boot-time replacement, reboots, and byte-verifies the installation.

To prepare the safe VM transfer and COM1 smoke-test folder:

```powershell
./scripts/prepare-vm-probe.ps1
```

Mount `build/vm-probe` as an 86Box virtual CD folder. The installable package is
isolated under `D:\ACTIVE`; all noninstallable utilities and probe binaries are
under `D:\PROBE`. The preparation script removes known legacy probe binaries
from the folder-CD root and writes `START-HERE.TXT` identifying both areas.

Run `D:\PROBE\V9XSER.EXE` for the COM1 smoke test. `D:\PROBE\V9XVXD.EXE` may
be run with `V9XPROBE.VXD` beside it to perform the separate dynamic
load/unload probe. `D:\PROBE\V9X16LD.EXE` loads the probe DRV as an inactive
Win16 library, requests GDIINFO, and validates supported and unsupported modes;
it never invokes the mode-setting `Enable` action. Never install
`V9XDISP.DRV` or `V9XMINI.VXD` from `D:\PROBE`.

`D:\PROBE\V9XSTAGE.EXE` is the preferred consolidated Phase 1 test. It holds the
hardware-inert VxD open while the Win16 helper performs that query-only DRV
preflight, then reports one result and writes the lifecycle to COM1. It still
does not install or activate the display driver.

For unbuffered host diagnostics, configure 86Box COM1 as a Named Pipe server
named `velocity9x-com1`, then run:

```powershell
./scripts/capture-serial-pipe.ps1
```

## Safety and licensing

Do not install generated artifacts in Windows 98 until the packaging gate in
`packaging/win98se/README.md` is cleared. Always keep a standard-VGA snapshot.

Copyright (c) 2026 Michael Dale. No project license has been selected. Unless
and until a license is added, this work is all rights reserved.
