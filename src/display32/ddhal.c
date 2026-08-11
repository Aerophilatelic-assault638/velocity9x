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

/* ViRGE linear-aperture register offsets. The active DPMI mapping covers
 * 64 MiB, while DirectDraw exposes only the first 4 MiB as allocatable VRAM. */
#define V9X_VIRGE_ENGINE_STATUS       0x00008504ul
#define V9X_VIRGE_DEST_BASE           0x0000a4d8ul
#define V9X_VIRGE_MONO_PAT_0          0x0000a4e8ul
#define V9X_VIRGE_MONO_PAT_1          0x0000a4ecul
#define V9X_VIRGE_DEST_SRC_STRIDE     0x0000a4e4ul
#define V9X_VIRGE_PATTERN_FG          0x0000a4f4ul
#define V9X_VIRGE_COMMAND             0x0000a500ul
#define V9X_VIRGE_RECT_WH             0x0000a504ul
#define V9X_VIRGE_RECT_DEST_XY        0x0000a50cul

/* ViRGE S3D setup and triangle register windows (new MMIO). */
#define V9X_VIRGE_3D_Z_BASE           0x0000b4d4ul
#define V9X_VIRGE_3D_DEST_BASE        0x0000b4d8ul
#define V9X_VIRGE_3D_CLIP_L_R         0x0000b4dcul
#define V9X_VIRGE_3D_CLIP_T_B         0x0000b4e0ul
#define V9X_VIRGE_3D_DEST_SRC_STRIDE  0x0000b4e4ul
#define V9X_VIRGE_3D_Z_STRIDE         0x0000b4e8ul
#define V9X_VIRGE_3D_TEX_BASE         0x0000b4ecul
#define V9X_VIRGE_3D_TEX_BORDER       0x0000b4f0ul
#define V9X_VIRGE_3D_FADE_COLOR       0x0000b4f4ul
#define V9X_VIRGE_3D_COMMAND          0x0000b500ul
#define V9X_VIRGE_3D_DGDX_DBDX        0x0000b53cul
#define V9X_VIRGE_3D_DADX_DRDX        0x0000b540ul
#define V9X_VIRGE_3D_DGDY_DBDY        0x0000b544ul
#define V9X_VIRGE_3D_DADY_DRDY        0x0000b548ul
#define V9X_VIRGE_3D_GS_BS            0x0000b54cul
#define V9X_VIRGE_3D_AS_RS            0x0000b550ul
#define V9X_VIRGE_3D_DXDY12           0x0000b560ul
#define V9X_VIRGE_3D_XEND12           0x0000b564ul
#define V9X_VIRGE_3D_DXDY01           0x0000b568ul
#define V9X_VIRGE_3D_XEND01           0x0000b56cul
#define V9X_VIRGE_3D_DXDY02           0x0000b570ul
#define V9X_VIRGE_3D_XSTART02         0x0000b574ul
#define V9X_VIRGE_3D_YSTART           0x0000b578ul
#define V9X_VIRGE_3D_Y01_Y12          0x0000b57cul

#define V9X_VIRGE_3D_CMD_FLAT_16_AE   0x83000007ul

#define V9X_VIRGE_STATUS_FIFO_SHIFT             8u
#define V9X_VIRGE_STATUS_FIFO_MASK       0x00001f00ul
#define V9X_VIRGE_STATUS_IDLE            0x00002000ul
#define V9X_VIRGE_FIFO_SPIN_LIMIT        0x00200000ul
#define V9X_VIRGE_IDLE_SPIN_LIMIT        0x00400000ul

#define V9X_VIRGE_CMD_DRAW_ENABLE        0x00000020ul
#define V9X_VIRGE_CMD_MONO_PATTERN       0x00000100ul
#define V9X_VIRGE_CMD_ROP_PATCOPY        (0x000000f0ul << 17)
#define V9X_VIRGE_CMD_X_POSITIVE         0x02000000ul
#define V9X_VIRGE_CMD_Y_POSITIVE         0x04000000ul

static const char v9x_hal_build_id[] = "V9XHAL build=" V9X_BUILD_ID;

static V9X_DD_SHARED *v9x_hal;

#define V9X_D3D_CONTEXT_COUNT 16u

typedef struct v9x_d3d_context {
    DWORD active;
    DWORD pid;
    V9X_DD_SURFACE_LCL *target;
    V9X_DD_SURFACE_LCL *zbuffer;
    DWORD target_offset;
    DWORD pitch;
    DWORD width;
    DWORD height;
} V9X_D3D_CONTEXT;

static V9X_D3D_CONTEXT v9x_d3d_contexts[V9X_D3D_CONTEXT_COUNT];
static V9X_D3DHAL_CALLBACKS2 v9x_d3d_callbacks2;

static const BYTE v9x_guid_d3d_callbacks2[16] = {
    0xe1u, 0x84u, 0xa5u, 0x0bu, 0xb6u, 0x70u, 0xd0u, 0x11u,
    0x88u, 0x9du, 0x00u, 0xaau, 0x00u, 0xbbu, 0xb7u, 0x6au
};

static unsigned char v9x_inp(unsigned short port);
#pragma aux v9x_inp = "in al,dx" parm [dx] value [al] modify exact [al];

static void v9x_outp(unsigned short port, unsigned char value);
#pragma aux v9x_outp = "out dx,al" parm [dx] [al] modify exact [];

static void v9x_fpu_save(void *area);
#pragma aux v9x_fpu_save = "fnsave [eax]" parm [eax] modify exact [];

static void v9x_fpu_restore(void *area);
#pragma aux v9x_fpu_restore = "frstor [eax]" parm [eax] modify exact [];

static LONG v9x_float_to_long(float value);
#pragma aux v9x_float_to_long = \
    "sub esp,4" \
    "fistp dword ptr [esp]" \
    "pop eax" \
    parm [8087] value [eax] modify exact [eax];

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

