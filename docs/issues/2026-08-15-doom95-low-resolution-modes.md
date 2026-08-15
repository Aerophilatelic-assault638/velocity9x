# Doom95 renders half-width in garbage colours

**Status:** Diagnosed. Partially fixed; the remaining part is blocked in
`ddraw.dll`, not in this driver.

**Target:** S3 ViRGE/DX (86Box VM on port 9869), driver build `lowres-003`.

## Symptom

Doom95 draws recognisable geometry - the level, the menu, the shareware
order screen are all legible - but the picture occupies only the left 320
columns of a 640-wide screen while filling the full 400 lines, and every
colour is wrong.

## Root cause

That signature is 8-bit data written into a 16-bit surface at the surface's
own pitch. Doom95 writes one index byte per pixel; the primary has two bytes
per pixel, so a 640-byte row fills 320 RGB565 pixels and each displayed pixel
is a pair of unrelated palette indices. Rows still advance by the full pitch,
which is why the vertical geometry stays correct.

Doom95 was configured for **640x400** (Advanced Options -> Screen Resolution).
The driver published only 640x480, 800x600 and 1024x768 at 8 and 16 bpp, so
`SetDisplayMode(640, 400, 8)` failed, Doom kept the 640x480x16 desktop mode
and drew into it anyway.

Setting Doom95 to 640x480 renders it perfectly - correct palette, correct
geometry, full screen. That is the proof the diagnosis is right, and the
workaround available today.

## What was ruled out

- **A refused live colour-depth change.** This was the first hypothesis and it
  was wrong. Live depth switching was implemented and verified separately (see
  the CHANGELOG entry for `livedepth-001`), and Doom95 still rendered exactly
  the same afterwards.
- **A format-converting Blt going through the hardware path.** `v9x_copy_rect_valid`
  already rejects a surface whose pitch is too small for the display's
  bytes-per-pixel, and the driver does not advertise `DDCAPS_CANBLTSYSMEM`.
- **Palettized 8 bpp being broken.** `V9XDDP /pal8` at 640x480 returns a true
  8-bpp primary, pitch 640, `CreatePalette` and `SetPalette` both `S_OK`, and a
  written index reads back unchanged.

## What was fixed

640x400x8 was added as VBE mode `100h`:

- `src/display16/ddi.c` - mode table entry
- `src/display32/ddhal.c` - `v9x_fill_modes`
- `include/velocity9x/win9x_ddraw_abi.h` - `V9X_DD_MODE_COUNT` 6 -> 7, ABI stamp
- `packaging/win98se/velocity9x.inf` - `MODES\8\640,400`

Verified on the guest: `V9XMSW /set:640x400x8` succeeds, the desktop renders
correctly at 640x400x8 with a correct palette, and GDI's `EnumDisplaySettings`
lists the mode.

## What is still broken

**DirectDraw refuses 640x400 no matter what the driver publishes.**

`V9XDDP /pal8` dumps both mode lists. The driver's entry reaches DirectDraw -
the `DDRAWI_DIRECTDRAW_GBL` table holds 640x400x8 at index 3 - but
`EnumDisplayModes` omits it and `SetDisplayMode(640, 400, 8)` returns
`DDERR_INVALIDMODE` (`0x88760078`).

Every mode DirectDraw hides is under 480 lines. Adding `DDSCL_ALLOWMODEX` to
the probe's cooperative level makes DirectDraw expose its own emulated
320x200 and 320x240, but **not** our 640x400: `ddraw.dll` appears to admit
sub-480-line modes only from its built-in ModeX set. Nothing the driver
publishes changes that.

**The low-resolution path itself does not work.** With `ALLOWMODEX`,
`SetDisplayMode(320, 240, 8)` returns `S_OK` and yields a 320x240x8 primary
with pitch 320, palette attached and `Lock` succeeding - but the probe dies
during that test (`Result=INCOMPLETE`, no `Pal8_320_200_*` keys were ever
written) and Doom95 at 320x200 crashes with an access violation reading
`ffffffff`. The API reports success while the surface is not actually usable.

## Next steps

1. Make the 320x200/320x240 ModeX path genuinely work, or decline those modes
   outright so applications fall back instead of crashing. Declining is the
   safer of the two and is the smaller change.
2. Decide whether to keep 640x400. It is a working GDI/desktop mode and real
   S3 Win9x drivers list it, but it does not help DirectDraw, so it buys
   nothing for games today.
