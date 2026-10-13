#include "src/runtime/xl6_stack_erased.h"
#include "src/runtime/xl6_stack_erased_methods.h"
#include "src/runtime/xl6_stack_erased_try.h"
#include "src/runtime/xl6_stack.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_string_type.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif

#define OWNER UINT64_C(9401)
#define ELEMENT UINT64_C(9402)
#define I64 OWNER,ELEMENT,1,sizeof(int64_t),xrtTypeInt64()
#define STACK_TEXT OWNER,ELEMENT,2,sizeof(xstrview),xl6StringTypeV1()
#define REF OWNER,ELEMENT,3,sizeof(xvalue*),xrtTypeValue()
#define ARRAY_OWNER UINT64_C(9404)
static void error(xerrkind);
static xvalue* primitive(unsigned,const xrttype*,unsigned,unsigned);
static unsigned batchResultFailure;
static unsigned tryMoveFailure;
extern bool __real_xrtCallResultMove(xrtcallresult*,xrtcallresult*);
bool __wrap_xrtCallResultMove(xrtcallresult* target,xrtcallresult* source)
{
    if(tryMoveFailure==1) return false;
    bool ok=__real_xrtCallResultMove(target,source);
    if(ok && tryMoveFailure) {
        if(tryMoveFailure!=3) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
        if(tryMoveFailure>=3) xrtSetErrorKind(XERR_IO);
        if(tryMoveFailure==5) return false;
    }
    return ok;
}
extern xtypedarray* __real_xrtTypedArrayTakeTail(xtypedarray*,size_t,bool);
extern xtypedarray* __real_xrtTypedArraySlice(const xtypedarray*,size_t,size_t,bool);
static xtypedarray* batchPending(xtypedarray* result)
{
    if(result && batchResultFailure) {
        if(batchResultFailure!=2) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
        if(batchResultFailure>=2) xrtSetErrorKind(XERR_IO);
    }
    return result;
}
xtypedarray* __wrap_xrtTypedArrayTakeTail(xtypedarray* array,size_t size,bool reverse)
{ return batchPending(__real_xrtTypedArrayTakeTail(array,size,reverse)); }
xtypedarray* __wrap_xrtTypedArraySlice(const xtypedarray* array,size_t start,size_t size,bool reverse)
{ return batchPending(__real_xrtTypedArraySlice(array,start,size,reverse)); }

static void batchMetadata(void)
{
    xvalue* stack=primitive(XL6_SEQUENCE_STACK_V1,xrtTypeInt64(),1,XL6_VALUE_ADMIT_I8);
    assert(stack);
    xvalue* erased=xl6StackEraseTakeV2(xrtValueRetain(stack),I64,NULL,false,ARRAY_OWNER);
    assert(erased==stack);
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(stack,XL6_SEQUENCE_STACK_V1,I64); assert(pin);
    assert(xl6SequenceStackResultArrayV1(pin)==ARRAY_OWNER);
    assert(xl6SequenceBindStackResultArrayV1(pin,ARRAY_OWNER));
    assert(!xl6SequenceBindStackResultArrayV1(pin,ARRAY_OWNER+1)); error(XERR_ARGUMENT);
    assert(xl6SequenceStackResultArrayV1(pin)==ARRAY_OWNER); xl6SequenceEndV1(pin);
    int64_t raw=11; assert(xl6StackPushV1(stack,I64,&raw));
    raw=23; assert(xl6StackPushV1(stack,I64,&raw));
    xvalue* copy=xl6StackCloneV1(stack,I64); assert(copy);
    pin=xl6SequenceBeginRoleV1(copy,XL6_SEQUENCE_STACK_V1,I64); assert(pin);
    assert(xl6SequenceStackResultArrayV1(pin)==ARRAY_OWNER); xl6SequenceEndV1(pin);
    xvalue* result=xl6StackErasedMethodV1(stack,XL6_STACK_ERASED_EQUALS,copy,NULL);
    bool same=false; assert(result && xrtValueGetBool(result,&same) && same); xrtValueRelease(result);
    xvalue* maximum=xrtValueInt(2); assert(maximum);
    result=xl6StackErasedMethodV1(copy,XL6_STACK_ERASED_POP_BATCH,maximum,NULL); assert(result);
    assert(xrtValueTypeId(result)==ARRAY_OWNER && xl6StackSizeV1(copy,I64,false)==0);
    pin=xl6SequenceBeginRoleV1(result,XL6_SEQUENCE_ARRAY_V1,ARRAY_OWNER,ELEMENT,1,sizeof(int64_t),xrtTypeInt64());
    assert(pin && xrtTypedArrayCount(xl6SequenceStorageV1(pin))==2);
    assert(*(const int64_t*)xrtTypedArrayConstGet(xl6SequenceStorageV1(pin),0)==23);
    assert(!xl6SequenceBindStackResultArrayV1(pin,ARRAY_OWNER)); error(XERR_ARGUMENT);
    xl6SequenceEndV1(pin);
    xrtValueRelease(result); xrtValueRelease(maximum); xrtValueRelease(copy); xrtValueRelease(erased); xrtValueRelease(stack);
}

