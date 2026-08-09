#ifndef VELOCITY9X_S3_VIRGE_H
#define VELOCITY9X_S3_VIRGE_H

#include "velocity9x/backend.h"

#define V9X_PCI_VENDOR_S3        ((v9x_u16)0x5333u)
#define V9X_PCI_DEVICE_VIRGE_DX  ((v9x_u16)0x8a01u)

v9x_status v9x_s3_virge_probe(struct v9x_backend_state *state,
                              const struct v9x_pci_identity *pci);
v9x_status v9x_s3_virge_bind_framebuffer(
    struct v9x_backend_state *state,
    const struct v9x_pci_bar_resource *bar,
    v9x_u32 detected_vram_bytes,
    v9x_u32 override_vram_bytes);
v9x_status v9x_s3_virge_validate_mode(struct v9x_backend_state *state,
                                      const struct v9x_mode_request *request,
                                      struct v9x_mode_layout *layout);
const struct v9x_backend_ops *v9x_s3_virge_backend(void);
v9x_status v9x_s3_virge_decode_clock_pll(
    v9x_u8 sr10,
    v9x_u8 sr11,
    struct v9x_clock_info *clocks);

#endif
