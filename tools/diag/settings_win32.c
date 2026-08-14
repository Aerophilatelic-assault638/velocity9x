#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#include "velocity9x/build.h"
#include "settings_status.h"

#define V9X_ID_COPY_REPORT  1001
#define V9X_ID_RECOVERY     1002
#define V9X_ID_CLOSE        1003
#define V9X_ID_GDI_TEST     1004
#define V9X_ID_LOGO_BITMAP  101

static const char v9x_class_name[] = "Velocity9xSettingsWindow";
static const char v9x_window_title[] = "Velocity9x Settings";
static HFONT v9x_ui_font;
static HBITMAP v9x_logo_bitmap;
static char v9x_recovery_path[MAX_PATH];
static char v9x_gdi_path[MAX_PATH];
static V9X_SETTINGS_STATUS v9x_status;

static void v9x_set_font(HWND control)
{
    SendMessageA(control, WM_SETFONT, (WPARAM)v9x_ui_font, TRUE);
}

static BOOL v9x_find_sibling(char *path, const char *file_name)
{
    DWORD length = GetModuleFileNameA(0, path, MAX_PATH);
    DWORD file_length = v9x_settings_string_length(file_name);
    DWORD index;
    DWORD file_index;

    if (length == 0ul || length >= MAX_PATH) {
        return FALSE;
    }
    index = length;
    while (index != 0ul && path[index - 1ul] != '\\' &&
           path[index - 1ul] != '/') {
        --index;
    }
    if (index == 0ul || index + file_length + 1ul > MAX_PATH) {
        return FALSE;
    }
    for (file_index = 0ul; file_index <= file_length; ++file_index) {
        path[index + file_index] = file_name[file_index];
    }
    return TRUE;
}

static HWND v9x_control(HWND parent,
                        const char *class_name,
                        const char *text,
                        DWORD style,
                        int x,
                        int y,
                        int width,
                        int height,
                        int identifier)
{
    HWND control = CreateWindowExA(0, class_name, text,
                                   WS_CHILD | WS_VISIBLE | style,
                                   x, y, width, height, parent,
                                   (HMENU)(UINT)identifier,
                                   GetModuleHandleA(0), 0);
    if (control != 0) {
        v9x_set_font(control);
    }
    return control;
}

static void v9x_create_controls(HWND window)
{
    HWND control;

    control = v9x_control(window, "STATIC", "",
        SS_BITMAP | SS_CENTERIMAGE, 25, 6, 390, 78, 0);
    if (control != 0 && v9x_logo_bitmap != 0) {
        SendMessageA(control, STM_SETIMAGE, IMAGE_BITMAP,
                     (LPARAM)v9x_logo_bitmap);
    }
    control = v9x_control(window, "STATIC",
        "Engineering bring-up build - conservative features are locked on.",
        SS_CENTER, 16, 86, 398, 16, 0);

    (void)v9x_control(window, "BUTTON", "Display",
                      BS_GROUPBOX, 14, 106, 402, 126, 0);
    (void)v9x_control(window, "STATIC", "Adapter:",
                      SS_LEFT, 28, 127, 76, 18, 0);
    (void)v9x_control(window, "STATIC", v9x_status.adapter_name,
                      SS_LEFT, 110, 127, 278, 18, 0);
    (void)v9x_control(window, "STATIC", "Active mode:",
                      SS_LEFT, 28, 151, 76, 18, 0);
    (void)v9x_control(window, "STATIC", v9x_status.active_mode,
                      SS_LEFT, 110, 151, 278, 18, 0);
    (void)v9x_control(window, "STATIC", "Core clock:",
                      SS_LEFT, 28, 175, 76, 18, 0);
    (void)v9x_control(window, "STATIC", v9x_status.core_clock,
                      SS_LEFT, 110, 175, 278, 18, 0);
    (void)v9x_control(window, "STATIC", "Memory clock:",
                      SS_LEFT, 28, 199, 76, 18, 0);
    (void)v9x_control(window, "STATIC", v9x_status.memory_clock,
                      SS_LEFT, 110, 199, 278, 18, 0);

    (void)v9x_control(window, "BUTTON", "Rendering and safety",
                      BS_GROUPBOX, 14, 242, 402, 86, 0);
    control = v9x_control(window, "BUTTON", "Windows DIB Engine rendering",
                          BS_AUTOCHECKBOX | WS_DISABLED, 28, 262, 250, 20, 0);
    SendMessageA(control, BM_SETCHECK, BST_CHECKED, 0);
    control = v9x_control(window, "BUTTON", "Hardware acceleration",
                          BS_AUTOCHECKBOX | WS_DISABLED, 28, 285, 250, 20, 0);
    SendMessageA(control, BM_SETCHECK,
                 v9x_status.hardware_acceleration
                     ? BST_CHECKED : BST_UNCHECKED, 0);
    (void)v9x_control(window, "STATIC", "Version: " V9X_VERSION_STRING,
                      SS_RIGHT, 218, 287, 180, 18, 0);
    control = v9x_control(window, "BUTTON", "Live mode switching",
                          BS_AUTOCHECKBOX | WS_DISABLED, 28, 308, 190, 20, 0);
    SendMessageA(control, BM_SETCHECK,
                 v9x_status.live_mode_switching ? BST_CHECKED : BST_UNCHECKED,
                 0);
    (void)v9x_control(window, "STATIC", "Build: " V9X_BUILD_ID,
                      SS_RIGHT, 218, 310, 180, 18, 0);

    (void)v9x_control(window, "BUTTON", "Runtime diagnostics",
                      BS_GROUPBOX, 14, 338, 402, 62, 0);
    (void)v9x_control(window, "STATIC", "Driver / framebuffer:",
                      SS_LEFT, 28, 358, 116, 18, 0);
    (void)v9x_control(window, "STATIC", v9x_status.framebuffer_status,
                      SS_LEFT, 148, 358, 250, 18, 0);
    (void)v9x_control(window, "STATIC", "Last GDI test:",
                      SS_LEFT, 28, 380, 116, 18, 0);
    (void)v9x_control(window, "STATIC", v9x_status.gdi_status,
                      SS_LEFT, 148, 380, 250, 18, 0);

    (void)v9x_control(window, "BUTTON", "Copy report",
                      BS_PUSHBUTTON | WS_TABSTOP, 15, 412, 92, 28,
                      V9X_ID_COPY_REPORT);
    (void)v9x_control(window, "BUTTON", "Run GDI test",
                      BS_PUSHBUTTON | WS_TABSTOP, 113, 412, 94, 28,
                      V9X_ID_GDI_TEST);
    (void)v9x_control(window, "BUTTON", "Recovery guide",
                      BS_PUSHBUTTON | WS_TABSTOP, 213, 412, 104, 28,
                      V9X_ID_RECOVERY);
    (void)v9x_control(window, "BUTTON", "Close",
                      BS_DEFPUSHBUTTON | WS_TABSTOP, 323, 412, 93, 28,
                      V9X_ID_CLOSE);
}

