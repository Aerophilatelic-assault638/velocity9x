#ifndef VELOCITY9X_MODE_H
#define VELOCITY9X_MODE_H

#include "velocity9x/status.h"

struct v9x_mode_request {
    v9x_u16 width;
    v9x_u16 height;
    v9x_u16 bits_per_pixel;
    v9x_u16 pitch_alignment;
    v9x_u32 framebuffer_bytes;
};

struct v9x_mode_layout {
    v9x_u32 pitch_bytes;
    v9x_u32 visible_bytes;
    v9x_u32 offscreen_bytes;
};

v9x_status v9x_mode_calculate(const struct v9x_mode_request *request,
                              struct v9x_mode_layout *layout);

#endif
