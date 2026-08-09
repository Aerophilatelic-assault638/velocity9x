# 800x600 boot fallback investigation

Status: open; pause mode-programming changes pending Windows 98 configuration
diagnosis. The `trace3` diagnostic rebuild (trace-only, adds the `libmain`
stage) is staged in `D:\ACTIVE`.  
Recorded: 2026-08-09; updated 2026-08-09 after host-side review fixes

## Summary

The current diagnostic package does not become the active Windows 98 display
driver during boot. Windows reaches the desktop in its 640x480x16-colour
troubleshooting fallback instead of the forced 800x600x8 mode. After deleting
the previous trace file and performing a full shutdown and cold boot,
`C:\V9XBOOT.INI` was not created.

This is currently a pre-entry/install-selection problem, not evidence that the
800x600 hardware-mode code failed. The same traced `V9XDISP.DRV`, when loaded
manually with `V9X16LD.EXE`, completes its DIB Engine inquiry and six-mode
validation and writes `Stage=query-ok` to `C:\V9XBOOT.INI`.

Do not make further mode-programming changes until the active Windows display
class and hardware-profile bindings have been identified.

## Current diagnostic package

- Build ID: `diag-force-800x600x8-trace3`
- Staged guest directory: `D:\ACTIVE`
- Host directory: `C:\everything\velocity9x\build\vm-probe\ACTIVE`
- `V9XDISP.DRV` SHA-256:
  `937958C5952380ABF972D70E46241DDBB3FF16699D3413DFE6BDEA1DEB5847DE`
- Compile-time forced mode index: `1`, corresponding to 800x600x8
- Boot tracing: enabled; expected output is `C:\V9XBOOT.INI`
- New in `trace3`: `LibMain` writes `Stage=libmain` to `C:\V9XBOOT.INI` the
  moment the DRV is loaded, before Windows decides whether to call `Enable`
- Dynamic in-session mode switching: disabled

The superseded `trace2` DRV (SHA-256
`571F2AA3B52C30D80F03B0E61B295AEFC9D04C2E304F8DB16A661D497631C30E`) differed
only in lacking the `libmain` stage. `trace3` contains no mode-programming
changes; the driver-change moratorium below applies to mode programming, and
this trace-only rebuild was made to close the load-versus-query ambiguity.

The forced diagnostic selection bypasses the registry-selected resolution once
the display driver's inquiry code is entered. Therefore a boot that loads this
exact DRV should at least create a trace stage, even if hardware enable later
fails.

## Proven baseline

The preserved `active-640-vdd1` milestone successfully completed two guest
passes at 640x480x8:

- the display DRV and mini-VDD loaded together and unloaded cleanly;
- the GDI framebuffer test passed twice;
- software-cursor behaviour passed twice;
- Standard VGA rollback worked.

It is archived at:

`C:\everything\velocity9x\build\milestones\active-640-vdd1`

A cold VM backup predating activation is available at:

`C:\Users\michael\86Box VMs\Velocity9x Backups\Win86SE-pre-velocity9x-20260809-080027`

## Failure chronology

1. The six-mode `phase3-matrix-v1` package booted at 640x480x8 but caused a
   `DESK.CPL` error and an Explorer invalid page fault in `GDI32.DLL`.
2. Preserving full ESI/EDI across the VDD configuration call and removing the
   duplicate call produced `phase3-matrix-v2`. It booted stably at 640x480x8;
   Display Properties, Explorer, and the GDI test worked.
3. Selecting 800x600x8 caused Windows to fall back to 640x480x16 and report
   that the adapter type or current settings did not work with the hardware.
4. Zeroing EDX in the VDD configuration call did not change the fallback.
5. A driver compiled to force 800x600x8 was produced to bypass registry mode
   selection. The guest still entered the 640x480x16 fallback.
6. The 86Box folder CD was found to cache stale content unless ejected and
   reinserted. Subsequent work used the refreshed `trace2` package.
7. Manual execution of `D:\ACTIVE\V9X16LD.EXE` passed and created:

   ```ini
   [Velocity9x]
   Stage=query-ok
   ```

8. After a clean deletion of `C:\V9XBOOT.INI`, a full shutdown, and another
   boot, Windows again used 640x480x16 and did not recreate the file.

## Registry evidence

The display class is:

`HKEY_LOCAL_MACHINE\System\CurrentControlSet\Services\Class\Display`

Two instances are present:

- `0000` is a stale/unrelated Matrox configuration. Its `DEFAULT` values name
  `mgapdx64.drv` and `mgapdx64.vxd`.
- `0001` is Velocity9x. Its parent values include the matching PCI ID
  `PCI\VEN_5333&DEV_8A01`, `InfSection=Velocity9x.Install`, and the Velocity9x
  driver description.

`0001\DEFAULT` contains the intended configuration:

