#ifndef VELOCITY9X_WIN9X_DDRAW_ABI_H
#define VELOCITY9X_WIN9X_DDRAW_ABI_H

/*
 * Minimal Windows 9x DirectDraw HAL ABI used by Velocity9x, written from
 * the published Windows 98 DDK interface documentation (DDRAWI.H layouts).
 * The same header compiles in the 16-bit display driver (wcc) and the
 * 32-bit V9XHAL.DLL (wcc386). Every structure is packed to one byte so the
 * cross-bitness shared block has one layout.
 *
 * Pointer-width rule: fields DDRAW dereferences on the 16-bit side are
 * 16:16 far pointers, which are 4 bytes wide - the same width as the flat
 * 32-bit pointers the DDRAW32/HEL side uses. Both compilers therefore see
 * identical offsets; the 16-bit compilation uses FAR pointer types and the
 * 32-bit compilation uses flat types through V9X_DD_PTR/V9X_DD_CODE_PTR.
 */

#ifdef __386__
#define V9X_DD_PTR(type)        type *
#define V9X_DD_VOID_PTR         void *
typedef void *V9X_DD_CODE_PTR;
#else
#define V9X_DD_PTR(type)        type FAR *
#define V9X_DD_VOID_PTR         void FAR *
typedef void (FAR PASCAL *V9X_DD_CODE_PTR)();
#endif

/* Escape plumbing (values from the DDK DCI/DDRAWI contracts). */
#define V9X_QUERYESCSUPPORT               8u
#define V9X_DCICOMMAND                 3075u
#define V9X_DD_VERSION           0x00000200ul
#define V9X_DD_HAL_VERSION           0x0100u
#define V9X_DD_RUNTIME_VERSION   0x0000050aul

#define V9X_DDCREATEDRIVEROBJECT         10ul
#define V9X_DDGET32BITDRIVERNAME         11ul
#define V9X_DDNEWCALLBACKFNS             12ul
#define V9X_DDVERSIONINFO                13ul

/* Driver-side return conventions. */
#define V9X_DDHAL_DRIVER_NOTHANDLED  0x00000000ul
#define V9X_DDHAL_DRIVER_HANDLED     0x00000001ul
#define V9X_DD_OK                    0x00000000ul
#define V9X_DDERR_WASSTILLDRAWING    0x8876021cul

/* Caps and flag bits used by this driver (DDK values). */
#define V9X_DDCAPS_BLT               0x00000040ul
#define V9X_DDCAPS_GDI               0x00000400ul
#define V9X_DDCAPS_BLTCOLORFILL      0x04000000ul
#define V9X_DDSCAPS_OFFSCREENPLAIN   0x00000040ul
#define V9X_DDSCAPS_PRIMARYSURFACE   0x00000200ul
#define V9X_DDSCAPS_FLIP             0x00000010ul
#define V9X_DDSCAPS_VIDEOMEMORY      0x00004000ul
#define V9X_DDPF_RGB                 0x00000040ul
#define V9X_DDPF_PALETTEINDEXED8     0x00000020ul
#define V9X_VIDMEM_ISLINEAR          0x00000001ul
#define V9X_DDMODEINFO_PALETTIZED        0x0001u
#define V9X_DDHALINFO_ISPRIMARYDISPLAY 0x00000001ul

#define V9X_DDHAL_CB32_WAITFORVERTICALBLANK 0x00000010ul
#define V9X_DDHAL_CB32_FLIPTOGDISURFACE     0x00000200ul
#define V9X_DDHAL_SURFCB32_FLIP          0x00000002ul
#define V9X_DDHAL_SURFCB32_LOCK          0x00000008ul
#define V9X_DDHAL_SURFCB32_UNLOCK        0x00000010ul
#define V9X_DDHAL_SURFCB32_BLT           0x00000020ul
#define V9X_DDHAL_SURFCB32_GETBLTSTATUS  0x00000100ul
#define V9X_DDHAL_SURFCB32_GETFLIPSTATUS 0x00000200ul

