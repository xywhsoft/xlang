#ifndef XLIR_CONTINUATION_SCOPE_DLL_H
#define XLIR_CONTINUATION_SCOPE_DLL_H
#include "src/runtime/xl6_continuation_queue.h"
#include "xlir_resident_closure_dll.h"
typedef struct ContinuationScopeHooks {
    void (*run)(void*);
    void (*release)(void*);
    unsigned ran, released;
} ContinuationScopeHooks;
typedef struct ContinuationScopeFrame {
    XL6_ContinuationTicket ticket;
    ContinuationScopeHooks* hooks;
    void* fixture;
    unsigned index;
} ContinuationScopeFrame;
#endif
