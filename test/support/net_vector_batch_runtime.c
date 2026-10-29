/* Real selected providers and literal own IPv4/IPv6 loopback endpoints only.
 * No DNS, external traffic, probes, multicast, or network-security tests. */
#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "src/runtime/xl6_net_datagram_batch_internal.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_native_class_borrow.h"
#include "src/runtime/xl6_string_type.h"
#include "src/runtime/xl6_net_socket_owner.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifndef TEST_ROOT
#error TEST_ROOT must select an actual closure
#endif
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);assert(!xrtGetError());
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static void checked(xl6_error_scope* scope,xerrkind kind)
{xl6_error_result outcome=xl6ErrorScopeEnd(scope);assert(!outcome.ok&&xrtErrorKind(outcome.error)==kind);
 xrtErrorFree(outcome.error);assert(!xrtGetError());}
#if TEST_ROOT==0 || TEST_ROOT==7
static void progress(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    void* value=xl6NetBatchProgressCreateV1();assert(value&&!xl6NetBatchProgressAttemptedV1(value));
    assert(!xl6NetBatchProgressCountV1(value)&&!xl6NetBatchProgressStartV1(value)&&xl6NetBatchProgressStatusV1(value)==XNET_RESULT_ERROR);
    *(XL6_NetBatchProgressV1*)value=(XL6_NetBatchProgressV1){3,4,XNET_RESULT_AGAIN,true};
    assert(xl6NetBatchProgressCountV1(value)==3&&xl6NetBatchProgressStartV1(value)==4&&xl6NetBatchProgressAttemptedV1(value));
    xl6NetBatchProgressClearV1(value);assert(!xl6NetBatchProgressAttemptedV1(value)&&!xl6NetBatchProgressCountV1(value));
    xl6NetBatchProgressDestroyV1(value);
    xl6_error_scope failure={0};assert(xl6ErrorScopeBegin(&failure)&&xrtMemDebugFailAfter(0));
    assert(!xl6NetBatchProgressCreateV1()&&xrtMemDebugFailTriggered());xrtMemDebugFailClear();checked(&failure,XERR_MEMORY);
    balanced(&before);
}
#endif
#if TEST_ROOT==1 || TEST_ROOT>=4
enum {BUFFER_TAG=101,PACKET_TAG=102,ARRAY_ID=201,BUFFER_ID=202,PACKET_ID=203};
static xvalue* objectTake(void* raw,const XL6_NativeHandlePolicyV1* policy,int tag)
{
    xvalue* handle=xl6NativeHandleTakeV1(&raw,policy);assert(handle&&!raw);
    xvalue* object=xrtValueObject(),*kind=xrtValueInt(tag);assert(object&&kind);
    assert(xrtValueObjectSet(object,(xstrview){"$xlir.class.runtime-type",24},kind));
    assert(xrtValueObjectSet(object,(xstrview){"handle",6},handle));xrtValueRelease(kind);xrtValueRelease(handle);return object;
}
#if TEST_ROOT==1 || TEST_ROOT==6 || TEST_ROOT==7
static XL6_NativeClassFieldContextV1 bufferContext(void)
{return (XL6_NativeClassFieldContextV1){"handle",6,xl6BufferHandlePolicyV1Get()};}
#endif
#endif
#if TEST_ROOT==1 || TEST_ROOT==6 || TEST_ROOT==7
static void packet(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    void* packet=xl6NetDatagramPacketCreateTextV1((xstrview){"a\0b",3},99);assert(packet);
    xvalue* shared=xl6NetDatagramPacketBufferHandleV1(packet);assert(shared);
    xbuffer* buffer=xl6NetDatagramPacketBufferV1(packet);assert(buffer&&buffer->Size==3&&buffer->Data[1]==0);
    xnetaddr target;assert(xrtNetAddrLoopback(&target,6,123));xl6NetDatagramPacketTargetSetV1(packet,&target);
    assert(xl6NetDatagramPacketHasTargetV1(packet)&&!xl6NetDatagramPacketValidV1(packet));
    xnetaddr* copy=xl6NetDatagramPacketTargetV1(packet);assert(copy&&copy->Port==123);xl6NetAddressDestroyV1(copy);
    xl6_error_scope failure={0};assert(xl6ErrorScopeBegin(&failure));
    assert(xl6NetDatagramPacketStatusV1(packet)==XNET_RESULT_ERROR);checked(&failure,XERR_STATE);
    xl6NetDatagramPacketClearV1(packet);assert(!buffer->Size&&xl6NetDatagramPacketHasTargetV1(packet));
    xl6NetDatagramPacketTargetClearV1(packet);assert(!xl6NetDatagramPacketHasTargetV1(packet));
    xl6NetDatagramPacketDestroyV1(packet);
    void* surviving=NULL;assert(xrtValueGetHandle(shared,&surviving,NULL,NULL)&&surviving==buffer);
    assert(xrtBufferAppend(buffer,(xbytesview){(const uint8_t*)"retained",8}));xrtValueRelease(shared);
    void* raw=xl6BufferCreateV1();assert(raw);xbuffer* original=raw;
    xvalue* object=objectTake(raw,xl6BufferHandlePolicyV1Get(),BUFFER_TAG);
    packet=xl6NetDatagramPacketCreateBufferV1(bufferContext(),object);assert(packet);xrtValueRelease(object);
    assert(xl6NetDatagramPacketBufferV1(packet)==original);
    assert(xrtBufferAppend(original,(xbytesview){(const uint8_t*)"share",5}));
    shared=xl6NetDatagramPacketBufferHandleV1(packet);assert(shared);xl6NetDatagramPacketDestroyV1(packet);xrtValueRelease(shared);
    bool completed=false;
    for(size_t fail=0;!completed&&fail<16;++fail){
        assert(xl6ErrorScopeBegin(&failure)&&xrtMemDebugFailAfter(fail));
        packet=xl6NetDatagramPacketCreateTextV1((xstrview){"a\0b",3},-1);
        bool injected=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(injected){assert(!packet);checked(&failure,XERR_MEMORY);}
        else {assert(packet&&xl6ErrorScopeEnd(&failure).ok);xl6NetDatagramPacketDestroyV1(packet);completed=true;}
    }
    assert(completed);balanced(&before);
}
typedef struct Edge {xrtownershipref child;size_t count;} Edge;
static bool edge(xrtownershipref child,void* raw)
{Edge* found=raw;found->child=child;++found->count;return true;}
static void packetGraph(XL6_OwnershipCatalog* catalog,bool restore,bool reverse)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    void* raw=xl6NetDatagramPacketCreateV1(32);assert(raw);
    xvalue* handle=xl6NativeHandleTakeV1(&raw,xl6NetDatagramPacketHandlePolicyV1Get());assert(handle&&!raw);
    xrtownershipref refs[2]={xrtValueOwnership(handle),{0}};Edge found={0};
    xrtownershipscope scope={0};assert(xrtOwnershipFreezeTryBegin(&scope));
    assert(refs[0].Ops->Trace(refs[0].Data,edge,&found)&&found.count==1);refs[1]=found.child;
    const xrtownershipadapterv1* adapters[2];int token=0;
    for(size_t i=0;i<2;++i){const xrtownershippreparationv1* prep=NULL;const XL6_OwnershipFinishV1* finish=NULL;
        assert(xl6OwnershipCatalogProbeV1(refs[i],catalog,&adapters[i],&prep,&finish)==XL6_OWNERSHIP_ADMISSION_ADMITTED);
        assert(!prep&&!finish&&adapters[i]->Hold(refs[i].Data));}
    for(size_t i=0;i<2;++i)assert(adapters[i]->Claim(refs[i].Data,&token));
    for(size_t i=0;i<2;++i){size_t index=reverse?1-i:i;
        if(restore)adapters[index]->Restore(refs[index].Data,&token);else adapters[index]->Clear(refs[index].Data,&token);}
    assert(xrtOwnershipScopeEnd(&scope));
    if(!restore)for(size_t i=0;i<2;++i){size_t index=reverse?1-i:i;
        assert(!adapters[index]->Finish||adapters[index]->Finish(refs[index].Data,&token));
        assert(!adapters[index]->Finish||adapters[index]->Finish(refs[index].Data,&token));}
    xrtValueRelease(handle);for(size_t i=0;i<2;++i)adapters[i]->Drop(refs[i].Data);balanced(&before);
}
#endif
#if TEST_ROOT>=2
#if defined(_WIN32)
#include <windows.h>
static uint64_t milliseconds(void)
{LARGE_INTEGER now,frequency;assert(QueryPerformanceCounter(&now)&&QueryPerformanceFrequency(&frequency));
 uint64_t ticks=(uint64_t)now.QuadPart,rate=(uint64_t)frequency.QuadPart;assert(rate);
 return ticks/rate*1000+(ticks%rate)*1000/rate;}
