# Open-Source Windows 9x VGA Driver Discussion

## User

Are there any open source drivers for mid/late 90s vga cards for windows 9x?

## Assistant

Yes, but with an important distinction: **there are open-source/free graphics-driver projects from that era, but very few are actually complete Windows 95/98 accelerated display drivers for physical mid/late-90s cards.**

The interesting projects are:

- **FreeBE/AF** — genuinely open source, with source available. It implements VBE/AF 2.0 drivers for various 1990s graphics chipsets and can expose things such as linear framebuffers, hardware scrolling, page flipping and, on some cards, **2D hardware acceleration**. However, it's fundamentally a VBE/AF driver interface rather than a native Windows 9x `DISPLAY.DRV`/mini-VDD replacement.
- **VBEMP / Universal VBE 9x Display Driver** — probably the closest thing to a universal replacement Windows 9x driver. `VBEMP.DRV` is a real Win9x display driver and supports Windows 95/98/98SE/ME using VESA VBE. It can therefore run many physical cards that have a usable VBE BIOS. However, **it isn't open source in the conventional sense**; its licence is freeware/non-commercial and restrictive.
- **SciTech Display Doctor / SNAP** — technologically this is the really interesting one. SciTech developed a common accelerated driver architecture covering a huge range of chips including **S3, Cirrus, Tseng, Matrox, ATI, NVIDIA, 3dfx, Trident, SiS, Rendition, Alliance, Number Nine, Oak, Weitek**, etc. Display Doctor 6.5 even contained a replacement Windows 95+ video driver. Unfortunately, despite plans to open-source SNAP, ownership changes meant that did not happen in the straightforward way originally proposed.

### For the kind of 1995–1999 hardware you're probably thinking about

| GPU | Open-source low-level driver code? | Open-source native Win9x accelerated driver? |
|---|---:|---:|
| S3 Trio64 / ViRGE | **Yes-ish** – FreeBE/AF etc. | No complete mature one I'm aware of |
| Tseng ET4000/W32 / ET6000 | **Yes** | No |
| Cirrus GD543x/5446 | **Yes** | No |
| Matrox Millennium/Mystique | Some OSS driver code exists | No |
| ATI Mach64 | Lots of OSS knowledge/code | No |
| 3dfx Voodoo/Banshee/Voodoo3 | Lots of OSS code/spec knowledge | No complete Win9x replacement |
| Rendition Vérité | Some OSS code/knowledge | No |
| NVIDIA RIVA 128/TNT | OSS implementations exist elsewhere | No practical native Win9x OSS driver |

So there's an interesting gap.

**FreeBE/AF is probably the project I'd examine first if your objective is actually developing a new open-source Win9x driver for period hardware.** Its source already contains the difficult chipset-specific pieces for several cards: mode setup, framebuffer handling, register programming and in some cases 2D acceleration.

You would then need to put a **Windows 9x display-driver/VxD layer around that hardware abstraction** rather than starting from zero.

For a retro RTS-type workload, this could be particularly interesting. A small open-source Win95/98 driver targeting **one very well documented accelerator such as S3 Trio64/ViRGE or Tseng ET6000** could potentially provide accelerated BitBlt/fills while remaining simple enough to understand. That's substantially more achievable than attempting a general Direct3D driver.

---

## User

Which chipset has the most available documentation sis 6326 vs s3 trio vs s3 verge vs matrox millennium 2?

## Assistant

If the goal is **writing an open-source Windows 9x accelerated display driver**, I'd rank them:

| Rank | Chipset | Documentation / OSS knowledge | 2D complexity | Win9x-driver target |
|---|---|---|---|---|
| **1** | **S3 ViRGE / ViRGE DX** | ★★★★★ | ★★★★☆ | **Best overall** |
| **2** | **S3 Trio64 / Trio64V+** | ★★★★★ | ★★★★★ | **Best if 2D only** |
| **3** | **Matrox Millennium II (MGA2164W)** | ★★★★☆ | ★★★★☆ | Very good |
| **4** | **SiS 6326** | ★★★☆☆ | ★★★☆☆ | Harder |

### S3 ViRGE is probably the sweet spot

There's an unusually large amount of surviving **working source code** for ViRGE. XFree86 had a fully accelerated `s3virge` driver supporting the original 86C325, DX/86C375, GX, GX2, VX and others. It implemented screen-to-screen copies, rectangle fills, CPU→screen colour expansion and pattern fills.

That matters more than merely possessing a datasheet: you have working code demonstrating the actual register sequences, synchronization and hardware quirks.

ViRGE/DX is particularly attractive because XFree86 documentation indicates much of its testing centered on ViRGE/DX, making that variant one of the best-understood.

