# Phase 1 timebox and first implementation slice

Status: active; clock started 2026-08-08
Date: 2026-08-08

## Decision

Timebox the first Phase 1 spike to five focused development days once the Phase 0
ABI draft is reviewable, an Open Watcom installation is available, and a
disposable Windows 98SE standard-VGA VM is available.

The first source slice is deliberately limited to code that can be reviewed and
tested without assuming an unproven Windows 9x binary boundary:

1. portable fixed-width types and status values;
2. framebuffer layout validation;
3. the chipset backend contract;
4. fixed-size diagnostic events;
5. strict ViRGE/DX PCI matching;
6. 16-bit display and mini-VDD lifecycle shells;
7. host tests and repeatable build entry points.

## Exit or re-scope trigger

At the end of the timebox, record whether Open Watcom can create the required NE
display DRV and LE VxD using only redistributable project files plus explicitly
documented external tools. If either binary needs unredistributable input or an
unacceptable amount of handwritten ABI surface, revise the binary split before
adding mode-setting or acceleration code.

This timebox does not start the 20-cycle installation gate. That gate begins only
after the DRV, VxD, INF, serial transport, uninstall path, and standard-VGA
recovery procedure have each been reviewed.

The first guest checkpoint is a separate dynamic `V9XPROBE.VXD`. It proves
ring-0 load, Win32 open/close, unload, and bounded serial output without
registering as a mini-VDD or touching display hardware. The installable-driver
cycle gate remains closed until this probe passes in the disposable VM.
