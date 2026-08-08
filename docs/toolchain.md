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
- Win16 DDI path: compiled with Open Watcom and linked against the external
  `DIBENG.LIB` import library, and verified as an NE image with the documented
  display and cursor export surface. The historical `phase-next-final` shell
  rejected mode entry. The current host-audited `active-640-vdd1` candidate is
  8,102 bytes with SHA-256
  `D5C80873083552AC2856B926E33F1221838DAE5460FA216A479928A9A9CF85AB`.
  It includes the master-VDD registration/unregistration handoff; guest
  activation remains pending.
- Win9x mini-VDD path: assembled and linked with the external Windows 98 DDK
  tools and headers, then verified as an LE image with an exported device
  descriptor and embedded build identifier. The historical shell rejected
  initialization. The `active-640-vdd1` candidate validates the master VDD table,
  installs zero callbacks, and is 4,748 bytes with SHA-256
  `5D4910145EEA0713AEC9FC69C948CA7E530BB819559BBA71A77C8A42A989F0FE`.
- Windows 98 settings/status utility: linked without a C runtime and audited to
  import only ANSI-era APIs from `KERNEL32`, `USER32`, `GDI32`, and `SHELL32`.
  The `active-640-vdd1` image is 5,632 bytes with SHA-256
  `750C363105282C391B50800BCEBD5564569554B921538BE5D0D35E2F8E0455B0`.
- Windows 98 GDI framebuffer smoke test: runtime-free PE image importing only
  ANSI-era APIs from `KERNEL32`, `USER32`, and `GDI32`. It exercises on-screen
  fills, lines, text, BitBlt, StretchBlt, SetPixel, and tolerant GetPixel
  readback. The `active-640-vdd1` image is 4,608 bytes with SHA-256
  `7CE0BFE1AB92C13AFF0DC6C8ACB1824095D466E09C78C8C4F8500DD4BA530276`.