static int v9x_engine_ready(void)
{
    return v9x_hal != 0 &&
           (v9x_hal->fb.flags & V9X_DD_FB_VALID) != 0ul &&
           (v9x_hal->engine.flags &
            (V9X_DD_ENGINE_VALID | V9X_DD_ENGINE_S3_VIRGE_DX)) ==
            (V9X_DD_ENGINE_VALID | V9X_DD_ENGINE_S3_VIRGE_DX) &&
           v9x_hal->engine.control_linear_base != 0ul &&
           v9x_hal->engine.mapped_aperture_bytes >
               V9X_VIRGE_RECT_DEST_XY + sizeof(DWORD);
}

static int v9x_engine_status_validated(void)
{
    return v9x_engine_ready() &&
           (v9x_hal->engine.flags &
            V9X_DD_ENGINE_STATUS_VALIDATED) != 0ul;
}

static DWORD v9x_mmio_read(DWORD offset)
{
    volatile DWORD *reg = (volatile DWORD *)
        (v9x_hal->engine.control_linear_base + offset);

    return *reg;
}

static void v9x_mmio_write(DWORD offset, DWORD value)
{
    volatile DWORD *reg = (volatile DWORD *)
        (v9x_hal->engine.control_linear_base + offset);

    *reg = value;
}

static DWORD v9x_engine_status(void)
{
    return v9x_mmio_read(V9X_VIRGE_ENGINE_STATUS);
}

static DWORD v9x_fifo_free(DWORD status)
{
    return (status & V9X_VIRGE_STATUS_FIFO_MASK) >>
           V9X_VIRGE_STATUS_FIFO_SHIFT;
}

/* CR66 bit 1 is the ViRGE/DX graphics-engine reset used by the Windows 98
 * S3 sample. It is touched only after a bounded wait has expired. */
static void v9x_engine_recover(void)
{
    unsigned char cr66;

    if (!v9x_engine_ready()) {
        return;
    }
    cr66 = v9x_read_crtc(0x66u);
    v9x_write_crtc(0x66u, (unsigned char)(cr66 | 0x02u));
    v9x_write_crtc(0x66u, cr66);
    ++v9x_hal->engine.reset_count;
}

static int v9x_wait_fifo(DWORD entries, int wait)
{
    DWORD spins;

    if (!v9x_engine_ready() || entries == 0ul || entries > 31ul) {
        return 0;
    }
    if (v9x_fifo_free(v9x_engine_status()) >= entries) {
        return 1;
    }
    if (!wait) {
        return 0;
    }
    spins = V9X_VIRGE_FIFO_SPIN_LIMIT;
    while (spins-- != 0ul) {
        if (v9x_fifo_free(v9x_engine_status()) >= entries) {
            return 1;
        }
    }
    ++v9x_hal->engine.fifo_timeouts;
    v9x_engine_recover();
    return 0;
}