#define V9X_DDFLIP_NOVSYNC           0x00000008ul
#define V9X_DDFLIP_DONOTWAIT         0x00000020ul
#define V9X_DDWAITVB_I_TESTVB        0x80000006ul
#define V9X_DDWAITVB_BLOCKBEGIN      0x00000001ul
#define V9X_DDWAITVB_BLOCKEND        0x00000004ul
#define V9X_DDGFS_CANFLIP            0x00000001ul
#define V9X_DDGFS_ISFLIPDONE         0x00000002ul
#define V9X_DDGBS_CANBLT             0x00000001ul
#define V9X_DDGBS_ISBLTDONE          0x00000002ul

#define V9X_DDBLT_ASYNC              0x00000200ul
#define V9X_DDBLT_COLORFILL          0x00000400ul
#define V9X_DDBLT_WAIT               0x01000000ul
#define V9X_DDBLT_DONOTWAIT          0x08000000ul
#define V9X_DDLOCK_WAIT              0x00000001ul
#define V9X_DDLOCK_DONOTWAIT         0x00004000ul

#pragma pack(push, 1)

/* DCI escape command block (DCIDDI.H layout). */
typedef struct v9x_dcicmd {
    DWORD dwCommand;
    DWORD dwParam1;
    DWORD dwParam2;
    DWORD dwVersion;
    DWORD dwReserved;
} V9X_DCICMD;

/* DDGET32BITDRIVERNAME output (DDRAWI.H DD32BITDRIVERDATA layout). */
typedef struct v9x_dd32bitdriverdata {
    char szName[260];
    char szEntryPoint[64];
    DWORD dwContext;
} V9X_DD32BITDRIVERDATA;

/* DDVERSIONINFO output (DDRAWI.H DDVERSIONDATA layout). */
typedef struct v9x_ddversiondata {
    DWORD dwHALVersion;
    DWORD dwReserved1;
    DWORD dwReserved2;
} V9X_DDVERSIONDATA;

/* DDRAW16 function table delivered by DDNEWCALLBACKFNS (DDHALDDRAWFNS). */
typedef struct v9x_ddhalddrawfns {
    DWORD dwSize;
    V9X_DD_CODE_PTR lpSetInfo;
    V9X_DD_CODE_PTR lpVidMemAlloc;
    V9X_DD_CODE_PTR lpVidMemFree;
} V9X_DDHALDDRAWFNS;

/* DDPIXELFORMAT (32 bytes). */
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

typedef struct v9x_ddcolorkey {
    DWORD dwColorSpaceLowValue;
    DWORD dwColorSpaceHighValue;
} V9X_DDCOLORKEY;

/* DDBLTFX (100 bytes). Pointer-valued union members are all DWORD-sized on
 * both sides of the Win9x DirectDraw boundary. */
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

/* VIDMEM heap descriptor (24 bytes). ddsCaps fields are restriction
 * masks: what the heap can NOT be used for. */
typedef struct v9x_vidmem {
    DWORD dwFlags;
    DWORD fpStart;
    DWORD fpEnd;
    DWORD ddsCaps;
    DWORD ddsCapsAlt;
    DWORD lpHeap;
} V9X_VIDMEM;

/* VIDMEMINFO (80 bytes at pack(1)). */
typedef struct v9x_vidmeminfo {
    DWORD fpPrimary;
    DWORD dwFlags;
    DWORD dwDisplayWidth;
    DWORD dwDisplayHeight;
    LONG lDisplayPitch;
    V9X_DDPIXELFORMAT ddpfDisplay;
    DWORD dwOffscreenAlign;
    DWORD dwOverlayAlign;
    DWORD dwTextureAlign;
    DWORD dwZBufferAlign;
    DWORD dwAlphaAlign;
    DWORD dwNumHeaps;
    V9X_DD_PTR(V9X_VIDMEM) pvmList;
} V9X_VIDMEMINFO;

