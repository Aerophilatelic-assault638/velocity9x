; Original stack-transparent forwarding thunks for the Windows 98 DIB Engine.
; The external import library supplies the DIB_* targets; no DDK object code is
; copied into this source file.

.286
.model compact
.code

V9X_FORWARD MACRO public_name, target_name
    PUBLIC public_name
    EXTRN target_name:FAR
public_name PROC FAR
    jmp target_name
public_name ENDP
ENDM

V9X_FORWARD BitBlt,              DIB_BitBlt
V9X_FORWARD ColorInfo,           DIB_ColorInfo
V9X_FORWARD Control,             DIB_Control
V9X_FORWARD EnumDFonts,          DIB_EnumDFonts
V9X_FORWARD EnumObj,             DIB_EnumObj
V9X_FORWARD Output,              DIB_Output
V9X_FORWARD Pixel,               DIB_Pixel
V9X_FORWARD RealizeObject,       DIB_RealizeObject
V9X_FORWARD StrBlt,              DIB_StrBlt
V9X_FORWARD ScanLR,              DIB_ScanLR
V9X_FORWARD DeviceMode,          DIB_DeviceMode
V9X_FORWARD ExtTextOut,          DIB_ExtTextOut
V9X_FORWARD GetCharWidth,        DIB_GetCharWidth
V9X_FORWARD DeviceBitmap,        DIB_DeviceBitmap
V9X_FORWARD FastBorder,          DIB_FastBorder
V9X_FORWARD SetAttribute,        DIB_SetAttribute
V9X_FORWARD DibBlt,              DIB_DibBlt
V9X_FORWARD CreateDIBitmap,      DIB_CreateDIBitmap
V9X_FORWARD DibToDevice,         DIB_DibToDevice
V9X_FORWARD SetPalette,          DIB_SetPalette
V9X_FORWARD GetPalette,          DIB_GetPalette
V9X_FORWARD SetPaletteTranslate, DIB_SetPaletteTranslate
V9X_FORWARD GetPaletteTranslate, DIB_GetPaletteTranslate
V9X_FORWARD UpdateColors,        DIB_UpdateColors
V9X_FORWARD StretchBlt,          DIB_StretchBlt
V9X_FORWARD StretchDIBits,       DIB_StretchDIBits
V9X_FORWARD SelectBitmap,        DIB_SelectBitmap
V9X_FORWARD BitmapBits,          DIB_BitmapBits
V9X_FORWARD Inquire,             DIB_Inquire

END