There's also emulator implementation knowledge today, including fairly detailed ViRGE implementations in projects such as 86Box.

### S3 Trio64 is arguably even easier for a pure 2D driver

If your objective is:

**Windows 95/98 → GDI acceleration → BitBlt → fills → hardware cursor → framebuffer**

and you don't care about Direct3D, I'd seriously consider **Trio64V+**.

S3's older architecture was extremely well understood. XFree86 had accelerated support covering:

- 864
- 868
- 964
- 968
- Trio32
- Trio64
- Trio64V+
- Trio64V2

and related devices.

The big advantage is architectural simplicity. You're not dragging the ViRGE 3D engine into the project at all.

For something like a Win9x RTS, that's arguably desirable:

```text
GDI
  ↓
BitBlt
  ↓
S3 BitBLT engine
  ↓
VRAM
```

You could potentially get a very useful driver from a relatively small subset of the hardware.

### Matrox Millennium II is a strong second family

The **MGA2164W** is also extremely attractive.

XFree86/Xorg's open `mga` driver supports the Millennium, Mystique, Millennium II and later Matrox architectures, providing useful working reference code.

For **performance**, I'd probably prefer the Millennium II over the S3.

For **ease of implementing a driver**, I'd prefer S3.

### SiS 6326 would be my last choice

There's open-source implementation knowledge for it, but the ecosystem isn't remotely as rich as S3.

You'd spend more time answering questions such as:

> "What exactly does this undocumented register bit do?"

With S3, there's a good chance that someone already answered the question in XFree86, Linux, FreeBE/AF, an emulator, or an old programming document.

### Recommended target

I'd specifically choose **S3 ViRGE/DX 86C375**.

It gives you an unusually good combination of:

**period correct (1996) + PCI + cheap/common hardware + mature OSS accelerated driver code + well-understood VGA compatibility + linear framebuffer + 2D acceleration + optional 3D engine.**

The available open-source documentation and drivers already demonstrate primitives such as:

```text
Screen → Screen BitBLT
Solid rectangle fill
CPU → Screen colour expansion
8×8 pattern fill
VRAM caching
Linear framebuffer
8/15/16/24/32bpp
```

ViRGE is also closely related to the Trio 2D architecture, so researching S3 effectively gives access to knowledge covering multiple generations rather than one isolated GPU family.

For an open-source Windows 95/98 S3 ViRGE driver, a sensible first target would be **86C375 ViRGE/DX**, initially implementing **VGA + framebuffer + 8/16-bit modes + GDI BitBlt/fill**, while ignoring S3D/Direct3D.

---

## User

Let’s say we use s3, what could we done that existing drivers don’t do

## Assistant

Yes. If we built a new S3 Win9x driver today, the opportunity would not mainly be “make ViRGE faster than S3’s 1997 driver.” The biggest gains would be **modern compatibility, stability, configurability, and a clean open implementation**.

The original S3 drivers already did the important 2D acceleration quite well; ViRGE was actually a strong Windows GUI accelerator even though its 3D reputation was poor.

The areas where a modern driver could materially improve things are:

| Feature | Original S3 driver | New OSS driver opportunity |
|---|---|---|
| GDI BitBlt/fills | Good | Optimize corner cases / batching |
| Hardware cursor | Yes | Yes, with cleaner fallback |
| DirectDraw 2D | Yes | **Better compatibility/fixes** |
| Modern LCD resolutions | Limited/era-specific | **Major improvement** |
| EDID/DDC handling | Basic | **Much better** |
| Custom timings | Awkward | **Built in** |
| Widescreen modes | Generally poor | **Native 1280×720, 1360×768 etc.** |
| Refresh-rate control | Often troublesome | **Explicit modeline-style control** |
| VSync/page flipping | Supported variably | **Expose predictably** |
| VRAM detection | Sometimes quirky | **Robust probing/override** |
| 2 MB/4 MB variants | Supported | Better automatic handling |
| Debugging | Proprietary/opaque | **Logging + diagnostics** |
| Source code | No | **Fully open** |
| Emulator support | Not a design goal | **86Box/PCem/QEMU-friendly** |
| Win95 → WinME | Driver-version dependent | **One unified driver** |

### The most valuable feature: modern display timing support

This would probably be the killer feature.

Instead of S3's predefined 1990s mode tables:

```text
640×480
800×600
1024×768
1152×864
1280×1024
```

we could let Windows 98 use things like:

```text
640×480 @ 60/70/72/75
800×600 @ 56/60/72/75
1024×768 @ 60/70/75
1152×864 @ 60/70/75

1280×720 @ 60
1280×800 @ 60
1280×960 @ 60
1360×768 @ 60
1366×768 @ 60   if the PLL/timing constraints allow it
```