```text
CHIPID        = 00 00 00 00 00 00
drv           = v9xdisp.drv
drv2          = v9xdisp.drv
ExtModeSwitch = 0
minivdd       = v9xmini.vxd
Mode          = 8,800,600
RefreshRate   = 0
vdd           = *vdd,*vflatd
```

Windows did not create `0001\CURRENT` during driver installation, even though
the generated INF contains explicit `HKR,CURRENT` values. A diagnostic import
then created `0001\CURRENT` with values matching `DEFAULT`. The import file is
`tools/diag/V9XCUR1.REG`; it is deliberately specific to this lab VM's confirmed
display instance.

Two anomalies in this data are worth recording:

- The observed `0001\DEFAULT` value `RefreshRate = 0` does not match the
  generated INF, which writes `HKR,DEFAULT,RefreshRate,,-1`. Either the value
  above was transcribed loosely, or the observed `DEFAULT` key was not written
  verbatim by the current INF — the latter would itself be evidence of a stale
  or partial installation.
- The INF's `[Velocity9x.Previous]` section performs `DelReg` on `HKR,CURRENT`
  before the `AddReg` section runs. A setupx DelReg/AddReg ordering quirk is a
  plausible mechanism for the missing `CURRENT` key, independent of the boot
  failure.

The next cold boot still fell back and produced no trace. Therefore the absent
`CURRENT` key was a real installation anomaly but was not the sole cause of the
boot failure.

## What the evidence establishes

Established:

- the staged traced DRV can be loaded manually;
- its DIB Engine inquiry and all six mode validations succeed;
- its trace-file mechanism works at the desktop;
- `Display\0001` is the Velocity9x software instance;
- both `0001\DEFAULT` and the manually added `0001\CURRENT` request
  `v9xdisp.drv`, `v9xmini.vxd`, and 800x600x8;
- the cold boot does not reach a trace point in the traced DRV, or does not use
  that exact traced copy of the DRV.

A designed limitation of the `trace2` INI trace: its earliest stage write
(`query-start`) sat inside the `Enable` entry point in
`src/display16/ddi.c`; `LibMain` wrote nothing to `C:\V9XBOOT.INI`, so the
absence of the file could not distinguish "DRV never loaded" from "DRV loaded
but Windows never called `Enable`". `trace3` closes this: `LibMain` now writes
`Stage=libmain` immediately on load. After a `trace3` boot, an absent
`C:\V9XBOOT.INI` means the DRV was never loaded; `Stage=libmain` alone means
it was loaded but never asked for its inquiry.

The COM1 serial line `V9X-DRV load build=<id>` emitted by `LibMain` is **not**
usable for this purpose: review of the existing boot captures in
`build/vm-logs` shows it has never appeared in any boot capture, including the
two successful `active-640-vdd1` boots where the DRV demonstrably loaded and
drove the desktop. Ring-3 port writes from the DRV evidently do not reach the
host serial log at boot; only the ring-0 mini-VDD lines do.

The existing captures also contain unused evidence: on the failed forced-800
boots (`com1-diag-force-800x600x8-v1.bin`, `...-trace1.bin`), the mini-VDD
from the then-installed package printed `V9X-MINI init` and
`V9X-MINI defaults-ok`. Windows therefore read the Velocity9x registry
configuration far enough to load `v9xmini.vxd` at boot even while falling back
to 640x480x16 for the display driver. The failure is specific to loading or
using the display DRV, not a wholesale rejection of the device or its class
configuration. No capture was recorded for the `trace2` boot.

Not yet established:

- that `C:\WINDOWS\SYSTEM\V9XDISP.DRV` is byte-identical to the refreshed
  `D:\ACTIVE\V9XDISP.DRV` after the latest installation;
- that the active S3 PCI enumeration node binds to `Display\0001` rather than
  the stale `Display\0000` instance;
- which display instance and mode the active hardware profile selects;
- whether Windows rejects the device during Config Manager/PnP processing
  before it asks the display DRV to perform its inquiry;
- whether boot logging records a missing file, failed load, or configuration
  rejection.

## Recommended next checks

Record every output before changing anything. The only permitted change is
the `trace3` installation at the end of check 1; do not change Display
Properties between checks.

### 1. Verify the installed DRV bytes

`D:\ACTIVE` now holds the `trace3` package, and the superseded `trace2`
binaries no longer exist on the host, so the installed files are expected to
mismatch `D:\ACTIVE` until `trace3` is installed. Run the comparisons in this
order, before overwriting anything, so the evidence of what was actually
installed is preserved:

```bat
FC /B C:\WINDOWS\SYSTEM\V9XDISP.DRV D:\PROBE\V9XDISP.DRV
FC /B C:\WINDOWS\SYSTEM\V9XMINI.VXD D:\PROBE\V9XMINI.VXD
FC /B C:\WINDOWS\SYSTEM\V9XDISP.DRV D:\ACTIVE\V9XDISP.DRV
FC /B C:\WINDOWS\SYSTEM\V9XMINI.VXD D:\ACTIVE\V9XMINI.VXD
```

