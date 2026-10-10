#define main xl6ExistingListMethodsMain
#include "test/cases/stdlib_list_methods_native_host.c"
#undef main
#include "src/runtime/xl6_list_typed_read.h"
#include "src/runtime/xl6_list_value_read.h"
#include "src/runtime/xl6_list_value_write.h"

static unsigned providerCountdown,providerSeen;
static bool providerFailure(void)
{ if (!providerCountdown || ++providerSeen!=providerCountdown) return false; xrtClearError(); return true; }
extern bool __real_xl6ListGetV1(const xvalue*,const XL6_ListLayoutV1*,int64_t,void*,bool,bool*);
bool __wrap_xl6ListGetV1(const xvalue* source,const XL6_ListLayoutV1* t,int64_t key,void* out,bool take,bool* found)
{ return !providerFailure() && __real_xl6ListGetV1(source,t,key,out,take,found); }
extern bool __real_xl6ListErasedReadV1(const xvalue*,int64_t,bool,xvalue**,bool*);
bool __wrap_xl6ListErasedReadV1(const xvalue* source,int64_t key,bool take,xvalue** out,bool* found)
{ return !providerFailure() && __real_xl6ListErasedReadV1(source,key,take,out,found); }
extern bool __real_xl6ListErasedSetV1(const xvalue*,int64_t,const xvalue*);
bool __wrap_xl6ListErasedSetV1(const xvalue* source,int64_t key,const xvalue* item)
{ return !providerFailure() && __real_xl6ListErasedSetV1(source,key,item); }
static void textExact(xstrview value,const char* expected,size_t size)
{ assert(value.Data && value.Size==size && !memcmp(value.Data,expected,size)); }
static void staticVectors(void)
{
    XL6_ListLayoutV1 t=tuple(); t.value.transfer=1; t.value.type=xrtTypeInt64(); t.value.size=sizeof(int64_t);
    xvalue* list=profile(&t,XL6_VALUE_ADMIT_I64,false,t.value.elementIdentity,NULL);
    int64_t number=0; assert(xl6ListSetV1(list,&t,INT64_MIN,&number));
    assert(xl6ListReadI64V1(list,t.identity,t.value.identity,t.value.elementIdentity,INT64_MIN)==0 && !xrtGetError());
    assert(xrtMemDebugFailAfter(0));
    assert(xl6ListReadI64V1(list,t.identity,t.value.identity,t.value.elementIdentity,INT64_MIN)==0 &&
        !xrtGetError() && !xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    assert(xl6ListReadI64V1(list,t.identity,t.value.identity,t.value.elementIdentity,INT64_MAX)==0); checkError(XERR_RANGE);
    assert(xl6ListReadI64V1(list,t.identity+17,t.value.identity,t.value.elementIdentity,INT64_MIN)==0); checkError(XERR_STATE);
    assert(xl6ListReadI64V1(list,t.identity,t.value.identity+17,t.value.elementIdentity,INT64_MIN)==0); checkError(XERR_STATE);
    assert(xl6ListReadI64V1(list,t.identity,t.value.identity,t.value.elementIdentity+17,INT64_MIN)==0); checkError(XERR_STATE);
    assert(xl6ListValueSetTakeV1(list,INT64_MAX,xrtValueInt(7)));
    assert(xl6ListReadI64V1(list,t.identity,t.value.identity,t.value.elementIdentity,INT64_MAX)==7);
    assert(!xl6ListValueSetTakeV1(list,INT64_MAX,query())); checkError(XERR_TYPE);
    assert(xl6ListReadI64V1(list,t.identity,t.value.identity,t.value.elementIdentity,INT64_MAX)==7);
    xrtValueRelease(list);
    t.value.type=xrtTypeUInt64(); t.value.size=sizeof(uint64_t);
    list=profile(&t,XL6_VALUE_ADMIT_U64,false,t.value.elementIdentity,NULL);
    uint64_t wide=UINT64_MAX; assert(xl6ListSetV1(list,&t,-7,&wide));
    assert(xl6ListReadU64V1(list,t.identity,t.value.identity,t.value.elementIdentity,-7)==UINT64_MAX);
    assert(xl6ListReadI64V1(list,t.identity,t.value.identity,t.value.elementIdentity,-7)==0); checkError(XERR_STATE);
    xrtValueRelease(list);
    t.value.type=xrtTypeBool(); t.value.size=sizeof(bool); list=profile(&t,XL6_VALUE_ADMIT_BOOL,false,t.value.elementIdentity,NULL);
    bool bit=false; assert(xl6ListSetV1(list,&t,0,&bit));
    assert(!xl6ListReadBoolV1(list,t.identity,t.value.identity,t.value.elementIdentity,0) && !xrtGetError());
    xrtValueRelease(list);
    t.value.type=xrtTypeFloat64(); t.value.size=sizeof(double); list=profile(&t,XL6_VALUE_ADMIT_F64,false,t.value.elementIdentity,NULL);
    double real=0.0; assert(xl6ListSetV1(list,&t,0,&real));
    assert(xl6ListReadF64V1(list,t.identity,t.value.identity,t.value.elementIdentity,0)==0.0 && !xrtGetError());
    xrtValueRelease(list);
    t=tuple(); list=create(true); set(list,INT64_MIN);
    xstrview text=xl6ListReadStringV1(list,t.identity,t.value.identity,t.value.elementIdentity,INT64_MIN);
    textExact(text,"a\0\xE4\xBD\xA0",5); xrtFree((void*)text.Data);
    text=xl6ListReadStringV1(list,t.identity,t.value.identity,t.value.elementIdentity,7);
    assert(!text.Data && !text.Size); checkError(XERR_RANGE);
    xvalue* missing=xl6ListValueReadCopyV1(list,7); assert(missing && xrtValueType(missing)==XVALUE_NULL); xrtValueRelease(missing);
    missing=xl6ListValueReadOrNullCopyV1(NULL,7); assert(missing && xrtValueType(missing)==XVALUE_NULL); xrtValueRelease(missing);
    missing=xl6ListValueReadOrNullCopyV1(xrtValueNull(),7); assert(missing && xrtValueType(missing)==XVALUE_NULL); xrtValueRelease(missing);
    xvalue* needle=query(); assert(needle);
    xrtSetErrorKind(XERR_ARGUMENT); const xerror* incoming=xrtGetError(); assert(incoming);
    text=xl6ListReadStringV1(list,t.identity,t.value.identity,t.value.elementIdentity,INT64_MIN);
    textExact(text,"a\0\xE4\xBD\xA0",5); assert(xrtGetError()==incoming); xrtFree((void*)text.Data); xrtClearError();
    assert(xl6ListValueSetTakeV1(list,INT64_MAX,needle)); xrtValueRelease(list);
    /* Explicit wire migration inputs share checked semantics, not aborting C helpers. */
    list=xrtValueIntMap(); assert(list && xl6ListValueSetTakeV1(list,INT64_MIN,query()));
    text=xl6ListReadStringV1(list,t.identity,t.value.identity,t.value.elementIdentity,INT64_MIN);
    textExact(text,"a\0\xE4\xBD\xA0",5); xrtFree((void*)text.Data); xrtValueRelease(list);
    void* pointer=(void*)1; list=xrtValueHandleTake(&pointer,&foreignOps,NULL); assert(list && xrtValueTypeIdBind(list,t.identity));
    text=xl6ListReadStringV1(list,t.identity,t.value.identity,t.value.elementIdentity,0);
    assert(!text.Data); checkError(XERR_TYPE); assert(!xl6ListValueSetTakeV1(list,0,query())); checkError(XERR_TYPE);
    xrtValueRelease(list);
}
static bool staticAttempt(unsigned operation,size_t point,const char* mode)
{
    xl6ProductMemoryBegin(SIZE_MAX); XL6_ListLayoutV1 t=tuple(); xvalue* list=create(true);
    set(list,INT64_MIN); set(list,INT64_MAX); xvalue* input=query(); assert(input);
    xl6_error_scope boundary={0}; assert(xl6ErrorScopeBegin(&boundary));
    bool scopes=!strcmp(mode,"scopes"),providers=!strcmp(mode,"providers");
    if (scopes) { scopeSeen=0; scopeCountdown=(int)point+1; }
    else if (providers) { providerSeen=0; providerCountdown=(unsigned)point+1; }
    else assert(xrtMemDebugFailAfter(point));
    xstrview text={0}; xvalue* value=NULL; bool success;
    if (!operation) { text=xl6ListReadStringV1(list,t.identity,t.value.identity,t.value.elementIdentity,INT64_MIN); success=text.Data!=NULL; }
    else if (operation==1) { value=xl6ListValueReadCopyV1(list,INT64_MIN); success=value!=NULL; }
    else { success=xl6ListValueSetTakeV1(list,INT64_MAX,input); input=NULL; }
    bool hit=scopes ? scopeSeen>=scopeCountdown : providers ? providerSeen>=providerCountdown : xrtMemDebugFailTriggered();
    scopeCountdown=0; providerCountdown=0; xrtMemDebugFailClear();
    if (!success) xl6ErrorScopeCaptureFailure(&boundary);
    xl6_error_result outcome=xl6ErrorScopeEnd(&boundary);
    assert(success==!hit && outcome.ok==!hit);
    if (text.Data) textExact(text,"a\0\xE4\xBD\xA0",5);
    if (value) { xstrview view={0}; assert(xrtValueGetString(value,&view)); textExact(view,"a\0\xE4\xBD\xA0",5); }
    assert(xl6ListValueCountV1(list)==2);
    xrtErrorFree(outcome.error); xrtFree((void*)text.Data); xrtValueRelease(value); xrtValueRelease(input); xrtValueRelease(list);
    xrtClearError(); assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    printf("List static operation=%u point=%zu hit=%d balanced\n",operation,point,hit); return hit;
}
#ifndef XL6_STATIC_INDEX_NO_MAIN
int main(int argc,char** argv)
{
    assert(argc==2);
    if (!strcmp(argv[1],"vectors")) {
        xl6ProductMemoryBegin(SIZE_MAX); staticVectors(); assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
        puts("Static List scalar/string/owned/safe/write vectors balanced"); return 0;
    }
    assert(!strcmp(argv[1],"oom") || !strcmp(argv[1],"scopes") || !strcmp(argv[1],"providers"));
    for (unsigned operation=0;operation<3;++operation) {
        bool closed=false;
        for (size_t point=0;point<4096;++point) if (!staticAttempt(operation,point,argv[1])) {
            printf("List static closed-batch operation=%u runs=%zu first-no-hit=%zu\n",operation,point+1,point); closed=true; break;
        }
        assert(closed);
    }
    return 0;
}
#endif
