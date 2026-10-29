#include "../../build/test/__xl_runtime/xruntime_decl.h"

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
