#ifndef VELOCITY9X_LOG_H
#define VELOCITY9X_LOG_H

#include "velocity9x/status.h"

#define V9X_LOG_MAGIC   ((v9x_u32)0x4c583956ul)
#define V9X_LOG_VERSION ((v9x_u16)1u)

#define V9X_EVENT_BUILD_ID       ((v9x_u16)1u)
#define V9X_EVENT_COMPONENT_INIT ((v9x_u16)2u)
#define V9X_EVENT_COMPONENT_STOP ((v9x_u16)3u)
#define V9X_EVENT_DEVICE_PROBE   ((v9x_u16)4u)
#define V9X_EVENT_MODE_VALIDATE  ((v9x_u16)5u)
#define V9X_EVENT_TIMEOUT        ((v9x_u16)6u)
#define V9X_EVENT_RECOVERY       ((v9x_u16)7u)

struct v9x_log_record {
    v9x_u32 magic;
    v9x_u16 version;
    v9x_u16 size;
    v9x_u32 sequence;
    v9x_u16 event_id;
    v9x_u16 status;
    v9x_u32 argument0;
    v9x_u32 argument1;
    v9x_u32 argument2;
    v9x_u32 argument3;
};

typedef char v9x_assert_log_record_is_32[
    (sizeof(struct v9x_log_record) == 32) ? 1 : -1
];

typedef v9x_status (*v9x_log_sink)(void *context,
                                   const struct v9x_log_record *record);

struct v9x_logger {
    v9x_log_sink sink;
    void *context;
    v9x_u32 next_sequence;
};

void v9x_log_init(struct v9x_logger *logger,
                  v9x_log_sink sink,
                  void *context);
v9x_status v9x_log_emit(struct v9x_logger *logger,
                        v9x_u16 event_id,
                        v9x_status status,
                        v9x_u32 argument0,
                        v9x_u32 argument1,
                        v9x_u32 argument2,
                        v9x_u32 argument3);

#endif
