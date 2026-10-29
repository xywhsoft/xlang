/* Consumer-side observation only. The real product handles every operation,
 * allocator and reset. Legacy suites keep their original assertions. */
#include "xruntime_decl.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#if defined(STDLIB_SUITE_UNTRACKED_REENTRY)
extern int stdlibNativeSuiteEntry(int,char**);
int main(int argc,char** argv)
{
    assert(argc==2 && !strcmp(argv[1],"reentry"));
    int code=stdlibNativeSuiteEntry(argc,argv);
    assert(!code && !xrtGetError() && !xrtMemDebugEnabled());
    puts("stdlib-suite-reentry debug-quarantine=0 ledger-acceptance=0 caller-error=0");
    return 0;
}
#else

static uint64_t allocations, frees, resets;
#if defined(STDLIB_SUITE_SKIP_LOGICAL_INJECTION)
bool __wrap_xrtMemDebugFailAfter(uint64_t budget)
{ (void)budget;xrtMemDebugFailClear();return true; }
#endif
static void check(const xmemdebugsnapshot* value)
{
    assert(xrtMemDebugEnabled());
    assert(!value->InvalidFreeCount && !value->DoubleFreeCount && !value->UseAfterFreeCount);
    assert(value->AllocCount>=value->FreeCount);
    assert(value->AllocCount-value->FreeCount==value->LiveCount);
}
extern bool __real_xrtMemDebugReset(void);
bool __wrap_xrtMemDebugReset(void)
{
    xmemdebugsnapshot before;
#if defined(STDLIB_SUITE_LEAK_BEFORE_RESET)
    static bool injected;
    if (!injected) {assert(xrtMalloc(37));injected=true;}
#endif
    xrtMemDebugSnapshot(&before);check(&before);
    bool result=__real_xrtMemDebugReset();
    if (result) {
        assert(!before.LiveCount && !before.LiveBytes);
        allocations+=before.AllocCount;frees+=before.FreeCount;++resets;
    }
    return result;
}

#if defined(STDLIB_SUITE_VOID_ENTRY)
extern int stdlibNativeSuiteEntry(void);
#else
extern int stdlibNativeSuiteEntry(int,char**);
#endif
int main(int argc,char** argv)
{
#if defined(STDLIB_SUITE_DISABLE_DEBUG)
    assert(xrtMemDebugEnable(false));
#else
    assert(xrtMemDebugEnable(true));
#endif
    assert(xrtMemDebugEnabled());
#if defined(STDLIB_SUITE_VOID_ENTRY)
    assert(argc==1);(void)argv;int code=stdlibNativeSuiteEntry();
#else
    int code=stdlibNativeSuiteEntry(argc,argv);
#endif
    assert(!code && !xrtGetError() && !xrtMemDebugFailTriggered());
#if defined(STDLIB_SUITE_LEAK_AT_END)
    assert(xrtMalloc(37));
#endif
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);check(&after);
    assert(!after.LiveCount && !after.LiveBytes);
    allocations+=after.AllocCount;frees+=after.FreeCount;
    assert(allocations==frees);
    printf("stdlib-suite-ledger resets=%" PRIu64 " allocations=%" PRIu64
        " frees=%" PRIu64 " live=0 bytes=0 invalid=0 double=0 uaf=0 caller-error=0\n",
        resets,allocations,frees);
    return 0;
}
#endif
