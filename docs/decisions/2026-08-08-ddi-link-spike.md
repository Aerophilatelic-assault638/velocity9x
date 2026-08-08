# DDI and mini-VDD link spike

Status: accepted build-path result
Recorded: 2026-08-08

## Decision

Use Open Watcom for the 16-bit display driver and the locally installed Windows
98 DDK assembler/linker for the 32-bit mini-VDD boundary. Keep all DDK inputs
external and generate only original project sources and build artifacts.

## Evidence

- The Win16 skeleton links against the external DIB Engine import library and
  exposes the documented display, cursor, and `ValidateMode` entry points.
- The mini-VDD skeleton assembles from original source using the DDK's public
  VxD macros and links as an LE image with `V9XMINI_DDB` at ordinal 1.
- Both images embed a source/build identifier and are produced by command-line
  scripts.

The current entry points deliberately reject initialization. These artifacts
prove binary construction only; they are not installable drivers and do not
satisfy the Phase 1 guest load, logging, recovery, or cycle gates.

## Consequence

The toolchain kill-risk is reduced: both required legacy image formats can be
produced without vendoring DDK files. The next driver step is a VM-only loader
probe with serial logging, followed by the original display-mode and mini-VDD
registration behavior needed for a recoverable skeleton installation.
