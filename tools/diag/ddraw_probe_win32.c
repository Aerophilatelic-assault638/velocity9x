/*
 * DirectDraw presentation probe for the Velocity9x bring-up guest.
 *
 * Reproduces the exact presentation path used by fullscreen DirectDraw
 * applications (SetDisplayMode, flip-chain primary, Flip with DDFLIP_WAIT)
 * and records every HRESULT and timing to C:\V9XDD.INI so a host can
 * distinguish a mode-switch refusal, a vertical-blank wait, and raw
 * framebuffer write cost. ddraw.dll is loaded dynamically; the module keeps
 * the diagnostic-suite rule of runtime-free static imports.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifndef V9X_BUILD_ID
#define V9X_BUILD_ID "local"
#endif

#define V9X_RESULT_PATH "C:\\V9XDD.INI"
#define V9X_SECTION     "Velocity9xDDraw"

#define V9X_DDSD_CAPS               0x00000001ul
#define V9X_DDSD_HEIGHT             0x00000002ul
#define V9X_DDSD_WIDTH              0x00000004ul
#define V9X_DDSD_BACKBUFFERCOUNT    0x00000020ul
#define V9X_DDSCAPS_BACKBUFFER      0x00000004ul
#define V9X_DDSCAPS_COMPLEX         0x00000008ul
#define V9X_DDSCAPS_FLIP            0x00000010ul
#define V9X_DDSCAPS_OFFSCREENPLAIN  0x00000040ul
#define V9X_DDSCAPS_PRIMARYSURFACE  0x00000200ul
#define V9X_DDSCAPS_SYSTEMMEMORY    0x00000800ul
#define V9X_DDSCAPS_3DDEVICE        0x00002000ul
#define V9X_DDSCAPS_VIDEOMEMORY     0x00004000ul
#define V9X_DDSCL_FULLSCREEN        0x00000001ul
#define V9X_DDSCL_NORMAL            0x00000008ul
#define V9X_DDSCL_EXCLUSIVE         0x00000010ul
#define V9X_DDFLIP_WAIT             0x00000001ul
#define V9X_DDBLT_COLORFILL          0x00000400ul
#define V9X_DDBLT_WAIT               0x01000000ul
#define V9X_DDGBS_CANBLT              0x00000001ul
#define V9X_DDGBS_ISBLTDONE          0x00000002ul
#define V9X_DDWAITVB_BLOCKBEGIN     0x00000001ul
#define V9X_DDLOCK_WAIT             0x00000001ul
#define V9X_DDERR_WASSTILLDRAWING   0x8876021cul
#define V9X_D3DPT_TRIANGLELIST               4ul
#define V9X_D3DVT_TLVERTEX                   3ul

typedef struct v9x_ddscaps {
    DWORD dwCaps;
} V9X_DDSCAPS;

typedef struct v9x_ddcolorkey {
    DWORD dwColorSpaceLowValue;
    DWORD dwColorSpaceHighValue;
} V9X_DDCOLORKEY;

typedef struct v9x_ddpixelformat {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwFourCC;
    DWORD dwRGBBitCount;
    DWORD dwRBitMask;
    DWORD dwGBitMask;
    DWORD dwBBitMask;
    DWORD dwRGBAlphaBitMask;
} V9X_DDPIXELFORMAT;

typedef struct v9x_ddbltfx {
    DWORD dwSize;
    DWORD dwDDFX;
    DWORD dwROP;
    DWORD dwDDROP;
    DWORD dwRotationAngle;
    DWORD dwZBufferOpCode;
    DWORD dwZBufferLow;
    DWORD dwZBufferHigh;
    DWORD dwZBufferBaseDest;
    DWORD dwZDestConstBitDepth;
    DWORD dwZDestConst;
    DWORD dwZSrcConstBitDepth;
    DWORD dwZSrcConst;
    DWORD dwAlphaEdgeBlendBitDepth;
    DWORD dwAlphaEdgeBlend;
    DWORD dwReserved;
    DWORD dwAlphaDestConstBitDepth;
    DWORD dwAlphaDestConst;
    DWORD dwAlphaSrcConstBitDepth;
    DWORD dwAlphaSrcConst;
    DWORD dwFillColor;
    V9X_DDCOLORKEY ddckDestColorkey;
    V9X_DDCOLORKEY ddckSrcColorkey;
} V9X_DDBLTFX;

typedef struct v9x_ddsurfacedesc {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwHeight;
    DWORD dwWidth;
    LONG lPitch;
    DWORD dwBackBufferCount;
    DWORD dwMipMapCount;
    DWORD dwAlphaBitDepth;
    DWORD dwReserved;
    LPVOID lpSurface;
    V9X_DDCOLORKEY ddckCKDestOverlay;
    V9X_DDCOLORKEY ddckCKDestBlt;
    V9X_DDCOLORKEY ddckCKSrcOverlay;
    V9X_DDCOLORKEY ddckCKSrcBlt;
    V9X_DDPIXELFORMAT ddpfPixelFormat;
    V9X_DDSCAPS ddsCaps;
} V9X_DDSURFACEDESC;

struct v9x_dd;
struct v9x_dds;
struct v9x_d3d2;
struct v9x_d3d_device2;
struct v9x_d3d_viewport2;

typedef struct v9x_d3d_transform_caps {
    DWORD dwSize;
    DWORD dwCaps;
} V9X_D3D_TRANSFORM_CAPS;

typedef struct v9x_d3d_lighting_caps {
    DWORD dwSize;
    DWORD dwCaps;
    DWORD dwLightingModel;
    DWORD dwNumLights;
} V9X_D3D_LIGHTING_CAPS;

typedef struct v9x_d3d_prim_caps {
    DWORD values[14];
} V9X_D3D_PRIM_CAPS;

typedef struct v9x_d3d_device_desc {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dcmColorModel;
    DWORD dwDevCaps;
    V9X_D3D_TRANSFORM_CAPS dtcTransformCaps;
    DWORD bClipping;
    V9X_D3D_LIGHTING_CAPS dlcLightingCaps;
    V9X_D3D_PRIM_CAPS dpcLineCaps;
    V9X_D3D_PRIM_CAPS dpcTriCaps;
    DWORD dwDeviceRenderBitDepth;
    DWORD dwDeviceZBufferBitDepth;
    DWORD dwMaxBufferSize;
    DWORD dwMaxVertexCount;
    DWORD dx5Caps[8];
} V9X_D3D_DEVICE_DESC;

typedef HRESULT (__stdcall *V9X_D3D_ENUM_CALLBACK)(
    GUID *, char *, char *, V9X_D3D_DEVICE_DESC *,
    V9X_D3D_DEVICE_DESC *, void *);

typedef struct v9x_d3d2_vtbl {
    HRESULT (__stdcall *QueryInterface)(struct v9x_d3d2 *, const void *,
                                        void **);
    ULONG (__stdcall *AddRef)(struct v9x_d3d2 *);
    ULONG (__stdcall *Release)(struct v9x_d3d2 *);
    HRESULT (__stdcall *EnumDevices)(struct v9x_d3d2 *,
                                     V9X_D3D_ENUM_CALLBACK, void *);
    HRESULT (__stdcall *CreateLight)(struct v9x_d3d2 *, void **, void *);
    HRESULT (__stdcall *CreateMaterial)(struct v9x_d3d2 *, void **, void *);
    HRESULT (__stdcall *CreateViewport)(struct v9x_d3d2 *, void **, void *);
    HRESULT (__stdcall *FindDevice)(struct v9x_d3d2 *, void *, void *);
    HRESULT (__stdcall *CreateDevice)(struct v9x_d3d2 *, const GUID *,
                                      struct v9x_dds *,
                                      struct v9x_d3d_device2 **);
} V9X_D3D2_VTBL;

typedef struct v9x_d3d_device2_vtbl {
    HRESULT (__stdcall *QueryInterface)(struct v9x_d3d_device2 *,
                                        const void *, void **);
    ULONG (__stdcall *AddRef)(struct v9x_d3d_device2 *);
    ULONG (__stdcall *Release)(struct v9x_d3d_device2 *);
    void *GetCaps;
    void *SwapTextureHandles;
    void *GetStats;
    HRESULT (__stdcall *AddViewport)(struct v9x_d3d_device2 *,
                                     struct v9x_d3d_viewport2 *);
    HRESULT (__stdcall *DeleteViewport)(struct v9x_d3d_device2 *,
                                        struct v9x_d3d_viewport2 *);
    void *NextViewport;
    void *EnumTextureFormats;
    HRESULT (__stdcall *BeginScene)(struct v9x_d3d_device2 *);
    HRESULT (__stdcall *EndScene)(struct v9x_d3d_device2 *);
    void *GetDirect3D;
    HRESULT (__stdcall *SetCurrentViewport)(struct v9x_d3d_device2 *,
                                            struct v9x_d3d_viewport2 *);
    void *GetCurrentViewport;
    void *SetRenderTarget;
    void *GetRenderTarget;
    void *Begin;
    void *BeginIndexed;
    void *Vertex;
    void *Index;
    void *End;
    void *GetRenderState;
    void *SetRenderState;
    void *GetLightState;
    void *SetLightState;
    void *SetTransform;
    void *GetTransform;
    void *MultiplyTransform;
    HRESULT (__stdcall *DrawPrimitive)(struct v9x_d3d_device2 *, DWORD,
                                       DWORD, void *, DWORD, DWORD);
    void *DrawIndexedPrimitive;
    void *SetClipStatus;
    void *GetClipStatus;
} V9X_D3D_DEVICE2_VTBL;

typedef struct v9x_d3d_viewport2_vtbl {
    void *QueryInterface;
    void *AddRef;
    ULONG (__stdcall *Release)(struct v9x_d3d_viewport2 *);
    void *Initialize;
    void *GetViewport;
    void *SetViewport;
    void *TransformVertices;
    void *LightElements;
    void *SetBackground;
    void *GetBackground;
    void *SetBackgroundDepth;
    void *GetBackgroundDepth;
    void *Clear;
    void *AddLight;
    void *DeleteLight;
    void *NextLight;
    void *GetViewport2;
    HRESULT (__stdcall *SetViewport2)(struct v9x_d3d_viewport2 *, void *);
} V9X_D3D_VIEWPORT2_VTBL;

struct v9x_d3d_viewport2 {
    const V9X_D3D_VIEWPORT2_VTBL *vtbl;
};

typedef struct v9x_d3d_viewport_desc2 {
    DWORD dwSize;
    DWORD dwX;
    DWORD dwY;
    DWORD dwWidth;
    DWORD dwHeight;
    float dvClipX;
    float dvClipY;
    float dvClipWidth;
    float dvClipHeight;
    float dvMinZ;
    float dvMaxZ;
} V9X_D3D_VIEWPORT_DESC2;

typedef struct v9x_d3dtlvertex {
    float sx;
    float sy;
    float sz;
    float rhw;
    DWORD color;
    DWORD specular;
    float tu;
    float tv;
} V9X_D3DTLVERTEX;

struct v9x_d3d2 {
    const V9X_D3D2_VTBL *vtbl;
};

struct v9x_d3d_device2 {
    const V9X_D3D_DEVICE2_VTBL *vtbl;
};

static const GUID v9x_iid_d3d2 = {
    0x6aae1ec1ul, 0x662a, 0x11d0,
    { 0x88, 0x9d, 0x00, 0xaa, 0x00, 0xbb, 0xb7, 0x6a }
};

static const GUID v9x_iid_d3d_hal = {
    0x84e63de0ul, 0x46aa, 0x11cf,
    { 0x81, 0x6f, 0x00, 0x00, 0xc0, 0x20, 0x15, 0x6e }
};

typedef struct v9x_d3d_enum_result {
    DWORD hal_found;
    DWORD flags;
    DWORD render_depth;
} V9X_D3D_ENUM_RESULT;

/* IDirectDraw version 1 method table, in vtable order. */
typedef struct v9x_dd_vtbl {
    HRESULT (__stdcall *QueryInterface)(struct v9x_dd *, const void *,
                                        void **);
    ULONG (__stdcall *AddRef)(struct v9x_dd *);
    ULONG (__stdcall *Release)(struct v9x_dd *);
    HRESULT (__stdcall *Compact)(struct v9x_dd *);
    HRESULT (__stdcall *CreateClipper)(struct v9x_dd *, DWORD, void **,
                                       void *);
    HRESULT (__stdcall *CreatePalette)(struct v9x_dd *, DWORD, void *,
                                       void **, void *);
    HRESULT (__stdcall *CreateSurface)(struct v9x_dd *,
                                       V9X_DDSURFACEDESC *,
                                       struct v9x_dds **, void *);
    HRESULT (__stdcall *DuplicateSurface)(struct v9x_dd *, struct v9x_dds *,
                                          struct v9x_dds **);
    HRESULT (__stdcall *EnumDisplayModes)(struct v9x_dd *, DWORD,
                                          V9X_DDSURFACEDESC *, void *,
                                          void *);
    HRESULT (__stdcall *EnumSurfaces)(struct v9x_dd *, DWORD,
                                      V9X_DDSURFACEDESC *, void *, void *);
    HRESULT (__stdcall *FlipToGDISurface)(struct v9x_dd *);
    HRESULT (__stdcall *GetCaps)(struct v9x_dd *, void *, void *);
    HRESULT (__stdcall *GetDisplayMode)(struct v9x_dd *,
                                        V9X_DDSURFACEDESC *);
    HRESULT (__stdcall *GetFourCCCodes)(struct v9x_dd *, DWORD *, DWORD *);
    HRESULT (__stdcall *GetGDISurface)(struct v9x_dd *, struct v9x_dds **);
    HRESULT (__stdcall *GetMonitorFrequency)(struct v9x_dd *, DWORD *);
    HRESULT (__stdcall *GetScanLine)(struct v9x_dd *, DWORD *);
    HRESULT (__stdcall *GetVerticalBlankStatus)(struct v9x_dd *, BOOL *);
    HRESULT (__stdcall *Initialize)(struct v9x_dd *, void *);
    HRESULT (__stdcall *RestoreDisplayMode)(struct v9x_dd *);
    HRESULT (__stdcall *SetCooperativeLevel)(struct v9x_dd *, HWND, DWORD);
    HRESULT (__stdcall *SetDisplayMode)(struct v9x_dd *, DWORD, DWORD,
                                        DWORD);
    HRESULT (__stdcall *WaitForVerticalBlank)(struct v9x_dd *, DWORD,
                                              HANDLE);
} V9X_DD_VTBL;