Record each result. "No differences" against `D:\PROBE` means the guest had
the deliberately-failing probe binary installed and the entire boot failure is
explained. Also record the installed files' sizes and dates
(`DIR C:\WINDOWS\SYSTEM\V9X*.*`). Then install the `trace3` package from
`D:\ACTIVE`, re-run the two `D:\ACTIVE` comparisons, and confirm "no
differences" before performing the diagnostic boot in check 2.

This check has a known concrete failure mode: until 2026-08-09 the folder-CD
root `D:\` carried a *different*, same-named `V9XDISP.DRV`/`V9XMINI.VXD` pair
placed there by `scripts/prepare-vm-probe.ps1` (DRV SHA-256 prefix
`BBB4DFED`). That probe build deliberately fails initialization and must never
be installed. If any past (re)install was pointed at `D:\` instead of
`D:\ACTIVE`, the guest holds the wrong binary and the silent fallback with no
trace is fully explained; the `FC /B` commands above would catch this. The
probe bundle has since been relocated to `D:\PROBE` and the script updated, so
the hazard cannot recur, but an installation that predates the move remains a
candidate cause.

### 2. Boot the `trace3` package once and read both boot signals

With `trace3` installed and byte-verified (check 1), delete any stale
`C:\V9XBOOT.INI`, configure 86Box to log COM1 to a host file, and perform one
cold boot. Two independent signals result:

COM1 serial (mini-VDD only — the DRV's own serial line has never been
observable at boot; see the limitation discussion above):

```text
V9X-MINI init build=diag-force-800x600x8-trace3
V9X-MINI defaults-ok callbacks=0 build=diag-force-800x600x8-trace3
```

- Lines present with the `trace3` build ID: the installed mini-VDD is current
  and Windows is reading the Velocity9x registry configuration at boot.
- Lines present with an older build ID: the installed `V9XMINI.VXD` is stale —
  corroborates a check-1 mismatch.
- No lines: the mini-VDD is not loading — points at Config Manager/PnP
  enumeration binding (check 3).

`C:\V9XBOOT.INI` (new `libmain` stage):

- File absent: Windows never loaded `V9XDISP.DRV` — driver/class selection
  problem; proceed to checks 3-5.
- `Stage=libmain` only: the DRV loaded but Windows never called `Enable` —
  rejection happens during display configuration after load.
- `Stage=query-ok` or later: the inquiry ran; the investigation moves past
  pre-entry and into hardware enable.

This combination directly resolves the disjunction stated above, so it should
be run before the registry checks.

### 3. Identify the S3 PCI hardware-enumeration binding

In Registry Editor, locate the S3 node beneath:

`HKEY_LOCAL_MACHINE\Enum\PCI`

Find the device whose hardware ID contains `VEN_5333&DEV_8A01`. Record or export
the complete device-instance key, especially these values when present:

- `Driver` (expected to identify `Display\0001`);
- `Class` and `ClassGUID`;
- `ConfigFlags`;
- `Problem` and `StatusFlags`;
- `HardwareID` and `CompatibleIDs`.

If `Driver` points to `Display\0000`, an absent instance, or anything other than
`Display\0001`, that binding takes priority over editing `0001\CURRENT`.

### 4. Inspect the active hardware-profile display settings

Export or photograph these keys if present:

```text
HKEY_CURRENT_CONFIG\Display\Settings
HKEY_LOCAL_MACHINE\Config\0001\Display\Settings
```

Record `BitsPerPixel`, `Resolution`, `RefreshRate`, and any adapter/device
selection values. Do not edit them yet.

### 5. Inspect the legacy display-loader setting

At a DOS prompt run:

```bat
FIND /I "display.drv" C:\WINDOWS\SYSTEM.INI
```

Record the exact result. On a PnP-managed Windows 98 installation this may name
the PnP display loader rather than the final vendor DRV; the result is evidence,
not an instruction to edit `SYSTEM.INI`.

### 6. Collect boot-loader evidence

If normal boot logging can be enabled without entering Safe Mode, retain
`C:\BOOTLOG.TXT` from one failed cold boot and search it for:

```text
V9XDISP
V9XMINI
VDD
DISPLAY
```

Do not enable Safe Mode solely for this investigation because remote access
depends on a normal Windows boot.

## Working diagnosis

The leading diagnosis is that Windows 98 is selecting or rejecting the display
device before entering the traced Velocity9x DRV. The existing COM1 captures
already narrow this: the mini-VDD loads even on failed boots, so Windows reads
the Velocity9x configuration but does not load (or does not use) the display
DRV. The most useful discriminators are now the exact installed-file
comparison (which also rules out the pre-move decoy probe binaries), one
`trace3` cold boot read for the `Stage=libmain` marker, and the S3 PCI
enumeration node's `Driver` binding. If all three are clean, hardware-profile
and boot-log evidence should be collected before modifying the driver again.

The 800x600 implementation itself remains unproven in the guest: the present
failure occurs too early to count as either a pass or a hardware-mode failure.
