#include "src/runtime/xl6_text_value.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#endif

#define VIEW(Text) ((xstrview){Text, sizeof(Text)-1})
#define KEY "XLANG_ENV_NATIVE_20261003"
#define UTF8_KEY KEY "_\xe4\xbd\xa0"
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
    assert(after.AllocCount-before.AllocCount == after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
    assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
}
static void exact(xstrview value, xstrview expected)
{
    assert(value.Data && value.Size == expected.Size && !memcmp(value.Data, expected.Data, value.Size));
    assert(!value.Data[value.Size]); xrtFree((void*)value.Data);
}
static void failure(xerrkind kind)
{ assert(xrtErrorKind(xrtGetError()) == kind); xrtClearError(); }
static void functional(void)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xl6EnvRemoveV1(VIEW(KEY), -1);
    assert(!xl6EnvGetV1(VIEW(KEY), -1).Data && !xl6EnvHasV1(VIEW(KEY), -1) && !xrtGetError());
    /* No trailing byte is available; selected-prefix copies must not overread. */
    char* unterminated = xrtMalloc(sizeof(KEY)-1); assert(unterminated);
    memcpy(unterminated, KEY, sizeof(KEY)-1);
    xl6EnvSetV1((xstrview){unterminated,sizeof(KEY)-1}, VIEW(""), -1, -1);
    assert(xl6EnvHasV1(VIEW(KEY), 9999)); exact(xl6EnvGetV1(VIEW(KEY), -1), VIEW(""));
    xrtFree(unterminated);
    xl6EnvSetV1(VIEW(KEY "\0ignored"), VIEW("\xe4\xbd\xa0-value\0ignored"), sizeof(KEY)-1, 9);
    xstrview saved = xl6EnvGetV1(VIEW(KEY), -1);
    xl6EnvSetV1(VIEW(KEY), VIEW("replacement"), -1, -1);
    exact(saved, VIEW("\xe4\xbd\xa0-value"));
    (void)xl6EnvGetV1(VIEW(KEY "\0ignored"), -1); failure(XERR_VALUE);
    (void)xl6EnvHasV1(VIEW(""), -1); failure(XERR_VALUE);
    xl6EnvRemoveV1(VIEW(KEY "=bad"), -1); failure(XERR_VALUE);
    xl6EnvSetV1(VIEW(KEY), VIEW("bad\0value"), -1, -1); failure(XERR_VALUE);
    xl6EnvSetV1(VIEW(KEY), VIEW("\xe4\xbd\xa0"), -1, 1); failure(XERR_VALUE);
    (void)xl6EnvGetV1(VIEW(KEY), -2); failure(XERR_RANGE);
    (void)xl6EnvGetV1((xstrview){NULL,1}, -1); failure(XERR_ARGUMENT);
    exact(xl6EnvGetV1(VIEW(KEY), -1), VIEW("replacement"));
    xl6EnvSetV1(VIEW(UTF8_KEY), VIEW("\xe4\xbd\xa0"), -1, -1);
    exact(xl6EnvGetV1(VIEW(UTF8_KEY), -1), VIEW("\xe4\xbd\xa0"));
    xl6EnvRemoveV1(VIEW(UTF8_KEY), -1);
    /* Successful operations, including missing, preserve an ambient error. */
    xrtSetErrorInfo(XERR_STATE, "env-test", 1, "pre-existing");
    const xerror* pending = xrtGetError(); assert(pending);
    exact(xl6EnvGetV1(VIEW(KEY), -1), VIEW("replacement"));
    assert(xl6EnvHasV1(VIEW(KEY), -1) && xrtGetError() == pending);
    xl6EnvSetV1(VIEW(KEY), VIEW(""), -1, -1); assert(xrtGetError() == pending);
    xl6EnvRemoveV1(VIEW(KEY), -1); assert(xrtGetError() == pending);
    assert(!xl6EnvGetV1(VIEW(KEY), -1).Data && xrtGetError() == pending);
    assert(!xl6EnvHasV1(VIEW(KEY), -1) && xrtGetError() == pending);
    xl6EnvRemoveV1(VIEW(KEY), -1); assert(xrtGetError() == pending);
    xrtClearError(); balanced(before);
}
static void externalInvalid(void)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
#if defined(_WIN32)
    const wchar_t invalid[] = {0xd800,0};
    assert(SetEnvironmentVariableW(L"XLANG_ENV_NATIVE_20261003", invalid));
#else
    assert(!setenv(KEY, "\xc3\x28", 1));
