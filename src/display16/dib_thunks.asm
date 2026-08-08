; Original stack-transparent forwarding thunks for the Windows 98 DIB Engine.
; The external import library supplies the DIB_* targets; no DDK object code is
; copied into this source file.

.286
.model compact
.386
.code

V9X_FORWARD MACRO public_name, target_name
    PUBLIC public_name
    EXTRN target_name:FAR
public_name PROC FAR
    jmp target_name
public_name ENDP
ENDM

; Some extended DIB Engine entry points require the screen PDevice appended
; immediately below the original far return address.
V9X_FORWARD_PDEVICE MACRO public_name, target_name
    PUBLIC public_name
    EXTRN target_name:FAR
public_name PROC FAR
    mov ax,DGROUP
    mov es,ax
    pop ecx
    push dword ptr es:_v9x_driver_pdevice
    push ecx
    jmp target_name
public_name ENDP
ENDM

; DIB_DibBltExt takes the current palettized-state word as its extra argument.
V9X_FORWARD_PALETTIZED MACRO public_name, target_name
    PUBLIC public_name
    EXTRN target_name:FAR
public_name PROC FAR
    mov ax,DGROUP
    mov es,ax
    pop ecx
    push word ptr es:_v9x_palettized
    push ecx
    jmp target_name
public_name ENDP
ENDM

EXTRN _v9x_driver_pdevice:DWORD
EXTRN _v9x_palettized:WORD

V9X_FORWARD BitBlt,                    DIB_BitBlt
V9X_FORWARD ColorInfo,                 DIB_ColorInfo
V9X_FORWARD Control,                   DIB_Control
V9X_FORWARD EnumDFonts,                DIB_EnumDFonts
V9X_FORWARD_PDEVICE EnumObj,           DIB_EnumObjExt
V9X_FORWARD Output,                    DIB_Output
V9X_FORWARD Pixel,                     DIB_Pixel
V9X_FORWARD_PDEVICE RealizeObject,     DIB_RealizeObjectExt
V9X_FORWARD StrBlt,                    DIB_StrBlt
V9X_FORWARD ScanLR,                    DIB_ScanLR
V9X_FORWARD DeviceMode,                DIB_DeviceMode
V9X_FORWARD ExtTextOut,                DIB_ExtTextOut
V9X_FORWARD GetCharWidth,              DIB_GetCharWidth
V9X_FORWARD DeviceBitmap,              DIB_DeviceBitmap
V9X_FORWARD FastBorder,                DIB_FastBorder
V9X_FORWARD SetAttribute,              DIB_SetAttribute
V9X_FORWARD_PALETTIZED DibBlt,         DIB_DibBltExt
V9X_FORWARD CreateDIBitmap,            DIB_CreateDIBitmap
V9X_FORWARD DibToDevice,               DIB_DibToDevice
V9X_FORWARD_PDEVICE GetPalette,        DIB_GetPaletteExt
V9X_FORWARD_PDEVICE SetPaletteTranslate, DIB_SetPaletteTranslateExt
V9X_FORWARD_PDEVICE GetPaletteTranslate, DIB_GetPaletteTranslateExt
V9X_FORWARD_PDEVICE UpdateColors,      DIB_UpdateColorsExt
V9X_FORWARD StretchBlt,                DIB_StretchBlt
V9X_FORWARD StretchDIBits,             DIB_StretchDIBits
V9X_FORWARD SelectBitmap,              DIB_SelectBitmap
V9X_FORWARD BitmapBits,                DIB_BitmapBits
V9X_FORWARD Inquire,                   DIB_Inquire
V9X_FORWARD_PDEVICE SetCursor,         DIB_SetCursorExt
V9X_FORWARD_PDEVICE MoveCursor,        DIB_MoveCursorExt

; DIBENG may poll CheckCursor while the display is disabled. Match the sample
; minidriver's guarded behavior instead of injecting a null PDevice.
PUBLIC CheckCursor
EXTRN DIB_CheckCursorExt:FAR
CheckCursor PROC FAR
    mov ax,DGROUP
    mov es,ax
    cmp dword ptr es:_v9x_driver_pdevice,0
    je short V9xCheckCursorDone
    pop ecx
    push dword ptr es:_v9x_driver_pdevice
    push ecx
    jmp DIB_CheckCursorExt
V9xCheckCursorDone:
    retf
CheckCursor ENDP

END
