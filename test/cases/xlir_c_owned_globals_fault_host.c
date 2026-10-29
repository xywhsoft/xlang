#include <assert.h>
#include <stdlib.h>
#include <__xl_runtime/xruntime_decl.h>

/* Count only C producer allocations, independently of XRT debug allocation
 * bookkeeping. Every failure point must balance them after module cleanup. */
static size_t allocated, freed;
static void* trackedMalloc(size_t size)
{
    void* data=malloc(size);
    if(data)++allocated;
    return data;
}
static void trackedFree(void* data)
{
    if(data)++freed;
    free(data);
}
#define malloc trackedMalloc
#define free trackedFree
#include "xlir_c_owned_native.c"
#undef malloc
#undef free

#define main unusedGeneratedMain
#include XL6_C_OWNED_GLOBALS_PROGRAM
#undef main

int main(int argc,char** argv)
{
    assert(argc==2);
    assert(xrtMemDebugEnable(true) && xrtMemDebugEnabled());
    assert(xrtMemDebugFailAfter((uint64_t)atoi(argv[1])));
    bool ok=xlir_module_init();
    bool failed=xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();xrtClearError();
    xlir_module_fini();
    assert(allocated==freed);
    assert(failed || ok);
    return failed ? 0 : 10;
}
