#ifndef XLIR_CODE_FINALIZER_DROP_H
#define XLIR_CODE_FINALIZER_DROP_H
#include "src/runtime/xl6_code_lease.h"
typedef struct FinalizerHooks {
    void* context;
    void (*finalize)(struct xvalue*, void*);
    bool (*trace)(const void*, XL6_ModuleOwnershipVisit, void*);
    void (*release)(void*);
    void (*field)(void*);
    unsigned finalizedReturns, releasedReturns, fieldReturns;
} FinalizerHooks;
#endif
