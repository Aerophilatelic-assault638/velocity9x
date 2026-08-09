; Original Velocity9x Win16 DIB Engine and framebuffer runtime glue.
;
; The DIB Engine names are supplied by the external Windows 98 DDK import
; library. Hardware bring-up is restricted to an audited VBE mode table and a
; validated,
; 64-KiB-aligned S3 linear aperture read from CR59/CR5A.

.model compact
.386p

.data
EXTRN _v9x_active_vbe_mode:WORD
EXTRN _v9x_active_visible_bytes:DWORD
V9xScreenSelector dw 0
V9xLinearAddress  dd 0
V9xPhysicalBase   dd 0
V9xEnableResult   dw 0
V9xVddEntryPoint dd 0
V9xVmHandle       dw 0
V9xVddRegistered dw 0
V9xHardwareStageCode dw 0
V9xCreateDibReturn dd 0

.code

EXTRN DIB_Enable:FAR
EXTRN CreateDIBPDevice:FAR
EXTRN DIB_BeginAccess:FAR
EXTRN DIB_EndAccess:FAR
EXTRN DIB_SetPaletteExt:FAR
EXTRN DIB_SetPaletteTranslateExt:FAR
EXTRN RESETHIRESMODE:FAR

VDD_DEVICE_ID          EQU 000ah
VDD_DRIVER_REGISTER    EQU 0080h
VDD_DRIVER_UNREGISTER  EQU 0081h
VDD_SAVE_DRIVER_STATE  EQU 0082h
VDD_GET_DISPLAY_CONFIG EQU 0085h
VDD_PRE_MODE_CHANGE    EQU 0086h
VDD_POST_MODE_CHANGE   EQU 0087h
STOP_IO_TRAP           EQU 4000h
START_IO_TRAP          EQU 4007h

PUBLIC V9XDIBENABLECALL
V9XDIBENABLECALL PROC FAR
    jmp DIB_Enable
V9XDIBENABLECALL ENDP

PUBLIC V9XCREATEDIBPDEVICECALL
V9XCREATEDIBPDEVICECALL PROC FAR
    ; CreateDIBPDevice is an unusual Win16 API: DIBENG returns its DWORD in
    ; EAX, while Open Watcom's 16-bit C ABI expects DWORD results in DX:AX.
    ; Convert explicitly; a transparent jump can turn a valid selector:0
    ; result into a false zero depending on the stale value in DX.
    ; Remove this wrapper's far return address so DIBENG sees the original
    ; Pascal argument frame immediately below its own return address.
    pop  dword ptr V9xCreateDibReturn
    call CreateDIBPDevice
    push dword ptr V9xCreateDibReturn
    mov  dx, ax
    shr  eax, 16
    xchg ax, dx
    retf
V9XCREATEDIBPDEVICECALL ENDP

PUBLIC V9XDIBBEGINACCESS
V9XDIBBEGINACCESS PROC FAR
    jmp DIB_BeginAccess
V9XDIBBEGINACCESS ENDP

PUBLIC V9XDIBENDACCESS
V9XDIBENDACCESS PROC FAR
    jmp DIB_EndAccess
V9XDIBENDACCESS ENDP

PUBLIC V9XDIBSETPALETTECALL
V9XDIBSETPALETTECALL PROC FAR
    jmp DIB_SetPaletteExt
V9XDIBSETPALETTECALL ENDP

PUBLIC V9XDIBSETPALETTETRANSLATECALL
V9XDIBSETPALETTETRANSLATECALL PROC FAR
    jmp DIB_SetPaletteTranslateExt
V9XDIBSETPALETTETRANSLATECALL ENDP

V9xVddInitialize PROC NEAR
    cmp     V9xVddEntryPoint, 0
    jne     short V9xVddInitializeReady

    mov     ax, 1684h
    mov     bx, VDD_DEVICE_ID
    int     2fh
    mov     word ptr V9xVddEntryPoint, di
    mov     word ptr V9xVddEntryPoint+2, es
    mov     ax, es
    or      ax, di
    jz      short V9xVddInitializeFailed

    mov     ax, 1683h
    int     2fh
    mov     V9xVmHandle, bx

V9xVddInitializeReady:
    mov     ax, 1
    ret
V9xVddInitializeFailed:
    mov     V9xVddEntryPoint, 0
    xor     ax, ax
    ret
V9xVddInitialize ENDP

