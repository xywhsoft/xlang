/* Genuine product APIs only. An OS TLS destructor witnesses RETURN from XRT's
 * native thread entry, after its running reference was released. This is not
 * public xrtThreadWait == OS join, nor an image-unload proof. Keep products
 * loaded. Compare allocation METADATA only; never dereference a freed thread.
 * No forced OS-ID reuse, fake scheduler, provider or reference counter. */
#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0600
#endif
#include "xruntime_decl.h"
#include "src/runtime/xl6_text_value.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#endif

typedef struct Work {
    unsigned family;
    uint64_t budget;
    xbuffer* input;
    xvalue* bytes;
    void* resource;
    uintptr_t creator;
    bool hit;
#if defined(_WIN32)
    HANDLE exited;
#else
    sem_t exited;
#endif
} Work;
#if defined(_WIN32)
static DWORD exitKey;
static VOID CALLBACK nativeExit(void* raw)
{
    Work* work=raw;
    assert(FlsSetValue(exitKey,NULL));
    assert(SetEvent(work->exited));
}
#else
static pthread_key_t exitKey;
static void nativeExit(void* raw)
{ Work* work=raw;assert(!sem_post(&work->exited)); }
#endif

static int32 createOwned(void* raw)
{
    Work* work=raw;
    assert(xrtThreadCurrent() && !xrtGetError());
    work->creator=(uintptr_t)xrtThreadCurrent();
#if defined(_WIN32)
    assert(FlsSetValue(exitKey,work));
#else
    assert(!pthread_setspecific(exitKey,work));
#endif
    if(work->budget!=UINT64_MAX)assert(xrtMemDebugFailAfter(work->budget));
    if(work->family==0)work->resource=xl6TextIteratorCreateV1(XRT_STR_LITERAL("你\0|b"),XRT_STR_LITERAL("|"),0,-1,-1);
    else if(work->family==1)work->resource=xl6UnicodeValidatorCreateV1();
    else work->resource=xl6BufferIteratorCreateV1(work->input,-1);
    work->hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    if(work->hit)assert(!work->resource && xrtErrorKind(xrtGetError())==XERR_MEMORY);
    else{
        assert(work->resource && !xrtGetError());
        if(work->family==0){
            xstrview first=xl6TextIteratorNextV1(work->resource);
            assert(first.Data && first.Size==4 && !memcmp(first.Data,"你\0",4));
            xrtFree((void*)first.Data);
        }else if(work->family==1){
            assert(xl6UnicodeValidatorFeedV1(work->resource,work->bytes,false,-1)==XUTF_OK);
            assert(xl6UnicodeValidatorTotalV1(work->resource)==3);
        }else{
            assert(xl6BufferIteratorHasNextV1(work->resource));
            assert(xl6BufferIteratorNextV1(work->resource)=='a');
        }
        assert(!xrtGetError());
    }
    xrtClearError();return 0;
}

typedef struct Allocation {uintptr_t address;bool live;} Allocation;
static bool visit(const xmemdebugallocation* allocation,void* raw)
{
    Allocation* target=raw;
    if((uintptr_t)allocation->Address==target->address)target->live=true;
    return true;
}
static bool allocated(uintptr_t address)
{
    Allocation target={address,false};
    assert(xrtMemDebugVisitLive(visit,&target)>0 && !xrtGetError());
    return target.live;
}