static int v9x_wait_idle(int wait)
{
    DWORD spins;

    if (!v9x_engine_ready()) {
        return 0;
    }
    if ((v9x_engine_status() & V9X_VIRGE_STATUS_IDLE) != 0ul) {
        return 1;
    }
    if (!wait) {
        return 0;
    }
    spins = V9X_VIRGE_IDLE_SPIN_LIMIT;
    while (spins-- != 0ul) {
        if ((v9x_engine_status() & V9X_VIRGE_STATUS_IDLE) != 0ul) {
            return 1;
        }
    }
    ++v9x_hal->engine.idle_timeouts;
    v9x_engine_recover();
    return 0;
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
    if (v9x_engine_status_validated() &&
        !v9x_wait_idle((data->dwFlags & V9X_DDFLIP_DONOTWAIT) == 0ul)) {
        data->ddRVal = V9X_DDERR_WASSTILLDRAWING;
        return V9X_DDHAL_DRIVER_HANDLED;
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
    DWORD dwReserved;
    DWORD ddRVal;
    DWORD FlipToGDISurface;
} V9X_DDHAL_FLIPTOGDIDATA;

DWORD __stdcall V9xHalFlipToGDISurface(V9X_DDHAL_FLIPTOGDIDATA *data)
{
    if (data->dwToGDI != 0ul) {
        if (v9x_engine_status_validated() && !v9x_wait_idle(1)) {
            data->ddRVal = V9X_DDERR_WASSTILLDRAWING;
            return V9X_DDHAL_DRIVER_HANDLED;
        }
        v9x_set_display_start(0ul);
    }
    data->ddRVal = V9X_DD_OK;
    return V9X_DDHAL_DRIVER_HANDLED;
}

typedef struct v9x_ddhal_setexclusivemodedata {
    DWORD lpDD;
    DWORD dwEnterExcl;
    DWORD dwReserved;
    DWORD ddRVal;
    DWORD SetExclusiveMode;
} V9X_DDHAL_SETEXCLUSIVEMODEDATA;

DWORD __stdcall V9xHalSetExclusiveMode(
    V9X_DDHAL_SETEXCLUSIVEMODEDATA *data)
{
    if (data == 0) {
        return V9X_DDHAL_DRIVER_HANDLED;
    }
    if (data->dwEnterExcl == 0ul) {
        if (v9x_engine_status_validated() && !v9x_wait_idle(1)) {
            data->ddRVal = V9X_DDERR_WASSTILLDRAWING;
            return V9X_DDHAL_DRIVER_HANDLED;
        }
        v9x_set_display_start(0ul);
    }
    data->ddRVal = V9X_DD_OK;
    return V9X_DDHAL_DRIVER_HANDLED;
}

DWORD __stdcall V9xHalLock(V9X_DDHAL_LOCKDATA *data)
{
    /* Serialize CPU access after asynchronous engine work. DDRAW still
     * computes and returns the actual surface pointer. */
    if (v9x_engine_status_validated() &&
        !v9x_wait_idle((data->dwFlags & V9X_DDLOCK_DONOTWAIT) == 0ul)) {
        data->ddRVal = V9X_DDERR_WASSTILLDRAWING;
        return V9X_DDHAL_DRIVER_HANDLED;
    }
    data->ddRVal = V9X_DD_OK;
    return V9X_DDHAL_DRIVER_NOTHANDLED;
}

DWORD __stdcall V9xHalUnlock(V9X_DDHAL_UNLOCKDATA *data)
{
    data->ddRVal = V9X_DD_OK;
    return V9X_DDHAL_DRIVER_NOTHANDLED;
}

static int v9x_fill_rect_valid(const V9X_DDHAL_BLTDATA *data,
                               DWORD bytes_per_pixel,
                               DWORD *offset_out)
{
    const V9X_DD_SURFACE_GBL *surface;
    DWORD offset;
    DWORD right_bytes;
    DWORD last_row;

    if (data == 0 || data->lpDDDestSurface == 0 ||
        data->lpDDDestSurface->lpGbl == 0 || bytes_per_pixel == 0ul ||
        data->rDest[0] < 0l || data->rDest[1] < 0l ||
        data->rDest[2] <= data->rDest[0] ||
        data->rDest[3] <= data->rDest[1]) {
        return 0;
    }
    surface = data->lpDDDestSurface->lpGbl;
    if (data->rDest[2] > (LONG)surface->wWidth ||
        data->rDest[3] > (LONG)surface->wHeight ||
        data->rDest[2] > 2048l || data->rDest[3] > 2048l ||
        surface->lPitch <= 0l || ((DWORD)surface->lPitch & 7ul) != 0ul) {
        return 0;
    }
    offset = v9x_surface_offset(data->lpDDDestSurface);
    if (offset == 0xfffffffful || (offset & 7ul) != 0ul) {
        return 0;
    }
    right_bytes = (DWORD)data->rDest[2] * bytes_per_pixel;
    last_row = (DWORD)(data->rDest[3] - 1l) * (DWORD)surface->lPitch;
    if (right_bytes > v9x_hal->fb.vram_bytes ||
        last_row > v9x_hal->fb.vram_bytes - right_bytes ||
        offset > v9x_hal->fb.vram_bytes - right_bytes - last_row) {
        return 0;
    }
    *offset_out = offset;
    return 1;
}

DWORD __stdcall V9xHalBlt(V9X_DDHAL_BLTDATA *data)
{
    DWORD allowed = V9X_DDBLT_COLORFILL | V9X_DDBLT_WAIT |
                    V9X_DDBLT_DONOTWAIT | V9X_DDBLT_ASYNC;
    DWORD bytes_per_pixel;
    DWORD offset;
    DWORD width;
    DWORD height;
    DWORD command;
    int wait;

    if (!v9x_engine_status_validated() || data == 0 ||
        (data->dwFlags & V9X_DDBLT_COLORFILL) == 0ul ||
        (data->dwFlags & ~allowed) != 0ul || data->lpDDSrcSurface != 0) {
        if (data != 0) {
            data->ddRVal = V9X_DD_OK;
        }
        return V9X_DDHAL_DRIVER_NOTHANDLED;
    }
    bytes_per_pixel = v9x_hal->fb.bits_per_pixel >> 3;
    if (!v9x_fill_rect_valid(data, bytes_per_pixel, &offset)) {
        data->ddRVal = V9X_DD_OK;
        return V9X_DDHAL_DRIVER_NOTHANDLED;
    }
    wait = (data->dwFlags &
            (V9X_DDBLT_ASYNC | V9X_DDBLT_DONOTWAIT)) == 0ul;
    if (!v9x_wait_fifo(8ul, wait)) {
        data->ddRVal = V9X_DDERR_WASSTILLDRAWING;
        return V9X_DDHAL_DRIVER_HANDLED;
    }

    width = (DWORD)(data->rDest[2] - data->rDest[0]);
    height = (DWORD)(data->rDest[3] - data->rDest[1]);
    command = V9X_VIRGE_CMD_ROP_PATCOPY |
              V9X_VIRGE_CMD_X_POSITIVE | V9X_VIRGE_CMD_Y_POSITIVE |
              V9X_VIRGE_CMD_MONO_PATTERN | V9X_VIRGE_CMD_DRAW_ENABLE |
              ((bytes_per_pixel - 1ul) << 2);

    v9x_mmio_write(V9X_VIRGE_DEST_BASE, offset);
    v9x_mmio_write(V9X_VIRGE_PATTERN_FG, data->bltFX.dwFillColor);
    v9x_mmio_write(V9X_VIRGE_DEST_SRC_STRIDE,
                   (DWORD)data->lpDDDestSurface->lpGbl->lPitch << 16);
    v9x_mmio_write(V9X_VIRGE_MONO_PAT_0, 0xfffffffful);
    v9x_mmio_write(V9X_VIRGE_MONO_PAT_1, 0xfffffffful);
    v9x_mmio_write(V9X_VIRGE_RECT_WH, ((width - 1ul) << 16) | height);
    v9x_mmio_write(V9X_VIRGE_RECT_DEST_XY,
                   ((DWORD)data->rDest[0] << 16) |
                   (DWORD)data->rDest[1]);
    v9x_mmio_write(V9X_VIRGE_COMMAND, command);

    data->ddRVal = V9X_DD_OK;
    return V9X_DDHAL_DRIVER_HANDLED;
}

DWORD __stdcall V9xHalGetBltStatus(V9X_DDHAL_GETBLTSTATUSDATA *data)
{
    int ready;
    DWORD status;

    if (!v9x_engine_ready()) {
        data->ddRVal = V9X_DD_OK;
        return V9X_DDHAL_DRIVER_NOTHANDLED;
    }
    if (data->dwFlags == V9X_DDGBS_CANBLT) {
        status = v9x_engine_status();
        ready = v9x_fifo_free(status) >= 8ul &&
                (status & V9X_VIRGE_STATUS_IDLE) != 0ul;
        if (ready) {
            v9x_hal->engine.flags |= V9X_DD_ENGINE_STATUS_VALIDATED;
        }
    } else if (data->dwFlags == V9X_DDGBS_ISBLTDONE) {
        if (!v9x_engine_status_validated()) {
            data->ddRVal = V9X_DD_OK;
            return V9X_DDHAL_DRIVER_NOTHANDLED;
        }
        ready = (v9x_engine_status() & V9X_VIRGE_STATUS_IDLE) != 0ul;
    } else {
        data->ddRVal = V9X_DD_OK;
        return V9X_DDHAL_DRIVER_NOTHANDLED;
    }
    data->ddRVal = ready ? V9X_DD_OK : V9X_DDERR_WASSTILLDRAWING;
    return V9X_DDHAL_DRIVER_HANDLED;
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

static V9X_D3D_CONTEXT *v9x_d3d_context_from_handle(DWORD handle)
{
    DWORD index;

    for (index = 0ul; index < V9X_D3D_CONTEXT_COUNT; ++index) {
        if ((DWORD)&v9x_d3d_contexts[index] == handle &&
            v9x_d3d_contexts[index].active != 0ul) {
            return &v9x_d3d_contexts[index];
        }
    }
    return 0;
}

static int v9x_d3d_triangle(V9X_D3D_CONTEXT *context,
                            const V9X_D3DTLVERTEX *first);

static V9X_DD_SURFACE_LCL *v9x_d3d_surface_lcl(void *surface)
{
    V9X_DD_SURFACE_INT *wrapper = (V9X_DD_SURFACE_INT *)surface;

    return wrapper != 0 ? wrapper->lpLcl : 0;
}

static int v9x_d3d_set_target(V9X_D3D_CONTEXT *context, void *surface,
                              void *zbuffer)
{
    V9X_DD_SURFACE_LCL *target = v9x_d3d_surface_lcl(surface);
    V9X_DD_SURFACE_GBL *global;
    DWORD offset;
    DWORD last_byte;

    if (context == 0 || target == 0 || target->lpGbl == 0 ||
        (target->ddsCaps & V9X_DDSCAPS_SYSTEMMEMORY) != 0ul) {
        return 0;
    }
    global = target->lpGbl;
    offset = v9x_surface_offset(target);
    if (offset == 0xfffffffful || global->lPitch <= 0l ||
        ((DWORD)global->lPitch & 7ul) != 0ul || global->wWidth == 0u ||
        global->wHeight == 0u || global->wWidth > 2048u ||
        global->wHeight > 2048u) {
        return 0;
    }
    last_byte = (DWORD)(global->wHeight - 1u) * (DWORD)global->lPitch +
                (DWORD)global->wWidth * 2ul;
    if (last_byte > v9x_hal->fb.vram_bytes ||
        offset > v9x_hal->fb.vram_bytes - last_byte) {
        return 0;
    }
    context->target = target;
    context->zbuffer = zbuffer != 0 ? v9x_d3d_surface_lcl(zbuffer) : 0;
    context->target_offset = offset;
    context->pitch = (DWORD)global->lPitch;
    context->width = global->wWidth;
    context->height = global->wHeight;
    return context->zbuffer == 0;
}

DWORD __stdcall V9xD3dContextCreate(V9X_D3DHAL_CONTEXTCREATEDATA *data)
{
    DWORD index;
    V9X_D3D_CONTEXT *context;

    if (data == 0 || v9x_hal == 0 || data->lpDDS == 0 ||
        (v9x_hal->fb.flags & V9X_DD_FB_VALID) == 0ul ||
        v9x_hal->fb.bits_per_pixel != 16ul) {
        if (data != 0) {
            data->ddrval = 0x80070057ul;
        }
        if (v9x_hal != 0) {
            ++v9x_hal->d3d_diagnostics.context_rejects;
        }
        return V9X_DDHAL_DRIVER_HANDLED;
    }
    for (index = 0ul; index < V9X_D3D_CONTEXT_COUNT; ++index) {
        context = &v9x_d3d_contexts[index];
        if (context->active == 0ul) {
            if (!v9x_d3d_set_target(context, data->lpDDS, data->lpDDSZ)) {
                data->ddrval = 0x80070057ul;
                ++v9x_hal->d3d_diagnostics.context_rejects;
                return V9X_DDHAL_DRIVER_HANDLED;
            }
            context->pid = data->dwPID;
            context->active = 1ul;
            data->dwhContext = (DWORD)context;
            data->ddrval = V9X_DD_OK;
            ++v9x_hal->d3d_diagnostics.context_creates;
            return V9X_DDHAL_DRIVER_HANDLED;
        }
    }
    data->ddrval = 0x8007000eul;
    ++v9x_hal->d3d_diagnostics.context_rejects;
    return V9X_DDHAL_DRIVER_HANDLED;
}

DWORD __stdcall V9xD3dContextDestroy(V9X_D3DHAL_CONTEXTDESTROYDATA *data)
{
    V9X_D3D_CONTEXT *context;

    context = data != 0 ? v9x_d3d_context_from_handle(data->dwhContext) : 0;
    if (context == 0) {
        if (data != 0) {
            data->ddrval = 0x80070057ul;
        }
        if (v9x_hal != 0) {
            ++v9x_hal->d3d_diagnostics.context_rejects;
        }
        return V9X_DDHAL_DRIVER_HANDLED;
    }
    context->active = 0ul;
    context->pid = 0ul;
    context->target = 0;
    context->zbuffer = 0;
    context->target_offset = 0ul;
    context->pitch = 0ul;
    context->width = 0ul;
    context->height = 0ul;
    data->ddrval = V9X_DD_OK;
    ++v9x_hal->d3d_diagnostics.context_destroys;
    return V9X_DDHAL_DRIVER_HANDLED;
}

DWORD __stdcall V9xD3dContextDestroyAll(
    V9X_D3DHAL_CONTEXTDESTROYALLDATA *data)
{
    DWORD index;

    if (data == 0) {
        return V9X_DDHAL_DRIVER_HANDLED;
    }
    for (index = 0ul; index < V9X_D3D_CONTEXT_COUNT; ++index) {
        if (v9x_d3d_contexts[index].active != 0ul &&
            v9x_d3d_contexts[index].pid == data->dwPID) {
            v9x_d3d_contexts[index].active = 0ul;
            v9x_d3d_contexts[index].pid = 0ul;
            v9x_d3d_contexts[index].target = 0;
            v9x_d3d_contexts[index].zbuffer = 0;
            v9x_d3d_contexts[index].target_offset = 0ul;
            v9x_d3d_contexts[index].pitch = 0ul;
            v9x_d3d_contexts[index].width = 0ul;
            v9x_d3d_contexts[index].height = 0ul;
        }
    }
    data->ddrval = V9X_DD_OK;
    ++v9x_hal->d3d_diagnostics.context_destroy_alls;
    return V9X_DDHAL_DRIVER_HANDLED;
}

DWORD __stdcall V9xD3dRenderState(V9X_D3DHAL_RENDERSTATEDATA *data)
{
    if (v9x_hal != 0) {
        ++v9x_hal->d3d_diagnostics.render_state_calls;
    }
    if (data != 0) {
        data->ddrval = V9X_DD_OK;
    }
    return V9X_DDHAL_DRIVER_NOTHANDLED;
}

DWORD __stdcall V9xD3dRenderPrimitive(
    V9X_D3DHAL_RENDERPRIMITIVEDATA *data)
{
    V9X_FPU_AREA fpu;
    V9X_D3D_CONTEXT *context;
    V9X_DD_SURFACE_LCL *exe;
    V9X_DD_SURFACE_LCL *tl;
    const V9X_D3DTRIANGLE *triangles;
    const V9X_D3DTLVERTEX *vertices;
    DWORD status;
    DWORD index;
    int ok = 0;

    v9x_fpu_save(&fpu);
    context = data != 0 ? v9x_d3d_context_from_handle(data->dwhContext) : 0;
    exe = data != 0 ? v9x_d3d_surface_lcl(data->lpExeBuf) : 0;
    tl = data != 0 ? v9x_d3d_surface_lcl(data->lpTLBuf) : 0;
    if (!v9x_engine_status_validated() && v9x_engine_ready()) {
        status = v9x_engine_status();
        if (v9x_fifo_free(status) >= 8ul &&
            (status & V9X_VIRGE_STATUS_IDLE) != 0ul) {
            v9x_hal->engine.flags |= V9X_DD_ENGINE_STATUS_VALIDATED;
        }
    }
    if (context != 0 && exe != 0 && exe->lpGbl != 0 && tl != 0 &&
        tl->lpGbl != 0 && v9x_engine_status_validated() &&
        data->diInstruction.bOpcode == 3u &&
        data->diInstruction.bSize >= sizeof(V9X_D3DTRIANGLE) &&
        data->diInstruction.wCount <= 64u) {
        triangles = (const V9X_D3DTRIANGLE *)
            (exe->lpGbl->fpVidMem + data->dwOffset);
        vertices = (const V9X_D3DTLVERTEX *)
            (tl->lpGbl->fpVidMem + data->dwTLOffset);
        ok = 1;
        for (index = 0ul; index < data->diInstruction.wCount; ++index) {
            const V9X_D3DTRIANGLE *triangle =
                (const V9X_D3DTRIANGLE *)
                ((const BYTE *)triangles +
                 index * data->diInstruction.bSize);
            if (triangle->v1 >= 192u || triangle->v2 >= 192u ||
                triangle->v3 >= 192u) {
                ok = 0;
                break;
            }
            {
                V9X_D3DTLVERTEX ordered[3];

                ordered[0] = vertices[triangle->v1];
                ordered[1] = vertices[triangle->v2];
                ordered[2] = vertices[triangle->v3];
                if (!v9x_d3d_triangle(context, ordered)) {
                    ok = 0;
                    break;
                }
            }
        }
    }
    if (v9x_hal != 0) {
        ++v9x_hal->d3d_diagnostics.render_primitive_calls;
    }
    if (data != 0) {
        data->ddrval = ok ? V9X_DD_OK : 0x80070057ul;
    }
    v9x_fpu_restore(&fpu);
    return V9X_DDHAL_DRIVER_HANDLED;
}

DWORD __stdcall V9xD3dSetRenderTarget(
    V9X_D3DHAL_SETRENDERTARGETDATA *data)
{
    V9X_D3D_CONTEXT *context = data != 0
        ? v9x_d3d_context_from_handle(data->dwhContext) : 0;

    if (context == 0 ||
        !v9x_d3d_set_target(context, data->lpDDS, data->lpDDSZ)) {
        if (data != 0) {
            data->ddrval = 0x80070057ul;
        }
        return V9X_DDHAL_DRIVER_HANDLED;
    }
    data->ddrval = V9X_DD_OK;
    return V9X_DDHAL_DRIVER_HANDLED;
}

static LONG v9x_d3d_fixed_12_20(float value)
{
    return v9x_float_to_long(value * 1048576.0f);
}

static int v9x_d3d_triangle(V9X_D3D_CONTEXT *context,
                            const V9X_D3DTLVERTEX *first)
{
    const V9X_D3DTLVERTEX *p0 = &first[0];
    const V9X_D3DTLVERTEX *p1 = &first[1];
    const V9X_D3DTLVERTEX *p2 = &first[2];
    const V9X_D3DTLVERTEX *temp;
    LONG i0y, i1y, i2y;
    LONG dy01, dy12, dy02;
    float fdy01, fdy12, fdy02r, fdycc;
    float dxdy01, dxdy12, dxdy02, dx;
    DWORD color = first->color;
    DWORD gs_bs;
    DWORD as_rs;

    if (!(p0->sx >= 0.0f && p0->sx <= (float)(context->width - 1ul) &&
          p0->sy >= 0.0f && p0->sy <= (float)(context->height - 1ul) &&
          p1->sx >= 0.0f && p1->sx <= (float)(context->width - 1ul) &&
          p1->sy >= 0.0f && p1->sy <= (float)(context->height - 1ul) &&
          p2->sx >= 0.0f && p2->sx <= (float)(context->width - 1ul) &&
          p2->sy >= 0.0f && p2->sy <= (float)(context->height - 1ul))) {
        return 0;
    }
    if (p2->sy > p1->sy) { temp = p2; p2 = p1; p1 = temp; }
    if (p2->sy > p0->sy) { temp = p2; p2 = p0; p0 = temp; }
    if (p1->sy > p0->sy) { temp = p1; p1 = p0; p0 = temp; }

    i2y = v9x_float_to_long(p2->sy);
    i0y = v9x_float_to_long(p0->sy);
    dy02 = i0y - i2y;
    if (dy02 == 0l || p0->sy == p2->sy) {
        return 1;
    }
    i1y = v9x_float_to_long(p1->sy);
    dy12 = i1y - i2y;
    dy01 = i0y - i1y;
    fdy02r = 1.0f / (p0->sy - p2->sy);
    fdy01 = p0->sy - p1->sy;
    fdy12 = p1->sy - p2->sy;
    fdycc = p0->sy - (float)i0y;
    if (fdycc == 0.0f && dy01 == 0l) {
        if (dy02 <= 1l) {
            return 1;
        }
        --i0y;
        --i1y;
        fdycc = 1.0f;
    }
    dxdy12 = fdy12 != 0.0f ? (p2->sx - p1->sx) / fdy12 : 0.0f;
    dxdy01 = fdy01 != 0.0f ? (p1->sx - p0->sx) / fdy01 : 0.0f;
    dxdy02 = (p2->sx - p0->sx) * fdy02r;
    dx = p1->sx - (fdy01 * dxdy02 + p0->sx);
    if (dx > -0.000002f && dx < 0.000002f) {
        return 1;
    }

    if (!v9x_wait_idle(1) || !v9x_wait_fifo(9ul, 1)) {
        return 0;
    }
    v9x_mmio_write(V9X_VIRGE_3D_Z_BASE, 0ul);
    v9x_mmio_write(V9X_VIRGE_3D_DEST_BASE, context->target_offset);
    v9x_mmio_write(V9X_VIRGE_3D_CLIP_L_R, context->width - 1ul);
    v9x_mmio_write(V9X_VIRGE_3D_CLIP_T_B, context->height - 1ul);
    v9x_mmio_write(V9X_VIRGE_3D_DEST_SRC_STRIDE, context->pitch << 16);
    v9x_mmio_write(V9X_VIRGE_3D_Z_STRIDE, context->width * 2ul);
    v9x_mmio_write(V9X_VIRGE_3D_TEX_BASE, 0ul);
    v9x_mmio_write(V9X_VIRGE_3D_TEX_BORDER, 0xfffffffful);
    v9x_mmio_write(V9X_VIRGE_3D_FADE_COLOR, 0ul);

    if (!v9x_wait_fifo(15ul, 1)) {
        return 0;
    }
    /* With AE set, CMD_SET establishes persistent state; the final
     * Y01_Y12 write launches the triangle. */
    v9x_mmio_write(V9X_VIRGE_3D_COMMAND, V9X_VIRGE_3D_CMD_FLAT_16_AE);
    gs_bs = (((color >> 8) & 0xfful) << 23) |
            ((color & 0xfful) << 7);
    as_rs = (255ul << 23) | (((color >> 16) & 0xfful) << 7);
    v9x_mmio_write(V9X_VIRGE_3D_DGDX_DBDX, 0ul);
    v9x_mmio_write(V9X_VIRGE_3D_DADX_DRDX, 0ul);
    v9x_mmio_write(V9X_VIRGE_3D_DGDY_DBDY, 0ul);
    v9x_mmio_write(V9X_VIRGE_3D_DADY_DRDY, 0ul);
    v9x_mmio_write(V9X_VIRGE_3D_GS_BS, gs_bs);
    v9x_mmio_write(V9X_VIRGE_3D_AS_RS, as_rs);
    v9x_mmio_write(V9X_VIRGE_3D_DXDY12,
                   (DWORD)v9x_d3d_fixed_12_20(dxdy12));
    v9x_mmio_write(V9X_VIRGE_3D_XEND12,
                   (DWORD)v9x_d3d_fixed_12_20(
                       p1->sx + dxdy12 * (p1->sy - (float)i1y)));
    v9x_mmio_write(V9X_VIRGE_3D_DXDY01,
                   (DWORD)v9x_d3d_fixed_12_20(dxdy01));
    v9x_mmio_write(V9X_VIRGE_3D_XEND01,
                   (DWORD)v9x_d3d_fixed_12_20(p0->sx + dxdy01 * fdycc));
    v9x_mmio_write(V9X_VIRGE_3D_DXDY02,
                   (DWORD)v9x_d3d_fixed_12_20(dxdy02));
    v9x_mmio_write(V9X_VIRGE_3D_XSTART02,
                   (DWORD)v9x_d3d_fixed_12_20(p0->sx + dxdy02 * fdycc));
    v9x_mmio_write(V9X_VIRGE_3D_YSTART, (DWORD)i0y);
    v9x_mmio_write(V9X_VIRGE_3D_Y01_Y12,
                   ((DWORD)dy01 << 16) |
                   (DWORD)(dy12 + (p2->sy == (float)i2y ? 1l : 0l)) |
                   (dx > 0.0f ? 0x80000000ul : 0ul));
    return 1;
}

DWORD __stdcall V9xD3dDrawOnePrimitive(
    V9X_D3DHAL_DRAWONEPRIMITIVEDATA *data)
{
    V9X_FPU_AREA fpu;
    V9X_D3D_CONTEXT *context;
    DWORD status;
    int ok = 0;

    v9x_fpu_save(&fpu);
    context = data != 0 ? v9x_d3d_context_from_handle(data->dwhContext) : 0;
    if (!v9x_engine_status_validated() && v9x_engine_ready()) {
        status = v9x_engine_status();
        if (v9x_fifo_free(status) >= 8ul &&
            (status & V9X_VIRGE_STATUS_IDLE) != 0ul) {
            v9x_hal->engine.flags |= V9X_DD_ENGINE_STATUS_VALIDATED;
        }
    }
    if (context != 0 && v9x_engine_status_validated() &&
        data->PrimitiveType == V9X_D3DPT_TRIANGLELIST &&
        data->VertexType == V9X_D3DVT_TLVERTEX &&
        data->lpvVertices != 0 && data->dwNumVertices == 3ul) {
        ok = v9x_d3d_triangle(context,
                              (const V9X_D3DTLVERTEX *)data->lpvVertices);
    }
    if (v9x_hal != 0) {
        ++v9x_hal->d3d_diagnostics.render_primitive_calls;
    }
    if (data != 0) {
        data->ddrval = ok ? V9X_DD_OK : 0x80070057ul;
    }
    v9x_fpu_restore(&fpu);
    return V9X_DDHAL_DRIVER_HANDLED;
}

DWORD __stdcall V9xD3dDrawPrimitives(V9X_D3DHAL_DRAWPRIMITIVESDATA *data)
{
    V9X_FPU_AREA fpu;
    V9X_D3D_CONTEXT *context;
    V9X_D3DHAL_DRAWPRIMCOUNTS *counts;
    BYTE *cursor;
    DWORD status;
    DWORD record;
    DWORD vertex;
    int ok = 0;

    v9x_fpu_save(&fpu);
    context = data != 0 ? v9x_d3d_context_from_handle(data->dwhContext) : 0;
    if (!v9x_engine_status_validated() && v9x_engine_ready()) {
        status = v9x_engine_status();
        if (v9x_fifo_free(status) >= 8ul &&
            (status & V9X_VIRGE_STATUS_IDLE) != 0ul) {
            v9x_hal->engine.flags |= V9X_DD_ENGINE_STATUS_VALIDATED;
        }
    }
    if (context != 0 && v9x_engine_status_validated() &&
        data->lpvData != 0) {
        cursor = (BYTE *)data->lpvData;
        ok = 1;
        for (record = 0ul; record < 64ul; ++record) {
            counts = (V9X_D3DHAL_DRAWPRIMCOUNTS *)cursor;
            cursor += sizeof(*counts);
            if (counts->wNumStateChanges > 64u) {
                ok = 0;
                break;
            }
            cursor += (DWORD)counts->wNumStateChanges * 2ul * sizeof(DWORD);
            if (counts->wNumVertices == 0u) {
                break;
            }
            cursor = (BYTE *)(((DWORD)cursor + 31ul) & ~31ul);
            if (counts->wPrimitiveType != V9X_D3DPT_TRIANGLELIST ||
                counts->wVertexType != V9X_D3DVT_TLVERTEX ||
                counts->wNumVertices > 192u ||
                (counts->wNumVertices % 3u) != 0u) {
                ok = 0;
                break;
            }
            for (vertex = 0ul; vertex < counts->wNumVertices; vertex += 3ul) {
                if (!v9x_d3d_triangle(
                        context,
                        &((const V9X_D3DTLVERTEX *)cursor)[vertex])) {
                    ok = 0;
                    break;
                }
            }
            if (!ok) {
                break;
            }
            cursor += (DWORD)counts->wNumVertices *
                      sizeof(V9X_D3DTLVERTEX);
        }
        if (record == 64ul) {
            ok = 0;
        }
    }
    if (v9x_hal != 0) {
        ++v9x_hal->d3d_diagnostics.render_primitive_calls;
    }
    if (data != 0) {
        data->ddrval = ok ? V9X_DD_OK : 0x80070057ul;
    }
    v9x_fpu_restore(&fpu);
    return V9X_DDHAL_DRIVER_HANDLED;
}

DWORD __stdcall V9xD3dDrawOneIndexedPrimitive(void *data)
{
    (void)data;
    return V9X_DDHAL_DRIVER_NOTHANDLED;
}

DWORD __stdcall V9xHalGetDriverInfo(V9X_DDHAL_GETDRIVERINFODATA *data)
{
    DWORD index;
    DWORD bytes;
    BYTE *destination;
    const BYTE *source;

    if (data == 0) {
        return V9X_DD_OK;
    }
    data->dwActualSize = 0ul;
    data->ddRVal = 0x88760028ul;
    for (index = 0ul; index < 16ul; ++index) {
        if (data->guidInfo[index] != v9x_guid_d3d_callbacks2[index]) {
            return V9X_DD_OK;
        }
    }
    bytes = data->dwExpectedSize < sizeof(v9x_d3d_callbacks2)
        ? data->dwExpectedSize : sizeof(v9x_d3d_callbacks2);
    data->dwActualSize = sizeof(v9x_d3d_callbacks2);
    if (data->lpvData != 0) {
        v9x_d3d_callbacks2.dwSize = bytes;
        destination = (BYTE *)data->lpvData;
        source = (const BYTE *)&v9x_d3d_callbacks2;
        for (index = 0ul; index < bytes; ++index) {
            destination[index] = source[index];
        }
        data->ddRVal = V9X_DD_OK;
    }
    return V9X_DD_OK;
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
    shared->info.GetDriverInfo = (V9X_DD_CODE_PTR)V9xHalGetDriverInfo;
    shared->info.lpD3DGlobalDriverData = (DWORD)&shared->d3d_global;
    shared->info.lpD3DHALCallbacks = (DWORD)&shared->d3d_callbacks;
    shared->info.lpDDExeBufCallbacks = 0;

    shared->info.vmiData.dwFlags = 0ul;
    shared->info.vmiData.dwOffscreenAlign = 8ul;
    shared->info.vmiData.dwOverlayAlign = 8ul;
    shared->info.vmiData.dwTextureAlign = 8ul;
    shared->info.vmiData.dwZBufferAlign = 8ul;
    shared->info.vmiData.dwAlphaAlign = 8ul;
    shared->info.vmiData.dwNumHeaps = 1ul;

    shared->info.ddCaps.dwSize = sizeof(V9X_DDCORECAPS);
    shared->info.ddCaps.dwCaps = V9X_DDCAPS_3D | V9X_DDCAPS_GDI |
                                 V9X_DDCAPS_BLTCOLORFILL;
    shared->info.ddCaps.ddsCaps = V9X_DDSCAPS_3DDEVICE |
                                  V9X_DDSCAPS_OFFSCREENPLAIN |
                                  V9X_DDSCAPS_FLIP |
                                  V9X_DDSCAPS_PRIMARYSURFACE;
    shared->info.ddCaps.dwVidMemTotal =
        shared->fb.vram_bytes - shared->fb.visible_bytes;
    shared->info.ddCaps.dwVidMemFree = shared->info.ddCaps.dwVidMemTotal;

    shared->dd_callbacks.dwSize = sizeof(V9X_DDHAL_DDCALLBACKS);
    shared->dd_callbacks.dwFlags = V9X_DDHAL_CB32_WAITFORVERTICALBLANK |
                                   V9X_DDHAL_CB32_SETEXCLUSIVEMODE |
                                   V9X_DDHAL_CB32_FLIPTOGDISURFACE;
    shared->dd_callbacks.WaitForVerticalBlank =
        (V9X_DD_CODE_PTR)V9xHalWaitForVerticalBlank;
    shared->dd_callbacks.SetExclusiveMode =
        (V9X_DD_CODE_PTR)V9xHalSetExclusiveMode;
    shared->dd_callbacks.FlipToGDISurface =
        (V9X_DD_CODE_PTR)V9xHalFlipToGDISurface;

    shared->surface_callbacks.dwSize = sizeof(V9X_DDHAL_DDSURFACECALLBACKS);
    shared->surface_callbacks.dwFlags =
        V9X_DDHAL_SURFCB32_FLIP | V9X_DDHAL_SURFCB32_GETFLIPSTATUS |
        V9X_DDHAL_SURFCB32_LOCK | V9X_DDHAL_SURFCB32_UNLOCK |
        V9X_DDHAL_SURFCB32_BLT | V9X_DDHAL_SURFCB32_GETBLTSTATUS;
    shared->surface_callbacks.Flip = (V9X_DD_CODE_PTR)V9xHalFlip;
    shared->surface_callbacks.GetFlipStatus =
        (V9X_DD_CODE_PTR)V9xHalGetFlipStatus;
    shared->surface_callbacks.Lock = (V9X_DD_CODE_PTR)V9xHalLock;
    shared->surface_callbacks.Unlock = (V9X_DD_CODE_PTR)V9xHalUnlock;
    shared->surface_callbacks.Blt = (V9X_DD_CODE_PTR)V9xHalBlt;
    shared->surface_callbacks.GetBltStatus =
        (V9X_DD_CODE_PTR)V9xHalGetBltStatus;

    shared->palette_callbacks.dwSize =
        sizeof(V9X_DDHAL_DDPALETTECALLBACKS);
    shared->palette_callbacks.dwFlags = 0ul;

    shared->d3d_global.dwSize = sizeof(V9X_D3DHAL_GLOBALDRIVERDATA);
    shared->d3d_global.hwCaps.dwSize = sizeof(V9X_D3DDEVICEDESC_V1);
    shared->d3d_global.hwCaps.dwFlags =
        V9X_D3DDD_COLORMODEL | V9X_D3DDD_DEVCAPS |
        V9X_D3DDD_TRICAPS |
        V9X_D3DDD_DEVICERENDERBITDEPTH;
    shared->d3d_global.hwCaps.dcmColorModel = V9X_D3DCOLOR_RGB;
    shared->d3d_global.hwCaps.dwDevCaps =
        V9X_D3DDEVCAPS_FLOATTLVERTEX |
        V9X_D3DDEVCAPS_TLVERTEXSYSTEMMEMORY |
        V9X_D3DDEVCAPS_DRAWPRIMTLVERTEX;
    shared->d3d_global.hwCaps.dtcTransformCaps.dwSize =
        sizeof(V9X_D3DTRANSFORMCAPS);
    shared->d3d_global.hwCaps.dlcLightingCaps.dwSize =
        sizeof(V9X_D3DLIGHTINGCAPS);
    shared->d3d_global.hwCaps.dpcLineCaps.dwSize =
        sizeof(V9X_D3DPRIMCAPS);
    shared->d3d_global.hwCaps.dpcTriCaps.dwSize =
        sizeof(V9X_D3DPRIMCAPS);
    shared->d3d_global.hwCaps.dpcTriCaps.dwMiscCaps =
        V9X_D3DPMISCCAPS_CULLNONE;
    shared->d3d_global.hwCaps.dpcTriCaps.dwShadeCaps =
        V9X_D3DPSHADECAPS_COLORFLATRGB;
    shared->d3d_global.hwCaps.dwDeviceRenderBitDepth = V9X_DDBD_16;
    shared->d3d_global.hwCaps.dwDeviceZBufferBitDepth = 0ul;
    shared->d3d_global.dwNumVertices = 0ul;
    shared->d3d_global.dwNumClipVertices = 0ul;
    shared->d3d_global.dwNumTextureFormats = 0ul;
    shared->d3d_global.lpTextureFormats = 0;

    shared->d3d_callbacks.dwSize = sizeof(V9X_D3DHAL_CALLBACKS);
    shared->d3d_callbacks.ContextCreate =
        (V9X_DD_CODE_PTR)V9xD3dContextCreate;
    shared->d3d_callbacks.ContextDestroy =
        (V9X_DD_CODE_PTR)V9xD3dContextDestroy;
    shared->d3d_callbacks.ContextDestroyAll =
        (V9X_DD_CODE_PTR)V9xD3dContextDestroyAll;
    shared->d3d_callbacks.RenderState =
        (V9X_DD_CODE_PTR)V9xD3dRenderState;
    shared->d3d_callbacks.RenderPrimitive =
        (V9X_DD_CODE_PTR)V9xD3dRenderPrimitive;

    v9x_d3d_callbacks2.dwSize = sizeof(V9X_D3DHAL_CALLBACKS2);
    v9x_d3d_callbacks2.dwFlags =
        V9X_D3DHAL2_CB32_SETRENDERTARGET |
        V9X_D3DHAL2_CB32_DRAWONEPRIMITIVE |
        V9X_D3DHAL2_CB32_DRAWONEINDEXEDPRIMITIVE |
        V9X_D3DHAL2_CB32_DRAWPRIMITIVES;
    v9x_d3d_callbacks2.SetRenderTarget =
        (V9X_DD_CODE_PTR)V9xD3dSetRenderTarget;
    v9x_d3d_callbacks2.DrawOnePrimitive =
        (V9X_DD_CODE_PTR)V9xD3dDrawOnePrimitive;
    v9x_d3d_callbacks2.DrawOneIndexedPrimitive =
        (V9X_DD_CODE_PTR)V9xD3dDrawOneIndexedPrimitive;
    v9x_d3d_callbacks2.DrawPrimitives =
        (V9X_DD_CODE_PTR)V9xD3dDrawPrimitives;

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
