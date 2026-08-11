/*
 * V9XHAL.DLL - flat 32-bit DirectDraw HAL for Velocity9x (S3 ViRGE/DX).
 *
 * DDRAW loads this DLL by the name returned from the 16-bit driver's
 * DDGET32BITDRIVERNAME escape and calls DriverInit with the linear address
 * of the shared V9X_DD_SHARED block. This module owns all DirectDraw
 * content: caps, mode table, callback tables, heap policy, and the runtime
 * callbacks. Flip programs the S3 CRTC display-start registers directly
 * (ring-3 port I/O; the driver's VDD registration stopped VGA trapping).
 *
 * The DLL is linked at a fixed base inside the Win9x shared arena with all
 * sections marked shared, so the flat callback pointers stored in the
 * shared block are valid in every process.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "velocity9x/win9x_ddraw_abi.h"

#ifndef V9X_BUILD_ID
#define V9X_BUILD_ID "local"
#endif

#define V9X_HAL_BASE            0xb0400000ul

#define V9X_CRTC_INDEX              0x03d4u
#define V9X_CRTC_DATA               0x03d5u
#define V9X_INPUT_STATUS_1          0x03dau
#define V9X_STATUS_VBLANK              0x08u

/* Bounded vblank polling so a broken timing source cannot hang a caller. */
#define V9X_VBLANK_SPIN_LIMIT   0x00200000ul

static const char v9x_hal_build_id[] = "V9XHAL build=" V9X_BUILD_ID;

static V9X_DD_SHARED *v9x_hal;

static unsigned char v9x_inp(unsigned short port);
#pragma aux v9x_inp = "in al,dx" parm [dx] value [al] modify exact [al];

static void v9x_outp(unsigned short port, unsigned char value);
#pragma aux v9x_outp = "out dx,al" parm [dx] [al] modify exact [];

static void v9x_fpu_save(void *area);
#pragma aux v9x_fpu_save = "fnsave [eax]" parm [eax] modify exact [];

static void v9x_fpu_restore(void *area);
#pragma aux v9x_fpu_restore = "frstor [eax]" parm [eax] modify exact [];

/* 32-bit protected-mode FNSAVE area is 108 bytes. */
typedef struct v9x_fpu_area {
    char bytes[112];
} V9X_FPU_AREA;

static unsigned char v9x_read_crtc(unsigned char index)
{
    v9x_outp(V9X_CRTC_INDEX, index);
    return v9x_inp(V9X_CRTC_DATA);
}

static void v9x_write_crtc(unsigned char index, unsigned char value)
{
    v9x_outp(V9X_CRTC_INDEX, index);
    v9x_outp(V9X_CRTC_DATA, value);
}

static int v9x_in_vblank(void)
{
    return (v9x_inp(V9X_INPUT_STATUS_1) & V9X_STATUS_VBLANK) != 0u;
}

/*
 * Program the S3 display start address. The offset is expressed in
 * doublewords: CR0D holds bits 7:0, CR0C bits 15:8, and the low nibble of
 * CR69 bits 19:16 (the high nibble must be preserved).
 */
static void v9x_set_display_start(DWORD byte_offset)
{
    DWORD start = byte_offset >> 2;
    unsigned char extension;

    v9x_write_crtc(0x0du, (unsigned char)(start & 0xfful));
    v9x_write_crtc(0x0cu, (unsigned char)((start >> 8) & 0xfful));
    extension = v9x_read_crtc(0x69u);
    extension = (unsigned char)((extension & 0xf0u) |
                                (unsigned char)((start >> 16) & 0x0ful));
    v9x_write_crtc(0x69u, extension);
}

static DWORD v9x_surface_offset(const V9X_DD_SURFACE_LCL *surface)
{
    DWORD address;

    if (surface == 0 || surface->lpGbl == 0 || v9x_hal == 0 ||
        (v9x_hal->fb.flags & V9X_DD_FB_VALID) == 0ul) {
        return 0xfffffffful;
    }
    address = surface->lpGbl->fpVidMem;
    if (address < v9x_hal->fb.linear_base ||
        address >= v9x_hal->fb.linear_base + v9x_hal->fb.vram_bytes) {
        return 0xfffffffful;
    }
    return address - v9x_hal->fb.linear_base;
}

static DWORD v9x_flip_body(V9X_DDHAL_FLIPDATA *data)
{
    DWORD offset = v9x_surface_offset(data->lpSurfTarg);

    if (offset == 0xfffffffful) {
        data->ddRVal = V9X_DD_OK;
        return V9X_DDHAL_DRIVER_NOTHANDLED;
    }
    if (data->lpSurfCurr != 0 &&
        (data->lpSurfCurr->ddsCaps & V9X_DDSCAPS_PRIMARYSURFACE) != 0ul) {
        v9x_set_display_start(offset);
    }
    data->ddRVal = V9X_DD_OK;
    return V9X_DDHAL_DRIVER_HANDLED;
}

