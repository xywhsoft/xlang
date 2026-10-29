#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_closure.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define VIEW(s) (xstrview){s,sizeof(s)-1}

/* Permanently resident capture-free producer, real closure Create/Enter/Leave.
 * No fake scope implementation or producer-written closure header. */
static void* session;
static void* snapshot;
static unsigned calls;
static bool failCallback;
static bool probeThread;
static int wrongThread(void* handle)
{
    assert(!xl6RegexReplacerSnapshotV1(handle) && xrtErrorKind(xrtGetError())==XERR_STATE);
    xrtClearError();return 0;
}
static bool invoke(void* environment,xstrview* output)
{
    assert(!environment);++calls;
    if (probeThread) {
        probeThread=false;xthread* thread=xrtThreadCreate(wrongThread,session,0);assert(thread);
        assert(xrtThreadWait(thread)==XWAIT_OK);xrtThreadDestroy(thread);
    }
    void* item=xl6RegexReplacerSnapshotV1(session);if (!item) return false;
    xl6RegexMatchDestroyV1(snapshot);snapshot=item;
    if (failCallback) {
        xrtSetErrorTake(xrtErrorCreate(XERR_RANGE,"native.callback",731,"original exception"));return false;
    }
    output->Data=xrtStrDupView(VIEW("<\0>"));output->Size=3;return output->Data!=NULL;
}
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount &&
        before.UseAfterFreeCount==after.UseAfterFreeCount);
}
static void clearSnapshot(void)
{xl6RegexMatchDestroyV1(snapshot);snapshot=NULL;}
static void failures(void* regex,XL6_Closure* callback)
{
    unsigned rejected=0,completed=0;
    for (unsigned kind=0;kind<3;++kind) {
        size_t point;
        for (point=0;point<2048;++point) {
            xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
            xstrview result={0};xbuffer* target=NULL;session=NULL;
            if (kind) {session=xl6RegexReplacerCreateV1(regex,VIEW("aaaaaaaaaaaaaaaa"),-1,-1);assert(session);}
            if (kind==2) {target=xrtBufferCreate();assert(target);assert(xrtBufferAppend(target,(xbytesview){(const uint8*)"p",1}));}
            assert(xrtMemDebugFailAfter(point));
            if (!kind) session=xl6RegexReplacerCreateV1(regex,VIEW("a\0b"),-1,-1);
            if (kind==1) result=xl6RegexReplacerRunV1(session,callback);
            if (kind==2) xl6RegexReplacerRunBufferV1(session,target,callback);
            bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
            if (hit) {
                assert(xrtErrorKind(xrtGetError())==XERR_MEMORY);++rejected;
                if (!kind) assert(!session);
                if (kind==1) assert(!result.Data);
                if (target) {xbytesview v=xrtBufferView(target);assert(v.Size==1 && v.Data[0]=='p');}
            } else {assert(!xrtGetError());++completed;}
            clearSnapshot();xl6RegexReplacerDestroyV1(session);session=NULL;
            xrtFree((void*)result.Data);xrtBufferDestroy(target);xrtClearError();balanced(before);
            if (!hit) break;
        }
        assert(point<2048);
    }
    printf("regex callback native OOM: %u actual failures, %u first no-hit completions, balanced\n",rejected,completed);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    const XL6_ClosureRuntimeV1* runtime=xl6ClosureRuntimeV1Get();
    XL6_Closure* callback=runtime->Create(NULL,(void (*)(void))invoke,NULL,NULL,NULL,NULL,NULL,0,NULL);assert(callback);
    void* regex=xl6RegexCreateV1(VIEW("."),0,1024,8,-1);assert(regex);
    session=xl6RegexReplacerCreateV1(regex,VIEW("a\0b"),-1,-1);assert(session);
    probeThread=true;
    xstrview value=xl6RegexReplacerRunV1(session,callback);
    assert(value.Data && value.Size==9 && !memcmp(value.Data,"<\0><\0><\0>",9) && calls==3);
    xrtFree((void*)value.Data);xl6RegexReplacerDestroyV1(session);session=NULL;
    value=xl6RegexMatchInputV1(snapshot);assert(value.Size==3 && !memcmp(value.Data,"a\0b",3));
    xrtFree((void*)value.Data);clearSnapshot();
    session=xl6RegexReplacerCreateV1(regex,VIEW("abc"),-1,-1);assert(session);failCallback=true;
    assert(!xl6RegexReplacerRunV1(session,callback).Data);
    assert(xrtErrorKind(xrtGetError())==XERR_RANGE && xrtErrorCode(xrtGetError())==731 &&
        !strcmp(xrtErrorDomain(xrtGetError()),"native.callback"));
    xrtClearError();clearSnapshot();xl6RegexReplacerDestroyV1(session);session=NULL;failCallback=false;
    failures(regex,callback);runtime->Release(callback);xl6RegexDestroyV1(regex);xrtClearError();balanced(before);
    puts("regex actual closure scope, exact input, original exceptions, transactions and lifetimes passed");return 0;
}
