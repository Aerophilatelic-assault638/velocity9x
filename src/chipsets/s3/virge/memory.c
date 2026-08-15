#include "velocity9x/s3_virge.h"

/*
 * Installed video memory, from CRTC extended register 36h bits 7:5.
 *
 * The S3 Trio32/64 and the ViRGE/DX share this encoding. Only the codes those
 * two families actually emit are decoded; 1, 2 and 5 are used by other S3
 * parts (ViRGE/VX and Trio3D/2X encode the same sizes differently), so they
 * are reported as unsupported rather than guessed at. A wrong answer here
 * would be worse than no answer: the settings page prints it as fact.
 */
#define V9X_S3_MEMORY_CODE_SHIFT 5u
#define V9X_S3_MEMORY_CODE_MASK  0x07u

v9x_status v9x_s3_virge_decode_memory_size(v9x_u8 cr36, v9x_u32 *bytes)
{
    v9x_u8 code;

    if (bytes == 0) {
        return V9X_STATUS_INVALID_ARGUMENT;
    }
    *bytes = 0ul;

    code = (v9x_u8)((cr36 >> V9X_S3_MEMORY_CODE_SHIFT) &
                    V9X_S3_MEMORY_CODE_MASK);
    switch (code) {
    case 0u: *bytes = 4ul * 1024ul * 1024ul; break;
    case 3u: *bytes = 8ul * 1024ul * 1024ul; break;
    case 4u: *bytes = 2ul * 1024ul * 1024ul; break;
    case 6u: *bytes = 1ul * 1024ul * 1024ul; break;
    case 7u: *bytes = 512ul * 1024ul; break;
    default: return V9X_STATUS_UNSUPPORTED;
    }
    return V9X_STATUS_OK;
}
