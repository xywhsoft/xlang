#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"

typedef struct xvalue xvalue;

/* Same stable host-side XCall ABI as cgen_xcall_error_bridge_host.c. The
 * combined declaration snapshot currently rejects unrelated conversion
 * feature closure in JIT host files, so declare only this boundary here. */
typedef struct xrtcallframe {
    xvalue* Self;
    const void* Signature;
    size_t ArgumentCount;
    xvalue* const* Arguments;
    size_t KeywordCount;
    const void* KeywordNames;
    xvalue* const* KeywordValues;
    void* Context;
} xrtcallframe;
typedef struct xrtcallresult {
    size_t Count;
    size_t OverflowCapacity;
    xvalue* Inline[4];
    xvalue** Overflow;
} xrtcallresult;
void xrtCallResultInit(xrtcallresult* result);
void xrtCallResultUnit(xrtcallresult* result);
bool xrtValueInvoke(const xvalue* callback, const xrtcallframe* frame, xrtcallresult* result);
xvalue* xrtValueRetain(xvalue* value);
void xrtValueRelease(xvalue* value);

static xvalue* saved;

void foreign_scope_invoke(xvalue* callback)
{
    xrtcallframe frame = {0};
    xrtcallresult result;
    xrtCallResultInit(&result);
    (void)xrtValueInvoke(callback, &frame, &result);
    xrtCallResultUnit(&result);
    xrtClearError();
}

void foreign_scope_save(xvalue* callback)
{
    xvalue* previous = saved;
    saved = xrtValueRetain(callback);
    xrtValueRelease(previous);
}

void foreign_scope_run_saved(void)
{
    foreign_scope_invoke(saved);
}

void foreign_scope_clear_saved(void)
{
    xrtValueRelease(saved);
    saved = NULL;
}

int64_t foreign_scope_native_error(void)
{
    xrtSetErrorTake(xrtErrorCreate(XERR_STATE, "native", 0, "foreign native error"));
    return 17;
}
