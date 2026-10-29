#define XRT_MODULE_MEMORY
#define XRT_MODULE_ERROR
#define XRT_MODULE_FUTURE
#include "lib/xrt/xrt.h"

static xpromise* gate;

xfuture* xlir_void_gate_open(void)
{
    xfuture* future = NULL;
    if (gate != NULL) { xrtSetErrorKind(XERR_STATE); return NULL; }
    gate = xrtPromiseCreate(&future, NULL);
    return future;
}

bool xlir_void_gate_finish(int64_t outcome)
{
    bool ok = false;
    xerror* error;
    if (gate == NULL) { xrtSetErrorKind(XERR_STATE); return false; }
    switch (outcome) {
    case 0: ok = xrtPromiseResolve(gate, NULL); break;
    case 1:
        error = xrtErrorCreate(XERR_STATE, "void gate", 1, "gate failed");
        if (error != NULL) { ok = xrtPromiseReject(gate, error); xrtErrorFree(error); }
        break;
    case 2: ok = xrtPromiseCancel(gate); break;
    case 3: ok = xrtPromiseClose(gate); break;
    default: xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    xrtPromiseDestroy(gate);
    gate = NULL;
    return ok;
}
