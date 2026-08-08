# Phase 2 portable resource groundwork timebox

Status: preparatory slice completed; superseded by active bring-up candidate
Date: 2026-08-08

## Decision

Timebox the host-testable resource-discovery groundwork to two focused
development days. This slice may define and validate PCI identity, decoded BAR,
VRAM override, framebuffer-size, and capability-state contracts.

This restriction governed the portable resource slice. The subsequent Phase 1
driver-pair lifecycle and VDD table probes passed, so the separately recorded
active 640x480x8 candidate may now perform a read-only PCI BIOS lookup, firmware
mode entry, aperture mapping, and palette I/O. Guest installation remains gated
on a cold VM disk/NVR backup and explicit recovery test.

## Exit condition

- Both host compilers pass deterministic and randomized resource tests.
- The 16-bit loader still builds with the expanded shared state structures.
- Unsupported devices and malformed resources leave no usable stale binding.
- No framebuffer capability is advertised merely because a BAR looks valid.
