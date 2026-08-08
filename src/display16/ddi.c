/*
 * Velocity9x first active DIB Engine path.
 *
 * Scope is intentionally restricted to one unaccelerated 640x480x8 linear
 * framebuffer. The assembly runtime performs VBE mode entry and DPMI mapping;
 * every drawing operation remains with the Windows DIB Engine.
 */
#define SetCursor V9xUserSetCursor
#include <windows.h>
#undef SetCursor

#include "velocity9x/build.h"
#include "win9x_display_abi.h"

#define V9X_WIDTH                 640u
#define V9X_HEIGHT                480u
#define V9X_BPP                     8u
#define V9X_PITCH                 640u
#define V9X_BITMAP_HEADER_SIZE     40u
#define V9X_PALETTE_ENTRIES       256u
#define V9X_PALETTE_BYTES        1024u
#define V9X_PDEVICE_EXTRA       (V9X_BITMAP_HEADER_SIZE + V9X_PALETTE_BYTES)

#define V9X_COM1_DATA_PORT      0x03f8u
#define V9X_COM1_LCR_PORT       0x03fbu
#define V9X_COM1_LSR_PORT       0x03fdu
#define V9X_COM1_TX_EMPTY          0x20u
#define V9X_SERIAL_SPIN_LIMIT   0xffffu

#define V9X_COLOR_NONSTATIC        0x80u
#define V9X_COLOR_MAP_TO_WHITE     0x40u

extern WORD FAR PASCAL V9xDibEnableCall(LPVOID, WORD, LPSTR, LPSTR, LPVOID);
extern DWORD FAR PASCAL V9xCreateDibPDeviceCall(LPBITMAPINFO, LPVOID,
                                                LPVOID, WORD);
extern void FAR PASCAL V9xDibBeginAccess(void);
extern void FAR PASCAL V9xDibEndAccess(void);
extern DWORD FAR PASCAL V9xDibSetPaletteCall(WORD, WORD, LPVOID, LPVOID);
extern DWORD FAR PASCAL V9xDibSetPaletteTranslateCall(LPVOID, LPVOID);
extern WORD FAR PASCAL V9xHardwarePresent(void);
extern WORD FAR PASCAL V9xHardwareEnable(void);
extern WORD FAR PASCAL V9xHardwareReset(void);
extern DWORD FAR PASCAL V9xHardwareBase(void);
extern void FAR PASCAL V9xHardwareDisable(void);
extern WORD FAR PASCAL V9xVddRegister(void);
extern void FAR PASCAL V9xVddPostMode(void);
extern void FAR PASCAL V9xVddUnregister(void);

V9X_DIB_ENGINE FAR *v9x_driver_pdevice;
static RGBQUAD FAR *v9x_color_table;
static WORD v9x_dib_pdevice_size;
static WORD v9x_screen_selector;
static WORD v9x_enabled;

static BYTE v9x_port_in(WORD port);
#pragma aux v9x_port_in = "in al,dx" parm [dx] value [al] modify exact [al]

static void v9x_port_out(WORD port, BYTE value);
#pragma aux v9x_port_out = "out dx,al" parm [dx] [al] modify exact []

static void v9x_serial_write(const char FAR *message)
{
    BYTE saved_lcr;
    WORD spins;

    if (v9x_port_in(V9X_COM1_LSR_PORT) == 0xffu) {
        return;
    }
    saved_lcr = v9x_port_in(V9X_COM1_LCR_PORT);
    v9x_port_out(V9X_COM1_LCR_PORT, (BYTE)(saved_lcr & 0x7fu));

    while (*message != '\0') {
        spins = V9X_SERIAL_SPIN_LIMIT;
        while ((v9x_port_in(V9X_COM1_LSR_PORT) & V9X_COM1_TX_EMPTY) == 0u) {
            if (--spins == 0u) {
                v9x_port_out(V9X_COM1_LCR_PORT, saved_lcr);
                return;
            }
        }
        v9x_port_out(V9X_COM1_DATA_PORT, (BYTE)*message++);
    }
    v9x_port_out(V9X_COM1_LCR_PORT, saved_lcr);
}

static void v9x_serial_write_hex32(DWORD value)
{
    static const char digits[] = "0123456789ABCDEF";
    char text[9];
    short shift;

    for (shift = 28; shift >= 0; shift -= 4) {
        text[(28 - shift) / 4] = digits[(WORD)(value >> shift) & 0x000fu];
    }
    text[8] = '\0';
    v9x_serial_write(text);
}

void v9x_display_boot_log(void)
{
    v9x_serial_write("V9X-DRV load build=" V9X_BUILD_ID "\r\n");
}

