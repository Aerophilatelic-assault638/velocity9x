# Velocity9x diagnostic record protocol

Status: version 1

The portable core emits fixed-size 32-byte records. A transport may write these
records to a serial port, a memory ring, or a host test sink without changing the
producer.

All integer fields are unsigned and little-endian on the supported x86 targets.

| Offset | Size | Field |
|---:|---:|---|
| 0 | 4 | magic (`V9XL`, `0x4c583956`) |
| 4 | 2 | protocol version (`1`) |
| 6 | 2 | record size (`32`) |
| 8 | 4 | monotonically increasing sequence |
| 12 | 2 | event identifier |
| 14 | 2 | project status value |
| 16 | 4 | argument 0 |
| 20 | 4 | argument 1 |
| 24 | 4 | argument 2 |
| 28 | 4 | argument 3 |

Sequence wrap is allowed. No event contains pointers, compiler-dependent enums,
or variable-length data. Transports must never wait indefinitely. A failed or
absent logger must not prevent driver initialization or recovery.

## Transport smoke test

`tools/diag/serial_smoke.c` is a DOS utility for verifying the VM's COM1 host
transport before any driver is loaded. It programs the COM1 8250-compatible
UART directly for 9600 8N1 and sends one ASCII line beginning
`V9X-SERIAL-SMOKE`. Direct UART output avoids BIOS INT 14h modem-status waits,
which can time out with a file-backed serial device even when its transmit path
is usable. Every transmit-register wait remains bounded.

That line is not a version-1 binary diagnostic record. It is an explicit
transport-only probe and must not be accepted by a binary record decoder.