static unsigned denyOtherCreator(Work* work)
{
    unsigned guards=0;
#define DENY(Call) do{assert(!xrtGetError());Call;assert(xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();++guards;}while(0)
    if(work->family==0){
        DENY(assert(!xl6TextIteratorNextV1(work->resource).Data));
        DENY(assert(!xl6TextIteratorClosedV1(work->resource)));
        DENY(xl6TextIteratorCloseV1(work->resource));
    }else if(work->family==1){
        DENY(assert(xl6UnicodeValidatorFeedV1(work->resource,work->bytes,false,-1)==XUTF_INVALID));
        DENY(xl6UnicodeValidatorResetV1(work->resource));
        DENY(assert(xl6UnicodeValidatorErrorV1(work->resource)==-1));
        DENY(assert(xl6UnicodeValidatorTotalV1(work->resource)==0));
        DENY(assert(!xl6UnicodeValidatorFinishedV1(work->resource)));
    }else{
        DENY(assert(!xl6BufferIteratorHasNextV1(work->resource)));
        DENY(assert(xl6BufferIteratorNextV1(work->resource)==0));
        DENY(assert(!xl6BufferIteratorClosedV1(work->resource)));
        DENY(xl6BufferIteratorCloseV1(work->resource));
    }
#undef DENY
    return guards;
}

int main(int argc,char** argv)
{
    assert((argc==2 || argc==3) && xrtMemDebugEnable(true));
    Work work={0};work.family=(unsigned)strtoul(argv[1],NULL,10);work.budget=UINT64_MAX;
    assert(work.family<3);
    if(argc==3){char* end=NULL;errno=0;work.budget=strtoull(argv[2],&end,10);assert(*argv[2] && *argv[2]!='-' && end && !*end && !errno);}
#if defined(_WIN32)
    work.exited=CreateEventW(NULL,true,false,NULL);assert(work.exited);
    exitKey=FlsAlloc(nativeExit);assert(exitKey!=FLS_OUT_OF_INDEXES);
#else
    assert(!sem_init(&work.exited,0,0));assert(!pthread_key_create(&exitKey,nativeExit));
#endif
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    work.input=xrtBufferCreate();assert(work.input);
    assert(xrtBufferAppend(work.input,(xbytesview){(const unsigned char*)"a\0b",3}));
    work.bytes=xrtValueBytes((xbytesview){(const unsigned char*)"你",3});assert(work.bytes);
    xthread* thread=xrtThreadCreate(createOwned,&work,0);assert(thread);
#if defined(_WIN32)
    assert(WaitForSingleObject(work.exited,10000)==WAIT_OBJECT_0);
#else
    struct timespec deadline;assert(!clock_gettime(CLOCK_REALTIME,&deadline));deadline.tv_sec+=10;
    int waited;do{waited=sem_timedwait(&work.exited,&deadline);}while(waited && errno==EINTR);assert(!waited);
#endif
    assert(xrtThreadWaitFor(thread,UINT64_C(5000000))==XWAIT_OK && xrtThreadExitCode(thread)==0);
    assert(!xrtGetError() && !xrtMemDebugFailTriggered());
    xrtThreadDestroy(thread);
    bool pinned=allocated(work.creator);
    bool expected=work.resource!=NULL;
    unsigned guards=0;
    if(work.resource){
        guards=denyOtherCreator(&work);
        xrtSetErrorKind(XERR_IO);const xerror* prior=xrtGetError();
        if(work.family==0)xl6TextIteratorDestroyV1(work.resource);
        else if(work.family==1)xl6UnicodeValidatorDestroyV1(work.resource);
        else xl6BufferIteratorDestroyV1(work.resource);
        work.resource=NULL;
        assert(xrtGetError()==prior);xrtClearError();
    }
    assert(!allocated(work.creator));
    xrtValueRelease(work.bytes);xrtBufferDestroy(work.input);assert(!xrtGetError());
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount);
    assert(before.UseAfterFreeCount==after.UseAfterFreeCount);
#if defined(_WIN32)
    assert(FlsFree(exitKey));assert(CloseHandle(work.exited));
#else
    assert(!pthread_key_delete(exitKey));assert(!sem_destroy(&work.exited));
#endif
    printf("resource-identity family=%u point=%llu hit=%d creator-pinned=%d expected=%d guards=%u "
        "native-entry-returned=1 caller-isolated=1 balanced=1\n",work.family,(unsigned long long)work.budget,
        work.hit,pinned,expected,guards);
    if(pinned!=expected){fputs("managed creator identity was not retained\n",stderr);return 2;}
    return 0;
}
