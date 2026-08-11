#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define V9X_AUTOEXEC_PATH "C:\\AUTOEXEC.BAT"
#define V9X_GUARD_TOKEN "V9XSAFE\\MGA2\\V9XGUARD.BAT"
#define V9X_GUARD_LINE "CALL C:\\V9XSAFE\\MGA2\\V9XGUARD.BAT\r\n"
#define V9X_AUTOEXEC_LIMIT 65535u

static BYTE v9x_autoexec[V9X_AUTOEXEC_LIMIT];

static BYTE v9x_upper(BYTE value)
{
    if (value >= (BYTE)'a' && value <= (BYTE)'z') {
        value = (BYTE)(value - ((BYTE)'a' - (BYTE)'A'));
    }
    return value;
}

static int v9x_contains_ci(const BYTE *data, DWORD length, const char *token)
{
    DWORD token_length = (DWORD)lstrlenA(token);
    DWORD offset;
    DWORD index;

    if (token_length == 0u || token_length > length) return 0;
    for (offset = 0u; offset + token_length <= length; ++offset) {
        for (index = 0u; index < token_length; ++index) {
            if (v9x_upper(data[offset + index]) !=
                v9x_upper((BYTE)token[index])) break;
        }
        if (index == token_length) return 1;
    }
    return 0;
}

static int v9x_write_all(HANDLE file, const void *data, DWORD length)
{
    const BYTE *bytes = (const BYTE *)data;
    DWORD written;

    while (length != 0u) {
        if (!WriteFile(file, bytes, length, &written, 0) || written == 0u) {
            return 0;
        }
        bytes += written;
        length -= written;
    }
    return 1;
}

void WINAPI V9xMatroxGuardAutoexecEntry(void)
{
    HANDLE file;
    DWORD size;
    DWORD read;
    DWORD logical_end;
    DWORD line_length = (DWORD)lstrlenA(V9X_GUARD_LINE);
    int needs_separator;
    int ok;

    file = CreateFileA(V9X_AUTOEXEC_PATH, GENERIC_READ, FILE_SHARE_READ, 0,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE) ExitProcess(2u);
    size = GetFileSize(file, 0);
    if (size == INVALID_FILE_SIZE || size > V9X_AUTOEXEC_LIMIT ||
        !ReadFile(file, v9x_autoexec, size, &read, 0) || read != size) {
        CloseHandle(file);
        ExitProcess(3u);
    }
    CloseHandle(file);

    logical_end = 0u;
    while (logical_end < size && v9x_autoexec[logical_end] != 0u &&
           v9x_autoexec[logical_end] != 0x1au) {
        ++logical_end;
    }
    if (v9x_contains_ci(v9x_autoexec, logical_end, V9X_GUARD_TOKEN)) {
        ExitProcess(0u);
    }
    needs_separator = logical_end != 0u &&
                      v9x_autoexec[logical_end - 1u] != (BYTE)'\n';
    if (logical_end + (needs_separator ? 2u : 0u) + line_length >
        V9X_AUTOEXEC_LIMIT) {
        ExitProcess(4u);
    }

    file = CreateFileA(V9X_AUTOEXEC_PATH, GENERIC_WRITE, 0, 0,
                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE) ExitProcess(5u);
    ok = v9x_write_all(file, v9x_autoexec, logical_end);
    if (ok && needs_separator) ok = v9x_write_all(file, "\r\n", 2u);
    if (ok) ok = v9x_write_all(file, V9X_GUARD_LINE, line_length);
    if (ok) ok = FlushFileBuffers(file) != 0;
    CloseHandle(file);
    ExitProcess(ok ? 0u : 6u);
}