PUBLIC V9XVDDGETDISPLAYCONFIG
V9XVDDGETDISPLAYCONFIG PROC FAR
    push    bp
    mov     bp, sp
    push    bx
    push    cx
    push    dx
    push    esi
    push    edi
    push    es

    call    V9xVddInitialize
    or      ax, ax
    jz      short V9xVddGetConfigFailed
    movzx   edi, word ptr [bp+6]
    mov     es, word ptr [bp+8]
    mov     eax, VDD_GET_DISPLAY_CONFIG
    movzx   ebx, V9xVmHandle
    mov     ecx, 34
    xor     edx, edx
    call    dword ptr V9xVddEntryPoint
    cmp     eax, VDD_GET_DISPLAY_CONFIG
    je      short V9xVddGetConfigFailed
    inc     eax
    jz      short V9xVddGetConfigFailed
    mov     ax, 1
    jmp     short V9xVddGetConfigDone
V9xVddGetConfigFailed:
    xor     ax, ax
V9xVddGetConfigDone:
    pop     es
    pop     edi
    pop     esi
    pop     dx
    pop     cx
    pop     bx
    pop     bp
    retf    4
V9XVDDGETDISPLAYCONFIG ENDP

PUBLIC V9XVDDPREMODE
V9XVDDPREMODE PROC FAR
    push    bx
    push    cx
    push    dx
    push    si
    push    di
    push    es

    call    V9xVddInitialize
    or      ax, ax
    jz      short V9xVddPreModeFailed
    mov     eax, VDD_PRE_MODE_CHANGE
    movzx   ebx, V9xVmHandle
    call    dword ptr V9xVddEntryPoint
    mov     ax, 1
    jmp     short V9xVddPreModeDone
V9xVddPreModeFailed:
    xor     ax, ax
V9xVddPreModeDone:
    pop     es
    pop     di
    pop     si
    pop     dx
    pop     cx
    pop     bx
    retf
V9XVDDPREMODE ENDP

PUBLIC V9XVDDREGISTER
V9XVDDREGISTER PROC FAR
    push    bx
    push    cx
    push    dx
    push    di
    push    es

    cmp     V9xVddRegistered, 0
    jne     short V9xVddRegisterReady
    call    V9xVddInitialize
    or      ax, ax
    jz      short V9xVddRegisterFailed

    mov     ax, STOP_IO_TRAP
    int     2fh

    mov     eax, VDD_DRIVER_REGISTER
    movzx   ebx, V9xVmHandle
    mov     ecx, _v9x_active_visible_bytes
    mov     ax, SEG RESETHIRESMODE
    mov     es, ax
    mov     di, OFFSET RESETHIRESMODE
    xor     edx, edx
    call    dword ptr V9xVddEntryPoint
    cmp     eax, VDD_DRIVER_REGISTER
    je      short V9xVddRegisterRestartTrap

    mov     V9xVddRegistered, 1
    mov     eax, VDD_POST_MODE_CHANGE
    movzx   ebx, V9xVmHandle
    call    dword ptr V9xVddEntryPoint
    mov     eax, VDD_SAVE_DRIVER_STATE
    movzx   ebx, V9xVmHandle
    call    dword ptr V9xVddEntryPoint

V9xVddRegisterReady:
    mov     ax, 1
    jmp     short V9xVddRegisterDone

V9xVddRegisterRestartTrap:
    mov     ax, START_IO_TRAP
    int     2fh
V9xVddRegisterFailed:
    xor     ax, ax
V9xVddRegisterDone:
    pop     es
    pop     di
    pop     dx
    pop     cx
    pop     bx
    retf
V9XVDDREGISTER ENDP

PUBLIC V9XVDDPOSTMODE
V9XVDDPOSTMODE PROC FAR
    push    bx
    cmp     V9xVddRegistered, 0
    je      short V9xVddPostModeDone
    mov     eax, VDD_POST_MODE_CHANGE
    movzx   ebx, V9xVmHandle
    call    dword ptr V9xVddEntryPoint
    mov     eax, VDD_SAVE_DRIVER_STATE
    movzx   ebx, V9xVmHandle
    call    dword ptr V9xVddEntryPoint
V9xVddPostModeDone:
    pop     bx
    retf
V9XVDDPOSTMODE ENDP

PUBLIC V9XVDDUNREGISTER
V9XVDDUNREGISTER PROC FAR
    push    bx
    cmp     V9xVddRegistered, 0
    je      short V9xVddUnregisterDone
    mov     ax, START_IO_TRAP
    int     2fh
    mov     eax, VDD_DRIVER_UNREGISTER
    movzx   ebx, V9xVmHandle
    call    dword ptr V9xVddEntryPoint
    mov     V9xVddRegistered, 0
