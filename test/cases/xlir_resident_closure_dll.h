#ifndef XLIR_RESIDENT_CLOSURE_DLL_H
#define XLIR_RESIDENT_CLOSURE_DLL_H
#include "src/runtime/xl6_closure.h"
typedef struct ClosureTestEnvironment ClosureTestEnvironment;
typedef struct ClosureTestHooks {
    void (*during)(ClosureTestEnvironment*);
    void (*drop)(ClosureTestEnvironment*);
    bool (*trace)(const ClosureTestEnvironment*, XL6_ModuleOwnershipVisit, void*);
    bool (*result)(struct xrtcallresult*, int64_t);
    unsigned invoked, dropped, adapted;
} ClosureTestHooks;
struct ClosureTestEnvironment {
    ClosureTestHooks *producer, *adapter;
    void* context;
    struct xvalue* captured;
};
typedef bool (*ClosureTestInvoke)(void*, int64_t*);
typedef void (*ClosureTestDrop)(void*);
typedef const char* const* (*ClosureTestNames)(void);
typedef const xlir_call_default* (*ClosureTestDefaults)(void);
#endif
