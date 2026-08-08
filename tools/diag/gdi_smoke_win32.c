#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifndef V9X_BUILD_ID
#define V9X_BUILD_ID "local"
#endif

#define V9X_TEST_MESSAGE (WM_APP + 1u)

static const char v9x_class_name[] = "Velocity9xGdiSmokeWindow";
static const char v9x_title[] = "Velocity9x GDI framebuffer test";
static const char v9x_full_title[] =
    "Velocity9x GDI framebuffer test - " V9X_BUILD_ID;
static int v9x_test_posted;

static int v9x_string_length(const char *text)
{
    int length = 0;
    while (text[length] != '\0') {
        ++length;
    }
    return length;
}

static int v9x_channel_near(BYTE actual, BYTE expected)
{
    int difference = (int)actual - (int)expected;
    if (difference < 0) {
        difference = -difference;
    }
    return difference <= 64;
}

static int v9x_color_near(COLORREF actual, COLORREF expected)
{
    return actual != CLR_INVALID &&
           v9x_channel_near(GetRValue(actual), GetRValue(expected)) &&
           v9x_channel_near(GetGValue(actual), GetGValue(expected)) &&
           v9x_channel_near(GetBValue(actual), GetBValue(expected));
}

static void v9x_paint_pattern(HWND window, HDC display)
{
    static const COLORREF colors[] = {
        RGB(0, 0, 0), RGB(255, 255, 255), RGB(255, 0, 0),
        RGB(0, 255, 0), RGB(0, 0, 255), RGB(255, 255, 0),
        RGB(0, 255, 255), RGB(255, 0, 255)
    };
    RECT client;
    RECT area;
    HBRUSH brush;
    HPEN pen;
    HPEN old_pen;
    HDC memory;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    WORD index;

    GetClientRect(window, &client);
    FillRect(display, &client, (HBRUSH)(COLOR_WINDOW + 1));
    SetBkMode(display, TRANSPARENT);
    SetTextColor(display, RGB(0, 0, 0));
    TextOutA(display, 18, 16,
        "DIB Engine screen-path test: colors, lines, text and blits",
        v9x_string_length(
          "DIB Engine screen-path test: colors, lines, text and blits"));

    for (index = 0u; index < 8u; ++index) {
        area.left = 18 + (int)index * 54;
        area.top = 48;
        area.right = area.left + 48;
        area.bottom = 112;
        brush = CreateSolidBrush(colors[index]);
        FillRect(display, &area, brush);
        DeleteObject(brush);
    }

    pen = CreatePen(PS_SOLID, 2, RGB(0, 0, 128));
    old_pen = (HPEN)SelectObject(display, pen);
    MoveToEx(display, 18, 136, 0);
    LineTo(display, 450, 136);
    MoveToEx(display, 18, 145, 0);
    LineTo(display, 450, 205);
    SelectObject(display, old_pen);
    DeleteObject(pen);

    brush = CreateSolidBrush(RGB(0, 0, 255));
    memory = CreateCompatibleDC(display);
    bitmap = CreateCompatibleBitmap(display, 96, 64);
    old_bitmap = (HBITMAP)SelectObject(memory, bitmap);
    area.left = 0;
    area.top = 0;
    area.right = 96;
    area.bottom = 64;
    FillRect(memory, &area, brush);
    DeleteObject(brush);
    brush = CreateSolidBrush(RGB(255, 255, 255));
    area.left = 38;
    area.right = 58;
    FillRect(memory, &area, brush);
    area.left = 0;
    area.top = 22;
    area.right = 96;
    area.bottom = 42;
    FillRect(memory, &area, brush);
    DeleteObject(brush);

    BitBlt(display, 18, 226, 96, 64, memory, 0, 0, SRCCOPY);
    StretchBlt(display, 142, 216, 144, 84, memory, 0, 0, 96, 64, SRCCOPY);
    SelectObject(memory, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(memory);

    SetPixel(display, 330, 250, RGB(255, 0, 0));
    SetPixel(display, 331, 250, RGB(255, 0, 0));
    SetPixel(display, 330, 251, RGB(255, 0, 0));
    SetPixel(display, 331, 251, RGB(255, 0, 0));
    TextOutA(display, 18, 315,
        "Leave this window visible while checking the software cursor.",
        v9x_string_length(
          "Leave this window visible while checking the software cursor."));

    if (!v9x_test_posted) {
        v9x_test_posted = 1;
        PostMessageA(window, V9X_TEST_MESSAGE, 0, 0);
    }
}

static void v9x_check_pixels(HWND window)
{
    HDC display = GetDC(window);
    int passed;

    passed = v9x_color_near(GetPixel(display, 28, 72), RGB(0, 0, 0)) &&
             v9x_color_near(GetPixel(display, 82, 72), RGB(255, 255, 255)) &&
             v9x_color_near(GetPixel(display, 136, 72), RGB(255, 0, 0)) &&
             v9x_color_near(GetPixel(display, 28, 236), RGB(0, 0, 255)) &&
             v9x_color_near(GetPixel(display, 330, 250), RGB(255, 0, 0));
    ReleaseDC(window, display);

    MessageBoxA(window,
        passed ?
          "PASS: display writes, BitBlt and pixel readback are coherent. "
          "Move the cursor around the pattern, then close the window." :
          "FAIL: one or more framebuffer pixels did not read back as drawn. "
          "Keep the window visible and report the color corruption.",
        v9x_title, MB_OK | (passed ? MB_ICONINFORMATION : MB_ICONERROR));
}

static LRESULT CALLBACK v9x_window_proc(HWND window,
                                        UINT message,
                                        WPARAM wparam,
                                        LPARAM lparam)
{
    (void)wparam;
    (void)lparam;
    switch (message) {
    case WM_PAINT:
        {
            PAINTSTRUCT paint;
            HDC display = BeginPaint(window, &paint);
            v9x_paint_pattern(window, display);
            EndPaint(window, &paint);
        }
        return 0;
    case V9X_TEST_MESSAGE:
        v9x_check_pixels(window);
        return 0;
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(window, message, wparam, lparam);
}

void WINAPI V9xGdiSmokeEntry(void)
{
    HINSTANCE instance = GetModuleHandleA(0);
    WNDCLASSA window_class;
    HWND window;
    MSG message;

    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = v9x_window_proc;
    window_class.cbClsExtra = 0;
    window_class.cbWndExtra = 0;
    window_class.hInstance = instance;
    window_class.hIcon = LoadIconA(0, IDI_APPLICATION);
    window_class.hCursor = LoadCursorA(0, IDC_ARROW);
    window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    window_class.lpszMenuName = 0;
    window_class.lpszClassName = v9x_class_name;
    if (!RegisterClassA(&window_class)) {
        ExitProcess(1ul);
    }

    window = CreateWindowExA(WS_EX_DLGMODALFRAME, v9x_class_name,
        v9x_full_title,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        60, 40, 490, 390, 0, 0, instance, 0);
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
