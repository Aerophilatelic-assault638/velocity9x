/*
 * Velocity9x first active DIB Engine path.
 *
 * Scope is restricted to an unaccelerated standard VBE mode matrix. The
 * assembly runtime performs VBE mode entry and DPMI mapping; every drawing
 * operation remains with the Windows DIB Engine.
 */
#define SetCursor V9xUserSetCursor
#include <windows.h>
#undef SetCursor

#include "velocity9x/build.h"
#include "win9x_display_abi.h"

#define V9X_BITMAP_HEADER_SIZE     40u
#define V9X_PALETTE_ENTRIES       256u
#define V9X_PALETTE_BYTES        1024u

#define V9X_COM1_DATA_PORT      0x03f8u
#define V9X_COM1_LCR_PORT       0x03fbu
#define V9X_COM1_LSR_PORT       0x03fdu
#define V9X_COM1_TX_EMPTY          0x20u
#define V9X_SERIAL_SPIN_LIMIT   0xffffu

#define V9X_COLOR_NONSTATIC        0x80u
#define V9X_COLOR_MAP_TO_WHITE     0x40u

#ifndef V9X_FORCE_MODE_INDEX
#define V9X_FORCE_MODE_INDEX         -1
#endif

extern WORD FAR PASCAL V9xDibEnableCall(LPVOID, WORD, LPSTR, LPSTR, LPVOID);
extern DWORD FAR PASCAL V9xCreateDibPDeviceCall(LPBITMAPINFO, LPVOID,
                                                LPVOID, WORD);
extern void FAR PASCAL V9xDibBeginAccess(void);
extern void FAR PASCAL V9xDibEndAccess(void);
extern DWORD FAR PASCAL V9xDibSetPaletteCall(WORD, WORD, LPVOID, LPVOID);
extern DWORD FAR PASCAL V9xDibSetPaletteTranslateCall(LPVOID, LPVOID);
extern WORD FAR PASCAL V9xHardwarePresent(void);
extern WORD FAR PASCAL V9xHardwareEnable(void);
extern WORD FAR PASCAL V9xHardwareStage(void);
extern WORD FAR PASCAL V9xHardwareReset(void);
extern DWORD FAR PASCAL V9xHardwareBase(void);
extern void FAR PASCAL V9xHardwareDisable(void);
extern WORD FAR PASCAL V9xVddRegister(void);
extern void FAR PASCAL V9xVddPostMode(void);
extern void FAR PASCAL V9xVddUnregister(void);
extern WORD FAR PASCAL V9xVddGetDisplayConfig(V9X_DISPLAY_INFO FAR *);

typedef struct v9x_display_mode {
    WORD width;
    WORD height;
    WORD bits_per_pixel;
    WORD pitch;
    WORD vbe_mode;
    short english_low;
    short english_high;
} V9X_DISPLAY_MODE;

static const V9X_DISPLAY_MODE v9x_modes[] = {
    {  640u, 480u,  8u,  640u, 0x0101u, 254, 127 },
    {  800u, 600u,  8u,  800u, 0x0103u, 318, 159 },
    { 1024u, 768u,  8u, 1024u, 0x0105u, 407, 203 },
    {  640u, 480u, 16u, 1280u, 0x0111u, 254, 127 },
    {  800u, 600u, 16u, 1600u, 0x0114u, 318, 159 },
    { 1024u, 768u, 16u, 2048u, 0x0117u, 407, 203 }
};
#define V9X_MODE_COUNT (sizeof(v9x_modes) / sizeof(v9x_modes[0]))

V9X_DIB_ENGINE FAR *v9x_driver_pdevice;
WORD v9x_active_vbe_mode = 0x0101u;
DWORD v9x_active_visible_bytes = 307200ul;
WORD v9x_palettized = 1u;
static RGBQUAD FAR *v9x_color_table;
static const V9X_DISPLAY_MODE *v9x_selected_mode = &v9x_modes[0];
static const V9X_DISPLAY_MODE *v9x_active_mode;
static WORD v9x_dib_pdevice_size;
static WORD v9x_screen_selector;
static WORD v9x_enabled;
static WORD v9x_dpi = 96u;

#ifdef V9X_BOOT_TRACE
static BOOL v9x_boot_trace(const char FAR *stage)
{
    return WritePrivateProfileString("Velocity9x", "Stage", stage,
                                     "C:\\V9XBOOT.INI");
}