/* IDirectDrawSurface version 1 method table, in vtable order. */
typedef struct v9x_dds_vtbl {
    HRESULT (__stdcall *QueryInterface)(struct v9x_dds *, const void *,
                                        void **);
    ULONG (__stdcall *AddRef)(struct v9x_dds *);
    ULONG (__stdcall *Release)(struct v9x_dds *);
    HRESULT (__stdcall *AddAttachedSurface)(struct v9x_dds *,
                                            struct v9x_dds *);
    HRESULT (__stdcall *AddOverlayDirtyRect)(struct v9x_dds *, RECT *);
    HRESULT (__stdcall *Blt)(struct v9x_dds *, RECT *, struct v9x_dds *,
                             RECT *, DWORD, void *);
    HRESULT (__stdcall *BltBatch)(struct v9x_dds *, void *, DWORD, DWORD);
    HRESULT (__stdcall *BltFast)(struct v9x_dds *, DWORD, DWORD,
                                 struct v9x_dds *, RECT *, DWORD);
    HRESULT (__stdcall *DeleteAttachedSurface)(struct v9x_dds *, DWORD,
                                               struct v9x_dds *);
    HRESULT (__stdcall *EnumAttachedSurfaces)(struct v9x_dds *, void *,
                                              void *);
    HRESULT (__stdcall *EnumOverlayZOrders)(struct v9x_dds *, DWORD, void *,
                                            void *);
    HRESULT (__stdcall *Flip)(struct v9x_dds *, struct v9x_dds *, DWORD);
    HRESULT (__stdcall *GetAttachedSurface)(struct v9x_dds *, V9X_DDSCAPS *,
                                            struct v9x_dds **);
    HRESULT (__stdcall *GetBltStatus)(struct v9x_dds *, DWORD);
    HRESULT (__stdcall *GetCaps)(struct v9x_dds *, V9X_DDSCAPS *);
    HRESULT (__stdcall *GetClipper)(struct v9x_dds *, void **);
    HRESULT (__stdcall *GetColorKey)(struct v9x_dds *, DWORD,
                                     V9X_DDCOLORKEY *);
    HRESULT (__stdcall *GetDC)(struct v9x_dds *, HDC *);
    HRESULT (__stdcall *GetFlipStatus)(struct v9x_dds *, DWORD);
    HRESULT (__stdcall *GetOverlayPosition)(struct v9x_dds *, LONG *,
                                            LONG *);
    HRESULT (__stdcall *GetPalette)(struct v9x_dds *, void **);
    HRESULT (__stdcall *GetPixelFormat)(struct v9x_dds *,
                                        V9X_DDPIXELFORMAT *);
    HRESULT (__stdcall *GetSurfaceDesc)(struct v9x_dds *,
                                        V9X_DDSURFACEDESC *);
    HRESULT (__stdcall *Initialize)(struct v9x_dds *, struct v9x_dd *,
                                    V9X_DDSURFACEDESC *);
    HRESULT (__stdcall *IsLost)(struct v9x_dds *);
    HRESULT (__stdcall *Lock)(struct v9x_dds *, RECT *,
                              V9X_DDSURFACEDESC *, DWORD, HANDLE);
    HRESULT (__stdcall *ReleaseDC)(struct v9x_dds *, HDC);
    HRESULT (__stdcall *Restore)(struct v9x_dds *);
    HRESULT (__stdcall *SetClipper)(struct v9x_dds *, void *);
    HRESULT (__stdcall *SetColorKey)(struct v9x_dds *, DWORD,
                                     V9X_DDCOLORKEY *);
    HRESULT (__stdcall *SetOverlayPosition)(struct v9x_dds *, LONG, LONG);
    HRESULT (__stdcall *SetPalette)(struct v9x_dds *, void *);
    HRESULT (__stdcall *Unlock)(struct v9x_dds *, void *);
    HRESULT (__stdcall *UpdateOverlay)(struct v9x_dds *, RECT *,
                                       struct v9x_dds *, RECT *, DWORD,
                                       void *);
    HRESULT (__stdcall *UpdateOverlayDisplay)(struct v9x_dds *, DWORD);
    HRESULT (__stdcall *UpdateOverlayZOrder)(struct v9x_dds *, DWORD,
                                             struct v9x_dds *);
} V9X_DDS_VTBL;

