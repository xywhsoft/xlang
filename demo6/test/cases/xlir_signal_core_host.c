/* Native tests own this child process. Only ignored or watched INT is raised;
 * no console broadcast, parent PID, CLOSE/LOGOFF/SHUTDOWN is ever targeted. */
#include "src/runtime/xl6_text_value.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

extern bool xl6SignalTestDormant(void);
extern void xl6SignalTestTotals(uint64 first, uint64 second);
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
    assert(after.AllocCount-before.AllocCount == after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
    assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
}
static void name(int64 code, const char* expected)
{
    xstrview text = xl6SignalNameV1(code);
    assert(text.Data && text.Size == strlen(expected));
    assert(!memcmp(text.Data, expected, text.Size) && !text.Data[text.Size]);
    xrtFree((void*)text.Data);
}
static void failure(xerrkind kind)
{ assert(xrtErrorKind(xrtGetError()) == kind); xrtClearError(); }
static void functional(void)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    int codes[] = {XSIGNAL_INT, XSIGNAL_TERM, XSIGNAL_HUP, XSIGNAL_BREAK,
        XSIGNAL_CLOSE, XSIGNAL_LOGOFF, XSIGNAL_SHUTDOWN, XSIGNAL_NONE, 3};
    const char* names[] = {"INT","TERM","HUP","BREAK","CLOSE","LOGOFF","SHUTDOWN","UNKNOWN","UNKNOWN"};
    for (unsigned i=0; i<sizeof(codes)/sizeof(codes[0]); ++i) name(codes[i], names[i]);
    name(INT64_MIN,"UNKNOWN"); name(INT64_MAX,"UNKNOWN"); name(INT64_C(4294967298),"UNKNOWN");
    assert(!xl6SignalSupportedV1(INT64_C(4294967298)) && !xl6SignalSupportedV1(XSIGNAL_NONE));
    assert(xl6SignalSupportedV1(XSIGNAL_INT) && xl6SignalSupportedV1(XSIGNAL_TERM));
#if defined(_WIN32)
    assert(!xl6SignalSupportedV1(XSIGNAL_HUP) && xl6SignalSupportedV1(XSIGNAL_BREAK));
    int unsupported=XSIGNAL_HUP;
#else
    assert(xl6SignalSupportedV1(XSIGNAL_HUP) && !xl6SignalSupportedV1(XSIGNAL_BREAK));
    int unsupported=XSIGNAL_BREAK;