The driver could read EDID and generate CRT timings dynamically instead of relying almost entirely on static tables.

That would make a ViRGE much nicer with current LCDs and VGA converters.

### Expose the hardware much more transparently

A modern implementation could include an **S3 Control Panel**:

```text
S3 Open Display Driver

Card:
  S3 ViRGE/DX 86C375
  PCI 5333:8A01
  Revision 01

Memory:
  4 MB EDO DRAM
  64-bit memory bus
  Linear framebuffer enabled

Clock:
  Memory: 55 MHz
  Pixel PLL: 65.0 MHz

Acceleration
 [x] Hardware BitBlt
 [x] Rectangle fill
 [x] Pattern fill
 [x] Hardware cursor
 [x] DirectDraw
 [x] Offscreen bitmap cache
 [x] Page flipping
 [ ] S3D acceleration

Display
  1280×720
  16-bit
  60.0 Hz

EDID
  Dell U2412M
  Preferred: 1920×1200
```

Original drivers were essentially black boxes.

For retro development, seeing what is actually happening would be extremely useful.

### An acceleration toggle per primitive

This would be especially useful for compatibility testing:

```text
Acceleration:

[x] Screen-to-screen BitBlt
[x] Solid fill
[x] Pattern fill
[x] Color expansion
[x] Hardware cursor
[x] Offscreen bitmap cache

[ ] Accelerate transparent BitBlt
[ ] Accelerate stretch operations
```

Old GPUs often have obscure hardware bugs triggered by particular ROP combinations, alignment, pitch, clipping or overlapping rectangles.

Instead of:

> "Move the Windows acceleration slider down two notches"

you could disable the **exact primitive causing the problem**.

### A hybrid software/hardware strategy could potentially outperform the old driver

A shadow framebuffer can outperform hardware acceleration for some **small fills and blits**, because waiting for the ViRGE command engine incurs overhead.

So we don't necessarily want:

```text
GDI operation
      ↓
ALWAYS SEND TO S3 ENGINE
```

We could have:

```text
                 ┌→ CPU framebuffer copy
GDI BitBlt → size/ROP decision
                 └→ S3 BitBlt engine
```

Something like:

```text
if copy < 128 bytes
    CPU
else
    S3 engine
```

with thresholds discovered through benchmarking.

A Pentium III or Athlon running Windows 98 could therefore behave quite differently from the Pentium-era machine S3 originally optimized for.

That raises the possibility that a **modern ViRGE driver could outperform S3's own driver on fast retro CPUs**, at least for some GDI workloads.

### Better framebuffer access could matter a lot for games

For an RTS-type workload in particular, significant attention should go into:

**DirectDraw surfaces + linear framebuffer + page flipping + palette operations.**

A lot of 1990s games don't really need 3D acceleration.

They need:

```text
Lock surface
write pixels
unlock
flip
```

or:

```text
sprite bitmap
     ↓
colour-keyed BitBlt
     ↓
video memory
```

A driver optimized around these workloads could be very good.

For example, expose reliable:

- DirectDraw primary surfaces
- back buffers
- hardware page flipping
- colour-key blits
- clipping
- VRAM surfaces
- system-memory → VRAM blits
- VBlank synchronization
- 8-bit palette updates

That would potentially be more useful to 1995–1998 games than trying to rescue S3D.

### Deliberately avoid ViRGE 3D initially

Version 1 should probably be:

**Windows 95/98/ME**

```text
S3 Trio64
S3 Trio64V+
S3 ViRGE
S3 ViRGE/DX
S3 ViRGE/GX
```

with:

**GDI + DirectDraw + modern modes + EDID + excellent diagnostics.**

No Direct3D.

That gives a much smaller and more achievable project.

A particularly interesting **Phase 2** would be implementing a modern open DirectDraw HAL and benchmarking exactly which operations should run on the CPU versus the S3 accelerator. For a fast Pentium MMX/Pentium II/III-era machine, that hybrid approach could produce a genuinely better Win98 experience than the original S3 driver rather than merely reproducing it.

---

## User

what is a good name for a retro driver, the plan is to also support matrox and trio in future too.

## Assistant

Since the project is intended to grow beyond ViRGE to **S3 Trio and Matrox**, I'd avoid putting S3, ViRGE, or any chipset name in the project name. Something that sounds like a plausible 1996 graphics-driver product would fit nicely.

My favourites:

