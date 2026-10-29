#include "src/runtime/xl6_text_value.h"
#include "test/support/array_producer_test.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define VIEW(s) (xstrview){s,sizeof(s)-1}
static void text(xstrview value,const char* expected,size_t length)
{
    assert(value.Data&&value.Size==length&&!memcmp(value.Data,expected,length));
    assert(value.Data[length]==0);xrtFree((void*)value.Data);
}
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount);
    assert(before.UseAfterFreeCount==after.UseAfterFreeCount);
}
static int wrongThread(void* handle)
{
    assert(!xl6RegexIteratorHasNextV1(handle)&&xrtErrorKind(xrtGetError())==XERR_STATE);
    xrtClearError();xl6RegexIteratorCloseV1(handle);
    assert(xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();return 0;
}
static int wrongSplitThread(void* handle)
{
    assert(!xl6RegexSplitHasNextV1(handle)&&xrtErrorKind(xrtGetError())==XERR_STATE);
    xrtClearError();xl6RegexSplitCloseV1(handle);
    assert(xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();return 0;
}
typedef struct ThreadIdentityProbe {void* regex;void* iterator;void* split;} ThreadIdentityProbe;
static int createOnThread(void* context)
{
    ThreadIdentityProbe* p=context;
    p->iterator=xl6RegexIteratorCreateV1(p->regex,VIEW("abc"),0,-1,-1);
    p->split=xl6RegexSplitCreateV1(p->regex,VIEW("abc"),-1,0,-1);
    assert(p->iterator && p->split);return 0;
}
static int rejectExitedOwner(void* context)
{
    ThreadIdentityProbe* p=context;
    wrongThread(p->iterator);wrongSplitThread(p->split);return 0;
}
static void exitedThreadIdentity(void* regex)
{
    ThreadIdentityProbe p={regex,NULL,NULL};
    xthread* creator=xrtThreadCreate(createOnThread,&p,0);assert(creator);
    assert(xrtThreadWait(creator)==XWAIT_OK);xrtThreadDestroy(creator);
    /* Platform IDs may be recycled, but the owned creator object may not. */
    for (size_t i=0;i<32;++i) {
        xthread* other=xrtThreadCreate(rejectExitedOwner,&p,0);assert(other);
        assert(xrtThreadWait(other)==XWAIT_OK);xrtThreadDestroy(other);
    }
    xl6RegexIteratorDestroyV1(p.iterator);xl6RegexSplitDestroyV1(p.split);
}
static void snapshots(void* regex)
{
    char input[]="a\0b";
    void* it=xl6RegexIteratorCreateV1(regex,(xstrview){input,3},0,-1,-1);assert(it);
    input[0]='z';
    assert(xl6RegexIteratorHasNextV1(it));
    xthread* t=xrtThreadCreate(wrongThread,it,0);assert(t);
    assert(xrtThreadWait(t)==XWAIT_OK&&xrtThreadExitCode(t)==0);xrtThreadDestroy(t);
    assert(xrtMemDebugFailAfter(0));
    assert(!xl6RegexIteratorSnapshotV1(it)&&xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();assert(xrtErrorKind(xrtGetError())==XERR_MEMORY);xrtClearError();
    assert(xl6RegexIteratorHasNextV1(it));
    void* first=xl6RegexIteratorSnapshotV1(it);assert(first);
    xl6RegexIteratorConsumeV1(it);text(xl6RegexMatchTextV1(first,0),"a",1);
    assert(xl6RegexIteratorHasNextV1(it));
    void* second=xl6RegexIteratorSnapshotV1(it);assert(second);
    xl6RegexIteratorConsumeV1(it);text(xl6RegexMatchTextV1(second,0),"\0",1);
    xl6RegexIteratorCloseV1(it);xl6RegexIteratorCloseV1(it);
    assert(xl6RegexIteratorClosedV1(it)&&!xl6RegexIteratorHasNextV1(it));
    xl6RegexIteratorDestroyV1(it);
    text(xl6RegexMatchInputV1(first),"a\0b",3);xl6RegexMatchDestroyV1(first);xl6RegexMatchDestroyV1(second);
    void* separator=xl6RegexCreateV1(VIEW(","),0,1024,8,-1);assert(separator);
    void* split=xl6RegexSplitCreateV1(separator,VIEW("a,b"),-1,0,-1);assert(split);
    xl6RegexDestroyV1(separator);assert(xl6RegexSplitHasNextV1(split));
    t=xrtThreadCreate(wrongSplitThread,split,0);assert(t);
    assert(xrtThreadWait(t)==XWAIT_OK&&xrtThreadExitCode(t)==0);xrtThreadDestroy(t);
    assert(xrtMemDebugFailAfter(0));
    assert(!xl6RegexSplitSnapshotV1(split)&&xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();assert(xrtErrorKind(xrtGetError())==XERR_MEMORY);xrtClearError();
    assert(xl6RegexSplitHasNextV1(split));
    void* part=xl6RegexSplitSnapshotV1(split);assert(part);
    xl6RegexSplitConsumeV1(split);xl6RegexSplitDestroyV1(split);
    text(xl6RegexSplitPartTextV1(part),"a",1);assert(xl6RegexSplitPartStartV1(part)==0);
    xl6RegexSplitPartDestroyV1(part);
    /* Retaining snapshots scales with spans, not whole-input copies. */
    char large[8192];memset(large,'a',sizeof(large));
    it=xl6RegexIteratorCreateV1(regex,(xstrview){large,sizeof(large)},0,-1,-1);assert(it);
    assert(xl6RegexIteratorHasNextV1(it));
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    void* results[100];
    for(size_t i=0;i<100;++i){
        assert(xl6RegexIteratorHasNextV1(it));results[i]=xl6RegexIteratorSnapshotV1(it);assert(results[i]);
        xl6RegexIteratorConsumeV1(it);
    }
    xrtMemDebugSnapshot(&after);assert(after.LiveBytes-before.LiveBytes<65536);
    xl6RegexIteratorDestroyV1(it);
    for(size_t i=0;i<100;++i){assert(xl6RegexMatchStartV1(results[i],0)==(int64_t)i);xl6RegexMatchDestroyV1(results[i]);}
}
static void failures(void* regex)
{
    xvalue* patterns=xrtValueArray();assert(patterns);
    assert(xrtValueArrayAppendNew(patterns,xrtValueString(VIEW("a"))));
    assert(xrtValueArrayAppendNew(patterns,xrtValueString(VIEW("b"))));
    void* set=xl6RegexSetCreateV1(patterns,0,1024,8,4,-1);assert(set);
    unsigned rejected=0,completed=0;
    for(unsigned kind=0;kind<9;++kind){
        unsigned point;
        for(point=0;point<512;++point){
            xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
            xbuffer* target=NULL;
            if(kind==3){target=xrtBufferCreate();assert(target);assert(xrtBufferAppend(target,(xbytesview){(const uint8*)"p",1}));}
            void* prepared=NULL;
            if(kind==7)prepared=xl6RegexIteratorCreateV1(regex,VIEW("a\0b"),0,-1,-1);
            if(kind==8)prepared=xl6RegexSplitCreateV1(regex,VIEW("a\0b"),-1,1,-1);
            if(kind>=7)assert(prepared);
            assert(xrtMemDebugFailAfter(point));
            void* handle=NULL;xstrview output={NULL,0};xvalue* hits=NULL;bool advanced=false;
            if(kind==0)handle=xl6RegexIteratorCreateV1(regex,VIEW("a\0b"),0,-1,-1);
            if(kind==1)handle=xl6RegexSplitCreateV1(regex,VIEW("a\0b"),-1,1,-1);
            if(kind==2)output=xl6RegexReplaceV1(regex,VIEW("a\0b"),VIEW("<$0>"),-1,-1,-1);
            if(kind==3){
                char large[1024];memset(large,'a',sizeof(large));
                xl6RegexReplaceBufferV1(regex,target,(xstrview){large,sizeof(large)},VIEW("<$0>"),-1,-1,-1);
            }
            if(kind==4)handle=xl6RegexSetCreateV1(patterns,0,1024,8,4,-1);
            if(kind==5)hits=xl6RegexSetHitsV2(XL6_TEST_INT_ARRAY_IDS,set,VIEW("ab"),0,-1);
            if(kind==6)output=xl6RegexSetPatternV1(set,0);
            if(kind==7)advanced=xl6RegexIteratorHasNextV1(prepared);
            if(kind==8)advanced=xl6RegexSplitHasNextV1(prepared);
            bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
            if(hit){
                assert(!handle&&!output.Data&&!hits&&xrtErrorKind(xrtGetError())==XERR_MEMORY);++rejected;
                if(target){xbytesview v=xrtBufferView(target);assert(v.Size==1&&v.Data[0]=='p');}
                if(kind==7)assert(!advanced&&xl6RegexIteratorClosedV1(prepared));
                if(kind==8)assert(!advanced&&xl6RegexSplitClosedV1(prepared));
            }else{assert((handle||output.Data||hits||target||advanced)&&!xrtGetError());++completed;}
            if(kind==0)xl6RegexIteratorDestroyV1(handle);
            else if(kind==1)xl6RegexSplitDestroyV1(handle);
            else xl6RegexSetDestroyV1(handle);
            if(kind==7)xl6RegexIteratorDestroyV1(prepared);
            if(kind==8)xl6RegexSplitDestroyV1(prepared);
            xrtFree((void*)output.Data);xrtValueRelease(hits);xrtBufferDestroy(target);xrtClearError();balanced(before);
            if(!hit)break;
        }
        assert(point<512);
    }
    xl6RegexSetDestroyV1(set);xrtValueRelease(patterns);
    printf("regex extended native OOM: %u actual failures, %u first no-hit completions, balanced\n",rejected,completed);
}
static void byteBoundaries(void)
{
    void* regex=xl6RegexCreateV1(VIEW("\\C"),0,1024,8,-1);assert(regex);
    void* match=xl6RegexMatchCreateV1(regex,VIEW("\xE4\xBD\xA0"),0,0,-1);assert(match);
    assert(xl6RegexMatchStartV1(match,0)==0&&xl6RegexMatchEndV1(match,0)==1);
    assert(!xl6RegexMatchTextV1(match,0).Data&&xrtErrorKind(xrtGetError())==XERR_VALUE);xrtClearError();
    assert(!xl6RegexReplaceV1(regex,VIEW("\xE4\xBD\xA0"),VIEW("<$0>"),-1,-1,-1).Data);
    assert(xrtErrorKind(xrtGetError())==XERR_VALUE);xrtClearError();
    xbuffer* target=xrtBufferCreate();assert(target);
    assert(xl6RegexReplaceBufferV1(regex,target,VIEW("\xE4\xBD\xA0"),VIEW("<$0>"),-1,-1,-1)==3);
    xbytesview bytes=xrtBufferView(target);
    assert(bytes.Size==9&&!memcmp(bytes.Data,"<\xE4><\xBD><\xA0>",9));
    xrtBufferDestroy(target);xl6RegexMatchDestroyV1(match);xl6RegexDestroyV1(regex);
    regex=xl6RegexCreateV1(VIEW("(\\C)"),0,1024,8,-1);assert(regex);
    void* split=xl6RegexSplitCreateV1(regex,VIEW("\xE4\xBD\xA0"),-1,1,-1);assert(split);
    assert(xl6RegexSplitHasNextV1(split));xl6RegexSplitConsumeV1(split);
    assert(xl6RegexSplitHasNextV1(split));void* part=xl6RegexSplitSnapshotV1(split);assert(part);
    assert(!xl6RegexSplitPartTextV1(part).Data&&xrtErrorKind(xrtGetError())==XERR_VALUE);xrtClearError();
    xl6RegexSplitPartDestroyV1(part);xl6RegexSplitDestroyV1(split);xl6RegexDestroyV1(regex);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    void* regex=xl6RegexCreateV1(VIEW("."),0,1024,8,-1);assert(regex);
    snapshots(regex);exitedThreadIdentity(regex);
    text(xl6RegexReplaceV1(regex,VIEW("a\0b"),VIEW("<$0>"),-1,-1,-1),"<a><\0><b>",9);
    text(xl6RegexReplaceV1(regex,VIEW("abc"),VIEW(""),-1,-1,-1),"",0);
    text(xl6RegexReplaceV1(regex,VIEW(""),VIEW("x"),-1,-1,-1),"",0);
    xbuffer* target=xrtBufferCreate();assert(target);
    assert(xrtBufferAppend(target,(xbytesview){(const uint8*)"ab",2}));
    xbytesview alias=xrtBufferView(target);
    assert(xl6RegexReplaceBufferV1(regex,target,(xstrview){(const char*)alias.Data,2},VIEW("<$0>"),-1,-1,-1)==2);
    alias=xrtBufferView(target);assert(alias.Size==8&&!memcmp(alias.Data,"ab<a><b>",8));xrtBufferDestroy(target);
    failures(regex);byteBoundaries();xl6RegexDestroyV1(regex);xrtClearError();balanced(before);
    puts("regex extended snapshots, retry, thread confinement, shared input and transactions passed");return 0;
}