/* DDHALMODEINFO (36 bytes). */
typedef struct v9x_ddhalmodeinfo {
    DWORD dwWidth;
    DWORD dwHeight;
    LONG lPitch;
    DWORD dwBPP;
    WORD wFlags;
    WORD wRefreshRate;
    DWORD dwRBitMask;
    DWORD dwGBitMask;
    DWORD dwBBitMask;
    DWORD dwAlphaBitMask;
} V9X_DDHALMODEINFO;

#define V9X_DD_ROP_SPACE 8

/* DDCORECAPS (312 bytes = 78 DWORDs). */
typedef struct v9x_ddcorecaps {
    DWORD dwSize;
    DWORD dwCaps;
    DWORD dwCaps2;
    DWORD dwCKeyCaps;
    DWORD dwFXCaps;
    DWORD dwFXAlphaCaps;
    DWORD dwPalCaps;
    DWORD dwSVCaps;
    DWORD dwAlphaBltConstBitDepths;
    DWORD dwAlphaBltPixelBitDepths;
    DWORD dwAlphaBltSurfaceBitDepths;
    DWORD dwAlphaOverlayConstBitDepths;
    DWORD dwAlphaOverlayPixelBitDepths;
    DWORD dwAlphaOverlaySurfaceBitDepths;
    DWORD dwZBufferBitDepths;
    DWORD dwVidMemTotal;
    DWORD dwVidMemFree;
    DWORD dwMaxVisibleOverlays;
    DWORD dwCurrVisibleOverlays;
    DWORD dwNumFourCCCodes;
    DWORD dwAlignBoundarySrc;
    DWORD dwAlignSizeSrc;
    DWORD dwAlignBoundaryDest;
    DWORD dwAlignSizeDest;
    DWORD dwAlignStrideAlign;
    DWORD dwRops[V9X_DD_ROP_SPACE];
    DWORD ddsCaps;
    DWORD dwMinOverlayStretch;
    DWORD dwMaxOverlayStretch;
    DWORD dwMinLiveVideoStretch;
    DWORD dwMaxLiveVideoStretch;
    DWORD dwMinHwCodecStretch;
    DWORD dwMaxHwCodecStretch;
    DWORD dwReserved1;
    DWORD dwReserved2;
    DWORD dwReserved3;
    DWORD dwSVBCaps;
    DWORD dwSVBCKeyCaps;
    DWORD dwSVBFXCaps;
    DWORD dwSVBRops[V9X_DD_ROP_SPACE];
    DWORD dwVSBCaps;
    DWORD dwVSBCKeyCaps;
    DWORD dwVSBFXCaps;
    DWORD dwVSBRops[V9X_DD_ROP_SPACE];
    DWORD dwSSBCaps;
    DWORD dwSSBCKeyCaps;
    DWORD dwSSBFXCaps;
    DWORD dwSSBRops[V9X_DD_ROP_SPACE];
    DWORD dwMaxVideoPorts;
    DWORD dwCurrVideoPorts;
    DWORD dwSVBCaps2;
} V9X_DDCORECAPS;

/* DIRECTDRAW object callbacks (48 bytes: 2 DWORDs + 10 pointers). */
typedef struct v9x_ddhal_ddcallbacks {
    DWORD dwSize;
    DWORD dwFlags;
    V9X_DD_CODE_PTR DestroyDriver;
    V9X_DD_CODE_PTR CreateSurface;
    V9X_DD_CODE_PTR SetColorKey;
    V9X_DD_CODE_PTR SetMode;
    V9X_DD_CODE_PTR WaitForVerticalBlank;
    V9X_DD_CODE_PTR CanCreateSurface;
    V9X_DD_CODE_PTR CreatePalette;
    V9X_DD_CODE_PTR GetScanLine;
    V9X_DD_CODE_PTR SetExclusiveMode;
    V9X_DD_CODE_PTR FlipToGDISurface;
} V9X_DDHAL_DDCALLBACKS;