DWORD __stdcall V9xHalFlip(V9X_DDHAL_FLIPDATA *data)
{
    V9X_FPU_AREA fpu;
    DWORD result;

    v9x_fpu_save(&fpu);
    result = v9x_flip_body(data);
    v9x_fpu_restore(&fpu);
    return result;
}

DWORD __stdcall V9xHalGetFlipStatus(V9X_DDHAL_GETFLIPSTATUSDATA *data)
{
    data->ddRVal = V9X_DD_OK;
    return V9X_DDHAL_DRIVER_HANDLED;
}

/* DDRAW leaves exclusive mode: the visible page must return to the GDI
 * surface at the start of VRAM. Layout: {lpDD, dwToGDI, ddRVal, fn}. */
typedef struct v9x_ddhal_fliptogdidata {
    DWORD lpDD;
    DWORD dwToGDI;
    DWORD ddRVal;
    DWORD FlipToGDISurface;
} V9X_DDHAL_FLIPTOGDIDATA;

DWORD __stdcall V9xHalFlipToGDISurface(V9X_DDHAL_FLIPTOGDIDATA *data)
{
    if (data->dwToGDI != 0ul) {
        v9x_set_display_start(0ul);
    }
    data->ddRVal = V9X_DD_OK;
    return V9X_DDHAL_DRIVER_HANDLED;
}

DWORD __stdcall V9xHalLock(V9X_DDHAL_LOCKDATA *data)
{
    /* DDRAW computes the surface pointer itself from fpVidMem. */
    data->ddRVal = V9X_DD_OK;
    return V9X_DDHAL_DRIVER_NOTHANDLED;
}

DWORD __stdcall V9xHalUnlock(V9X_DDHAL_UNLOCKDATA *data)
{
    data->ddRVal = V9X_DD_OK;
    return V9X_DDHAL_DRIVER_NOTHANDLED;
}

DWORD __stdcall V9xHalWaitForVerticalBlank(
    V9X_DDHAL_WAITFORVERTICALBLANKDATA *data)
{
    DWORD spins;

    switch (data->dwFlags) {
    case V9X_DDWAITVB_I_TESTVB:
        data->bIsInVB = v9x_in_vblank() ? 1ul : 0ul;
        data->ddRVal = V9X_DD_OK;
        return V9X_DDHAL_DRIVER_HANDLED;
    case V9X_DDWAITVB_BLOCKBEGIN:
        spins = V9X_VBLANK_SPIN_LIMIT;
        while (v9x_in_vblank() && spins-- != 0ul) {
        }
        spins = V9X_VBLANK_SPIN_LIMIT;
        while (!v9x_in_vblank() && spins-- != 0ul) {
        }
        data->ddRVal = V9X_DD_OK;
        return V9X_DDHAL_DRIVER_HANDLED;
    case V9X_DDWAITVB_BLOCKEND:
        spins = V9X_VBLANK_SPIN_LIMIT;
        while (!v9x_in_vblank() && spins-- != 0ul) {
        }
        spins = V9X_VBLANK_SPIN_LIMIT;
        while (v9x_in_vblank() && spins-- != 0ul) {
        }
        data->ddRVal = V9X_DD_OK;
        return V9X_DDHAL_DRIVER_HANDLED;
    default:
        data->ddRVal = V9X_DD_OK;
        return V9X_DDHAL_DRIVER_NOTHANDLED;
    }
}

static void v9x_fill_modes(V9X_DD_SHARED *shared)
{
    static const struct {
        DWORD width;
        DWORD height;
        LONG pitch;
        DWORD bpp;
    } modes[V9X_DD_MODE_COUNT] = {
        {  640ul, 480ul,  640l,  8ul },
        {  800ul, 600ul,  800l,  8ul },
        { 1024ul, 768ul, 1024l,  8ul },
        {  640ul, 480ul, 1280l, 16ul },
        {  800ul, 600ul, 1600l, 16ul },
        { 1024ul, 768ul, 2048l, 16ul }
    };
    DWORD index;

    for (index = 0ul; index < V9X_DD_MODE_COUNT; ++index) {
        V9X_DDHALMODEINFO *mode = &shared->modes[index];

        mode->dwWidth = modes[index].width;
        mode->dwHeight = modes[index].height;
        mode->lPitch = modes[index].pitch;
        mode->dwBPP = modes[index].bpp;
        mode->wRefreshRate = 60u;
        if (modes[index].bpp == 8ul) {
            mode->wFlags = V9X_DDMODEINFO_PALETTIZED;
            mode->dwRBitMask = 0ul;
            mode->dwGBitMask = 0ul;
            mode->dwBBitMask = 0ul;
        } else {
            mode->wFlags = 0u;
            mode->dwRBitMask = 0x0000f800ul;
            mode->dwGBitMask = 0x000007e0ul;
            mode->dwBBitMask = 0x0000001ful;
        }
        mode->dwAlphaBitMask = 0ul;
    }
}