#endif
    assert(!xl6EnvGetV1(VIEW(KEY), -1).Data); failure(XERR_VALUE);
    assert(!xl6EnvHasV1(VIEW(KEY), -1)); failure(XERR_VALUE);
    xl6EnvRemoveV1(VIEW(KEY), -1); assert(!xrtGetError());
    balanced(before);
}
static void failurePath(unsigned kind)
{
    switch(kind) {
    case 0: { xstrview value=xl6EnvGetV1(VIEW(KEY),-1); xrtFree((void*)value.Data); break; }
    case 1: (void)xl6EnvHasV1(VIEW(KEY),-1); break;
    case 2: xl6EnvSetV1(VIEW(KEY),VIEW("new"),-1,-1); break;
    case 3: xl6EnvRemoveV1(VIEW(KEY),-1); break;
    case 4: (void)xl6EnvGetV1(VIEW("bad=name"),-1); break;
    case 5: xl6EnvSetV1(VIEW(KEY),VIEW("bad\0value"),-1,-1); break;
    case 6: xl6EnvSetV1(VIEW(KEY),VIEW("\xe4\xbd\xa0"),-1,1); break;
    default: assert(0);
    }
}
static void failures(void)
{
    unsigned hits=0, completions=0;
    for(unsigned kind=0;kind<7;++kind) {
        bool completed=false;
        for(unsigned point=0;point<128;++point) {
            xl6EnvSetV1(VIEW(KEY),VIEW("old"),-1,-1); assert(!xrtGetError());
            xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
            assert(xrtMemDebugFailAfter(point)); failurePath(kind);
            bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
            if(hit){assert(xrtErrorKind(xrtGetError())==XERR_MEMORY);++hits;}
            else {++completions;completed=true;}
            xrtClearError(); balanced(before);
            if(hit) exact(xl6EnvGetV1(VIEW(KEY),-1),VIEW("old"));
            if(!hit) break;
        }
        assert(completed);
    }
    xl6EnvRemoveV1(VIEW(KEY),-1); assert(!xrtGetError());
    printf("environment errors: %u actual allocation failures, %u first no-hit completions, balanced\n",hits,completions);
}
typedef struct Worker { unsigned index, checks; const char* longValue; size_t longSize; xatomic32* ready; } Worker;
static int32 worker(void* data)
{
    Worker* context=data;
    (void)xrtAtomic32FetchAdd(context->ready,1,XMEMORY_ACQ_REL);
    while(xrtAtomic32Load(context->ready,XMEMORY_ACQUIRE)!=4) xrtThreadYield();
    for(unsigned i=0;i<1500;++i) {
        if(context->index<2) {
            if((i+context->index)%3==0) xl6EnvRemoveV1(VIEW(KEY),-1);
            else xl6EnvSetV1(VIEW(KEY), (i+context->index)%3==1 ? VIEW("\xe4\xbd\xa0") :
                (xstrview){context->longValue,context->longSize}, -1,-1);
        } else {
            xstrview value=xl6EnvGetV1(VIEW(KEY),-1);
            if(value.Data) {
                assert((value.Size==3&&!memcmp(value.Data,"\xe4\xbd\xa0",3)) ||
                    (value.Size==context->longSize&&!memcmp(value.Data,context->longValue,value.Size)));
                xrtFree((void*)value.Data);
            }
            (void)xl6EnvHasV1(VIEW(KEY),-1); /* Separate snapshots, not an atomic pair. */
        }
        assert(!xrtGetError()); ++context->checks;
    }
    return 0;
}
static void concurrent(void)
{
    char longValue[4097]; memset(longValue,'x',sizeof(longValue)-1); longValue[sizeof(longValue)-1]=0;
    Worker workers[4]; xthread* threads[4]; xatomic32 ready=XRT_ATOMIC32_INIT(0);
    xl6EnvRemoveV1(VIEW(KEY),-1);
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    for(unsigned i=0;i<4;++i) {
        workers[i]=(Worker){i,0,longValue,sizeof(longValue)-1,&ready};
        threads[i]=xrtThreadCreate(worker,&workers[i],0); assert(threads[i]);
    }
    for(unsigned i=0;i<4;++i) {
        assert(xrtThreadWait(threads[i])==XWAIT_OK && xrtThreadExitCode(threads[i])==0);
        xrtThreadDestroy(threads[i]); assert(workers[i].checks==1500);
    }
    xl6EnvRemoveV1(VIEW(KEY),-1); assert(!xrtGetError()); balanced(before);
    puts("environment concurrent: 6000 operations, whole snapshots, balanced");
}
int main(void)
{
    assert(xrtMemDebugEnable(true)); functional(); externalInvalid(); failures(); concurrent();
    puts("environment native: exact views, nullable/empty, UTF-8, ambient errors and ownership passed");
    return 0;
}