static LRESULT CALLBACK v9x_window_proc(HWND window,
                                        UINT message,
                                        WPARAM wparam,
                                        LPARAM lparam)
{
    (void)lparam;
    switch (message) {
    case WM_CREATE:
        v9x_create_controls(window);
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case V9X_ID_COPY_REPORT:
            (void)v9x_settings_copy_report(window, v9x_window_title,
                                           v9x_status.report);
            return 0;
        case V9X_ID_RECOVERY:
            if (!v9x_find_sibling(v9x_recovery_path, "RECOVER.TXT") ||
                (UINT)ShellExecuteA(window, "open", v9x_recovery_path,
                                    0, 0, SW_SHOWNORMAL) <= 32u) {
                MessageBoxA(window,
                    "RECOVER.TXT was not found beside V9XSET.EXE.",
                    v9x_window_title, MB_OK | MB_ICONERROR);
            }
            return 0;
        case V9X_ID_GDI_TEST:
            if (!v9x_find_sibling(v9x_gdi_path, "V9XGDI.EXE") ||
                (UINT)ShellExecuteA(window, "open", v9x_gdi_path,
                                    0, 0, SW_SHOWNORMAL) <= 32u) {
                MessageBoxA(window,
                    "V9XGDI.EXE was not found beside V9XSET.EXE.",
                    v9x_window_title, MB_OK | MB_ICONERROR);
            }
            return 0;
        case V9X_ID_CLOSE:
            DestroyWindow(window);
            return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        if (v9x_logo_bitmap != 0) {
            DeleteObject(v9x_logo_bitmap);
            v9x_logo_bitmap = 0;
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(window, message, wparam, lparam);
}

void WINAPI V9xSettingsEntry(void)
{
    HINSTANCE instance = GetModuleHandleA(0);
    WNDCLASSA window_class;
    HWND window;
    MSG message;
    int x;
    int y;

    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = v9x_window_proc;
    window_class.cbClsExtra = 0;
    window_class.cbWndExtra = 0;
    window_class.hInstance = instance;
    window_class.hIcon = LoadIconA(0, IDI_APPLICATION);
    window_class.hCursor = LoadCursorA(0, IDC_ARROW);
    window_class.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    window_class.lpszMenuName = 0;
    window_class.lpszClassName = v9x_class_name;
    if (!RegisterClassA(&window_class)) {
        ExitProcess(1ul);
    }

    v9x_ui_font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    v9x_logo_bitmap = LoadBitmapA(instance,
                                  MAKEINTRESOURCEA(V9X_ID_LOGO_BITMAP));
    v9x_settings_collect(&v9x_status, V9X_VERSION_STRING, V9X_BUILD_ID);
    x = (GetSystemMetrics(SM_CXSCREEN) - 440) / 2;
    y = (GetSystemMetrics(SM_CYSCREEN) - 475) / 2;
    window = CreateWindowExA(WS_EX_DLGMODALFRAME,
                             v9x_class_name, v9x_window_title,
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                             x, y, 440, 475, 0, 0, instance, 0);
    if (window == 0) {
        ExitProcess(2ul);
    }
    ShowWindow(window, SW_SHOWNORMAL);
    UpdateWindow(window);

    while (GetMessageA(&message, 0, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
    ExitProcess((DWORD)message.wParam);
}