/* DIRECTDRAWSURFACE object callbacks (68 bytes: 2 DWORDs + 15 pointers). */
typedef struct v9x_ddhal_ddsurfacecallbacks {
    DWORD dwSize;
    DWORD dwFlags;
    V9X_DD_CODE_PTR DestroySurface;
    V9X_DD_CODE_PTR Flip;
    V9X_DD_CODE_PTR SetClipList;
    V9X_DD_CODE_PTR Lock;
    V9X_DD_CODE_PTR Unlock;
    V9X_DD_CODE_PTR Blt;
    V9X_DD_CODE_PTR SetColorKey;
    V9X_DD_CODE_PTR AddAttachedSurface;
    V9X_DD_CODE_PTR GetBltStatus;
    V9X_DD_CODE_PTR GetFlipStatus;
    V9X_DD_CODE_PTR UpdateOverlay;
    V9X_DD_CODE_PTR SetOverlayPosition;
    V9X_DD_CODE_PTR reserved4;
    V9X_DD_CODE_PTR SetPalette;
} V9X_DDHAL_DDSURFACECALLBACKS;

/* DIRECTDRAWPALETTE object callbacks (16 bytes). */
typedef struct v9x_ddhal_ddpalettecallbacks {
    DWORD dwSize;
    DWORD dwFlags;
    V9X_DD_CODE_PTR DestroyPalette;
    V9X_DD_CODE_PTR SetEntries;
} V9X_DDHAL_DDPALETTECALLBACKS;

/* DDHALINFO (V2 layout, 456 bytes at pack(1)). */
typedef struct v9x_ddhalinfo {
    DWORD dwSize;
    V9X_DD_PTR(V9X_DDHAL_DDCALLBACKS) lpDDCallbacks;
    V9X_DD_PTR(V9X_DDHAL_DDSURFACECALLBACKS) lpDDSurfaceCallbacks;
    V9X_DD_PTR(V9X_DDHAL_DDPALETTECALLBACKS) lpDDPaletteCallbacks;
    V9X_VIDMEMINFO vmiData;
    V9X_DDCORECAPS ddCaps;
    DWORD dwMonitorFrequency;
    V9X_DD_CODE_PTR GetDriverInfo;
    DWORD dwModeIndex;
    V9X_DD_VOID_PTR lpdwFourCC;
    DWORD dwNumModes;
    V9X_DD_PTR(V9X_DDHALMODEINFO) lpModeInfo;
    DWORD dwFlags;
    V9X_DD_VOID_PTR lpPDevice;
    DWORD hInstance;
    DWORD lpD3DGlobalDriverData;
    DWORD lpD3DHALCallbacks;
    V9X_DD_VOID_PTR lpDDExeBufCallbacks;
} V9X_DDHALINFO;

#define V9X_DDHALINFO_SIZE 460ul

/*
 * 32-bit-side views of the runtime structures DDRAW passes to flat
 * callbacks. Only the fields the HAL reads are laid out; access is by
 * documented offset, so trailing fields are omitted.
 */
#ifdef __386__

/* DDRAWI_DDRAWSURFACE_GBL prefix: fpVidMem at +20, lPitch at +24. */
typedef struct v9x_dd_surface_gbl {
    DWORD dwRefCnt;
    DWORD dwGlobalFlags;
    DWORD dwBlockSizeY;
    DWORD dwBlockSizeX;
    DWORD lpDD;
    DWORD fpVidMem;
    LONG lPitch;
    WORD wHeight;
    WORD wWidth;
} V9X_DD_SURFACE_GBL;

/* DDRAWI_DDRAWSURFACE_LCL prefix: lpGbl at +4, ddsCaps at +32. */
typedef struct v9x_dd_surface_lcl {
    DWORD lpSurfMore;
    V9X_DD_SURFACE_GBL *lpGbl;
    DWORD hDDSurface;
    DWORD lpAttachList;
    DWORD lpAttachListFrom;
    DWORD dwLocalRefCnt;
    DWORD dwProcessId;
    DWORD dwFlags;
    DWORD ddsCaps;
} V9X_DD_SURFACE_LCL;

