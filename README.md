# Velocity9x

Velocity9x is a ground-up Windows 9x display-driver project. The first target is
Windows 98 Second Edition on the S3 ViRGE/DX 86C375 (`5333:8A01`).

The repository contains a preparatory Phase 1 scaffold while the Phase 0 ABI
specification in [PLAN.md](PLAN.md) is completed. It currently contains original,
host-testable driver core code. It does **not** yet produce an installable
Windows 98 display driver.

## Current implementation

- strict matching for the first supported PCI device;
- overflow-safe framebuffer pitch and size calculation;
- a chipset-neutral backend contract;
- fixed-size diagnostic records suitable for a serial transport;
- lifecycle shells for the 16-bit display and 32-bit mini-VDD sides;
- host tests and an Open Watcom build entry point.

No acceleration capability is advertised yet. The operating-system ABI thunks,
resource mapping, and mode programming remain gated on the Phase 1 ABI and
toolchain spike.

## Host build

Install an Open Watcom v2 snapshot, initialize its environment, and run:

```powershell
./scripts/build-host.ps1
```

The script builds and runs `build/host/v9x-host-tests.exe`. A missing compiler is
reported as a prerequisite failure; the script does not download toolchains or
licensed SDK/DDK material.

To check the repository structure without a compiler:

```powershell
./scripts/check-tree.ps1
```

## Safety and licensing

Do not install generated artifacts in Windows 98 until the packaging gate in
`packaging/win98se/README.md` is cleared. Always keep a standard-VGA snapshot.

No project license has been selected. Unless and until a license is added, this
work is all rights reserved.
