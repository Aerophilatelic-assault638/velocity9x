; Velocity9x first boot-loadable Windows 9x mini-VDD.
;
; This stage verifies the master VDD ABI, installs legacy VESA and Windows 98
; monitor-power callbacks, and emits bounded COM1 diagnostics. It advertises
; D0 only because the VESA BIOS resume path can blank an S3 ViRGE display
; without reliably restoring the active high-resolution framebuffer.

.386p

.xlist
include VMM.INC
include MINIVDD.INC
.list

Declare_Virtual_Device V9XMINI, 1, 0, MiniVDD_Control, \
                       Undefined_Device_ID, VDD_Init_Order, , ,

VxD_LOCKED_DATA_SEG
include V9XBUILD.INC
public V9xMiniVddBuildId
VxD_LOCKED_DATA_ENDS

VxD_LOCKED_CODE_SEG

; ESI points to ECX bytes. Preserve all registers and bound every UART wait.
BeginProc V9xMini_Serial_Write
    pushfd
    pushad

    mov     dx, 03fbh
    in      al, dx
    cmp     al, 0ffh
    je      short V9xMini_Serial_Done
    mov     ah, al
    and     al, 07fh
    out     dx, al

V9xMini_Serial_Next:
    test    ecx, ecx
    jz      short V9xMini_Serial_Restore
    mov     ebx, 0000ffffh

V9xMini_Serial_Wait:
    mov     dx, 03fdh
    in      al, dx
    test    al, 020h
    jnz     short V9xMini_Serial_Send
    dec     ebx
    jnz     short V9xMini_Serial_Wait
    jmp     short V9xMini_Serial_Restore

V9xMini_Serial_Send:
    mov     al, [esi]
    mov     dx, 03f8h
    out     dx, al
    inc     esi
    dec     ecx
    jmp     short V9xMini_Serial_Next

V9xMini_Serial_Restore:
    mov     dx, 03fbh
    mov     al, ah
    out     dx, al

V9xMini_Serial_Done:
    popad
    popfd
    ret
EndProc V9xMini_Serial_Write

; Update the S3 ViRGE DPMS state without changing the active video mode.
;
; CL contains the S3 SR0D DPMS bits: bit 4 disables horizontal sync and bit 6
; disables vertical sync.  Windows power states map to 00h (D0), 10h (D1),
; 40h (D2), and 50h (D3).  The routine also clears CR56[2:1], the alternate
; S3 DPMS controls, and clears SR01[5] on wake in case the BIOS used the
; generic VGA screen-off bit.  All registers and flags are preserved.
BeginProc V9xMini_Set_Dpms
    pushfd
    pushad

    ; Save and unlock the extended sequencer registers.
    mov     dx, 03c4h
    in      al, dx
    mov     bl, al
    mov     al, 08h
    out     dx, al
    inc     dx
    in      al, dx
    mov     bh, al
    mov     al, 06h
    out     dx, al

    ; Program SR0D horizontal/vertical sync suppression.
    dec     dx
    mov     al, 0dh
    out     dx, al
    inc     dx
    in      al, dx
    and     al, 0afh
    or      al, cl
    out     dx, al

    ; D0 must also undo the generic VGA sequencer screen-off bit.
    test    cl, cl
    jnz     short V9xMini_Dpms_Seq_Restore
    dec     dx
    mov     al, 01h
    out     dx, al
    inc     dx
    in      al, dx
    and     al, 0dfh
    out     dx, al

V9xMini_Dpms_Seq_Restore:
    ; Restore the sequencer extension lock and caller's index.
    dec     dx
    mov     al, 08h
    out     dx, al
    inc     dx
    mov     al, bh
    out     dx, al
    dec     dx
    mov     al, bl
    out     dx, al

    ; On wake, clear CR56[2:1].  Some S3 BIOSes use these alternate DPMS
    ; controls in addition to SR0D.  Select the mono/color CRTC from 3CCh.
    test    cl, cl
    jnz     short V9xMini_Dpms_Done
    mov     dx, 03cch
    in      al, dx
    test    al, 01h
    jz      short V9xMini_Dpms_Mono
    mov     dx, 03d4h
    jmp     short V9xMini_Dpms_Crtc_Selected
V9xMini_Dpms_Mono:
    mov     dx, 03b4h
