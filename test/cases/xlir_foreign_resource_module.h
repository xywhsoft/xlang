#ifndef XLIR_FOREIGN_RESOURCE_MODULE_TEST_H
#define XLIR_FOREIGN_RESOURCE_MODULE_TEST_H
#include "src/runtime/xl6_foreign_resource.h"
#include "src/runtime/xl6_closure.h"
#include <stdatomic.h>
typedef struct ForeignModulePayload { int value; atomic_int* completed; } ForeignModulePayload;
typedef struct ForeignModuleHooks ForeignModuleHooks;
typedef bool (*ForeignModuleProducer)(ForeignModuleHooks*,const xrtcallframe*);
struct ForeignModuleHooks {
    void (*dispose)(const xrtcallframe*);
    ForeignModuleProducer producer;
    unsigned stages;
};
#endif
