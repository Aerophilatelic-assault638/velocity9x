# VGA hardware survey report contract

`V9XSURV.EXE` is a real-mode DOS tool given to owners of cards Velocity9x does
not support, to collect what writing a chipset backend requires. It writes one
INI report, by default `C:\V9XSURV.INI`, consumed by
`scripts/parse-vga-survey.ps1`.

This contract is separate from `hardware-diagnostics.md`. That one describes what
a *running driver* publishes about hardware it already supports; this one
describes what an *external tool* captures about hardware nothing here supports
yet.

## The governing rule

The tool captures; it does not interpret. PCI configuration space, the video BIOS
image and EDID go into the report verbatim as hex, and every decode happens
host-side. A decoding mistake is then fixed by editing a PowerShell script and
re-running it over every report already collected, rather than by shipping a new
executable to everyone who helped.

The only interpretation the tool performs is what it prints on screen so a tester
can see the run worked.

## Safety tiers

**Tier 1** always runs. It writes no device register except to select an index,
and restores every index it touches. PCI is read through PCI BIOS function
`B108h` only; the write functions `B10Bh`/`B10Ch`/`B10Dh`, VBE `4F02h` (set mode)
and VBE `4F14h` (OEM extension) do not appear in the source, and
`scripts/build-vga-survey.ps1` fails the build if they ever do. No BAR sizing is
performed, because determining an aperture size means writing to a BAR on a live
device.

**Tier 2** is opt-in, and the report records which way the tester answered. It
writes documented per-vendor unlock keys, reads the registers behind them, and
restores the originals. It is dispatched on PCI vendor ID, so an unfamiliar card
is never poked speculatively.

Tier 1 is closed on disk before Tier 2 begins, and Tier 2 reopens the file in
append mode. If a vendor probe wedges an unknown card, the tester still has a
complete Tier 1 report.

In practice a declined Tier 2 costs less on S3 parts than it looks. Captures
from both the ViRGE/DX and Trio64 test machines show CR38 and CR39 already
holding `48`/`A5` before the survey runs — the video BIOS leaves the extended
bank unlocked at POST — so the Tier 1 register dump, which unlocks nothing,
already carries CR2D/CR2E (device ID), CR30 (chip ID) and CR36 (memory size).
Do not assume this of an unfamiliar card, but do read a Tier 2-declined S3
report before asking the tester to run it again.

Implemented Tier 2 families: S3 (`5333`, CR38/CR39 and SR08 unlock) and Cirrus
Logic (`1013`, SR06 key). Every other vendor gets a section with
`Status=unsupported` and a reason — ATI because the register base would have to
come from a table inside the card's own ROM, Trident because reading SR0B
switches the register file mode, Tseng because its unlock writes the
non-indexed CGA and Hercules mode-control ports, and Matrox/nVidia/3dfx/SiS
because their registers are MMIO that real mode cannot reach.

## Formatting rules

- ASCII, one `Key=Value` per line, no spaces around `=`.
- Parsers must split on the **first** `=` only: strings extracted from a video
  BIOS can contain more.
- `;` begins a comment only at the start of a line.
- Hex is uppercase without a `0x` prefix, fixed width to the field
  (`VendorId=5333`, `Bar0=F8000008`).
- Keys ending `Bytes`, `KB`, `Count` are decimal; others are hex unless noted.
- Blobs are offset-keyed lines of contiguous hex, 16 bytes per line:
  `Config.00=3353018A83000002...`. Offsets are two hex digits for blobs up to
  256 bytes and four for the ROM image. One rule reassembles config space, the
  ROM and EDID alike.

## Status vocabulary

Every section carries a `Status`. A probe that could not run says so — it is
never silently omitted, because the parser has to distinguish "this card does not
have that" from "we never looked".

| Value | Meaning |
|---|---|
| `ok` | probed, data present |
| `unavailable` | the mechanism answered, but with nothing usable |
| `unsupported` | the call or probe does not exist on this hardware, or policy excludes it |
| `skipped` | deliberately not attempted |
| `declined` | the tester declined Tier 2 |
| `error` | the probe failed part way |

Sections carrying anything other than `ok` also carry a `Reason`.

## Sections

| Section | Contents |
|---|---|
| `[Report]` | `SchemaVersion`, `Tool`, `Build`, `Access`, `Date`, `Time`, `CommandLine`, `Note` |
| `[System]` | DOS version, `WindowsPresent`, conventional memory, coarse CPU class |
| `[BiosData]` | BIOS data area video fields, INT 10h `AH=1Ah` display combination, `AH=1Bh` functionality block |
| `[PciBios]` | INT 1Ah `B101h` presence, version, hardware mechanism, last bus |
| `[PciInventory]` | every PCI function found, one CSV line each, with a `Fields` key naming the columns |
| `[PciDevice.N]` | one per display-class device: decoded header fields plus the full 256-byte `Config.` blob |
| `[VideoBios]` | the ROM at `C000`: size, checksum, `PCIR` structure, `$PnP` header, extracted strings, and a `Rom.` blob |
| `[OptionRom.N]`, `[OptionRomScan]` | secondary adapter ROMs found between `C000` and `E000` |
| `[VBE]` | `4F00h` controller info with the `VBE2` request, OEM strings, current mode, `4F0Ah`/`4F10h`/`4F11h` capability queries |
| `[VBEModes]` | the complete mode list walked from `VideoModePtr`, one CSV line per mode, with a `Fields` key and a `Truncated` flag |
| `[EDID]` | DDC level, then `Block0.`/`Block1.` blobs |
| `[VGARegisters]` | MISC, feature control, DAC state, and `Seq.`/`Crtc.`/`Gdc.`/`Atc.` register banks, plus `Trust` |
| `[Tier1]` | marks the end of the always-safe capture |
| `[Tier2]` | `Requested`, `Decision` |
| `[Chipset.*]` | vendor probe results, or the reason there are none |
| `[Result]` | `Status`, `DisplayDeviceCount`, `Complete` |

### `Trust` on `[VGARegisters]`

`hardware` or `virtualized`. Under Windows the virtual display driver traps VGA
port I/O and returns per-VM values rather than what the silicon holds. The
capture is still recorded, but nothing about the chipset may be concluded from
it, and the parser warns.

### `Complete` on `[Result]`

`Complete=yes` is the last key the tool writes. A report without it was cut off —
by a full disk, a power cycle, or a mangled transfer — and the parser rejects it
rather than drawing conclusions from a fragment.

## Exit codes

| Code | Meaning |
|---|---|
| 0 | report written |
| 1 | report written, but it could not be reopened for the vendor probe |
| 2 | report written, no PCI display device found (an ISA or VLB card) |
| 3 | nowhere writable; nothing was produced |

## Versioning

`SchemaVersion` is `1`. Adding a section or a key does not bump it — consumers
must ignore what they do not recognise. Removing or repurposing a key does.
