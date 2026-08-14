# DirectDraw accepts `SetInfo` but reports `DDCAPS_NOHARDWARE`

Status: Open — investigation

Date: 2026-08-14

## Summary

The Velocity9x display driver registers a DirectDraw HAL through `DDHAL_SetInfo`, and DirectDraw creates the driver object, but Win98 subsequently reports `DDCAPS_NOHARDWARE` (`0x02000000`). DirectDraw operations that appeared to prove acceleration were actually completed by the software HEL fallback: live tracing shows no entries into the driver's `Blt`, `Flip`, or `WaitForVerticalBlank` callbacks.

This corrects the earlier conclusion that successful fill and flip pixel tests demonstrated HAL execution. Pixel correctness alone is insufficient because DirectDraw's HEL can produce the same result.

The Trio64 engine implementation is present but remains unproven until both callback-dispatch counters and pixel validation pass.

## Environment

- Guest: Windows 98 SE
- Emulator: 86Box
- VM: `C:\Users\michael\86Box VMs\Win98SE-Trio64`
- Display adapter: `[PCI] S3 Trio64`, PCI ID `5333:8811`, 4 MB VRAM
- Current mode: 1024 x 768 x 16 bpp
- Velocity9x guest build: `trio64-hal-pending-001`
- Remote agent: v0.5.2, host port 9871 forwarded to guest port 9869
- Last stable observation: boot counter 28, desktop and GDI test working

## Expected behavior

After the 16-bit driver publishes a valid `DDHALINFO` and callback tables:

- `IDirectDraw::GetCaps` should report hardware capabilities without `DDCAPS_NOHARDWARE`.
- `WaitForVerticalBlank`, `Blt`, and supported surface operations should enter the Velocity9x HAL callbacks.
- Supported solid fills should be executed by the S3 Trio64 engine.
- Unsupported operations should safely return the documented fallback result and remain available through HEL.
- The display settings publisher should advertise hardware acceleration only after dispatch and output are verified.

## Actual behavior

- Driver initialization and `Dd16CreateObject` occur.
- The HAL callback tables have nonzero flags and plausible flat callback addresses before `DDHAL_SetInfo`.
- A correctly sized 316-byte `DDCAPS_DX3` call succeeds but returns `dwCaps = 0x02000000` (`DDCAPS_NOHARDWARE`).
- `WaitForVerticalBlank` returns `0x80004001` (`E_NOTIMPL`) in normal and exclusive modes.
- `GetVerticalBlankStatus` can succeed, but that is runtime/HEL behavior and does not demonstrate HAL dispatch.
- Live trace counters remain at zero for `WaitForVerticalBlank`, `Blt`, `Flip`, and the other tested callbacks while the DirectDraw object is alive.
- Fill/flip pixel tests can still succeed through HEL.

## Evidence

The diagnostic probe has a `/hold` mode that keeps the DirectDraw object and HAL alive for 15 seconds. Taking a trace snapshot during that interval showed driver creation but no callback entries.

Key results:

```text
GetCapsHr=0
ReportedCaps=33554432
ReportedCapsHex=0x02000000
WaitForVerticalBlankHr=0x80004001
HAL callback entry counters=0
```

Ignored build artifacts containing the current evidence:

- `build/trio64-bringup/V9XDD-HAL-DGROUP-001.INI`
- `build/trio64-bringup/V9XSNAP-HAL-DGROUP-001.INI`
- `build/trio64-bringup/V9XDD-HAL-VERSION-001.INI`
- `build/trio64-bringup/V9XSNAP-HAL-VERSION-001.INI`
- `build/trio64-bringup/V9XDD-CAPS-DX3.INI`
- `build/trio64-bringup/V9XSNAP-HAL-TABLE-001.INI`
- `build/trio64-bringup/V9XDISPLAY.REG`
- `build/trio64-bringup/V9XCC.REG`
- `build/trio64-bringup/SETTINGS-HAL-PENDING-001.BMP`

The settings screenshot shows the framebuffer and GDI path working, with hardware acceleration deliberately left unchecked while HAL dispatch is unresolved.

## Reproduction

1. Build and deploy the current Velocity9x Trio64 driver to the Win98 VM.
2. Boot to the stable desktop at 1024 x 768 x 16 bpp.
3. Run the DirectDraw probe with `/auto /hold`.
4. During the 15-second hold, run `V9XTRACE.EXE` and capture the callback counters.
5. Inspect the probe's DX3 caps result and the trace snapshot.
6. Confirm that `dwCaps` contains `0x02000000` and that the HAL callback counters remain zero.

The probe result may be marked incomplete when snapshotted during `/hold`; that is intentional because the process has not yet left the hold interval.

