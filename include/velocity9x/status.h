#ifndef VELOCITY9X_STATUS_H
#define VELOCITY9X_STATUS_H

#include "velocity9x/types.h"

typedef v9x_u16 v9x_status;

#define V9X_STATUS_OK                  ((v9x_status)0u)
#define V9X_STATUS_INVALID_ARGUMENT    ((v9x_status)1u)
#define V9X_STATUS_UNSUPPORTED         ((v9x_status)2u)
#define V9X_STATUS_INTEGER_OVERFLOW    ((v9x_status)3u)
#define V9X_STATUS_INSUFFICIENT_MEMORY ((v9x_status)4u)
#define V9X_STATUS_INVALID_STATE       ((v9x_status)5u)
#define V9X_STATUS_TIMEOUT             ((v9x_status)6u)
#define V9X_STATUS_IO_ERROR            ((v9x_status)7u)

#endif
