# Phase 2 portable resource groundwork timebox

Status: active preparatory slice; hardware gate closed
Date: 2026-08-08

## Decision

Timebox the host-testable resource-discovery groundwork to two focused
development days. This slice may define and validate PCI identity, decoded BAR,
VRAM override, framebuffer-size, and capability-state contracts.

It may not perform PCI configuration writes, physical mappings, register I/O, or
install a display driver. Those actions remain gated on the Phase 1 ABI,
mini-VDD, logging transport, uninstall, and standard-VGA recovery work.

## Exit condition

- Both host compilers pass deterministic and randomized resource tests.
- The 16-bit loader still builds with the expanded shared state structures.
- Unsupported devices and malformed resources leave no usable stale binding.
- No framebuffer capability is advertised merely because a BAR looks valid.
