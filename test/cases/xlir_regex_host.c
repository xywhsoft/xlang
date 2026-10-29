#include "src/runtime/xl6_text_value.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define VIEW(s) (xstrview){s, sizeof(s)-1}
static void text(xstrview actual, const char* expected, size_t length)
{
    assert(actual.Data && actual.Size == length && !memcmp(actual.Data, expected, length));
    assert(actual.Data[length] == 0); xrtFree((void*)actual.Data);
}
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
    assert(after.AllocCount-before.AllocCount == after.FreeCount-before.FreeCount);
    assert(before.DoubleFreeCount == after.DoubleFreeCount);
    assert(before.InvalidFreeCount == after.InvalidFreeCount);
    assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
}
static void failures(void)
{
    void* regex = xl6RegexCreateV1(VIEW("(?<x>a)(b)?()"), 0, 1024, 8, -1);
    assert(regex);
    unsigned rejected = 0, complete = 0;
    for (unsigned kind=0; kind<5; ++kind) {
        unsigned point;
        for (point=0; point<512; ++point) {
            xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
            assert(xrtMemDebugFailAfter(point));
            void* handle = NULL; xstrview output = {NULL,0};
            if (kind == 0) handle = xl6RegexCreateV1(VIEW("(?<x>a)(b)?()"), 0, 1024, 8, -1);
            if (kind == 1 || kind == 2) handle = xl6RegexMatchCreateV1(regex, VIEW("a\0z"), kind-1, 0, -1);
            if (kind == 3) output = xl6RegexEscapeV1(VIEW("a\0.+"), -1);
            if (kind == 4) output = xl6RegexPatternV1(regex);
            bool hit = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
            if (hit) {
                assert(!handle && !output.Data && xrtErrorKind(xrtGetError()) == XERR_MEMORY); ++rejected;
            } else { assert((handle || output.Data) && !xrtGetError()); ++complete; }
            if (kind == 0) xl6RegexDestroyV1(handle);
            else xl6RegexMatchDestroyV1(handle);
            xrtFree((void*)output.Data); xrtClearError(); balanced(before);
            if (!hit) break;
        }
        assert(point < 512);
    }
    xl6RegexDestroyV1(regex);
    printf("regex native OOM: %u actual failures, %u first no-hit completions, balanced\n",rejected,complete);
}
static int worker(void* snapshot)
{
    for (unsigned i=0; i<250; ++i) {
        text(xl6RegexMatchTextV1(snapshot,1), "a",1);
        assert(xl6RegexMatchStartV1(snapshot,1)==0);
        assert(xl6RegexMatchIndexV1(snapshot,VIEW("x"),-1)==1);
    }
    assert(!xrtGetError()); return 0;
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    void* regex = xl6RegexCreateV1(VIEW("(?<x>a)(b)?()"), 0, 1024, 8, -1);
    assert(regex && xl6RegexCaptureCountV1(regex)==4);
    char input[] = "a\0z";
    void* snapshot = xl6RegexMatchCreateV1(regex, (xstrview){input,3}, 0, 0, -1);
    assert(snapshot && xl6RegexMatchMatchedV1(snapshot));
    assert(xl6RegexMatchCountV1(snapshot)==4);
    assert(xl6RegexMatchTextV1(snapshot,2).Data==NULL && !xrtGetError());
    text(xl6RegexMatchTextV1(snapshot,3),"",0);
    assert(xl6RegexMatchStartV1(snapshot,2)==-1 && xl6RegexMatchEndV1(snapshot,2)==-1);
    input[0]='z'; xl6RegexDestroyV1(regex);
    text(xl6RegexMatchInputV1(snapshot),"a\0z",3);
    text(xl6RegexMatchNameV1(snapshot,1),"x",1);
    assert(xl6RegexMatchIndexV1(snapshot,VIEW(""),-1)==-1);
    xthread* threads[4];
    for (unsigned i=0;i<4;++i) { threads[i]=xrtThreadCreate(worker,snapshot,0); assert(threads[i]); }
    xrtSetErrorKind(XERR_RANGE);
    for (unsigned i=0;i<4;++i) {
        assert(xrtThreadWait(threads[i])==XWAIT_OK && xrtThreadExitCode(threads[i])==0);
        xrtThreadDestroy(threads[i]);
    }
    assert(xrtErrorKind(xrtGetError())==XERR_RANGE);
    xl6RegexMatchDestroyV1(snapshot); assert(xrtErrorKind(xrtGetError())==XERR_RANGE);
    xrtClearError(); balanced(before);
    failures();
    xrtMemDebugSnapshot(&before);
    regex=xl6RegexCreateV1(VIEW("("),0,1024,8,-1);
    assert(!regex && xrtGetError()); xrtClearError(); balanced(before);
    puts("regex snapshots: NUL, unmatched/empty, lifetime, 1000 cross-thread reads passed");
    return 0;
}
