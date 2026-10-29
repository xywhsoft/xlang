#define main xl6ListExistingMethodsMain
#include "stdlib_list_methods_native_host.c"
#undef main
#include "src/runtime/xl6_list_algebra.h"

static unsigned providerCountdown,providerSeen;
static bool providerFailure(void)
{
    if (!providerCountdown || ++providerSeen!=providerCountdown) return false;
    xrtClearError(); return true;
}
extern xvalue* __real_xl6ListErasedCloneV1(const xvalue*);
xvalue* __wrap_xl6ListErasedCloneV1(const xvalue* value)
{ return providerFailure() ? NULL : __real_xl6ListErasedCloneV1(value); }
extern bool __real_xl6ListMergeV1(xvalue*,const xvalue*,const XL6_ListLayoutV1*,bool);
bool __wrap_xl6ListMergeV1(xvalue* a,const xvalue* b,const XL6_ListLayoutV1* t,bool replace)
{ return !providerFailure() && __real_xl6ListMergeV1(a,b,t,replace); }
extern bool __real_xl6ListErasedSetV1(xvalue*,int64_t,const xvalue*);
bool __wrap_xl6ListErasedSetV1(xvalue* a,int64_t key,const xvalue* value)
{ return !providerFailure() && __real_xl6ListErasedSetV1(a,key,value); }
extern bool __real_xl6ListErasedKeyV1(xvalue*,int64_t,bool,bool*);
bool __wrap_xl6ListErasedKeyV1(xvalue* a,int64_t key,bool remove,bool* found)
{ return !providerFailure() && __real_xl6ListErasedKeyV1(a,key,remove,found); }

static void algebraSet(xvalue* list,int64_t key,const char* text,size_t size)
{
    if (xl6ListErasedProbeV1(list)) {
        XL6_ListLayoutV1 t=tuple(); xstrview item={text,size}; assert(xl6ListSetV1(list,&t,key,&item));
    } else {
        xvalue* item=xrtValueString((xstrview){text,size}); assert(item);
        assert(xrtValueIntMapSet(list,key,item)); xrtValueRelease(item);
    }
}
static xvalue* algebraInput(bool native,bool right)
{
    xvalue* list=native ? create(true) : xrtValueIntMap(); assert(list);
    algebraSet(list,INT64_MIN,right ? "r\0x" : "a\0\xE4\xBD\xA0",right ? 3 : 5);
    algebraSet(list,right ? 17 : INT64_MAX,"z",1); return list;
}
static void algebraText(const xvalue* list,int64_t key,const char* expected,size_t size)
{
    bool found=false; xvalue* value=NULL;
    if (xl6ListErasedProbeV1(list)) assert(xl6ListErasedReadV1(list,key,false,&value,&found));
    else { const xvalue* item=xrtValueIntMapGet(list,key); found=item!=NULL; value=xrtValueRetain(item); }
    if (!expected) assert(!found && !value);
    else {
        xstrview text={0}; assert(found && value && xrtValueGetString(value,&text));
        assert(text.Size==size && !memcmp(text.Data,expected,size));
    }
    xrtValueRelease(value);
}
static void unchanged(const xvalue* left,const xvalue* right)
{
    assert(xl6ListValueCountV1(left)==2 && xl6ListValueCountV1(right)==2);
    algebraText(left,INT64_MIN,"a\0\xE4\xBD\xA0",5); algebraText(left,INT64_MAX,"z",1);
    algebraText(right,INT64_MIN,"r\0x",3); algebraText(right,17,"z",1);
}
/* Cases 0..1 clone; 2..5 all carrier pairs; 6 self merge; 7..10 remove
 * present/absent from either carrier. All are independent owned results. */
