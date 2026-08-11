# Matrox Millennium II physical baseline

The physical Windows 98 target at `10.0.1.170` was inventoried through the
remote agent before making any driver change. It is currently stable at
1024x768x24 with the stock Matrox driver.

## Confirmed hardware

- PCI identity: `VEN_102B&DEV_051B&SUBSYS_1200102B&REV_00`
- Windows description: Matrox Millennium II PCI
- Graphics processor: MGA-2164W
- RAMDAC: TI TVP3026
- Video memory: 8 MiB
- Active class key: `Display\0009`
- Stock display driver: `MGAPDX64.DRV`
- Stock mini-VDD: `MGAPDX64.VXD`
- Stock INF section: `MATROX~1.INF`, `MGAX_PCI`

The registry exports and desktop capture are retained under
`build/physical-mga-discovery`. They are discovery evidence, not distributable
driver inputs.

## First implementation boundary

The new backend recognizes only `102B:051B`. Millennium (`0519`) and Mystique
(`051A`) records also exist in the machine's registry, but are historical and
must not be accepted by the Millennium II backend.

The first stage is deliberately query-only:

1. strict PCI dispatch and resource validation;
2. read-only identification of BARs, VRAM, BIOS and safe register facts;
3. a boot-recoverable 640x480x8 software-framebuffer candidate;
4. expansion to 800x600 and 1024x768, then 16/24-bit modes;
5. acceleration, cursor and clock reporting only after basic modes survive
   repeated boot and GDI tests.

No Velocity9x INF is to be installed on this physical machine until a stock
driver restoration path and an automatic boot rollback have been exercised.

## Completed follow-up

The read-only Configuration Manager inventory, documented MMIO query and
two-boot recovery guard have now been exercised on the physical machine. See
`docs/specifications/matrox-millennium2-bringup.md` for the verified resource
map, query values and recovery evidence. The INF installation block remains in
force. Guarded drop-in candidates have since exercised mode entry and DIB
Engine enable, but Candidate 8 still failed the physical framebuffer/GDI
checkpoint and was automatically rolled back to the byte-identical stock
pair. See `docs/decisions/2026-08-11-millennium2-physical-candidate8.md` for
the latest physical result.
