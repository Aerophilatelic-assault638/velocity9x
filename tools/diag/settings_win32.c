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
#define V9X_ID_LOGO_BITMAP  101

static const char v9x_class_name[] = "Velocity9xSettingsWindow";
static const char v9x_window_title[] = "Velocity9x Settings";
static HFONT v9x_ui_font;
static HBITMAP v9x_logo_bitmap;
static char v9x_recovery_path[MAX_PATH];
static char v9x_gdi_path[MAX_PATH];
static char v9x_active_mode[48];
static char v9x_driver_stage[80];
static char v9x_framebuffer_status[96];
static char v9x_gdi_status[160];
static char v9x_report[1024];

static DWORD v9x_string_length(const char *text)
{
    DWORD length = 0ul;
    while (text[length] != '\0') {
        ++length;
    }
    return length;
}

static void v9x_append(char *destination, DWORD capacity, const char *text)
{
    DWORD offset = v9x_string_length(destination);
    DWORD index = 0ul;
    while (text[index] != '\0' && offset + index + 1ul < capacity) {
        destination[offset + index] = text[index];
        ++index;
    }
    destination[offset + index] = '\0';
}

static void v9x_append_uint(char *destination, DWORD capacity, UINT value)
{
    char reverse[12];
    char number[12];
    int count = 0;
    int index;
    do {
        reverse[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0u);
    for (index = 0; index < count; ++index) {
        number[index] = reverse[count - index - 1];
    }
    number[count] = '\0';
    v9x_append(destination, capacity, number);
}

static void v9x_build_runtime_status(void)
{
    HDC display = GetDC(0);
    UINT width = (UINT)GetDeviceCaps(display, HORZRES);
    UINT height = (UINT)GetDeviceCaps(display, VERTRES);
    UINT bits = (UINT)(GetDeviceCaps(display, BITSPIXEL) *
                       GetDeviceCaps(display, PLANES));
    char result[16];
    char test_build[80];
    char test_width[12];
    char test_height[12];
    char test_bits[12];
    ReleaseDC(0, display);

    v9x_active_mode[0] = '\0';
    v9x_append_uint(v9x_active_mode, sizeof(v9x_active_mode), width);
    v9x_append(v9x_active_mode, sizeof(v9x_active_mode), " x ");
    v9x_append_uint(v9x_active_mode, sizeof(v9x_active_mode), height);
    v9x_append(v9x_active_mode, sizeof(v9x_active_mode), " x ");
    v9x_append_uint(v9x_active_mode, sizeof(v9x_active_mode), bits);
    v9x_append(v9x_active_mode, sizeof(v9x_active_mode), " bpp");

    GetPrivateProfileStringA("Velocity9x", "Stage", "not recorded",
                             v9x_driver_stage, sizeof(v9x_driver_stage),
                             "C:\\V9XBOOT.INI");
    v9x_framebuffer_status[0] = '\0';
    if (lstrcmpiA(v9x_driver_stage, "enable-ok") == 0) {
        v9x_append(v9x_framebuffer_status, sizeof(v9x_framebuffer_status),
                   "Active - S3 linear aperture mapped");
    } else {
        v9x_append(v9x_framebuffer_status, sizeof(v9x_framebuffer_status),
                   "Not confirmed - stage: ");
        v9x_append(v9x_framebuffer_status, sizeof(v9x_framebuffer_status),
                   v9x_driver_stage);
    }

    GetPrivateProfileStringA("Velocity9xGDI", "Result", "not run", result,
                             sizeof(result), "C:\\V9XGDI.INI");
    GetPrivateProfileStringA("Velocity9xGDI", "Build", "unknown", test_build,
                             sizeof(test_build), "C:\\V9XGDI.INI");
    GetPrivateProfileStringA("Velocity9xGDI", "Width", "?", test_width,
                             sizeof(test_width), "C:\\V9XGDI.INI");
    GetPrivateProfileStringA("Velocity9xGDI", "Height", "?", test_height,
                             sizeof(test_height), "C:\\V9XGDI.INI");
    GetPrivateProfileStringA("Velocity9xGDI", "BitsPerPixel", "?", test_bits,
                             sizeof(test_bits), "C:\\V9XGDI.INI");
    v9x_gdi_status[0] = '\0';
    v9x_append(v9x_gdi_status, sizeof(v9x_gdi_status), result);
    if (lstrcmpiA(result, "not run") != 0) {
        v9x_append(v9x_gdi_status, sizeof(v9x_gdi_status), " - ");
        v9x_append(v9x_gdi_status, sizeof(v9x_gdi_status), test_width);
        v9x_append(v9x_gdi_status, sizeof(v9x_gdi_status), "x");
        v9x_append(v9x_gdi_status, sizeof(v9x_gdi_status), test_height);
        v9x_append(v9x_gdi_status, sizeof(v9x_gdi_status), "x");
        v9x_append(v9x_gdi_status, sizeof(v9x_gdi_status), test_bits);
        v9x_append(v9x_gdi_status, sizeof(v9x_gdi_status), " (");
        v9x_append(v9x_gdi_status, sizeof(v9x_gdi_status), test_build);
        v9x_append(v9x_gdi_status, sizeof(v9x_gdi_status), ")");
    }

    v9x_report[0] = '\0';
    v9x_append(v9x_report, sizeof(v9x_report),
        "Velocity9x settings report\r\nBuild: " V9X_BUILD_ID
        "\r\nTarget: S3 ViRGE/DX 86C375 (PCI 5333:8A01)\r\nActive mode: ");
    v9x_append(v9x_report, sizeof(v9x_report), v9x_active_mode);
    v9x_append(v9x_report, sizeof(v9x_report), "\r\nDriver stage: ");
    v9x_append(v9x_report, sizeof(v9x_report), v9x_driver_stage);
    v9x_append(v9x_report, sizeof(v9x_report), "\r\nFramebuffer: ");
    v9x_append(v9x_report, sizeof(v9x_report), v9x_framebuffer_status);
    v9x_append(v9x_report, sizeof(v9x_report), "\r\nLast GDI test: ");
    v9x_append(v9x_report, sizeof(v9x_report), v9x_gdi_status);
    v9x_append(v9x_report, sizeof(v9x_report),
        "\r\nSupported modes: 640x480, 800x600, 1024x768 at 8/16 bpp"
        "\r\nRendering: Windows DIB Engine (software)"
        "\r\nAcceleration: disabled"
        "\r\nMini-VDD callbacks: master VDD defaults\r\n");
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
    (void)v9x_control(window, "STATIC", "S3 ViRGE/DX 86C375 (5333:8A01)",
                      SS_LEFT, 110, 127, 278, 18, 0);
    (void)v9x_control(window, "STATIC", "Resolutions:",
                      SS_LEFT, 28, 151, 76, 18, 0);
    (void)v9x_control(window, "STATIC", "640x480, 800x600, 1024x768",
                      SS_LEFT, 110, 151, 278, 18, 0);
    (void)v9x_control(window, "STATIC", "Colour depths:",
                      SS_LEFT, 28, 175, 76, 18, 0);
    (void)v9x_control(window, "STATIC", "8-bit indexed, 16-bit RGB 5:6:5",
                      SS_LEFT, 110, 175, 278, 18, 0);
    (void)v9x_control(window, "STATIC", "Active mode:",
                      SS_LEFT, 28, 199, 76, 18, 0);
    (void)v9x_control(window, "STATIC", v9x_active_mode,
                      SS_LEFT, 110, 199, 278, 18, 0);

    (void)v9x_control(window, "BUTTON", "Rendering and safety",
                      BS_GROUPBOX, 14, 242, 402, 86, 0);
    control = v9x_control(window, "BUTTON", "Windows DIB Engine rendering",
                          BS_AUTOCHECKBOX | WS_DISABLED, 28, 262, 250, 20, 0);
    SendMessageA(control, BM_SETCHECK, BST_CHECKED, 0);
    control = v9x_control(window, "BUTTON", "Hardware acceleration",
                          BS_AUTOCHECKBOX | WS_DISABLED, 28, 285, 250, 20, 0);
    SendMessageA(control, BM_SETCHECK, BST_UNCHECKED, 0);
    control = v9x_control(window, "BUTTON", "Extended mode switching",
                          BS_AUTOCHECKBOX | WS_DISABLED, 28, 308, 190, 20, 0);
    SendMessageA(control, BM_SETCHECK, BST_UNCHECKED, 0);
    (void)v9x_control(window, "STATIC", "Build: " V9X_BUILD_ID,
                      SS_RIGHT, 218, 310, 180, 18, 0);

    (void)v9x_control(window, "BUTTON", "Runtime diagnostics",
                      BS_GROUPBOX, 14, 338, 402, 62, 0);
    (void)v9x_control(window, "STATIC", "Driver / framebuffer:",
                      SS_LEFT, 28, 358, 116, 18, 0);
    (void)v9x_control(window, "STATIC", v9x_framebuffer_status,
                      SS_LEFT, 148, 358, 250, 18, 0);
    (void)v9x_control(window, "STATIC", "Last GDI test:",
                      SS_LEFT, 28, 380, 116, 18, 0);
    (void)v9x_control(window, "STATIC", v9x_gdi_status,
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
    v9x_build_runtime_status();
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
