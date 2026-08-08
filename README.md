# Velocity9x

Velocity9x is a ground-up Windows 9x display-driver project. The first target is
Windows 98 Second Edition on the S3 ViRGE/DX 86C375 (`5333:8A01`).

The repository contains a preparatory Phase 1 scaffold while the Phase 0 ABI
specification in [PLAN.md](PLAN.md) is completed. It currently contains original,
host-testable driver core code. It does **not** yet produce an installable
Windows 98 display driver.

## Current implementation

- strict matching for the first supported PCI device;
- decoded PCI framebuffer-resource validation and bounded VRAM overrides;
- overflow-safe framebuffer pitch and size calculation;
- a chipset-neutral backend contract;
- fixed-size diagnostic records suitable for a serial transport;
- lifecycle shells for the 16-bit display and 32-bit mini-VDD sides;
- host tests and an Open Watcom build entry point.

No acceleration capability is advertised yet. The operating-system ABI thunks,
resource mapping, and mode programming remain gated on the Phase 0 ABI
specification and the Phase 1 toolchain spike.

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
Engine, but `Enable`, `ReEnable`, and `ValidateMode` deliberately reject use. It
is a link/ABI artifact and remains unsafe to install.

The same external DDK supplies the MASM and linker needed to prove the 32-bit
mini-VDD image path:

```powershell
./scripts/build-minivdd-skeleton.ps1
```

This produces `build/minivdd32/v9xmini.vxd`. Its initialization entry point
always reports failure, so it cannot claim or program hardware. It is a build
artifact only and must not be installed.

To check the repository structure without a compiler:

```powershell
./scripts/check-tree.ps1
```

## Safety and licensing

Do not install generated artifacts in Windows 98 until the packaging gate in
`packaging/win98se/README.md` is cleared. Always keep a standard-VGA snapshot.

Copyright (c) 2026 Michael Dale. No project license has been selected. Unless
and until a license is added, this work is all rights reserved.