V9xMini_Dpms_Crtc_Selected:
    in      al, dx
    mov     bl, al

    ; Save CR38/CR39, unlock S3 system registers, clear CR56 DPMS bits,
    ; then restore the locks and the caller's CRTC index.
    mov     al, 38h
    out     dx, al
    inc     dx
    in      al, dx
    mov     bh, al
    dec     dx
    mov     al, 39h
    out     dx, al
    inc     dx
    in      al, dx
    mov     ch, al
    dec     dx
    mov     al, 38h
    out     dx, al
    inc     dx
    mov     al, 48h
    out     dx, al
    dec     dx
    mov     al, 39h
    out     dx, al
    inc     dx
    mov     al, 0a5h
    out     dx, al
    dec     dx
    mov     al, 56h
    out     dx, al
    inc     dx
    in      al, dx
    and     al, 0f9h
    out     dx, al
    dec     dx
    mov     al, 39h
    out     dx, al
    inc     dx
    mov     al, ch
    out     dx, al
    dec     dx
    mov     al, 38h
    out     dx, al
    inc     dx
    mov     al, bh
    out     dx, al
    dec     dx
    mov     al, bl
    out     dx, al

V9xMini_Dpms_Done:
    popad
    popfd
    ret
EndProc V9xMini_Set_Dpms

; Windows 98 DDK SET_MONITOR_POWER_STATE callback.
; Entry: [ESP+4] devnode, [ESP+8] CM_POWERSTATE_D0..D3.
; Exit:  EAX = CR_SUCCESS when handled, CR_DEFAULT for an unknown state.
BeginProc MiniVDD_SetMonitorPowerState
    mov     eax, [esp+8]
    cmp     eax, 00000001h             ; CM_POWERSTATE_D0
    je      short V9xMini_Set_Monitor_D0
    cmp     eax, 00000002h             ; CM_POWERSTATE_D1
    je      short V9xMini_Set_Monitor_D1
    cmp     eax, 00000004h             ; CM_POWERSTATE_D2
    je      short V9xMini_Set_Monitor_D2
    cmp     eax, 00000008h             ; CM_POWERSTATE_D3
    je      short V9xMini_Set_Monitor_D3
    mov     eax, 00000001h             ; CR_DEFAULT
    ret

V9xMini_Set_Monitor_D0:
    xor     ecx, ecx
    call    V9xMini_Set_Dpms
    mov     esi, OFFSET32 V9xMiniPowerOnLine
    mov     ecx, V9xMiniPowerOnLineLength
    call    V9xMini_Serial_Write
    xor     eax, eax                   ; CR_SUCCESS
    ret
V9xMini_Set_Monitor_D1:
    mov     ecx, 10h
    jmp     short V9xMini_Set_Monitor_Low_Power
V9xMini_Set_Monitor_D2:
    mov     ecx, 40h
    jmp     short V9xMini_Set_Monitor_Low_Power
V9xMini_Set_Monitor_D3:
    mov     ecx, 50h
V9xMini_Set_Monitor_Low_Power:
    call    V9xMini_Set_Dpms
    mov     esi, OFFSET32 V9xMiniPowerOffLine
    mov     ecx, V9xMiniPowerOffLineLength
    call    V9xMini_Serial_Write
    xor     eax, eax                   ; CR_SUCCESS
    ret
EndProc MiniVDD_SetMonitorPowerState

; Windows 98 DDK GET_MONITOR_POWER_STATE_CAPS callback. Resume through the
; Win98 VESA fallback is not reliable for this driver, so advertise D0 only.
BeginProc MiniVDD_GetMonitorPowerStateCaps
    mov     eax, 00000001h
    ret
EndProc MiniVDD_GetMonitorPowerStateCaps

; Handle VESA DPMS Set Display Power State before the video BIOS sees it.
; Legacy master VDDs use this route when their dispatch table predates the
; Windows 98 4.1 monitor-power entries.  Carry set means fully handled.
; Entry: AX=VESA function, EBP=Client_Reg_Struc (BL=0 query or BL=1 set).
BeginProc MiniVDD_VESASupport
    cmp     ax, 4f10h
    jne     short V9xMini_Vesa_Default
    cmp     [ebp.Client_BL], 00h
    je      short V9xMini_Vesa_Query
    cmp     [ebp.Client_BL], 01h
    jne     short V9xMini_Vesa_Default
    push    ecx
    mov     cl, [ebp.Client_BH]
    test    cl, cl
    jz      short V9xMini_Vesa_D0
    cmp     cl, 01h
    je      short V9xMini_Vesa_D1
    cmp     cl, 02h
    je      short V9xMini_Vesa_D2
    cmp     cl, 04h
    je      short V9xMini_Vesa_D3
    pop     ecx
    jmp     short V9xMini_Vesa_Default
V9xMini_Vesa_D0:
    xor     ecx, ecx
    jmp     short V9xMini_Vesa_Apply
