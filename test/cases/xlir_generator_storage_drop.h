#ifndef XLIR_GENERATOR_STORAGE_DROP_H
#define XLIR_GENERATOR_STORAGE_DROP_H
#include "src/api/xl_foreign_error.h"
#include "src/runtime/xl6_generator.h"
typedef struct GeneratorStorageHooks {
    const XL6_GeneratorRuntimeV1* api;
    void (*during)(void*);
    void* context;
    unsigned resumed, dropped, returned;
} GeneratorStorageHooks;
typedef struct GeneratorStorageFrame { GeneratorStorageHooks* hooks; } GeneratorStorageFrame;
#endif
