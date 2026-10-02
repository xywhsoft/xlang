#include "src/runtime/xl6_text_value.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define VIEW(s) (xstrview){s,sizeof(s)-1}
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount &&
        before.UseAfterFreeCount==after.UseAfterFreeCount);
}
static void equal(xstrview result,xstrview expected)
{
    assert(result.Data && result.Size==expected.Size && !memcmp(result.Data,expected.Data,result.Size));
    xrtFree((void*)result.Data);
}
static int readSnapshot(void* snapshot)
{
    for (size_t i=0;i<1000;++i) {
        assert(xl6PatternMatchCountV1(snapshot)==2 && xl6PatternMatchPatternIndexV1(snapshot)==0);
        equal(xl6PatternMatchTextV1(snapshot,0),VIEW("你好"));
        equal(xl6PatternMatchTextV1(snapshot,1),VIEW("a/b\0c"));
    }
    return 0;
}
static void oom(void)
{
    unsigned failures=0,completions=0;
    for (unsigned kind=0;kind<5;++kind) {
        size_t point;
        for (point=0;point<2048;++point) {
            void* options=NULL;void* program=NULL;void* snapshot=NULL;xstrview text={0};
            if (kind==2) {options=xl6PatternOptionsCreateV1(VIEW("/"),1024,8,8,4096,1048576,-1);assert(options);}
            if (kind>=3) {program=xl6PatternCreateV1(VIEW("/u/{id}/{*tail}"),-1);assert(program);}
            if (kind==4) {snapshot=xl6PatternMatchCreateV1(program,VIEW("/u/你好/a/b\0c"),-1);assert(snapshot);}
            xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
            assert(xrtMemDebugFailAfter(point));
            void* created=NULL;
            if (kind==0) created=xl6PatternCreateV1(VIEW("/u/{id}/{*tail}"),-1);
            if (kind==1) created=xl6PatternOptionsCreateV1(VIEW("/"),1024,8,8,4096,1048576,-1);
            if (kind==2) created=xl6PatternCreateOptionsV1(options,VIEW("/u/{id}/{*tail}"),-1);
            if (kind==3) created=xl6PatternMatchCreateV1(program,VIEW("/u/你好/a/b\0c"),-1);
            if (kind==4) text=xl6PatternMatchTextV1(snapshot,1);
            bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
            if (hit) {assert(!created && !text.Data && xrtErrorKind(xrtGetError())==XERR_MEMORY);++failures;}
            else {assert((kind==4?text.Data!=NULL:created!=NULL) && !xrtGetError());++completions;}
            if (kind==0 || kind==2) xl6PatternDestroyV1(created);
            if (kind==1) xl6PatternOptionsDestroyV1(created);
            if (kind==3) xl6PatternMatchDestroyV1(created);
            xrtFree((void*)text.Data);xrtClearError();balanced(before);
            xl6PatternMatchDestroyV1(snapshot);xl6PatternDestroyV1(program);xl6PatternOptionsDestroyV1(options);
            if (!hit) break;
        }
        assert(point<2048);
    }
    printf("pattern native OOM: %u actual failures, %u first no-hit completions, balanced\n",failures,completions);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    oom();
    void* program=xl6PatternCreateV1(VIEW("/u/{id}/{*tail}"),-1);assert(program);
    void* snapshot=xl6PatternMatchCreateV1(program,VIEW("/u/你好/a/b\0c"),-1);assert(snapshot);
    xl6PatternDestroyV1(program);
    assert(xl6PatternMatchStartV1(snapshot,0)==3 && xl6PatternMatchEndV1(snapshot,0)==9);
    equal(xl6PatternMatchInputV1(snapshot),VIEW("/u/你好/a/b\0c"));
    equal(xl6PatternMatchSourceV1(snapshot),VIEW("/u/{id}/{*tail}"));
    xthread* threads[4];for (unsigned i=0;i<4;++i) {threads[i]=xrtThreadCreate(readSnapshot,snapshot,0);assert(threads[i]);}
    for (unsigned i=0;i<4;++i) {assert(xrtThreadWait(threads[i])==XWAIT_OK);xrtThreadDestroy(threads[i]);}
    xl6PatternMatchDestroyV1(snapshot);
    program=xl6PatternCreateV1(VIEW("/{*tail}"),-1);assert(program);
    snapshot=xl6PatternMatchCreateV1(program,VIEW("/"),-1);assert(snapshot);
    assert(xl6PatternMatchStartV1(snapshot,0)==1 && xl6PatternMatchEndV1(snapshot,0)==1);
    equal(xl6PatternMatchTextV1(snapshot,0),VIEW(""));
    xl6PatternMatchDestroyV1(snapshot);xl6PatternDestroyV1(program);xrtClearError();balanced(before);
    puts("pattern exact input, native resources contain no language tags, independent snapshots, empty tails and 4000 cross-thread reads passed");return 0;
}
