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

/* C3 negotiation experiment. Stage 1 publishes GetDriverInfo but declines
 * every GUID; stage 2 changes this to 1 to serve only D3DCallbacks2. */
#define V9X_C3_SERVE_D3D_CALLBACKS2 1

/* C4 caps discriminator: 0 = control, 1 = no textures/perspective,
 * 2 = self-consistent texture advertisement. */
#define V9X_C4_CAPS_VARIANT 0

#define V9X_HAL_BASE            0xb0400000ul

#define V9X_CRTC_INDEX              0x03d4u
#define V9X_CRTC_DATA               0x03d5u
#define V9X_INPUT_STATUS_1          0x03dau
#define V9X_STATUS_VBLANK              0x08u

/* Trio32/64 enhanced 8514/A-compatible drawing engine ports. */
#define V9X_TRIO_CUR_Y                 0x82e8u
#define V9X_TRIO_CUR_X                 0x86e8u
#define V9X_TRIO_MAJ_AXIS_PCNT         0x96e8u
#define V9X_TRIO_CMD_STATUS            0x9ae8u
#define V9X_TRIO_FRGD_COLOR            0xa6e8u
#define V9X_TRIO_FRGD_MIX              0xbae8u
#define V9X_TRIO_MULTIFUNC_CNTL        0xbee8u
#define V9X_TRIO_PIXEL_CNTL_FRGD_MIX   0xa000u
#define V9X_TRIO_FRGD_MIX_NEW          0x0027u
#define V9X_TRIO_CMD_RECT_SOLID        0x40b1u
#define V9X_TRIO_STATUS_BUSY           0x0200u
#define V9X_TRIO_IDLE_SPIN_LIMIT       0x00400000ul

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
#define V9X_VIRGE_3D_TBV              0x0000b504ul
#define V9X_VIRGE_3D_TBU              0x0000b508ul
#define V9X_VIRGE_3D_DVDX             0x0000b51cul
#define V9X_VIRGE_3D_DUDX             0x0000b520ul
#define V9X_VIRGE_3D_DVDY             0x0000b528ul
#define V9X_VIRGE_3D_DUDY             0x0000b52cul
#define V9X_VIRGE_3D_DS               0x0000b530ul
#define V9X_VIRGE_3D_VS               0x0000b534ul
#define V9X_VIRGE_3D_US               0x0000b538ul
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

#define V9X_VIRGE_3D_CMD_GOURAUD_16_AE 0x83000007ul
#define V9X_VIRGE_3D_CMD_ALPHA_SOURCE   0x00040000ul
#define V9X_VIRGE_3D_CMD_ALPHA_ENABLE   0x00080000ul
#define V9X_VIRGE_3D_CMD_TEXTURE_UNLIT  0x10000000ul
#define V9X_VIRGE_3D_CMD_TEXTURE_LIT    0x08000000ul
#define V9X_VIRGE_3D_CMD_TEX_ARGB1555   0x00000040ul
#define V9X_VIRGE_3D_CMD_FILTER_NEAREST 0x00004000ul
#define V9X_VIRGE_3D_CMD_FILTER_LINEAR  0x00006000ul
#define V9X_VIRGE_3D_CMD_MIP_NEAREST    0x00000000ul
#define V9X_VIRGE_3D_CMD_MIP_LINEAR     0x00001000ul
#define V9X_VIRGE_3D_CMD_LINEAR_MIP_NEAREST 0x00002000ul
#define V9X_VIRGE_3D_CMD_LINEAR_MIP_LINEAR  0x00003000ul
#define V9X_VIRGE_3D_CMD_TEX_MODULATE   0x00008000ul
#define V9X_VIRGE_3D_CMD_TEXTURE_WRAP   0x04000000ul

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
#define V9X_D3D_TEXTURE_COUNT 256u
#define V9X_D3D_MAX_BATCH_TRIANGLES 256u

typedef struct v9x_d3d_context {
    DWORD active;
    DWORD pid;
    V9X_DD_SURFACE_LCL *target;
    V9X_DD_SURFACE_LCL *zbuffer;
    DWORD target_offset;
    DWORD pitch;
    DWORD width;
    DWORD height;
    DWORD specular_enable;
    DWORD fog_enable;
    DWORD fog_color;
    DWORD alpha_blend_enable;
    DWORD src_blend;
    DWORD dest_blend;
    DWORD texture_handle;
    DWORD texture_min;
    DWORD texture_mag;
    DWORD texture_blend;
    DWORD texture_wrap;
    DWORD texture_border;
} V9X_D3D_CONTEXT;

typedef struct v9x_d3d_texture {
    DWORD active;
    DWORD context;
    void *surface;
} V9X_D3D_TEXTURE;

static V9X_D3D_CONTEXT v9x_d3d_contexts[V9X_D3D_CONTEXT_COUNT];
static V9X_D3D_TEXTURE v9x_d3d_textures[V9X_D3D_TEXTURE_COUNT];
static V9X_D3DHAL_CALLBACKS2 v9x_d3d_callbacks2;

#if V9X_C3_SERVE_D3D_CALLBACKS2
static const BYTE v9x_guid_d3d_callbacks2[16] = {
    0xe1u, 0x84u, 0xa5u, 0x0bu, 0xb6u, 0x70u, 0xd0u, 0x11u,
    0x88u, 0x9du, 0x00u, 0xaau, 0x00u, 0xbbu, 0xb7u, 0x6au
};
#endif

/*
 * Bounded callback trace (Hellbender plan H1). Events land in the shared
 * block so the last callbacks before a fault survive the faulting process.
 * Writers are allocation-free and import-free; status polls are counted
 * but kept out of the ring so a wait loop cannot flush the history.
 */
static void v9x_trace_push(WORD id, DWORD detail)
{
    V9X_DD_TRACE *trace;
    DWORD slot;

    if (v9x_hal == 0) {
        return;
    }
    trace = &v9x_hal->trace;
    slot = trace->head < V9X_DD_TRACE_RING_COUNT ? trace->head : 0ul;
    trace->ring[slot].id = id;
    trace->ring[slot].seq = (WORD)trace->seq;
    trace->ring[slot].detail = detail;
    trace->head = slot + 1ul < V9X_DD_TRACE_RING_COUNT ? slot + 1ul : 0ul;
    ++trace->seq;
}

static void v9x_trace_count(WORD id, DWORD detail)
{
    if (v9x_hal == 0) {
        return;
    }
    v9x_hal->trace.last_enter_id = id;
    v9x_hal->trace.last_enter_detail = detail;
    if (id < V9X_DD_TRACE_ID_COUNT) {
        ++v9x_hal->trace.counters[id];
    }
}

static void v9x_trace_enter(WORD id, DWORD detail)
{
    v9x_trace_count(id, detail);
    v9x_trace_push(id, detail);
}

static void v9x_trace_exit(WORD id, DWORD result)
{
    if (v9x_hal == 0) {
        return;
    }
    v9x_hal->trace.last_exit_id = id;
    v9x_hal->trace.last_exit_result = result;
    v9x_trace_push((WORD)(id | V9X_DD_TRACE_EXIT_FLAG), result);
}

#define V9X_TRACE_PATH "C:\\V9XTRACE.INI"

static int v9x_fault_flush_active;

static const char *v9x_trace_name(WORD id)
{
    switch (id & (WORD)~V9X_DD_TRACE_EXIT_FLAG) {
    case V9X_TRACE_DRIVERINIT:           return "DriverInit";
    case V9X_TRACE_DD16_CREATEOBJECT:    return "Dd16CreateObject";
    case V9X_TRACE_DD16_DESTROYDRIVER:   return "Dd16DestroyDriver";
    case V9X_TRACE_DD16_NEWCALLBACKFNS:  return "Dd16NewCallbackFns";
    case V9X_TRACE_DD16_GET32BITNAME:    return "Dd16Get32BitName";
    case V9X_TRACE_FLIP:                 return "Flip";
    case V9X_TRACE_GETFLIPSTATUS:        return "GetFlipStatus";
    case V9X_TRACE_LOCK:                 return "Lock";
    case V9X_TRACE_UNLOCK:               return "Unlock";
    case V9X_TRACE_BLT:                  return "Blt";
    case V9X_TRACE_GETBLTSTATUS:         return "GetBltStatus";
    case V9X_TRACE_WAITFORVBLANK:        return "WaitForVerticalBlank";
    case V9X_TRACE_SETEXCLUSIVE:         return "SetExclusiveMode";
    case V9X_TRACE_FLIPTOGDI:            return "FlipToGDISurface";
    case V9X_TRACE_GETDRIVERINFO:        return "GetDriverInfo";
    case V9X_TRACE_CANCREATESURFACE:     return "CanCreateSurface";
    case V9X_TRACE_CREATESURFACE:        return "CreateSurface";
    case V9X_TRACE_DESTROYSURFACE:       return "DestroySurface";
    case V9X_TRACE_ADDATTACHEDSURFACE:   return "AddAttachedSurface";
    case V9X_TRACE_BLT_ENGINE:           return "BltEngine";
    case V9X_TRACE_D3D_CTXCREATE:        return "D3dContextCreate";
    case V9X_TRACE_D3D_CTXDESTROY:       return "D3dContextDestroy";
    case V9X_TRACE_D3D_CTXDESTROYALL:    return "D3dContextDestroyAll";
    case V9X_TRACE_D3D_RENDERSTATE:      return "D3dRenderState";
    case V9X_TRACE_D3D_RENDERPRIM:       return "D3dRenderPrimitive";
    case V9X_TRACE_D3D_SETRENDERTARGET:  return "D3dSetRenderTarget";
    case V9X_TRACE_D3D_DRAWONEPRIM:      return "D3dDrawOnePrimitive";
    case V9X_TRACE_D3D_DRAWPRIMS:        return "D3dDrawPrimitives";
    case V9X_TRACE_D3D_DRAWONEINDEXED:   return "D3dDrawOneIndexed";
    case V9X_TRACE_D3D_TARGET_LAYOUT:    return "D3dTargetLayout";
    case V9X_TRACE_D3D_EXECUTE:          return "D3dExecute";
    case V9X_TRACE_EXEBUF_CANCREATE:     return "ExeBufCanCreate";
    case V9X_TRACE_EXEBUF_CREATE:        return "ExeBufCreate";
    case V9X_TRACE_EXEBUF_DESTROY:       return "ExeBufDestroy";
    case V9X_TRACE_EXEBUF_LOCK:          return "ExeBufLock";
    case V9X_TRACE_EXEBUF_UNLOCK:        return "ExeBufUnlock";
    case V9X_TRACE_D3D_TEXTURECREATE:    return "D3dTextureCreate";
    case V9X_TRACE_D3D_TEXTUREDESTROY:   return "D3dTextureDestroy";
    case V9X_TRACE_D3D_TEXTURESWAP:      return "D3dTextureSwap";
    case V9X_TRACE_D3D_TEXTUREGETSURF:   return "D3dTextureGetSurf";
    case V9X_TRACE_D3D_PRIMREJECT:       return "D3dPrimitiveReject";
    default:                             return "Unknown";
    }
}

static char *v9x_text_append(char *at, const char *text)
{
    while (*text != '\0') {
        *at++ = *text++;
    }
    return at;
}

static char *v9x_hex_append(char *at, DWORD value)
{
    static const char digits[] = "0123456789ABCDEF";
    int shift;

    *at++ = '0';
    *at++ = 'x';
    for (shift = 28; shift >= 0; shift -= 4) {
        *at++ = digits[(value >> shift) & 0xful];
    }
    return at;
}

static void v9x_file_text(HANDLE file, const char *text)
{
    DWORD length = 0ul;
    DWORD written;

    while (text[length] != '\0') {
        ++length;
    }
    WriteFile(file, text, length, &written, 0);
}

/* This deliberately uses only fixed storage and KERNEL32 file I/O. It is
 * callable after an engine timeout and from the process exception filter,
 * where profile APIs, heap allocation, and GDI re-entry are unsafe. */