V9xMini_Vesa_D1:
    mov     ecx, 10h
    jmp     short V9xMini_Vesa_Apply
V9xMini_Vesa_D2:
    mov     ecx, 40h
    jmp     short V9xMini_Vesa_Apply
V9xMini_Vesa_D3:
    mov     ecx, 50h
V9xMini_Vesa_Apply:
    call    V9xMini_Set_Dpms
    mov     [ebp.Client_AX], 004fh      ; VESA call supported and successful
    pop     ecx
    stc
    ret
V9xMini_Vesa_Query:
    mov     [ebp.Client_BX], 0000h      ; no low-power DPMS states supported
    mov     [ebp.Client_AX], 004fh
    stc
    ret
V9xMini_Vesa_Default:
    clc
    ret
EndProc MiniVDD_VESASupport

; Legacy Win9x master VDDs issue VESA 4F10h directly when the 4.1 monitor-
; power callbacks are unavailable.  The post hook runs after that BIOS call;
; on a Set Display Power State / D0 request, force the S3 DPMS controls back
; on so a BIOS/emulator combination cannot leave the display latched blank.
; Entry: DX = VESA function, EBP = Client_Reg_Struc.  Preserve used registers.
BeginProc MiniVDD_VESACallPostProcessing
    cmp     dx, 4f10h
    jne     short V9xMini_Vesa_Post_Done
    push    eax
    mov     ax, [ebp.Client_BX]
    cmp     al, 01h                   ; Set Display Power State
    jne     short V9xMini_Vesa_Post_Restore
    test    ah, ah                    ; BH=0 is D0 / monitor on
    jnz     short V9xMini_Vesa_Post_Restore
    push    ecx
    xor     ecx, ecx
    call    V9xMini_Set_Dpms
    pop     ecx
V9xMini_Vesa_Post_Restore:
    pop     eax
V9xMini_Vesa_Post_Done:
    ret
EndProc MiniVDD_VESACallPostProcessing

VxD_LOCKED_CODE_ENDS

VxD_ICODE_SEG
public MiniVDD_Dynamic_Init
BeginProc MiniVDD_Dynamic_Init
    mov     esi, OFFSET32 V9xMiniInitLine
    mov     ecx, V9xMiniInitLineLength
    call    V9xMini_Serial_Write

    VxDCall VDD_Get_Mini_Dispatch_Table
    test    edi, edi
    jz      short V9xMini_Init_Failed
    cmp     ecx, NBR_MINI_VDD_FUNCTIONS
    jb      short V9xMini_Init_Failed

    ; These callbacks exist in the legacy 49-entry table.  VESA_SUPPORT
    ; prevents the problematic BIOS DPMS call; the post hook is a D0 safety
    ; net for calls that another component sends directly to the BIOS.
    MiniVDDDispatch VESA_SUPPORT, VESASupport
    MiniVDDDispatch VESA_CALL_POST_PROCESSING, VESACallPostProcessing

    ; Power callbacks were added to the Windows 98 (4.1) dispatch table.
    ; Older tables are covered by the VESA post hook above.
    cmp     ecx, GET_MONITOR_POWER_STATE_CAPS + 1
    jb      short V9xMini_Power_Defaults
    MiniVDDDispatch SET_MONITOR_POWER_STATE, SetMonitorPowerState
    MiniVDDDispatch GET_MONITOR_POWER_STATE_CAPS, GetMonitorPowerStateCaps

    mov     esi, OFFSET32 V9xMiniPowerCallbacksLine
    mov     ecx, V9xMiniPowerCallbacksLineLength
    call    V9xMini_Serial_Write
    jmp     short V9xMini_Init_Succeeded

V9xMini_Power_Defaults:

    mov     esi, OFFSET32 V9xMiniDefaultsLine
    mov     ecx, V9xMiniDefaultsLineLength
    call    V9xMini_Serial_Write
V9xMini_Init_Succeeded:
    xor     eax, eax
    clc
    ret

V9xMini_Init_Failed:
    mov     esi, OFFSET32 V9xMiniFailLine
    mov     ecx, V9xMiniFailLineLength
    call    V9xMini_Serial_Write
    stc
    ret
EndProc MiniVDD_Dynamic_Init
VxD_ICODE_ENDS

VxD_LOCKED_CODE_SEG
Begin_Control_Dispatch MiniVDD
    Control_Dispatch Device_Init, MiniVDD_Dynamic_Init
    Control_Dispatch Sys_Dynamic_Device_Init, MiniVDD_Dynamic_Init
End_Control_Dispatch MiniVDD
VxD_LOCKED_CODE_ENDS

end
