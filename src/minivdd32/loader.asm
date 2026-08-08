; Original, deliberately inert Windows 9x mini-VDD loader skeleton.
;
; This image proves the DDK MASM/VxD link path.  Its initialization entry
; point always returns carry set, so it cannot claim or program hardware.

.386p

.xlist
include VMM.INC
include MINIVDD.INC
.list

Declare_Virtual_Device V9XMINI, 1, 0, MiniVDD_Control, \
                       Undefined_Device_ID, VDD_Init_Order, , ,

VxD_LOCKED_DATA_SEG
public V9xMiniVddBuildId
V9xMiniVddBuildId label byte
include V9XBUILD.INC
VxD_LOCKED_DATA_ENDS

VxD_ICODE_SEG
public MiniVDD_Dynamic_Init
BeginProc MiniVDD_Dynamic_Init
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