static void v9x_set_color(RGBQUAD FAR *entry,
                          BYTE red,
                          BYTE green,
                          BYTE blue,
                          BYTE flags)
{
    entry->rgbRed = red;
    entry->rgbGreen = green;
    entry->rgbBlue = blue;
    entry->rgbReserved = flags;
}

static void v9x_build_palette(RGBQUAD FAR *palette)
{
    WORD index;

    for (index = 0u; index < V9X_PALETTE_ENTRIES; ++index) {
        v9x_set_color(&palette[index], 0u, 0u, 0u, V9X_COLOR_NONSTATIC);
    }

    v9x_set_color(&palette[0],   0u,   0u,   0u, 0u);
    v9x_set_color(&palette[1], 128u,   0u,   0u, 0u);
    v9x_set_color(&palette[2],   0u, 128u,   0u, 0u);
    v9x_set_color(&palette[3], 128u, 128u,   0u, 0u);
    v9x_set_color(&palette[4],   0u,   0u, 128u, 0u);
    v9x_set_color(&palette[5], 128u,   0u, 128u, 0u);
    v9x_set_color(&palette[6],   0u, 128u, 128u, 0u);
    v9x_set_color(&palette[7], 192u, 192u, 192u, V9X_COLOR_MAP_TO_WHITE);
    v9x_set_color(&palette[8], 192u, 220u, 192u,
                  V9X_COLOR_NONSTATIC | V9X_COLOR_MAP_TO_WHITE);
    v9x_set_color(&palette[9], 166u, 202u, 240u,
                  V9X_COLOR_NONSTATIC | V9X_COLOR_MAP_TO_WHITE);

    v9x_set_color(&palette[246], 255u, 251u, 240u,
                  V9X_COLOR_NONSTATIC | V9X_COLOR_MAP_TO_WHITE);
    v9x_set_color(&palette[247], 160u, 160u, 164u,
                  V9X_COLOR_NONSTATIC | V9X_COLOR_MAP_TO_WHITE);
    v9x_set_color(&palette[248], 128u, 128u, 128u, V9X_COLOR_MAP_TO_WHITE);
    v9x_set_color(&palette[249], 255u,   0u,   0u, 0u);
    v9x_set_color(&palette[250],   0u, 255u,   0u, V9X_COLOR_MAP_TO_WHITE);
    v9x_set_color(&palette[251], 255u, 255u,   0u, V9X_COLOR_MAP_TO_WHITE);
    v9x_set_color(&palette[252],   0u,   0u, 255u, 0u);
    v9x_set_color(&palette[253], 255u,   0u, 255u, 0u);
    v9x_set_color(&palette[254],   0u, 255u, 255u, V9X_COLOR_MAP_TO_WHITE);
    v9x_set_color(&palette[255], 255u, 255u, 255u, V9X_COLOR_MAP_TO_WHITE);
}

static void v9x_program_palette(WORD start, WORD count)
{
    RGBQUAD FAR *entry;

    if (v9x_color_table == 0 || start >= V9X_PALETTE_ENTRIES) {
        return;
    }
    if (count > V9X_PALETTE_ENTRIES - start) {
        count = V9X_PALETTE_ENTRIES - start;
    }

    v9x_port_out(0x03c8u, (BYTE)start);
    entry = &v9x_color_table[start];
    while (count-- != 0u) {
        v9x_port_out(0x03c9u, (BYTE)(entry->rgbRed >> 2));
        v9x_port_out(0x03c9u, (BYTE)(entry->rgbGreen >> 2));
        v9x_port_out(0x03c9u, (BYTE)(entry->rgbBlue >> 2));
        ++entry;
    }
}

static void v9x_set_point(V9X_POINT_TYPE FAR *point, short x, short y)
{
    point->x = x;
    point->y = y;
}

