#ifndef XLIR_GENERATOR_MANAGED_DLL_H
#define XLIR_GENERATOR_MANAGED_DLL_H
#include "src/runtime/xl6_generator.h"
typedef struct ManagedDllHooks {
    const XL6_GeneratorRuntimeV1* api;
    void (*during)(void*);
    void* context;
    unsigned entered,returned;
} ManagedDllHooks;
typedef struct ManagedDllFrame {ManagedDllHooks* hooks;} ManagedDllFrame;
#endif
