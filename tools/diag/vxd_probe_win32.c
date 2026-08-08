#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static void v9x_finish(const char *message, UINT icon, DWORD exit_code)
{
    MessageBoxA(0, message, "Velocity9x VxD probe", MB_OK | icon);
    ExitProcess(exit_code);
}

void WINAPI V9xVxdProbeEntry(void)
{
    HANDLE device;

    device = CreateFileA("\\\\.\\V9XPROBE.VXD", 0, 0, 0, CREATE_NEW,
                         FILE_FLAG_DELETE_ON_CLOSE, 0);
    if (device == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_NOT_SUPPORTED) {
            v9x_finish("The VxD loaded but rejected the Win32 control channel.",
                       MB_ICONERROR, 2ul);
        }
        v9x_finish("Loading V9XPROBE.VXD failed. Keep both probe files in "
                   "the same directory.", MB_ICONERROR, 1ul);
    }

    MessageBoxA(0, "V9XPROBE.VXD loaded successfully. Click OK to unload it.",
                "Velocity9x VxD probe", MB_OK | MB_ICONINFORMATION);
    CloseHandle(device);
    v9x_finish("V9XPROBE.VXD unloaded successfully.",
               MB_ICONINFORMATION, 0ul);
}
