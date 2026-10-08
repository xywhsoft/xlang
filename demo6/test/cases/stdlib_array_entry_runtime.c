#include "src/runtime/xl6_array_entry.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_input.h"
#include "src/runtime/xl6_array_os_argv.h"
#include "src/runtime/xl6_dynamic_collection.h"
#include "src/runtime/xl6_value_contains.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ARRAY_ID UINT64_C(0x12345678)
#define VAR_ID UINT64_C(0xabcdefff)
static unsigned drops;
static void failedDrop(void* token,void* context)
{
    (void)token; (void)context; ++drops;
    xrtSetErrorKind(XERR_ARGUMENT); xl6ForeignCallbackFail();
}
static const xvaluehandleops failedOps={.Drop=failedDrop};
static void boolean(xvalue* result,bool expected)
{ bool actual=false; assert(result && xrtValueGetBool(result,&actual) && actual==expected); xrtValueRelease(result); }
static void number(xvalue* result,int64_t expected)
{ int64_t actual=0; assert(result && xrtValueGetInt(result,&actual) && actual==expected); xrtValueRelease(result); }
static void dynamicVectors(void)
{
    for (int native=0;native<2;++native) {
        xvalue* a=native ? xl6ArrayEntryAcquireV1(ARRAY_ID,VAR_ID,NULL) : xrtValueArray(); assert(a);
        xvalue* alias=xrtValueRetain(a); assert(alias);
        boolean(xl6DynamicCollectionCallTakeV1(a,XL6_DYNAMIC_COLLECTION_PUSH,xrtValueInt(1),NULL),true);
        boolean(xl6DynamicCollectionCallTakeV1(alias,XL6_DYNAMIC_COLLECTION_PUSH,xrtValueInt(3),NULL),true);
        boolean(xl6DynamicCollectionCallTakeV1(a,XL6_DYNAMIC_COLLECTION_INSERT,xrtValueInt(1),xrtValueInt(2)),true);
        xvalue* needle=xrtValueInt(3); assert(needle && xl6ValueContainsV1(a,needle)); xrtValueRelease(needle);
        boolean(xl6DynamicCollectionCallTakeV1(a,XL6_DYNAMIC_COLLECTION_REMOVE,xrtValueInt(2),NULL),true);
        number(xl6DynamicCollectionCallTakeV1(a,XL6_DYNAMIC_COLLECTION_POP,NULL,NULL),2);
        boolean(xl6DynamicCollectionCallTakeV1(a,XL6_DYNAMIC_COLLECTION_RESERVE,xrtValueInt(8),NULL),true);
        number(xl6DynamicCollectionCallTakeV1(a,XL6_DYNAMIC_COLLECTION_INDEX_OF,xrtValueInt(1),NULL),0);
        boolean(xl6DynamicCollectionCallTakeV1(a,XL6_DYNAMIC_COLLECTION_REMOVE_VALUE,xrtValueInt(99),NULL),false);
        assert(!xl6DynamicCollectionCallTakeV1(a,XL6_DYNAMIC_COLLECTION_TAKE,xrtValueInt(-1),NULL));
        assert(xrtErrorKind(xrtGetError())==XERR_RANGE); xrtClearError();
        number(xl6DynamicCollectionCallTakeV1(a,XL6_DYNAMIC_COLLECTION_TAKE,xrtValueInt(0),NULL),1);
        boolean(xl6DynamicCollectionCallTakeV1(a,XL6_DYNAMIC_COLLECTION_CLEAR,NULL,NULL),true);
        assert(native ? xl6ArrayErasedSizeV1(alias)==0 : xrtValueCount(alias)==0);
        assert(!xl6DynamicCollectionCallTakeV1(a,XL6_DYNAMIC_COLLECTION_POP,NULL,NULL));
        assert(xrtErrorKind(xrtGetError())==XERR_RANGE); xrtClearError();
        xrtValueRelease(alias); xrtValueRelease(a);
    }
}
static void vectors(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    dynamicVectors();
    xvalue* a=xl6ArrayEntryAcquireV1(ARRAY_ID,VAR_ID,NULL); assert(a);
    assert(xl6SequenceAdmitsRoleV1(a,XL6_SEQUENCE_ARRAY_V1,ARRAY_ID,VAR_ID,3,sizeof(xvalue*),xrtTypeValue()));
    assert(xrtMemDebugFailAfter(0));
    xvalue* b=xl6ArrayEntryAcquireV1(ARRAY_ID,VAR_ID,a);
    assert(b==a && !xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    XL6_Sequence* pinned=xl6SequenceBeginV1(a,ARRAY_ID,VAR_ID,3,sizeof(xvalue*),xrtTypeValue()); assert(pinned);
    assert(!xl6ArrayEntryAcquireV1(ARRAY_ID,VAR_ID,a) && xrtErrorKind(xrtGetError())==XERR_STATE);
    xl6SequenceEndV1(pinned); xrtClearError();
    xrtValueRelease(a); xrtValueRelease(b);
    xvalue* wire=xrtValueArray(); assert(wire);
    assert(xrtValueArrayAppendNew(wire,xrtValueString((xstrview){"a\0\xe4\xbd\xa0",5})));
    assert(xrtValueArrayAppendNew(wire,xrtValueNull()));
    a=xl6ArrayEntryAcquireV1(ARRAY_ID,VAR_ID,wire); assert(a && a!=wire);
    assert(xl6SequenceAdmitsRoleV1(a,XL6_SEQUENCE_ARRAY_V1,ARRAY_ID,VAR_ID,3,sizeof(xvalue*),xrtTypeValue()) && xl6ArrayErasedSizeV1(a)==2);
    xvalue* item=xl6ArrayErasedReadV1(a,0); xstrview view={0};
    assert(item && xrtValueGetString(item,&view) && view.Size==5 && !memcmp(view.Data,"a\0\xe4\xbd\xa0",5));
    xrtValueRelease(item); xrtValueRelease(a); xrtValueRelease(wire);
    /* Same owner/layout IDs but a different full admission policy cannot be
     * reused as ANY. Explicit conversion must make a fresh canonical owner. */
    a=xl6SequenceReferenceCreateV1(XL6_SEQUENCE_ARRAY_V1,ARRAY_ID,VAR_ID,XL6_VALUE_ADMIT_BYTES,false,VAR_ID); assert(a);
    a=xl6ArrayProjectionTakeV1(a,ARRAY_ID,VAR_ID,3,sizeof(xvalue*),xrtTypeValue(),NULL); assert(a);
    b=xl6ArrayEntryAcquireV1(ARRAY_ID,VAR_ID,a);
    if(!b) fprintf(stderr,"policy conversion failed: kind=%d %s\n",(int)xrtErrorKind(xrtGetError()),xrtErrorMessage(xrtGetError()));
    assert(b && b!=a);
    assert(xl6SequenceAdmitsRoleV1(b,XL6_SEQUENCE_ARRAY_V1,ARRAY_ID,VAR_ID,3,sizeof(xvalue*),xrtTypeValue())); xrtValueRelease(a); xrtValueRelease(b);
    char first[]="",second[]="\xe4\xbd\xa0"; char* arguments[]={first,second};
    a=xl6ArrayArgvCreateV1(ARRAY_ID,VAR_ID,2,arguments); assert(a && xl6ArrayErasedSizeV1(a)==2);
    second[0]='X'; item=xl6ArrayErasedReadV1(a,1);
    assert(xrtValueGetString(item,&view) && view.Size==3 && !memcmp(view.Data,"\xe4\xbd\xa0",3));
    xrtValueRelease(item); xrtValueRelease(a);
    assert(!xl6ArrayArgvCreateV1(ARRAY_ID,VAR_ID,-1,arguments)); xrtClearError();
    assert(!xl6ArrayArgvCreateV1(ARRAY_ID,VAR_ID,1,NULL)); xrtClearError();
    arguments[0]=NULL; assert(!xl6ArrayArgvCreateV1(ARRAY_ID,VAR_ID,2,arguments)); xrtClearError();
    assert(!xl6ArrayEntryAcquireV1(0,VAR_ID,NULL)); xrtClearError();
    assert(!xl6ArrayEntryAcquireV1(ARRAY_ID,VAR_ID,xrtValueNull())); xrtClearError();
    /* A NULL diagnostic is still the first failure. Later cleanup diagnostics
     * must not turn it into a different error or successful return. */
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
    xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    XL6_ArrayInputV1 input={0}; input.items=xrtMalloc(2*sizeof(xvalue*)); assert(input.items);
    void* token=(void*)1; input.items[0]=xrtValueHandleTake(&token,&failedOps,NULL); assert(input.items[0]);
    token=(void*)2; input.items[1]=xrtValueHandleTake(&token,&failedOps,NULL); assert(input.items[1]); input.count=2;
    xl6ArrayInputReleaseV1(&input);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope); assert(!outcome.ok && !outcome.error);
    assert(drops==2 && !input.items && !input.count);
    assert(xl6ErrorScopeBegin(&scope));
    input.items=xrtMalloc(sizeof(xvalue*)); assert(input.items);
    token=(void*)3; input.items[0]=xrtValueHandleTake(&token,&failedOps,NULL); assert(input.items[0]); input.count=1;
    xl6ArrayInputReleaseV1(&input); outcome=xl6ErrorScopeEnd(&scope);
    assert(!outcome.ok && xrtErrorKind(outcome.error)==XERR_ARGUMENT && drops==3);
    xrtErrorFree(outcome.error); xrtClearError();
    uint16_t wideEmpty[]={0},wideUnicode[]={0x4f60,0xd83d,0xde42,0};
    const uint16_t* wide[]={wideEmpty,wideUnicode};
    a=xl6ArrayUtf16ArgvCreateV1(ARRAY_ID,VAR_ID,2,wide); assert(a);
    wideUnicode[0]='X'; item=xl6ArrayErasedReadV1(a,1);
    assert(xrtValueGetString(item,&view) && view.Size==7 && !memcmp(view.Data,"\xe4\xbd\xa0\xf0\x9f\x99\x82",7));
    xrtValueRelease(item); xrtValueRelease(a);
    uint16_t invalid[]={0xd800,0}; wide[1]=invalid;
    assert(!xl6ArrayUtf16ArgvCreateV1(ARRAY_ID,VAR_ID,2,wide)); assert(xrtGetError()); xrtClearError();
    wide[1]=NULL; assert(!xl6ArrayUtf16ArgvCreateV1(ARRAY_ID,VAR_ID,2,wide)); xrtClearError();
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    puts("native entry vectors balanced");
}
int main(int argc,char** argv)
{
    assert(argc==1 || argc==3); if(argc==1) { vectors(); return 0; }
    xl6ProductMemoryBegin(SIZE_MAX);
    xvalue* source=xrtValueArray(); assert(source);
    for(int i=0;i<24;++i) assert(xrtValueArrayAppendNew(source,xrtValueString((xstrview){"a\0b",3})));
    char* arguments[24]; for(int i=0;i<24;++i) arguments[i]="value";
    uint16_t word[]={0x4f60,0xd83d,0xde42,0}; const uint16_t* wide[24];
    for(int i=0;i<24;++i) wide[i]=word;
    assert(xrtMemDebugFailAfter((size_t)strtoull(argv[2],NULL,10)));
    xvalue* a=!strcmp(argv[1],"utf16") ? xl6ArrayUtf16ArgvCreateV1(ARRAY_ID,VAR_ID,24,wide) :
        !strcmp(argv[1],"argv") ? xl6ArrayArgvCreateV1(ARRAY_ID,VAR_ID,24,arguments) :
        xl6ArrayEntryAcquireV1(ARRAY_ID,VAR_ID,source);
    bool hit=xl6ProductMemoryEnd(); xerror* error=xrtTakeError();
    assert(hit ? !a && xrtErrorKind(error)==XERR_MEMORY : a && !error);
    xrtValueRelease(a); xrtErrorFree(error); xrtClearError();
    assert(xrtValueCount(source)==24); xrtValueRelease(source);
    xl6ProductMemoryVerify(); printf("native entry %s point=%s hit=%d balanced\n",argv[1],argv[2],hit); return 0;
}