## Changes and hypotheses already tested

The following changes did not remove `DDCAPS_NOHARDWARE` or cause callback dispatch:

- Added a bounded Trio64 solid-rectangle fill path for 8/16-bpp display-pitch surfaces.
- Added the vertical-blank capability.
- Added the PATCOPY ROP capability bit (`dwRops[7] = 0x00010000`).
- Corrected `DD_HAL_VERSION` from `0x0100` to the Win98 DDK value `0x00FF`.
- Changed `hInstance` from an incorrect flat DLL base to the 16-bit DGROUP selector using `SELECTOROF(&v9x_dd_shared)`.
- Removed `C1_SLOW_CARD` after introducing acceleration.
- Removed `C1_DIBENGINE` to match the accelerated S3 DDK sample.
- Mirrored `DDHALINFO`, callback tables, heap data, and the mode list into the 16-bit driver's DGROUP before `DDHAL_SetInfo`.
- Checked exported display-class and current-config registry data; no `Acceleration.Level` or `NoHardware` setting explains the result.

Pre-`SetInfo` trace records confirm:

```text
dd_callbacks.dwFlags = 0x00000332
WaitForVerticalBlank = 0xB0402309
surface_callbacks.dwFlags = 0x000003BB
Blt = 0xB04021F8
```

The map file associates those addresses with the expected functions, so the immediate problem is not an obviously null callback table.

## Current implementation safety state

The Trio64 solid-fill code uses the documented enhanced-command ports, bounded idle polling, and a narrow support gate. It attempts only 8/16-bpp display-pitch surfaces and falls back for unsupported cases. Because DirectDraw does not dispatch the callback, this code has not yet been proven against the emulated hardware.

The user-facing settings publisher currently reports `Acceleration=directdraw-hal-pending` and does not claim hardware acceleration.

## Relevant source references

- `include/velocity9x/win9x_ddraw_abi.h`
- `src/display16/dd16.c`
- `src/display16/ddi.c`
- `src/display32/ddhal.c`
- `tools/diag/ddraw_probe_win32.c`
- Windows 98 DDK: `C:\98DDK\src\display\inc\DDRAWI.H`
- Windows 98 DDK: `C:\98DDK\src\display\inc\DDRAW.H`
- Windows 98 DDK S3 sample: `C:\98DDK\src\display\mini\s3v\DDDRV.C`
- Windows 98 DDK S3 sample: `C:\98DDK\src\display\mini\s3v\CONTROL.ASM`
- Windows 98 DDK S3 sample: `C:\98DDK\src\display\mini\s3v\ENABLE.ASM`
- Local 86Box reference: `build/reference/86box/src/video/vid_s3.c`
- [S3 Trio32/Trio64 Graphics Accelerators manual](https://www.bitsavers.org/components/s3/DB014-B_Trio32_Trio64_Graphics_Accelerators_Mar1995.pdf)

The DDK confirms that `DD_HAL_VERSION` is `0x00FF`, `DDCAPS_NOHARDWARE` is `0x02000000`, the accelerated S3 sample omits `C1_SLOW_CARD` and `C1_DIBENGINE`, and its HAL data resides in DGROUP with `hInstance` set to a selector.

## Next investigation steps

1. Determine the precise condition inside the Win98 DirectDraw runtime that converts the accepted `DDHALINFO` into `DDCAPS_NOHARDWARE`.
2. Compare the complete published `DDHALINFO` byte-for-byte and field-for-field with a known-working, DDK-shaped S3 driver rather than checking only callback flags and pointers.
3. Verify `DDHAL_SetInfo` return semantics, registration order, driver-object lifetime, and whether the runtime requires a different 16-bit owner-module identity.
4. Build a minimal HAL registration containing only one callback, preferably `WaitForVerticalBlank`, to isolate which structure or capability addition triggers rejection.
5. Use the stock S3 Win98 driver in a throwaway clone as a control and record its caps, registry state, registration lifecycle, and callback behavior under the same 86Box adapter.
6. Keep acceleration unpublished until a single run proves both nonzero driver callback counters and correct framebuffer output.

## Acceptance criteria

This issue is resolved when all of the following are true:

- `GetCaps` no longer reports `DDCAPS_NOHARDWARE`.
- At least one supported DirectDraw operation increments the corresponding Velocity9x HAL callback counter.
- The same operation produces the expected pixels without relying on HEL.
- Unsupported operations still fail or fall back safely.
- Repeated GDI, mode-change, DirectDraw, and reboot tests remain stable.
- Hardware acceleration is then accurately reported in the Velocity9x display settings.