static void pauseHost(void){Sleep(1);}
#else
#include <time.h>
static uint64_t milliseconds(void){struct timespec now;assert(!clock_gettime(CLOCK_MONOTONIC,&now));return (uint64_t)now.tv_sec*1000+(uint64_t)now.tv_nsec/1000000;}
static void pauseHost(void){const struct timespec delay={0,1000000};(void)nanosleep(&delay,NULL);}
#endif
typedef struct Pair {void* input;void* output;xnetaddr* target;} Pair;
static Pair pair(int family)
{
    Pair p={xl6NetSocketCreateV1(2,family,true),xl6NetSocketCreateV1(2,family,true),NULL};assert(p.input&&p.output);
    xnetaddr loop;assert(xrtNetAddrLoopback(&loop,(xnetfamily)family,0));
    xl6NetSocketBindV1(p.input,&loop);xl6NetSocketBindV1(p.output,&loop);p.target=xl6NetSocketLocalV1(p.input);assert(p.target&&p.target->Port);
    assert(xl6NetSocketConnectV1(p.output,p.target)==XNET_RESULT_OK);return p;
}
static void pairEnd(Pair p)
{xl6NetAddressDestroyV1(p.target);xl6NetSocketDestroyV1(p.output);xl6NetSocketDestroyV1(p.input);}
#if TEST_ROOT==2 || TEST_ROOT==3 || TEST_ROOT==4 || TEST_ROOT==7
static void drain(Pair p,const void* data,size_t size)
{
    xbuffer output={0};XL6_NetDatagramInfoV1 info={0};uint64_t start=milliseconds();XL6_ProductI64U64V1 got;
    do {got=xl6NetSocketReceiveFromIntoV1(p.input,&output,&info,64);assert(milliseconds()-start<5000);
        if(got.e0==XNET_RESULT_AGAIN)pauseHost();}while(got.e0==XNET_RESULT_AGAIN);
    assert(got.e0==XNET_RESULT_OK&&got.e1==size&&output.Size==size&&info.valid);
    assert(!size||!memcmp(output.Data,data,size));xrtBufferUnit(&output);
}
#endif
static const uint8_t payload[]={'A',0,'B',0xe4,0xbd,0xa0};
#endif
#if TEST_ROOT==2 || TEST_ROOT==7
static xvalue* textArray(size_t count)
{
    xvalue* value=xl6SequencePrimitiveCreateV1(1,301,302,2,sizeof(xstrview),xl6StringTypeV1(),XL6_VALUE_ADMIT_STRING,false,302);assert(value);
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(value,1,301,302,2,sizeof(xstrview),xl6StringTypeV1());assert(pin&&xl6SequenceBindProjectionV1(pin,NULL));
    for(size_t i=0;i<count;++i){xstrview text={(const char*)payload+(i%2)*3,3};assert(xl6SequenceAdmitSlotV1(pin,&text)&&xrtTypedArrayPush(xl6SequenceStorageV1(pin),&text));}
    xl6SequenceEndV1(pin);return value;
}
#endif
#if TEST_ROOT==3 || TEST_ROOT==7
static xvalue* bytesArray(size_t count)
{
    xvalue* value=xl6SequenceReferenceCreateV1(1,401,402,XL6_VALUE_ADMIT_BYTES,false,402);assert(value);
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(value,1,401,402,3,sizeof(xvalue*),xrtTypeValue());assert(pin&&xl6SequenceBindProjectionV1(pin,NULL));
    for(size_t i=0;i<count;++i){xvalue* bytes=xrtValueBytes((xbytesview){payload+(i%2)*3,3});assert(bytes);
        assert(xl6SequenceAdmitSlotV1(pin,&bytes)&&xrtTypedArrayPush(xl6SequenceStorageV1(pin),&bytes));xrtValueRelease(bytes);}
    xl6SequenceEndV1(pin);return value;
}
#endif
#if TEST_ROOT>=4
static XL6_NativeClassArrayContextV1 arrayContext(bool packets)
{
    #if TEST_ROOT==6 || TEST_ROOT==7
    if(packets)return (XL6_NativeClassArrayContextV1){{ARRAY_ID,PACKET_ID,3,sizeof(xvalue*),xrtTypeValue()},
        "handle",6,xl6NetDatagramPacketHandlePolicyV1Get()};
    #else
    assert(!packets);
    #endif
    return (XL6_NativeClassArrayContextV1){{ARRAY_ID,BUFFER_ID,3,sizeof(xvalue*),xrtTypeValue()},
        "handle",6,xl6BufferHandlePolicyV1Get()};
}
static xvalue* classArray(bool packets,xvalue* const* objects,size_t count)
{
    uint64_t tag=packets?PACKET_TAG:BUFFER_TAG;XL6_NominalAdmissionV1 schema={XL6_NOMINAL_ADMIT_CLASS_TAG,0,&tag,1};
    uint64_t id=packets?PACKET_ID:BUFFER_ID;
    xvalue* value=xl6SequenceNominalCreateV1(1,ARRAY_ID,id,&schema,false,id);assert(value);
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(value,1,ARRAY_ID,id,3,sizeof(xvalue*),xrtTypeValue());assert(pin&&xl6SequenceBindProjectionV1(pin,NULL));
    for(size_t i=0;i<count;++i)assert(xl6SequenceAdmitSlotV1(pin,&objects[i])&&xrtTypedArrayPush(xl6SequenceStorageV1(pin),&objects[i]));
    xl6SequenceEndV1(pin);return value;
}
#if TEST_ROOT==4 || TEST_ROOT==5 || TEST_ROOT==7
static xvalue* bufferArray(size_t count,xbuffer** outputs,bool duplicate)
{
    xvalue* objects[65]={0};assert(count<=65);
    for(size_t i=0;i<count;++i){
        if(duplicate&&i){objects[i]=xrtValueRetain(objects[0]);if(outputs)outputs[i]=outputs[0];continue;}
        void* raw=xl6BufferCreateV1();assert(raw);if(outputs)outputs[i]=raw;
        if(!outputs)assert(xrtBufferAppend(raw,(xbytesview){payload+(i%2)*3,3}));
        objects[i]=objectTake(raw,xl6BufferHandlePolicyV1Get(),BUFFER_TAG);
    }
    xvalue* result=classArray(false,objects,count);for(size_t i=0;i<count;++i)xrtValueRelease(objects[i]);return result;
}
#endif
#endif
#if TEST_ROOT==2 || TEST_ROOT==3 || TEST_ROOT==4 || TEST_ROOT==7
static XL6_ProductI64U64V1 sendVector(unsigned family,unsigned mode,Pair p,xvalue* input,void* control,int64_t size)
{
    (void)family;(void)mode;(void)p;(void)input;(void)control;(void)size;
    #define SEND(K) do {if(!mode)return xl6NetSocketSendVector##K##V1(p.output,input,size); \
        if(mode==1)return xl6NetSocketSendVectorTo##K##V1(p.output,input,p.target,size); \
        if(mode==2)return xl6NetSocketSendMessageVector##K##V1(p.output,input,p.target,control,size); \
        return xl6NetSocketSendConnectedMessageVector##K##V1(p.output,input,control,size);}while(0)
    #if TEST_ROOT==2 || TEST_ROOT==7
    if(family==2)SEND(Text);
    #endif
    #if TEST_ROOT==3 || TEST_ROOT==7
    if(family==3)SEND(Bytes);
    #endif
    #undef SEND
    #if TEST_ROOT==4 || TEST_ROOT==7
    XL6_NativeClassArrayContextV1 c=arrayContext(false);
    if(!mode)return xl6NetSocketSendVectorBufferV1(c,p.output,input,size);
    if(mode==1)return xl6NetSocketSendVectorToBufferV1(c,p.output,input,p.target,size);
    if(mode==2)return xl6NetSocketSendMessageVectorBufferV1(c,p.output,input,p.target,control,size);
    return xl6NetSocketSendConnectedMessageVectorBufferV1(c,p.output,input,control,size);
    #else
    assert(0);return (XL6_ProductI64U64V1){XNET_RESULT_ERROR,0};
    #endif
}
static xvalue* inputArray(unsigned family,size_t count)
{
    (void)family;(void)count;
    #if TEST_ROOT==2 || TEST_ROOT==7
    if(family==2)return textArray(count);
    #endif
    #if TEST_ROOT==3 || TEST_ROOT==7
    if(family==3)return bytesArray(count);
    #endif
    #if TEST_ROOT==4 || TEST_ROOT==7
    return bufferArray(count,NULL,false);
    #else
    assert(0);return NULL;
    #endif
}
static void vectors(unsigned family,int ip)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Pair p=pair(ip);
    void* control=xl6NetDatagramControlCreateV1();assert(control);xvalue* input=inputArray(family,2);
    const int64_t limits[]={-1,4,99,0};const size_t sizes[]={6,4,6,0};
    for(unsigned mode=0;mode<4;++mode)for(unsigned i=0;i<4;++i){
        xmemdebugsnapshot prior,after;xrtMemDebugSnapshot(&prior);
        XL6_ProductI64U64V1 sent=sendVector(family,mode,p,input,control,limits[i]);
        xrtMemDebugSnapshot(&after);assert(sent.e0==XNET_RESULT_OK&&sent.e1==sizes[i]);
        assert(prior.AllocCount==after.AllocCount);drain(p,payload,sizes[i]);
    }
    xl6_error_scope failure={0};assert(xl6ErrorScopeBegin(&failure));
    assert(sendVector(family,0,p,input,control,-2).e0==XNET_RESULT_ERROR);checked(&failure,XERR_RANGE);
    Pair absent=p;absent.output=NULL;assert(xl6ErrorScopeBegin(&failure));
    assert(sendVector(family,0,absent,input,control,-1).e0==XNET_RESULT_ERROR);checked(&failure,XERR_ARGUMENT);
    absent=p;absent.target=NULL;
    for(unsigned mode=1;mode<=2;++mode){assert(xl6ErrorScopeBegin(&failure));
        assert(sendVector(family,mode,absent,input,control,-1).e0==XNET_RESULT_ERROR);checked(&failure,XERR_ARGUMENT);}
    assert(xl6ErrorScopeBegin(&failure));
    assert(sendVector(family,2,p,input,NULL,-1).e0==XNET_RESULT_ERROR);checked(&failure,XERR_ARGUMENT);
    xvalue* tooMany=inputArray(family,65);assert(xl6ErrorScopeBegin(&failure));
    assert(sendVector(family,0,p,tooMany,control,-1).e0==XNET_RESULT_ERROR);checked(&failure,XERR_RANGE);xrtValueRelease(tooMany);
    xrtValueRelease(input);xl6NetDatagramControlDestroyV1(control);pairEnd(p);balanced(&before);
}
#endif
#if TEST_ROOT==5 || TEST_ROOT==7
static void scatter(int ip)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Pair p=pair(ip);xbuffer* outputs[2];
    xvalue* array=bufferArray(2,outputs,false);XL6_NativeClassArrayContextV1 c=arrayContext(false);XL6_NetDatagramInfoV1 info={0};
    xl6_error_scope preflight={0};assert(xl6ErrorScopeBegin(&preflight));
    assert(xl6NetSocketReceiveFromVectorIntoV1(c,p.input,array,&info,-1).e0==XNET_RESULT_ERROR);checked(&preflight,XERR_RANGE);
    assert(xl6ErrorScopeBegin(&preflight));
    assert(xl6NetSocketReceiveFromVectorIntoV1(c,p.input,array,NULL,3).e0==XNET_RESULT_ERROR);checked(&preflight,XERR_ARGUMENT);
    assert(xl6ErrorScopeBegin(&preflight));
    assert(xl6NetSocketReceiveFromVectorIntoV1(c,NULL,array,&info,3).e0==XNET_RESULT_ERROR);checked(&preflight,XERR_ARGUMENT);
    assert(xl6NetSocketSendTextV1(p.output,(xstrview){(const char*)payload,6},-1).e0==XNET_RESULT_OK);
    for(size_t fail=0;fail<2;++fail){
        /* Fail the first and later reserve independently; the first attempt
         * may grow capacity, but must not change logical output or consume. */
        xrtBufferUnit(outputs[0]);xrtBufferUnit(outputs[1]);
        xl6_error_scope failure={0};assert(xl6ErrorScopeBegin(&failure)&&xrtMemDebugFailAfter(fail));
        XL6_ProductI64U64V1 result=xl6NetSocketReceiveFromVectorIntoV1(c,p.input,array,&info,3);
        assert(result.e0==XNET_RESULT_ERROR&&xrtMemDebugFailTriggered());xrtMemDebugFailClear();checked(&failure,XERR_MEMORY);
        assert(!outputs[0]->Size&&!outputs[1]->Size&&!info.valid);
    }
    uint64_t start=milliseconds();XL6_ProductI64U64V1 got;
    do{got=xl6NetSocketReceiveFromVectorIntoV1(c,p.input,array,&info,3);assert(milliseconds()-start<5000);
        if(got.e0==XNET_RESULT_AGAIN)pauseHost();}while(got.e0==XNET_RESULT_AGAIN);
    assert(got.e0==XNET_RESULT_OK&&got.e1==6&&outputs[0]->Size==3&&outputs[1]->Size==3&&info.valid);
    assert(!memcmp(outputs[0]->Data,payload,3)&&!memcmp(outputs[1]->Data,payload+3,3));
    got=xl6NetSocketReceiveFromVectorIntoV1(c,p.input,array,&info,3);assert(got.e0==XNET_RESULT_AGAIN&&info.valid&&outputs[0]->Size==3);
    xvalue* aliased=bufferArray(2,outputs,true);xl6_error_scope failure={0};assert(xl6ErrorScopeBegin(&failure));
    assert(xl6NetSocketReceiveFromVectorIntoV1(c,p.input,aliased,&info,3).e0==XNET_RESULT_ERROR);checked(&failure,XERR_VALUE);
    xrtValueRelease(aliased);
    assert(xl6NetSocketSendTextV1(p.output,(xstrview){(const char*)payload,6},-1).e0==XNET_RESULT_OK);
    got=xl6NetSocketReceiveFromVectorIntoV1(c,p.input,array,&info,1);
    assert(got.e0==XNET_RESULT_TRUNCATED&&got.e1==2);
    xrtValueRelease(array);pairEnd(p);balanced(&before);
}
#endif
#if TEST_ROOT==6 || TEST_ROOT==7
static void batch(int ip)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Pair p=pair(ip);xvalue* sendObjects[3],*recvObjects[3];
    XL6_NetDatagramPacketV1* incoming[3];
    for(size_t i=0;i<3;++i){void* packet=xl6NetDatagramPacketCreateTextV1((xstrview){(const char*)payload,i?3:0},-1);assert(packet);
        xl6NetDatagramPacketTargetSetV1(packet,p.target);sendObjects[i]=objectTake(packet,xl6NetDatagramPacketHandlePolicyV1Get(),PACKET_TAG);
        packet=xl6NetDatagramPacketCreateTextV1((xstrview){"old",3},-1);assert(packet);incoming[i]=packet;
        recvObjects[i]=objectTake(packet,xl6NetDatagramPacketHandlePolicyV1Get(),PACKET_TAG);}
    xvalue* sends=classArray(true,sendObjects,3),*reads=classArray(true,recvObjects,3);
    for(size_t i=0;i<3;++i){xrtValueRelease(sendObjects[i]);xrtValueRelease(recvObjects[i]);}
    XL6_NativeClassArrayContextV1 c=arrayContext(true);void* progress=xl6NetBatchProgressCreateV1();assert(progress);
    XL6_ProductI64U64V1 sent=xl6NetSocketSendBatchV1(c,p.output,sends,progress,1,99);
    assert(sent.e0==XNET_RESULT_OK&&sent.e1==2&&xl6NetBatchProgressCountV1(progress)==2&&xl6NetBatchProgressStartV1(progress)==1);
    XL6_ProductI64U64V1 got;uint64_t start=milliseconds();
    do{got=xl6NetSocketReceiveBatchV1(c,p.input,reads,progress,2,1,99);assert(milliseconds()-start<5000);
        if(got.e0==XNET_RESULT_AGAIN)pauseHost();}while(got.e0==XNET_RESULT_AGAIN);
    assert(got.e1==2&&xl6NetBatchProgressAttemptedV1(progress)&&xl6NetBatchProgressCountV1(progress)==2);
    for(size_t i=1;i<3;++i){xbuffer* b=xl6NetDatagramPacketBufferV1(incoming[i]);assert(b&&b->Size==2&&!memcmp(b->Data,payload,2));
        assert(incoming[i]->hasResult&&incoming[i]->status==XNET_RESULT_TRUNCATED&&incoming[i]->info.valid);}
    assert(!incoming[0]->hasResult&&xl6NetDatagramPacketBufferV1(incoming[0])->Size==3);
    got=xl6NetSocketReceiveBatchV1(c,p.input,reads,progress,4,0,3);assert(got.e0==XNET_RESULT_AGAIN&&!got.e1);
    assert(incoming[1]->hasResult&&xl6NetDatagramPacketBufferV1(incoming[1])->Size==2);
    xl6_error_scope failure={0};assert(xl6ErrorScopeBegin(&failure));
    assert(xl6NetSocketReceiveBatchV1(c,p.input,reads,progress,1,-1,-1).e0==XNET_RESULT_ERROR);checked(&failure,XERR_RANGE);
    assert(!xl6NetBatchProgressAttemptedV1(progress)&&!xl6NetBatchProgressCountV1(progress));
    for(unsigned invalid=0;invalid<4;++invalid){assert(xl6ErrorScopeBegin(&failure));
        got=xl6NetSocketReceiveBatchV1(c,invalid==0?NULL:p.input,reads,progress,
            invalid==1?-1:64,invalid==2?4:0,invalid==3?-2:-1);
        assert(got.e0==XNET_RESULT_ERROR);checked(&failure,invalid?XERR_RANGE:XERR_ARGUMENT);
        assert(!xl6NetBatchProgressAttemptedV1(progress));}
    assert(xl6ErrorScopeBegin(&failure));
    assert(xl6NetSocketSendBatchV1(c,p.output,sends,NULL,0,-1).e0==XNET_RESULT_ERROR);checked(&failure,XERR_ARGUMENT);
    /* Two different packet wrappers sharing ONE actual Buffer must be refused
     * before I/O, even though their outer class identities are different. */
    void* rawBuffer=xl6BufferCreateV1();assert(rawBuffer);
    xvalue* bufferObject=objectTake(rawBuffer,xl6BufferHandlePolicyV1Get(),BUFFER_TAG);
    xvalue* aliases[2];
    for(size_t i=0;i<2;++i){void* packet=xl6NetDatagramPacketCreateBufferV1(bufferContext(),bufferObject);assert(packet);
        aliases[i]=objectTake(packet,xl6NetDatagramPacketHandlePolicyV1Get(),PACKET_TAG);}
    xrtValueRelease(bufferObject);xvalue* aliasArray=classArray(true,aliases,2);
    xrtValueRelease(aliases[0]);xrtValueRelease(aliases[1]);
    sent=xl6NetSocketSendBatchV1(c,p.output,sends,progress,1,2);assert(sent.e0==XNET_RESULT_OK&&sent.e1==2);
    assert(xl6ErrorScopeBegin(&failure));
    assert(xl6NetSocketReceiveBatchV1(c,p.input,aliasArray,progress,64,0,-1).e0==XNET_RESULT_ERROR);checked(&failure,XERR_VALUE);
    assert(!xl6NetBatchProgressAttemptedV1(progress)&&!((xbuffer*)rawBuffer)->Capacity);xrtValueRelease(aliasArray);
    /* Both reserve-failure positions preserve the queued datagrams and every
     * logical result; earlier successful capacity growth is permitted. */
    for(size_t fail=0;fail<2;++fail){
        xrtBufferUnit(xl6NetDatagramPacketBufferV1(incoming[1]));xrtBufferUnit(xl6NetDatagramPacketBufferV1(incoming[2]));
        XL6_NetDatagramInfoV1 oldInfo[2]={incoming[1]->info,incoming[2]->info};
        assert(xl6ErrorScopeBegin(&failure)&&xrtMemDebugFailAfter(fail));
        got=xl6NetSocketReceiveBatchV1(c,p.input,reads,progress,64,1,2);
        assert(got.e0==XNET_RESULT_ERROR&&xrtMemDebugFailTriggered());xrtMemDebugFailClear();checked(&failure,XERR_MEMORY);
        assert(!xl6NetBatchProgressAttemptedV1(progress)&&!xl6NetBatchProgressCountV1(progress));
        for(size_t i=1;i<3;++i)assert(!xl6NetDatagramPacketBufferV1(incoming[i])->Size&&
            !memcmp(&oldInfo[i-1],&incoming[i]->info,sizeof(oldInfo[0]))&&incoming[i]->status==XNET_RESULT_TRUNCATED);
    }
    got=xl6NetSocketReceiveBatchV1(c,p.input,reads,progress,64,3,99);
    assert(got.e0==XNET_RESULT_OK&&!got.e1&&xl6NetBatchProgressAttemptedV1(progress)&&xl6NetBatchProgressStartV1(progress)==3);
    start=milliseconds();
    do{got=xl6NetSocketReceiveBatchV1(c,p.input,reads,progress,64,1,2);assert(milliseconds()-start<5000);
        if(got.e0==XNET_RESULT_AGAIN)pauseHost();}while(got.e0==XNET_RESULT_AGAIN);
    assert(got.e0==XNET_RESULT_OK&&got.e1==2);
    for(size_t i=1;i<3;++i)assert(xl6NetDatagramPacketBufferV1(incoming[i])->Size==3&&incoming[i]->status==XNET_RESULT_OK);
    got=xl6NetSocketReceiveBatchV1(c,p.input,reads,progress,64,0,-1);assert(got.e0==XNET_RESULT_AGAIN&&!got.e1);
    xvalue* repeated[65];
    /* A fresh empty packet is safe to repeat for a send-only bound check. */
    void* packet=xl6NetDatagramPacketCreateV1(0);assert(packet);
    xvalue* object=objectTake(packet,xl6NetDatagramPacketHandlePolicyV1Get(),PACKET_TAG);
    for(size_t i=0;i<65;++i)repeated[i]=object;
    xvalue* tooMany=classArray(true,repeated,65);xrtValueRelease(object);
    assert(xl6ErrorScopeBegin(&failure));
    assert(xl6NetSocketSendBatchV1(c,p.output,tooMany,progress,0,-1).e0==XNET_RESULT_ERROR);checked(&failure,XERR_RANGE);
    assert(!xl6NetBatchProgressAttemptedV1(progress));xrtValueRelease(tooMany);
    xl6NetBatchProgressDestroyV1(progress);xrtValueRelease(reads);xrtValueRelease(sends);pairEnd(p);balanced(&before);
}
#endif
int main(void)
{
    assert(xrtMemDebugEnable(true));xl6_error_scope warm={0};assert(xl6ErrorScopeBegin(&warm)&&xl6ErrorScopeEnd(&warm).ok);
    const XL6_OwnershipDomainV1* domains[]={
        #include "net-vector-domains.h"
    };
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(*domains));assert(catalog);
    #if TEST_ROOT==0 || TEST_ROOT==7
    progress();
    #endif
    #if TEST_ROOT==1 || TEST_ROOT==6 || TEST_ROOT==7
    packet();for(unsigned restore=0;restore<2;++restore)for(unsigned reverse=0;reverse<2;++reverse)packetGraph(catalog,restore!=0,reverse!=0);
    #endif
    for(int ip=4;ip<=6;ip+=2){
        #if TEST_ROOT==2 || TEST_ROOT==7
        vectors(2,ip);
        #endif
        #if TEST_ROOT==3 || TEST_ROOT==7
        vectors(3,ip);
        #endif
        #if TEST_ROOT==4 || TEST_ROOT==7
        vectors(4,ip);
        #endif
        #if TEST_ROOT==5 || TEST_ROOT==7
        scatter(ip);
        #endif
        #if TEST_ROOT==6 || TEST_ROOT==7
        batch(ip);
        #endif
    }
    xl6OwnershipCatalogDestroyV1(catalog);
    puts("Vector/batch runtime: exact values/owning graph, own loopback, checked failures and memory balance");return 0;
}
