#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#ifndef V9X_BUILD_ID
#define V9X_BUILD_ID "local"
#endif

#define V9X_ID_COPY_REPORT  1001
#define V9X_ID_RECOVERY     1002
#define V9X_ID_CLOSE        1003
#define V9X_ID_GDI_TEST     1004

static const char v9x_class_name[] = "Velocity9xSettingsWindow";
static const char v9x_window_title[] = "Velocity9x Settings";
static const char v9x_report[] =
    "Velocity9x settings report\r\n"
    "Build: " V9X_BUILD_ID "\r\n"
    "Target: S3 ViRGE/DX 86C375 (PCI 5333:8A01)\r\n"
    "Resolutions: 640x480, 800x600, 1024x768\r\n"
    "Color depths: 8-bit indexed; 16-bit RGB 5:6:5\r\n"
    "Rendering: Windows DIB Engine (software)\r\n"
    "Acceleration: disabled\r\n"
    "Mini-VDD callbacks: master VDD defaults\r\n";

static HFONT v9x_ui_font;
static char v9x_recovery_path[MAX_PATH];
static char v9x_gdi_path[MAX_PATH];

static DWORD v9x_string_length(const char *text)
{
    DWORD length = 0ul;
    while (text[length] != '\0') {
        ++length;
    }
    return length;
}

static void v9x_set_font(HWND control)
{
    SendMessageA(control, WM_SETFONT, (WPARAM)v9x_ui_font, TRUE);
}

static BOOL v9x_find_sibling(char *path, const char *file_name)
{
    DWORD length = GetModuleFileNameA(0, path, MAX_PATH);
    DWORD file_length = v9x_string_length(file_name);
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

static void v9x_copy_report(HWND window)
{
    DWORD length = v9x_string_length(v9x_report) + 1ul;
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, length);
    char *destination;
    DWORD index;

    if (memory == 0) {
        MessageBoxA(window, "Could not allocate the report buffer.",
                    v9x_window_title, MB_OK | MB_ICONERROR);
        return;
    }
    destination = (char *)GlobalLock(memory);
    if (destination == 0) {
        GlobalFree(memory);
        return;
    }
    for (index = 0ul; index < length; ++index) {
        destination[index] = v9x_report[index];
    }
    GlobalUnlock(memory);

    if (!OpenClipboard(window)) {
        GlobalFree(memory);
        MessageBoxA(window, "Could not open the clipboard.", v9x_window_title,
                    MB_OK | MB_ICONERROR);
        return;
    }
    EmptyClipboard();
    if (SetClipboardData(CF_TEXT, memory) == 0) {
        CloseClipboard();
        GlobalFree(memory);
        MessageBoxA(window, "Could not copy the report.", v9x_window_title,
                    MB_OK | MB_ICONERROR);
        return;
    }
    CloseClipboard();
    MessageBoxA(window, "The diagnostic report is on the clipboard.",
                v9x_window_title, MB_OK | MB_ICONINFORMATION);
}

static void v9x_create_controls(HWND window)
{
    HWND control;

    control = v9x_control(window, "STATIC",
        "Engineering bring-up build - conservative features are locked on.",
        SS_CENTER, 16, 14, 388, 20, 0);

    (void)v9x_control(window, "BUTTON", "Display",
                      BS_GROUPBOX, 14, 42, 392, 102, 0);
    (void)v9x_control(window, "STATIC", "Adapter:",
                      SS_LEFT, 28, 63, 76, 18, 0);
    (void)v9x_control(window, "STATIC", "S3 ViRGE/DX 86C375 (5333:8A01)",
                      SS_LEFT, 110, 63, 278, 18, 0);
    (void)v9x_control(window, "STATIC", "Resolutions:",
                      SS_LEFT, 28, 87, 76, 18, 0);
    (void)v9x_control(window, "STATIC", "640x480, 800x600, 1024x768",
                      SS_LEFT, 110, 87, 278, 18, 0);
    (void)v9x_control(window, "STATIC", "Colour depths:",
                      SS_LEFT, 28, 111, 76, 18, 0);
    (void)v9x_control(window, "STATIC", "8-bit indexed, 16-bit RGB 5:6:5",
                      SS_LEFT, 110, 111, 278, 18, 0);

    (void)v9x_control(window, "BUTTON", "Rendering and safety",
                      BS_GROUPBOX, 14, 154, 392, 100, 0);
    control = v9x_control(window, "BUTTON", "Windows DIB Engine rendering",
                          BS_AUTOCHECKBOX | WS_DISABLED, 28, 176, 250, 20, 0);
    SendMessageA(control, BM_SETCHECK, BST_CHECKED, 0);
    control = v9x_control(window, "BUTTON", "Hardware acceleration",
                          BS_AUTOCHECKBOX | WS_DISABLED, 28, 199, 250, 20, 0);
    SendMessageA(control, BM_SETCHECK, BST_UNCHECKED, 0);
    control = v9x_control(window, "BUTTON", "Extended mode switching",
                          BS_AUTOCHECKBOX | WS_DISABLED, 28, 222, 190, 20, 0);
    SendMessageA(control, BM_SETCHECK, BST_UNCHECKED, 0);
    (void)v9x_control(window, "STATIC", "Build: " V9X_BUILD_ID,
                      SS_RIGHT, 228, 224, 160, 18, 0);

    (void)v9x_control(window, "BUTTON", "Copy report",
                      BS_PUSHBUTTON | WS_TABSTOP, 15, 270, 92, 28,
                      V9X_ID_COPY_REPORT);
    (void)v9x_control(window, "BUTTON", "Run GDI test",
                      BS_PUSHBUTTON | WS_TABSTOP, 113, 270, 94, 28,
                      V9X_ID_GDI_TEST);
    (void)v9x_control(window, "BUTTON", "Recovery guide",
                      BS_PUSHBUTTON | WS_TABSTOP, 213, 270, 104, 28,
                      V9X_ID_RECOVERY);
    (void)v9x_control(window, "BUTTON", "Close",
                      BS_DEFPUSHBUTTON | WS_TABSTOP, 323, 270, 83, 28,
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
            v9x_copy_report(window);
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
    x = (GetSystemMetrics(SM_CXSCREEN) - 430) / 2;
    y = (GetSystemMetrics(SM_CYSCREEN) - 340) / 2;
    window = CreateWindowExA(WS_EX_DLGMODALFRAME,
                             v9x_class_name, v9x_window_title,
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                             x, y, 430, 340, 0, 0, instance, 0);
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
