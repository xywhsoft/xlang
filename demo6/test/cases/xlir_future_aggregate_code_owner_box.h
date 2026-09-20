#ifndef XLIR_FUTURE_AGGREGATE_CODE_OWNER_BOX_H
#define XLIR_FUTURE_AGGREGATE_CODE_OWNER_BOX_H
#include <stdint.h>
struct xvalue;
typedef struct AggregateBoxHooks {
    struct xvalue* (*box)(int64_t);
    void (*during)(void*, unsigned);
    void* context;
    unsigned calls, children, returned;
} AggregateBoxHooks;
typedef struct AggregateBoxPayload { int64_t value; AggregateBoxHooks* hooks; } AggregateBoxPayload;
#endif
