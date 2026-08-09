#include "velocity9x/s3_virge.h"

#define V9X_S3_REFERENCE_CLOCK_KHZ ((v9x_u32)14318ul)
#define V9X_S3_MIN_CLOCK_KHZ       ((v9x_u32)10000ul)
#define V9X_S3_MAX_CLOCK_KHZ       ((v9x_u32)100000ul)

v9x_status v9x_s3_virge_decode_clock_pll(
    v9x_u8 sr10,
    v9x_u8 sr11,
    struct v9x_clock_info *clocks)
{
    v9x_u32 divider;
    v9x_u32 multiplier;
    v9x_u32 memory_khz;

    if (clocks == 0) {
        return V9X_STATUS_INVALID_ARGUMENT;
    }
    clocks->core_clock_khz = 0ul;
    clocks->memory_clock_khz = 0ul;
    clocks->flags = 0u;

    /* ViRGE SR10 holds N1 in bits 0..4 and N2 in bits 5..6. SR11
     * holds M in bits 0..6. The graphics engine shares MCLK rather than
     * exposing a distinct programmable core clock. */
    divider = ((v9x_u32)(sr10 & 0x1fu) + 2ul) <<
              ((v9x_u32)(sr10 >> 5) & 0x03ul);
    multiplier = (v9x_u32)(sr11 & 0x7fu) + 2ul;
    memory_khz = (V9X_S3_REFERENCE_CLOCK_KHZ * multiplier +
                  divider / 2ul) / divider;
    if (memory_khz < V9X_S3_MIN_CLOCK_KHZ ||
        memory_khz > V9X_S3_MAX_CLOCK_KHZ) {
        return V9X_STATUS_UNSUPPORTED;
    }

    clocks->memory_clock_khz = memory_khz;
    clocks->core_clock_khz = memory_khz;
    clocks->flags = V9X_CLOCK_CORE_VALID | V9X_CLOCK_MEMORY_VALID |
                    V9X_CLOCK_CORE_SHARED_MCLK;
    return V9X_STATUS_OK;
}
