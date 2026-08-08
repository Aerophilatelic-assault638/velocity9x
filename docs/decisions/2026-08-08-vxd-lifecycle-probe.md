# Dynamic VxD lifecycle probe result

Status: accepted guest result
Recorded: 2026-08-08

## Decision

The Windows 98 dynamic-VxD load and unload boundary is viable. Keep the probe
separate from the mini-VDD until VDD registration and recovery behavior are
implemented and reviewed.

## Evidence

The runtime-free Win32 utility dynamically loaded `V9XPROBE.VXD` from the VM
probe folder, received a successful Win32 control-channel open, closed its
handle, and unloaded the VxD. A live 86Box named-pipe capture received these
four ring-0 messages in order:

```text
V9X-VXD init build=vxd-life-1
V9X-VXD open build=vxd-life-1
V9X-VXD close build=vxd-life-1
V9X-VXD exit build=vxd-life-1
```

The 125-byte capture has SHA-256
`731A39E9EB64BD091B1C32F73458D5C3DC1AB52EF3B9C7B0B34D2B678FEA2B2A`.
The tested VxD has SHA-256
`1200A8BB419ED2F7B57C38152442752D459B42AD9949AD51625954D7CE2D5473`;
the loader utility has SHA-256
`00323D4E9B1C1314042CF96FEF000906F6C8C5C1782003F2786C5FD68F49C6A7`.

The probe registers no mini-VDD callbacks and touches no display registers. Its
only ring-0 side effect is bounded COM1 UART output. The desktop remained
operable after unload.

## Consequence

Phase 1 may proceed to the smallest recoverable system-VDD registration probe.
The installable-driver and 20-cycle gates remain closed: `V9XMINI.VXD` still
rejects initialization, and `V9XDISP.DRV` still rejects display enable.