typedef struct v9x_ddhal_flipdata {
    DWORD lpDD;
    V9X_DD_SURFACE_LCL *lpSurfCurr;
    V9X_DD_SURFACE_LCL *lpSurfTarg;
    DWORD dwFlags;
    DWORD ddRVal;
    DWORD Flip;
} V9X_DDHAL_FLIPDATA;

typedef struct v9x_ddhal_getflipstatusdata {
    DWORD lpDD;
    V9X_DD_SURFACE_LCL *lpDDSurface;
    DWORD dwFlags;
    DWORD ddRVal;
    DWORD GetFlipStatus;
} V9X_DDHAL_GETFLIPSTATUSDATA;

typedef struct v9x_ddhal_lockdata {
    DWORD lpDD;
    V9X_DD_SURFACE_LCL *lpDDSurface;
    DWORD bHasRect;
    LONG rArea[4];
    DWORD lpSurfData;
    DWORD ddRVal;
    DWORD Lock;
    DWORD dwFlags;
} V9X_DDHAL_LOCKDATA;

typedef struct v9x_ddhal_unlockdata {
    DWORD lpDD;
    V9X_DD_SURFACE_LCL *lpDDSurface;
    DWORD ddRVal;
    DWORD Unlock;
} V9X_DDHAL_UNLOCKDATA;

typedef struct v9x_ddhal_bltdata {
    DWORD lpDD;
    V9X_DD_SURFACE_LCL *lpDDDestSurface;
    LONG rDest[4];
    V9X_DD_SURFACE_LCL *lpDDSrcSurface;
    LONG rSrc[4];
    DWORD dwFlags;
    DWORD dwROPFlags;
    V9X_DDBLTFX bltFX;
    DWORD ddRVal;
    DWORD Blt;
} V9X_DDHAL_BLTDATA;

typedef struct v9x_ddhal_getbltstatusdata {
    DWORD lpDD;
    V9X_DD_SURFACE_LCL *lpDDSurface;
    DWORD dwFlags;
    DWORD ddRVal;
    DWORD GetBltStatus;
} V9X_DDHAL_GETBLTSTATUSDATA;

typedef struct v9x_ddhal_waitforverticalblankdata {
    DWORD lpDD;
    DWORD dwFlags;
    DWORD bIsInVB;
    DWORD hEvent;
    DWORD ddRVal;
    DWORD WaitForVerticalBlank;
} V9X_DDHAL_WAITFORVERTICALBLANKDATA;

#else /* 16-bit */

/* DDHAL_DESTROYDRIVERDATA, consumed by the 16-bit DestroyDriver callback. */
typedef struct v9x_ddhal_destroydriverdata {
    V9X_DD_VOID_PTR lpDD;
    DWORD ddRVal;
    V9X_DD_CODE_PTR DestroyDriver;
} V9X_DDHAL_DESTROYDRIVERDATA;

#endif /* __386__ */

/*
 * Cross-bitness shared block. The 16-bit driver allocates it with DPMI in
 * globally visible memory; its linear address is the dwContext handed to
 * V9XHAL.DLL's DriverInit. The 32-bit side owns all content except the
 * framebuffer descriptor, which the 16-bit side refreshes on every enable.
 */
#define V9X_DD_SHARED_ABI   2026081101ul
#define V9X_DD_MODE_COUNT            6u

/* fb.flags */
#define V9X_DD_FB_VALID          0x00000001ul

typedef struct v9x_dd_framebuffer {
    DWORD linear_base;      /* flat address of the mapped LFB           */
    DWORD physical_base;    /* PCI aperture physical address            */
    DWORD vram_bytes;       /* mapped aperture size (4 MiB)             */
    DWORD visible_bytes;    /* pitch * height of the active mode        */
    DWORD pitch;
    DWORD width;
    DWORD height;
    DWORD bits_per_pixel;
    DWORD flags;            /* V9X_DD_FB_*                              */
} V9X_DD_FRAMEBUFFER;

