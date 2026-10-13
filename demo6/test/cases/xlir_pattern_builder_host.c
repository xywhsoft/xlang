#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_pattern_internal.h"
#include <assert.h>
#include <stdio.h>
#define VIEW(s) (xstrview){s,sizeof(s)-1}
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount &&
        before.UseAfterFreeCount==after.UseAfterFreeCount);
}
static xvalue* stateCreate(void* builder)
{
    xvalue* identity=xl6PatternBuilderIdentityV1(builder);assert(identity);
    xvalue* state=xl6PatternStateCreateV1(identity);assert(state);
    assert(xrtValueObjectGet(state,VIEW("identity"))==identity);
    assert(xrtValueType(xrtValueObjectGet(state,VIEW("tags")))==XVALUE_INT_MAP);
    xrtValueRelease(identity);return state;
}
static void stateOom(void)
{
    assert(!xl6PatternStateCreateV1(NULL) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT);
    xrtClearError(); unsigned failures=0;
    for(size_t point=0;;++point) {
        assert(point<2048);xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        xvalue* identity=xrtValueUInt(1);assert(identity);
        assert(xrtMemDebugFailAfter(point));
        xvalue* state=xl6PatternStateCreateV1(identity);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(hit){assert(!state && xrtErrorKind(xrtGetError())==XERR_MEMORY);++failures;}
        else {
            assert(state && !xrtGetError());
            assert(xrtValueObjectGet(state,VIEW("identity"))==identity);
            assert(xrtValueType(xrtValueObjectGet(state,VIEW("tags")))==XVALUE_INT_MAP);
        }
        xrtClearError();xrtValueRelease(state);xrtValueRelease(identity);balanced(before);
        if(!hit)break;
    }
    assert(failures>0);printf("pattern state factory OOM: %u actual failures, balanced\n",failures);
}
static void* add(void* builder,xvalue* state,xstrview expression,xvalue* tag)
{
    void* edit=xl6PatternEditAddV1(builder,expression,0,-1);assert(edit);
    void* key=xl6PatternKeyCreateV1(edit,0);assert(key);
    xl6PatternEditApplyV1(edit,state,tag);assert(!xrtGetError());
    xl6PatternEditCancelV1(edit);xl6PatternEditDestroyV1(edit);return key;
}
static void* threadBuilder;
static void* threadEdit;
static xvalue* strings(void)
{
    xvalue* a=xrtValueArray();assert(a);
    xvalue* s=xrtValueString(VIEW("/a/{id}"));assert(s);assert(xrtValueArrayAppend(a,s));xrtValueRelease(s);
    s=xrtValueString(VIEW("/b/{id}"));assert(s);assert(xrtValueArrayAppend(a,s));xrtValueRelease(s);return a;
}
static xvalue* integers(int64 value)
{
    xvalue* a=xrtValueArray();assert(a);
    for (unsigned i=0;i<2;++i) {xvalue* n=xrtValueInt(value);assert(n);assert(xrtValueArrayAppend(a,n));xrtValueRelease(n);}return a;
}
static int wrongThread(void* ignored)
{
    (void)ignored;assert(xl6PatternBuilderCountV1(threadBuilder)==0);
    assert(xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    xl6PatternEditCancelV1(threadEdit);
    assert(xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();return 0;
}
static void functional(void)
{
    void* b=xl6PatternBuilderCreateV1();assert(b);xvalue* state=stateCreate(b);
    xvalue* tag=xrtValueInt(42);assert(tag);
    void* k=add(b,state,VIEW("/u/{id}"),tag);
    assert(xl6PatternBuilderCountV1(b)==1 && xl6PatternBuilderContainsV1(b,k));
    xvalue* snapshot=xl6PatternBuilderTagsV1(b,state);assert(snapshot);
    void* p=xl6PatternBuilderCompileV1(b);assert(p);
    uint64_t version=xl6PatternBuilderVersionV1(b),id=xl6PatternKeyIdV1(k);
    void* e=xl6PatternEditSetV1(b,k,VIEW("/v/{id}"),5,-1);assert(e);
    xvalue* changed=xrtValueInt(84);assert(changed);
    xl6PatternEditApplyV1(e,state,changed);assert(!xrtGetError());
    xl6PatternEditDestroyV1(e);assert(xl6PatternBuilderVersionV1(b)==version+1);
    int64 n=0;assert(xrtValueGetInt(xrtValueIntMapGet(snapshot,xl6PatternTagKeyV1(p,0)),&n) && n==42);
    xvalue* current=xl6PatternBuilderTagsV1(b,state);assert(current);
    assert(xrtValueGetInt(xrtValueIntMapGet(current,xl6PatternTagKeyV1(p,0)),&n) && n==84);
    assert(xl6PatternTestV1(p,VIEW("/u/a"),-1));
    void* other=xl6PatternBuilderCreateV1();assert(other);
    assert(!xl6PatternBuilderContainsV1(other,k));
    assert(!xl6PatternEditRemoveV1(other,k) && xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    e=xl6PatternEditRemoveV1(b,k);assert(e);
    xl6PatternEditApplyV1(e,state,xrtValueNull());assert(!xrtGetError());xl6PatternEditDestroyV1(e);
    assert(!xl6PatternBuilderContainsV1(b,k));
    assert(!xl6PatternEditSetV1(b,k,VIEW("/bad"),0,-1) && xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    void* reused=add(b,state,VIEW("/new"),xrtValueNull());assert(xl6PatternKeyIdV1(reused)!=id);
    e=xl6PatternEditClearV1(b);assert(e);
    xl6PatternEditApplyV1(e,state,xrtValueNull());assert(!xrtGetError());xl6PatternEditDestroyV1(e);
    assert(xl6PatternBuilderCountV1(b)==0 && !xl6PatternBuilderContainsV1(b,reused));
    e=xl6PatternEditAddV1(b,VIEW("/cancelled"),0,-1);assert(e);
    void* cancelled=xl6PatternKeyCreateV1(e,0);assert(cancelled && !xl6PatternBuilderContainsV1(b,cancelled));
    xl6PatternEditCancelV1(e);xl6PatternEditDestroyV1(e);
    void* committed=add(b,state,VIEW("/committed"),xrtValueNull());
    assert(xl6PatternKeyIdV1(cancelled)==xl6PatternKeyIdV1(committed));
    assert(!xl6PatternBuilderContainsV1(b,cancelled) && xl6PatternBuilderContainsV1(b,committed));
    xl6PatternKeyDestroyV1(cancelled);xl6PatternKeyDestroyV1(committed);
    e=xl6PatternEditClearV1(b);assert(e);xl6PatternEditApplyV1(e,state,xrtValueNull());
    assert(!xrtGetError());xl6PatternEditDestroyV1(e);
    e=xl6PatternEditAddV1(b,VIEW("/thread"),0,-1);assert(e);
    threadBuilder=b;threadEdit=e;xthread* t=xrtThreadCreate(wrongThread,NULL,0);assert(t);
    assert(xrtThreadWait(t)==XWAIT_OK);xrtThreadDestroy(t);
    assert(xl6PatternEditCountV1(e)==1);xl6PatternEditCancelV1(e);xl6PatternEditDestroyV1(e);
    /* A numerically equal but foreign marker is rejected without unboxing an
     * arbitrary native handle. An orphan edit is safe to cancel and destroy. */
    e=xl6PatternEditAddV1(b,VIEW("/orphan"),0,-1);assert(e);
    xvalue* forged=xrtValueUInt(1);assert(forged);
    assert(xrtValueObjectSetTake(state,VIEW("identity"),&forged));
    xl6PatternEditApplyV1(e,state,xrtValueNull());assert(xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    assert(xl6PatternBuilderCountV1(b)==0);
    xl6PatternBuilderDestroyV1(b);
    assert(xl6PatternEditCountV1(e)==0 && xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    xl6PatternEditCancelV1(e);xl6PatternEditDestroyV1(e);
    xl6PatternBuilderDestroyV1(other);xl6PatternKeyDestroyV1(k);xl6PatternKeyDestroyV1(reused);
    xl6PatternDestroyV1(p);xrtValueRelease(state);xrtValueRelease(snapshot);xrtValueRelease(current);
    xrtValueRelease(tag);xrtValueRelease(changed);
}
static void oom(void)
{
    unsigned failures=0,completions=0;
    for (unsigned kind=0;kind<10;++kind) for (size_t point=0;;++point) {
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        assert(point<2048);void* b=NULL;void* e=NULL;void* k=NULL;xvalue* state=NULL;void* result=NULL;xvalue* snapshot=NULL;
        xvalue* tag=xrtValueInt(17);assert(tag);
        xvalue *expressions=NULL,*priorities=NULL,*sizes=NULL,*payload=NULL;
        if (kind) {b=xl6PatternBuilderCreateV1();assert(b);state=stateCreate(b);}
        if (kind>=3) k=add(b,state,VIEW("/old"),tag);
        if (kind==7 || kind==8) {expressions=strings();priorities=integers(0);sizes=integers(-1);payload=integers(99);}
        if (kind==8) {e=xl6PatternEditAddManyV1(b,expressions,priorities,sizes);assert(e);}
        if (kind==2) {e=xl6PatternEditAddV1(b,VIEW("/new"),0,-1);assert(e);}
        if (kind==4 || kind==5) {
            snapshot=xl6PatternBuilderTagsV1(b,state);assert(snapshot);
            e=kind==4?xl6PatternEditSetV1(b,k,VIEW("/changed"),0,-1):xl6PatternEditClearV1(b);assert(e);
        }
        if (kind==9) {snapshot=xl6PatternBuilderTagsV1(b,state);assert(snapshot);e=xl6PatternEditRemoveV1(b,k);assert(e);}
        uint64_t version=kind?xl6PatternBuilderVersionV1(b):0;
        assert(xrtMemDebugFailAfter(point));
        if (kind==0) result=xl6PatternBuilderCreateV1();
        if (kind==1) result=xl6PatternEditAddV1(b,VIEW("/new"),0,-1);
        if (kind==2) result=xl6PatternKeyCreateV1(e,0);
        if (kind==3) result=xl6PatternBuilderTagsV1(b,state);
        if (kind==4 || kind==5) xl6PatternEditApplyV1(e,state,tag);
        if (kind==6) result=xl6PatternBuilderCompileV1(b);
        if (kind==7) result=xl6PatternEditAddManyV1(b,expressions,priorities,sizes);
        if (kind==8) xl6PatternEditApplyManyV1(e,state,payload);
        if (kind==9) xl6PatternEditApplyV1(e,state,xrtValueNull());
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if (hit) {
            assert(!result && xrtErrorKind(xrtGetError())==XERR_MEMORY);++failures;
            if (kind==4 || kind==5 || kind==8 || kind==9) {
                assert(xl6PatternBuilderCountV1(b)==1 && xl6PatternBuilderVersionV1(b)==version);
                xrtClearError();xvalue* unchanged=xl6PatternBuilderTagsV1(b,state);assert(unchanged);
                int64 n;assert(xrtValueGetInt(xrtValueIntMapGet(unchanged,(int64_t)xl6PatternKeyIdV1(k)),&n) && n==17);
                xrtValueRelease(unchanged);
            }
        } else {assert(!xrtGetError());++completions;if (kind==4 || kind==5 || kind==8 || kind==9) assert(xl6PatternBuilderVersionV1(b)==version+1);else assert(result);}
        xrtClearError();
        if (kind==0) xl6PatternBuilderDestroyV1(result);
        if (kind==1) xl6PatternEditDestroyV1(result);
        if (kind==2) xl6PatternKeyDestroyV1(result);
        if (kind==3) xrtValueRelease(result);
        if (kind==6) xl6PatternDestroyV1(result);
        if (kind==7) xl6PatternEditDestroyV1(result);
        /* Capacity reservation and compiled caching may keep new allocations
         * without changing logical state. Balance the complete operation. */
        xl6PatternEditDestroyV1(e);xl6PatternKeyDestroyV1(k);xl6PatternBuilderDestroyV1(b);
        xrtValueRelease(snapshot);xrtValueRelease(state);xrtValueRelease(tag);
        xrtValueRelease(expressions);xrtValueRelease(priorities);xrtValueRelease(sizes);xrtValueRelease(payload);
        balanced(before);
        if (!hit) break;
    }
    printf("pattern builder adapter OOM: %u actual failures, %u first no-hit completions\n",failures,completions);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    assert(xl6PatternTagKeyFromId(0)==0 && xl6PatternTagKeyFromId(INT64_MAX)==INT64_MAX);
    assert(xl6PatternTagKeyFromId(UINT64_C(1)<<63)==INT64_MIN && xl6PatternTagKeyFromId(UINT64_MAX)==-1);
    stateOom();functional();oom();xrtClearError();balanced(before);
    puts("pattern builder adapter: identity, stale keys, COW snapshots, orphan edits, thread checks and whole-sweep balance passed");return 0;
}
