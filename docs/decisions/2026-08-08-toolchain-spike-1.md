# Toolchain spike 1: portable core and Win16 NE image

Status: accepted  
Date: 2026-08-08

## Result

The locally installed Open Watcom 2.0 beta toolchain successfully:

- compiled, linked, and ran the 32-bit portable host test suite;
- compiled project-owned shared code with the 16-bit Windows compiler;
- linked an original 16-bit Windows NE image named `v9xdisp.drv`;
- exported `WEP` and used the toolchain's standard Win16 library entry point;
- embedded the requested project build identifier;
- produced the image without Microsoft DDK files.

The result proves the compiler and NE linker path. It does not prove the Windows
9x display DDI, DIB Engine imports, installation, mode setting, or mini-VDD path,
so the image remains a non-installable loader shell.

## Inputs

Exact tool hashes are recorded in `docs/toolchain.md`. Open Watcom remains an
external prerequisite and its headers, libraries, samples, and binaries are not
copied into the repository or release package.

## Next gate

Establish the required Win98SE display DDI exports and structure layouts from
permitted documentation, then add only the smallest safe entry-point set. The
mini-VDD LE/VxD build path remains a separate unresolved spike because the Open
Watcom installation does not include a Windows 9x VxD framework or mini-VDD
sample.
