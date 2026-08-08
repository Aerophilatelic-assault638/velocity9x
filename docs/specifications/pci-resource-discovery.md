# PCI and framebuffer resource discovery contract

Status: Phase 2 portable contract, version 1
Target: S3 ViRGE/DX `5333:8A01`

## Boundary

The OS-facing driver layer obtains the PCI identity and assigned BAR resources.
The portable core validates decoded scalar values; it does not enumerate the PCI
bus, write BAR sizing probes, map physical memory, or perform port I/O.

The first backend accepts only vendor `5333` and device `8A01`. An unsupported
identity is rejected before resources are inspected or hardware is touched.

## Framebuffer candidate

The discovery layer supplies a candidate BAR with:

- a non-zero 32-bit physical base;
- a non-zero, power-of-two aperture size;
- natural alignment of the base to the aperture size;
- a memory-resource flag and no I/O or 64-bit-resource flag;
- no unknown flag bits.

The highest aperture byte must fit in the 32-bit physical address space. A valid
binding records the base, aperture size, selected VRAM size, and whether an
override was used. It does not mean the aperture has been mapped or accessed.

## VRAM size and override

The selected VRAM size is the explicit override when non-zero, otherwise the
detected size. It must be non-zero and no larger than the assigned aperture. An
override may recover from an unavailable or implausible detected value, but it
cannot expand beyond the resource Windows assigned.

Chipset-specific probing of the actual ViRGE/DX memory configuration remains an
OS/hardware-layer task and requires a separate register-level specification.

## Capability policy

Successful PCI matching sets only `initialized`. Successful structural resource
validation additionally sets `resources_bound`. Neither operation advertises
`V9X_CAP_LINEAR_FRAMEBUFFER`; that bit remains clear until mapping and guarded
read/write tests pass in the disposable Windows 98SE VM.

Any failed rebind clears the previous framebuffer binding and all capability
bits so stale resources cannot remain usable.
