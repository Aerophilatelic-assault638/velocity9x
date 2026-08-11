#ifndef VELOCITY9X_BUILD_H
#define VELOCITY9X_BUILD_H

#include "velocity9x/types.h"

#ifndef V9X_BUILD_ID
#define V9X_BUILD_ID "dev-unversioned"
#endif

#define V9X_VERSION_MAJOR 0u
#define V9X_VERSION_MINOR 2u
#define V9X_VERSION_PATCH 0u
#define V9X_VERSION_STRING "0.2"

struct v9x_build_identity {
    v9x_u16 major;
    v9x_u16 minor;
    v9x_u16 patch;
    const char *build_id;
};

const struct v9x_build_identity *v9x_get_build_identity(void);

#endif