struct v9x_dd {
    const V9X_DD_VTBL *vtbl;
};

struct v9x_dds {
    const V9X_DDS_VTBL *vtbl;
};

typedef HRESULT (__stdcall *V9X_DDCREATE)(void *, struct v9x_dd **, void *);
typedef DWORD (__stdcall *V9X_TIMEGETTIME)(void);

static V9X_TIMEGETTIME v9x_time;

static int v9x_guid_equal(const GUID *left, const GUID *right)
{
    const BYTE *a = (const BYTE *)left;
    const BYTE *b = (const BYTE *)right;
    unsigned index;

    for (index = 0u; index < sizeof(GUID); ++index) {
        if (a[index] != b[index]) {
            return 0;
        }
    }
    return 1;
}

static HRESULT __stdcall v9x_enum_d3d_device(
    GUID *guid, char *description, char *name,
    V9X_D3D_DEVICE_DESC *hardware, V9X_D3D_DEVICE_DESC *software,
    void *context)
{
    V9X_D3D_ENUM_RESULT *result = (V9X_D3D_ENUM_RESULT *)context;

    (void)description;
    (void)name;
    (void)software;
    if (guid != 0 && hardware != 0 &&
        v9x_guid_equal(guid, &v9x_iid_d3d_hal)) {
        result->hal_found = 1ul;
        result->flags = hardware->dwFlags;
        result->render_depth = hardware->dwDeviceRenderBitDepth;
    }
    return 1l;
}

