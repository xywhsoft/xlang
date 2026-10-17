/* Real product entry points only. INT is raised only inside this child while
 * ignored or watched. No source inclusion, recorder injection, private layout,
 * parent PID, console broadcast, CLOSE/LOGOFF/SHUTDOWN or disposition change
 * outside this private test process. */
#include "src/runtime/xl6_text_value.h"
#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#include <errno.h>
#endif

static uint64_t resources(void)
{
#if defined(_WIN32)
    DWORD count=0;assert(GetProcessHandleCount(GetCurrentProcess(),&count));return count;
#else
    DIR* directory=opendir("/proc/self/fd");assert(directory);
    char own[32];snprintf(own,sizeof(own),"%d",dirfd(directory));
    uint64_t count=0;struct dirent* entry;errno=0;
    while((entry=readdir(directory))!=NULL){
        if(strcmp(entry->d_name,".") && strcmp(entry->d_name,"..") && strcmp(entry->d_name,own))++count;
    }
    assert(!errno && !closedir(directory));return count;
#endif
}

static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount);
    assert(before.UseAfterFreeCount==after.UseAfterFreeCount);
}
static void error(xerrkind kind)
{ assert(xrtErrorKind(xrtGetError())==kind);xrtClearError(); }
static void name(xstrview value,const char* expected)
{
    size_t size=strlen(expected);
    assert(value.Data && value.Size==size && !memcmp(value.Data,expected,size+1));
    xrtFree((void*)value.Data);
}
static void event(void* value)
{
    assert(value && xl6SignalEventCodeV1(value)==XSIGNAL_INT);
    assert(xl6SignalEventCountV1(value)>0 && xl6SignalEventTotalV1(value)>=xl6SignalEventCountV1(value));
    assert(xl6SignalEventTimeV1(value)>0);
    name(xl6SignalEventNameV1(value),"INT");
}
static void metadata(void)
{
    assert(xl6SignalSupportedV1(XSIGNAL_INT));
    assert(!xl6SignalSupportedV1(INT64_MIN) && !xl6SignalSupportedV1(INT64_MAX));
    name(xl6SignalNameV1(XSIGNAL_INT),"INT");name(xl6SignalNameV1(INT64_MAX),"UNKNOWN");
    assert(!xl6SignalRaiseV1(INT64_C(4294967298)));error(XERR_RANGE);
    assert(!xl6SignalIgnoreV1(INT64_MIN));error(XERR_RANGE);
    assert(!xl6SignalRestoreV1(INT64_MAX));error(XERR_RANGE);
    assert(!xl6SignalCountV1(INT64_MAX));error(XERR_RANGE);
    assert(!xl6SignalReceivedV1(INT64_MIN));error(XERR_RANGE);
    xl6SignalClearV1(INT64_MAX);error(XERR_RANGE);
    assert(!xl6SignalIgnoreV1(XSIGNAL_NONE));error(XERR_ARGUMENT);
    assert(!xl6SignalRaiseV1(3));error(XERR_ARGUMENT);
    xrtSetErrorKind(XERR_STATE);const xerror* prior=xrtGetError();
    assert(xl6SignalSupportedV1(XSIGNAL_INT));name(xl6SignalNameV1(XSIGNAL_INT),"INT");
    assert(xl6SignalIgnoreV1(XSIGNAL_INT) && xl6SignalRaiseV1(XSIGNAL_INT));
    assert(xl6SignalRestoreV1(XSIGNAL_INT) && xl6SignalRestoreAllV1());
    xl6SignalResetV1();assert(xrtGetError()==prior);xrtClearError();
}
typedef struct Foreign {void* queue;void* snapshot;unsigned guards,reads;} Foreign;
static int32 foreign(void* raw)
{
    Foreign* work=raw;
#define DENY(Call) do { assert(!xrtGetError());Call;error(XERR_STATE);++work->guards; } while(0)
    DENY(assert(!xl6SignalQueueActiveV1(work->queue)));
    DENY(assert(!xl6SignalQueueClosedV1(work->queue)));
    DENY(assert(!xl6SignalQueueCodeV1(work->queue)));
    DENY(assert(!xl6SignalQueuePendingV1(work->queue)));
    DENY(assert(!xl6SignalQueuePrepareV1(work->queue,0)));
    DENY(assert(!xl6SignalQueueSnapshotV1(work->queue)));
    DENY(xl6SignalQueueConsumeV1(work->queue));
    DENY(assert(!xl6SignalQueueCloseV1(work->queue)));
    DENY(xl6SignalQueueCheckOwnerV1(work->queue));
#undef DENY
    for(unsigned i=0;i<100;++i){event(work->snapshot);++work->reads;}
    assert(!xrtGetError());return 0;
}
static void lifecycle(void)
{
    void* queue=xl6SignalQueueCreateV1(XSIGNAL_INT,false,2);assert(queue);
    assert(xl6SignalQueueActiveV1(queue) && !xl6SignalQueueClosedV1(queue));
    assert(xl6SignalQueueCodeV1(queue)==XSIGNAL_INT && !xl6SignalQueuePendingV1(queue));
    assert(!xl6SignalQueuePrepareV1(queue,0) && !xrtGetError());
    assert(xl6SignalRaiseV1(XSIGNAL_INT) && xl6SignalQueuePrepareV1(queue,2000));
    void* saved=xl6SignalQueueSnapshotV1(queue);event(saved);
    int64_t pending=xl6SignalQueuePendingV1(queue);assert(pending==1);
    assert(xrtMemDebugFailAfter(0));assert(!xl6SignalQueueSnapshotV1(queue));
    assert(xrtMemDebugFailTriggered());xrtMemDebugFailClear();error(XERR_MEMORY);
    assert(xl6SignalQueuePendingV1(queue)==pending && xl6SignalQueuePrepareV1(queue,0));
    void* retry=xl6SignalQueueSnapshotV1(queue);event(retry);
    assert(xl6SignalEventCountV1(retry)==xl6SignalEventCountV1(saved));
    assert(xl6SignalEventTotalV1(retry)==xl6SignalEventTotalV1(saved));
    xl6SignalQueueConsumeV1(queue);assert(!xl6SignalQueuePendingV1(queue));
    Foreign work={queue,saved,0,0};xthread* thread=xrtThreadCreate(foreign,&work,0);assert(thread);
    assert(xrtThreadWaitFor(thread,UINT64_C(5000000))==XWAIT_OK && !xrtThreadExitCode(thread));
    xrtThreadDestroy(thread);assert(work.guards==9 && work.reads==100);
    assert(xl6SignalQueueCloseV1(queue) && xl6SignalQueueCloseV1(queue));
    assert(xl6SignalQueueClosedV1(queue) && !xl6SignalQueueActiveV1(queue));
    assert(!xl6SignalQueuePrepareV1(queue,-1) && !xrtGetError());
    xrtSetErrorKind(XERR_IO);const xerror* prior=xrtGetError();
    xl6SignalQueueDestroyV1(queue);assert(xrtGetError()==prior);xrtClearError();
    assert(xrtSignalShutdown());event(saved);event(retry);
    xl6SignalEventDestroyV1(saved);xl6SignalEventDestroyV1(retry);assert(!xrtGetError());
    puts("signal product: real event, snapshot retry, nine owner guards, 100 foreign snapshot reads and detached lifetime passed");
}
static void once(void)
{
    for(unsigned i=0;i<32;++i){
        void* queue=xl6SignalQueueCreateV1(XSIGNAL_INT,true,2);assert(queue);
        assert(xl6SignalRaiseV1(XSIGNAL_INT) && xl6SignalQueuePrepareV1(queue,2000));
        void* snapshot=xl6SignalQueueSnapshotV1(queue);event(snapshot);
        xl6SignalQueueConsumeV1(queue);
        assert(!xl6SignalQueuePrepareV1(queue,-1) && !xl6SignalQueueActiveV1(queue));
        assert(xl6SignalQueueCloseV1(queue));xl6SignalQueueDestroyV1(queue);
        assert(xrtSignalShutdown());event(snapshot);xl6SignalEventDestroyV1(snapshot);
        assert(!xrtGetError());
    }
    puts("signal product: 32 real once deliveries and exhaustion passed");
}
static void corePath(unsigned kind)
{
    switch(kind){
    case 0:{xstrview text=xl6SignalNameV1(XSIGNAL_INT);xrtFree((void*)text.Data);break;}
    case 1:(void)xl6SignalRaiseV1(INT64_C(4294967298));break;
    case 2:(void)xl6SignalIgnoreV1(XSIGNAL_NONE);break;
    case 3:(void)xl6SignalRaiseV1(3);break;
    case 4:(void)xl6SignalRestoreV1(3);break;
    case 5:xl6SignalClearV1(3);break;
    case 6:(void)xl6SignalCountV1(INT64_MAX);break;
    default:assert(0);
    }
}
static unsigned coreFailures,queueFailures;
static void failures(void)
{
    for(unsigned kind=0;kind<7;++kind){
        unsigned point,first=UINT_MAX;
        for(point=0;point<128;++point){
            xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
            assert(xrtMemDebugFailAfter(point));corePath(kind);
            bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
            if(hit){assert(first==UINT_MAX && xrtErrorKind(xrtGetError())==XERR_MEMORY);++coreFailures;}
            else if(!kind)assert(!xrtGetError());
            else assert(xrtErrorKind(xrtGetError())==((kind==1 || kind==6)?XERR_RANGE:XERR_ARGUMENT));
            xrtClearError();assert(xrtSignalShutdown());balanced(before);
            if(!hit){
                if(first!=UINT_MAX){assert(point==first+1);printf("signal prefix core=%u failures=%u first=%u confirmed=%u balanced\n",kind,first,first,point);break;}
                first=point;
            }
        }
        assert(point<128);
    }
    for(unsigned kind=0;kind<6;++kind){
        unsigned point,first=UINT_MAX;
        for(point=0;point<128;++point){
            xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
            void* queue=NULL;void* snapshot=NULL;xstrview text={0};
            if(kind>=3){
                queue=xl6SignalQueueCreateV1(XSIGNAL_INT,false,2);assert(queue);
                assert(xl6SignalRaiseV1(XSIGNAL_INT) && xl6SignalQueuePrepareV1(queue,2000));
                if(kind==4){snapshot=xl6SignalQueueSnapshotV1(queue);assert(snapshot);}
            }
            assert(xrtMemDebugFailAfter(point));
            if(kind<3)queue=xl6SignalQueueCreateV1(XSIGNAL_INT,false,kind==0?2:kind==1?0:65537);
            if(kind==3)snapshot=xl6SignalQueueSnapshotV1(queue);
            if(kind==4)text=xl6SignalEventNameV1(snapshot);
            if(kind==5)(void)xl6SignalQueuePrepareV1(queue,-2);
            bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
            if(hit){assert(first==UINT_MAX && xrtErrorKind(xrtGetError())==XERR_MEMORY);++queueFailures;if(kind<3)assert(!queue);if(kind==3)assert(!snapshot);if(kind==4)assert(!text.Data);}
            else if(kind==1 || kind==2 || kind==5)assert(xrtErrorKind(xrtGetError())==XERR_RANGE);
            else assert(!xrtGetError());
            xrtClearError();
            if(kind==3 && hit){assert(xl6SignalQueuePrepareV1(queue,0));snapshot=xl6SignalQueueSnapshotV1(queue);event(snapshot);}
            xrtFree((void*)text.Data);xl6SignalEventDestroyV1(snapshot);xl6SignalQueueDestroyV1(queue);
            assert(xrtSignalShutdown() && !xrtGetError());balanced(before);
            if(!hit){
                if(first!=UINT_MAX){assert(point==first+1);printf("signal prefix queue=%u failures=%u first=%u confirmed=%u balanced\n",kind,first,first,point);break;}
                first=point;
            }
        }
        assert(point<128);
    }
    printf("signal product core errors: %u actual failures, 7 first no-hit completions, balanced\n",coreFailures);
    printf("signal product queue errors: %u actual failures, 6 first no-hit completions, balanced\n",queueFailures);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    metadata();lifecycle();
    /* Warm-up above includes CRT output and native dispatcher initialization.
     * Keep all 32 once deliveries inside the measured interval, alongside
     * failure/retirement cycles. This is not an OS join/unload assertion. */
    uint64_t baseline=resources();
    once();failures();assert(xrtSignalShutdown() && !xrtGetError());balanced(before);
    uint64_t after=resources();assert(after==baseline);
    printf("signal product resources: before=%" PRIu64 " after=%" PRIu64 " balanced\n",baseline,after);
    return 0;
}