static WORD v9x_fill_gdi_info(V9X_GDI_INFO FAR *info,
                              LPSTR destination_type,
                              LPSTR output_file,
                              LPVOID data)
{
    WORD result;

    result = V9xDibEnableCall(info, 1u, destination_type, output_file, data);
    if (result == 0u || info->dpDEVICEsize <= 0) {
        return 0u;
    }

    v9x_dib_pdevice_size = (WORD)info->dpDEVICEsize;
    info->dpVersion = V9X_DRV_VERSION;
    info->dpTechnology = V9X_DT_RASDISPLAY;
    info->dpHorzSize = 208;
    info->dpVertSize = 156;
    info->dpHorzRes = V9X_WIDTH;
    info->dpVertRes = V9X_HEIGHT;
    info->dpBitsPixel = V9X_BPP;
    info->dpPlanes = 1;
    info->dpNumBrushes = -1;
    info->dpNumPens = 16;
    info->dpNumFonts = 0;
    info->dpNumColors = 20;
    info->dpDEVICEsize = (short)(v9x_dib_pdevice_size + V9X_PDEVICE_EXTRA);
    info->dpRaster |= V9X_RC_PALETTE | V9X_RC_DIBTODEV;

    v9x_set_point(&info->dpMLoWin, 2080, 1560);
    v9x_set_point(&info->dpMLoVpt, V9X_WIDTH, -V9X_HEIGHT);
    v9x_set_point(&info->dpMHiWin, 20800, 15600);
    v9x_set_point(&info->dpMHiVpt, V9X_WIDTH, -V9X_HEIGHT);
    v9x_set_point(&info->dpELoWin, 325, 325);
    v9x_set_point(&info->dpELoVpt, 254, -254);
    v9x_set_point(&info->dpEHiWin, 1625, 1625);
    v9x_set_point(&info->dpEHiVpt, 127, -127);
    v9x_set_point(&info->dpTwpWin, 2340, 2340);
    v9x_set_point(&info->dpTwpVpt, 127, -127);

    info->dpLogPixelsX = 96;
    info->dpLogPixelsY = 96;
    info->dpDCManage = V9X_DC_IGNORE_DFNP;
    info->dpCaps1 |= V9X_C1_DIBENGINE | V9X_C1_REINIT_ABLE |
                     V9X_C1_BYTE_PACKED | V9X_C1_COLORCURSOR |
                     V9X_C1_SLOW_CARD;
    info->dpNumPalReg = V9X_PALETTE_ENTRIES;
    info->dpPalReserved = 20u;
    info->dpColorRes = 18u;
    return V9X_GDIINFO_SIZE;
}

static WORD v9x_build_pdevice(LPVOID device_info,
                              LPSTR destination_type,
                              LPSTR output_file,
                              LPVOID data)
{
    BITMAPINFO FAR *bitmap_info;
    DWORD created;

    if (v9x_dib_pdevice_size == 0u) {
        return 0u;
    }
    if (V9xHardwarePresent() == 0u) {
        v9x_serial_write("V9X-DRV enable-fail stage=device-id\r\n");
        return 0u;
    }
    v9x_screen_selector = V9xHardwareEnable();
    if (v9x_screen_selector == 0u) {
        v9x_serial_write("V9X-DRV enable-fail stage=mode-map\r\n");
        return 0u;
    }
    if (V9xVddRegister() == 0u) {
        v9x_serial_write("V9X-DRV enable-fail stage=vdd-register\r\n");
        V9xHardwareDisable();
        v9x_screen_selector = 0u;
        return 0u;
    }

    if (V9xDibEnableCall(device_info, 0u, destination_type,
                         output_file, data) == 0u) {
        v9x_serial_write("V9X-DRV enable-fail stage=dib-enable\r\n");
        V9xVddUnregister();
        V9xHardwareDisable();
        v9x_screen_selector = 0u;
        return 0u;
    }
    (void)V9xDibSetPaletteTranslateCall(0, device_info);

    bitmap_info = (BITMAPINFO FAR *)
        ((BYTE FAR *)device_info + v9x_dib_pdevice_size);
    bitmap_info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap_info->bmiHeader.biWidth = V9X_WIDTH;
    bitmap_info->bmiHeader.biHeight = V9X_HEIGHT;
    bitmap_info->bmiHeader.biPlanes = 1u;
    bitmap_info->bmiHeader.biBitCount = V9X_BPP;
    bitmap_info->bmiHeader.biCompression = BI_RGB;
    bitmap_info->bmiHeader.biSizeImage = (DWORD)V9X_PITCH * V9X_HEIGHT;
    bitmap_info->bmiHeader.biXPelsPerMeter = 0;
    bitmap_info->bmiHeader.biYPelsPerMeter = 0;
    bitmap_info->bmiHeader.biClrUsed = V9X_PALETTE_ENTRIES;
    bitmap_info->bmiHeader.biClrImportant = V9X_PALETTE_ENTRIES;

    v9x_color_table = bitmap_info->bmiColors;
    v9x_build_palette(v9x_color_table);
    created = V9xCreateDibPDeviceCall(bitmap_info, device_info,
                                     MAKELP(v9x_screen_selector, 0u),
                                     V9X_DE_MINIDRIVER | V9X_DE_PALETTIZED |
                                     V9X_DE_VRAM);
    if (created == 0ul) {
        v9x_serial_write("V9X-DRV enable-fail stage=create-pdevice\r\n");
        V9xVddUnregister();
        V9xHardwareDisable();
        v9x_screen_selector = 0u;
        v9x_color_table = 0;
        return 0u;
    }

    v9x_driver_pdevice = (V9X_DIB_ENGINE FAR *)device_info;
    v9x_driver_pdevice->deBeginAccess = V9xDibBeginAccess;
    v9x_driver_pdevice->deEndAccess = V9xDibEndAccess;
    v9x_driver_pdevice->deVersion = V9X_DE_VERSION;
    v9x_program_palette(0u, V9X_PALETTE_ENTRIES);
    v9x_enabled = 1u;
    v9x_serial_write("V9X-DRV lfb=0x");
    v9x_serial_write_hex32(V9xHardwareBase());
    v9x_serial_write(" bytes=00400000\r\n");
    v9x_serial_write("V9X-DRV enable-ok mode=640x480x8 lfb-mapped\r\n");
    return 1u;
}

