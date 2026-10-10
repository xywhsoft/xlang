#include "src/runtime/xl6_list.h"
#include "src/runtime/xl6_list_graph_access.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "src/runtime/xl6_value_copy.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_array_graph.h"
#include "src/runtime/xl6_sequence_projection.h"

static XL6_ListLayoutV1 tuple(void)
{ return (XL6_ListLayoutV1){0xD710,{0xD711,0xD712,2,sizeof(xstrview),xl6StringTypeV1()}}; }
static xvalue* profile(XL6_ListLayoutV1* t,unsigned rule,bool nullable,uint64_t base,
    const xvalueconverter* converter)
{
    xvalue* witness=t->value.transfer==3 ? xl6SequenceReferenceCreateV1(1,t->value.identity,
        t->value.elementIdentity,rule,nullable,base) : xl6SequencePrimitiveCreateV1(1,t->value.identity,
        t->value.elementIdentity,t->value.transfer,t->value.size,t->value.type,rule,nullable,base);
    assert(witness); xvalue* result=xl6ListProjectionTakeV1(xl6ListCreateTakeV1(t,witness),t,converter);
    assert(result); return result;
}
static xvalue* create(bool project)
{
    XL6_ListLayoutV1 t=tuple();
    xvalue* witness=xl6SequencePrimitiveCreateV1(1,t.value.identity,t.value.elementIdentity,
        t.value.transfer,t.value.size,t.value.type,XL6_VALUE_ADMIT_STRING,false,t.value.elementIdentity);
    assert(witness); xvalue* result=xl6ListCreateTakeV1(&t,witness);
    if (project) result=xl6ListProjectionTakeV1(result,&t,xl6StringConverterV1());
    assert(result); return result;
}
static void set(xvalue* list,int64_t key)
{ XL6_ListLayoutV1 t=tuple(); xstrview text={"a\0\xE4\xBD\xA0",5}; assert(xl6ListSetV1(list,&t,key,&text)); }
static xvalue* query(void) { return xrtValueString((xstrview){"a\0\xE4\xBD\xA0",5}); }
static void checkError(xerrkind kind)
{ assert(xrtGetError() && xrtErrorKind(xrtGetError())==kind); xrtClearError(); }
static int scopeCountdown,scopeSeen;
extern bool __real_xl6ErrorScopeBegin(xl6_error_scope*);
bool __wrap_xl6ErrorScopeBegin(xl6_error_scope* scope)
{
    if (scopeCountdown && ++scopeSeen==scopeCountdown) { xrtClearError(); return false; }
    return __real_xl6ErrorScopeBegin(scope);
}

static unsigned providerCountdown,providerSeen;
static bool providerFails(void)
{
    if (!providerCountdown || ++providerSeen!=providerCountdown) return false;
    xrtClearError(); return true;
}
extern xvalue* __real_xl6ListErasedCloneV1(const xvalue*);
xvalue* __wrap_xl6ListErasedCloneV1(const xvalue* source)
{ return providerFails() ? NULL : __real_xl6ListErasedCloneV1(source); }
extern xvalue* __real_xl6ValueCloneV1(const xvalue*);
xvalue* __wrap_xl6ValueCloneV1(const xvalue* source)
{ return providerFails() ? NULL : __real_xl6ValueCloneV1(source); }
extern xvalue* __real_xl6ValueDeepCloneNativeV1(const xvalue*,const XL6_ValueGraphPlanV1*);
xvalue* __wrap_xl6ValueDeepCloneNativeV1(const xvalue* source,const XL6_ValueGraphPlanV1* plan)
{ return providerFails() ? NULL : __real_xl6ValueDeepCloneNativeV1(source,plan); }
extern xvalue* __real_xrtValueGraphNextV1(xvaluegraphnextv1*,const xvalue*);
xvalue* __wrap_xrtValueGraphNextV1(xvaluegraphnextv1* next,const xvalue* source)
{ return providerFails() ? NULL : __real_xrtValueGraphNextV1(next,source); }

