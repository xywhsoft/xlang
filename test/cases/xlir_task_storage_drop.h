#ifndef XLIR_TASK_STORAGE_DROP_H
#define XLIR_TASK_STORAGE_DROP_H
#include "src/runtime/xl6_task.h"
typedef struct TaskStorageHooks {
    void (*during)(void*,int); void* context;
    unsigned runs,drops,returned;
} TaskStorageHooks;
typedef struct TaskStoragePayload { TaskStorageHooks* hooks; } TaskStoragePayload;
#endif
