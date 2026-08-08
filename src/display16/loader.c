/*
 * Win16 NE loader for the first active DIB Engine display candidate.
 * windows.h is supplied by the external Open Watcom toolchain.
 */
#include <windows.h>

#include "velocity9x/build.h"
#include "velocity9x/components.h"

static struct v9x_logger v9x_display_logger;
static struct v9x_backend_state v9x_display_backend;
static struct v9x_component_state v9x_display_component;
static const struct v9x_build_identity *v9x_display_build_identity;

extern void v9x_display_boot_log(void);

#pragma off (unreferenced)
BOOL FAR PASCAL LibMain(HINSTANCE instance,
                        WORD data_segment,
                        WORD heap_size,
                        LPSTR command_line)
#pragma on (unreferenced)
{
    v9x_display_boot_log();
    v9x_display_build_identity = v9x_get_build_identity();
    v9x_log_init(&v9x_display_logger, 0, 0);
    return v9x_display16_start(&v9x_display_component,
                               &v9x_display_logger,
                               &v9x_display_backend) == V9X_STATUS_OK;
}

#pragma off (unreferenced)
int FAR PASCAL WEP(int reason)
#pragma on (unreferenced)
{
    if (v9x_display_component.started != V9X_FALSE) {
        (void)v9x_display16_stop(&v9x_display_component);
    }
    v9x_display_build_identity = 0;
    return 1;
}
