#ifndef VELOCITY9X_BACKEND_REGISTRY_H
#define VELOCITY9X_BACKEND_REGISTRY_H

#include "velocity9x/backend.h"

/* Returns null for hardware that does not have an explicitly supported backend. */
const struct v9x_backend_ops *v9x_backend_for_pci(
    const struct v9x_pci_identity *pci);

#endif
