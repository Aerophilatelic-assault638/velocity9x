#include "velocity9x/build.h"

static const struct v9x_build_identity v9x_identity = {
    (v9x_u16)V9X_VERSION_MAJOR,
    (v9x_u16)V9X_VERSION_MINOR,
    (v9x_u16)V9X_VERSION_PATCH,
    V9X_BUILD_ID
};

const struct v9x_build_identity *v9x_get_build_identity(void)
{
    return &v9x_identity;
}
