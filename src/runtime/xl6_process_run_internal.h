#ifndef XL6_PROCESS_RUN_INTERNAL_H
#define XL6_PROCESS_RUN_INTERNAL_H
#include "xl6_process_internal.h"
/* Collector options belong to process_run, not the basic spawn/wait facade. */
typedef struct XL6_ProcessRunOptions {
    xprocessrunoptions options;
    int64_t timeoutMs;
} XL6_ProcessRunOptions;
#endif
