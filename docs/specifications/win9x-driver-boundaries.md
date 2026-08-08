# Windows 9x driver boundary specification (working draft)

Status: DDK-backed ABI baseline; implementation incomplete
Target: Windows 98SE, S3 ViRGE/DX 86C375

## Proven project-owned boundary

The portable core owns validation, capability policy, diagnostic record layout,
and chipset dispatch. It accepts flat scalar values and project-owned structs;
it does not include Windows or DDK headers. Code shared with a 16-bit component
must remain C89-compatible and must not assume that `int` or pointers are 32
bits.

The first hardware backend recognizes only PCI vendor `5333` and device `8A01`.
Recognition alone grants no capability. Resources, aperture sizes, VRAM, and a
mode must be validated before the framebuffer capability can be enabled.

## 16-bit display DRV boundary

The display component will own the Windows display DDI entry points and DIB
Engine interoperation that empirical testing proves cannot live elsewhere. It
will translate segmented pointers and Windows-owned structures into the small
project-owned contracts in `include/velocity9x`.

The Windows 98 DDK establishes these requirements:

- `Enable` is ordinal 5 and is called for both `GDIINFO` and `PDEVICE` setup;
- `Disable` is ordinal 4 and releases selectors and allocations;
- `ReEnable` is ordinal 31 and handles dynamic mode changes;
- `ValidateMode` is exported by name, conventionally at ordinal 700, and must
  validate without changing the screen;
- drawing entry points occupy ordinals 1 through 30 and may forward to DIBENG;
- cursor entry points occupy ordinals 101 through 104;
- `ResetHiResMode` is a fixed-segment callback registered with the system VDD;
- a linear framebuffer selector is created by the 16-bit display minidriver.

Still unresolved before an NE image may be called installable:

- initialization and teardown ordering;
- complete `GDIINFO`, `DIBENGINE`, and selector construction;
- VDD registration and full-screen transition behavior;
- the legal and technical provenance of every header, import library, and tool.

The source under `src/display16` is therefore a lifecycle shell, not a complete
Windows display DDI implementation.

## 32-bit mini-VDD boundary

The mini-VDD will own only responsibilities established by the Windows 98SE
mini-VDD contract and the spike. Candidate responsibilities are adapter/resource
discovery, screen-state transitions, mappings, engine synchronization, logging,
and recovery. The final split must reflect observed DDI constraints rather than
the preferred architecture in the plan.

Unresolved before an LE image may be called loadable:

- device descriptor block and loader entry format;
- required mini-VDD services and dispatch ordinals;
- interaction with the system VGA VDD and display DRV;
- locked/pageable segment requirements;
- port I/O, mapping, and ring/context restrictions;
- required DDK tools, headers, libraries, and redistribution constraints.

The source under `src/minivdd32` is a testable service-state shell. It deliberately
does not include a guessed VxD loader thunk.

## DirectDraw boundary

No DirectDraw callbacks or capabilities are present in Phase 1. DirectDraw work
begins only after framebuffer mode stability. Its capability table must be
derived from operations that have passed conformance tests.

## Failure rules

- Unknown PCI IDs are rejected before hardware access.
- Integer overflow or insufficient VRAM rejects a mode.
- Optional capability bits begin clear.
- Every future hardware wait must be bounded.
- A driver-side failure must prefer an operable software/standard-VGA path.
