#ifndef VELOCITY9X_COMPONENTS_H
#define VELOCITY9X_COMPONENTS_H

#include "velocity9x/backend.h"
#include "velocity9x/log.h"

/* Component identifiers used as argument0 in lifecycle log events. */
#define V9X_COMPONENT_DISPLAY16 ((v9x_u32)16ul)
#define V9X_COMPONENT_MINIVDD32 ((v9x_u32)32ul)

struct v9x_component_state {
    struct v9x_logger *logger;
    struct v9x_backend_state *backend;
    v9x_u16 started;
};

v9x_status v9x_display16_start(struct v9x_component_state *component,
                               struct v9x_logger *logger,
                               struct v9x_backend_state *backend);
v9x_status v9x_display16_stop(struct v9x_component_state *component);
v9x_status v9x_minivdd32_start(struct v9x_component_state *component,
                               struct v9x_logger *logger,
                               struct v9x_backend_state *backend);
v9x_status v9x_minivdd32_stop(struct v9x_component_state *component);

#endif