V9xVddUnregisterDone:
    pop     bx
    retf
V9XVDDUNREGISTER ENDP

V9xSetVbeMode PROC NEAR
    mov     ax, 4f02h
    mov     bx, _v9x_active_vbe_mode
    ; The Windows 98 S3 ViRGE sample uses the S3/VBE no-clear flag for these
    ; modes. It only requests the generic VBE linear-framebuffer bit on GX2,
    ; not on the 86C375 ViRGE/DX targeted here.
    or      bx, 8000h
    int     10h
    cmp     ax, 004fh
    jne     short V9xSetVbeModeFailed
    mov     ax, 1
    ret
V9xSetVbeModeFailed:
    xor     ax, ax
    ret
V9xSetVbeMode ENDP

V9xFindPciDevice PROC NEAR
    mov     ax, 0b102h
    mov     cx, 08a01h
    mov     dx, 05333h
    xor     si, si
    int     1ah
    jc      short V9xFindPciDeviceFailed
    or      ah, ah
    jne     short V9xFindPciDeviceFailed
    mov     ax, 1
    ret
V9xFindPciDeviceFailed:
    xor     ax, ax
    ret
V9xFindPciDevice ENDP

PUBLIC V9XHARDWAREPRESENT
V9XHARDWAREPRESENT PROC FAR
    push    bx
    push    cx
    push    dx
    push    si
    call    V9xFindPciDevice
    pop     si
    pop     dx
    pop     cx
    pop     bx
    retf
V9XHARDWAREPRESENT ENDP

V9xReadS3Aperture PROC NEAR
    mov     dx, 03d4h
    mov     ax, 4838h
    out     dx, ax
    mov     ax, 0a039h
    out     dx, ax

    mov     al, 59h
    out     dx, al
    inc     dx
    in      al, dx
    mov     bh, al
    dec     dx
    mov     al, 5ah
    out     dx, al
    inc     dx
    in      al, dx
    mov     bl, al

    xor     eax, eax
    mov     ax, bx
    shl     eax, 16
    cmp     eax, 01000000h
    jb      short V9xReadS3ApertureFailed
    cmp     eax, 0ffc00000h
    ja      short V9xReadS3ApertureFailed
    ret
V9xReadS3ApertureFailed:
    xor     eax, eax
    ret
V9xReadS3Aperture ENDP

V9xEnableS3LinearAperture PROC NEAR
    ; Unlock the S3 system-extension registers, select a 4-MiB aperture in
    ; CR58[1:0], and explicitly enable linear addressing in CR58[4]. This is
    ; the sequence used by the Windows 98 S3 display sample after 4F02h.
    mov     dx, 03d4h
    mov     ax, 4838h
    out     dx, ax
    mov     ax, 0a039h
    out     dx, ax

    mov     al, 58h
    out     dx, al
    inc     dx
    in      al, dx
    and     al, 0fch
    or      al, 13h
    out     dx, al
    in      al, dx
    and     al, 13h
    cmp     al, 13h
    jne     short V9xEnableS3LinearApertureFailed
    mov     ax, 1
    ret
V9xEnableS3LinearApertureFailed:
    xor     ax, ax
    ret
V9xEnableS3LinearAperture ENDP

PUBLIC V9XHARDWAREENABLE
V9XHARDWAREENABLE PROC FAR
    push    bx
    push    cx
    push    dx
    push    si
    push    di
    push    es

    mov     V9xEnableResult, 0
    mov     V9xHardwareStageCode, 1
    call    V9xFindPciDevice
    or      ax, ax
    jnz     short V9xHardwareDeviceFound
    jmp     V9xHardwareEnableDone
V9xHardwareDeviceFound:
    mov     V9xHardwareStageCode, 2
    call    V9xSetVbeMode
    or      ax, ax
    jnz     short V9xHardwareModeSet
    jmp     V9xHardwareEnableDone
V9xHardwareModeSet:
    mov     V9xHardwareStageCode, 3
    call    V9xReadS3Aperture
    or      eax, eax
    jnz     short V9xHardwareBaseValid
    jmp     V9xHardwareEnableDone
V9xHardwareBaseValid:
    mov     V9xHardwareStageCode, 8
    push    eax
    call    V9xEnableS3LinearAperture
    mov     dx, ax
    pop     eax
    or      dx, dx
    jnz     short V9xHardwareApertureEnabled
    jmp     V9xHardwareEnableDone
