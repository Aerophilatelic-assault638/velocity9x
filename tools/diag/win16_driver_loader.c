#include <windows.h>

#ifndef V9X_BUILD_ID
#define V9X_BUILD_ID "local"
#endif

static const char v9x_title[] = "Velocity9x Win16 probe " V9X_BUILD_ID;

static int v9x_is_quiet(const char FAR *command_line)
{
    if (command_line == 0) {
        return 0;
    }
    while (*command_line == ' ' || *command_line == '\t') {
        ++command_line;
    }
    return (command_line[0] == '/' || command_line[0] == '-') &&
           (command_line[1] == 'q' || command_line[1] == 'Q');
}

#pragma off (unreferenced)
int PASCAL WinMain(HINSTANCE instance,
                   HINSTANCE previous_instance,
                   LPSTR command_line,
                   int show_command)
#pragma on (unreferenced)
{
    HINSTANCE driver;
    int quiet = v9x_is_quiet(command_line);

    driver = LoadLibrary("V9XDISP.DRV");
    if ((UINT)driver < 32u) {
        if (!quiet) {
            MessageBox(0,
                       "Loading V9XDISP.DRV as an inactive library failed. "
                       "Keep V9X16LD.EXE and V9XDISP.DRV together.",
                       v9x_title,
                       MB_OK | MB_ICONHAND);
        }
        return 1;
    }

    if (!quiet) {
        MessageBox(0,
                   "V9XDISP.DRV loaded without enabling the display. "
                   "Click OK to unload it.",
                   v9x_title,
                   MB_OK | MB_ICONINFORMATION);
    }
    FreeLibrary(driver);
    if (!quiet) {
        MessageBox(0,
                   "V9XDISP.DRV unloaded successfully.",
                   v9x_title,
                   MB_OK | MB_ICONINFORMATION);
    }
    return 0;
}