static xvalue* algebraOperation(unsigned operation,const xvalue* left,const xvalue* right)
{
    if (operation<2) return xl6ListValueCloneV1(left);
    if (operation<7) return xl6ListValueMergeRightV1(left,operation==6 ? left : right);
    return xl6ListValueWithoutKeyV1(left,(operation==7 || operation==9) ? INT64_MIN : -17);
}
static bool leftNative(unsigned operation)
{ return operation==0 || operation==2 || operation==3 || operation==6 || operation==7 || operation==8; }
static bool rightNative(unsigned operation) { return operation!=3 && operation!=5; }
static void algebraOutput(unsigned operation,const xvalue* result)
{
    assert(result && xl6ListErasedProbeV1(result)==(int)leftNative(operation));
    bool removed=operation==7 || operation==9;
    bool merged=operation>=2 && operation<6;
    assert(xl6ListValueCountV1(result)==(removed ? 1 : merged ? 3 : 2));
    algebraText(result,INT64_MIN,removed ? NULL : merged ? "r\0x" : "a\0\xE4\xBD\xA0",merged ? 3 : 5);
    algebraText(result,INT64_MAX,"z",1);
    if (merged) algebraText(result,17,"z",1);
}
static void algebraVectors(void)
{
    for (unsigned operation=0;operation<11;++operation) {
        xvalue* left=algebraInput(leftNative(operation),false),*right=algebraInput(rightNative(operation),true);
        xvalue* result=algebraOperation(operation,left,right); algebraOutput(operation,result); unchanged(left,right);
        algebraSet(result,INT64_MAX,"changed",7); unchanged(left,right);
        xrtValueRelease(result); xrtValueRelease(right); xrtValueRelease(left);
    }
    /* Copy keeps exact tuple and immutable admission, including null vs empty. */
    XL6_ListLayoutV1 t=tuple(); t.value.elementIdentity++;
    t.value.type=xl6NullableStringTypeV1();
    xvalue* source=profile(&t,XL6_VALUE_ADMIT_STRING,true,t.value.elementIdentity-1,xl6NullableStringConverterV1());
    xstrview empty={"",0},absent={0}; assert(xl6ListSetV1(source,&t,0,&absent) && xl6ListSetV1(source,&t,1,&empty));
    xvalue* copy=xl6ListValueCloneV1(source); assert(copy);
    XL6_ListLayoutV1 actual={0}; XL6_SequenceAdmissionV3 policy={0};
    assert(xl6ListSchemaCopyV1(copy,&actual,&policy) && actual.identity==t.identity &&
        actual.value.identity==t.value.identity && actual.value.elementIdentity==t.value.elementIdentity &&
        actual.value.type==t.value.type); xl6SequenceAdmissionDropV3(&policy);
    xvalue* value=NULL; bool found=false;
    assert(xl6ListErasedReadV1(copy,0,false,&value,&found) && found && xrtValueType(value)==XVALUE_NULL);
    xrtValueRelease(value); value=xrtValueString((xstrview){"a\0b",3}); assert(value);
    assert(xl6ListErasedSetV1(copy,0,value)); xrtValueRelease(value); value=NULL;
    assert(xl6ListErasedReadV1(source,0,false,&value,&found) && found && xrtValueType(value)==XVALUE_NULL);
    xrtValueRelease(value); algebraText(copy,1,"",0); xrtValueRelease(copy); xrtValueRelease(source);
    /* Reference slots retain value objects, not containers' mutable storage. */
    t=tuple(); t.value.transfer=3; t.value.type=xrtTypeValue(); t.value.size=sizeof(xvalue*);
    source=profile(&t,XL6_VALUE_ADMIT_BYTES,false,t.value.elementIdentity,NULL);
    value=xrtValueBytes((xbytesview){(const uint8_t*)"a\0b",3}); assert(value && xl6ListSetV1(source,&t,0,&value));
    xrtValueRelease(value); value=NULL;
    copy=xl6ListValueCloneV1(source); assert(copy);
    assert(xl6ListErasedReadV1(copy,0,false,&value,&found) && found);
    xbytesview bytes={0}; assert(xrtValueGetBytes(value,&bytes) && bytes.Size==3 && !memcmp(bytes.Data,"a\0b",3));
    xrtValueRelease(value); xrtValueRelease(copy); xrtValueRelease(source);
}
static void algebraRejected(void)
{
    xvalue* left=algebraInput(true,false),*right=algebraInput(true,true);
    XL6_ListLayoutV1 t=tuple(); void* raw=NULL,*context=NULL; const xvaluehandleops* ops=NULL;
    assert(xrtValueGetHandle(left,&raw,&ops,&context)); XL6_ListGraphViewV1 view={0};
    assert(xl6ListGraphBeginV1(ops,raw,context,t.identity,&view)==1);
    assert(!xl6ListValueCloneV1(left)); checkError(XERR_STATE); xl6ListGraphEndV1(&view);
    xvalue* unbound=create(false); assert(!xl6ListValueCloneV1(unbound)); checkError(XERR_STATE); xrtValueRelease(unbound);
    xvalue* cursor=xl6ListCursorCreateV1(left,&t,0xD713,false,false,0); assert(cursor);
    assert(!xl6ListValueCloneV1(cursor)); checkError(XERR_TYPE); xrtValueRelease(cursor);
    void* pointer=(void*)1; xvalue* fake=xrtValueHandleTake(&pointer,&foreignOps,NULL); assert(fake);
    assert(xrtValueTypeIdBind(fake,t.identity)); assert(!xl6ListValueCloneV1(fake)); checkError(XERR_TYPE); xrtValueRelease(fake);
    XL6_ListLayoutV1 wrong=t; ++wrong.value.identity;
    xvalue* other=profile(&wrong,XL6_VALUE_ADMIT_STRING,false,wrong.value.elementIdentity,xl6StringConverterV1());
    assert(!xl6ListValueMergeRightV1(left,other)); checkError(XERR_STATE); xrtValueRelease(other);
    other=xrtValueIntMap(); xvalue* integer=xrtValueInt(7); assert(other && integer && xrtValueIntMapSet(other,0,integer));
    assert(!xl6ListValueMergeRightV1(left,other)); checkError(XERR_TYPE);
    xrtValueRelease(integer); xrtValueRelease(other); unchanged(left,right);
    assert(!xl6ListValueCloneV1(NULL)); checkError(XERR_ARGUMENT);
    /* Ambient diagnostic restored on success, never treated as new failure. */
    xrtSetErrorKind(XERR_ARGUMENT); const xerror* prior=xrtGetError();
    xvalue* copy=xl6ListValueCloneV1(left); assert(copy && xrtGetError()==prior); xrtClearError(); xrtValueRelease(copy);
    xrtValueRelease(right); xrtValueRelease(left);
}
static bool algebraAttempt(unsigned operation,size_t point,unsigned mode)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    xvalue* left=algebraInput(leftNative(operation),false),*right=algebraInput(rightNative(operation),true);
    xl6_error_scope outer={0}; assert(xl6ErrorScopeBegin(&outer));
    if (mode==1) { scopeSeen=0; scopeCountdown=(int)point+1; }
    else if (mode==2) { providerSeen=0; providerCountdown=(unsigned)point+1; }
    else assert(xrtMemDebugFailAfter(point));
    xvalue* result=algebraOperation(operation,left,right);
    bool hit=mode==1 ? scopeSeen>=scopeCountdown : mode==2 ? providerSeen>=providerCountdown : xrtMemDebugFailTriggered();
    scopeCountdown=0; providerCountdown=0; xrtMemDebugFailClear();
    xl6_error_result outcome=xl6ErrorScopeEnd(&outer);
    assert(hit ? !result && !outcome.ok : result && outcome.ok);
    xrtErrorFree(outcome.error); unchanged(left,right); if (result) algebraOutput(operation,result);
    xrtValueRelease(result); xrtValueRelease(right); xrtValueRelease(left); xrtClearError();
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    printf("List algebra operation=%u point=%zu hit=%d balanced\n",operation,point,hit); return hit;
}
#ifndef XL6_LIST_ALGEBRA_NO_MAIN
int main(int argc,char** argv)
{
    assert(argc==2);
    if (!strcmp(argv[1],"vectors")) {
        xl6ProductMemoryBegin(SIZE_MAX); algebraVectors(); algebraRejected();
        assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
        puts("List algebra vectors balanced"); return 0;
    }
    unsigned mode=!strcmp(argv[1],"scopes") ? 1 : !strcmp(argv[1],"providers") ? 2 : 0;
    assert(mode || !strcmp(argv[1],"oom"));
    for (unsigned operation=0;operation<11;++operation) {
        bool closed=false;
        for (size_t point=0;point<4096;++point) if (!algebraAttempt(operation,point,mode)) {
            printf("List algebra closed-batch operation=%u runs=%zu first-no-hit=%zu\n",operation,point+1,point); closed=true; break;
        }
        assert(closed);
    }
    return 0;
}
#endif
