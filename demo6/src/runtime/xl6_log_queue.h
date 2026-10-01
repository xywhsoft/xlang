#ifndef XL6_LOG_QUEUE_H
#define XL6_LOG_QUEUE_H
#include "src/runtime/xl6_log.h"
#include <limits.h>
#include <string.h>
typedef struct xl6_log_queue_stats { uint64_t values[18]; } xl6_log_queue_stats;
static inline bool log_queue_size(int64_t value,size_t* size)
{ if (value<0 || (uint64_t)value>SIZE_MAX) { xrtSetErrorKind(XERR_RANGE); return false; } *size=(size_t)value; return true; }
#endif
