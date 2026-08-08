#ifndef VELOCITY9X_WIN9X_DISPLAY_ABI_H
#define VELOCITY9X_WIN9X_DISPLAY_ABI_H

/*
 * Minimal public Windows 9x display/DDI structures used by Velocity9x.
 * These declarations intentionally cover only the fields needed by the
 * fixed 8-bpp DIB Engine bring-up paths.
 */

#include <windows.h>

#define V9X_GDIINFO_SIZE             110u
#define V9X_DIBENGINE_SIZE            48u

#define V9X_DRV_VERSION           0x0400u
#define V9X_DT_RASDISPLAY         0x0001u
#define V9X_DC_IGNORE_DFNP        0x0004u
#define V9X_RC_PALETTE            0x0100u
#define V9X_RC_DIBTODEV           0x0200u
#define V9X_C1_DIBENGINE          0x0010u
#define V9X_C1_REINIT_ABLE        0x0080u
#define V9X_C1_BYTE_PACKED        0x0400u
#define V9X_C1_COLORCURSOR        0x0800u
#define V9X_C1_SLOW_CARD          0x2000u

#define V9X_DE_MINIDRIVER         0x0001u
#define V9X_DE_PALETTIZED         0x0002u
#define V9X_DE_BUSY               0x0010u
#define V9X_DE_FIVE6FIVE          0x0040u
#define V9X_DE_VRAM               0x8000u
#define V9X_DE_VERSION            0x0400u

#define V9X_VALMODE_YES                0u
#define V9X_VALMODE_NO_WRONG_DRIVER    1u
#define V9X_VALMODE_NO_NOMEM           2u

typedef struct v9x_point_type {
    short x;
    short y;
} V9X_POINT_TYPE;

typedef struct v9x_gdi_info {
    short dpVersion;
    short dpTechnology;
    short dpHorzSize;
    short dpVertSize;
    short dpHorzRes;
    short dpVertRes;
    short dpBitsPixel;
    short dpPlanes;
    short dpNumBrushes;
    short dpNumPens;
    short dpCapsFE;
    short dpNumFonts;
    short dpNumColors;
    short dpDEVICEsize;
    WORD dpCurves;
    WORD dpLines;
    WORD dpPolygonals;
    WORD dpText;
    WORD dpClip;
    WORD dpRaster;
    short dpAspectX;
    short dpAspectY;
    short dpAspectXY;
    short dpStyleLen;
    V9X_POINT_TYPE dpMLoWin;
    V9X_POINT_TYPE dpMLoVpt;
    V9X_POINT_TYPE dpMHiWin;
    V9X_POINT_TYPE dpMHiVpt;
    V9X_POINT_TYPE dpELoWin;
    V9X_POINT_TYPE dpELoVpt;
    V9X_POINT_TYPE dpEHiWin;
    V9X_POINT_TYPE dpEHiVpt;
    V9X_POINT_TYPE dpTwpWin;
    V9X_POINT_TYPE dpTwpVpt;
    short dpLogPixelsX;
    short dpLogPixelsY;
    short dpDCManage;
    WORD dpCaps1;
    short futureUse4;
    short futureUse5;
    short futureUse6;
    short futureUse7;
    WORD dpNumPalReg;
    WORD dpPalReserved;
    WORD dpColorRes;
} V9X_GDI_INFO;

typedef void (FAR PASCAL *V9X_ACCESS_PROC)(void);

typedef struct v9x_dib_engine {
    WORD deType;
    WORD deWidth;
    WORD deHeight;
    WORD deWidthBytes;
    BYTE dePlanes;
    BYTE deBitsPixel;
    DWORD deReserved1;
    DWORD deDeltaScan;
    LPBYTE delpPDevice;
    DWORD deBitsOffset;
    WORD deBitsSelector;
    WORD deFlags;
    WORD deVersion;
    LPBITMAPINFO deBitmapInfo;
    V9X_ACCESS_PROC deBeginAccess;
    V9X_ACCESS_PROC deEndAccess;
    DWORD deDriverReserved;
} V9X_DIB_ENGINE;

typedef struct v9x_display_validate_mode {
    WORD size;
    WORD bits_per_pixel;
    short width;
    short height;
} V9X_DISPLAY_VALIDATE_MODE;

/* Legacy prefix returned by the master VDD's VDD_GET_DISPLAY_CONFIG API. */
typedef struct v9x_display_info {
    WORD header_size;
    WORD info_flags;
    DWORD device_node;
    char driver_name[16];
    WORD width;
    WORD height;
    WORD dpi;
    BYTE planes;
    BYTE bits_per_pixel;
    WORD maximum_refresh;
} V9X_DISPLAY_INFO;

typedef char v9x_assert_gdi_info_size[
    sizeof(V9X_GDI_INFO) == V9X_GDIINFO_SIZE ? 1 : -1];
typedef char v9x_assert_dib_engine_size[
    sizeof(V9X_DIB_ENGINE) == V9X_DIBENGINE_SIZE ? 1 : -1];
typedef char v9x_assert_validate_mode_size[
    sizeof(V9X_DISPLAY_VALIDATE_MODE) == 8u ? 1 : -1];
typedef char v9x_assert_display_info_size[
    sizeof(V9X_DISPLAY_INFO) == 34u ? 1 : -1];

#endif
