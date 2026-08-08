# Toolchain manifest

Status: local Phase 1 spike input  
Recorded: 2026-08-08

## Open Watcom

Installation root: `C:\WATCOM`  
Reported version: Open Watcom C/C++ 2.0 beta, 2026-07-21 04:35:21, 64-bit host tool

| File | SHA-256 |
|---|---|
| `binnt64\wcl386.exe` | `97F6C5A51C4DBF5D8FBA74B5EAFBB6111D263F6B56E2A43AD8DA888F5B99371D` |
| `binnt\wcc386.exe` | `079B4542DBD2B42C9AC09E0F0F3E5ECE7936034FBA14D3D149A318CD9A2818E6` |
| `binnt\wcc.exe` | `A205B4CA46C465B8FA54D4FAA847D4C8EC09ED412ADE753852F66A3FD0686F22` |
| `binnt\wlink.exe` | `ACB0421AE9B41A3E7BC0072369F2D7484D4371987FB3F25BB83F5F50BF6E2706` |
| `owsetenv.bat` | `ECA08566602B460BD541F111A370EA8F6BC6733CF4FE6411E4F3972B2461D6B0` |

The Open Watcom installation is an external build prerequisite and is not part
of the repository or release package. The repository build scripts locate it
from `WATCOM` and then fall back to `C:\WATCOM`.

The supplied tree includes Win16 Windows headers and libraries and can create an
NE DLL/DRV image. Open Watcom itself does not include the Win9x mini-VDD headers,
but its linker supports the Windows VxD image format. The separate external
Windows 98 DDK at `C:\98DDK` supplies the relevant Win9x interfaces and samples;
see `docs/ddk-inputs.md`. Open Watcom's `h\nt\ddk` tree still must not be mistaken
for the Windows 9x VxD DDK.

## Verified builds

- Portable host suite: compiled and linked with `wcl386`, tests pass.
- Win16 loader shell: compiled and linked with `wcc`/`wlink`; MZ and NE
  signatures, `WEP` export, and embedded build identifier verified. The first
  verified image was 1,602 bytes with SHA-256
  `297C4D8B48DD65A0B613CBBE5D0D58544B14C8DFA2EC91DFA73F8D91FC22C6B9`
  for build identifier `watcom-spike-2`.
- Win16 DDI shell: compiled with Open Watcom, linked against the external
  `DIBENG.LIB` import library, and verified as an NE image with the documented
  display and cursor export surface. Its mode-entry functions reject use. The
  verified `phase-next-final` image is 4,494 bytes with SHA-256
  `6C49ACF4ECD18D0F6086198D2D142A5652AEBB97CAEB779A561CA1BAA6E8E8D0`.
- Win9x mini-VDD shell: assembled and linked with the external Windows 98 DDK
  tools and headers, then verified as an LE image with an exported device
  descriptor and embedded build identifier. Its initialization always fails.
  The verified `phase-next-final` image is 4,668 bytes with SHA-256
  `A0E0F9A456EA131838112C3D092AEC0441D2A01E257A34A18C4EE24C6F4A06D7`.
