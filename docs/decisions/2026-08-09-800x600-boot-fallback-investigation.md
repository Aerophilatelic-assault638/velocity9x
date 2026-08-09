# 800x600 boot fallback investigation

Status: open; pause driver changes pending Windows 98 configuration diagnosis  
Recorded: 2026-08-09

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

- Build ID: `diag-force-800x600x8-trace2`
- Staged guest directory: `D:\ACTIVE`
- Host directory: `C:\everything\velocity9x\build\vm-probe\ACTIVE`
- `V9XDISP.DRV` SHA-256:
  `571F2AA3B52C30D80F03B0E61B295AEFC9D04C2E304F8DB16A661D497631C30E`
- Compile-time forced mode index: `1`, corresponding to 800x600x8
- Boot tracing: enabled; expected output is `C:\V9XBOOT.INI`
- Dynamic in-session mode switching: disabled

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

Perform read-only checks first and retain the outputs. Do not reinstall the
driver or change Display Properties between checks.

### 1. Verify the installed DRV bytes

After refreshing the folder CD, run:

```bat
FC /B C:\WINDOWS\SYSTEM\V9XDISP.DRV D:\ACTIVE\V9XDISP.DRV
FC /B C:\WINDOWS\SYSTEM\V9XMINI.VXD D:\ACTIVE\V9XMINI.VXD
```

Record whether each command reports no differences, a size difference, or a
missing file. A mismatch would explain the missing boot trace without requiring
any further registry theory.

### 2. Identify the S3 PCI hardware-enumeration binding

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

### 3. Inspect the active hardware-profile display settings

Export or photograph these keys if present:

```text
HKEY_CURRENT_CONFIG\Display\Settings
HKEY_LOCAL_MACHINE\Config\0001\Display\Settings
```

Record `BitsPerPixel`, `Resolution`, `RefreshRate`, and any adapter/device
selection values. Do not edit them yet.

### 4. Inspect the legacy display-loader setting

At a DOS prompt run:

```bat
FIND /I "display.drv" C:\WINDOWS\SYSTEM.INI
```

Record the exact result. On a PnP-managed Windows 98 installation this may name
the PnP display loader rather than the final vendor DRV; the result is evidence,
not an instruction to edit `SYSTEM.INI`.

### 5. Collect boot-loader evidence

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
device before entering the traced Velocity9x DRV. The most useful discriminator
is now the combination of an exact installed-file comparison and the S3 PCI
enumeration node's `Driver` binding. If both are correct, hardware-profile and
boot-log evidence should be collected before modifying the driver again.

The 800x600 implementation itself remains unproven in the guest: the present
failure occurs too early to count as either a pass or a hardware-mode failure.
