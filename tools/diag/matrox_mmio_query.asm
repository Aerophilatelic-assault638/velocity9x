; Read-only MGA-2164W control-aperture query.
.386p

.xlist
include VMM.INC
include VWIN32.INC
.list

V9XMGAQ_IOCTL_QUERY equ 1
V9XMGAQ_RESULT_SIZE equ 20
V9XMGAQ_MAGIC equ 3241474dh       ; "MGA2" in little endian

Declare_Virtual_Device V9XMGAQ, 1, 0, V9xMgaQ_Control, \
                       Undefined_Device_ID, Undefined_Init_Order, , ,

VxD_LOCKED_DATA_SEG
include V9XBUILD.INC
public V9xMgaQBuildId
VxD_LOCKED_DATA_ENDS

VxD_LOCKED_CODE_SEG
BeginProc V9xMgaQ_Dynamic_Init
    clc
    ret
EndProc V9xMgaQ_Dynamic_Init

BeginProc V9xMgaQ_Dynamic_Exit
    clc
    ret
EndProc V9xMgaQ_Dynamic_Exit

BeginProc V9xMgaQ_W32_DeviceIoControl
    cmp     ecx, DIOC_OPEN
    je      short V9xMgaQ_Success
    cmp     ecx, DIOC_CLOSEHANDLE
    je      short V9xMgaQ_Success
    cmp     ecx, V9XMGAQ_IOCTL_QUERY
    jne     short V9xMgaQ_Error

    cmp     [esi.DIOCParams.cbInBuffer], 4
    jne     short V9xMgaQ_Error
    cmp     [esi.DIOCParams.cbOutBuffer], V9XMGAQ_RESULT_SIZE
    jb      short V9xMgaQ_Error
    mov     ebx, [esi.DIOCParams.lpvInBuffer]
    test    ebx, ebx
    jz      short V9xMgaQ_Error
    mov     eax, [ebx]
    test    eax, 00003fffh
    jnz     short V9xMgaQ_Error

    VMMCall _MapPhysToLinear, <eax, 00004000h, 0>
    cmp     eax, -1
    je      short V9xMgaQ_Error
    test    eax, eax
    jz      short V9xMgaQ_Error

    mov     ebx, [esi.DIOCParams.lpvOutBuffer]
    test    ebx, ebx
    jz      short V9xMgaQ_Error
    mov     dword ptr [ebx+0], V9XMGAQ_MAGIC
    mov     edx, [eax+01e14h]     ; STATUS: documented read-only register
    mov     [ebx+4], edx
    mov     edx, [eax+01e20h]     ; VCOUNT: documented read-only register
    mov     [ebx+8], edx
    mov     edx, [eax+01e54h]     ; OPMODE: read without modification
    mov     [ebx+12], edx
    mov     edx, [esi.DIOCParams.lpvInBuffer]
    mov     edx, [edx]
    mov     [ebx+16], edx
    mov     ebx, [esi.DIOCParams.lpcbBytesReturned]
    test    ebx, ebx
    jz      short V9xMgaQ_Success
    mov     dword ptr [ebx], V9XMGAQ_RESULT_SIZE

V9xMgaQ_Success:
    xor     eax, eax
    ret
V9xMgaQ_Error:
    mov     eax, 1
    ret
EndProc V9xMgaQ_W32_DeviceIoControl

Begin_Control_Dispatch V9xMgaQ
    Control_Dispatch Sys_Dynamic_Device_Init, V9xMgaQ_Dynamic_Init
    Control_Dispatch Sys_Dynamic_Device_Exit, V9xMgaQ_Dynamic_Exit
    Control_Dispatch W32_DeviceIoControl, V9xMgaQ_W32_DeviceIoControl
End_Control_Dispatch V9xMgaQ

VxD_LOCKED_CODE_ENDS
end