static void v9x_trace_flush_fault(DWORD code, DWORD address)
{
    HANDLE file;
    char line[112];
    char *at;
    DWORD index;

    if (v9x_hal == 0 || v9x_fault_flush_active) {
        return;
    }
    v9x_fault_flush_active = 1;
    file = CreateFileA(V9X_TRACE_PATH, GENERIC_WRITE,
                       FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, 0);
    if (file == INVALID_HANDLE_VALUE) {
        v9x_fault_flush_active = 0;
        return;
    }
    v9x_file_text(file, "[Velocity9xTrace]\r\nFaultFlush=1\r\nBuild=");
    v9x_file_text(file, v9x_hal_build_id);
    v9x_file_text(file, "\r\n");

#define V9X_WRITE_HEX_KEY(key, value) \
    do { \
        at = v9x_text_append(line, key "="); \
        at = v9x_hex_append(at, (DWORD)(value)); \
        at = v9x_text_append(at, "\r\n"); \
        *at = '\0'; \
        v9x_file_text(file, line); \
    } while (0)

    V9X_WRITE_HEX_KEY("FaultCode", code);
    V9X_WRITE_HEX_KEY("FaultAddress", address);
    V9X_WRITE_HEX_KEY("ModeWidth", v9x_hal->fb.width);
    V9X_WRITE_HEX_KEY("ModeHeight", v9x_hal->fb.height);
    V9X_WRITE_HEX_KEY("ModeBpp", v9x_hal->fb.bits_per_pixel);
    V9X_WRITE_HEX_KEY("ModePitch", v9x_hal->fb.pitch);
    V9X_WRITE_HEX_KEY("DisplayPitch", v9x_hal->info.vmiData.lDisplayPitch);
    V9X_WRITE_HEX_KEY("DisplayFormatFlags",
                      v9x_hal->info.vmiData.ddpfDisplay.dwFlags);
    V9X_WRITE_HEX_KEY("DisplayRMask",
                      v9x_hal->info.vmiData.ddpfDisplay.dwRBitMask);
    V9X_WRITE_HEX_KEY("DisplayGMask",
                      v9x_hal->info.vmiData.ddpfDisplay.dwGBitMask);
    V9X_WRITE_HEX_KEY("DisplayBMask",
                      v9x_hal->info.vmiData.ddpfDisplay.dwBBitMask);
    V9X_WRITE_HEX_KEY("TraceEvents", v9x_hal->trace.seq);
    V9X_WRITE_HEX_KEY("LastEnterId", v9x_hal->trace.last_enter_id);
    V9X_WRITE_HEX_KEY("LastEnterDetail",
                      v9x_hal->trace.last_enter_detail);
    V9X_WRITE_HEX_KEY("LastExitId", v9x_hal->trace.last_exit_id);
    V9X_WRITE_HEX_KEY("LastExitResult",
                      v9x_hal->trace.last_exit_result);
    V9X_WRITE_HEX_KEY("EngineFifoTimeouts",
                      v9x_hal->engine.fifo_timeouts);
    V9X_WRITE_HEX_KEY("EngineIdleTimeouts",
                      v9x_hal->engine.idle_timeouts);
    V9X_WRITE_HEX_KEY("EngineResets", v9x_hal->engine.reset_count);

    for (index = 0ul; index < V9X_DD_TRACE_RING_COUNT; ++index) {
        DWORD slot = v9x_hal->trace.head + index;
        const V9X_DD_TRACE_ENTRY *entry;

        if (slot >= V9X_DD_TRACE_RING_COUNT) {
            slot -= V9X_DD_TRACE_RING_COUNT;
        }
        entry = &v9x_hal->trace.ring[slot];
        if (entry->id == 0u && entry->seq == 0u && entry->detail == 0ul) {
            continue;
        }
        at = v9x_text_append(line, "Ring=");
        at = v9x_hex_append(at, entry->seq);
        *at++ = ' ';
        at = v9x_text_append(at, v9x_trace_name(entry->id));
        at = v9x_text_append(at,
            (entry->id & V9X_DD_TRACE_EXIT_FLAG) != 0u ? " exit "
                                                       : " enter ");
        at = v9x_hex_append(at, entry->detail);
        at = v9x_text_append(at, "\r\n");
        *at = '\0';
        v9x_file_text(file, line);
    }
#undef V9X_WRITE_HEX_KEY
    FlushFileBuffers(file);
    CloseHandle(file);
    v9x_fault_flush_active = 0;
}

static LONG WINAPI v9x_unhandled_exception_filter(
    struct _EXCEPTION_POINTERS *exception)
{
    DWORD code = 0ul;
    DWORD address = 0ul;

    if (exception != 0 && exception->ExceptionRecord != 0) {
        code = exception->ExceptionRecord->ExceptionCode;
        address = (DWORD)exception->ExceptionRecord->ExceptionAddress;
    }
    v9x_trace_flush_fault(code, address);
    return EXCEPTION_CONTINUE_SEARCH;
}

static unsigned char v9x_inp(unsigned short port);
#pragma aux v9x_inp = "in al,dx" parm [dx] value [al] modify exact [al];

static void v9x_outp(unsigned short port, unsigned char value);
#pragma aux v9x_outp = "out dx,al" parm [dx] [al] modify exact [];

static unsigned short v9x_inpw(unsigned short port);
#pragma aux v9x_inpw = "in ax,dx" parm [dx] value [ax] modify exact [ax];

static void v9x_outpw(unsigned short port, unsigned short value);
#pragma aux v9x_outpw = "out dx,ax" parm [dx] [ax] modify exact [];

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

static int v9x_trio_engine_ready(void)
{
    return v9x_hal != 0 &&
           (v9x_hal->fb.flags & V9X_DD_FB_VALID) != 0ul &&
           (v9x_hal->engine.flags &
            (V9X_DD_ENGINE_VALID | V9X_DD_ENGINE_S3_TRIO64)) ==
            (V9X_DD_ENGINE_VALID | V9X_DD_ENGINE_S3_TRIO64);
}

