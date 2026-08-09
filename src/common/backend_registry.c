#include "velocity9x/backend_registry.h"
#include "velocity9x/matrox_millennium2.h"
#include "velocity9x/s3_virge.h"

const struct v9x_backend_ops *v9x_backend_for_pci(
    const struct v9x_pci_identity *pci)
{
    if (pci == 0) {
        return 0;
    }
    if (pci->vendor_id == V9X_PCI_VENDOR_S3 &&
        pci->device_id == V9X_PCI_DEVICE_VIRGE_DX) {
        return v9x_s3_virge_backend();
    }
    if (pci->vendor_id == V9X_PCI_VENDOR_MATROX &&
        pci->device_id == V9X_PCI_DEVICE_MILLENNIUM_II) {
        return v9x_matrox_millennium2_backend();
    }
    return 0;
}
