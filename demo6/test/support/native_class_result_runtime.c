/* Generic unpublished class construction, offline native values only. */
#include "include/xruntime_decl.h"
#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(C) do{if(!(C)){fprintf(stderr,"line %u: %s\n",(unsigned)__LINE__,#C);abort();}}while(0)
extern xnetudppacket* udpFixturePacket(const xnetaddr*);
extern xnetudperrorpacket* udpFixtureError(void);
extern xnetudpbatch* udpFixtureBatch(void);
static const XL6_ClassIdentitySchemaV3 schemas[]={
    {sizeof(XL6_ClassIdentitySchemaV3),101,101,XL6_CLASS_IDENTITY_REFERENCE_V3,XL6_CLASS_IDENTITY_REFERENCE_V3,NULL,0,{0}},
    {sizeof(XL6_ClassIdentitySchemaV3),102,102,XL6_CLASS_IDENTITY_REFERENCE_V3,XL6_CLASS_IDENTITY_REFERENCE_V3,NULL,0,{0}},
    {sizeof(XL6_ClassIdentitySchemaV3),103,103,XL6_CLASS_IDENTITY_REFERENCE_V3,XL6_CLASS_IDENTITY_REFERENCE_V3,NULL,0,{0}}};
typedef struct Owner {unsigned refs,acquires,releases;bool closed;} Owner;
static int acquire(void* raw){Owner* o=raw;if(o->closed)return 0;++o->refs;++o->acquires;return 1;}
static void release(void* raw){Owner* o=raw;CHECK(o->refs);--o->refs;++o->releases;}
static const XL6_NativeHandlePolicyV1* policy(unsigned kind)
{return kind==0?xl6UdpPacketHandlePolicyV1Get():kind==1?xl6UdpErrorPacketHandlePolicyV1Get():xl6UdpBatchHandlePolicyV1Get();}
static void* native(unsigned kind)
{xnetaddr a;CHECK(xrtNetAddrLoopback(&a,XNET_FAMILY_IPV4,0));return kind==0?(void*)udpFixturePacket(&a):kind==1?(void*)udpFixtureError():(void*)udpFixtureBatch();}
static void balanced(const xmemdebugsnapshot* before)
{xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);CHECK(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes);
 CHECK(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&after.InvalidFreeCount==before->InvalidFreeCount&&after.DoubleFreeCount==before->DoubleFreeCount);}
int main(void)
{
    CHECK(xrtMemDebugEnable(true));xl6_error_scope warm={0};CHECK(xl6ErrorScopeBegin(&warm));xl6_error_result outcome=xl6ErrorScopeEnd(&warm);CHECK(outcome.ok);
    unsigned prefixes=0,failures=0;
    for(unsigned kind=0;kind<3;++kind){bool complete=false;
        for(unsigned point=0;point<256;++point){
            xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Owner counter={0};XL6_CodeOwner owner={sizeof(owner),&counter,acquire,release};
            XL6_NativeClassResultContextV1 context={{"handle",6,policy(kind)},&owner,&schemas[kind]};void* raw=native(kind);CHECK(raw);
            xl6_error_scope scope={0};CHECK(xl6ErrorScopeBegin(&scope)&&xrtMemDebugFailAfter(point));
            xvalue* prepared=xl6NativeClassResultPrepareV1(context);xvalue* object=NULL;
            if(prepared){CHECK(!xrtValueObjectGet(prepared,(xstrview){"handle",6}));object=xl6NativeClassResultCommitTakeV1(context,&prepared,&raw);CHECK(!prepared&&!raw);}
            bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();outcome=xl6ErrorScopeEnd(&scope);++prefixes;
            CHECK((object!=NULL)==outcome.ok&&(object!=NULL)==!hit);
            if(object){void* handle=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
                CHECK(xrtValueTypeId(object)==schemas[kind].identity&&counter.refs==1);
                CHECK(xrtValueGetHandle(xrtValueObjectGet(object,(xstrview){"handle",6}),&handle,&ops,&user)&&handle&&!user&&ops==policy(kind)->Ops);
                if(kind==0)CHECK(xl6UdpPacketLengthV1(handle)==5);
                if(kind==1)CHECK(xl6UdpErrorPacketLengthV1(handle)==3);
                if(kind==2)CHECK(xl6UdpBatchCountV1(handle)==1);
            }else{CHECK(hit&&xrtErrorKind(outcome.error)==XERR_MEMORY);++failures;}
            xrtValueRelease(object);if(raw)policy(kind)->Ops->Drop(raw,NULL);xrtErrorFree(outcome.error);
            CHECK(!counter.refs&&counter.acquires==counter.releases);balanced(&before);
            if(!hit){complete=true;break;}
        }CHECK(complete);
    }
    for(unsigned bad=0;bad<5;++bad){xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_ClassIdentitySchemaV3 schema=schemas[0];
        XL6_NativeClassResultContextV1 context={{"handle",6,policy(0)},NULL,&schema};
        if(bad==0)schema.identity=0;
        if(bad==1)schema.runtimeTypeTag=0;
        if(bad==2)schema.hashMode=XL6_CLASS_IDENTITY_DERIVED_V3;
        if(bad==3){context.field.fieldName="$xlir.class.runtime-type";context.field.fieldSize=24;}
        if(bad==4)context.field.policy=NULL;
        CHECK(!xl6NativeClassResultPrepareV1(context)&&xrtErrorKind(xrtGetError())==XERR_ARGUMENT);balanced(&before);
    }
    {
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Owner counter={0};XL6_CodeOwner owner={sizeof(owner),&counter,acquire,release};
        XL6_CodeLease* parent=NULL;CHECK(xl6CodeLeaseAcquire(&owner,&parent)==XL6_CODE_LEASE_ACQUIRED&&parent);
        counter.closed=true;XL6_NativeClassResultContextV1 c={{"handle",6,policy(0)},&owner,&schemas[0]};
        xvalue* prepared=xl6NativeClassResultPrepareFromLeaseV1(c,parent);CHECK(prepared);
        void* raw=native(0);CHECK(raw);xvalue* object=xl6NativeClassResultCommitTakeV1(c,&prepared,&raw);CHECK(object&&!prepared&&!raw);
        CHECK(counter.acquires==1&&counter.refs==1);xl6CodeLeaseRelease(parent);CHECK(counter.refs==1);
        xvalue* alias=xrtValueRetain(object);raw=native(0);CHECK(alias&&raw);xl6_error_scope scope={0};CHECK(xl6ErrorScopeBegin(&scope));
        CHECK(!xl6NativeClassResultCommitTakeV1(c,&alias,&raw)&&!alias&&!raw);outcome=xl6ErrorScopeEnd(&scope);
        CHECK(!outcome.ok&&xrtErrorKind(outcome.error)==XERR_STATE);xrtErrorFree(outcome.error);
        xrtValueRelease(object);CHECK(counter.refs==0&&counter.acquires==counter.releases);balanced(&before);
    }
    CHECK(xrtMemDebugReset());printf("native class result: three actual native policies, full prefixes=%u failures=%u; CodeLease balanced after Close, duplicate publication refused, no native placeholders\n",prefixes,failures);return 0;
}
