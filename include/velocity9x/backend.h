#ifndef VELOCITY9X_BACKEND_H
#define VELOCITY9X_BACKEND_H

#include "velocity9x/mode.h"
#include "velocity9x/resources.h"

#define V9X_CAP_LINEAR_FRAMEBUFFER ((v9x_u32)0x00000001ul)
#define V9X_CAP_HARDWARE_CURSOR    ((v9x_u32)0x00000002ul)
#define V9X_CAP_SOLID_FILL         ((v9x_u32)0x00000004ul)
#define V9X_CAP_SCREEN_COPY        ((v9x_u32)0x00000008ul)
#define V9X_CAP_CPU_UPLOAD         ((v9x_u32)0x00000010ul)
#define V9X_CAP_VBLANK_STATUS      ((v9x_u32)0x00000020ul)

struct v9x_pci_identity {
    v9x_u16 vendor_id;
    v9x_u16 device_id;
    v9x_u8 revision;
};

struct v9x_backend_state {
    struct v9x_pci_identity pci;
    struct v9x_framebuffer_binding framebuffer;
    v9x_u32 vram_bytes;
    v9x_u32 capabilities;
    v9x_u16 initialized;
    v9x_u16 resources_bound;
};

struct v9x_backend_ops {
    v9x_status (*probe)(struct v9x_backend_state *state,
                        const struct v9x_pci_identity *pci);
    v9x_status (*bind_framebuffer)(
        struct v9x_backend_state *state,
        const struct v9x_pci_bar_resource *bar,
        v9x_u32 detected_vram_bytes,
        v9x_u32 override_vram_bytes);
    v9x_status (*validate_mode)(struct v9x_backend_state *state,
                                const struct v9x_mode_request *request,
                                struct v9x_mode_layout *layout);
    v9x_status (*enter_mode)(struct v9x_backend_state *state,
                             const struct v9x_mode_request *request);
    v9x_status (*leave_mode)(struct v9x_backend_state *state);
    v9x_status (*wait_idle)(struct v9x_backend_state *state,
                            v9x_u32 timeout_ticks);
    v9x_status (*recover)(struct v9x_backend_state *state);
};

#endif
