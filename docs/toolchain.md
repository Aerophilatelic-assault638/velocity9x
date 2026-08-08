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
NE DLL/DRV image. No Windows 9x VxD framework, mini-VDD headers, or relevant VxD
sample was found in the installation. The installed `h\nt\ddk` tree targets the
Windows NT driver model and must not be mistaken for the Windows 9x VxD DDK.

## Verified builds

- Portable host suite: compiled and linked with `wcl386`, tests pass.
- Win16 loader shell: compiled and linked with `wcc`/`wlink`; MZ and NE
  signatures, `WEP` export, and embedded build identifier verified. The first
  verified image was 1,602 bytes with SHA-256
  `297C4D8B48DD65A0B613CBBE5D0D58544B14C8DFA2EC91DFA73F8D91FC22C6B9`
  for build identifier `watcom-spike-2`.
- Win9x mini-VDD LE image: not yet supported; external ABI/tool requirements are
  still under investigation.
