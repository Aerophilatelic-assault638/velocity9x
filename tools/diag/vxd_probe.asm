; Velocity9x dynamic-VxD lifecycle probe.
;
; This device is deliberately separate from the mini-VDD skeleton. It touches
; no display hardware and registers no mini-VDD callbacks. Its only ring-0
; side effect is bounded output to the conventional COM1 UART at 03F8h.

.386p

.xlist
include VMM.INC
.list

Declare_Virtual_Device V9XPROBE, 1, 0, V9xProbe_Control, \
                       Undefined_Device_ID, Undefined_Init_Order, , ,

VxD_LOCKED_DATA_SEG
include V9XBUILD.INC
public V9xProbeBuildId
VxD_LOCKED_DATA_ENDS

VxD_LOCKED_CODE_SEG

; ESI points to ECX bytes. Preserve all registers and bound every UART wait.
BeginProc V9xProbe_Serial_Write
    pushfd
    pushad

    mov     dx, 03fbh
    in      al, dx
    cmp     al, 0ffh
    je      short V9xProbe_Serial_Done
    mov     ah, al
    and     al, 07fh
    out     dx, al

V9xProbe_Serial_Next:
    test    ecx, ecx
    jz      short V9xProbe_Serial_Restore
    mov     ebx, 0000ffffh

V9xProbe_Serial_Wait:
    mov     dx, 03fdh
    in      al, dx
    test    al, 020h
    jnz     short V9xProbe_Serial_Send
    dec     ebx
    jnz     short V9xProbe_Serial_Wait
    jmp     short V9xProbe_Serial_Restore

V9xProbe_Serial_Send:
    mov     al, [esi]
    mov     dx, 03f8h
    out     dx, al
    inc     esi
    dec     ecx
    jmp     short V9xProbe_Serial_Next

V9xProbe_Serial_Restore:
    mov     dx, 03fbh
    mov     al, ah
    out     dx, al

V9xProbe_Serial_Done:
    popad
    popfd
    ret
EndProc V9xProbe_Serial_Write

BeginProc V9xProbe_Dynamic_Init
    mov     esi, OFFSET32 V9xProbeInitLine
    mov     ecx, V9xProbeInitLineLength
    call    V9xProbe_Serial_Write
    clc
    ret
EndProc V9xProbe_Dynamic_Init

BeginProc V9xProbe_Dynamic_Exit
    mov     esi, OFFSET32 V9xProbeExitLine
    mov     ecx, V9xProbeExitLineLength
    call    V9xProbe_Serial_Write
    clc
    ret
EndProc V9xProbe_Dynamic_Exit

BeginProc V9xProbe_W32_DeviceIoControl
    cmp     ecx, DIOC_OPEN
    je      short V9xProbe_Dioc_Open
    cmp     ecx, DIOC_CLOSEHANDLE
    je      short V9xProbe_Dioc_Close
    mov     eax, 1
    ret

V9xProbe_Dioc_Open:
    mov     esi, OFFSET32 V9xProbeOpenLine
    mov     ecx, V9xProbeOpenLineLength
    call    V9xProbe_Serial_Write
    xor     eax, eax
    ret

V9xProbe_Dioc_Close:
    mov     esi, OFFSET32 V9xProbeCloseLine
    mov     ecx, V9xProbeCloseLineLength
    call    V9xProbe_Serial_Write
    xor     eax, eax
    ret
EndProc V9xProbe_W32_DeviceIoControl

Begin_Control_Dispatch V9xProbe
    Control_Dispatch Sys_Dynamic_Device_Init, V9xProbe_Dynamic_Init
    Control_Dispatch Sys_Dynamic_Device_Exit, V9xProbe_Dynamic_Exit
    Control_Dispatch W32_DeviceIoControl, V9xProbe_W32_DeviceIoControl
End_Control_Dispatch V9xProbe

VxD_LOCKED_CODE_ENDS

end
