# Hardware diagnostics contract

Velocity9x chipset backends publish read-only runtime facts to
`C:\V9XHW.INI`. The settings utility consumes this contract without direct
port or MMIO access, keeping the UI independent of S3, Matrox, 3dfx, or other
future hardware backends.

The version 1 section is `[Velocity9xHardware]`:

- `SchemaVersion`: decimal contract version, currently `1`.
- `Adapter`: human-readable model from the active backend.
- `VendorId` and `DeviceId`: four-digit hexadecimal PCI identifiers.
- `ClockDetector`: stable backend detector identifier.
- `ClockStatus`: `valid` or `unavailable`.
- `CoreClockKHz`: integer engine/core clock in kHz when valid.
- `MemoryClockKHz`: integer memory clock in kHz when valid.
- `CoreClockRelation`: `independent` or `shared-memory-clock`.

A backend must omit clock values and publish `unavailable` when it cannot
identify the relevant clock domain safely. The settings layer never substitutes
a pixel clock, BIOS default, marketing value, or device-table estimate.

For S3 ViRGE/DX, extended sequencer registers SR10 and SR11 describe MCLK. The
backend decodes the PLL using the 14.318 MHz reference clock and range-checks
the result. The ViRGE graphics engine shares MCLK, so the same detected value is
published for core and memory with `shared-memory-clock`; it is not represented
as an independently programmable core clock.
