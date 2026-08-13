/*
 * 16-bit DirectDraw ABI glue.
 *
 * This module holds no policy: it answers the DCICOMMAND escape, hands
 * DDRAW the linear address of the shared block and the V9XHAL.DLL name,
 * stamps the few 16:16 far pointers DDRAW16 requires, refreshes the
 * framebuffer descriptor from the active mode, and calls the SetInfo
 * entry captured from DDNEWCALLBACKFNS. All DirectDraw content (caps,
 * mode table, callback tables, heap policy) is built by V9XHAL.DLL.
 */
#define SetCursor V9xUserSetCursor
#include <windows.h>
#undef SetCursor

#include "velocity9x/win9x_ddraw_abi.h"

extern void v9x_serial_write(const char FAR *message);
extern LONG FAR PASCAL V9xDibControlCall(LPVOID device, WORD function,
                                         LPVOID input, LPVOID output);

#ifndef V9X_TARGET_MATROX_MILLENNIUM2

extern WORD FAR PASCAL V9xDdSharedAlloc(void);
extern DWORD FAR PASCAL V9xDdSharedLinear(void);
extern DWORD FAR PASCAL V9xLinearBase(void);
extern DWORD FAR PASCAL V9xHardwareBase(void);

typedef WORD (FAR PASCAL *V9X_SETINFO_FN)(V9X_DDHALINFO FAR *info,
                                          WORD reset);

static V9X_DD_SHARED FAR *v9x_dd_shared;
static V9X_SETINFO_FN v9x_dd_set_info;

static void v9x_dd_trace(const char FAR *stage)
{
    WritePrivateProfileString("Velocity9xDDraw", "Stage", stage,
                              "C:\\V9XDDH.INI");
}

/* 16-bit writer for the shared callback trace ring (same record layout as
 * the 32-bit HAL writers in ddhal.c). */
static void v9x_dd_trace_event(WORD id, DWORD detail)
{
    V9X_DD_TRACE FAR *trace;
    DWORD slot;

    if (v9x_dd_shared == 0) {
        return;
    }
    trace = &v9x_dd_shared->trace;
    if ((id & V9X_DD_TRACE_EXIT_FLAG) != 0u) {
        trace->last_exit_id = id & (WORD)~V9X_DD_TRACE_EXIT_FLAG;
        trace->last_exit_result = detail;
    } else {
        trace->last_enter_id = id;
        trace->last_enter_detail = detail;
        if (id < V9X_DD_TRACE_ID_COUNT) {
            ++trace->counters[id];
        }
    }
    slot = trace->head < V9X_DD_TRACE_RING_COUNT ? trace->head : 0ul;
    trace->ring[slot].id = id;
    trace->ring[slot].seq = (WORD)trace->seq;
    trace->ring[slot].detail = detail;
    trace->head = slot + 1ul < V9X_DD_TRACE_RING_COUNT ? slot + 1ul : 0ul;
    ++trace->seq;
}

/* Provided by ddi.c: the live PDEVICE far pointer and the active mode. */
extern V9X_DD_VOID_PTR v9x_dd_active_pdevice(void);
extern WORD v9x_dd_active_mode(WORD FAR *width, WORD FAR *height,
                               WORD FAR *bpp, WORD FAR *pitch);

static V9X_DD_SHARED FAR *v9x_dd_block(void)
{
    WORD selector;

    if (v9x_dd_shared != 0) {
        return v9x_dd_shared;
    }
    selector = V9xDdSharedAlloc();
    if (selector == 0u) {
        return 0;
    }
    v9x_dd_shared = (V9X_DD_SHARED FAR *)MAKELP(selector, 0u);
    {
        BYTE FAR *bytes = (BYTE FAR *)v9x_dd_shared;
        WORD index;

        for (index = 0u; index < sizeof(V9X_DD_SHARED); ++index) {
            bytes[index] = 0u;
        }
    }
    v9x_dd_shared->dwSize = sizeof(V9X_DD_SHARED);
    v9x_dd_shared->abi = V9X_DD_SHARED_ABI;
    v9x_dd_trace("shared-ready");
    return v9x_dd_shared;
}

