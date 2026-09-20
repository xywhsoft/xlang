#ifndef XLIR_FUTURE_CALLBACK_STORAGE_DROP_H
#define XLIR_FUTURE_CALLBACK_STORAGE_DROP_H
#include "src/runtime/xl6_future_callback.h"
typedef struct FutureCallbackStorageHooks {
    void (*during)(void*,int);
    void (*complete)(struct xpromise*,struct xfuture*);
    void* context;
    struct xfuture* source;
    unsigned calls,children,drops,returned,childCalls;
} FutureCallbackStorageHooks;
typedef struct FutureCallbackStorageData { FutureCallbackStorageHooks* hooks; } FutureCallbackStorageData;
#endif
