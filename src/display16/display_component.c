#include "velocity9x/components.h"

v9x_status v9x_display16_start(struct v9x_component_state *component,
                               struct v9x_logger *logger,
                               struct v9x_backend_state *backend)
{
    if (component == 0 || backend == 0) {
        return V9X_STATUS_INVALID_ARGUMENT;
    }
    if (component->started != V9X_FALSE) {
        return V9X_STATUS_INVALID_STATE;
    }

    component->logger = logger;
    component->backend = backend;
    component->started = V9X_TRUE;
    (void)v9x_log_emit(logger, V9X_EVENT_COMPONENT_INIT, V9X_STATUS_OK,
                       V9X_COMPONENT_DISPLAY16, 0ul, 0ul, 0ul);
    return V9X_STATUS_OK;
}

v9x_status v9x_display16_stop(struct v9x_component_state *component)
{
    if (component == 0) {
        return V9X_STATUS_INVALID_ARGUMENT;
    }
    if (component->started == V9X_FALSE) {
        return V9X_STATUS_INVALID_STATE;
    }

    (void)v9x_log_emit(component->logger, V9X_EVENT_COMPONENT_STOP,
                       V9X_STATUS_OK, V9X_COMPONENT_DISPLAY16, 0ul, 0ul, 0ul);
    component->started = V9X_FALSE;
    component->backend = 0;
    component->logger = 0;
    return V9X_STATUS_OK;
}
