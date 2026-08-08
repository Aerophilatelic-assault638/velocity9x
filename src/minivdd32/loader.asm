; Velocity9x first boot-loadable Windows 9x mini-VDD.
;
; This stage verifies the master VDD ABI and emits bounded COM1 diagnostics.
; It intentionally leaves every mini-VDD dispatch-table entry untouched, so
; the master VDD retains its default handlers and no stale callbacks exist.

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

    mov     esi, OFFSET32 V9xMiniDefaultsLine
    mov     ecx, V9xMiniDefaultsLineLength
    call    V9xMini_Serial_Write
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
