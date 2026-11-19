#include "foreign_xui_codeedit_diagnostics_heap.h"
#include <xge3_runtime.h>
#include <assert.h>
#include <math.h>

static xmemdebugsnapshot before;

void fixtureDiagnosticBudget(int allowance)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    if (allowance < 0) xrtMemDebugFailClear();
    else assert(xrtMemDebugFailAfter(allowance));
}

void fixtureDiagnosticHeapBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
}

int fixtureDiagnosticHeapUnchanged(void)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    return after.AllocCount == before.AllocCount && after.FreeCount == before.FreeCount &&
        after.ReallocCount == before.ReallocCount;
}

float fixtureDiagnosticNaN(void)
{
    return NAN;
}
