#include "velocity9x/log.h"

void v9x_log_init(struct v9x_logger *logger,
                  v9x_log_sink sink,
                  void *context)
{
    if (logger == 0) {
        return;
    }
    logger->sink = sink;
    logger->context = context;
    logger->next_sequence = 0ul;
}

v9x_status v9x_log_emit(struct v9x_logger *logger,
                        v9x_u16 event_id,
                        v9x_status status,
                        v9x_u32 argument0,
                        v9x_u32 argument1,
                        v9x_u32 argument2,
                        v9x_u32 argument3)
{
    struct v9x_log_record record;

    if (logger == 0 || logger->sink == 0) {
        return V9X_STATUS_OK;
    }

    record.magic = V9X_LOG_MAGIC;
    record.version = V9X_LOG_VERSION;
    record.size = (v9x_u16)sizeof(record);
    record.sequence = logger->next_sequence++;
    record.event_id = event_id;
    record.status = status;
    record.argument0 = argument0;
    record.argument1 = argument1;
    record.argument2 = argument2;
    record.argument3 = argument3;
    return logger->sink(logger->context, &record);
}