/* The only 16-bit HAL callback: DDRAW is done with the driver object. */
static DWORD __loadds FAR PASCAL v9x_dd_destroy_driver(
    V9X_DDHAL_DESTROYDRIVERDATA FAR *data)
{
    v9x_dd_trace_event(V9X_TRACE_DD16_DESTROYDRIVER, 0ul);
    data->ddRVal = V9X_DD_OK;
    v9x_dd_set_info = 0;
    v9x_serial_write("V9X-DD destroy-driver\r\n");
    return V9X_DDHAL_DRIVER_HANDLED;
}

static void v9x_dd_refresh_framebuffer(void)
{
    V9X_DD_SHARED FAR *shared = v9x_dd_shared;
    WORD width;
    WORD height;
    WORD bpp;
    WORD pitch;

    if (shared == 0) {
        return;
    }
    if (v9x_dd_active_mode(&width, &height, &bpp, &pitch) == 0u) {
        shared->fb.flags &= ~V9X_DD_FB_VALID;
        return;
    }
    shared->fb.linear_base = V9xLinearBase();
    shared->fb.physical_base = V9xHardwareBase();
    shared->fb.vram_bytes = 0x00400000ul;
    shared->fb.pitch = pitch;
    shared->fb.width = width;
    shared->fb.height = height;
    shared->fb.bits_per_pixel = bpp;
    shared->fb.visible_bytes = (DWORD)pitch * (DWORD)height;
    shared->fb.flags |= V9X_DD_FB_VALID;

    /* V9xHardwareEnable maps the complete 64-MiB ViRGE linear aperture.
     * New-MMIO is a 64-KiB window at BAR + 16 MiB; register offsets such as
     * SUBSYS_STAT (0x8504) are relative to that window, not to VRAM. */
    shared->engine.control_linear_base =
        shared->fb.linear_base + 0x01000000ul;
    shared->engine.mapped_aperture_bytes = 0x00010000ul;
    shared->engine.flags = V9X_DD_ENGINE_VALID |
                           V9X_DD_ENGINE_S3_VIRGE_DX;
}

/*
 * Copy the mode-dependent DDHALINFO fields from the DLL-built mode table
 * and the framebuffer descriptor (mechanical field plumbing only).
 */
static void v9x_dd_refresh_info(void)
{
    V9X_DD_SHARED FAR *shared = v9x_dd_shared;
    V9X_DDHALINFO FAR *info;
    WORD index;

    if (shared == 0 || shared->driver_init_done == 0ul ||
        (shared->fb.flags & V9X_DD_FB_VALID) == 0ul) {
        return;
    }
    info = &shared->info;
    info->vmiData.fpPrimary = shared->fb.linear_base;
    info->vmiData.dwDisplayWidth = shared->fb.width;
    info->vmiData.dwDisplayHeight = shared->fb.height;
    info->vmiData.lDisplayPitch = (LONG)shared->fb.pitch;
    for (index = 0u; index < V9X_DD_MODE_COUNT; ++index) {
        if (shared->modes[index].dwWidth == shared->fb.width &&
            shared->modes[index].dwHeight == shared->fb.height &&
            shared->modes[index].dwBPP == shared->fb.bits_per_pixel) {
            info->dwModeIndex = index;
            info->vmiData.ddpfDisplay.dwSize = sizeof(V9X_DDPIXELFORMAT);
            info->vmiData.ddpfDisplay.dwFlags = V9X_DDPF_RGB;
            if ((shared->modes[index].wFlags &
                 V9X_DDMODEINFO_PALETTIZED) != 0u) {
                info->vmiData.ddpfDisplay.dwFlags |=
                    V9X_DDPF_PALETTEINDEXED8;
            }
            info->vmiData.ddpfDisplay.dwRGBBitCount =
                shared->modes[index].dwBPP;
            info->vmiData.ddpfDisplay.dwRBitMask =
                shared->modes[index].dwRBitMask;
            info->vmiData.ddpfDisplay.dwGBitMask =
                shared->modes[index].dwGBitMask;
            info->vmiData.ddpfDisplay.dwBBitMask =
                shared->modes[index].dwBBitMask;
            break;
        }
    }
    shared->heaps[0].dwFlags = V9X_VIDMEM_ISLINEAR;
    shared->heaps[0].fpStart =
        shared->fb.linear_base + shared->fb.visible_bytes;
    shared->heaps[0].fpEnd =
        shared->fb.linear_base + shared->fb.vram_bytes - 1ul;
    shared->heaps[0].ddsCaps = 0ul;
    shared->heaps[0].ddsCapsAlt = 0ul;
    shared->heaps[0].lpHeap = 0ul;
    info->vmiData.dwNumHeaps = 1ul;

    /* 16:16 far aliases DDRAW16 dereferences. */
    info->lpDDCallbacks = &shared->dd_callbacks;
    info->lpDDSurfaceCallbacks = &shared->surface_callbacks;
    info->lpDDPaletteCallbacks = &shared->palette_callbacks;
    info->vmiData.pvmList = &shared->heaps[0];
    info->lpModeInfo = &shared->modes[0];
    info->lpdwFourCC = 0;
    /* DriverInit supplies the flat DX5 extension callback. Preserve it when
     * refreshing the mode-dependent fields before SetInfo. */
    info->lpPDevice = v9x_dd_active_pdevice();
    shared->dd_callbacks.DestroyDriver =
        (V9X_DD_CODE_PTR)v9x_dd_destroy_driver;
}