static void error(xerrkind kind)
{ assert(xrtGetError() && xrtErrorKind(xrtGetError())==kind); xrtClearError(); }
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        after.InvalidFreeCount==before->InvalidFreeCount && after.DoubleFreeCount==before->DoubleFreeCount &&
        after.UseAfterFreeCount==before->UseAfterFreeCount && !xrtGetError());
}
static xvalue* primitive(unsigned role, const xrttype* type, unsigned transfer, unsigned rule)
{ return xl6SequencePrimitiveCreateV1(role,OWNER,ELEMENT,transfer,type->Size,type,rule,false,ELEMENT); }
static void text(const xvalue* value, const char* data, size_t size)
{
    xstrview actual={0}; assert(value && xrtValueGetString(value,&actual));
    assert(actual.Size==size && !memcmp(actual.Data,data,size));
}
static int64_t integer(const xvalue* value)
{ int64_t result=0; assert(value && xrtValueGetInt(value,&result)); return result; }
static void scalar(void)
{
    xvalue* stack=primitive(XL6_SEQUENCE_STACK_V1,xrtTypeInt64(),1,XL6_VALUE_ADMIT_I8); assert(stack);
    int64_t raw=11; assert(xl6StackPushV1(stack,I64,&raw));
    xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
    assert(xrtMemDebugFailAfter(0));
    xvalue* alias=xl6StackEraseV1(stack,I64,NULL); assert(alias==stack);
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    xrtMemDebugSnapshot(&after); assert(before.AllocCount==after.AllocCount);
    xvalue* item=xrtValueInt(127); assert(item && xrtValueTypeIdBind(item,ELEMENT));
    assert(xl6StackErasedPushV1(alias,item)); xrtValueRelease(item);
    item=xrtValueInt(128); assert(item && xrtValueTypeIdBind(item,ELEMENT));
    assert(!xl6StackErasedPushV1(alias,item)); error(XERR_TYPE); xrtValueRelease(item);
    item=xrtValueUInt(4); assert(item); assert(!xl6StackErasedPushV1(alias,item)); error(XERR_TYPE); xrtValueRelease(item);
    assert(xl6StackSizeV1(stack,I64,false)==2);
    item=xl6StackErasedReadV1(alias,1,false); assert(integer(item)==11);
    assert(xrtValueTypeId(item)==ELEMENT); xrtValueRelease(item);
    assert(!xl6StackErasedReadV1(alias,-1,false)); error(XERR_RANGE);
    assert(!xl6StackErasedReadV1(alias,2,false)); error(XERR_RANGE);
    assert(!xl6StackErasedReadV1(alias,1,true)); error(XERR_ARGUMENT);
    xvalue* copy=xl6StackCloneV1(stack,I64); assert(copy);
    item=xl6StackErasedReadV1(copy,0,true); assert(integer(item)==127); xrtValueRelease(item);
    assert(xl6StackSizeV1(copy,I64,false)==1 && xl6StackSizeV1(stack,I64,false)==2);
    xrtValueRelease(copy);
    item=xl6StackErasedReadV1(alias,0,true); assert(integer(item)==127); xrtValueRelease(item);
    item=xl6StackErasedReadV1(alias,0,true); assert(integer(item)==11); xrtValueRelease(item);
    assert(!xl6StackErasedReadV1(alias,0,true)); error(XERR_RANGE);
    xrtValueRelease(alias); xrtValueRelease(stack);
}
static void precision(void)
{
    xvalue* stack=primitive(XL6_SEQUENCE_STACK_V1,xl6StringTypeV1(),2,XL6_VALUE_ADMIT_STRING);
    assert(stack); xvalue* alias=xl6StackEraseV1(stack,STACK_TEXT,xl6StringConverterV1()); assert(alias);
    xvalue* item=xrtValueString((xstrview){"a\0\xE4\xBD\xA0",5}); assert(item);
    assert(xrtValueTypeIdBind(item,ELEMENT) && xl6StackErasedPushV1(alias,item)); xrtValueRelease(item);
    item=xl6StackErasedReadV1(alias,0,true); text(item,"a\0\xE4\xBD\xA0",5);
    assert(xrtValueTypeId(item)==ELEMENT); xrtValueRelease(item); xrtValueRelease(alias); xrtValueRelease(stack);
    stack=xl6SequenceReferenceCreateV1(XL6_SEQUENCE_STACK_V1,OWNER,ELEMENT,XL6_VALUE_ADMIT_ANY,true,0);
    alias=xl6StackEraseV1(stack,REF,NULL); assert(stack && alias);
    item=xrtValueArray(); assert(item && xrtValueTypeIdBind(item,9403));
    assert(xl6StackErasedPushV1(alias,item)); xvalue* result=xl6StackErasedReadV1(alias,0,false);
    assert(result==item && xrtValueTypeId(result)==9403); xrtValueRelease(result);
    result=xl6StackErasedReadV1(alias,0,true); assert(result==item); xrtValueRelease(result);
    assert(xl6StackErasedPushV1(alias,xrtValueNull()));
    result=xl6StackErasedReadV1(alias,0,true); assert(result && xrtValueType(result)==XVALUE_NULL);
    xrtValueRelease(result); xrtValueRelease(item); xrtValueRelease(alias); xrtValueRelease(stack);
}
static void foreignDrop(ptr value, ptr user) { (void)value; (void)user; }
static const xvaluehandleops foreignOps={.Drop=foreignDrop};
static void rejection(void)
{
    xvalue* stack=primitive(XL6_SEQUENCE_STACK_V1,xrtTypeInt64(),1,XL6_VALUE_ADMIT_I8);
    xvalue* array=primitive(XL6_SEQUENCE_ARRAY_V1,xrtTypeInt64(),1,XL6_VALUE_ADMIT_I8); assert(stack && array);
    XL6_Sequence* pin=NULL; XL6_SequenceLayoutV1 layout={0}; const xvalueconverter* converter=NULL;
    assert(xl6SequenceBeginErasedRoleV1(stack,XL6_SEQUENCE_STACK_V1,&pin,&layout,&converter)==-1);
    assert(!pin && !layout.identity && !converter); error(XERR_STATE);
    xvalue* alias=xl6StackEraseV1(stack,I64,NULL); assert(alias);
    xvalue* arrayAlias=xl6ArrayEraseV1(array,I64,NULL); assert(arrayAlias);
    xrtSetErrorKind(XERR_IO); const xerror* ambient=xrtGetError();
    void* raw=(void*)(uintptr_t)1; xvalue* foreign=xrtValueHandleTake(&raw,&foreignOps,NULL); assert(foreign);
    assert(xrtValueTypeIdBind(foreign,OWNER));
    assert(xrtMemDebugFailAfter(0));
    assert(xl6SequenceBeginErasedArrayV1(stack,&pin,&layout,&converter)==0 && !pin && !layout.identity);
    assert(xl6SequenceBeginErasedRoleV1(array,XL6_SEQUENCE_STACK_V1,&pin,&layout,&converter)==0);
    assert(xl6SequenceBeginErasedRoleV1(foreign,XL6_SEQUENCE_STACK_V1,&pin,&layout,&converter)==0);
    assert(xl6SequenceBeginErasedRoleV1(xrtValueNull(),XL6_SEQUENCE_STACK_V1,&pin,&layout,&converter)==0);
    assert(xrtGetError()==ambient && !xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); xrtClearError();
    assert(!xl6StackEraseV1(array,I64,NULL)); error(XERR_ARGUMENT);
    assert(!xl6ArrayEraseV1(stack,I64,NULL)); error(XERR_ARGUMENT);
    assert(!xl6StackErasedReadV1(array,0,false)); error(XERR_TYPE);
    assert(!xl6StackErasedReadV1(foreign,0,false)); error(XERR_TYPE);
    assert(xl6SequenceBeginErasedRoleV1(stack,XL6_SEQUENCE_CURSOR_V1,&pin,&layout,&converter)==-1); error(XERR_ARGUMENT);
    pin=xl6SequenceBeginRoleV1(stack,XL6_SEQUENCE_STACK_V1,I64); assert(pin);
    assert(!xl6SequenceBindProjectionV1(pin,NULL)); error(XERR_ARGUMENT);
    assert(!xl6StackErasedReadV1(stack,0,false)); error(XERR_STATE); xl6SequenceEndV1(pin);
    xvalueconverter invalid=*xl6StringConverterV1(); invalid.Context=stack;
    assert(!xl6StackEraseV1(stack,I64,&invalid)); error(XERR_ARGUMENT);
    invalid.Context=NULL;
    assert(!xl6StackEraseV1(stack,I64,&invalid)); error(XERR_ARGUMENT);
    assert(!xl6StackEraseV1(stack,OWNER,ELEMENT+1,1,sizeof(int64_t),xrtTypeInt64(),NULL)); error(XERR_ARGUMENT);
    assert(xl6SequenceBeginErasedRoleV1(stack,XL6_SEQUENCE_STACK_V1,&pin,&layout,&converter)==1);
    assert(layout.identity==OWNER && layout.elementIdentity==ELEMENT && layout.type==xrtTypeInt64()); xl6SequenceEndV1(pin);
    xrtValueRelease(foreign); xrtValueRelease(arrayAlias); xrtValueRelease(array); xrtValueRelease(alias); xrtValueRelease(stack);
}
static unsigned faultMode;
static xvalue* callbackStack;
static xvalue* callbackAlias;
static xvalue* faultFrom(const xrttype* type,const void* source,ptr context)
{
    (void)context;
    xvalue* result=xl6StringConverterV1()->FromTyped(type,source,NULL);
    if (faultMode==1) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    else if (faultMode==2) xrtSetErrorKind(XERR_IO);
    else if (faultMode==3) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL}); xrtSetErrorKind(XERR_TYPE);
    }
    else if (faultMode==4 || faultMode==5) {
        if (faultMode==5) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
        xrtValueRelease(callbackAlias); callbackAlias=NULL;
        xrtValueRelease(callbackStack); callbackStack=NULL;
    }
    return result;
}
static bool faultTo(const xvalue* source,const xrttype* type,ptr target,ptr context)
{ (void)context; return xl6StringConverterV1()->ToTyped(source,type,target,NULL); }
static const xvalueconverter faults={.ToTyped=faultTo,.FromTyped=faultFrom};
static void callbacks(void)
{
    for (unsigned mode=0;mode<6;++mode) {
        faultMode=mode;
        xvalue* stack=primitive(XL6_SEQUENCE_STACK_V1,xl6StringTypeV1(),2,XL6_VALUE_ADMIT_STRING);
        xstrview input={"a\0b",3}; assert(stack && xl6StackPushV1(stack,STACK_TEXT,&input));
        xvalue* alias=xl6StackEraseV1(stack,STACK_TEXT,&faults); assert(alias);
        callbackStack=stack; callbackAlias=alias;
        xl6_error_scope call={0}; assert(xl6ErrorScopeBegin(&call));
        xvalue* item=xl6StackErasedReadV1(alias,0,true);
        xl6_error_result outcome=xl6ErrorScopeEnd(&call);
        if (!mode || mode==4) { assert(outcome.ok && !outcome.error); text(item,"a\0b",3); }
        else { assert(!item && !outcome.ok); assert(mode==2 ? outcome.error!=NULL : outcome.error==NULL); }
        xrtErrorFree(outcome.error); xrtClearError(); xrtValueRelease(item);
        if (mode>=4) { assert(!callbackAlias && !callbackStack); continue; }
        assert(xl6StackSizeV1(stack,STACK_TEXT,false)==(mode ? 1 : 0));
        faultMode=0;
        if (mode) { item=xl6StackErasedReadV1(alias,0,false); text(item,"a\0b",3); xrtValueRelease(item); }
        xrtValueRelease(alias); xrtValueRelease(stack);
    }
}
static void tryVector(const xrtcallresult* result,bool found,xvaluetype tag)
{
    assert(xrtCallResultCount(result)==2);
    bool actual=false; assert(xrtValueGetBool(xrtCallResultGet(result,0),&actual) && actual==found);
    assert(xrtCallResultGet(result,1) && xrtValueType(xrtCallResultGet(result,1))==tag);
}
static void tryResults(void)
{
    xvalue* stack=primitive(XL6_SEQUENCE_STACK_V1,xrtTypeInt64(),1,XL6_VALUE_ADMIT_I8);
    xvalue* alias=xl6StackEraseV1(stack,I64,NULL); assert(stack && alias);
    xrtcallresult result=XRT_CALL_RESULT_INIT;
    assert(xl6StackErasedTryCallV1(alias,0,true,&result)); tryVector(&result,false,XVALUE_NULL);
    assert(!xl6StackErasedTryCallV1(alias,0,true,&result)); error(XERR_ARGUMENT);
    tryVector(&result,false,XVALUE_NULL); xrtCallResultClear(&result);
    assert(!xl6StackErasedTryCallV1(alias,-1,false,&result)); error(XERR_RANGE);
    assert(!xrtCallResultCount(&result));
    assert(!xl6StackErasedTryCallV1(alias,1,true,&result)); error(XERR_ARGUMENT);
    int64_t raw=0; assert(xl6StackPushV1(stack,I64,&raw)); raw=127; assert(xl6StackPushV1(stack,I64,&raw));
    assert(xl6StackErasedTryCallV1(alias,1,false,&result)); tryVector(&result,true,XVALUE_INT);
    assert(integer(xrtCallResultGet(&result,1))==0 && xrtValueTypeId(xrtCallResultGet(&result,1))==ELEMENT);
    xrtCallResultClear(&result);
    assert(xl6StackErasedTryCallV1(alias,INT64_MAX,false,&result)); tryVector(&result,false,XVALUE_NULL);
    xrtCallResultClear(&result);
    assert(xl6StackErasedTryCallV1(alias,0,true,&result)); tryVector(&result,true,XVALUE_INT);
    assert(integer(xrtCallResultGet(&result,1))==127 && xl6StackSizeV1(stack,I64,false)==1);
    xrtCallResultUnit(&result); xrtValueRelease(alias); xrtValueRelease(stack);

    stack=xl6SequenceReferenceCreateV1(XL6_SEQUENCE_STACK_V1,OWNER,ELEMENT,XL6_VALUE_ADMIT_ANY,true,0);
    alias=xl6StackEraseV1(stack,REF,NULL); assert(stack && alias && xl6StackErasedPushV1(alias,xrtValueNull()));
    assert(xl6StackErasedTryCallV1(alias,0,true,&result)); tryVector(&result,true,XVALUE_NULL);
    xrtCallResultClear(&result);
    assert(xl6StackErasedTryCallV1(alias,0,true,&result)); tryVector(&result,false,XVALUE_NULL);
    xrtCallResultUnit(&result); xrtValueRelease(alias); xrtValueRelease(stack);

    xvalue* array=primitive(XL6_SEQUENCE_ARRAY_V1,xrtTypeInt64(),1,XL6_VALUE_ADMIT_I8);
    assert(array && !xl6StackErasedTryCallV1(array,0,false,&result)); error(XERR_TYPE);
    assert(!xrtCallResultCount(&result)); xrtValueRelease(array);
    stack=primitive(XL6_SEQUENCE_STACK_V1,xl6CharTypeV1(),1,XL6_VALUE_ADMIT_CHAR);
    uint32_t scalar=0x4F60;
    assert(stack && xl6StackPushV1(stack,OWNER,ELEMENT,1,sizeof(scalar),xl6CharTypeV1(),&scalar));
    alias=xl6StackEraseV1(stack,OWNER,ELEMENT,1,sizeof(scalar),xl6CharTypeV1(),xl6CharConverterV1()); assert(alias);
    assert(xl6StackErasedTryCallV1(alias,0,true,&result)); tryVector(&result,true,XVALUE_CHAR);
    uint32_t observed=0; assert(xrtValueGetChar(xrtCallResultGet(&result,1),&observed) && observed==scalar);
    xrtCallResultUnit(&result); xrtValueRelease(alias); xrtValueRelease(stack);

    stack=xl6SequenceReferenceCreateV1(XL6_SEQUENCE_STACK_V1,OWNER,ELEMENT,XL6_VALUE_ADMIT_BYTES,false,ELEMENT);
    alias=xl6StackEraseV1(stack,REF,NULL); assert(stack && alias);
    const unsigned char octets[]={0,255,128}; xvalue* bytes=xrtValueBytes((xbytesview){octets,sizeof(octets)});
    assert(bytes && xrtValueTypeIdBind(bytes,ELEMENT) && xl6StackErasedPushV1(alias,bytes));
    assert(xl6StackErasedTryCallV1(alias,0,true,&result)); tryVector(&result,true,XVALUE_BYTES);
    xbytesview data={0}; assert(xrtValueGetBytes(xrtCallResultGet(&result,1),&data));
    assert(data.Size==sizeof(octets) && !memcmp(data.Data,octets,sizeof(octets)));
    xrtCallResultUnit(&result); xrtValueRelease(bytes); xrtValueRelease(alias); xrtValueRelease(stack);
}
static void tryCallbacks(void)
{
    for(unsigned mode=0;mode<6;++mode) {
        faultMode=mode;
        xvalue* stack=primitive(XL6_SEQUENCE_STACK_V1,xl6StringTypeV1(),2,XL6_VALUE_ADMIT_STRING);
        xstrview input={"a\0b",3}; assert(stack && xl6StackPushV1(stack,STACK_TEXT,&input));
        xvalue* alias=xl6StackEraseV1(stack,STACK_TEXT,&faults); assert(alias);
        callbackStack=stack; callbackAlias=alias;
        xrtcallresult result=XRT_CALL_RESULT_INIT;
        xl6_error_scope call={0}; assert(xl6ErrorScopeBegin(&call));
        bool ok=xl6StackErasedTryCallV1(alias,0,true,&result);
        xl6_error_result outcome=xl6ErrorScopeEnd(&call);
        if(!mode || mode==4) { assert(ok && outcome.ok && !outcome.error); tryVector(&result,true,XVALUE_STRING); text(xrtCallResultGet(&result,1),"a\0b",3); }
        else { assert(!ok && !outcome.ok && !xrtCallResultCount(&result)); assert(mode==2?outcome.error!=NULL:outcome.error==NULL); }
        xrtErrorFree(outcome.error); xrtClearError(); xrtCallResultUnit(&result); faultMode=0;
        if(mode>=4) { assert(!callbackAlias && !callbackStack); continue; }
        assert(xl6StackSizeV1(stack,STACK_TEXT,false)==(mode?1:0));
        xrtValueRelease(alias); xrtValueRelease(stack);
    }
}
typedef struct Worker { xvalue* stack; } Worker;
static void worker(Worker* data)
{
    assert(!xl6StackErasedReadV1(data->stack,0,false)); error(XERR_STATE);
    xrtcallresult result=XRT_CALL_RESULT_INIT;
    assert(!xl6StackErasedTryCallV1(data->stack,0,false,&result)); error(XERR_STATE);
    assert(!xrtCallResultCount(&result)); xrtCallResultUnit(&result);
}
#if defined(_WIN32)
static DWORD WINAPI workerEntry(LPVOID data) { worker(data); return 0; }
#else
static void* workerEntry(void* data) { worker(data); return NULL; }
#endif
static void busy(void)
{
    xvalue* stack=primitive(XL6_SEQUENCE_STACK_V1,xrtTypeInt64(),1,XL6_VALUE_ADMIT_I8);
    xvalue* alias=xl6StackEraseV1(stack,I64,NULL); assert(stack && alias);
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(stack,XL6_SEQUENCE_STACK_V1,I64); assert(pin);
    Worker data={stack};
#if defined(_WIN32)
    HANDLE thread=CreateThread(NULL,0,workerEntry,&data,0,NULL); assert(thread);
    assert(WaitForSingleObject(thread,INFINITE)==WAIT_OBJECT_0); assert(CloseHandle(thread));
#else
    pthread_t thread; assert(!pthread_create(&thread,NULL,workerEntry,&data)); assert(!pthread_join(thread,NULL));
#endif
    xl6SequenceEndV1(pin); assert(!xrtGetError());
    xrtValueRelease(alias); xrtValueRelease(stack);
}
static void fault(const char* operation,size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue* stack=primitive(XL6_SEQUENCE_STACK_V1,xl6StringTypeV1(),2,XL6_VALUE_ADMIT_STRING);
    xvalue* alias=xl6StackEraseV1(stack,STACK_TEXT,xl6StringConverterV1()); assert(stack && alias);
    xstrview input={"old\0\xE4\xBD\xA0",7}; assert(xl6StackPushV1(stack,STACK_TEXT,&input));
    xvalue* extra=xrtValueString((xstrview){"new\0text",8}); assert(extra && xrtValueTypeIdBind(extra,ELEMENT));
    xl6_error_scope call={0}; assert(xl6ErrorScopeBegin(&call));
    assert(xrtMemDebugFailAfter(point)); xvalue* result=NULL; bool ok=false;
    if (!strcmp(operation,"pop")) { result=xl6StackErasedReadV1(alias,0,true); ok=result!=NULL; }
    else if (!strcmp(operation,"peek")) { result=xl6StackErasedReadV1(alias,0,false); ok=result!=NULL; }
    else if (!strcmp(operation,"push")) ok=xl6StackErasedPushV1(alias,extra);
    else if (!strcmp(operation,"clone")) { result=xl6StackCloneV1(alias,STACK_TEXT); ok=result!=NULL; }
    else abort();
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    xl6_error_result outcome=xl6ErrorScopeEnd(&call);
    assert(hit ? !ok : ok && outcome.ok && !outcome.error);
    if (hit && strcmp(operation,"clone")) assert(!outcome.ok);
    xrtErrorFree(outcome.error); xrtClearError();
    int64_t expected=hit ? 1 : !strcmp(operation,"pop") ? 0 : !strcmp(operation,"push") ? 2 : 1;
    assert(xl6StackSizeV1(stack,STACK_TEXT,false)==expected);
    if (expected) {
        xvalue* check=xl6StackErasedReadV1(alias,expected-1,false); text(check,input.Data,input.Size); xrtValueRelease(check);
    }
    xrtValueRelease(result); xrtValueRelease(extra); xrtValueRelease(alias); xrtValueRelease(stack); balanced(&before);
    printf("stack-erased %s point=%zu hit=%d balanced\n",operation,point,hit);
}
static void batchFault(const char* operation,size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue* stack=primitive(XL6_SEQUENCE_STACK_V1,xl6StringTypeV1(),2,XL6_VALUE_ADMIT_STRING);
    xvalue* alias=xl6StackEraseTakeV2(xrtValueRetain(stack),STACK_TEXT,xl6StringConverterV1(),false,ARRAY_OWNER);
    xstrview input={"old\0text",8}; assert(stack && alias && xl6StackPushV1(stack,STACK_TEXT,&input));
    xvalue* maximum=xrtValueInt(1); assert(maximum);
    unsigned method=!strcmp(operation,"batch-pop") ? XL6_STACK_ERASED_POP_BATCH : XL6_STACK_ERASED_PEEK_BATCH;
    xl6_error_scope call={0}; assert(xl6ErrorScopeBegin(&call));
    assert(xrtMemDebugFailAfter(point));
    xvalue* result=xl6StackErasedMethodV1(alias,method,maximum,NULL);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    xl6_error_result outcome=xl6ErrorScopeEnd(&call);
    assert(hit ? !result && !outcome.ok : result && outcome.ok && !outcome.error);
    xrtErrorFree(outcome.error); xrtClearError();
    int64_t expected=!hit && method==XL6_STACK_ERASED_POP_BATCH ? 0 : 1;
    assert(xl6StackSizeV1(stack,STACK_TEXT,false)==expected);
    if(expected) { xvalue* item=xl6StackErasedReadV1(alias,0,false); text(item,input.Data,input.Size); xrtValueRelease(item); }
    if(result) assert(xrtValueTypeId(result)==ARRAY_OWNER);
    xrtValueRelease(result); xrtValueRelease(maximum); xrtValueRelease(alias); xrtValueRelease(stack); balanced(&before);
    printf("stack-erased %s point=%zu hit=%d balanced\n",operation,point,hit);
}
static void batchPendingFailures(void)
{
    for(unsigned mode=1;mode<=3;++mode) for(unsigned take=0;take<=1;++take) {
        xvalue* stack=primitive(XL6_SEQUENCE_STACK_V1,xrtTypeInt64(),1,XL6_VALUE_ADMIT_I8);
        xvalue* alias=xl6StackEraseTakeV2(xrtValueRetain(stack),I64,NULL,false,ARRAY_OWNER);
        int64_t input=17; assert(stack && alias && xl6StackPushV1(stack,I64,&input));
        xvalue* maximum=xrtValueInt(1); assert(maximum);
        xl6_error_scope call={0}; assert(xl6ErrorScopeBegin(&call)); batchResultFailure=mode;
        xvalue* result=xl6StackErasedMethodV1(alias,take?XL6_STACK_ERASED_POP_BATCH:XL6_STACK_ERASED_PEEK_BATCH,maximum,NULL);
        batchResultFailure=0; xl6_error_result outcome=xl6ErrorScopeEnd(&call);
        assert(!result && !outcome.ok && (mode==2 ? outcome.error!=NULL : outcome.error==NULL));
        xrtErrorFree(outcome.error); xrtClearError();
        /* This injected failure is after native publication; Take already
         * committed. It must report failure and destroy its private result. */
        assert(xl6StackSizeV1(stack,I64,false)==(take?0:1));
        xrtValueRelease(maximum); xrtValueRelease(alias); xrtValueRelease(stack);
    }
}
static void tryPendingFailures(void)
{
    for(unsigned mode=1;mode<=5;++mode) for(unsigned take=0;take<=1;++take) {
        xvalue* stack=primitive(XL6_SEQUENCE_STACK_V1,xrtTypeInt64(),1,XL6_VALUE_ADMIT_I8);
        xvalue* alias=xl6StackEraseV1(stack,I64,NULL); int64_t input=17;
        assert(stack && alias && xl6StackPushV1(stack,I64,&input));
        xrtcallresult result=XRT_CALL_RESULT_INIT;
        xl6_error_scope call={0}; assert(xl6ErrorScopeBegin(&call)); tryMoveFailure=mode;
        bool ok=xl6StackErasedTryCallV1(alias,0,take!=0,&result);
        tryMoveFailure=0; xl6_error_result outcome=xl6ErrorScopeEnd(&call);
        assert(!ok && !outcome.ok && !xrtCallResultCount(&result));
        assert(mode==3 ? outcome.error!=NULL : outcome.error==NULL);
        xrtErrorFree(outcome.error); xrtClearError();
        assert(xl6StackSizeV1(stack,I64,false)==(take?0:1));
        xrtCallResultUnit(&result); xrtValueRelease(alias); xrtValueRelease(stack);
    }
}
static void tryFault(const char* operation,size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue* stack=primitive(XL6_SEQUENCE_STACK_V1,xl6StringTypeV1(),2,XL6_VALUE_ADMIT_STRING);
    xvalue* alias=xl6StackEraseV1(stack,STACK_TEXT,xl6StringConverterV1()); assert(stack && alias);
    bool empty=!strcmp(operation,"try-empty"),pop=!strcmp(operation,"try-pop");
    xstrview input={"old\0text",8}; if(!empty)assert(xl6StackPushV1(stack,STACK_TEXT,&input));
    xrtcallresult result=XRT_CALL_RESULT_INIT;
    xl6_error_scope call={0}; assert(xl6ErrorScopeBegin(&call)); assert(xrtMemDebugFailAfter(point));
    bool ok=xl6StackErasedTryCallV1(alias,0,pop,&result);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    xl6_error_result outcome=xl6ErrorScopeEnd(&call);
    assert(hit ? !ok && !outcome.ok && !xrtCallResultCount(&result) : ok && outcome.ok && !outcome.error);
    xrtErrorFree(outcome.error); xrtClearError();
    int64_t expected=empty || (!hit && pop) ? 0 : 1;
    assert(xl6StackSizeV1(stack,STACK_TEXT,false)==expected);
    if(ok) { tryVector(&result,!empty,empty?XVALUE_NULL:XVALUE_STRING); if(!empty)text(xrtCallResultGet(&result,1),input.Data,input.Size); }
    if(expected) { xvalue* item=xl6StackErasedReadV1(alias,0,false); text(item,input.Data,input.Size); xrtValueRelease(item); }
    xrtCallResultUnit(&result); xrtValueRelease(alias); xrtValueRelease(stack); balanced(&before);
    printf("stack-erased %s point=%zu hit=%d balanced\n",operation,point,hit);
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true));
    if (argc==3) {
        size_t point=(size_t)strtoull(argv[2],NULL,10);
        if(!strncmp(argv[1],"try-",4)) tryFault(argv[1],point);
        else if(!strncmp(argv[1],"batch-",6)) batchFault(argv[1],point); else fault(argv[1],point);
        return 0;
    }
    assert(argc==1); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    scalar(); precision(); rejection(); callbacks(); busy(); batchMetadata(); batchPendingFailures(); balanced(&before);
    tryResults(); tryCallbacks(); tryPendingFailures(); balanced(&before);
    puts("Stack projection scalar/text/reference/role/failure/busy balanced"); return 0;
}
