#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <__xl_runtime/xruntime_decl.h>

static size_t allocated,freed;
static void* trackedMalloc(size_t size) { void* p=malloc(size);if(p)++allocated;return p; }
static void trackedFree(void* p) { if(p)++freed;free(p); }
#define malloc trackedMalloc
#define free trackedFree
#include "xlir_c_owned_native.c"
#undef malloc
#undef free
#define main unusedGeneratedMain
#include XL6_C_OWNED_BOUNDARY_PROGRAM
#undef main

int main(void)
{
    assert(xrtMemDebugEnable(true) && xrtMemDebugEnabled());
    assert(xlir_module_init());
    size_t failures=0,successes=0;
    for(int mode=0;mode<5;++mode) {
        xl6_foreign_error_scope entry={0};
        assert(xl6ForeignCallBegin(&entry));
        assert(owned_boundary_exercise(mode));
        xl6_foreign_error_result outcome=xl6ForeignCallEnd(&entry);
        assert(outcome.ok && !outcome.error && allocated==freed);
        xmemdebugsnapshot before={0};xrtMemDebugSnapshot(&before);
        size_t modeFailures=0,modeSuccesses=0;
        for(unsigned limit=0;limit<96;++limit) {
            assert(xl6ForeignCallBegin(&entry));
            assert(xrtMemDebugFailAfter(limit));
            bool value=owned_boundary_exercise(mode);
            bool injected=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
            outcome=xl6ForeignCallEnd(&entry);
            if(outcome.ok){assert(value && !injected);++modeSuccesses;}
            else{assert(!value && injected && outcome.error);++modeFailures;}
            xrtErrorFree(outcome.error);xrtClearError();
            assert(allocated==freed);
            xmemdebugsnapshot after={0};xrtMemDebugSnapshot(&after);
            assert(after.LiveCount==before.LiveCount);
        }
        assert(modeFailures && modeSuccesses);
        failures+=modeFailures;successes+=modeSuccesses;
    }
    printf("C owner callable boundaries: %zu injected failures, %zu completions; no foreign or XRT leaks\n",failures,successes);
    return 0;
}