WORD FAR PASCAL V9xDdCreateDriverObject(WORD reset)
{
    WORD result;

    if (v9x_dd_set_info == 0) {
        v9x_dd_trace("setinfo-callback-missing");
        return 0u;
    }
    if (v9x_dd_block() == 0) {
        return 0u;
    }
    v9x_dd_trace_event(V9X_TRACE_DD16_CREATEOBJECT, (DWORD)reset);
    v9x_dd_refresh_framebuffer();
    v9x_dd_refresh_info();
    if (v9x_dd_shared->driver_init_done == 0ul) {
        /* DriverInit has not filled the content yet; DDRAW retries via
         * the DDCREATEDRIVEROBJECT escape after loading V9XHAL.DLL. */
        v9x_dd_trace("driverinit-pending");
        return 0u;
    }
    result = v9x_dd_set_info(&v9x_dd_shared->info, reset);
    v9x_dd_trace_event((WORD)(V9X_TRACE_DD16_CREATEOBJECT |
                              V9X_DD_TRACE_EXIT_FLAG),
                       (DWORD)result);
    v9x_serial_write(result != 0u ? "V9X-DD setinfo-ok\r\n"
                                  : "V9X-DD setinfo-fail\r\n");
    v9x_dd_trace(result != 0u ? "setinfo-ok" : "setinfo-fail");
    return result;
}

void FAR PASCAL V9xDdInvalidate(void)
{
    if (v9x_dd_shared != 0) {
        v9x_dd_shared->fb.flags &= ~V9X_DD_FB_VALID;
        v9x_dd_shared->engine.flags &= ~V9X_DD_ENGINE_VALID;
    }
}

