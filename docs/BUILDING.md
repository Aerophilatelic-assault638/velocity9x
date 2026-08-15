# Building Velocity9x

All build scripts are PowerShell and live in `scripts/`. None of them download
toolchains or licensed SDK/DDK material; a missing prerequisite is reported as
a prerequisite failure.

## Prerequisites

| Component | Needed for |
|---|---|
| [Open Watcom v2](https://github.com/open-watcom/open-watcom-v2) snapshot | Everything. Initialize its environment first. |
| Windows 98 DDK at `C:\98DDK` | The Win16 display driver, the mini-VDD, and the active package. |
| MSVC (optional) | A second-compiler warning pass over the host tests. |

To check the repository structure without any compiler:

```powershell
./scripts/check-tree.ps1
```

## Host tests

```powershell
./scripts/build-host.ps1
```

Builds and runs `build/host/v9x-host-tests.exe`. For an independent set of
warnings, the same suite can be built with MSVC at `/W4 /WX`:

```powershell
./scripts/build-host-msvc.ps1
```

This locates MSVC directly or via `vswhere` and builds into `build/host-msvc`.
Both scripts default the embedded build identifier to the current git revision,
with a `-dirty` suffix for a modified tree.

## The installable packages

This is what you want if you intend to test the driver on a guest.

```powershell
./scripts/build-active-package.ps1
```

Produces `build/win98se-active` for the S3 ViRGE/DX (`5333:8A01`), also staged
as `build/vm-probe/ACTIVE`. The package contains the driver, the mini-VDD, the
DirectDraw HAL, the settings page, the diagnostic utilities, and the
`FIRSTBOOT.TXT`, `INSTALL.TXT` and `RECOVER.TXT` you must read before
installing.

For the conservative S3 Trio32/64 (`5333:8811`) target:

```powershell
./scripts/build-active-package.ps1 -S3Trio64 -BootTrace
```

Produces `build/win98se-trio64`. It matches only `5333:8811`, uses the shared
S3 VBE/linear-framebuffer path, and deliberately does not expose the ViRGE
DirectDraw MMIO window, the S3D engine or Direct3D.

Pass `-BuildId <id>` to stamp a specific identifier into every binary, which is
how a guest-tested build stays traceable.

## The offline transfer disk

```powershell
./scripts/build-floppy-package.ps1
```

Builds both chip packages and assembles `build/floppy` — roughly 460 KB, so it
fits one 1.44 MB floppy with room to spare. It carries `VIRGE\` and `TRIO64\`
side by side plus a root `README.TXT` written for the real-hardware case
(identifying the card, the Have Disk flow, recovery, and which `C:\V9X*.INI`
files to collect when reporting a problem).

The output is a plain directory tree on purpose: Windows 98 has no built-in
extractor, so an offline machine must be able to use the files directly. Pass
`-Zip` to also produce an archive for network transfer, and `-SkipBuild` to
assemble from packages you have already built.

The script refuses to finish if the tree exceeds the usable space on a floppy.

## Component builds

These exist for bisecting a problem down to one image. None of them is
installable on its own.

```powershell
./scripts/build-win16-skeleton.ps1       # build/win16/v9xdisp.drv - NE signature proof only
./scripts/build-win16-ddi-skeleton.ps1   # display ordinals + DIB Engine imports (needs the DDK)
./scripts/build-minivdd-skeleton.ps1     # build/minivdd32/v9xmini.vxd (needs the DDK's MASM)
```

`build-win16-skeleton.ps1` proves the compiler and linker path only: the result
is not an installable display driver because the display DDI exports and DIB
Engine contract are incomplete. `build-minivdd-skeleton.ps1` verifies the
master VDD table and installs audited legacy-VESA and Windows 98 monitor-power
callbacks; the driver advertises D0 only, because the legacy BIOS resume path
does not reliably restore the active framebuffer.

## The probe folder CD

```powershell
./scripts/prepare-vm-probe.ps1
```

Mount `build/vm-probe` as an 86Box folder CD-ROM. The installable package is
isolated under `D:\ACTIVE`; every non-installable utility and probe binary is
under `D:\PROBE`. **Never install `V9XDISP.DRV` or `V9XMINI.VXD` from
`D:\PROBE`.**

| Utility | Purpose |
|---|---|
| `D:\PROBE\V9XSTAGE.EXE` | Preferred consolidated preflight: holds the VxD open while the Win16 helper does a query-only DRV check, then reports one result. |
| `D:\PROBE\V9XSER.EXE` | COM1 smoke test. |
| `D:\PROBE\V9XVXD.EXE` | Dynamic VxD load/unload probe; needs `V9XPROBE.VXD` beside it. |
| `D:\PROBE\V9X16LD.EXE` | Loads the DRV as an inactive Win16 library and validates GDIINFO and mode handling. It never invokes the mode-setting `Enable`. |

## Serial capture

Configure 86Box COM1 as a Named Pipe server called `velocity9x-com1`, then run
on the host:

```powershell
./scripts/capture-serial-pipe.ps1
```

This is unbuffered, so it survives a guest that never reaches the desktop. It
is the single most useful artefact for a failed boot.

## Automated guest testing

These scripts drive a Windows 98 guest through an out-of-band remote agent that
is **not** part of this repository. Point `V9X_AGENT_CTL` at the agent's
`v9xctl.ps1`, or pass `-ControllerPath`:

```powershell
$env:V9X_AGENT_CTL = "<path to>\v9xctl.ps1"
./scripts/run-vm-mode-matrix.ps1
```

The runner refuses a mismatched installed DRV/VXD pair, verifies the requested
mode and the `enable-ok` trace after every reboot, runs the machine-readable
GDI test, runs palette animation and readback in every 8-bit mode, and retains
a screenshot and JSON summary per mode. Use `-Repeat 2` or higher for repeated
reliability passes.

For a device already associated with Velocity9x, update a locked DRV/VXD pair
without SetupX media prompts:

```powershell
./scripts/update-associated-driver.ps1 -Port <agent port>
```

The updater verifies the existing class binding, runs the loader preflight,
refuses to overwrite unrelated pending `WININIT.INI` work, stages both files
for atomic boot-time replacement, reboots, and byte-verifies the installation.

To take a cold backup of an 86Box profile before any of this:

```powershell
./scripts/backup-86box-profile.ps1 -ProfilePath "<path to your 86Box profile>"
```

It refuses to run while 86Box is open, copies the VHD, `86box.cfg` and NVR
directory, and writes hashes.
