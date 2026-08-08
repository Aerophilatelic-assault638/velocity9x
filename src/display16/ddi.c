/*
 * Safe Win9x display-DDI boundary skeleton.
 *
 * The signatures and ordinals are defined by the external Windows 98 DDK.
 * Hardware enable and mode validation deliberately fail until discovery,
 * mapping, recovery, and the DIBENGINE PDEVICE construction are implemented.
 */
/* USER also declares an unrelated SetCursor function in windows.h. */
#define SetCursor V9xUserSetCursor
#include <windows.h>
#undef SetCursor

#define V9X_VALMODE_NO_WRONG_DRIVER ((WORD)1u)

WORD __loadds FAR PASCAL Enable(LPVOID device_info,
                                         WORD action,
                                         LPSTR destination_type,
                                         LPSTR output_file,
                                         LPVOID data)
{
    (void)device_info;
    (void)action;
    (void)destination_type;
    (void)output_file;
    (void)data;
    return FALSE;
}

void __loadds FAR PASCAL Disable(LPVOID destination_device)
{
    (void)destination_device;
}

WORD __loadds FAR PASCAL ReEnable(LPVOID destination_device,
                                           LPVOID gdi_info)
{
    (void)destination_device;
    (void)gdi_info;
    return FALSE;
}

WORD __loadds FAR PASCAL ValidateMode(LPVOID display_info)
{
    (void)display_info;
    return V9X_VALMODE_NO_WRONG_DRIVER;
}

void __loadds FAR PASCAL SetCursor(LPVOID cursor_shape)
{
    (void)cursor_shape;
}

void __loadds FAR PASCAL MoveCursor(WORD absolute_x,
                                              WORD absolute_y)
{
    (void)absolute_x;
    (void)absolute_y;
}

void __loadds FAR PASCAL CheckCursor(void)
{
}

/* VDD callback; this must remain in the fixed _TEXT segment. */
void __loadds FAR PASCAL ResetHiResMode(void)
{
}