static bool sameOwner(const xvalue* a,const xvalue* b)
{
    ptr ar=NULL,br=NULL,au=NULL,bu=NULL; const xvaluehandleops* ao=NULL,*bo=NULL;
    assert(xrtValueGetHandle(a,&ar,&ao,&au) && xrtValueGetHandle(b,&br,&bo,&bu));
    return ar==br && ao==bo && au==bu;
}
static void textAt(const xvalue* source,int64_t key)
{
    bool found=false; xvalue* value=NULL; xstrview text={0};
    assert(xl6ListErasedReadV1(source,key,false,&value,&found) && found && value &&
        xrtValueGetString(value,&text) && text.Size==5 && !memcmp(text.Data,"a\0\xE4\xBD\xA0",5));
    xrtValueRelease(value);
}
static xvalue* childAt(const xvalue* source,int64_t key)
{
    xvalue* value=NULL; bool found=false;
    if (xrtValueType(source)==XVALUE_ARRAY) return xrtValueRetain(xrtValueArrayAt(source,(size_t)key));
    assert(xl6ListErasedReadV1(source,key,false,&value,&found) && found && value); return value;
}
static xvalue* copyInput(unsigned operation)
{
    xvalue* child=create(true); set(child,INT64_MIN); set(child,INT64_MAX);
    if (operation<2) return child;
    xvalue* source=NULL;
    if (operation==4) {
        source=xrtValueArray(); assert(source && xrtValueArrayAppend(source,child) && xrtValueArrayAppend(source,child));
    } else {
        XL6_ListLayoutV1 t=tuple(); t.identity+=0x100; t.value.identity+=0x100; t.value.elementIdentity+=0x100;
        t.value.transfer=3; t.value.type=xrtTypeValue(); t.value.size=sizeof(xvalue*);
        source=profile(&t,XL6_VALUE_ADMIT_ANY,false,0,NULL);
        assert(xl6ListSetV1(source,&t,INT64_MIN,&child) && xl6ListSetV1(source,&t,INT64_MAX,&child));
    }
    xrtValueRelease(child); return source;
}
static xvalue* copyOperation(unsigned operation,const xvalue* source)
{ return operation==0 || operation==2 ? xl6ValueCopyV1(source) : xl6ValueCloneGraphV1(source); }
static void copyObserved(unsigned operation,xvalue* source,xvalue* result)
{
    assert(result && result!=source && xrtValueTypeId(result)==xrtValueTypeId(source));
    if (operation<2) {
        assert(!sameOwner(source,result)); textAt(result,INT64_MIN); textAt(result,INT64_MAX);
        assert(xl6ListErasedClearV1(result)); textAt(source,INT64_MIN); textAt(source,INT64_MAX);
    } else {
        int64_t first=operation==4 ? 0 : INT64_MIN,second=operation==4 ? 1 : INT64_MAX;
        xvalue* original=childAt(source,first),*a=childAt(result,first),*b=childAt(result,second);
        assert(sameOwner(a,b) && sameOwner(original,a)==(operation==2));
        textAt(a,INT64_MIN); textAt(a,INT64_MAX);
        if (operation!=2) { assert(xl6ListErasedClearV1(a)); textAt(original,INT64_MIN); textAt(original,INT64_MAX); }
        else { assert(xl6ListErasedClearV1(result)); textAt(original,INT64_MIN); textAt(original,INT64_MAX); }
        xrtValueRelease(b); xrtValueRelease(a); xrtValueRelease(original);
    }
}
static bool copyAttempt(unsigned operation,size_t point,unsigned mode)
{
    xl6ProductMemoryBegin(SIZE_MAX); xvalue* source=copyInput(operation);
    xl6_error_scope boundary={0}; assert(xl6ErrorScopeBegin(&boundary));
    if (!mode) assert(xrtMemDebugFailAfter(point));
    else if (mode==1) { scopeSeen=0; scopeCountdown=(int)point+1; }
    else if (mode==2) { providerSeen=0; providerCountdown=(unsigned)point+1; }
    xvalue* result=copyOperation(operation,source);
    bool hit=!mode ? xrtMemDebugFailTriggered() : mode==1 ? scopeSeen>=scopeCountdown :
        mode==2 ? providerSeen>=providerCountdown : false;
    scopeCountdown=0; providerCountdown=0; xrtMemDebugFailClear();
    if (!result) xl6ErrorScopeCaptureFailure(&boundary);
    xl6_error_result outcome=xl6ErrorScopeEnd(&boundary);
    if ((result!=NULL)!=!hit || outcome.ok!=!hit)
        fprintf(stderr,"value copy operation=%u point=%zu hit=%d result=%p ok=%d\n",operation,point,hit,(void*)result,outcome.ok);
    assert((result!=NULL)==!hit && outcome.ok==!hit); xrtErrorFree(outcome.error);
    if (result) copyObserved(operation,source,result);
    xrtValueRelease(result); xrtValueRelease(source); xrtClearError();
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    printf("Value copy operation=%u point=%zu hit=%d balanced\n",operation,point,hit); return hit;
}
static void extraVectors(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    xvalue* source=create(true); set(source,INT64_MIN);
    xrtSetErrorKind(XERR_TIMEOUT); const xerror* incoming=xrtGetError();
    xvalue* copy=xl6ValueCopyV1(source); assert(copy && xrtGetError()==incoming); xrtClearError(); xrtValueRelease(copy);
    xrtSetErrorKind(XERR_TIMEOUT); incoming=xrtGetError();
    copy=xl6ValueCloneGraphV1(source); assert(copy && xrtGetError()==incoming); xrtClearError(); xrtValueRelease(copy);
    xvalue* unbound=create(false); assert(!xl6ValueCopyV1(unbound)); checkError(XERR_STATE);
    assert(!xl6ValueCloneGraphV1(unbound)); checkError(XERR_STATE); xrtValueRelease(unbound);
    XL6_ListLayoutV1 t=tuple(); ptr raw=NULL,user=NULL; const xvaluehandleops* ops=NULL;
    assert(xrtValueGetHandle(source,&raw,&ops,&user)); XL6_ListGraphViewV1 view={0};
    assert(xl6ListGraphBeginV1(ops,raw,user,t.identity,&view)==1);
    assert(!xl6ValueCopyV1(source)); checkError(XERR_STATE);
    assert(!xl6ValueCloneGraphV1(source)); checkError(XERR_STATE); xl6ListGraphEndV1(&view);
    xrtValueRelease(source);
    /* Real typed Array fallback, including the former abort-on-scope-failure. */
    XL6_SequenceLayoutV1 layout={0xD720,0xD721,2,sizeof(xstrview),xl6StringTypeV1()};
    source=xl6SequencePrimitiveCreateV1(1,layout.identity,layout.elementIdentity,layout.transfer,
        layout.size,layout.type,XL6_VALUE_ADMIT_STRING,false,layout.elementIdentity); assert(source);
    source=xl6ArrayProjectionTakeV1(source,layout.identity,layout.elementIdentity,layout.transfer,
        layout.size,layout.type,xl6StringConverterV1()); assert(source);
    xvalue* text=query(); assert(text && xl6ArrayErasedPushV1(source,text)); xrtValueRelease(text);
    copy=xl6ValueCopyV1(source); assert(copy && !sameOwner(source,copy) && xl6ArrayErasedClearV1(copy));
    assert(xl6ArrayErasedSizeV1(source)==1); xrtValueRelease(copy);
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope)); scopeCountdown=1; scopeSeen=0;
    assert(!xl6ValueCloneV1(source)); scopeCountdown=0;
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope); assert(!outcome.ok && !outcome.error);
    xrtValueRelease(source); assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
}
#ifndef XL6_VALUE_COPY_NO_MAIN
int main(int argc,char** argv)
{
    assert(argc==2); unsigned mode=!strcmp(argv[1],"scopes") ? 1 : !strcmp(argv[1],"providers") ? 2 :
        !strcmp(argv[1],"vectors") ? 3 : 0; assert(mode || !strcmp(argv[1],"oom"));
    if (mode==3) extraVectors();
    for (unsigned operation=0;operation<5;++operation) {
        if (mode==3) { assert(!copyAttempt(operation,0,mode)); continue; }
        bool closed=false;
        for (size_t point=0;point<4096;++point) if (!copyAttempt(operation,point,mode)) {
            printf("Value copy closed-batch operation=%u runs=%zu first-no-hit=%zu\n",operation,point+1,point);
            closed=true; break;
        }
        assert(closed);
    }
    puts("Native/open copy and shared List clone memo: balanced"); return 0;
}
#endif
