#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifndef V9X_BUILD_ID
#define V9X_BUILD_ID "local"
#endif

static const char v9x_message[] =
    "V9X-SERIAL-WIN32 v2 build=" V9X_BUILD_ID "\r\n";
static DCB v9x_state;
static COMMTIMEOUTS v9x_timeouts;

static void v9x_fail(HANDLE port, const char *message)
{
    if (port != INVALID_HANDLE_VALUE) {
        CloseHandle(port);
    }
    MessageBoxA(0, message, "Velocity9x COM1 probe", MB_OK | MB_ICONERROR);
    ExitProcess(1ul);
}

void WINAPI V9xSerialEntry(void)
{
    HANDLE port;
    DWORD written = 0ul;

    port = CreateFileA("COM1", GENERIC_READ | GENERIC_WRITE, 0, 0,
                       OPEN_EXISTING, 0, 0);
    if (port == INVALID_HANDLE_VALUE) {
        v9x_fail(port, "Opening COM1 through Windows VCOMM failed.");
    }

    v9x_state.DCBlength = sizeof(v9x_state);
    if (!GetCommState(port, &v9x_state)) {
        v9x_fail(port, "Reading the COM1 state failed.");
    }

    v9x_state.BaudRate = CBR_9600;
    v9x_state.fBinary = TRUE;
    v9x_state.fParity = FALSE;
    v9x_state.fOutxCtsFlow = FALSE;
    v9x_state.fOutxDsrFlow = FALSE;
    v9x_state.fDtrControl = DTR_CONTROL_ENABLE;
    v9x_state.fDsrSensitivity = FALSE;
    v9x_state.fOutX = FALSE;
    v9x_state.fInX = FALSE;
    v9x_state.fRtsControl = RTS_CONTROL_ENABLE;
    v9x_state.ByteSize = 8;
    v9x_state.Parity = NOPARITY;
    v9x_state.StopBits = ONESTOPBIT;
    if (!SetCommState(port, &v9x_state)) {
        v9x_fail(port, "Configuring COM1 for 9600 8N1 failed.");
    }

    v9x_timeouts.ReadIntervalTimeout = MAXDWORD;
    v9x_timeouts.WriteTotalTimeoutConstant = 2000ul;
    if (!SetCommTimeouts(port, &v9x_timeouts)) {
        v9x_fail(port, "Applying the bounded COM1 timeout failed.");
    }

    if (!WriteFile(port, v9x_message, sizeof(v9x_message) - 1u, &written, 0)) {
        v9x_fail(port, "Writing the COM1 smoke line failed.");
    }
    if (written != sizeof(v9x_message) - 1u) {
        v9x_fail(port, "COM1 accepted only part of the smoke line.");
    }

    (void)FlushFileBuffers(port);
    CloseHandle(port);
    MessageBoxA(0, "Velocity9x Win32 COM1 smoke line sent at 9600 8N1.",
                "Velocity9x COM1 probe", MB_OK | MB_ICONINFORMATION);
    ExitProcess(0ul);
}