- **OpenGX** — short, period-appropriate, and sounds like a 90s graphics subsystem. “OpenGX Display Driver for Windows 9x.”
- **RetroGLX** — clearly retro, although “GLX” has a strong X/OpenGL association that could be misleading.
- **LegacyGX** — professional sounding and naturally covers multiple vendors.
- **OpenDisplay 9x** — extremely clear about what it is, though less distinctive.
- **RetroDisplay** — simple and expandable across S3, Matrox, Cirrus, Tseng, etc.
- **OpenVGA 9x** — memorable, although technically these cards are much more than VGA.
- **FreeGX** — has a very authentic FreeDOS/FreeVBE/FreeBE-era feel.
- **VGA9x** — excellent descriptive project/repository name.
- **Accel9x** — particularly suitable because it communicates the project's focus on accelerated Windows 9x graphics.
- **OpenAccel** — also strong, but less explicitly Win9x.
- **Velocity9x** — suitably 90s and graphics-oriented.
- **Pixel9x** — simple and memorable.
- **Raster9x** — technically appropriate and has a retro workstation feel.
- **Display9x** — simple and extremely clear.
- **OpenDDX** — nice driver-development feel, although DDX has X11 connotations.

### Suggested branding

One strong option was:

**Accel9x**  
*Open Accelerated Display Drivers for Windows 9x*

For example:

```text
Accel9x
Open Accelerated Display Drivers for Windows 9x

Drivers:
  S3 ViRGE
  S3 ViRGE/DX
  S3 ViRGE/GX
  S3 Trio64
  S3 Trio64V+
  Matrox Millennium
  Matrox Millennium II
```

Possible component names:

```text
ACCEL9X.DRV
ACCEL9X.VXD

S3VIRGE.DRV
S3TRIO.DRV
MGA.DRV
```

The vendor-neutral naming leaves room for eventually adding **ATI Mach64, Cirrus Logic, Tseng ET6000, SiS 6326, 3dfx, etc.**

**OpenGX** was another strong choice if the aim was something that sounded more like an actual 1990s commercial graphics architecture.

---

## User

I'm going to pick Velocity9x because I have a large 3dfx collection so it works for me :)

## Assistant

**Velocity9x** is a great fit—especially with the subtle 3dfx **Voodoo Velocity** connection. It also doesn't tie the project to one vendor, so it still works naturally for S3, Matrox, and potentially 3dfx later.

Suggested branding:

# Velocity9x

**Open accelerated display drivers for Windows 9x**

A possible project structure:

```text
Velocity9x
├── core/
│   ├── display
│   ├── modes
│   ├── edid
│   ├── gdi
│   └── directdraw
│
├── drivers/
│   ├── s3/
│   │   ├── trio
│   │   └── virge
│   ├── matrox/
│   │   ├── millennium
│   │   └── millennium2
│   └── 3dfx/
│       └── ...
│
└── tools/
    ├── control-panel
    ├── diagnostics
    └── benchmarks
```

That architecture is worth thinking about **before writing the ViRGE driver**, because if the chipset-specific layer can mostly consist of mode-setting, memory management and acceleration primitives, adding Trio and Millennium II later becomes considerably easier.

If Velocity9x eventually gets a 3dfx backend, the name becomes even more appropriate.

---

# Current Velocity9x Direction

The project direction emerging from the discussion is:

1. **Initial hardware target:** S3 ViRGE/DX (86C375).
2. **Initial operating systems:** Windows 95, Windows 98/98SE and potentially Windows ME.
3. **Primary goal:** A clean, open-source, accelerated Windows 9x display driver.
4. **Initial acceleration focus:** GDI and DirectDraw rather than Direct3D/S3D.
5. **Core acceleration primitives:** BitBlt, solid fills, pattern fills, colour expansion, hardware cursor and off-screen surfaces.
6. **Modern display improvements:** EDID/DDC, custom timings, widescreen modes and improved refresh-rate handling.
7. **Diagnostics:** Detailed hardware information, logging and per-primitive acceleration controls.
8. **Performance:** Benchmark CPU framebuffer operations versus hardware acceleration and dynamically choose the faster path where practical.
9. **DirectDraw:** Reliable surfaces, back buffers, page flipping, colour-key blits, clipping, VBlank synchronization and palette operations.
10. **Future S3 support:** Trio64, Trio64V+, other ViRGE variants.
11. **Future Matrox support:** Millennium and Millennium II.
12. **Potential future 3dfx support:** A particularly appropriate expansion given the **Velocity9x** name.
13. **Architecture:** Keep common Windows 9x display/GDI/DirectDraw functionality separate from chipset-specific hardware backends.

The guiding idea is not merely to reproduce the original vendor drivers, but to create a maintainable Windows 9x graphics stack that can provide **better modern-display compatibility, diagnostics, configurability and potentially better performance on later retro CPUs**.
