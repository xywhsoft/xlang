/* In-memory ordinary codec/value/lifecycle tests only. No sockets, packets,
 * host names, network traffic, protocol fuzzing or security tests. */
#include "src/runtime/xl6_net_frame_internal.h"
#include "src/runtime/xl6_net_buffer_abi.h"
#include "src/runtime/xl6_native_handle.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define VIEW(Text) ((xstrview){Text,sizeof(Text)-1})
#ifndef TEST_ROOT
#error TEST_ROOT is required
#endif
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&before.UseAfterFreeCount==after.UseAfterFreeCount);
}
static void handle(void** raw,const XL6_NativeHandlePolicyV1* policy)
{
    xvalue* value=xl6NativeHandleTakeV1(raw,policy);assert(value&&!*raw);
    xrtownershipref ref=xrtValueOwnership(value);xrtownershipscope freeze={0};size_t count;
    assert(xrtOwnershipFreezeTryBegin(&freeze)&&ref.Ops->Count(ref.Data,&count)&&count==1);
    const xrtownershipadapterv1* adapter=xrtValueHandleOwnershipAdapterV1(ref,policy->Ops,policy->Trace);assert(adapter);
    int token=0;assert(adapter->Hold(ref.Data)&&adapter->Claim(ref.Data,&token));adapter->Restore(ref.Data,&token);
    assert(xrtOwnershipScopeEnd(&freeze));adapter->Drop(ref.Data);xrtValueRelease(value);
}
#if TEST_ROOT!=6
static void errorEnd(xl6_error_scope* scope,xerrkind kind)
{xl6_error_result result=xl6ErrorScopeEnd(scope);assert(!result.ok&&xrtErrorKind(result.error)==kind);xrtErrorFree(result.error);assert(!xrtGetError());}
#endif
#if TEST_ROOT==0||TEST_ROOT==1||TEST_ROOT==7
static void buffers(void)
{
    void* a=xl6NetBufferCreateV1(100);void* b=xl6NetBufferCreateV1(100);assert(a&&b);
    assert(xrtNetBufAppend(&((XL6_NetBufferV1*)a)->native,"ab",2)&&xrtNetBufAppend(&((XL6_NetBufferV1*)b)->native,"cd",2));
    assert(xl6NetBufferLengthV1(a)==2&&xl6NetBufferSpanCountV1(a)>0&&xl6NetBufferLimitV1(a)==100);
    assert(xrtMemDebugFailAfter(0));xl6NetBufferAppendMoveV1(a,b);assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    assert(xl6NetBufferLengthV1(a)==4&&!xl6NetBufferLengthV1(b));xl6NetBufferCompactV1(a,-1);
    assert(xl6NetBufferSpanCountV1(a)==1&&xl6NetBufferFindV1(a,'c',0)==2&&xl6NetBufferFindV1(a,0,4)== -1);
    assert(xl6NetBufferConsumeV1(a,99)==4);xl6NetBufferCompactV1(a,0);xl6NetBufferClearV1(a);
    XL6_NetBufferGuardV1 active={0};assert(xl6NetBufferEnterV1(a,&active));
    xl6_error_scope error={0};assert(xl6ErrorScopeBegin(&error));(void)xl6NetBufferLengthV1(a);errorEnd(&error,XERR_STATE);
    assert(xl6ErrorScopeBegin(&error));xl6NetBufferAppendMoveV1(b,a);errorEnd(&error,XERR_STATE);
    xl6NetBufferLeaveV1(&active);assert(!xl6NetBufferLengthV1(a)&&!xl6NetBufferLengthV1(b));
    handle(&a,xl6NetBufferHandlePolicyV1Get());xl6NetBufferDestroyV1(b);
}
#endif
#if TEST_ROOT==1||TEST_ROOT==7
static void bufferValues(void)
{
    void* a=xl6NetBufferCreateV1(50);assert(a);const char text[]={'a',0,'b'};
    xl6NetBufferAppendTextV1(a,(xstrview){text,3},99);xl6NetBufferPrependTextV1(a,VIEW("x"),1);
    xvalue* value=xl6NetBufferReadV1(a,2);xbytesview bytes;assert(value&&xrtValueGetBytes(value,&bytes)&&bytes.Size==2&&!memcmp(bytes.Data,"xa",2));xrtValueRelease(value);
    void* target=xl6BufferCreateV1();assert(target);assert(xl6NetBufferPeekIntoV1(a,target,0,99)==2);
    assert(((xbuffer*)target)->Size==2&&!memcmp(((xbuffer*)target)->Data,"\0b",2));
    assert(xl6NetBufferReadIntoV1(a,target,-1)==2&&!xl6NetBufferLengthV1(a));
    xl6NetBufferAppendBufferV1(a,target,1);value=xl6NetBufferPeekV1(a,0,-1);
    assert(value&&xrtValueGetBytes(value,&bytes)&&bytes.Size==1&&bytes.Data[0]==0);xrtValueRelease(value);
    xl6BufferDestroyV1(target);xl6NetBufferDestroyV1(a);
}
#endif
#if TEST_ROOT==2||TEST_ROOT==3||TEST_ROOT==7
static void lines(void)
{
    void* input=xl6NetBufferCreateV1(100);void* line=xl6NetLineFramerCreateTextV1(VIEW("\r\n"),20,false,100,-1);assert(input&&line);
    assert(xrtNetBufAppend(&((XL6_NetBufferV1*)input)->native,"a\r",2));xl6NetLineFramerAppendMoveV1(line,input);
    void* more=xl6NetLineFramerNextV1(line);assert(more&&xl6NetFrameResultStatusV1(more)==0&&xl6NetLineFramerBufferedV1(line)==2);
    handle(&more,xl6NetFrameResultHandlePolicyV1Get());
    assert(xrtNetBufAppend(&((XL6_NetBufferV1*)input)->native,"\n\r\n",3));xl6NetLineFramerAppendMoveV1(line,input);
    void* result=xl6NetLineFramerNextV1(line);assert(result&&xl6NetFrameResultStatusV1(result)==1);
    assert(xl6NetFrameResultPayloadSizeV1(result)==1&&xl6NetFrameResultFrameSizeV1(result)==3&&xl6NetFrameResultDeclaredV1(result)==1);
    void* same=xl6NetLineFramerNextV1(line);assert(same);xl6NetLineFramerCommitV1(line,same);
    assert(xl6NetLineFramerBufferedV1(line)==2);xl6NetFrameResultDestroyV1(same);
    xl6_error_scope error={0};assert(xl6ErrorScopeBegin(&error));xl6NetLineFramerCommitV1(line,result);errorEnd(&error,XERR_STATE);
    assert(xl6NetLineFramerBufferedV1(line)==2);
    same=xl6NetLineFramerNextV1(line);assert(same&&xl6NetFrameResultPayloadSizeV1(same)==0);
    xl6NetLineFramerCommitV1(line,same);xl6NetFrameResultDestroyV1(same);
    void* other=xl6NetLineFramerCreateTextV1(VIEW("\r\n"),20,false,100,-1);assert(other);
    assert(xl6ErrorScopeBegin(&error));xl6NetLineFramerCommitV1(other,result);errorEnd(&error,XERR_STATE);xl6NetLineFramerDestroyV1(other);
    ((XL6_NetLineFramerV1*)line)->revision=UINT64_MAX;
    assert(xl6ErrorScopeBegin(&error));xl6NetLineFramerResetV1(line);errorEnd(&error,XERR_RANGE);
    ((XL6_NetLineFramerV1*)line)->revision=1;
    xl6NetLineFramerClearV1(line);assert(xl6NetFrameResultPayloadSizeV1(result)==1);handle(&result,xl6NetFrameResultHandlePolicyV1Get());
    handle(&line,xl6NetLineFramerHandlePolicyV1Get());xl6NetBufferDestroyV1(input);
}
#endif
#if TEST_ROOT==4||TEST_ROOT==5||TEST_ROOT==7
static void lengths(void)
{
    for(int width=1;width<=8;++width)for(int little=0;little<=1;++little){
        void* length=xl6NetLengthFramerCreateV1(1,width,0,1+width,100,little!=0,100);assert(length);
        void* input=xl6NetBufferCreateV1(100);assert(input);uint8_t data[12]={0};data[little?1:width]=3;memcpy(data+1+width,"a\0b",3);
        assert(xrtNetBufAppend(&((XL6_NetBufferV1*)input)->native,data,(size_t)width));xl6NetLengthFramerAppendMoveV1(length,input);
        void* more=xl6NetLengthFramerNextV1(length);assert(more&&xl6NetFrameResultStatusV1(more)==0);xl6NetFrameResultDestroyV1(more);
        assert(xrtNetBufAppend(&((XL6_NetBufferV1*)input)->native,data+width,4));xl6NetLengthFramerAppendMoveV1(length,input);
        void* result=xl6NetLengthFramerNextV1(length);assert(result&&xl6NetFrameResultPayloadOffsetV1(result)==width+1&&xl6NetFrameResultPayloadSizeV1(result)==3);
        xl6NetLengthFramerCommitV1(length,result);
        assert(xl6NetFrameResultDeclaredV1(result)==3&&!xl6NetLengthFramerBufferedV1(length));
        xl6_error_scope error={0};assert(xl6ErrorScopeBegin(&error));xl6NetLengthFramerCommitV1(length,result);errorEnd(&error,XERR_STATE);
        handle(&result,xl6NetFrameResultHandlePolicyV1Get());xl6NetBufferDestroyV1(input);handle(&length,xl6NetLengthFramerHandlePolicyV1Get());
    }
}
#endif
#if TEST_ROOT==3||TEST_ROOT==5||TEST_ROOT==7
static void directOutput(void)
{
    void* target=xl6BufferCreateReserveV1(100);assert(target);
#if TEST_ROOT==3||TEST_ROOT==7
    void* line=xl6NetLineFramerCreateTextV1(VIEW("\n"),20,false,100,-1);assert(line);
    xl6NetLineFramerAppendTextV1(line,VIEW("one\n"),-1);
    assert(xrtMemDebugFailAfter(0));assert(xl6NetLineFramerNextIntoV1(line,target,true));assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    assert(((xbuffer*)target)->Size==3&&!memcmp(((xbuffer*)target)->Data,"one",3));xl6NetLineFramerDestroyV1(line);
#endif
#if TEST_ROOT==5||TEST_ROOT==7
    void* length=xl6NetLengthFramerCreateV1(0,1,0,1,100,false,100);assert(length);const char data[]={3,'t','w','o'};
    xl6NetLengthFramerAppendTextV1(length,(xstrview){data,4},-1);size_t before=((xbuffer*)target)->Size;
    assert(xrtMemDebugFailAfter(0));assert(xl6NetLengthFramerNextIntoV1(length,target,true));assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    assert(((xbuffer*)target)->Size==before+3&&!memcmp(((xbuffer*)target)->Data+before,"two",3));xl6NetLengthFramerDestroyV1(length);
#endif
    xl6BufferDestroyV1(target);
}
#endif
#if TEST_ROOT==6
static void resultOnly(void)
{
    XL6_NetBufferV1 input={0};assert(xrtNetBufInit(&input.native,NULL)&&xrtNetBufAppend(&input.native,"abc",3));
    xnetframe frame={1,2,3,UINT64_MAX};void* result=xl6NetFrameCopyV1(&input,&frame,1,0);assert(result);
    xrtNetBufClear(&input.native);assert(xl6NetFrameResultDeclaredV1(result)==UINT64_MAX&&xl6NetFrameResultPayloadSizeV1(result)==2);
    handle(&result,xl6NetFrameResultHandlePolicyV1Get());
}
#endif
#if TEST_ROOT==7
static void prefixes(void)
{
    size_t terminal=SIZE_MAX;
    for(size_t point=0;point<256;++point){xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        void *buffer=NULL,*line=NULL,*length=NULL,*result=NULL; xvalue* value=NULL;xl6_error_scope scope={0};
        assert(xl6ErrorScopeBegin(&scope)&&xrtMemDebugFailAfter(point));
#define REQUIRE(Expression) do{if(!(Expression)||scope.failed||xrtGetError())goto done;}while(0)
        REQUIRE((buffer=xl6NetBufferCreateV1(100)));xl6NetBufferAppendTextV1(buffer,VIEW("abc\n"),-1);REQUIRE(!scope.failed);
        REQUIRE((value=xl6NetBufferPeekV1(buffer,0,-1)));xrtValueRelease(value);value=NULL;
        REQUIRE((line=xl6NetLineFramerCreateTextV1(VIEW("\n"),20,false,100,-1)));
        xl6NetLineFramerAppendMoveV1(line,buffer);REQUIRE(!scope.failed);
        REQUIRE((result=xl6NetLineFramerNextV1(line)));xl6NetLineFramerCommitV1(line,result);REQUIRE(!scope.failed);xl6NetFrameResultDestroyV1(result);result=NULL;
        REQUIRE((length=xl6NetLengthFramerCreateV1(0,1,0,1,100,false,100)));
        const char data[]={3,'a',0,'b'};xl6NetLengthFramerAppendTextV1(length,(xstrview){data,4},-1);REQUIRE(!scope.failed);
        REQUIRE((result=xl6NetLengthFramerNextV1(length)));REQUIRE((value=xl6NetFrameResultBytesV1(result)));
        xl6NetLengthFramerCommitV1(length,result);REQUIRE(!scope.failed);
done:;
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        xrtValueRelease(value);xl6NetFrameResultDestroyV1(result);xl6NetLengthFramerDestroyV1(length);xl6NetLineFramerDestroyV1(line);xl6NetBufferDestroyV1(buffer);
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope);assert(hit?!outcome.ok&&xrtErrorKind(outcome.error)==XERR_MEMORY:outcome.ok&&!outcome.error);
        xrtErrorFree(outcome.error);assert(!xrtGetError());balanced(before);if(!hit){terminal=point;break;}
#undef REQUIRE
    }
    assert(terminal!=SIZE_MAX);printf("Frame owned prefixes=%zu first-no-hit=%zu balanced\n",terminal+1,terminal);
}
#endif
int main(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
#if TEST_ROOT==0||TEST_ROOT==1||TEST_ROOT==7
    buffers();
#endif
#if TEST_ROOT==1||TEST_ROOT==7
    bufferValues();
#endif
#if TEST_ROOT==2||TEST_ROOT==3||TEST_ROOT==7
    lines();
#endif
#if TEST_ROOT==4||TEST_ROOT==5||TEST_ROOT==7
    lengths();
#endif
#if TEST_ROOT==3||TEST_ROOT==5||TEST_ROOT==7
    directOutput();
#endif
#if TEST_ROOT==6
    resultOnly();
#endif
#if TEST_ROOT==7
    prefixes();
#endif
    assert(!xrtGetError());balanced(before);printf("Frame native root %d: offline lifecycle balanced\n",TEST_ROOT);return 0;
}
