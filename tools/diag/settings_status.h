/*
 * Shared read-only status collection for the Velocity9x settings surfaces.
 *
 * Both the standalone V9XSET.EXE panel and the Display Properties
 * property-sheet page render the same driver-published INI facts through
 * this module. It performs no hardware access and no registry writes.
 */
#ifndef V9X_SETTINGS_STATUS_H
#define V9X_SETTINGS_STATUS_H

typedef struct v9x_settings_status {
    char adapter_name[96];
    char active_mode[48];
    char core_clock[96];
    char memory_clock[64];
    char clock_detector[64];
    char driver_stage[80];
    char framebuffer_status[96];
    char gdi_status[160];
    char mode_switching[80];
    int live_mode_switching;
    char report[1024];
} V9X_SETTINGS_STATUS;

unsigned long v9x_settings_string_length(const char *text);
void v9x_settings_collect(V9X_SETTINGS_STATUS *status, const char *build_id);
int v9x_settings_copy_report(void *owner_window,
                             const char *caption,
                             const char *report);

#endif
