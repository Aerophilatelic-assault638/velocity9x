# Matrox Millennium II bring-up boundary

Target: MGA-2164W PCI `102B:051B`. The first physical board is subsystem
`1200:102B`, revision `00`, with a TI TVP3026 RAMDAC and 8 MiB WRAM.

## Resource map

The physical Windows 98 Configuration Manager allocation and the MGA-2164W
specification agree:

| Physical range | Size | MGA name | Use |
|---|---:|---|---|
| `E0000000-E0FFFFFF` | 16 MiB | MGABASE2 | Direct framebuffer aperture |
| `E1000000-E1003FFF` | 16 KiB | MGABASE1 | Control aperture |
| `E2000000-E27FFFFF` | 8 MiB | MGABASE3 | ILOAD pseudo-DMA window |

The control aperture must never be confused with the framebuffer. Only the
installed VRAM portion of MGABASE2 is usable; this board reports 8 MiB.

## Proven read-only MMIO

The dynamic `V9XMGAQ.VXD` probe maps only the 16 KiB MGABASE1 allocation and
reads these documented registers:

- `STATUS` at `MGABASE1+1E14h`;
- `VCOUNT` at `MGABASE1+1E20h`;
- `OPMODE` at `MGABASE1+1E54h`.

Two physical samples at 1024x768x24 returned stable `STATUS=00020024h` and
`OPMODE=00000000h`; `VCOUNT` changed from `02BCh` to `00C0h`, proving that the
mapped address is live rather than a cached or unmapped page. No MMIO, I/O or
configuration write occurred.

## Recovery requirement

Physical candidates initially retain the stock filenames `MGAPDX64.DRV` and
`MGAPDX64.VXD` and do not change the `Display\0009` class association. The
validated `C:\V9XSAFE\MGA2` two-boot guard restores byte-identical stock files
before Windows loads on the second boot unless the first candidate boot is
explicitly disarmed.

The guard was validated through boot counters 3-5:

1. unarmed boot remained 1024x768x24 and left `LASTBOOT=PREPARED`;
2. first armed boot created `ARMED.2` and recorded `CANDIDATE-FIRST-BOOT`;
3. second boot recorded `ROLLED-BACK`, removed `ARMED.2`, and both restored
   files passed `FC /B` against their saved copies.

## Next candidate

Before replacing either stock file, a DOS/VBE inventory must confirm the
board's mode numbers and linear-framebuffer addresses for 640x480x8,
800x600x8 and 1024x768x8. The first display candidate is then limited to
640x480x8, DIB Engine rendering and BIOS/VBE mode entry. It must obtain the
MGABASE2 address from the physical resource contract and contain no drawing
engine, pseudo-DMA, DAC-clock or acceleration writes.