static int v9x_has_switch(const char *option)
{
    const char *command_line = GetCommandLineA();
    unsigned offset;
    unsigned index;

    for (offset = 0u; command_line[offset] != '\0'; ++offset) {
        for (index = 0u; option[index] != '\0'; ++index) {
            char left = command_line[offset + index];
            char right = option[index];

            if (left >= 'A' && left <= 'Z') {
                left = (char)(left + ('a' - 'A'));
            }
            if (left != right) {
                break;
            }
        }
        if (option[index] == '\0') {
            return 1;
        }
    }
    return 0;
}

static void v9x_zero(void *block, unsigned length)
{
    unsigned char *bytes = (unsigned char *)block;

    while (length-- != 0u) {
        *bytes++ = 0u;
    }
}

static void v9x_uint_text(char *text, DWORD value)
{
    char reverse[12];
    int count = 0;
    int index;

    do {
        reverse[count++] = (char)('0' + (value % 10ul));
        value /= 10ul;
    } while (value != 0ul);
    for (index = 0; index < count; ++index) {
        text[index] = reverse[count - index - 1];
    }
    text[count] = '\0';
}

static void v9x_hex_text(char *text, DWORD value)
{
    static const char digits[] = "0123456789ABCDEF";
    int index;

    text[0] = '0';
    text[1] = 'x';
    for (index = 0; index < 8; ++index) {
        text[2 + index] = digits[(value >> ((7 - index) * 4)) & 0xful];
    }
    text[10] = '\0';
}

static void v9x_write_text(const char *key, const char *value)
{
    WritePrivateProfileStringA(V9X_SECTION, key, value, V9X_RESULT_PATH);
}

static void v9x_write_uint(const char *key, DWORD value)
{
    char text[12];

    v9x_uint_text(text, value);
    v9x_write_text(key, text);
}

static void v9x_write_hresult(const char *key, HRESULT value)
{
    char text[11];

    v9x_hex_text(text, (DWORD)value);
    v9x_write_text(key, text);
}

static void v9x_write_mode(const char *prefix,
                           const V9X_DDSURFACEDESC *desc)
{
    char key[32];
    int offset = 0;
    int index;

    for (index = 0; prefix[index] != '\0'; ++index) {
        key[offset++] = prefix[index];
    }
    key[offset] = 'W';
    key[offset + 1] = '\0';
    v9x_write_uint(key, desc->dwWidth);
    key[offset] = 'H';
    v9x_write_uint(key, desc->dwHeight);
    key[offset] = 'B';
    key[offset + 1] = 'p';
    key[offset + 2] = 'p';
    key[offset + 3] = '\0';
    v9x_write_uint(key, desc->ddpfPixelFormat.dwRGBBitCount);
}

