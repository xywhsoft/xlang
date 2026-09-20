#ifndef XLIR_ASYNC_MANAGED_DLL_H
#define XLIR_ASYNC_MANAGED_DLL_H
#define XRT_MODULE_FUTURE
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_continuation_future.h"
typedef struct AsyncDllHooks {void (*during)(void*);void* context;unsigned entered,returned;} AsyncDllHooks;
typedef struct AsyncDllFrame {
    AsyncDllHooks* hooks;
    XL6_ContinuationTicket ticket;xfuturewatch watch;xfuture* source;xpromise* outer;
} AsyncDllFrame;
#endif