static int v9x_trio_wait_idle(int wait)
{
    DWORD spins;

    if (!v9x_trio_engine_ready()) {
        return 0;
    }
    if ((v9x_inpw(V9X_TRIO_CMD_STATUS) & V9X_TRIO_STATUS_BUSY) == 0u) {
        return 1;
    }
    if (!wait) {
        return 0;
    }
    spins = V9X_TRIO_IDLE_SPIN_LIMIT;
    while (spins-- != 0ul) {
        if ((v9x_inpw(V9X_TRIO_CMD_STATUS) & V9X_TRIO_STATUS_BUSY) == 0u) {
            return 1;
        }
    }
    ++v9x_hal->engine.idle_timeouts;
    v9x_trace_flush_fault(0x54394944ul, V9X_TRIO_CMD_STATUS);
    return 0;
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
    v9x_trace_flush_fault(0x56394646ul, V9X_VIRGE_ENGINE_STATUS);
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
    v9x_trace_flush_fault(0x56394944ul, V9X_VIRGE_ENGINE_STATUS);
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

DWORD __stdcall V9xHalCanCreateSurface(
    V9X_DDHAL_CANCREATESURFACEDATA *data)
{
    const V9X_DDSURFACEDESC *desc = data != 0
        ? (const V9X_DDSURFACEDESC *)data->lpDDSurfaceDesc : 0;
    DWORD caps = desc != 0 ? desc->ddsCaps.dwCaps : 0ul;

    v9x_trace_enter(V9X_TRACE_CANCREATESURFACE, caps);
    if (data != 0) {
        data->ddRVal = V9X_DD_OK;
    }
    v9x_trace_exit(V9X_TRACE_CANCREATESURFACE, V9X_DD_OK);
    return V9X_DDHAL_DRIVER_HANDLED;
}

DWORD __stdcall V9xHalCreateSurface(V9X_DDHAL_CREATESURFACEDATA *data)
{
    v9x_trace_enter(V9X_TRACE_CREATESURFACE,
                    data != 0 ? data->dwSCnt : 0ul);
    if (data != 0) {
        data->ddRVal = V9X_DD_OK;
    }
    v9x_trace_exit(V9X_TRACE_CREATESURFACE, V9X_DD_OK);
    return V9X_DDHAL_DRIVER_NOTHANDLED;
}

DWORD __stdcall V9xHalDestroySurface(V9X_DDHAL_DESTROYSURFACEDATA *data)
{
    v9x_trace_enter(V9X_TRACE_DESTROYSURFACE,
                    data != 0 ? data->lpDDSurface : 0ul);
    if (data != 0) {
        data->ddRVal = V9X_DD_OK;
    }
    v9x_trace_exit(V9X_TRACE_DESTROYSURFACE, V9X_DD_OK);
    return V9X_DDHAL_DRIVER_NOTHANDLED;
}

DWORD __stdcall V9xHalAddAttachedSurface(
    V9X_DDHAL_ADDATTACHEDSURFACEDATA *data)
{
    v9x_trace_enter(V9X_TRACE_ADDATTACHEDSURFACE,
                    data != 0 ? data->lpSurfAttached : 0ul);
    if (data != 0) {
        data->ddRVal = V9X_DD_OK;
    }
    v9x_trace_exit(V9X_TRACE_ADDATTACHEDSURFACE, V9X_DD_OK);
    return V9X_DDHAL_DRIVER_NOTHANDLED;
}

DWORD __stdcall V9xHalFlip(V9X_DDHAL_FLIPDATA *data)
{
    V9X_FPU_AREA fpu;
    DWORD result;

    v9x_trace_enter(V9X_TRACE_FLIP, data->dwFlags);
    v9x_fpu_save(&fpu);
    result = v9x_flip_body(data);
    v9x_fpu_restore(&fpu);
    v9x_trace_exit(V9X_TRACE_FLIP, data->ddRVal);
    return result;
}

DWORD __stdcall V9xHalGetFlipStatus(V9X_DDHAL_GETFLIPSTATUSDATA *data)
{
    v9x_trace_count(V9X_TRACE_GETFLIPSTATUS, data->dwFlags);
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
    v9x_trace_enter(V9X_TRACE_FLIPTOGDI, data->dwToGDI);
    if (data->dwToGDI != 0ul) {
        if (v9x_engine_status_validated() && !v9x_wait_idle(1)) {
            data->ddRVal = V9X_DDERR_WASSTILLDRAWING;
            v9x_trace_exit(V9X_TRACE_FLIPTOGDI, data->ddRVal);
            return V9X_DDHAL_DRIVER_HANDLED;
        }
        v9x_set_display_start(0ul);
    }
    data->ddRVal = V9X_DD_OK;
    v9x_trace_exit(V9X_TRACE_FLIPTOGDI, data->ddRVal);
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
    v9x_trace_enter(V9X_TRACE_SETEXCLUSIVE, data->dwEnterExcl);
    if (data->dwEnterExcl == 0ul) {
        if (v9x_engine_status_validated() && !v9x_wait_idle(1)) {
            data->ddRVal = V9X_DDERR_WASSTILLDRAWING;
            v9x_trace_exit(V9X_TRACE_SETEXCLUSIVE, data->ddRVal);
            return V9X_DDHAL_DRIVER_HANDLED;
        }
        v9x_set_display_start(0ul);
    }
    data->ddRVal = V9X_DD_OK;
    v9x_trace_exit(V9X_TRACE_SETEXCLUSIVE, data->ddRVal);
    return V9X_DDHAL_DRIVER_HANDLED;
}

DWORD __stdcall V9xHalLock(V9X_DDHAL_LOCKDATA *data)
{
    v9x_trace_enter(V9X_TRACE_LOCK, data->dwFlags);
    /* Serialize CPU access after asynchronous engine work. DDRAW still
     * computes and returns the actual surface pointer. */
    if (v9x_engine_status_validated() &&
        !v9x_wait_idle((data->dwFlags & V9X_DDLOCK_DONOTWAIT) == 0ul)) {
        data->ddRVal = V9X_DDERR_WASSTILLDRAWING;
        v9x_trace_exit(V9X_TRACE_LOCK, data->ddRVal);
        return V9X_DDHAL_DRIVER_HANDLED;
    }
    data->ddRVal = V9X_DD_OK;
    v9x_trace_exit(V9X_TRACE_LOCK, data->ddRVal);
    return V9X_DDHAL_DRIVER_NOTHANDLED;
}

DWORD __stdcall V9xHalUnlock(V9X_DDHAL_UNLOCKDATA *data)
{
    v9x_trace_enter(V9X_TRACE_UNLOCK, 0ul);
    data->ddRVal = V9X_DD_OK;
    v9x_trace_exit(V9X_TRACE_UNLOCK, data->ddRVal);
    return V9X_DDHAL_DRIVER_NOTHANDLED;
}

DWORD __stdcall V9xExeBufCanCreate(V9X_DDHAL_CANCREATESURFACEDATA *data)
{
    v9x_trace_enter(V9X_TRACE_EXEBUF_CANCREATE,
                    data != 0 ? data->bIsDifferentPixelFormat : 0ul);
    if (data != 0) {
        data->ddRVal = V9X_DD_OK;
    }
    v9x_trace_exit(V9X_TRACE_EXEBUF_CANCREATE, V9X_DD_OK);
    return V9X_DDHAL_DRIVER_NOTHANDLED;
}

DWORD __stdcall V9xExeBufCreate(V9X_DDHAL_CREATESURFACEDATA *data)
{
    v9x_trace_enter(V9X_TRACE_EXEBUF_CREATE,
                    data != 0 ? data->dwSCnt : 0ul);
    if (data != 0) {
        data->ddRVal = V9X_DD_OK;
    }
    v9x_trace_exit(V9X_TRACE_EXEBUF_CREATE, V9X_DD_OK);
    return V9X_DDHAL_DRIVER_NOTHANDLED;
}

DWORD __stdcall V9xExeBufDestroy(V9X_DDHAL_DESTROYSURFACEDATA *data)
{
    v9x_trace_enter(V9X_TRACE_EXEBUF_DESTROY, 0ul);
    if (data != 0) {
        data->ddRVal = V9X_DD_OK;
    }
    v9x_trace_exit(V9X_TRACE_EXEBUF_DESTROY, V9X_DD_OK);
    return V9X_DDHAL_DRIVER_NOTHANDLED;
}

DWORD __stdcall V9xExeBufLock(V9X_DDHAL_LOCKDATA *data)
{
    v9x_trace_enter(V9X_TRACE_EXEBUF_LOCK,
                    data != 0 ? data->dwFlags : 0ul);
    if (data != 0) {
        data->ddRVal = V9X_DD_OK;
    }
    v9x_trace_exit(V9X_TRACE_EXEBUF_LOCK, V9X_DD_OK);
    return V9X_DDHAL_DRIVER_NOTHANDLED;
}

DWORD __stdcall V9xExeBufUnlock(V9X_DDHAL_UNLOCKDATA *data)
{
    v9x_trace_enter(V9X_TRACE_EXEBUF_UNLOCK, 0ul);
    if (data != 0) {
        data->ddRVal = V9X_DD_OK;
    }
    v9x_trace_exit(V9X_TRACE_EXEBUF_UNLOCK, V9X_DD_OK);
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

/*
 * Bounded video-memory source copy.
 *
 * A driver that sets DDCAPS_BLT owns every blit DirectDraw can express with
 * the ROPs it advertises, and the Win9x runtime will not accept DDCAPS_BLT
 * without ROP3 SRCCOPY (measured; see dd16.c). Declining a source copy after
 * claiming it does not fall back to the HEL - the runtime returns
 * DDERR_UNSUPPORTED to the application - so the claim has to be honoured.
 *
 * This is a CPU copy through the mapped linear aperture, which is what the
 * HEL would have done, so it costs nothing relative to the previous
 * behaviour while keeping the engine-accelerated colour fill reachable.
 * Replacing it with the Trio64 screen-to-screen BitBLT is the next bounded
 * 2D primitive; the surface validation here already matches that engine's
 * display-pitch constraint.
 */
static int v9x_copy_rect_valid(const V9X_DD_SURFACE_LCL *surface,
                               const LONG *rect, DWORD bytes_per_pixel,
                               DWORD *offset_out)
{
    const V9X_DD_SURFACE_GBL *global;
    DWORD offset;
    DWORD right_bytes;
    DWORD last_row;

    if (surface == 0 || surface->lpGbl == 0 ||
        rect[0] < 0l || rect[1] < 0l ||
        rect[2] <= rect[0] || rect[3] <= rect[1]) {
        return 0;
    }
    global = surface->lpGbl;
    if (rect[2] > (LONG)global->wWidth || rect[3] > (LONG)global->wHeight ||
        global->lPitch <= 0l) {
        return 0;
    }
    offset = v9x_surface_offset(surface);
    if (offset == 0xfffffffful) {
        return 0;
    }
    right_bytes = (DWORD)rect[2] * bytes_per_pixel;
    /* The row must fit the surface's own pitch. This also rejects a surface
     * whose pixel format differs from the display, whose pitch would be too
     * small for the display's bytes-per-pixel - the copy assumes both
     * surfaces carry the display format. */
    if (right_bytes > (DWORD)global->lPitch) {
        return 0;
    }
    last_row = (DWORD)(rect[3] - 1l) * (DWORD)global->lPitch;
    if (right_bytes > v9x_hal->fb.vram_bytes ||
        last_row > v9x_hal->fb.vram_bytes - right_bytes ||
        offset > v9x_hal->fb.vram_bytes - right_bytes - last_row) {
        return 0;
    }
    *offset_out = offset;
    return 1;
}

static void v9x_copy_row(BYTE *destination, const BYTE *source, DWORD bytes)
{
    if (destination == source || bytes == 0ul) {
        return;
    }
    if (destination < source) {
        while (bytes-- != 0ul) {
            *destination++ = *source++;
        }
    } else {
        destination += bytes;
        source += bytes;
        while (bytes-- != 0ul) {
            *--destination = *--source;
        }
    }
}

static DWORD v9x_srccopy_body(V9X_DDHAL_BLTDATA *data)
{
    const DWORD allowed = V9X_DDBLT_ROP | V9X_DDBLT_WAIT |
                          V9X_DDBLT_DONOTWAIT | V9X_DDBLT_ASYNC;
    DWORD bytes_per_pixel;
    DWORD source_offset;
    DWORD destination_offset;
    DWORD source_pitch;
    DWORD destination_pitch;
    DWORD row_bytes;
    DWORD height;
    DWORD row;
    BYTE *base;

    data->ddRVal = V9X_DD_OK;
    if ((data->dwFlags & ~allowed) != 0ul ||
        ((data->dwFlags & V9X_DDBLT_ROP) != 0ul &&
         data->bltFX.dwROP != V9X_DDROP_SRCCOPY) ||
        v9x_hal == 0 || (v9x_hal->fb.flags & V9X_DD_FB_VALID) == 0ul ||
        (v9x_hal->fb.bits_per_pixel != 8ul &&
         v9x_hal->fb.bits_per_pixel != 16ul)) {
        return V9X_DDHAL_DRIVER_NOTHANDLED;
    }
    /* No stretching, mirroring, colour keying or format conversion. */
    if (data->rSrc[2] - data->rSrc[0] != data->rDest[2] - data->rDest[0] ||
        data->rSrc[3] - data->rSrc[1] != data->rDest[3] - data->rDest[1]) {
        return V9X_DDHAL_DRIVER_NOTHANDLED;
    }
    bytes_per_pixel = v9x_hal->fb.bits_per_pixel >> 3;
    if (!v9x_copy_rect_valid(data->lpDDSrcSurface, data->rSrc,
                             bytes_per_pixel, &source_offset) ||
        !v9x_copy_rect_valid(data->lpDDDestSurface, data->rDest,
                             bytes_per_pixel, &destination_offset)) {
        return V9X_DDHAL_DRIVER_NOTHANDLED;
    }
    /* Drain whichever engine owns this chipset before touching the same
     * memory from the CPU. With no engine enabled there is nothing in
     * flight, so the copy can start immediately. */
    {
        int wait = (data->dwFlags &
                    (V9X_DDBLT_ASYNC | V9X_DDBLT_DONOTWAIT)) == 0ul;
        int idle = v9x_trio_engine_ready() ? v9x_trio_wait_idle(wait)
                 : v9x_engine_status_validated() ? v9x_wait_idle(wait)
                 : 1;

        if (!idle) {
            data->ddRVal = V9X_DDERR_WASSTILLDRAWING;
            return V9X_DDHAL_DRIVER_HANDLED;
        }
    }

    source_pitch = (DWORD)data->lpDDSrcSurface->lpGbl->lPitch;
    destination_pitch = (DWORD)data->lpDDDestSurface->lpGbl->lPitch;
    row_bytes = (DWORD)(data->rSrc[2] - data->rSrc[0]) * bytes_per_pixel;
    height = (DWORD)(data->rSrc[3] - data->rSrc[1]);
    base = (BYTE *)v9x_hal->fb.linear_base;
    source_offset += (DWORD)data->rSrc[1] * source_pitch +
                     (DWORD)data->rSrc[0] * bytes_per_pixel;
    destination_offset += (DWORD)data->rDest[1] * destination_pitch +
                          (DWORD)data->rDest[0] * bytes_per_pixel;

    /* Source and destination can be the same surface (window scrolling), so
     * pick the row order that keeps an overlapping copy correct. */
    if (destination_offset > source_offset) {
        for (row = height; row-- != 0ul;) {
            v9x_copy_row(base + destination_offset + row * destination_pitch,
                         base + source_offset + row * source_pitch,
                         row_bytes);
        }
    } else {
        for (row = 0ul; row < height; ++row) {
            v9x_copy_row(base + destination_offset + row * destination_pitch,
                         base + source_offset + row * source_pitch,
                         row_bytes);
        }
    }
    return V9X_DDHAL_DRIVER_HANDLED;
}

static DWORD v9x_blt_body(V9X_DDHAL_BLTDATA *data)
{
    DWORD allowed = V9X_DDBLT_COLORFILL | V9X_DDBLT_WAIT |
                    V9X_DDBLT_DONOTWAIT | V9X_DDBLT_ASYNC;
    DWORD bytes_per_pixel;
    DWORD offset;
    DWORD width;
    DWORD height;
    DWORD command;
    int wait;

    if (data != 0 && data->lpDDSrcSurface != 0) {
        return v9x_srccopy_body(data);
    }
    if (v9x_trio_engine_ready()) {
        DWORD trio_allowed = V9X_DDBLT_COLORFILL | V9X_DDBLT_WAIT |
                             V9X_DDBLT_DONOTWAIT | V9X_DDBLT_ASYNC;
        DWORD trio_offset;
        DWORD trio_pitch;
        DWORD trio_y;

        if (data == 0 ||
            (data->dwFlags & V9X_DDBLT_COLORFILL) == 0ul ||
            (data->dwFlags & ~trio_allowed) != 0ul ||
            data->lpDDSrcSurface != 0 || data->lpDDDestSurface == 0 ||
            data->lpDDDestSurface->lpGbl == 0 ||
            (v9x_hal->fb.bits_per_pixel != 8ul &&
             v9x_hal->fb.bits_per_pixel != 16ul)) {
            if (data != 0) {
                data->ddRVal = V9X_DD_OK;
            }
            return V9X_DDHAL_DRIVER_NOTHANDLED;
        }
        bytes_per_pixel = v9x_hal->fb.bits_per_pixel >> 3;
        if (!v9x_fill_rect_valid(data, bytes_per_pixel, &trio_offset)) {
            data->ddRVal = V9X_DD_OK;
            return V9X_DDHAL_DRIVER_NOTHANDLED;
        }
        trio_pitch = v9x_hal->fb.pitch;
        if ((DWORD)data->lpDDDestSurface->lpGbl->lPitch != trio_pitch ||
            trio_pitch == 0ul || (trio_offset % trio_pitch) != 0ul) {
            data->ddRVal = V9X_DD_OK;
            return V9X_DDHAL_DRIVER_NOTHANDLED;
        }
        trio_y = trio_offset / trio_pitch + (DWORD)data->rDest[1];
        width = (DWORD)(data->rDest[2] - data->rDest[0]);
        height = (DWORD)(data->rDest[3] - data->rDest[1]);
        if (trio_y >= 2048ul || height > 2048ul - trio_y) {
            data->ddRVal = V9X_DD_OK;
            return V9X_DDHAL_DRIVER_NOTHANDLED;
        }
        wait = (data->dwFlags &
                (V9X_DDBLT_ASYNC | V9X_DDBLT_DONOTWAIT)) == 0ul;
        if (!v9x_trio_wait_idle(wait)) {
            data->ddRVal = V9X_DDERR_WASSTILLDRAWING;
            return V9X_DDHAL_DRIVER_HANDLED;
        }

        /* S3 Trio32/64 databook section 13.3.3: solid rectangle fill. */
        v9x_outpw(V9X_TRIO_FRGD_MIX, V9X_TRIO_FRGD_MIX_NEW);
        v9x_outpw(V9X_TRIO_FRGD_COLOR,
                  (unsigned short)data->bltFX.dwFillColor);
        v9x_outpw(V9X_TRIO_MULTIFUNC_CNTL,
                  V9X_TRIO_PIXEL_CNTL_FRGD_MIX);
        v9x_outpw(V9X_TRIO_CUR_X, (unsigned short)data->rDest[0]);
        v9x_outpw(V9X_TRIO_CUR_Y, (unsigned short)trio_y);
        v9x_outpw(V9X_TRIO_MAJ_AXIS_PCNT, (unsigned short)(width - 1ul));
        v9x_outpw(V9X_TRIO_MULTIFUNC_CNTL,
                  (unsigned short)(height - 1ul));
        v9x_outpw(V9X_TRIO_CMD_STATUS, V9X_TRIO_CMD_RECT_SOLID);
        data->ddRVal = V9X_DD_OK;
        return V9X_DDHAL_DRIVER_HANDLED;
    }

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

DWORD __stdcall V9xHalBlt(V9X_DDHAL_BLTDATA *data)
{
    DWORD result;

    v9x_trace_enter(V9X_TRACE_BLT, data != 0 ? data->dwFlags : 0ul);
    result = v9x_blt_body(data);
    /* Record the driver return, not ddRVal: DDHAL_DRIVER_HANDLED is the only
     * evidence that the engine - rather than the HEL - executed the blit,
     * and ddRVal is DD_OK in both cases. The separate counter survives a
     * ring wrap; GetBltStatus polling floods the ring after every blit. */
    if (result == V9X_DDHAL_DRIVER_HANDLED) {
        v9x_trace_count(V9X_TRACE_BLT_ENGINE,
                        data != 0 ? data->bltFX.dwFillColor : 0ul);
    }
    v9x_trace_exit(V9X_TRACE_BLT, result);
    return result;
}

DWORD __stdcall V9xHalGetBltStatus(V9X_DDHAL_GETBLTSTATUSDATA *data)
{
    int ready;
    DWORD status;

    v9x_trace_count(V9X_TRACE_GETBLTSTATUS, data->dwFlags);
    if (v9x_trio_engine_ready()) {
        if (data->dwFlags != V9X_DDGBS_CANBLT &&
            data->dwFlags != V9X_DDGBS_ISBLTDONE) {
            data->ddRVal = V9X_DD_OK;
            return V9X_DDHAL_DRIVER_NOTHANDLED;
        }
        ready = v9x_trio_wait_idle(0);
        data->ddRVal = ready ? V9X_DD_OK : V9X_DDERR_WASSTILLDRAWING;
        return V9X_DDHAL_DRIVER_HANDLED;
    }
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

    v9x_trace_count(V9X_TRACE_WAITFORVBLANK, data->dwFlags);
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

static V9X_D3D_TEXTURE *v9x_d3d_texture_from_handle(DWORD handle,
                                                     DWORD context)
{
    DWORD index;

    for (index = 0ul; index < V9X_D3D_TEXTURE_COUNT; ++index) {
        if ((DWORD)&v9x_d3d_textures[index] == handle &&
            v9x_d3d_textures[index].active != 0ul &&
            v9x_d3d_textures[index].context == context) {
            return &v9x_d3d_textures[index];
        }
    }
    return 0;
}

static V9X_DD_SURFACE_LCL *v9x_d3d_surface_lcl(void *surface);

static void v9x_d3d_textures_destroy_context(DWORD context)
{
    DWORD index;

    for (index = 0ul; index < V9X_D3D_TEXTURE_COUNT; ++index) {
        if (v9x_d3d_textures[index].active != 0ul &&
            v9x_d3d_textures[index].context == context) {
            v9x_d3d_textures[index].active = 0ul;
            v9x_d3d_textures[index].context = 0ul;
            v9x_d3d_textures[index].surface = 0;
        }
    }
}

static int v9x_d3d_texture_info(V9X_D3D_CONTEXT *context,
                                DWORD *offset_out, DWORD *size_log_out,
                                int *mipmapped_out)
{
    V9X_D3D_TEXTURE *texture;
    V9X_DD_SURFACE_LCL *surface;
    DWORD size;
    DWORD size_log = 0ul;
    DWORD offset;
    DWORD last_byte;

    if (context->texture_handle == 0ul) {
        return 0;
    }
    texture = v9x_d3d_texture_from_handle(context->texture_handle,
                                           (DWORD)context);
    surface = texture != 0 ? v9x_d3d_surface_lcl(texture->surface) : 0;
    if (surface == 0 || surface->lpGbl == 0) {
        return 0;
    }
    if ((surface->ddsCaps & V9X_DDSCAPS_TEXTURE) == 0ul ||
        (surface->ddsCaps & V9X_DDSCAPS_SYSTEMMEMORY) != 0ul) {
        return 0;
    }
    if (surface->lpGbl->wWidth != surface->lpGbl->wHeight ||
        surface->lpGbl->wWidth < 4u || surface->lpGbl->wWidth > 512u) {
        return 0;
    }
    if (surface->lpGbl->lPitch != (LONG)surface->lpGbl->wWidth * 2l) {
        return 0;
    }
    size = surface->lpGbl->wWidth;
    while ((1ul << size_log) < size && size_log < 9ul) {
        ++size_log;
    }
    if ((1ul << size_log) != size) {
        return 0;
    }
    offset = v9x_surface_offset(surface);
    last_byte = size * size * 2ul;
    if ((surface->ddsCaps & V9X_DDSCAPS_MIPMAP) != 0ul) {
        last_byte += last_byte / 3ul;
    }
    if (offset == 0xfffffffful || last_byte > v9x_hal->fb.vram_bytes ||
        offset > v9x_hal->fb.vram_bytes - last_byte) {
        return 0;
    }
    *offset_out = offset;
    *size_log_out = size_log;
    *mipmapped_out = (surface->ddsCaps & V9X_DDSCAPS_MIPMAP) != 0ul;
    return 1;
}

static int v9x_d3d_triangle(V9X_D3D_CONTEXT *context,
                            const V9X_D3DTLVERTEX *first);
DWORD __stdcall V9xD3dRenderPrimitive(
    V9X_D3DHAL_RENDERPRIMITIVEDATA *data);

static BYTE v9x_d3d_lerp_byte(BYTE first, BYTE second, float amount)
{
    return (BYTE)v9x_float_to_long((float)first +
        ((float)second - (float)first) * amount);
}

static DWORD v9x_d3d_lerp_color(DWORD first, DWORD second, float amount)
{
    return ((DWORD)v9x_d3d_lerp_byte((BYTE)(first >> 24),
                                     (BYTE)(second >> 24), amount) << 24) |
           ((DWORD)v9x_d3d_lerp_byte((BYTE)(first >> 16),
                                     (BYTE)(second >> 16), amount) << 16) |
           ((DWORD)v9x_d3d_lerp_byte((BYTE)(first >> 8),
                                     (BYTE)(second >> 8), amount) << 8) |
           (DWORD)v9x_d3d_lerp_byte((BYTE)first, (BYTE)second, amount);
}

static void v9x_d3d_lerp_vertex(V9X_D3DTLVERTEX *result,
                                const V9X_D3DTLVERTEX *first,
                                const V9X_D3DTLVERTEX *second,
                                float amount)
{
    result->sx = first->sx + (second->sx - first->sx) * amount;
    result->sy = first->sy + (second->sy - first->sy) * amount;
    result->sz = first->sz + (second->sz - first->sz) * amount;
    result->rhw = first->rhw + (second->rhw - first->rhw) * amount;
    result->color = v9x_d3d_lerp_color(first->color, second->color, amount);
    result->specular = v9x_d3d_lerp_color(first->specular,
                                          second->specular, amount);
    result->tu = first->tu + (second->tu - first->tu) * amount;
    result->tv = first->tv + (second->tv - first->tv) * amount;
}

static int v9x_d3d_clip_triangle(const V9X_D3D_CONTEXT *context,
                                 const V9X_D3DTLVERTEX *triangle,
                                 V9X_D3DTLVERTEX *result)
{
    V9X_D3DTLVERTEX buffers[2][8];
    V9X_D3DTLVERTEX *input = buffers[0];
    V9X_D3DTLVERTEX *output = buffers[1];
    DWORD count = 3ul;
    DWORD edge;
    DWORD index;

    for (index = 0ul; index < 3ul; ++index) {
        if (!(triangle[index].sx >= -2048.0f &&
              triangle[index].sx < 2048.0f &&
              triangle[index].sy >= -2048.0f &&
              triangle[index].sy < 2048.0f)) {
            return -1;
        }
        input[index] = triangle[index];
    }
    for (edge = 0ul; edge < 4ul && count != 0ul; ++edge) {
        V9X_D3DTLVERTEX previous = input[count - 1ul];
        int previous_inside;
        DWORD output_count = 0ul;
        float boundary = (edge == 0ul || edge == 2ul) ? 0.0f :
            (edge == 1ul ? (float)(context->width - 1ul) :
                           (float)(context->height - 1ul));

        if (edge < 2ul) {
            previous_inside = edge == 0ul ? previous.sx >= boundary
                                          : previous.sx <= boundary;
        } else {
            previous_inside = edge == 2ul ? previous.sy >= boundary
                                          : previous.sy <= boundary;
        }
        for (index = 0ul; index < count; ++index) {
            V9X_D3DTLVERTEX current = input[index];
            int current_inside;

            if (edge < 2ul) {
                current_inside = edge == 0ul ? current.sx >= boundary
                                             : current.sx <= boundary;
            } else {
                current_inside = edge == 2ul ? current.sy >= boundary
                                             : current.sy <= boundary;
            }
            if (current_inside != previous_inside) {
                float denominator = edge < 2ul
                    ? current.sx - previous.sx : current.sy - previous.sy;
                float numerator = edge < 2ul
                    ? boundary - previous.sx : boundary - previous.sy;

                if (denominator != 0.0f && output_count < 8ul) {
                    v9x_d3d_lerp_vertex(&output[output_count], &previous,
                                        &current, numerator / denominator);
                    if (edge < 2ul) {
                        output[output_count].sx = boundary;
                    } else {
                        output[output_count].sy = boundary;
                    }
                    ++output_count;
                }
            }
            if (current_inside && output_count < 8ul) {
                output[output_count++] = current;
            }
            previous = current;
            previous_inside = current_inside;
        }
        count = output_count;
        {
            V9X_D3DTLVERTEX *swap = input;
            input = output;
            output = swap;
        }
    }
    for (index = 0ul; index < count; ++index) {
        result[index] = input[index];
    }
    return (int)count;
}

static V9X_DD_SURFACE_LCL *v9x_d3d_surface_lcl(void *surface)
{
    V9X_DD_SURFACE_INT *wrapper = (V9X_DD_SURFACE_INT *)surface;

    return wrapper != 0 ? wrapper->lpLcl : 0;
}

static int v9x_d3d_set_target(V9X_D3D_CONTEXT *context, void *surface,
                              void *zbuffer)
{
    V9X_DD_SURFACE_LCL *target = v9x_d3d_surface_lcl(surface);
    V9X_DD_SURFACE_LCL *depth = zbuffer != 0
        ? v9x_d3d_surface_lcl(zbuffer) : 0;
    V9X_DD_SURFACE_GBL *global;
    DWORD offset;
    DWORD last_byte;
    DWORD pitch;
    DWORD width;
    DWORD height;
    int primary;
    int display_layout;

    if (context == 0 || target == 0 || target->lpGbl == 0 ||
        (target->ddsCaps & V9X_DDSCAPS_SYSTEMMEMORY) != 0ul) {
        return 0;
    }
    global = target->lpGbl;
    offset = v9x_surface_offset(target);
    primary = (target->ddsCaps & V9X_DDSCAPS_PRIMARYSURFACE) != 0ul;
    display_layout = (target->ddsCaps &
        (V9X_DDSCAPS_PRIMARYSURFACE | V9X_DDSCAPS_BACKBUFFER)) != 0ul;
    pitch = (DWORD)global->lPitch;
    width = global->wWidth;
    height = global->wHeight;
    /* Low byte: 0x80 marks raw DDRAW metadata; bits 1:0 identify
     * offscreen/primary/backbuffer. The following event records the pitch
     * actually selected after display-layout normalization. */
    v9x_trace_push(V9X_TRACE_D3D_TARGET_LAYOUT,
                   ((pitch & 0xfffful) << 16) |
                   ((v9x_hal->fb.bits_per_pixel & 0xfful) << 8) | 0x80ul |
                   (primary ? 1ul : (display_layout ? 2ul : 0ul)));
    if (display_layout) {
        V9X_DDPIXELFORMAT *format = &v9x_hal->info.vmiData.ddpfDisplay;

        /* DDRAW's primary/flip-chain metadata has varied across the legacy
         * runtime paths. The scanout descriptor is authoritative for these
         * display-sized surfaces: using a stale surface pitch here creates
         * diagonal/striped S3D output and can walk beyond the page. */
        if ((primary && offset != 0ul) ||
            v9x_hal->fb.bits_per_pixel != 16ul ||
            v9x_hal->info.vmiData.lDisplayPitch !=
                (LONG)v9x_hal->fb.pitch ||
            format->dwSize != sizeof(V9X_DDPIXELFORMAT) ||
            (format->dwFlags & V9X_DDPF_RGB) == 0ul ||
            format->dwRGBBitCount != 16ul ||
            format->dwRBitMask != 0x0000f800ul ||
            format->dwGBitMask != 0x000007e0ul ||
            format->dwBBitMask != 0x0000001ful) {
            return 0;
        }
        pitch = v9x_hal->fb.pitch;
        width = v9x_hal->fb.width;
        height = v9x_hal->fb.height;
    }
    v9x_trace_push(V9X_TRACE_D3D_TARGET_LAYOUT,
                   ((pitch & 0xfffful) << 16) |
                   ((v9x_hal->fb.bits_per_pixel & 0xfful) << 8) |
                   (primary ? 1ul : (display_layout ? 2ul : 0ul)));
    if (offset == 0xfffffffful || (!display_layout && global->lPitch <= 0l) ||
        (pitch & 7ul) != 0ul || pitch > 0x00000ff8ul ||
        width == 0ul || width > pitch / 2ul || height == 0ul ||
        width > 2048ul || height > 2048ul) {
        return 0;
    }
    last_byte = (height - 1ul) * pitch + width * 2ul;
    if (last_byte > v9x_hal->fb.vram_bytes ||
        offset > v9x_hal->fb.vram_bytes - last_byte) {
        return 0;
    }
    if (depth != 0) {
        DWORD depth_offset;
        DWORD depth_pitch;
        DWORD depth_last_byte;

        if (depth->lpGbl == 0 ||
            (depth->ddsCaps & V9X_DDSCAPS_ZBUFFER) == 0ul ||
            (depth->ddsCaps & V9X_DDSCAPS_SYSTEMMEMORY) != 0ul ||
            depth->lpGbl->lPitch <= 0l || depth->lpGbl->wWidth < width ||
            depth->lpGbl->wHeight < height) {
            return 0;
        }
        depth_offset = v9x_surface_offset(depth);
        depth_pitch = (DWORD)depth->lpGbl->lPitch;
        depth_last_byte = (height - 1ul) * depth_pitch + width * 2ul;
        if ((depth_pitch & 7ul) != 0ul || depth_pitch > 0x00000ff8ul ||
            depth_offset == 0xfffffffful ||
            depth_last_byte > v9x_hal->fb.vram_bytes ||
            depth_offset > v9x_hal->fb.vram_bytes - depth_last_byte) {
            return 0;
        }
    }
    context->target = target;
    context->zbuffer = depth;
    context->target_offset = offset;
    context->pitch = pitch;
    context->width = width;
    context->height = height;
    return 1;
}

DWORD __stdcall V9xD3dContextCreate(V9X_D3DHAL_CONTEXTCREATEDATA *data)
{
    DWORD index;
    V9X_D3D_CONTEXT *context;

    v9x_trace_enter(V9X_TRACE_D3D_CTXCREATE,
                    data != 0 ? data->dwPID : 0ul);
    if (data == 0 || v9x_hal == 0 || data->lpDDS == 0 ||
        (v9x_hal->fb.flags & V9X_DD_FB_VALID) == 0ul ||
        v9x_hal->fb.bits_per_pixel != 16ul) {
        if (data != 0) {
            data->ddrval = 0x80070057ul;
        }
        if (v9x_hal != 0) {
            ++v9x_hal->d3d_diagnostics.context_rejects;
        }
        v9x_trace_exit(V9X_TRACE_D3D_CTXCREATE, 0x80070057ul);
        return V9X_DDHAL_DRIVER_HANDLED;
    }
    for (index = 0ul; index < V9X_D3D_CONTEXT_COUNT; ++index) {
        context = &v9x_d3d_contexts[index];
        if (context->active == 0ul) {
            if (!v9x_d3d_set_target(context, data->lpDDS, data->lpDDSZ)) {
                data->ddrval = 0x80070057ul;
                ++v9x_hal->d3d_diagnostics.context_rejects;
                v9x_trace_exit(V9X_TRACE_D3D_CTXCREATE, data->ddrval);
                return V9X_DDHAL_DRIVER_HANDLED;
            }
            context->pid = data->dwPID;
            context->specular_enable = 0ul;
            context->fog_enable = 0ul;
            context->fog_color = 0ul;
            context->alpha_blend_enable = 0ul;
            context->src_blend = V9X_D3DBLEND_SRCALPHA;
            context->dest_blend = V9X_D3DBLEND_INVSRCALPHA;
            context->texture_handle = 0ul;
            context->texture_min = V9X_D3DFILTER_NEAREST;
            context->texture_mag = V9X_D3DFILTER_NEAREST;
            context->texture_blend = V9X_D3DTBLEND_MODULATE;
            context->texture_wrap = 1ul;
            context->texture_border = 0ul;
            context->active = 1ul;
            data->dwhContext = (DWORD)context;
            data->ddrval = V9X_DD_OK;
            ++v9x_hal->d3d_diagnostics.context_creates;
            v9x_trace_exit(V9X_TRACE_D3D_CTXCREATE, data->ddrval);
            return V9X_DDHAL_DRIVER_HANDLED;
        }
    }
    data->ddrval = 0x8007000eul;
    ++v9x_hal->d3d_diagnostics.context_rejects;
    v9x_trace_exit(V9X_TRACE_D3D_CTXCREATE, data->ddrval);
    return V9X_DDHAL_DRIVER_HANDLED;
}

DWORD __stdcall V9xD3dContextDestroy(V9X_D3DHAL_CONTEXTDESTROYDATA *data)
{
    V9X_D3D_CONTEXT *context;

    context = data != 0 ? v9x_d3d_context_from_handle(data->dwhContext) : 0;
    v9x_trace_enter(V9X_TRACE_D3D_CTXDESTROY,
                    data != 0 ? data->dwhContext : 0ul);
    if (context == 0) {
        if (data != 0) {
            data->ddrval = 0x80070057ul;
        }
        if (v9x_hal != 0) {
            ++v9x_hal->d3d_diagnostics.context_rejects;
        }
        v9x_trace_exit(V9X_TRACE_D3D_CTXDESTROY, 0x80070057ul);
        return V9X_DDHAL_DRIVER_HANDLED;
    }
    v9x_d3d_textures_destroy_context(data->dwhContext);
    context->active = 0ul;
    context->pid = 0ul;
    context->target = 0;
    context->zbuffer = 0;
    context->target_offset = 0ul;
    context->pitch = 0ul;
    context->width = 0ul;
    context->height = 0ul;
    context->specular_enable = 0ul;
    context->fog_enable = 0ul;
    context->fog_color = 0ul;
    context->alpha_blend_enable = 0ul;
    context->src_blend = 0ul;
    context->dest_blend = 0ul;
    context->texture_handle = 0ul;
    context->texture_min = 0ul;
    context->texture_mag = 0ul;
    context->texture_blend = 0ul;
    context->texture_wrap = 0ul;
    context->texture_border = 0ul;
    data->ddrval = V9X_DD_OK;
    ++v9x_hal->d3d_diagnostics.context_destroys;
    v9x_trace_exit(V9X_TRACE_D3D_CTXDESTROY, data->ddrval);
    return V9X_DDHAL_DRIVER_HANDLED;
}

DWORD __stdcall V9xD3dContextDestroyAll(
    V9X_D3DHAL_CONTEXTDESTROYALLDATA *data)
{
    DWORD index;

    v9x_trace_enter(V9X_TRACE_D3D_CTXDESTROYALL,
                    data != 0 ? data->dwPID : 0ul);
    if (data == 0) {
        return V9X_DDHAL_DRIVER_HANDLED;
    }
    for (index = 0ul; index < V9X_D3D_CONTEXT_COUNT; ++index) {
        if (v9x_d3d_contexts[index].active != 0ul &&
            v9x_d3d_contexts[index].pid == data->dwPID) {
            v9x_d3d_textures_destroy_context(
                (DWORD)&v9x_d3d_contexts[index]);
            v9x_d3d_contexts[index].active = 0ul;
            v9x_d3d_contexts[index].pid = 0ul;
            v9x_d3d_contexts[index].target = 0;
            v9x_d3d_contexts[index].zbuffer = 0;
            v9x_d3d_contexts[index].target_offset = 0ul;
            v9x_d3d_contexts[index].pitch = 0ul;
            v9x_d3d_contexts[index].width = 0ul;
            v9x_d3d_contexts[index].height = 0ul;
            v9x_d3d_contexts[index].specular_enable = 0ul;
            v9x_d3d_contexts[index].fog_enable = 0ul;
            v9x_d3d_contexts[index].fog_color = 0ul;
            v9x_d3d_contexts[index].alpha_blend_enable = 0ul;
            v9x_d3d_contexts[index].src_blend = 0ul;
            v9x_d3d_contexts[index].dest_blend = 0ul;
            v9x_d3d_contexts[index].texture_handle = 0ul;
            v9x_d3d_contexts[index].texture_min = 0ul;
            v9x_d3d_contexts[index].texture_mag = 0ul;
            v9x_d3d_contexts[index].texture_blend = 0ul;
            v9x_d3d_contexts[index].texture_wrap = 0ul;
            v9x_d3d_contexts[index].texture_border = 0ul;
        }
    }
    data->ddrval = V9X_DD_OK;
    ++v9x_hal->d3d_diagnostics.context_destroy_alls;
    v9x_trace_exit(V9X_TRACE_D3D_CTXDESTROYALL, data->ddrval);
    return V9X_DDHAL_DRIVER_HANDLED;
}

DWORD __stdcall V9xD3dTextureCreate(V9X_D3DHAL_TEXTURECREATEDATA *data)
{
    DWORD index;

    v9x_trace_enter(V9X_TRACE_D3D_TEXTURECREATE,
                    data != 0 ? data->dwhContext : 0ul);
    if (data == 0 || data->lpDDS == 0 ||
        v9x_d3d_context_from_handle(data->dwhContext) == 0) {
        if (data != 0) {
            data->ddrval = 0x80070057ul;
        }
        v9x_trace_exit(V9X_TRACE_D3D_TEXTURECREATE, 0x80070057ul);
        return V9X_DDHAL_DRIVER_HANDLED;
    }
    for (index = 0ul; index < V9X_D3D_TEXTURE_COUNT; ++index) {
        if (v9x_d3d_textures[index].active == 0ul) {
            v9x_d3d_textures[index].active = 1ul;
            v9x_d3d_textures[index].context = data->dwhContext;
            v9x_d3d_textures[index].surface = data->lpDDS;
            data->dwHandle = (DWORD)&v9x_d3d_textures[index];
            data->ddrval = V9X_DD_OK;
            ++v9x_hal->d3d_diagnostics.texture_creates;
            v9x_trace_exit(V9X_TRACE_D3D_TEXTURECREATE, data->ddrval);
            return V9X_DDHAL_DRIVER_HANDLED;
        }
    }
    data->ddrval = 0x8007000eul;
    v9x_trace_exit(V9X_TRACE_D3D_TEXTURECREATE, data->ddrval);
    return V9X_DDHAL_DRIVER_HANDLED;
}

DWORD __stdcall V9xD3dTextureDestroy(V9X_D3DHAL_TEXTUREDESTROYDATA *data)
{
    V9X_D3D_TEXTURE *texture;

    v9x_trace_enter(V9X_TRACE_D3D_TEXTUREDESTROY,
                    data != 0 ? data->dwHandle : 0ul);
    texture = data != 0
        ? v9x_d3d_texture_from_handle(data->dwHandle, data->dwhContext) : 0;
    if (texture == 0) {
        if (data != 0) {
            data->ddrval = 0x80070057ul;
        }
        v9x_trace_exit(V9X_TRACE_D3D_TEXTUREDESTROY, 0x80070057ul);
        return V9X_DDHAL_DRIVER_HANDLED;
    }
    texture->active = 0ul;
    texture->context = 0ul;
    texture->surface = 0;
    data->ddrval = V9X_DD_OK;
    ++v9x_hal->d3d_diagnostics.texture_destroys;
    v9x_trace_exit(V9X_TRACE_D3D_TEXTUREDESTROY, data->ddrval);
    return V9X_DDHAL_DRIVER_HANDLED;
}

DWORD __stdcall V9xD3dTextureSwap(V9X_D3DHAL_TEXTURESWAPDATA *data)
{
    V9X_D3D_TEXTURE *first;
    V9X_D3D_TEXTURE *second;
    void *surface;

    v9x_trace_enter(V9X_TRACE_D3D_TEXTURESWAP,
                    data != 0 ? data->dwHandle1 : 0ul);
    first = data != 0
        ? v9x_d3d_texture_from_handle(data->dwHandle1, data->dwhContext) : 0;
    second = data != 0
        ? v9x_d3d_texture_from_handle(data->dwHandle2, data->dwhContext) : 0;
    if (first == 0 || second == 0) {
        if (data != 0) {
            data->ddrval = 0x80070057ul;
        }
        v9x_trace_exit(V9X_TRACE_D3D_TEXTURESWAP, 0x80070057ul);
        return V9X_DDHAL_DRIVER_HANDLED;
    }
    surface = first->surface;
    first->surface = second->surface;
    second->surface = surface;
    data->ddrval = V9X_DD_OK;
    ++v9x_hal->d3d_diagnostics.texture_swaps;
    v9x_trace_exit(V9X_TRACE_D3D_TEXTURESWAP, data->ddrval);
    return V9X_DDHAL_DRIVER_HANDLED;
}

DWORD __stdcall V9xD3dTextureGetSurf(V9X_D3DHAL_TEXTUREGETSURFDATA *data)
{
    V9X_D3D_TEXTURE *texture;

    v9x_trace_enter(V9X_TRACE_D3D_TEXTUREGETSURF,
                    data != 0 ? data->dwHandle : 0ul);
    texture = data != 0
        ? v9x_d3d_texture_from_handle(data->dwHandle, data->dwhContext) : 0;
    if (texture == 0) {
        if (data != 0) {
            data->ddrval = 0x80070057ul;
        }
        v9x_trace_exit(V9X_TRACE_D3D_TEXTUREGETSURF, 0x80070057ul);
        return V9X_DDHAL_DRIVER_HANDLED;
    }
    data->lpDDS = (DWORD)texture->surface;
    data->ddrval = V9X_DD_OK;
    ++v9x_hal->d3d_diagnostics.texture_get_surfs;
    v9x_trace_exit(V9X_TRACE_D3D_TEXTUREGETSURF, data->ddrval);
    return V9X_DDHAL_DRIVER_HANDLED;
}

DWORD __stdcall V9xD3dRenderState(V9X_D3DHAL_RENDERSTATEDATA *data)
{
    V9X_D3D_CONTEXT *context;
    V9X_DD_SURFACE_LCL *exe;
    V9X_D3DSTATE *states;
    DWORD index;

    v9x_trace_enter(V9X_TRACE_D3D_RENDERSTATE,
                    data != 0 ? data->dwCount : 0ul);
    if (v9x_hal != 0) {
        ++v9x_hal->d3d_diagnostics.render_state_calls;
    }
    context = data != 0
        ? v9x_d3d_context_from_handle(data->dwhContext) : 0;
    exe = data != 0 ? v9x_d3d_surface_lcl(data->lpExeBuf) : 0;
    if (context != 0 && exe != 0 && exe->lpGbl != 0 &&
        exe->lpGbl->fpVidMem != 0ul && data->dwCount <= 64ul) {
        states = (V9X_D3DSTATE *)(exe->lpGbl->fpVidMem + data->dwOffset);
        for (index = 0ul; index < data->dwCount; ++index) {
            switch (states[index].type) {
            case V9X_D3DRENDERSTATE_TEXTUREHANDLE:
                context->texture_handle = states[index].argument;
                break;
            case V9X_D3DRENDERSTATE_TEXTUREPERSPECTIVE:
                /* Perspective setup is added after the affine texture gate. */
                break;
            case V9X_D3DRENDERSTATE_WRAPU:
            case V9X_D3DRENDERSTATE_WRAPV:
                context->texture_wrap = states[index].argument != 0ul;
                break;
            case V9X_D3DRENDERSTATE_TEXTUREMAG:
                context->texture_mag = states[index].argument;
                break;
            case V9X_D3DRENDERSTATE_TEXTUREMIN:
                context->texture_min = states[index].argument;
                break;
            case V9X_D3DRENDERSTATE_TEXTUREMAPBLEND:
                context->texture_blend = states[index].argument;
                break;
            case V9X_D3DRENDERSTATE_BORDERCOLOR:
                context->texture_border = states[index].argument;
                break;
            case V9X_D3DRENDERSTATE_SRCBLEND:
                context->src_blend = states[index].argument;
                break;
            case V9X_D3DRENDERSTATE_DESTBLEND:
                context->dest_blend = states[index].argument;
                break;
            case V9X_D3DRENDERSTATE_ALPHABLENDENABLE:
                context->alpha_blend_enable = states[index].argument != 0ul;
                break;
            case V9X_D3DRENDERSTATE_FOGENABLE:
                context->fog_enable = states[index].argument != 0ul;
                break;
            case V9X_D3DRENDERSTATE_SPECULARENABLE:
                context->specular_enable = states[index].argument != 0ul;
                break;
            case V9X_D3DRENDERSTATE_FOGCOLOR:
                context->fog_color = states[index].argument;
                break;
            default:
                break;
            }
        }
    }
    if (data != 0) {
        data->ddrval = V9X_DD_OK;
    }
    v9x_trace_exit(V9X_TRACE_D3D_RENDERSTATE, V9X_DD_OK);
    return V9X_DDHAL_DRIVER_NOTHANDLED;
}

static BYTE v9x_d3d_saturating_add_byte(BYTE first, BYTE second)
{
    WORD sum = (WORD)first + (WORD)second;

    return sum > 255u ? 255u : (BYTE)sum;
}

static BYTE v9x_d3d_fog_byte(BYTE color, BYTE fog, BYTE factor)
{
    return (BYTE)(((DWORD)color * factor +
                   (DWORD)fog * (255u - factor) + 127ul) / 255ul);
}

static void v9x_d3d_apply_vertex_color(const V9X_D3D_CONTEXT *context,
                                       V9X_D3DTLVERTEX *vertex)
{
    DWORD color = vertex->color;
    BYTE alpha = (BYTE)(color >> 24);
    BYTE red = (BYTE)(color >> 16);
    BYTE green = (BYTE)(color >> 8);
    BYTE blue = (BYTE)color;

    if (context->specular_enable != 0ul) {
        red = v9x_d3d_saturating_add_byte(
            red, (BYTE)(vertex->specular >> 16));
        green = v9x_d3d_saturating_add_byte(
            green, (BYTE)(vertex->specular >> 8));
        blue = v9x_d3d_saturating_add_byte(blue, (BYTE)vertex->specular);
    }
    if (context->fog_enable != 0ul) {
        BYTE factor = (BYTE)(vertex->specular >> 24);

        red = v9x_d3d_fog_byte(red, (BYTE)(context->fog_color >> 16),
                               factor);
        green = v9x_d3d_fog_byte(green, (BYTE)(context->fog_color >> 8),
                                 factor);
        blue = v9x_d3d_fog_byte(blue, (BYTE)context->fog_color, factor);
    }
    vertex->color = ((DWORD)alpha << 24) | ((DWORD)red << 16) |
                    ((DWORD)green << 8) | (DWORD)blue;
}

#define V9X_D3DOP_TRIANGLE             3u
#define V9X_D3DOP_EXIT                11u
#define V9X_D3DHAL_EXECUTE_OVERRIDE    1ul
#define V9X_D3DHAL_EXECUTE_UNHANDLED   0x00000211ul

DWORD __stdcall V9xD3dExecute(V9X_D3DHAL_EXECUTEDATA *data)
{
    V9X_DD_SURFACE_LCL *exe;
    V9X_D3DINSTRUCTION *instruction;
    BYTE *base;
    DWORD offset;
    DWORD end;
    int one_instruction;

    v9x_trace_enter(V9X_TRACE_D3D_EXECUTE,
                    data != 0 ? data->dwFlags : 0ul);
    if (v9x_hal != 0) {
        ++v9x_hal->d3d_diagnostics.execute_calls;
    }
    exe = data != 0 ? v9x_d3d_surface_lcl(data->lpExeBuf) : 0;
    if (data == 0 || v9x_d3d_context_from_handle(data->dwhContext) == 0 ||
        exe == 0 || exe->lpGbl == 0 || exe->lpGbl->fpVidMem == 0ul ||
        data->deExData.dwInstructionLength > 0x00100000ul) {
        if (data != 0) {
            data->ddrval = 0x80070057ul;
        }
        v9x_trace_exit(V9X_TRACE_D3D_EXECUTE, 0x80070057ul);
        return V9X_DDHAL_DRIVER_HANDLED;
    }
    base = (BYTE *)exe->lpGbl->fpVidMem;
    one_instruction = (data->dwFlags & V9X_D3DHAL_EXECUTE_OVERRIDE) != 0ul;
    offset = one_instruction ? data->dwOffset
                             : data->deExData.dwInstructionOffset +
                               data->dwOffset;
    end = data->deExData.dwInstructionOffset +
          data->deExData.dwInstructionLength;
    for (;;) {
        DWORD bytes;

        if (!one_instruction && (offset > end ||
            end - offset < sizeof(V9X_D3DINSTRUCTION))) {
            data->ddrval = 0x80070057ul;
            v9x_trace_exit(V9X_TRACE_D3D_EXECUTE, data->ddrval);
            return V9X_DDHAL_DRIVER_HANDLED;
        }
        instruction = one_instruction ? &data->diInstruction
                                      : (V9X_D3DINSTRUCTION *)(base + offset);
        bytes = (DWORD)instruction->bSize * (DWORD)instruction->wCount;
        if (!one_instruction && bytes > end - offset -
            sizeof(V9X_D3DINSTRUCTION)) {
            data->ddrval = 0x80070057ul;
            v9x_trace_exit(V9X_TRACE_D3D_EXECUTE, data->ddrval);
            return V9X_DDHAL_DRIVER_HANDLED;
        }
        if (instruction->bOpcode == V9X_D3DOP_EXIT) {
            data->ddrval = V9X_DD_OK;
            v9x_trace_exit(V9X_TRACE_D3D_EXECUTE, data->ddrval);
            return V9X_DDHAL_DRIVER_HANDLED;
        }
        if (instruction->bOpcode == V9X_D3DOP_TRIANGLE) {
            V9X_D3DHAL_RENDERPRIMITIVEDATA primitive;

            primitive.dwhContext = data->dwhContext;
            primitive.dwOffset = one_instruction ? data->dwOffset
                : offset + sizeof(V9X_D3DINSTRUCTION);
            primitive.dwStatus = data->dwStatus;
            primitive.lpExeBuf = data->lpExeBuf;
            primitive.dwTLOffset = data->lpTLBuf != 0
                ? 0ul : data->deExData.dwVertexOffset;
            primitive.lpTLBuf = data->lpTLBuf != 0
                ? data->lpTLBuf : data->lpExeBuf;
            primitive.diInstruction = *instruction;
            primitive.ddrval = V9X_DD_OK;
            (void)V9xD3dRenderPrimitive(&primitive);
            if (primitive.ddrval != V9X_DD_OK) {
                data->ddrval = primitive.ddrval;
                v9x_trace_exit(V9X_TRACE_D3D_EXECUTE, data->ddrval);
                return V9X_DDHAL_DRIVER_HANDLED;
            }
        } else {
            data->dwOffset = one_instruction ? data->dwOffset
                : offset - data->deExData.dwInstructionOffset;
            data->ddrval = V9X_D3DHAL_EXECUTE_UNHANDLED;
            v9x_trace_exit(V9X_TRACE_D3D_EXECUTE, data->ddrval);
            return one_instruction ? V9X_DDHAL_DRIVER_NOTHANDLED
                                   : V9X_DDHAL_DRIVER_HANDLED;
        }
        if (one_instruction) {
            data->ddrval = V9X_DD_OK;
            v9x_trace_exit(V9X_TRACE_D3D_EXECUTE, data->ddrval);
            return V9X_DDHAL_DRIVER_HANDLED;
        }
        offset += sizeof(V9X_D3DINSTRUCTION) + bytes;
    }
}

DWORD __stdcall V9xD3dExecuteClipped(
    V9X_D3DHAL_EXECUTECLIPPEDDATA *data)
{
    V9X_D3DHAL_EXECUTEDATA execute;
    DWORD handled;

    if (data == 0) {
        return V9X_DDHAL_DRIVER_HANDLED;
    }
    execute.dwhContext = data->dwhContext;
    execute.dwOffset = data->dwOffset;
    execute.dwFlags = data->dwFlags;
    execute.dwStatus = data->dwStatus;
    execute.deExData = data->deExData;
    execute.lpExeBuf = data->lpExeBuf;
    execute.lpTLBuf = data->lpTLBuf;
    execute.diInstruction = data->diInstruction;
    execute.ddrval = data->ddrval;
    handled = V9xD3dExecute(&execute);
    data->dwOffset = execute.dwOffset;
    data->dwStatus = execute.dwStatus;
    data->ddrval = execute.ddrval;
    return handled;
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

    v9x_trace_enter(V9X_TRACE_D3D_RENDERPRIM,
                    data != 0
                        ? (((DWORD)data->diInstruction.bOpcode << 24) |
                           ((DWORD)data->diInstruction.bSize << 16) |
                           (DWORD)data->diInstruction.wCount)
                        : 0ul);
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
        data->diInstruction.wCount <= V9X_D3D_MAX_BATCH_TRIANGLES) {
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
            V9X_D3DTLVERTEX source[3];
            V9X_D3DTLVERTEX clipped[8];
            int clipped_count;
            int fan;

            source[0] = vertices[triangle->v1];
            source[1] = vertices[triangle->v2];
            source[2] = vertices[triangle->v3];
            v9x_d3d_apply_vertex_color(context, &source[0]);
            v9x_d3d_apply_vertex_color(context, &source[1]);
            v9x_d3d_apply_vertex_color(context, &source[2]);
            clipped_count = v9x_d3d_clip_triangle(context, source, clipped);
            if (clipped_count < 0) {
                v9x_trace_push(V9X_TRACE_D3D_PRIMREJECT,
                               0x20000000ul | index);
                ok = 0;
                break;
            }
            for (fan = 1; fan + 1 < clipped_count; ++fan) {
                V9X_D3DTLVERTEX clipped_triangle[3];

                clipped_triangle[0] = clipped[0];
                clipped_triangle[1] = clipped[fan];
                clipped_triangle[2] = clipped[fan + 1];
                if (!v9x_d3d_triangle(context, clipped_triangle)) {
                    v9x_trace_push(V9X_TRACE_D3D_PRIMREJECT,
                                   0x30000000ul | index);
                    ok = 0;
                    break;
                }
            }
            if (!ok) {
                break;
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
    v9x_trace_exit(V9X_TRACE_D3D_RENDERPRIM,
                   ok ? V9X_DD_OK : 0x80070057ul);
    return V9X_DDHAL_DRIVER_HANDLED;
}

DWORD __stdcall V9xD3dSetRenderTarget(
    V9X_D3DHAL_SETRENDERTARGETDATA *data)
{
    V9X_D3D_CONTEXT *context = data != 0
        ? v9x_d3d_context_from_handle(data->dwhContext) : 0;

    v9x_trace_enter(V9X_TRACE_D3D_SETRENDERTARGET,
                    data != 0 ? data->dwhContext : 0ul);
    if (context == 0 ||
        !v9x_d3d_set_target(context, data->lpDDS, data->lpDDSZ)) {
        if (data != 0) {
            data->ddrval = 0x80070057ul;
        }
        v9x_trace_exit(V9X_TRACE_D3D_SETRENDERTARGET, 0x80070057ul);
        return V9X_DDHAL_DRIVER_HANDLED;
    }
    data->ddrval = V9X_DD_OK;
    v9x_trace_exit(V9X_TRACE_D3D_SETRENDERTARGET, data->ddrval);
    return V9X_DDHAL_DRIVER_HANDLED;
}

static LONG v9x_d3d_fixed_12_20(float value)
{
    return v9x_float_to_long(value * 1048576.0f);
}

static WORD v9x_d3d_fixed_8_7(float value)
{
    LONG fixed = v9x_float_to_long(value * 128.0f);

    if (fixed < -32768l) {
        fixed = -32768l;
    } else if (fixed > 32767l) {
        fixed = 32767l;
    }
    return (WORD)fixed;
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
    float fdxr;
    float dgdx, dbdx, drdx;
    float dgdy, dbdy, drdy;
    float dadx, dady;
    float dudx = 0.0f, dvdx = 0.0f;
    float dudy = 0.0f, dvdy = 0.0f;
    DWORD color;
    DWORD gs_bs;
    DWORD as_rs;
    DWORD command;
    DWORD texture_offset = 0ul;
    DWORD texture_size_log = 0ul;
    DWORD texture_d = 0ul;
    DWORD texture_level = 0ul;
    BYTE trilinear_alpha = 0u;
    int trilinear_blend = 0;
    int texture_mipmapped = 0;
    int textured;

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
    fdxr = dx < 0.0f ? -1.0f / dx : 1.0f / dx;
    dgdy = ((float)((LONG)((p2->color >> 8) & 0xfful) -
                          (LONG)((p0->color >> 8) & 0xfful))) * fdy02r;
    dbdy = ((float)((LONG)(p2->color & 0xfful) -
                          (LONG)(p0->color & 0xfful))) * fdy02r;
    drdy = ((float)((LONG)((p2->color >> 16) & 0xfful) -
                          (LONG)((p0->color >> 16) & 0xfful))) * fdy02r;
    dgdx = ((float)((LONG)((p1->color >> 8) & 0xfful) -
                          (LONG)((p0->color >> 8) & 0xfful)) -
            dgdy * fdy01) * fdxr;
    dbdx = ((float)((LONG)(p1->color & 0xfful) -
                          (LONG)(p0->color & 0xfful)) -
            dbdy * fdy01) * fdxr;
    drdx = ((float)((LONG)((p1->color >> 16) & 0xfful) -
                          (LONG)((p0->color >> 16) & 0xfful)) -
            drdy * fdy01) * fdxr;
    dady = ((float)((LONG)((p2->color >> 24) & 0xfful) -
                          (LONG)((p0->color >> 24) & 0xfful))) * fdy02r;
    dadx = ((float)((LONG)((p1->color >> 24) & 0xfful) -
                          (LONG)((p0->color >> 24) & 0xfful)) -
            dady * fdy01) * fdxr;
    textured = v9x_d3d_texture_info(context, &texture_offset,
                                    &texture_size_log, &texture_mipmapped);
    if (textured) {
        dudy = (p2->tu - p0->tu) * 134217728.0f * fdy02r;
        dvdy = (p2->tv - p0->tv) * 134217728.0f * fdy02r;
        dudx = ((p1->tu - p0->tu) * 134217728.0f - dudy * fdy01) *
                fdxr;
        dvdx = ((p1->tv - p0->tv) * 134217728.0f - dvdy * fdy01) *
                fdxr;
        if (texture_mipmapped &&
            context->texture_min >= V9X_D3DFILTER_MIPNEAREST) {
            float rho = dudx < 0.0f ? -dudx : dudx;
            float derivative;
            float level_base = 134217728.0f /
                               (float)(1ul << texture_size_log);
            DWORD level = 0ul;

            derivative = dvdx < 0.0f ? -dvdx : dvdx;
            if (derivative > rho) rho = derivative;
            derivative = dudy < 0.0f ? -dudy : dudy;
            if (derivative > rho) rho = derivative;
            derivative = dvdy < 0.0f ? -dvdy : dvdy;
            if (derivative > rho) rho = derivative;
            while (level < texture_size_log && rho >= level_base * 2.0f) {
                level_base *= 2.0f;
                ++level;
            }
            texture_d = level << 27;
            texture_level = level;
            if ((context->texture_min == V9X_D3DFILTER_MIPLINEAR ||
                 context->texture_min == V9X_D3DFILTER_LINEARMIPLINEAR) &&
                level < texture_size_log && rho > level_base) {
                texture_d += (DWORD)v9x_float_to_long(
                    ((rho - level_base) / level_base) * 134217727.0f);
            }
            if (context->texture_min ==
                    V9X_D3DFILTER_LINEARMIPLINEAR &&
                context->alpha_blend_enable == 0ul &&
                level < texture_size_log &&
                (texture_d & 0x07fffffful) != 0ul) {
                trilinear_alpha = (BYTE)v9x_float_to_long(
                    ((float)(texture_d & 0x07fffffful) /
                     134217727.0f) * 255.0f);
                trilinear_blend = 1;
                texture_d = level << 27;
            }
        }
    }
    color = p0->color;

    if (!v9x_wait_idle(1) || !v9x_wait_fifo(9ul, 1)) {
        return 0;
    }
    v9x_mmio_write(V9X_VIRGE_3D_Z_BASE, 0ul);
    v9x_mmio_write(V9X_VIRGE_3D_DEST_BASE, context->target_offset);
    v9x_mmio_write(V9X_VIRGE_3D_CLIP_L_R, context->width - 1ul);
    v9x_mmio_write(V9X_VIRGE_3D_CLIP_T_B, context->height - 1ul);
    v9x_mmio_write(V9X_VIRGE_3D_DEST_SRC_STRIDE, context->pitch << 16);
    v9x_mmio_write(V9X_VIRGE_3D_Z_STRIDE, context->width * 2ul);
    v9x_mmio_write(V9X_VIRGE_3D_TEX_BASE,
                   textured ? texture_offset : 0ul);
    v9x_mmio_write(V9X_VIRGE_3D_TEX_BORDER, context->texture_border);
    v9x_mmio_write(V9X_VIRGE_3D_FADE_COLOR, 0ul);

    if (textured) {
        if (!v9x_wait_fifo(9ul, 1)) {
            return 0;
        }
        v9x_mmio_write(V9X_VIRGE_3D_TBV, 0ul);
        v9x_mmio_write(V9X_VIRGE_3D_TBU, 0ul);
        v9x_mmio_write(V9X_VIRGE_3D_DVDX, (DWORD)v9x_float_to_long(dvdx));
        v9x_mmio_write(V9X_VIRGE_3D_DUDX, (DWORD)v9x_float_to_long(dudx));
        v9x_mmio_write(V9X_VIRGE_3D_DVDY, (DWORD)v9x_float_to_long(dvdy));
        v9x_mmio_write(V9X_VIRGE_3D_DUDY, (DWORD)v9x_float_to_long(dudy));
        v9x_mmio_write(V9X_VIRGE_3D_DS, texture_d);
        v9x_mmio_write(V9X_VIRGE_3D_VS,
            (DWORD)v9x_float_to_long(p0->tv * 134217728.0f));
        v9x_mmio_write(V9X_VIRGE_3D_US,
            (DWORD)v9x_float_to_long(p0->tu * 134217728.0f));
    }
    if (!v9x_wait_fifo(15ul, 1)) {
        return 0;
    }
    /* With AE set, CMD_SET establishes persistent state; the final
     * Y01_Y12 write launches the triangle. */
    command = V9X_VIRGE_3D_CMD_GOURAUD_16_AE;
    if (textured) {
        command |= V9X_VIRGE_3D_CMD_TEX_ARGB1555 |
                   (texture_size_log << 8);
        if (context->texture_blend == V9X_D3DTBLEND_MODULATE) {
            command |= V9X_VIRGE_3D_CMD_TEXTURE_LIT |
                       V9X_VIRGE_3D_CMD_TEX_MODULATE;
        } else {
            command |= V9X_VIRGE_3D_CMD_TEXTURE_UNLIT;
        }
        if (texture_mipmapped &&
            context->texture_min == V9X_D3DFILTER_MIPNEAREST) {
            command |= V9X_VIRGE_3D_CMD_MIP_NEAREST;
        } else if (texture_mipmapped &&
                   context->texture_min == V9X_D3DFILTER_MIPLINEAR) {
            command |= V9X_VIRGE_3D_CMD_MIP_LINEAR;
        } else if (texture_mipmapped &&
                   context->texture_min ==
                       V9X_D3DFILTER_LINEARMIPNEAREST) {
            command |= V9X_VIRGE_3D_CMD_LINEAR_MIP_NEAREST;
        } else if (texture_mipmapped &&
                   context->texture_min ==
                       V9X_D3DFILTER_LINEARMIPLINEAR) {
            command |= trilinear_blend
                ? V9X_VIRGE_3D_CMD_LINEAR_MIP_NEAREST
                : V9X_VIRGE_3D_CMD_LINEAR_MIP_LINEAR;
        } else if (context->texture_min == V9X_D3DFILTER_LINEAR ||
                   context->texture_mag == V9X_D3DFILTER_LINEAR) {
            command |= V9X_VIRGE_3D_CMD_FILTER_LINEAR;
        } else {
            command |= V9X_VIRGE_3D_CMD_FILTER_NEAREST;
        }
        if (context->texture_wrap != 0ul) {
            command |= V9X_VIRGE_3D_CMD_TEXTURE_WRAP;
        }
    }
    if (context->alpha_blend_enable != 0ul &&
        context->src_blend == V9X_D3DBLEND_SRCALPHA &&
        context->dest_blend == V9X_D3DBLEND_INVSRCALPHA) {
        command |= V9X_VIRGE_3D_CMD_ALPHA_SOURCE |
                   V9X_VIRGE_3D_CMD_ALPHA_ENABLE;
    }
    v9x_mmio_write(V9X_VIRGE_3D_COMMAND, command);
    gs_bs = (((color >> 8) & 0xfful) << 23) |
            ((color & 0xfful) << 7);
    as_rs = (((color >> 24) & 0xfful) << 23) |
            (((color >> 16) & 0xfful) << 7);
    v9x_mmio_write(V9X_VIRGE_3D_DGDX_DBDX,
                   ((DWORD)v9x_d3d_fixed_8_7(dgdx) << 16) |
                   (DWORD)v9x_d3d_fixed_8_7(dbdx));
    v9x_mmio_write(V9X_VIRGE_3D_DADX_DRDX,
                   ((DWORD)v9x_d3d_fixed_8_7(dadx) << 16) |
                   (DWORD)v9x_d3d_fixed_8_7(drdx));
    v9x_mmio_write(V9X_VIRGE_3D_DGDY_DBDY,
                   ((DWORD)v9x_d3d_fixed_8_7(dgdy) << 16) |
                   (DWORD)v9x_d3d_fixed_8_7(dbdy));
    v9x_mmio_write(V9X_VIRGE_3D_DADY_DRDY,
                   ((DWORD)v9x_d3d_fixed_8_7(dady) << 16) |
                   (DWORD)v9x_d3d_fixed_8_7(drdy));
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
    if (trilinear_blend) {
        DWORD second_command = command;

        if (!v9x_wait_idle(1) || !v9x_wait_fifo(9ul, 1)) {
            return 0;
        }
        v9x_mmio_write(V9X_VIRGE_3D_TBV, 0ul);
        v9x_mmio_write(V9X_VIRGE_3D_TBU, 0ul);
        v9x_mmio_write(V9X_VIRGE_3D_DVDX,
                       (DWORD)v9x_float_to_long(dvdx));
        v9x_mmio_write(V9X_VIRGE_3D_DUDX,
                       (DWORD)v9x_float_to_long(dudx));
        v9x_mmio_write(V9X_VIRGE_3D_DVDY,
                       (DWORD)v9x_float_to_long(dvdy));
        v9x_mmio_write(V9X_VIRGE_3D_DUDY,
                       (DWORD)v9x_float_to_long(dudy));
        v9x_mmio_write(V9X_VIRGE_3D_DS, (texture_level + 1ul) << 27);
        v9x_mmio_write(V9X_VIRGE_3D_VS,
            (DWORD)v9x_float_to_long(p0->tv * 134217728.0f));
        v9x_mmio_write(V9X_VIRGE_3D_US,
            (DWORD)v9x_float_to_long(p0->tu * 134217728.0f));
        if (!v9x_wait_fifo(15ul, 1)) {
            return 0;
        }
        second_command &= ~V9X_VIRGE_3D_CMD_TEXTURE_UNLIT;
        second_command |= V9X_VIRGE_3D_CMD_TEXTURE_LIT |
                          V9X_VIRGE_3D_CMD_TEX_MODULATE |
                          V9X_VIRGE_3D_CMD_ALPHA_SOURCE |
                          V9X_VIRGE_3D_CMD_ALPHA_ENABLE;
        v9x_mmio_write(V9X_VIRGE_3D_COMMAND, second_command);
        if (context->texture_blend == V9X_D3DTBLEND_MODULATE) {
            v9x_mmio_write(V9X_VIRGE_3D_DGDX_DBDX,
                           ((DWORD)v9x_d3d_fixed_8_7(dgdx) << 16) |
                           (DWORD)v9x_d3d_fixed_8_7(dbdx));
            v9x_mmio_write(V9X_VIRGE_3D_DGDY_DBDY,
                           ((DWORD)v9x_d3d_fixed_8_7(dgdy) << 16) |
                           (DWORD)v9x_d3d_fixed_8_7(dbdy));
            v9x_mmio_write(V9X_VIRGE_3D_GS_BS, gs_bs);
            v9x_mmio_write(V9X_VIRGE_3D_AS_RS,
                ((DWORD)trilinear_alpha << 23) |
                (((color >> 16) & 0xfful) << 7));
            v9x_mmio_write(V9X_VIRGE_3D_DADX_DRDX,
                           (DWORD)v9x_d3d_fixed_8_7(drdx));
            v9x_mmio_write(V9X_VIRGE_3D_DADY_DRDY,
                           (DWORD)v9x_d3d_fixed_8_7(drdy));
        } else {
            v9x_mmio_write(V9X_VIRGE_3D_DGDX_DBDX, 0ul);
            v9x_mmio_write(V9X_VIRGE_3D_DGDY_DBDY, 0ul);
            v9x_mmio_write(V9X_VIRGE_3D_GS_BS,
                           (255ul << 23) | (255ul << 7));
            v9x_mmio_write(V9X_VIRGE_3D_AS_RS,
                           ((DWORD)trilinear_alpha << 23) |
                           (255ul << 7));
            v9x_mmio_write(V9X_VIRGE_3D_DADX_DRDX, 0ul);
            v9x_mmio_write(V9X_VIRGE_3D_DADY_DRDY, 0ul);
        }
        v9x_mmio_write(V9X_VIRGE_3D_DXDY12,
                       (DWORD)v9x_d3d_fixed_12_20(dxdy12));
        v9x_mmio_write(V9X_VIRGE_3D_XEND12,
                       (DWORD)v9x_d3d_fixed_12_20(
                           p1->sx + dxdy12 * (p1->sy - (float)i1y)));
        v9x_mmio_write(V9X_VIRGE_3D_DXDY01,
                       (DWORD)v9x_d3d_fixed_12_20(dxdy01));
        v9x_mmio_write(V9X_VIRGE_3D_XEND01,
                       (DWORD)v9x_d3d_fixed_12_20(
                           p0->sx + dxdy01 * fdycc));
        v9x_mmio_write(V9X_VIRGE_3D_DXDY02,
                       (DWORD)v9x_d3d_fixed_12_20(dxdy02));
        v9x_mmio_write(V9X_VIRGE_3D_XSTART02,
                       (DWORD)v9x_d3d_fixed_12_20(
                           p0->sx + dxdy02 * fdycc));
        v9x_mmio_write(V9X_VIRGE_3D_YSTART, (DWORD)i0y);
        v9x_mmio_write(V9X_VIRGE_3D_Y01_Y12,
                       ((DWORD)dy01 << 16) |
                       (DWORD)(dy12 +
                           (p2->sy == (float)i2y ? 1l : 0l)) |
                       (dx > 0.0f ? 0x80000000ul : 0ul));
    }
    return 1;
}

DWORD __stdcall V9xD3dDrawOnePrimitive(
    V9X_D3DHAL_DRAWONEPRIMITIVEDATA *data)
{
    V9X_FPU_AREA fpu;
    V9X_D3D_CONTEXT *context;
    DWORD status;
    int ok = 0;

    v9x_trace_enter(V9X_TRACE_D3D_DRAWONEPRIM,
                    data != 0
                        ? ((data->PrimitiveType << 16) |
                           (data->dwNumVertices & 0xfffful))
                        : 0ul);
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
    v9x_trace_exit(V9X_TRACE_D3D_DRAWONEPRIM,
                   ok ? V9X_DD_OK : 0x80070057ul);
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

    v9x_trace_enter(V9X_TRACE_D3D_DRAWPRIMS,
                    data != 0 ? (DWORD)data->lpvData : 0ul);
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
    v9x_trace_exit(V9X_TRACE_D3D_DRAWPRIMS,
                   ok ? V9X_DD_OK : 0x80070057ul);
    return V9X_DDHAL_DRIVER_HANDLED;
}

DWORD __stdcall V9xD3dDrawOneIndexedPrimitive(void *data)
{
    (void)data;
    v9x_trace_enter(V9X_TRACE_D3D_DRAWONEINDEXED, 0ul);
    v9x_trace_exit(V9X_TRACE_D3D_DRAWONEINDEXED, 0ul);
    return V9X_DDHAL_DRIVER_NOTHANDLED;
}

DWORD __stdcall V9xHalGetDriverInfo(V9X_DDHAL_GETDRIVERINFODATA *data)
{
#if V9X_C3_SERVE_D3D_CALLBACKS2
    DWORD index;
    DWORD bytes;
    BYTE *destination;
    const BYTE *source;
#endif

    if (data == 0) {
        return V9X_DDHAL_DRIVER_HANDLED;
    }
    v9x_trace_enter(V9X_TRACE_GETDRIVERINFO,
                    ((DWORD)data->guidInfo[3] << 24) |
                    ((DWORD)data->guidInfo[2] << 16) |
                    ((DWORD)data->guidInfo[1] << 8) |
                    (DWORD)data->guidInfo[0]);
    data->dwActualSize = 0ul;
    data->ddRVal = 0x88760028ul;
#if V9X_C3_SERVE_D3D_CALLBACKS2
    for (index = 0ul; index < 16ul; ++index) {
        if (data->guidInfo[index] != v9x_guid_d3d_callbacks2[index]) {
            v9x_trace_exit(V9X_TRACE_GETDRIVERINFO, data->ddRVal);
            return V9X_DDHAL_DRIVER_HANDLED;
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
#endif
    v9x_trace_exit(V9X_TRACE_GETDRIVERINFO, data->ddRVal);
    return V9X_DDHAL_DRIVER_HANDLED;
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
    SetUnhandledExceptionFilter(v9x_unhandled_exception_filter);
    v9x_trace_enter(V9X_TRACE_DRIVERINIT, (DWORD)shared);

    v9x_fill_modes(shared);

    shared->info.dwSize = sizeof(V9X_DDHALINFO);
    shared->info.dwNumModes = V9X_DD_MODE_COUNT;
    shared->info.dwFlags = V9X_DDHALINFO_ISPRIMARYDISPLAY;
    shared->info.dwMonitorFrequency = 60ul;
    /* The 16-bit side stamps the owning selector immediately before
     * DDHAL_SetInfo; a flat DLL base is not a valid DDRAW16 instance. */
    shared->info.hInstance = 0ul;
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
                                  V9X_DDSCAPS_PRIMARYSURFACE |
                                  V9X_DDSCAPS_TEXTURE |
                                  V9X_DDSCAPS_COMPLEX |
                                  V9X_DDSCAPS_MIPMAP |
                                  V9X_DDSCAPS_ZBUFFER;
    shared->info.ddCaps.dwVidMemTotal =
        shared->fb.vram_bytes - shared->fb.visible_bytes;
    shared->info.ddCaps.dwVidMemFree = shared->info.ddCaps.dwVidMemTotal;

    shared->dd_callbacks.dwSize = sizeof(V9X_DDHAL_DDCALLBACKS);
    shared->dd_callbacks.dwFlags = V9X_DDHAL_CB32_CREATESURFACE |
                                   V9X_DDHAL_CB32_CANCREATESURFACE |
                                   V9X_DDHAL_CB32_WAITFORVERTICALBLANK |
                                   V9X_DDHAL_CB32_SETEXCLUSIVEMODE |
                                   V9X_DDHAL_CB32_FLIPTOGDISURFACE;
    shared->dd_callbacks.CreateSurface =
        (V9X_DD_CODE_PTR)V9xHalCreateSurface;
    shared->dd_callbacks.CanCreateSurface =
        (V9X_DD_CODE_PTR)V9xHalCanCreateSurface;
    shared->dd_callbacks.WaitForVerticalBlank =
        (V9X_DD_CODE_PTR)V9xHalWaitForVerticalBlank;
    shared->dd_callbacks.SetExclusiveMode =
        (V9X_DD_CODE_PTR)V9xHalSetExclusiveMode;
    shared->dd_callbacks.FlipToGDISurface =
        (V9X_DD_CODE_PTR)V9xHalFlipToGDISurface;

    shared->surface_callbacks.dwSize = sizeof(V9X_DDHAL_DDSURFACECALLBACKS);
    shared->surface_callbacks.dwFlags =
        V9X_DDHAL_SURFCB32_DESTROYSURFACE |
        V9X_DDHAL_SURFCB32_FLIP | V9X_DDHAL_SURFCB32_GETFLIPSTATUS |
        V9X_DDHAL_SURFCB32_LOCK | V9X_DDHAL_SURFCB32_UNLOCK |
        V9X_DDHAL_SURFCB32_BLT | V9X_DDHAL_SURFCB32_ADDATTACHEDSURFACE |
        V9X_DDHAL_SURFCB32_GETBLTSTATUS;
    shared->surface_callbacks.DestroySurface =
        (V9X_DD_CODE_PTR)V9xHalDestroySurface;
    shared->surface_callbacks.Flip = (V9X_DD_CODE_PTR)V9xHalFlip;
    shared->surface_callbacks.GetFlipStatus =
        (V9X_DD_CODE_PTR)V9xHalGetFlipStatus;
    shared->surface_callbacks.Lock = (V9X_DD_CODE_PTR)V9xHalLock;
    shared->surface_callbacks.Unlock = (V9X_DD_CODE_PTR)V9xHalUnlock;
    shared->surface_callbacks.Blt = (V9X_DD_CODE_PTR)V9xHalBlt;
    shared->surface_callbacks.AddAttachedSurface =
        (V9X_DD_CODE_PTR)V9xHalAddAttachedSurface;
    shared->surface_callbacks.GetBltStatus =
        (V9X_DD_CODE_PTR)V9xHalGetBltStatus;

    shared->palette_callbacks.dwSize =
        sizeof(V9X_DDHAL_DDPALETTECALLBACKS);
    shared->palette_callbacks.dwFlags = 0ul;

    shared->execute_buffer_callbacks.dwSize =
        sizeof(V9X_DDHAL_DDEXEBUFCALLBACKS);
    shared->execute_buffer_callbacks.dwFlags =
        V9X_DDHAL_EXEBUFCB32_CANCREATE |
        V9X_DDHAL_EXEBUFCB32_CREATE |
        V9X_DDHAL_EXEBUFCB32_DESTROY |
        V9X_DDHAL_EXEBUFCB32_LOCK |
        V9X_DDHAL_EXEBUFCB32_UNLOCK;
    shared->execute_buffer_callbacks.CanCreateExecuteBuffer =
        (V9X_DD_CODE_PTR)V9xExeBufCanCreate;
    shared->execute_buffer_callbacks.CreateExecuteBuffer =
        (V9X_DD_CODE_PTR)V9xExeBufCreate;
    shared->execute_buffer_callbacks.DestroyExecuteBuffer =
        (V9X_DD_CODE_PTR)V9xExeBufDestroy;
    shared->execute_buffer_callbacks.LockExecuteBuffer =
        (V9X_DD_CODE_PTR)V9xExeBufLock;
    shared->execute_buffer_callbacks.UnlockExecuteBuffer =
        (V9X_DD_CODE_PTR)V9xExeBufUnlock;

    shared->d3d_global.dwSize = sizeof(V9X_D3DHAL_GLOBALDRIVERDATA);
    shared->d3d_global.hwCaps.dwSize = sizeof(V9X_D3DDEVICEDESC_V1);
    shared->d3d_global.hwCaps.dwFlags =
        V9X_D3DDD_COLORMODEL | V9X_D3DDD_DEVCAPS |
        V9X_D3DDD_TRICAPS |
        V9X_D3DDD_DEVICERENDERBITDEPTH |
        V9X_D3DDD_DEVICEZBUFFERBITDEPTH;
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
    shared->d3d_global.hwCaps.dpcTriCaps.dwRasterCaps =
        V9X_D3DPRASTERCAPS_ZTEST |
        V9X_D3DPRASTERCAPS_SUBPIXEL |
        V9X_D3DPRASTERCAPS_FOGVERTEX;
    shared->d3d_global.hwCaps.dpcTriCaps.dwZCmpCaps =
        V9X_D3DPCMPCAPS_NEVER | V9X_D3DPCMPCAPS_LESS |
        V9X_D3DPCMPCAPS_EQUAL | V9X_D3DPCMPCAPS_LESSEQUAL |
        V9X_D3DPCMPCAPS_GREATER | V9X_D3DPCMPCAPS_NOTEQUAL |
        V9X_D3DPCMPCAPS_GREATEREQUAL | V9X_D3DPCMPCAPS_ALWAYS;
    shared->d3d_global.hwCaps.dpcTriCaps.dwSrcBlendCaps =
        V9X_D3DPBLENDCAPS_SRCALPHA;
    shared->d3d_global.hwCaps.dpcTriCaps.dwDestBlendCaps =
        V9X_D3DPBLENDCAPS_INVSRCALPHA;
    shared->d3d_global.hwCaps.dpcTriCaps.dwShadeCaps =
        V9X_D3DPSHADECAPS_COLORFLATRGB |
        V9X_D3DPSHADECAPS_COLORGOURAUDRGB |
        V9X_D3DPSHADECAPS_SPECULARGOURAUDRGB |
        V9X_D3DPSHADECAPS_ALPHAFLATBLEND |
        V9X_D3DPSHADECAPS_ALPHAGOURAUDBLEND |
        V9X_D3DPSHADECAPS_FOGGOURAUD;
    shared->d3d_global.hwCaps.dpcTriCaps.dwTextureCaps =
#if V9X_C4_CAPS_VARIANT == 1
        0ul;
#elif V9X_C4_CAPS_VARIANT == 2
        V9X_D3DPTEXTURECAPS_PERSPECTIVE |
        V9X_D3DPTEXTURECAPS_POW2 |
        V9X_D3DPTEXTURECAPS_SQUAREONLY;
#else
        V9X_D3DPTEXTURECAPS_PERSPECTIVE;
#endif
#if V9X_C4_CAPS_VARIANT != 1
    shared->d3d_global.hwCaps.dpcTriCaps.dwTextureFilterCaps =
        V9X_D3DPTFILTERCAPS_NEAREST | V9X_D3DPTFILTERCAPS_LINEAR |
        V9X_D3DPTFILTERCAPS_MIPNEAREST |
        V9X_D3DPTFILTERCAPS_MIPLINEAR |
        V9X_D3DPTFILTERCAPS_LINEARMIPNEAREST |
        V9X_D3DPTFILTERCAPS_LINEARMIPLINEAR;
    shared->d3d_global.hwCaps.dpcTriCaps.dwTextureBlendCaps =
        V9X_D3DPTBLENDCAPS_DECAL | V9X_D3DPTBLENDCAPS_MODULATE |
        V9X_D3DPTBLENDCAPS_COPY;
    shared->d3d_global.hwCaps.dpcTriCaps.dwTextureAddressCaps =
        V9X_D3DPTADDRESSCAPS_WRAP | V9X_D3DPTADDRESSCAPS_CLAMP;
#endif
    shared->d3d_global.hwCaps.dwDeviceRenderBitDepth = V9X_DDBD_16;
    shared->d3d_global.hwCaps.dwDeviceZBufferBitDepth = V9X_DDBD_16;
    shared->d3d_global.dwNumVertices = 0ul;
    shared->d3d_global.dwNumClipVertices = 0ul;
    shared->texture_formats[0].dwSize = sizeof(V9X_DDSURFACEDESC);
    shared->texture_formats[0].dwFlags =
        V9X_DDSD_CAPS | V9X_DDSD_PIXELFORMAT;
    shared->texture_formats[0].ddpfPixelFormat.dwSize =
        sizeof(V9X_DDPIXELFORMAT);
    shared->texture_formats[0].ddpfPixelFormat.dwFlags =
        V9X_DDPF_RGB | V9X_DDPF_ALPHAPIXELS;
    shared->texture_formats[0].ddpfPixelFormat.dwRGBBitCount = 16ul;
    shared->texture_formats[0].ddpfPixelFormat.dwRBitMask = 0x00007c00ul;
    shared->texture_formats[0].ddpfPixelFormat.dwGBitMask = 0x000003e0ul;
    shared->texture_formats[0].ddpfPixelFormat.dwBBitMask = 0x0000001ful;
    shared->texture_formats[0].ddpfPixelFormat.dwRGBAlphaBitMask =
        0x00008000ul;
    shared->texture_formats[0].ddsCaps.dwCaps = V9X_DDSCAPS_TEXTURE;
#if V9X_C4_CAPS_VARIANT == 1
    shared->d3d_global.dwNumTextureFormats = 0ul;
    shared->d3d_global.lpTextureFormats = 0;
#else
    shared->d3d_global.dwNumTextureFormats = 1ul;
    shared->d3d_global.lpTextureFormats = &shared->texture_formats[0];
#endif

    shared->d3d_callbacks.dwSize = sizeof(V9X_D3DHAL_CALLBACKS);
    shared->d3d_callbacks.ContextCreate =
        (V9X_DD_CODE_PTR)V9xD3dContextCreate;
    shared->d3d_callbacks.ContextDestroy =
        (V9X_DD_CODE_PTR)V9xD3dContextDestroy;
    shared->d3d_callbacks.ContextDestroyAll =
        (V9X_DD_CODE_PTR)V9xD3dContextDestroyAll;
    shared->d3d_callbacks.Execute = 0;
    shared->d3d_callbacks.ExecuteClipped = 0;
    shared->d3d_callbacks.RenderState =
        (V9X_DD_CODE_PTR)V9xD3dRenderState;
    shared->d3d_callbacks.RenderPrimitive =
        (V9X_DD_CODE_PTR)V9xD3dRenderPrimitive;
    shared->d3d_callbacks.TextureCreate =
        (V9X_DD_CODE_PTR)V9xD3dTextureCreate;
    shared->d3d_callbacks.TextureDestroy =
        (V9X_DD_CODE_PTR)V9xD3dTextureDestroy;
    shared->d3d_callbacks.TextureSwap =
        (V9X_DD_CODE_PTR)V9xD3dTextureSwap;
    shared->d3d_callbacks.TextureGetSurf =
        (V9X_DD_CODE_PTR)V9xD3dTextureGetSurf;

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

    /* Trio64 shares the S3 scanout/vblank controls but not the ViRGE new-MMIO
     * or S3D engines. The chipset is not known here: DriverInit runs from
     * DDRAW's DDGET32BITDRIVERNAME escape, before the 16-bit side has
     * refreshed the engine descriptor. The 16-bit driver therefore owns the
     * per-chipset clamp and applies it immediately before DDHAL_SetInfo. */

    shared->hInstance = V9X_HAL_BASE;
    shared->driver_init_done = 1ul;
    v9x_trace_exit(V9X_TRACE_DRIVERINIT, 1ul);
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