V9xHardwareApertureEnabled:
    mov     V9xHardwareStageCode, 4

    cmp     V9xScreenSelector, 0
    je      short V9xHardwareAllocate
    cmp     eax, V9xPhysicalBase
    je      short V9xHardwareReuse
    jmp     V9xHardwareEnableDone
V9xHardwareReuse:
    mov     V9xHardwareStageCode, 0
    mov     ax, V9xScreenSelector
    mov     V9xEnableResult, ax
    jmp     V9xHardwareEnableDone

V9xHardwareAllocate:
    mov     V9xPhysicalBase, eax
    xor     ax, ax
    mov     cx, 1
    int     31h
    jnc     short V9xHardwareSelectorAllocated
    jmp     V9xHardwareMapFailed
V9xHardwareSelectorAllocated:
    mov     V9xHardwareStageCode, 5
    mov     V9xScreenSelector, ax

    mov     eax, V9xPhysicalBase
    mov     ebx, eax
    shr     ebx, 16
    mov     cx, ax
    mov     si, 003fh
    mov     di, 0ffffh
    mov     ax, 0800h
    int     31h
    jc      short V9xHardwareFreeSelector
    mov     word ptr V9xLinearAddress, cx
    mov     word ptr V9xLinearAddress+2, bx
    mov     V9xHardwareStageCode, 6

    mov     bx, V9xScreenSelector
    mov     dx, word ptr V9xLinearAddress
    mov     cx, word ptr V9xLinearAddress+2
    mov     ax, 0007h
    int     31h
    jc      short V9xHardwareUnmap
    mov     V9xHardwareStageCode, 7

    mov     bx, V9xScreenSelector
    mov     cx, 003fh
    mov     dx, 0ffffh
    mov     ax, 0008h
    int     31h
    jc      short V9xHardwareUnmap

    mov     V9xHardwareStageCode, 0
    mov     ax, V9xScreenSelector
    mov     V9xEnableResult, ax
    jmp     short V9xHardwareEnableDone

V9xHardwareUnmap:
    mov     bx, word ptr V9xLinearAddress+2
    mov     cx, word ptr V9xLinearAddress
    mov     ax, 0801h
    int     31h
    mov     V9xLinearAddress, 0

V9xHardwareFreeSelector:
    mov     bx, V9xScreenSelector
    mov     ax, 0001h
    int     31h
    mov     V9xScreenSelector, 0

V9xHardwareMapFailed:
    mov     V9xPhysicalBase, 0

V9xHardwareEnableDone:
    pop     es
    pop     di
    pop     si
    pop     dx
    pop     cx
    pop     bx
    mov     ax, V9xEnableResult
    retf
V9XHARDWAREENABLE ENDP

PUBLIC V9XHARDWARESTAGE
V9XHARDWARESTAGE PROC FAR
    mov     ax, V9xHardwareStageCode
    retf
V9XHARDWARESTAGE ENDP

PUBLIC V9XHARDWARERESET
V9XHARDWARERESET PROC FAR
    push    bx
    call    V9xSetVbeMode
    or      ax, ax
    jz      short V9xHardwareResetDone
    call    V9xEnableS3LinearAperture
V9xHardwareResetDone:
    pop     bx
    retf
V9XHARDWARERESET ENDP

PUBLIC V9XHARDWAREBASE
V9XHARDWAREBASE PROC FAR
    mov     ax, word ptr V9xPhysicalBase
    mov     dx, word ptr V9xPhysicalBase+2
    retf
V9XHARDWAREBASE ENDP

PUBLIC V9XHARDWAREDISABLE
V9XHARDWAREDISABLE PROC FAR
    push    bx
    push    cx
    push    dx

    mov     ax, 0003h
    int     10h

    cmp     V9xLinearAddress, 0
    je      short V9xHardwareDisableSelector
    mov     bx, word ptr V9xLinearAddress+2
    mov     cx, word ptr V9xLinearAddress
    mov     ax, 0801h
    int     31h
    mov     V9xLinearAddress, 0

V9xHardwareDisableSelector:
    cmp     V9xScreenSelector, 0
    je      short V9xHardwareDisableDone
    mov     bx, V9xScreenSelector
    mov     ax, 0001h
    int     31h
    mov     V9xScreenSelector, 0
    mov     V9xPhysicalBase, 0

V9xHardwareDisableDone:
    pop     dx
    pop     cx
    pop     bx
    retf
V9XHARDWAREDISABLE ENDP

END
