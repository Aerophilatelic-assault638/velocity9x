# External Windows 98 DDK inputs

Status: local ABI/toolchain reference
Recorded: 2026-08-08

The Windows 98 DDK is installed at `C:\98DDK`; matching retail symbols are at
`C:\win98retailsymbols`. These directories are external, read-only project
inputs. Their files must not be copied into source control or release packages.

## Selected input hashes

| File | SHA-256 |
|---|---|
| `License.txt` | `89F90F047AA98C71A405BF75A377CA075A6DFAB3540692E0E6A65655EE2B4019` |
| `help\OTHER.CHM` | `E9C3E8EC6E5042661DBD6C04DD762238A33ECE80B295013C1F9A37CD5E283D6F` |
| `inc\win98\MINIVDD.H` | `78B562F4BEF879B0637F5CECED2A4BDA3407D5BC340DBB54264C93B3B8F6DC9E` |
| `inc\win98\MINIVDD.INC` | `14492B744EBB954156E8E8C6442DE21622711E6BD94AC0AB3781B3074F484EE6` |
| `inc\win98\inc16\VALMODE.INC` | `D54A66E426459B82A8B1A48E91594D9E962D9C6EB3FF4622BC331E6E43955AC3` |
| `lib\win98\DIBENG.LIB` | `235FA8DF800C17059880B3442035AA2C038537BB75DD458E4D2C3AF7717DF055` |
| `bin\win98\ML.EXE` | `33455C7E38348FFDA7B73EB66B818F0C00E5D8D52F08EAE665616E3A5C9C9B8B` |
| `bin\LINK.EXE` | `62A80AF374052F78C82D7E1407B662646FC80425AA38B63EA6D67EE0FF3CD259` |
| `bin\win98\ADRC2VXD.EXE` | `1AA5E83D72E222C8C8DA6509CAD92BDB0E3460DB7C9CFB5501C16590733646DA` |
| `src\display\mini\mini\MINI.DEF` | `3488CBA62912A8E4DA5C07BF39CC0941AC43342B868FA8199E04119E8B298A27` |
| `src\display\mini\mini\DIBLINK.ASM` | `47A779D0BBE3136D68284B866632F253060274AAAE7FDAFB393C06E56A9127A0` |
| `DIBENG.SYM` | `70FCF2BAAA915A8FF95BBA7FE23834C3AA49954197E496AA459358B68790719E` |
| `VDD.SYM` | `D5380AE36734E266801C50304FDE480BFB17F836CBF495021AD4F3A200961E4C` |

`DIBENG.LIB` is an import library: the build uses it to describe imports from
Windows' `DIBENG` module. It is not linked into or redistributed with the DRV.

The mini-VDD build invokes the external DDK copies of `ML.EXE` and `LINK.EXE`.
No DDK executable, header, library, sample, or symbol file is copied into the
repository or release output.

## Confirmed architecture

The DDK documentation confirms that the project needs a 16-bit display
minidriver, the system DIB Engine, a hardware-specific mini-VDD, the system VDD,
and optionally VFLATD for banked apertures. For a linear framebuffer, the
display minidriver creates and owns the selector; the mini-VDD handles VGA
virtualization and screen switching rather than mapping that selector.

The source files in the DDK are consulted only to confirm ABI names, ordinals,
and build behavior. Velocity9x source must remain independently written.