static void v9x_fill_surface(struct v9x_dds *surface, DWORD pattern)
{
    V9X_DDSURFACEDESC desc;
    HRESULT hr;
    DWORD FAR *pixels;
    DWORD count;

    v9x_zero(&desc, sizeof(desc));
    desc.dwSize = sizeof(desc);
    hr = surface->vtbl->Lock(surface, 0, &desc, V9X_DDLOCK_WAIT, 0);
    if (hr != 0) {
        return;
    }
    pixels = (DWORD *)desc.lpSurface;
    count = ((DWORD)desc.lPitch * desc.dwHeight) / 4ul;
    while (count-- != 0ul) {
        *pixels++ = pattern;
    }
    surface->vtbl->Unlock(surface, 0);
}

static DWORD v9x_time_surface_fill(struct v9x_dds *surface)
{
    DWORD started;

    v9x_fill_surface(surface, 0x18e318e3ul);
    started = v9x_time();
    v9x_fill_surface(surface, 0x07e007e0ul);
    return v9x_time() - started;
}

static HRESULT v9x_hardware_fill(struct v9x_dds *surface, DWORD color,
                                 DWORD *elapsed, HRESULT *done_result)
{
    V9X_DDBLTFX fx;
    HRESULT hr;
    DWORD started;

    v9x_zero(&fx, sizeof(fx));
    fx.dwSize = sizeof(fx);
    fx.dwFillColor = color;
    started = v9x_time();
    hr = surface->vtbl->Blt(surface, 0, 0, 0,
                            V9X_DDBLT_COLORFILL | V9X_DDBLT_WAIT, &fx);
    if (hr == 0) {
        do {
            *done_result = surface->vtbl->GetBltStatus(
                surface, V9X_DDGBS_ISBLTDONE);
        } while (*done_result == (HRESULT)V9X_DDERR_WASSTILLDRAWING &&
                 v9x_time() - started < 2000ul);
    } else {
        *done_result = hr;
    }
    *elapsed = v9x_time() - started;
    return hr;
}

static int v9x_surface_pixel16_equals(struct v9x_dds *surface,
                                      DWORD x, DWORD y, WORD expected)
{
    V9X_DDSURFACEDESC desc;
    BYTE FAR *row;
    WORD value;
    HRESULT hr;

    v9x_zero(&desc, sizeof(desc));
    desc.dwSize = sizeof(desc);
    hr = surface->vtbl->Lock(surface, 0, &desc, V9X_DDLOCK_WAIT, 0);
    if (hr != 0) {
        return 0;
    }
    if (desc.lpSurface == 0 ||
        desc.ddpfPixelFormat.dwRGBBitCount != 16ul ||
        x >= desc.dwWidth || y >= desc.dwHeight) {
        surface->vtbl->Unlock(surface, 0);
        return 0;
    }
    row = (BYTE FAR *)desc.lpSurface + y * (DWORD)desc.lPitch;
    value = *(WORD FAR *)(row + x * 2ul);
    surface->vtbl->Unlock(surface, 0);
    return value == expected;
}

static WORD v9x_surface_pixel16(struct v9x_dds *surface, DWORD x, DWORD y)
{
    V9X_DDSURFACEDESC desc;
    BYTE FAR *row;
    WORD value = 0xffffu;

    v9x_zero(&desc, sizeof(desc));
    desc.dwSize = sizeof(desc);
    if (surface->vtbl->Lock(surface, 0, &desc, V9X_DDLOCK_WAIT, 0) == 0) {
        if (desc.lpSurface != 0 && desc.ddpfPixelFormat.dwRGBBitCount == 16ul &&
            x < desc.dwWidth && y < desc.dwHeight) {
            row = (BYTE FAR *)desc.lpSurface + y * (DWORD)desc.lPitch;
            value = ((WORD FAR *)row)[x];
        }
        surface->vtbl->Unlock(surface, 0);
    }
    return value;
}

static LRESULT CALLBACK v9x_window_proc(HWND window, UINT message,
                                        WPARAM wparam, LPARAM lparam)
{
    return DefWindowProcA(window, message, wparam, lparam);
}