DWORD __stdcall DriverInit(DWORD context)
{
    V9X_DD_SHARED *shared = (V9X_DD_SHARED *)context;

    if (shared == 0 || shared->dwSize != sizeof(V9X_DD_SHARED) ||
        shared->abi != V9X_DD_SHARED_ABI) {
        return 0ul;
    }
    v9x_hal = shared;

    v9x_fill_modes(shared);

    shared->info.dwSize = sizeof(V9X_DDHALINFO);
    shared->info.dwNumModes = V9X_DD_MODE_COUNT;
    shared->info.dwFlags = V9X_DDHALINFO_ISPRIMARYDISPLAY;
    shared->info.dwMonitorFrequency = 60ul;
    shared->info.hInstance = V9X_HAL_BASE;
    shared->info.lpD3DGlobalDriverData = 0ul;
    shared->info.lpD3DHALCallbacks = 0ul;
    shared->info.lpDDExeBufCallbacks = 0;

    shared->info.vmiData.dwFlags = 0ul;
    shared->info.vmiData.dwOffscreenAlign = 8ul;
    shared->info.vmiData.dwOverlayAlign = 8ul;
    shared->info.vmiData.dwTextureAlign = 8ul;
    shared->info.vmiData.dwZBufferAlign = 8ul;
    shared->info.vmiData.dwAlphaAlign = 8ul;
    shared->info.vmiData.dwNumHeaps = 1ul;

    shared->info.ddCaps.dwSize = sizeof(V9X_DDCORECAPS);
    shared->info.ddCaps.dwCaps = V9X_DDCAPS_GDI;
    shared->info.ddCaps.ddsCaps = V9X_DDSCAPS_OFFSCREENPLAIN |
                                  V9X_DDSCAPS_FLIP |
                                  V9X_DDSCAPS_PRIMARYSURFACE;
    shared->info.ddCaps.dwVidMemTotal =
        shared->fb.vram_bytes - shared->fb.visible_bytes;
    shared->info.ddCaps.dwVidMemFree = shared->info.ddCaps.dwVidMemTotal;

    shared->dd_callbacks.dwSize = sizeof(V9X_DDHAL_DDCALLBACKS);
    shared->dd_callbacks.dwFlags = V9X_DDHAL_CB32_WAITFORVERTICALBLANK |
                                   V9X_DDHAL_CB32_FLIPTOGDISURFACE;
    shared->dd_callbacks.WaitForVerticalBlank =
        (V9X_DD_CODE_PTR)V9xHalWaitForVerticalBlank;
    shared->dd_callbacks.FlipToGDISurface =
        (V9X_DD_CODE_PTR)V9xHalFlipToGDISurface;

    shared->surface_callbacks.dwSize = sizeof(V9X_DDHAL_DDSURFACECALLBACKS);
    shared->surface_callbacks.dwFlags =
        V9X_DDHAL_SURFCB32_FLIP | V9X_DDHAL_SURFCB32_GETFLIPSTATUS |
        V9X_DDHAL_SURFCB32_LOCK | V9X_DDHAL_SURFCB32_UNLOCK;
    shared->surface_callbacks.Flip = (V9X_DD_CODE_PTR)V9xHalFlip;
    shared->surface_callbacks.GetFlipStatus =
        (V9X_DD_CODE_PTR)V9xHalGetFlipStatus;
    shared->surface_callbacks.Lock = (V9X_DD_CODE_PTR)V9xHalLock;
    shared->surface_callbacks.Unlock = (V9X_DD_CODE_PTR)V9xHalUnlock;

    shared->palette_callbacks.dwSize =
        sizeof(V9X_DDHAL_DDPALETTECALLBACKS);
    shared->palette_callbacks.dwFlags = 0ul;

    shared->cb32.Flip = (DWORD)V9xHalFlip;
    shared->cb32.GetFlipStatus = (DWORD)V9xHalGetFlipStatus;
    shared->cb32.Lock = (DWORD)V9xHalLock;
    shared->cb32.Unlock = (DWORD)V9xHalUnlock;
    shared->cb32.WaitForVerticalBlank =
        (DWORD)V9xHalWaitForVerticalBlank;
    shared->cb32.flags = 0ul;

    shared->hInstance = V9X_HAL_BASE;
    shared->driver_init_done = 1ul;
    return 1ul;
}

BOOL __stdcall V9xHalEntry(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)instance;
    (void)reason;
    (void)reserved;
    /* Keep the linker-visible reference to the build marker. */
    return v9x_hal_build_id[0] != '\0';
}
