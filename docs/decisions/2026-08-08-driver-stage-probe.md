# Consolidated driver-stage lifecycle result

Status: accepted guest result  
Recorded: 2026-08-08

## Decision

Accept one consolidated driver-pair lifecycle test as the final inactive-loader
checkpoint before active display-driver bring-up. The next guest test must be a
recoverable, cold-backed-up 640x480x8 activation test rather than another
single-boundary loader probe.

## Test behavior

`V9XSTAGE.EXE` dynamically loads the hardware-inert `V9XPROBE.VXD`, starts the
Win16 `V9X16LD.EXE /quiet` helper, and keeps the VxD resident while that helper
loads and unloads `V9XDISP.DRV` as a library. It then waits for the Win16 result,
closes the VxD handle, and reports one combined PASS or FAIL result.

The test does not call the display driver's `Enable` entry point, install an INF,
register mini-VDD callbacks, set a display mode, or touch S3 registers.

## Evidence

The Windows 98SE guest displayed the PASS result. The live 86Box COM1 capture
contained the complete ordered sequence:

```text
V9X-STAGE begin build=driver-stage-1
V9X-VXD init build=driver-stage-1
V9X-VDD table-ok build=driver-stage-1
V9X-VXD open build=driver-stage-1
V9X-STAGE pair-loaded build=driver-stage-1
V9X-VXD close build=driver-stage-1
V9X-VXD exit build=driver-stage-1
V9X-STAGE PASS build=driver-stage-1
```

The 299-byte capture and tested files have these SHA-256 hashes:

```text
2E0BB08E9BFF4F07E4B3106BB08C086BA79950A412A408BCC3E3AC9EE941D73F  driver-stage-1.bin
7D13271AAC29B39946D9AF2B705889CC1320F8D75940460E517039BFCE6D4B82  V9XSTAGE.EXE
82F36C60B5A8910B23960BDE96EC190BF718CFC92E87062F2D078689F97E8F5E  V9X16LD.EXE
BBB4DFEDED4043B02FEE26D3585EF91E76D3E86B38F90AB37B9C8FECAF5BBE67  V9XDISP.DRV
58E9661649A4BD553E8F6566C05A30A0A33BB9B5A79157D487CC44E138E4B19F  V9XPROBE.VXD
```

## Consequence

The Win32-to-VxD and Win32-to-Win16 orchestration path is viable, and the DRV
and VxD can coexist through a complete load/unload lifecycle. This does not
clear the installable-driver gate: `V9XDISP.DRV` still rejects display enable
and `V9XMINI.VXD` still rejects initialization.

The next implementation step is the minimum active DIB Engine framebuffer path,
followed by static package audit, cold VM disk backup, installation, one boot,
serial verification, and rollback.