void __stdcall V9xDdrawProbeEntry(void)
{
    WNDCLASSA window_class;
    HWND window;
    HMODULE winmm;
    HMODULE ddraw_module;
    V9X_DDCREATE create;
    struct v9x_dd *ddraw = 0;
    struct v9x_dds *primary = 0;
    struct v9x_dds *backbuffer = 0;
    struct v9x_dds *stage = 0;
    struct v9x_dds *d3d_target = 0;
    struct v9x_d3d2 *d3d = 0;
    struct v9x_d3d_device2 *d3d_device = 0;
    struct v9x_d3d_viewport2 *d3d_viewport = 0;
    V9X_D3D_ENUM_RESULT d3d_result;
    V9X_DDSURFACEDESC desc;
    V9X_DDSCAPS caps;
    HRESULT hr;
    DWORD frequency = 0ul;
    DWORD started;
    DWORD elapsed;
    DWORD flip_total = 0ul;
    DWORD flip_max = 0ul;
    V9X_D3DTLVERTEX triangle[3];
    int index;

    WritePrivateProfileStringA(V9X_SECTION, 0, 0, V9X_RESULT_PATH);
    v9x_write_text("Build", V9X_BUILD_ID);
    v9x_write_text("Result", "INCOMPLETE");

    winmm = LoadLibraryA("WINMM.DLL");
    v9x_time = winmm != 0
        ? (V9X_TIMEGETTIME)GetProcAddress(winmm, "timeGetTime") : 0;
    ddraw_module = LoadLibraryA("DDRAW.DLL");
    create = ddraw_module != 0
        ? (V9X_DDCREATE)GetProcAddress(ddraw_module, "DirectDrawCreate")
        : 0;
    if (v9x_time == 0 || create == 0) {
        v9x_write_text("Result", "FAIL-LOAD");
        ExitProcess(1u);
    }

    v9x_zero(&window_class, sizeof(window_class));
    window_class.lpfnWndProc = v9x_window_proc;
    window_class.hInstance = GetModuleHandleA(0);
    window_class.lpszClassName = "Velocity9xDdrawProbeWindow";
    RegisterClassA(&window_class);
    window = CreateWindowExA(0ul, window_class.lpszClassName,
                             "Velocity9x DirectDraw probe", WS_POPUP,
                             0, 0, 64, 64, 0, 0, window_class.hInstance, 0);
    if (window == 0) {
        v9x_write_text("Result", "FAIL-WINDOW");
        ExitProcess(1u);
    }
    ShowWindow(window, SW_SHOWNORMAL);
    SetForegroundWindow(window);

    hr = create(0, &ddraw, 0);
    v9x_write_hresult("CreateHr", hr);
    if (hr != 0) {
        v9x_write_text("Result", "FAIL-CREATE");
        ExitProcess(1u);
    }

    v9x_zero(&d3d_result, sizeof(d3d_result));
    hr = ddraw->vtbl->QueryInterface(ddraw, &v9x_iid_d3d2,
                                     (void **)&d3d);
    v9x_write_hresult("D3DQueryHr", hr);
    if (hr == 0 && d3d != 0) {
        hr = d3d->vtbl->EnumDevices(d3d, v9x_enum_d3d_device,
                                    &d3d_result);
        v9x_write_hresult("D3DEnumHr", hr);
        v9x_write_uint("D3DHalFound", d3d_result.hal_found);
        v9x_write_uint("D3DHalFlags", d3d_result.flags);
        v9x_write_uint("D3DHalRenderDepth", d3d_result.render_depth);
    }

    /* Desktop mode and monitor frequency before any mode request. */
    v9x_zero(&desc, sizeof(desc));
    desc.dwSize = sizeof(desc);
    if (ddraw->vtbl->GetDisplayMode(ddraw, &desc) == 0) {
        v9x_write_mode("Desktop", &desc);
    }
    hr = ddraw->vtbl->GetMonitorFrequency(ddraw, &frequency);
    v9x_write_hresult("MonitorFreqHr", hr);
    v9x_write_uint("MonitorFreq", hr == 0 ? frequency : 0ul);

    /* Vertical-blank period from the desktop, no mode change involved. */
    hr = ddraw->vtbl->SetCooperativeLevel(ddraw, window, V9X_DDSCL_NORMAL);
    v9x_write_hresult("CoopNormalHr", hr);
    hr = ddraw->vtbl->WaitForVerticalBlank(ddraw, V9X_DDWAITVB_BLOCKBEGIN,
                                           0);
    v9x_write_hresult("VBlankHr", hr);
    if (hr == 0) {
        started = v9x_time();
        for (index = 0; index < 10; ++index) {
            ddraw->vtbl->WaitForVerticalBlank(ddraw,
                                              V9X_DDWAITVB_BLOCKBEGIN, 0);
        }
        v9x_write_uint("VBlank10Ms", v9x_time() - started);
    }

    /* The exact sequence a fullscreen game performs. */
    hr = ddraw->vtbl->SetCooperativeLevel(ddraw, window,
                                          V9X_DDSCL_EXCLUSIVE |
                                          V9X_DDSCL_FULLSCREEN);
    v9x_write_hresult("CoopExclusiveHr", hr);
    hr = ddraw->vtbl->SetDisplayMode(ddraw, 640ul, 480ul, 16ul);
    v9x_write_hresult("SetModeHr", hr);
    v9x_zero(&desc, sizeof(desc));
    desc.dwSize = sizeof(desc);
    if (ddraw->vtbl->GetDisplayMode(ddraw, &desc) == 0) {
        v9x_write_mode("AfterMode", &desc);
    }

    v9x_zero(&desc, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = V9X_DDSD_CAPS | V9X_DDSD_BACKBUFFERCOUNT;
    desc.ddsCaps.dwCaps = V9X_DDSCAPS_PRIMARYSURFACE | V9X_DDSCAPS_FLIP |
                          V9X_DDSCAPS_COMPLEX;
    desc.dwBackBufferCount = 1ul;
    hr = ddraw->vtbl->CreateSurface(ddraw, &desc, &primary, 0);
    v9x_write_hresult("PrimaryHr", hr);
    if (hr == 0) {
        v9x_zero(&desc, sizeof(desc));
        desc.dwSize = sizeof(desc);
        if (primary->vtbl->GetSurfaceDesc(primary, &desc) == 0) {
            v9x_write_mode("Primary", &desc);
            v9x_write_uint("PrimaryPitch", (DWORD)desc.lPitch);
        }
        caps.dwCaps = V9X_DDSCAPS_BACKBUFFER;
        hr = primary->vtbl->GetAttachedSurface(primary, &caps, &backbuffer);
        v9x_write_hresult("BackbufferHr", hr);
    }

    if (d3d != 0 && d3d_result.hal_found != 0ul) {
        v9x_zero(&desc, sizeof(desc));
        desc.dwSize = sizeof(desc);
        desc.dwFlags = V9X_DDSD_CAPS | V9X_DDSD_WIDTH | V9X_DDSD_HEIGHT;
        desc.dwWidth = 64ul;
        desc.dwHeight = 64ul;
        desc.ddsCaps.dwCaps = V9X_DDSCAPS_3DDEVICE |
                              V9X_DDSCAPS_OFFSCREENPLAIN |
                              V9X_DDSCAPS_VIDEOMEMORY;
        hr = ddraw->vtbl->CreateSurface(ddraw, &desc, &d3d_target, 0);
        v9x_write_hresult("D3DTargetHr", hr);
        if (hr == 0 && d3d_target != 0) {
            hr = d3d->vtbl->CreateDevice(d3d, &v9x_iid_d3d_hal,
                                         d3d_target, &d3d_device);
            v9x_write_hresult("D3DCreateDeviceHr", hr);
            if (hr == 0 && d3d_device != 0) {
                HRESULT begin_hr;
                HRESULT draw_hr;
                HRESULT end_hr;
                HRESULT viewport_hr;
                V9X_D3D_VIEWPORT_DESC2 viewport_desc;

                viewport_hr = d3d->vtbl->CreateViewport(
                    d3d, (void **)&d3d_viewport, 0);
                v9x_write_hresult("D3DCreateViewportHr", viewport_hr);
                if (viewport_hr == 0 && d3d_viewport != 0) {
                    viewport_hr = d3d_device->vtbl->AddViewport(
                        d3d_device, d3d_viewport);
                }
                v9x_write_hresult("D3DAddViewportHr", viewport_hr);
                if (viewport_hr == 0) {
                    v9x_zero(&viewport_desc, sizeof(viewport_desc));
                    viewport_desc.dwSize = sizeof(viewport_desc);
                    viewport_desc.dwWidth = 64ul;
                    viewport_desc.dwHeight = 64ul;
                    viewport_desc.dvClipX = -1.0f;
                    viewport_desc.dvClipY = 1.0f;
                    viewport_desc.dvClipWidth = 2.0f;
                    viewport_desc.dvClipHeight = 2.0f;
                    viewport_desc.dvMinZ = 0.0f;
                    viewport_desc.dvMaxZ = 1.0f;
                    viewport_hr = d3d_viewport->vtbl->SetViewport2(
                        d3d_viewport, &viewport_desc);
                }
                v9x_write_hresult("D3DSetViewportHr", viewport_hr);
                if (viewport_hr == 0) {
                    viewport_hr = d3d_device->vtbl->SetCurrentViewport(
                        d3d_device, d3d_viewport);
                }
                v9x_write_hresult("D3DCurrentViewportHr", viewport_hr);

                v9x_fill_surface(d3d_target, 0ul);
                triangle[0].sx = 8.0f;
                triangle[0].sy = 8.0f;
                triangle[0].sz = 0.0f;
                triangle[0].rhw = 1.0f;
                triangle[0].color = 0xffff0000ul;
                triangle[0].specular = 0ul;
                triangle[0].tu = 0.0f;
                triangle[0].tv = 0.0f;
                triangle[1] = triangle[0];
                triangle[1].sx = 56.0f;
                triangle[2] = triangle[0];
                triangle[2].sy = 56.0f;

                begin_hr = viewport_hr == 0
                    ? d3d_device->vtbl->BeginScene(d3d_device) : viewport_hr;
                v9x_write_hresult("D3DBeginSceneHr", begin_hr);
                if (begin_hr == 0) {
                    draw_hr = d3d_device->vtbl->DrawPrimitive(
                        d3d_device, V9X_D3DPT_TRIANGLELIST,
                        V9X_D3DVT_TLVERTEX, triangle, 3ul, 0ul);
                    v9x_write_hresult("D3DDrawPrimitiveHr", draw_hr);
                    end_hr = d3d_device->vtbl->EndScene(d3d_device);
                } else {
                    draw_hr = begin_hr;
                    end_hr = begin_hr;
                }
                v9x_write_hresult("D3DEndSceneHr", end_hr);
                v9x_write_uint("D3DTrianglePixelRaw",
                               v9x_surface_pixel16(d3d_target, 16ul, 16ul));
                v9x_write_uint("D3DTrianglePixelOk",
                    draw_hr == 0 && end_hr == 0 &&
                    v9x_surface_pixel16_equals(d3d_target, 16ul, 16ul,
                                               0x7c00u) ? 1ul : 0ul);
                if (d3d_viewport != 0) {
                    d3d_device->vtbl->DeleteViewport(d3d_device,
                                                     d3d_viewport);
                    d3d_viewport->vtbl->Release(d3d_viewport);
                    d3d_viewport = 0;
                }
                d3d_device->vtbl->Release(d3d_device);
                d3d_device = 0;
                v9x_write_uint("D3DContextCycleOk", 1ul);
            } else {
                v9x_write_uint("D3DContextCycleOk", 0ul);
            }
        }
    }

    if (backbuffer != 0) {
        HRESULT fill_done;
        HRESULT fill_can;

        started = 0ul;
        fill_can = backbuffer->vtbl->GetBltStatus(backbuffer,
                                                  V9X_DDGBS_CANBLT);
        v9x_write_hresult("BltCanHr", fill_can);
        if (v9x_has_switch("/status-only")) {
            if (d3d_target != 0) {
                d3d_target->vtbl->Release(d3d_target);
            }
            if (d3d != 0) {
                d3d->vtbl->Release(d3d);
            }
            backbuffer->vtbl->Release(backbuffer);
            primary->vtbl->Release(primary);
            ddraw->vtbl->RestoreDisplayMode(ddraw);
            ddraw->vtbl->SetCooperativeLevel(ddraw, window,
                                              V9X_DDSCL_NORMAL);
            ddraw->vtbl->Release(ddraw);
            DestroyWindow(window);
            v9x_write_text("Result", "STATUS-ONLY");
            ExitProcess(fill_can == 0 ? 0u : 2u);
        }
        if (fill_can == 0) {
            hr = v9x_hardware_fill(backbuffer, 0x000007e0ul, &started,
                                   &fill_done);
        } else {
            hr = fill_can;
            fill_done = fill_can;
        }
        v9x_write_hresult("BltFillHr", hr);
        v9x_write_hresult("BltFillDoneHr", fill_done);
        v9x_write_uint("BltFillMs", started);
        v9x_write_uint("BltFillPixelOk",
                       hr == 0 && fill_done == 0 &&
                       v9x_surface_pixel16_equals(backbuffer, 100ul, 100ul,
                                                  0x07e0u) ? 1ul : 0ul);

        /* Raw surface write cost, then the flip itself. */
        v9x_write_uint("BackFillMs", v9x_time_surface_fill(backbuffer));
        v9x_write_uint("PrimaryFillMs", v9x_time_surface_fill(primary));

        do {
            hr = primary->vtbl->Flip(primary, 0, V9X_DDFLIP_WAIT);
        } while (hr == (HRESULT)V9X_DDERR_WASSTILLDRAWING);
        v9x_write_hresult("FlipHr", hr);
        if (hr == 0) {
            started = v9x_time();
            for (index = 0; index < 20; ++index) {
                DWORD flip_started = v9x_time();

                do {
                    hr = primary->vtbl->Flip(primary, 0, V9X_DDFLIP_WAIT);
                } while (hr == (HRESULT)V9X_DDERR_WASSTILLDRAWING);
                elapsed = v9x_time() - flip_started;
                if (elapsed > flip_max) {
                    flip_max = elapsed;
                }
            }
            flip_total = v9x_time() - started;
            v9x_write_uint("Flip20Ms", flip_total);
            v9x_write_uint("FlipMaxMs", flip_max);

            /* Flip correctness: fill the backbuffer with a known color,
             * flip, and read the visible screen back through GDI. Two
             * rounds with different colors catch a flip that never moves
             * the display as well as one stuck on a single page. */
            {
                HDC screen;
                COLORREF seen_red;
                COLORREF seen_blue;
                int pixel_ok;
                /* /hold pauses on each verification color so the emulated
                 * scanout can be captured from the host: GDI readback only
                 * sees the fixed GDI page once real flips are in play. */
                int hold = v9x_has_switch("/hold");

                v9x_fill_surface(backbuffer, 0xf800f800ul);
                do {
                    hr = primary->vtbl->Flip(primary, 0, V9X_DDFLIP_WAIT);
                } while (hr == (HRESULT)V9X_DDERR_WASSTILLDRAWING);
                if (hold) {
                    Sleep(5000);
                }
                screen = GetDC(0);
                seen_red = GetPixel(screen, 100, 100);
                ReleaseDC(0, screen);

                v9x_fill_surface(backbuffer, 0x001f001ful);
                do {
                    hr = primary->vtbl->Flip(primary, 0, V9X_DDFLIP_WAIT);
                } while (hr == (HRESULT)V9X_DDERR_WASSTILLDRAWING);
                if (hold) {
                    Sleep(5000);
                }
                screen = GetDC(0);
                seen_blue = GetPixel(screen, 100, 100);
                ReleaseDC(0, screen);

                pixel_ok = seen_red != CLR_INVALID &&
                           seen_blue != CLR_INVALID &&
                           GetRValue(seen_red) > 0xc0u &&
                           GetGValue(seen_red) < 0x40u &&
                           GetBValue(seen_red) < 0x40u &&
                           GetBValue(seen_blue) > 0xc0u &&
                           GetRValue(seen_blue) < 0x40u &&
                           GetGValue(seen_blue) < 0x40u;
                v9x_write_uint("FlipPixelOk", pixel_ok ? 1u : 0u);
            }
        }
    }

    /* Stage-surface availability, mirroring the game's fallback ladder. */
    v9x_zero(&desc, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = V9X_DDSD_CAPS | V9X_DDSD_WIDTH | V9X_DDSD_HEIGHT;
    desc.dwWidth = 640ul;
    desc.dwHeight = 480ul;
    desc.ddsCaps.dwCaps = V9X_DDSCAPS_OFFSCREENPLAIN |
                          V9X_DDSCAPS_VIDEOMEMORY;
    hr = ddraw->vtbl->CreateSurface(ddraw, &desc, &stage, 0);
    v9x_write_hresult("VideoStageHr", hr);
    if (hr == 0 && stage != 0) {
        stage->vtbl->Release(stage);
        stage = 0;
    }
    desc.ddsCaps.dwCaps = V9X_DDSCAPS_OFFSCREENPLAIN |
                          V9X_DDSCAPS_SYSTEMMEMORY;
    hr = ddraw->vtbl->CreateSurface(ddraw, &desc, &stage, 0);
    v9x_write_hresult("SystemStageHr", hr);
    if (hr == 0 && stage != 0) {
        stage->vtbl->Release(stage);
        stage = 0;
    }

    if (backbuffer != 0) {
        backbuffer->vtbl->Release(backbuffer);
    }
    if (primary != 0) {
        primary->vtbl->Release(primary);
    }
    if (d3d_target != 0) {
        d3d_target->vtbl->Release(d3d_target);
    }
    if (d3d != 0) {
        d3d->vtbl->Release(d3d);
    }
    hr = ddraw->vtbl->RestoreDisplayMode(ddraw);
    v9x_write_hresult("RestoreHr", hr);
    ddraw->vtbl->SetCooperativeLevel(ddraw, window, V9X_DDSCL_NORMAL);
    ddraw->vtbl->Release(ddraw);
    DestroyWindow(window);
    v9x_write_text("Result", "COMPLETE");
    WritePrivateProfileStringA(0, 0, 0, V9X_RESULT_PATH);
    ExitProcess(0u);
}
