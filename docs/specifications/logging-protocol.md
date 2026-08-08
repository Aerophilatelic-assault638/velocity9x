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