/* The active S3 mapping spans the full 64-MiB linear aperture. Only the
 * first vram_bytes are allocatable VRAM; the register window is addressed
 * through control_linear_base and must never be exposed as a heap. */
#define V9X_DD_ENGINE_VALID          0x00000001ul
#define V9X_DD_ENGINE_S3_VIRGE_DX    0x00000002ul
#define V9X_DD_ENGINE_STATUS_VALIDATED 0x00000004ul

typedef struct v9x_dd_engine {
    DWORD control_linear_base;
    DWORD mapped_aperture_bytes;
    DWORD flags;
    DWORD fifo_timeouts;
    DWORD idle_timeouts;
    DWORD reset_count;
} V9X_DD_ENGINE;

typedef struct v9x_dd_cb32 {
    DWORD Flip;             /* flat function pointers filled by the DLL */
    DWORD GetFlipStatus;
    DWORD Lock;
    DWORD Unlock;
    DWORD WaitForVerticalBlank;
    DWORD flags;            /* extra DDHALINFO.dwFlags bits             */
} V9X_DD_CB32;

typedef struct v9x_dd_shared {
    DWORD dwSize;           /* sizeof(V9X_DD_SHARED)                    */
    DWORD abi;              /* V9X_DD_SHARED_ABI                        */
    DWORD driver_init_done; /* set by DriverInit after content build    */
    V9X_DD_FRAMEBUFFER fb;
    V9X_DD_ENGINE engine;
    V9X_DD_CB32 cb32;
    DWORD hInstance;        /* 32-bit DLL module handle                 */
    V9X_DDHALINFO info;
    V9X_DDHAL_DDCALLBACKS dd_callbacks;
    V9X_DDHAL_DDSURFACECALLBACKS surface_callbacks;
    V9X_DDHAL_DDPALETTECALLBACKS palette_callbacks;
    V9X_VIDMEM heaps[1];
    V9X_DDHALMODEINFO modes[V9X_DD_MODE_COUNT];
} V9X_DD_SHARED;

#pragma pack(pop)

/* One-byte-per-check size guards; both compilers must agree. */
typedef char v9x_dd_assert_pixelformat[
    sizeof(V9X_DDPIXELFORMAT) == 32 ? 1 : -1];
typedef char v9x_dd_assert_vidmem[sizeof(V9X_VIDMEM) == 24 ? 1 : -1];
typedef char v9x_dd_assert_bltfx[sizeof(V9X_DDBLTFX) == 100 ? 1 : -1];
typedef char v9x_dd_assert_vidmeminfo[sizeof(V9X_VIDMEMINFO) == 80 ? 1 : -1];
typedef char v9x_dd_assert_modeinfo[
    sizeof(V9X_DDHALMODEINFO) == 36 ? 1 : -1];
typedef char v9x_dd_assert_corecaps[
    sizeof(V9X_DDCORECAPS) == 316 ? 1 : -1];
typedef char v9x_dd_assert_halinfo[
    sizeof(V9X_DDHALINFO) == V9X_DDHALINFO_SIZE ? 1 : -1];
typedef char v9x_dd_assert_dcicmd[sizeof(V9X_DCICMD) == 20 ? 1 : -1];
typedef char v9x_dd_assert_dd32data[
    sizeof(V9X_DD32BITDRIVERDATA) == 328 ? 1 : -1];
#ifdef __386__
typedef char v9x_dd_assert_bltdata[
    sizeof(V9X_DDHAL_BLTDATA) == 160 ? 1 : -1];
#endif
typedef char v9x_dd_assert_shared_fits_dpmi_block[
    sizeof(V9X_DD_SHARED) <= 2048 ? 1 : -1];

#endif /* VELOCITY9X_WIN9X_DDRAW_ABI_H */