static void v9x_trace_hardware_failure(void)
{
    switch (V9xHardwareStage()) {
    case 1u: v9x_boot_trace("fail-hardware-pci"); break;
    case 2u: v9x_boot_trace("fail-hardware-vbe-mode"); break;
    case 3u: v9x_boot_trace("fail-hardware-aperture"); break;
    case 4u: v9x_boot_trace("fail-hardware-selector"); break;
    case 5u: v9x_boot_trace("fail-hardware-dpmi-map"); break;
    case 6u: v9x_boot_trace("fail-hardware-selector-base"); break;
    case 7u: v9x_boot_trace("fail-hardware-selector-limit"); break;
    default: v9x_boot_trace("fail-hardware-unknown"); break;
    }
}
#else
#define v9x_boot_trace(stage) ((void)0)
#define v9x_trace_hardware_failure() ((void)0)
#endif

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

static void v9x_serial_write_u16(WORD value)
{
    char text[6];
    WORD length = 0u;
    WORD index;

    do {
        text[length++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u && length < 5u);
    for (index = 0u; index < length / 2u; ++index) {
        char temporary = text[index];
        text[index] = text[length - index - 1u];
        text[length - index - 1u] = temporary;
    }
    text[length] = '\0';
    v9x_serial_write(text);
}

static void v9x_serial_write_mode(const char FAR *prefix)
{
    v9x_serial_write(prefix);
    v9x_serial_write_u16(v9x_selected_mode->width);
    v9x_serial_write("x");
    v9x_serial_write_u16(v9x_selected_mode->height);
    v9x_serial_write("x");
    v9x_serial_write_u16(v9x_selected_mode->bits_per_pixel);
}

static const V9X_DISPLAY_MODE *v9x_find_mode(WORD width,
                                              WORD height,
                                              WORD bits_per_pixel)
{
    WORD index;

    for (index = 0u; index < V9X_MODE_COUNT; ++index) {
        if (v9x_modes[index].width == width &&
            v9x_modes[index].height == height &&
            v9x_modes[index].bits_per_pixel == bits_per_pixel) {
            return &v9x_modes[index];
        }
    }
    return 0;
}

static void v9x_select_requested_mode(void)
{
    const V9X_DISPLAY_MODE *requested = 0;

#if V9X_FORCE_MODE_INDEX >= 0
    requested = &v9x_modes[V9X_FORCE_MODE_INDEX];
#else
    V9X_DISPLAY_INFO display_info;
    BYTE *bytes = (BYTE *)&display_info;
    WORD index;

    for (index = 0u; index < sizeof(display_info); ++index) {
        bytes[index] = 0u;
    }
    if (V9xVddGetDisplayConfig(&display_info) != 0u) {
        requested = v9x_find_mode(display_info.width, display_info.height,
                                  display_info.bits_per_pixel);
        if (display_info.dpi >= 72u && display_info.dpi <= 200u) {
            v9x_dpi = display_info.dpi;
        }
    }
#endif
    if (requested == 0) {
        requested = &v9x_modes[0];
    }
    v9x_selected_mode = requested;
    v9x_active_vbe_mode = requested->vbe_mode;
    v9x_active_visible_bytes =
        (DWORD)requested->pitch * (DWORD)requested->height;
    v9x_palettized = requested->bits_per_pixel == 8u ? 1u : 0u;
}

void v9x_display_boot_log(void)
{
    v9x_serial_write("V9X-DRV load build=" V9X_BUILD_ID "\r\n");
    /* Boot-capture evidence shows ring-3 serial writes from LibMain do not
     * normally reach the host log. The INI marker is strong load evidence,
     * but its absence is inconclusive because the early file write can fail. */
#ifdef V9X_BOOT_TRACE
    if (!v9x_boot_trace("libmain")) {
        v9x_serial_write("V9X-DRV trace-write-fail stage=libmain build="
                         V9X_BUILD_ID "\r\n");
    }
#endif
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
    WORD extra_size;

    v9x_boot_trace("query-start");
    if (v9x_enabled == 0u) {
        v9x_select_requested_mode();
    }
    v9x_boot_trace("query-mode-selected");

    result = V9xDibEnableCall(info, 1u, destination_type, output_file, data);
    if (result == 0u || info->dpDEVICEsize <= 0) {
        v9x_boot_trace("fail-dib-query");
        return 0u;
    }

    v9x_dib_pdevice_size = (WORD)info->dpDEVICEsize;
    info->dpVersion = V9X_DRV_VERSION;
    info->dpTechnology = V9X_DT_RASDISPLAY;
    info->dpHorzSize = 208;
    info->dpVertSize = 156;
    info->dpHorzRes = v9x_selected_mode->width;
    info->dpVertRes = v9x_selected_mode->height;
    info->dpBitsPixel = v9x_selected_mode->bits_per_pixel;
    info->dpPlanes = 1;
    info->dpNumBrushes = -1;
    info->dpNumFonts = 0;
    extra_size = V9X_BITMAP_HEADER_SIZE;
    info->dpRaster |= V9X_RC_DIBTODEV;
    if (v9x_palettized != 0u) {
        info->dpNumPens = 16;
        info->dpNumColors = 20;
        info->dpRaster |= V9X_RC_PALETTE;
        info->dpNumPalReg = V9X_PALETTE_ENTRIES;
        info->dpPalReserved = 20u;
        info->dpColorRes = 18u;
        extra_size += V9X_PALETTE_BYTES;
    } else {
        info->dpNumPens = -1;
        info->dpNumColors = -1;
        info->dpRaster &= (WORD)~V9X_RC_PALETTE;
        info->dpNumPalReg = 0u;
        info->dpPalReserved = 0u;
        info->dpColorRes = 0u;
    }
    info->dpDEVICEsize = (short)(v9x_dib_pdevice_size + extra_size);

    v9x_set_point(&info->dpMLoWin, 2080, 1560);
    v9x_set_point(&info->dpMLoVpt, (short)v9x_selected_mode->width,
                  -(short)v9x_selected_mode->height);
    v9x_set_point(&info->dpMHiWin, 20800, 15600);
    v9x_set_point(&info->dpMHiVpt, (short)v9x_selected_mode->width,
                  -(short)v9x_selected_mode->height);
    v9x_set_point(&info->dpELoWin, 325, 325);
    v9x_set_point(&info->dpELoVpt, v9x_selected_mode->english_low,
                  -v9x_selected_mode->english_low);
    v9x_set_point(&info->dpEHiWin, 1625, 1625);
    v9x_set_point(&info->dpEHiVpt, v9x_selected_mode->english_high,
                  -v9x_selected_mode->english_high);
    v9x_set_point(&info->dpTwpWin, 2340, 2340);
    v9x_set_point(&info->dpTwpVpt, v9x_selected_mode->english_high,
                  -v9x_selected_mode->english_high);

    info->dpLogPixelsX = (short)v9x_dpi;
    info->dpLogPixelsY = (short)v9x_dpi;
    info->dpDCManage = V9X_DC_IGNORE_DFNP;
    info->dpCaps1 |= V9X_C1_DIBENGINE | V9X_C1_REINIT_ABLE |
                     V9X_C1_BYTE_PACKED | V9X_C1_COLORCURSOR |
                     V9X_C1_SLOW_CARD;
    v9x_boot_trace("query-ok");
    return V9X_GDIINFO_SIZE;
}

static WORD v9x_build_pdevice(LPVOID device_info,
                              LPSTR destination_type,
                              LPSTR output_file,
                              LPVOID data)
{
    BITMAPINFO FAR *bitmap_info;
    DWORD created;
    WORD pdevice_flags = V9X_DE_MINIDRIVER | V9X_DE_VRAM;

    if (v9x_dib_pdevice_size == 0u) {
        v9x_boot_trace("fail-pdevice-size");
        return 0u;
    }
    v9x_boot_trace("enable-start");
    if (V9xHardwarePresent() == 0u) {
        v9x_boot_trace("fail-hardware-present");
        v9x_serial_write("V9X-DRV enable-fail stage=device-id\r\n");
        return 0u;
    }
    v9x_screen_selector = V9xHardwareEnable();
    if (v9x_screen_selector == 0u) {
        v9x_trace_hardware_failure();
        v9x_serial_write("V9X-DRV enable-fail stage=mode-map\r\n");
        return 0u;
    }
    if (V9xVddRegister() == 0u) {
        v9x_boot_trace("fail-vdd-register");
        v9x_serial_write("V9X-DRV enable-fail stage=vdd-register\r\n");
        V9xHardwareDisable();
        v9x_screen_selector = 0u;
        return 0u;
    }

    if (V9xDibEnableCall(device_info, 0u, destination_type,
                         output_file, data) == 0u) {
        v9x_boot_trace("fail-dib-enable");
        v9x_serial_write("V9X-DRV enable-fail stage=dib-enable\r\n");
        V9xVddUnregister();
        V9xHardwareDisable();
        v9x_screen_selector = 0u;
        return 0u;
    }
    if (v9x_palettized != 0u) {
        (void)V9xDibSetPaletteTranslateCall(0, device_info);
        pdevice_flags |= V9X_DE_PALETTIZED;
    } else {
        pdevice_flags |= V9X_DE_FIVE6FIVE;
    }

    bitmap_info = (BITMAPINFO FAR *)
        ((BYTE FAR *)device_info + v9x_dib_pdevice_size);
    bitmap_info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap_info->bmiHeader.biWidth = v9x_selected_mode->width;
    bitmap_info->bmiHeader.biHeight = v9x_selected_mode->height;
    bitmap_info->bmiHeader.biPlanes = 1u;
    bitmap_info->bmiHeader.biBitCount = v9x_selected_mode->bits_per_pixel;
    bitmap_info->bmiHeader.biCompression = BI_RGB;
    bitmap_info->bmiHeader.biSizeImage = v9x_active_visible_bytes;
    bitmap_info->bmiHeader.biXPelsPerMeter = 0;
    bitmap_info->bmiHeader.biYPelsPerMeter = 0;
    bitmap_info->bmiHeader.biClrUsed =
        v9x_palettized != 0u ? V9X_PALETTE_ENTRIES : 0u;
    bitmap_info->bmiHeader.biClrImportant = bitmap_info->bmiHeader.biClrUsed;

    if (v9x_palettized != 0u) {
        v9x_color_table = bitmap_info->bmiColors;
        v9x_build_palette(v9x_color_table);
    } else {
        v9x_color_table = 0;
    }
    created = V9xCreateDibPDeviceCall(bitmap_info, device_info,
                                     MAKELP(v9x_screen_selector, 0u),
                                     pdevice_flags);
    if (created == 0ul) {
        v9x_boot_trace("fail-create-pdevice");
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
    if (v9x_palettized != 0u) {
        v9x_program_palette(0u, V9X_PALETTE_ENTRIES);
    }
    v9x_enabled = 1u;
    v9x_active_mode = v9x_selected_mode;
    v9x_serial_write("V9X-DRV lfb=0x");
    v9x_serial_write_hex32(V9xHardwareBase());
    v9x_serial_write(" bytes=00400000\r\n");
    v9x_serial_write_mode("V9X-DRV enable-ok mode=");
    v9x_serial_write(" lfb-mapped\r\n");
    v9x_boot_trace("enable-ok");
    return 1u;
}

WORD __loadds FAR PASCAL Enable(LPVOID device_info,
                                WORD action,
                                LPSTR destination_type,
                                LPSTR output_file,
                                LPVOID data)
{
    if ((action & 1u) != 0u) {
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
    v9x_active_mode = 0;
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

    if (device == 0 || gdi_info == 0 || v9x_enabled == 0u ||
        v9x_active_mode == 0) {
        return 0u;
    }
    if (v9x_fill_gdi_info((V9X_GDI_INFO FAR *)gdi_info, 0, 0, 0) == 0u ||
        V9xHardwareReset() == 0u) {
        v9x_serial_write("V9X-DRV reenable-fail\r\n");
        return 0u;
    }
    device->deFlags &= (WORD)~V9X_DE_BUSY;
    if (v9x_palettized != 0u) {
        v9x_program_palette(0u, V9X_PALETTE_ENTRIES);
    }
    V9xVddPostMode();
    v9x_serial_write("V9X-DRV reenable-ok\r\n");
    return 1u;
}

WORD __loadds FAR PASCAL ValidateMode(LPVOID display_info)
{
    V9X_DISPLAY_VALIDATE_MODE FAR *mode =
        (V9X_DISPLAY_VALIDATE_MODE FAR *)display_info;
    const V9X_DISPLAY_MODE *candidate;

    if (mode == 0 || mode->size < sizeof(*mode)) {
        return V9X_VALMODE_NO_WRONG_DRIVER;
    }
    if (V9xHardwarePresent() == 0u) {
        return V9X_VALMODE_NO_WRONG_DRIVER;
    }
    candidate = v9x_find_mode((WORD)mode->width, (WORD)mode->height,
                              mode->bits_per_pixel);
    if (candidate == 0) {
        return V9X_VALMODE_NO_NOMEM;
    }
    return V9X_VALMODE_YES;
}

DWORD __loadds FAR PASCAL SetPalette(WORD start,
                                     WORD count,
                                     LPVOID palette)
{
    DWORD result;

    if (v9x_driver_pdevice == 0 || palette == 0 || v9x_palettized == 0u) {
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
        if (v9x_palettized != 0u) {
            v9x_program_palette(0u, V9X_PALETTE_ENTRIES);
        }
    }
}
