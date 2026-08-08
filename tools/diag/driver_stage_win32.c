#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifndef V9X_BUILD_ID
#define V9X_BUILD_ID "local"
#endif

static DCB v9x_serial_state;
static COMMTIMEOUTS v9x_serial_timeouts;

static const char v9x_begin_line[] =
    "V9X-STAGE begin build=" V9X_BUILD_ID "\r\n";
static const char v9x_pair_line[] =
    "V9X-STAGE pair-loaded build=" V9X_BUILD_ID "\r\n";
static const char v9x_pass_line[] =
    "V9X-STAGE PASS build=" V9X_BUILD_ID "\r\n";
static const char v9x_fail_vxd_line[] =
    "V9X-STAGE FAIL vxd-load build=" V9X_BUILD_ID "\r\n";
static const char v9x_fail_win16_line[] =
    "V9X-STAGE FAIL drv-load build=" V9X_BUILD_ID "\r\n";
static const char v9x_fail_wait_line[] =
    "V9X-STAGE FAIL drv-wait build=" V9X_BUILD_ID "\r\n";

static BOOL v9x_serial_write(const char *message, DWORD length)
{
    HANDLE port;
    DWORD written = 0ul;
    BOOL result = FALSE;

    port = CreateFileA("COM1", GENERIC_READ | GENERIC_WRITE, 0, 0,
                       OPEN_EXISTING, 0, 0);
    if (port == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    v9x_serial_state.DCBlength = sizeof(v9x_serial_state);
    if (GetCommState(port, &v9x_serial_state)) {
        v9x_serial_state.BaudRate = CBR_9600;
        v9x_serial_state.fBinary = TRUE;
        v9x_serial_state.fParity = FALSE;
        v9x_serial_state.fOutxCtsFlow = FALSE;
        v9x_serial_state.fOutxDsrFlow = FALSE;
        v9x_serial_state.fDtrControl = DTR_CONTROL_ENABLE;
        v9x_serial_state.fDsrSensitivity = FALSE;
        v9x_serial_state.fOutX = FALSE;
        v9x_serial_state.fInX = FALSE;
        v9x_serial_state.fRtsControl = RTS_CONTROL_ENABLE;
        v9x_serial_state.ByteSize = 8;
        v9x_serial_state.Parity = NOPARITY;
        v9x_serial_state.StopBits = ONESTOPBIT;
        v9x_serial_timeouts.ReadIntervalTimeout = MAXDWORD;
        v9x_serial_timeouts.WriteTotalTimeoutConstant = 2000ul;
        if (SetCommState(port, &v9x_serial_state) &&
            SetCommTimeouts(port, &v9x_serial_timeouts) &&
            WriteFile(port, message, length, &written, 0) &&
            written == length) {
            (void)FlushFileBuffers(port);
            result = TRUE;
        }
    }

    CloseHandle(port);
    return result;
}

#define V9X_WRITE_LINE(line) \
    v9x_serial_write((line), (DWORD)(sizeof(line) - 1u))

static void v9x_finish(HANDLE device,
                       const char *serial_line,
                       DWORD serial_length,
                       const char *message,
                       DWORD exit_code)
{
    if (device != INVALID_HANDLE_VALUE) {
        CloseHandle(device);
    }
    (void)v9x_serial_write(serial_line, serial_length);
    MessageBoxA(0, message, "Velocity9x driver-stage test",
                MB_OK | (exit_code == 0ul ? MB_ICONINFORMATION : MB_ICONERROR));
    ExitProcess(exit_code);
}

void WINAPI V9xDriverStageEntry(void)
{
    HANDLE device;
    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    DWORD wait_result;
    DWORD process_exit = 1ul;
    char command_line[] = "V9X16LD.EXE /quiet";

    (void)V9X_WRITE_LINE(v9x_begin_line);

    device = CreateFileA("\\\\.\\V9XPROBE.VXD", 0, 0, 0, CREATE_NEW,
                         FILE_FLAG_DELETE_ON_CLOSE, 0);
    if (device == INVALID_HANDLE_VALUE) {
        v9x_finish(device, v9x_fail_vxd_line,
                   (DWORD)(sizeof(v9x_fail_vxd_line) - 1u),
                   "FAIL: the ring-0 VxD did not load.", 1ul);
    }

    startup.cb = sizeof(startup);
    startup.lpReserved = 0;
    startup.lpDesktop = 0;
    startup.lpTitle = 0;
    startup.dwX = 0ul;
    startup.dwY = 0ul;
    startup.dwXSize = 0ul;
    startup.dwYSize = 0ul;
    startup.dwXCountChars = 0ul;
    startup.dwYCountChars = 0ul;
    startup.dwFillAttribute = 0ul;
    startup.dwFlags = 0ul;
    startup.wShowWindow = 0;
    startup.cbReserved2 = 0;
    startup.lpReserved2 = 0;
    startup.hStdInput = 0;
    startup.hStdOutput = 0;
    startup.hStdError = 0;

    process.hProcess = 0;
    process.hThread = 0;
    if (!CreateProcessA("V9X16LD.EXE", command_line, 0, 0, FALSE, 0,
                        0, 0, &startup, &process)) {
        v9x_finish(device, v9x_fail_win16_line,
                   (DWORD)(sizeof(v9x_fail_win16_line) - 1u),
                   "FAIL: the Win16 display DRV test did not start.", 2ul);
    }

    CloseHandle(process.hThread);
    wait_result = WaitForSingleObject(process.hProcess, 30000ul);
    if (wait_result != WAIT_OBJECT_0 ||
        !GetExitCodeProcess(process.hProcess, &process_exit)) {
        CloseHandle(process.hProcess);
        v9x_finish(device, v9x_fail_wait_line,
                   (DWORD)(sizeof(v9x_fail_wait_line) - 1u),
                   "FAIL: the Win16 display DRV test did not finish.", 3ul);
    }
    CloseHandle(process.hProcess);

    if (process_exit != 0ul) {
        v9x_finish(device, v9x_fail_win16_line,
                   (DWORD)(sizeof(v9x_fail_win16_line) - 1u),
                   "FAIL: V9XDISP.DRV did not load and unload cleanly.", 4ul);
    }

    (void)V9X_WRITE_LINE(v9x_pair_line);
    v9x_finish(device, v9x_pass_line,
               (DWORD)(sizeof(v9x_pass_line) - 1u),
               "PASS: the VxD and Win16 display DRV loaded together and "
               "unloaded cleanly. No display mode was changed.", 0ul);
}