WORD __loadds FAR PASCAL Enable(LPVOID device_info,
                                WORD action,
                                LPSTR destination_type,
                                LPSTR output_file,
                                LPVOID data)
{
    if ((action & 1u) != 0u) {
        v9x_serial_write("V9X-DRV enable-query mode=640x480x8\r\n");
        return v9x_fill_gdi_info((V9X_GDI_INFO FAR *)device_info,
                                 destination_type, output_file, data);
    }
    return v9x_build_pdevice(device_info, destination_type, output_file, data);
}

WORD __loadds FAR PASCAL Disable(LPVOID destination_device)
{
    V9X_DIB_ENGINE FAR *device =
        (V9X_DIB_ENGINE FAR *)destination_device;

    if (device != 0) {
        device->deFlags |= V9X_DE_BUSY;
    }
    v9x_enabled = 0u;
    v9x_driver_pdevice = 0;
    v9x_color_table = 0;
    V9xVddUnregister();
    V9xHardwareDisable();
    v9x_screen_selector = 0u;
    v9x_serial_write("V9X-DRV disable\r\n");
    return 0xffffu;
}

WORD __loadds FAR PASCAL ReEnable(LPVOID destination_device,
                                  LPVOID gdi_info)
{
    V9X_DIB_ENGINE FAR *device =
        (V9X_DIB_ENGINE FAR *)destination_device;

    if (device == 0 || gdi_info == 0 || v9x_enabled == 0u) {
        return 0u;
    }
    if (v9x_fill_gdi_info((V9X_GDI_INFO FAR *)gdi_info, 0, 0, 0) == 0u ||
        V9xHardwareReset() == 0u) {
        v9x_serial_write("V9X-DRV reenable-fail\r\n");
        return 0u;
    }
    device->deFlags &= (WORD)~V9X_DE_BUSY;
    v9x_program_palette(0u, V9X_PALETTE_ENTRIES);
    V9xVddPostMode();
    v9x_serial_write("V9X-DRV reenable-ok\r\n");
    return 1u;
}

WORD __loadds FAR PASCAL ValidateMode(LPVOID display_info)
{
    V9X_DISPLAY_VALIDATE_MODE FAR *mode =
        (V9X_DISPLAY_VALIDATE_MODE FAR *)display_info;

    if (mode == 0 || mode->size < sizeof(*mode)) {
        return V9X_VALMODE_NO_WRONG_DRIVER;
    }
    if (V9xHardwarePresent() == 0u) {
        return V9X_VALMODE_NO_WRONG_DRIVER;
    }
    if (mode->width != V9X_WIDTH || mode->height != V9X_HEIGHT ||
        mode->bits_per_pixel != V9X_BPP) {
        return V9X_VALMODE_NO_NOMEM;
    }
    return V9X_VALMODE_YES;
}

DWORD __loadds FAR PASCAL SetPalette(WORD start,
                                     WORD count,
                                     LPVOID palette)
{
    DWORD result;

    if (v9x_driver_pdevice == 0 || palette == 0) {
        return 0ul;
    }
    result = V9xDibSetPaletteCall(start, count, palette,
                                 v9x_driver_pdevice);
    if ((v9x_driver_pdevice->deFlags & V9X_DE_BUSY) == 0u) {
        v9x_program_palette(start, count);
    }
    return result;
}

void __loadds FAR PASCAL ResetHiResMode(void)
{
    if (V9xHardwareReset() != 0u) {
        v9x_program_palette(0u, V9X_PALETTE_ENTRIES);
    }
}