static LONG v9x_dd_command(V9X_DCICMD FAR *command, LPVOID output)
{
    switch (command->dwCommand) {
    case V9X_DDCREATEDRIVEROBJECT:
        if (V9xDdCreateDriverObject(0u) == 0u) {
            return 0;
        }
        if (output != 0) {
            *(DWORD FAR *)output = v9x_dd_shared->hInstance;
        }
        return 1;
    case V9X_DDGET32BITDRIVERNAME:
        if (v9x_dd_block() == 0 || output == 0) {
            return 0;
        }
        {
            static const char name[] = "V9XHAL.DLL";
            static const char entry[] = "DriverInit";
            V9X_DD32BITDRIVERDATA FAR *data =
                (V9X_DD32BITDRIVERDATA FAR *)output;
            WORD index;

            for (index = 0u; index < sizeof(data->szName); ++index) {
                data->szName[index] =
                    index < sizeof(name) ? name[index] : '\0';
            }
            for (index = 0u; index < sizeof(data->szEntryPoint); ++index) {
                data->szEntryPoint[index] =
                    index < sizeof(entry) ? entry[index] : '\0';
            }
            data->dwContext = V9xDdSharedLinear();
        }
        v9x_dd_trace_event(V9X_TRACE_DD16_GET32BITNAME, 0ul);
        v9x_serial_write("V9X-DD get32bitname\r\n");
        v9x_dd_trace("get32bitname");
        return 1;
    case V9X_DDNEWCALLBACKFNS:
        {
            V9X_DDHALDDRAWFNS FAR *fns =
                (V9X_DDHALDDRAWFNS FAR *)command->dwParam1;

            if (fns == 0) {
                return 0;
            }
            v9x_dd_set_info = (V9X_SETINFO_FN)fns->lpSetInfo;
        }
        v9x_dd_trace_event(V9X_TRACE_DD16_NEWCALLBACKFNS, 0ul);
        v9x_serial_write("V9X-DD newcallbackfns\r\n");
        v9x_dd_trace("newcallbackfns");
        return 1;
    case V9X_DDGETTRACE:
        /* Copy a snapshot of the trace state for the diagnostics tool.
         * Byte copy keeps the 16-bit build free of runtime helpers. */
        if (v9x_dd_block() == 0 || output == 0) {
            return 0;
        }
        {
            V9X_DD_TRACE_SNAPSHOT FAR *snapshot =
                (V9X_DD_TRACE_SNAPSHOT FAR *)output;
            const BYTE FAR *source;
            BYTE FAR *destination;
            WORD index;

            snapshot->dwSize = sizeof(V9X_DD_TRACE_SNAPSHOT);
            snapshot->abi = v9x_dd_shared->abi;
            snapshot->driver_init_done = v9x_dd_shared->driver_init_done;
            source = (const BYTE FAR *)&v9x_dd_shared->fb;
            destination = (BYTE FAR *)&snapshot->fb;
            for (index = 0u; index < sizeof(V9X_DD_FRAMEBUFFER); ++index) {
                destination[index] = source[index];
            }
            source = (const BYTE FAR *)&v9x_dd_shared->engine;
            destination = (BYTE FAR *)&snapshot->engine;
            for (index = 0u; index < sizeof(V9X_DD_ENGINE); ++index) {
                destination[index] = source[index];
            }
            source = (const BYTE FAR *)&v9x_dd_shared->d3d_diagnostics;
            destination = (BYTE FAR *)&snapshot->d3d;
            for (index = 0u; index < sizeof(V9X_D3D_DIAGNOSTICS); ++index) {
                destination[index] = source[index];
            }
            source = (const BYTE FAR *)&v9x_dd_shared->trace;
            destination = (BYTE FAR *)&snapshot->trace;
            for (index = 0u; index < sizeof(V9X_DD_TRACE); ++index) {
                destination[index] = source[index];
            }
        }
        return 1;
    case V9X_DDVERSIONINFO:
        if (output != 0) {
            V9X_DDVERSIONDATA FAR *version =
                (V9X_DDVERSIONDATA FAR *)output;

            version->dwHALVersion = V9X_DD_RUNTIME_VERSION;
            version->dwReserved1 = 0ul;
            version->dwReserved2 = 0ul;
        }
        return 1;
    default:
        return 0;
    }
}

#endif /* !V9X_TARGET_MATROX_MILLENNIUM2 */

LONG __loadds FAR PASCAL Control(LPVOID device,
                                 WORD function,
                                 LPVOID input,
                                 LPVOID output)
{
#ifndef V9X_TARGET_MATROX_MILLENNIUM2
    if (function == V9X_QUERYESCSUPPORT && input != 0) {
        if (*(WORD FAR *)input == V9X_DCICOMMAND) {
            return (LONG)V9X_DD_HAL_VERSION;
        }
    } else if (function == V9X_DCICOMMAND && input != 0) {
        V9X_DCICMD FAR *command = (V9X_DCICMD FAR *)input;

        if (command->dwVersion == V9X_DD_VERSION) {
            return v9x_dd_command(command, output);
        }
        /* Real DCI and unknown versions fall through to the DIB engine
         * (required for correct behavior of the emulated path). */
    }
#endif
    return V9xDibControlCall(device, function, input, output);
}