#endif
    assert(!xl6SignalIgnoreV1(unsupported)); failure(XERR_UNSUPPORTED);
    assert(!xl6SignalRestoreV1(unsupported)); failure(XERR_UNSUPPORTED);
    assert(!xl6SignalRaiseV1(unsupported)); failure(XERR_UNSUPPORTED);
    assert(xl6SignalHealthyV1() && xl6SignalTestDormant());
    assert(!xl6SignalRaiseV1(INT64_C(4294967298))); failure(XERR_RANGE);
    assert(!xl6SignalIgnoreV1(INT64_MIN)); failure(XERR_RANGE);
    assert(!xl6SignalRestoreV1(INT64_MAX)); failure(XERR_RANGE);
    assert(!xl6SignalCountV1(INT64_C(4294967298))); failure(XERR_RANGE);
    assert(!xl6SignalReceivedV1(INT64_C(4294967298))); failure(XERR_RANGE);
    xl6SignalClearV1(INT64_C(4294967298)); failure(XERR_RANGE);
    assert(!xl6SignalIgnoreV1(XSIGNAL_NONE)); failure(XERR_ARGUMENT);
    assert(!xl6SignalRaiseV1(3)); failure(XERR_ARGUMENT);
    assert(!xl6SignalRestoreV1(3)); failure(XERR_ARGUMENT);
    assert(!xl6SignalCountV1(3)); failure(XERR_ARGUMENT);
    xl6SignalClearV1(3); failure(XERR_ARGUMENT);
    xl6SignalTestTotals(UINT64_C(0x8000000000000005), 7);
    assert(xl6SignalCountV1(XSIGNAL_INT) == UINT64_C(0x8000000000000005));
    assert(xl6SignalCountV1(0) == UINT64_C(0x800000000000000c));
    xl6SignalTestTotals(UINT64_MAX-1, 9);
    assert(xl6SignalCountV1(0) == UINT64_MAX && xl6SignalReceivedV1(XSIGNAL_INT));
    xl6SignalClearV1(0); assert(!xl6SignalCountV1(0));
    xrtSetErrorInfo(XERR_STATE,"signal-test",1,"pre-existing");
    const xerror* pending = xrtGetError(); assert(pending);
    assert(xl6SignalSupportedV1(XSIGNAL_INT)); name(XSIGNAL_INT,"INT");
    assert(xl6SignalHealthyV1() && !xl6SignalCountV1(0) && !xl6SignalReceivedV1(XSIGNAL_INT));
    xl6SignalClearV1(0);
    assert(xl6SignalIgnoreV1(XSIGNAL_INT) && xl6SignalRaiseV1(XSIGNAL_INT));
    assert(xl6SignalRestoreV1(XSIGNAL_INT) && xl6SignalRestoreAllV1());
    xl6SignalResetV1(); assert(xrtGetError() == pending); xrtClearError();
    assert(xl6SignalTestDormant()); balanced(before);
}
static void failurePath(unsigned kind)
{
    switch (kind) {
    case 0: { xstrview text=xl6SignalNameV1(XSIGNAL_INT); xrtFree((void*)text.Data); break; }
    case 1: (void)xl6SignalRaiseV1(INT64_C(4294967298)); break;
    case 2: (void)xl6SignalIgnoreV1(XSIGNAL_NONE); break;
    case 3: (void)xl6SignalRaiseV1(3); break;
    case 4: (void)xl6SignalRestoreV1(3); break;
    case 5: xl6SignalClearV1(3); break;
    case 6: (void)xl6SignalCountV1(INT64_MAX); break;
    default: assert(0);
    }
}
static void failures(void)
{
    unsigned hits=0, completions=0;
    for (unsigned kind=0; kind<7; ++kind) {
        bool completed=false;
        for (unsigned point=0; point<128; ++point) {
            xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
            assert(xrtMemDebugFailAfter(point)); failurePath(kind);
            bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
            if (hit) { assert(xrtErrorKind(xrtGetError()) == XERR_MEMORY); ++hits; }
            else { ++completions; completed=true; }
            xrtClearError(); balanced(before);
            assert(xl6SignalTestDormant());
            if (!hit) break;
        }
        assert(completed);
    }
    printf("signal errors: %u actual allocation failures, %u first no-hit completions, balanced\n",hits,completions);
}
static void delivered(xsignalwatch* watch, const xsignalevent* event, void* context)
{
    assert(watch && event->Code == XSIGNAL_INT && event->Count > 0 && event->Total > 0);
    assert(!strcmp(event->Name,"INT"));
    xrtAtomic32Store(context,1,XMEMORY_RELEASE);
}
static void nativeLifecycle(void)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    for (unsigned i=0; i<32; ++i) {
        xatomic32 received=XRT_ATOMIC32_INIT(0);
        xsignalwatch* watch=xrtSignalOnce(XSIGNAL_INT,delivered,&received); assert(watch);
        assert(xl6SignalRaiseV1(XSIGNAL_INT));
        for (unsigned wait=0; wait<5000 && !xrtAtomic32Load(&received,XMEMORY_ACQUIRE); ++wait) xrtSleep(1);
        assert(xrtAtomic32Load(&received,XMEMORY_ACQUIRE));
        assert(xl6SignalCountV1(XSIGNAL_INT) >= 1 && xl6SignalReceivedV1(XSIGNAL_INT));
        assert(xl6SignalRestoreV1(XSIGNAL_INT) && !xrtSignalActive(watch));
        xrtSignalFree(watch); xl6SignalResetV1();
        assert(!xrtGetError() && xl6SignalTestDormant() && !xl6SignalCountV1(0));
    }
    balanced(before);
    puts("signal native lifecycle: 32 watched raises, restore/shutdown/restart, balanced");
}
static volatile sig_atomic_t originalHits;
typedef struct Retirement {
    xatomic32 entered, release, started, done;
    unsigned action;
} Retirement;
static void waiting(xsignalwatch* watch, const xsignalevent* event, void* data)
{
    Retirement* state=data;
    assert(watch && event->Code==XSIGNAL_INT);
    xrtAtomic32Store(&state->entered,1,XMEMORY_RELEASE);
    while (!xrtAtomic32Load(&state->release,XMEMORY_ACQUIRE)) xrtThreadYield();
}
static int32 retire(void* data)
{
    Retirement* state=data;
    xrtAtomic32Store(&state->started,1,XMEMORY_RELEASE);
    if (state->action==0) assert(xl6SignalRestoreV1(XSIGNAL_INT));
    else if (state->action==1) assert(xl6SignalRestoreAllV1());
    else xl6SignalResetV1();
    assert(!xrtGetError());
    xrtAtomic32Store(&state->done,1,XMEMORY_RELEASE);
    return 0;
}
static void await(xatomic32* flag)
{
    for (unsigned i=0; i<5000 && !xrtAtomic32Load(flag,XMEMORY_ACQUIRE); ++i) xrtSleep(1);
    assert(xrtAtomic32Load(flag,XMEMORY_ACQUIRE));
}
static void retirement(void)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    for (unsigned action=0; action<3; ++action) {
        Retirement state={XRT_ATOMIC32_INIT(0),XRT_ATOMIC32_INIT(0),
            XRT_ATOMIC32_INIT(0),XRT_ATOMIC32_INIT(0),action};
        xsignalwatch* watch=xrtSignalOn(XSIGNAL_INT,waiting,&state); assert(watch);
        assert(xl6SignalRaiseV1(XSIGNAL_INT)); await(&state.entered);
        xthread* thread=xrtThreadCreate(retire,&state,0); assert(thread);
        await(&state.started); xrtSleep(10);
        assert(!xrtAtomic32Load(&state.done,XMEMORY_ACQUIRE));
        xrtAtomic32Store(&state.release,1,XMEMORY_RELEASE);
        assert(xrtThreadWait(thread)==XWAIT_OK && !xrtThreadExitCode(thread));
        xrtThreadDestroy(thread); assert(xrtAtomic32Load(&state.done,XMEMORY_ACQUIRE));
        assert(!xrtSignalActive(watch)); xrtSignalFree(watch); xl6SignalResetV1();
        assert(!xrtGetError() && xl6SignalTestDormant());
    }
    balanced(before);
    puts("signal retirement: restore/restoreAll/shutdown wait for native callback, balanced");
}
static void original(int code) { if (code==SIGINT) ++originalHits; }
static void originalDisposition(void)
{
    void (*previous)(int) = signal(SIGINT,original); assert(previous != SIG_ERR);
    assert(xl6SignalIgnoreV1(XSIGNAL_INT) && xl6SignalRaiseV1(XSIGNAL_INT));
    assert(!originalHits && xl6SignalRestoreV1(XSIGNAL_INT));
    /* Raw CRT raise tests restoration of an original disposition, not SIG_DFL. */
    assert(!raise(SIGINT) && originalHits==1);
    assert(signal(SIGINT,previous) != SIG_ERR);
    xl6SignalResetV1(); assert(!xrtGetError());
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    assert(xl6SignalSupportedV1(XSIGNAL_INT) && xl6SignalHealthyV1());
    assert(xl6SignalTestDormant());
    functional(); failures(); nativeLifecycle(); retirement(); originalDisposition();
    puts("signal core native: unsigned totals, range, ownership, native errors and dispositions passed");
    return 0;
}
