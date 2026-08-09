/*
 * Win16 NE global initializer for the active DIB Engine display driver.
 * Windows 9x display DRVs use a DriverInit entry point rather than the
 * ordinary per-instance LibMain convention used by general Win16 DLLs.
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

/* The display-driver loader supplies heap size in CX, module handle in DI,
 * and the command line in ES:SI. This is the entry contract used by the
 * Windows 98 DDK display samples and vmdisp9x. */
#pragma aux DriverInit parm [cx] [di] [es si]

#pragma off (unreferenced)
UINT FAR DriverInit(UINT heap_size,
                    UINT module,
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
