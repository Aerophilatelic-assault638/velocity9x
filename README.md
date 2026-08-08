# Velocity9x

Velocity9x is a ground-up Windows 9x display-driver project. The first target is
Windows 98 Second Edition on the S3 ViRGE/DX 86C375 (`5333:8A01`).

The repository contains the portable core, proven Phase 1 loader probes, and a
host-audited 640x480x8 activation candidate. The candidate is not yet a release
or a guest-proven installable driver; its first activation remains gated on a
cold VM disk/NVR backup and recovery test.

## Current implementation

- strict matching for the first supported PCI device;
- decoded PCI framebuffer-resource validation and bounded VRAM overrides;
- overflow-safe framebuffer pitch and size calculation;
- a chipset-neutral backend contract;
- fixed-size diagnostic records suitable for a serial transport;
- lifecycle shells for the 16-bit display and 32-bit mini-VDD sides;
- a hardware-inert dynamic VxD lifecycle probe with bounded COM1 logging;
- a consolidated VxD-plus-Win16-DRV lifecycle test with one PASS/FAIL result;
- a fixed-mode DIB Engine framebuffer candidate and default-handler mini-VDD;
- a strict S3-only INF, recovery documentation, and read-only settings panel;
- host tests and an Open Watcom build entry point.

No acceleration capability is advertised. The active candidate uses firmware
mode entry and DPMI framebuffer mapping; guest activation evidence is still
pending.

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
Engine. It now contains the quarantined 640x480x8 activation path; build the full
package and follow its cold-backup procedure rather than installing this binary
alone.

The same external DDK supplies the MASM and linker needed to prove the 32-bit
mini-VDD image path:

```powershell
./scripts/build-minivdd-skeleton.ps1
```

This produces `build/minivdd32/v9xmini.vxd`. It verifies the master VDD table,
logs its build, succeeds, and installs zero callbacks so the master VDD retains
its defaults.

Build the quarantined active package, Windows 98 settings/status panel, and
post-boot GDI framebuffer test:

```powershell
./scripts/build-active-package.ps1
```

The output is `build/win98se-active`, also staged as `build/vm-probe/ACTIVE`.
Read `FIRSTBOOT.TXT`, `INSTALL.TXT`, and `RECOVER.TXT`. Do not install it until
86Box is completely stopped and the cold profile backup has completed.

To check the repository structure without a compiler:

```powershell
./scripts/check-tree.ps1
```

The inventoried local 86Box installations and the changes needed for an S3
ViRGE/DX test clone are recorded in `docs/vm-environment.md`.

To prepare the safe VM transfer and COM1 smoke-test folder:

```powershell
./scripts/prepare-vm-probe.ps1
```

Mount `build/vm-probe` as an 86Box virtual CD folder. `V9XSER.EXE` performs the
COM1 smoke test; `V9XVXD.EXE` may be run with `V9XPROBE.VXD` beside it to
perform the separate dynamic load/unload probe. The top-level `V9XDISP.DRV` and
`V9XMINI.VXD` remain historical noninstallable probe artifacts. Only the
quarantined `ACTIVE` subdirectory contains the activation INF. `V9X16LD.EXE`
loads the candidate DRV as an inactive Win16 library, requests GDIINFO, and
validates supported and unsupported modes; it never invokes the mode-setting
`Enable` action.

`V9XSTAGE.EXE` is the preferred consolidated Phase 1 test. It holds the
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
