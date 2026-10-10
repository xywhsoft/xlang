#include "src/runtime/xl6_list.h"
#include "src/runtime/xl6_list_methods.h"
#include "src/runtime/xl6_list_graph_access.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static const XL6_ListLayoutV1 layout={0xD710,{0xD711,0xD712,2,sizeof(xstrview),NULL}};
static XL6_ListLayoutV1 tuple(void)
{ XL6_ListLayoutV1 result=layout; result.value.type=xl6StringTypeV1(); return result; }
static xvalue* create(bool project)
{
    XL6_ListLayoutV1 t=tuple();
    xvalue* witness=xl6SequencePrimitiveCreateV1(1,t.value.identity,t.value.elementIdentity,
        t.value.transfer,t.value.size,t.value.type,XL6_VALUE_ADMIT_STRING,false,t.value.elementIdentity);
    assert(witness);
    xvalue* result=xl6ListCreateTakeV1(&t,witness); assert(result);
    if (project) result=xl6ListProjectionTakeV1(result,&t,xl6StringConverterV1());
    assert(result); return result;
}
static void set(xvalue* value,int64_t key)
{ XL6_ListLayoutV1 t=tuple(); xstrview text={"a\0\xE4\xBD\xA0",5}; assert(xl6ListSetV1(value,&t,key,&text)); }
static xvalue* query(void)
{ return xrtValueString((xstrview){"a\0\xE4\xBD\xA0",5}); }
static int scopeCountdown,scopeSeen;
extern bool __real_xl6ErrorScopeBegin(xl6_error_scope*);
bool __wrap_xl6ErrorScopeBegin(xl6_error_scope* scope)
{
    if (scopeCountdown && ++scopeSeen==scopeCountdown) { xrtClearError(); return false; }
    return __real_xl6ErrorScopeBegin(scope);
}
static void checkError(xerrkind kind)
{ assert(xrtGetError() && xrtErrorKind(xrtGetError())==kind); xrtClearError(); }
static void vectors(void)
{
    XL6_ListLayoutV1 t=tuple(); xvalue* list=create(true),*needle=query(); assert(needle);
    int64_t count=99; set(list,INT64_MIN); set(list,INT64_MAX);
    assert(xl6ListErasedProbeV1(list)==1);
    assert(xl6ListValueCountV1(list)==2);
    assert(xl6ListValueContainsV1(list,needle));
    xvalue* other=xrtValueInt(1); assert(other);
    assert(!xl6ListValueContainsV1(list,other) && !xrtGetError()); xrtValueRelease(other);
    xrtSetErrorKind(XERR_ARGUMENT); const xerror* prior=xrtGetError(); assert(prior);
    assert(xl6ListValueCountIfPresentV1(list,&count) && count==2 && xrtGetError()==prior);
    assert(xl6ListValueContainsV1(list,needle) && xrtGetError()==prior);
    xrtClearError(); count=99;
    assert(!xl6ListValueCountIfPresentV1(NULL,&count) && count==99 && !xrtGetError());
    assert(!xl6ListValueCountIfPresentV1(list,NULL)); checkError(XERR_ARGUMENT);
    xvalue* result=xl6ListValueCountOrNullV1(list); assert(result && xrtValueGetInt(result,&count) && count==2);
    xrtValueRelease(result); result=xl6ListValueCountOrNullV1(NULL);
    assert(result && xrtValueType(result)==XVALUE_NULL); xrtValueRelease(result);
    result=xl6ListValueTakeV1(list,17); assert(result && xrtValueType(result)==XVALUE_NULL);
    xrtValueRelease(result); assert(!xl6ListValueRemoveV1(list,17) && !xrtGetError());
    assert(xl6ListValueRemoveV1(list,INT64_MAX) && xl6ListValueCountV1(list)==1);
    result=xl6ListValueTakeV1(list,INT64_MIN); xstrview text={0};
    assert(result && xrtValueGetString(result,&text) && text.Size==5 && !memcmp(text.Data,"a\0\xE4\xBD\xA0",5));
    assert(xrtValueTypeId(result)==t.value.elementIdentity && xl6ListValueCountV1(list)==0);
    xrtValueRelease(result);
    assert(xl6ListValueTrimV1(list,-1)==-1); checkError(XERR_RANGE);
    assert(xl6ListValueClearV1(list) && xl6ListValueTrimV1(list,0)>=0);
    xrtValueRelease(list); xrtValueRelease(needle);
    /* Real legacy carrier still serves existing source producers this phase. */
    list=xrtValueIntMap(); needle=query(); assert(list && needle);
    assert(xrtValueIntMapSet(list,INT64_MIN,needle));
    assert(xl6ListValueCountV1(list)==1 && xl6ListValueContainsV1(list,needle));
    result=xl6ListValueTakeV1(list,INT64_MIN); assert(result && xrtValueEqual(result,needle));
    xrtValueRelease(result); assert(xl6ListValueCountV1(list)==0);
    assert(xl6ListValueClearV1(list) && xl6ListValueTrimV1(list,0)>=0);
    xrtValueRelease(list); xrtValueRelease(needle);
}
static bool foreignRetain(ptr value,ptr* output,ptr user)
{ (void)user; *output=value; return true; }
static void foreignDrop(ptr value,ptr user) { (void)value; (void)user; }
static const xvaluehandleops foreignOps={foreignRetain,foreignDrop,NULL,NULL};
static void rejected(void)
{
    XL6_ListLayoutV1 t=tuple(); xvalue* list=create(false),*needle=query();
    assert(xl6ListErasedProbeV1(list)==1 && !xl6ListValueContainsV1(list,needle)); checkError(XERR_STATE);
    xrtValueRelease(list); list=create(true);
    xvalue* cursor=xl6ListCursorCreateV1(list,&t,0xD713,false,false,0); assert(cursor);
    assert(xl6ListValueCountV1(cursor)==0); checkError(XERR_TYPE);
    xrtValueRelease(cursor);
    void* raw=NULL; void* context=NULL; const xvaluehandleops* ops=NULL;
    assert(xrtValueGetHandle(list,&raw,&ops,&context));
    XL6_ListGraphViewV1 view={0};
    assert(xl6ListGraphBeginV1(ops,raw,context,t.identity,&view)==1);
    int64_t count=99,trimmed=99; bool found=true;
    assert(!xl6ListValueCountIfPresentV1(list,&count) && count==99); checkError(XERR_STATE);
    assert(!xl6ListErasedTrimV1(list,0,&trimmed) && trimmed==99); checkError(XERR_STATE);
    assert(!xl6ListErasedContainsV1(list,needle,&found) && found); checkError(XERR_STATE);
    xl6ListGraphEndV1(&view); xrtValueRelease(list);
    void* pointer=(void*)1; list=xrtValueHandleTake(&pointer,&foreignOps,NULL); assert(list);
    assert(xrtValueTypeIdBind(list,t.identity) && xl6ListErasedProbeV1(list)==0);
    assert(xl6ListValueCountV1(list)==0); checkError(XERR_TYPE);
    assert(!xl6ListValueRemoveV1(list,0)); checkError(XERR_TYPE);
    assert(!xl6ListValueClearV1(list)); checkError(XERR_TYPE);
    assert(xl6ListValueTrimV1(list,0)==-1); checkError(XERR_TYPE);
    assert(!xl6ListValueTakeV1(list,0)); checkError(XERR_TYPE);
    assert(!xl6ListValueContainsV1(list,needle)); checkError(XERR_TYPE);
    xrtValueRelease(list); xrtValueRelease(needle);
}
static xvalue* profile(XL6_ListLayoutV1* t,unsigned rule,bool nullable,uint64_t base,
    const xvalueconverter* converter)
{
    xvalue* witness=t->value.transfer==3 ? xl6SequenceReferenceCreateV1(1,t->value.identity,
        t->value.elementIdentity,rule,nullable,base) : xl6SequencePrimitiveCreateV1(1,t->value.identity,
        t->value.elementIdentity,t->value.transfer,t->value.size,t->value.type,rule,nullable,base);
    assert(witness);
    xvalue* result=xl6ListProjectionTakeV1(xl6ListCreateTakeV1(t,witness),t,converter);
    assert(result); return result;
}
static void profiles(void)
{
    XL6_ListLayoutV1 t=tuple(); t.value.transfer=1; t.value.type=xrtTypeUInt64(); t.value.size=sizeof(uint64_t);
    xvalue* list=profile(&t,XL6_VALUE_ADMIT_U64,false,t.value.elementIdentity,NULL);
    uint64_t integer=UINT64_MAX; assert(xl6ListSetV1(list,&t,INT64_MAX,&integer));
    xvalue* value=xrtValueUInt(integer); assert(value && xl6ListValueContainsV1(list,value));
    xrtValueRelease(value); xrtValueRelease(list);
    t.value.type=xl6CharTypeV1(); t.value.size=sizeof(uint32_t);
    list=profile(&t,XL6_VALUE_ADMIT_CHAR,false,t.value.elementIdentity,xl6CharConverterV1());
    uint32_t character=0x4F60; assert(xl6ListSetV1(list,&t,-7,&character));
    value=xrtValueChar(character); assert(value && xl6ListValueContainsV1(list,value));
    xrtValueRelease(value); xrtValueRelease(list);
    t=tuple(); t.value.type=xl6NullableStringTypeV1(); t.value.elementIdentity++;
    list=profile(&t,XL6_VALUE_ADMIT_STRING,true,t.value.elementIdentity-1,xl6NullableStringConverterV1());
    xstrview absent={0},empty={"",0}; assert(xl6ListSetV1(list,&t,-7,&absent));
    value=xrtValueString(empty); assert(value);
    assert(xl6ListValueContainsV1(list,xrtValueNull()) && !xl6ListValueContainsV1(list,value));
    assert(xl6ListSetV1(list,&t,7,&empty) && xl6ListValueContainsV1(list,value));
    xrtValueRelease(value); xrtValueRelease(list);
    t=tuple(); t.value.transfer=3; t.value.type=xrtTypeValue(); t.value.size=sizeof(xvalue*);
    list=profile(&t,XL6_VALUE_ADMIT_BYTES,false,t.value.elementIdentity,NULL);
    const unsigned char binary[]={0,255,0,1}; value=xrtValueBytes((xbytesview){binary,sizeof(binary)}); assert(value);
    assert(xl6ListSetV1(list,&t,INT64_MIN,&value) && xl6ListValueContainsV1(list,value));
    xrtValueRelease(value); xrtValueRelease(list);
}
static bool operation(const char* name,xvalue* list,const xvalue* needle,xvalue** result)
{
    int64_t count=99;
    if (!strcmp(name,"count")) return xl6ListValueCountV1(list)==2;
    if (!strcmp(name,"nullable-count")) return xl6ListValueCountIfPresentV1(list,&count) && count==2;
    if (!strcmp(name,"boxed-count")) { *result=xl6ListValueCountOrNullV1(list); return *result!=NULL; }
    if (!strcmp(name,"contains")) return xl6ListValueContainsV1(list,needle);
    if (!strcmp(name,"take")) { *result=xl6ListValueTakeV1(list,INT64_MIN); return *result!=NULL; }
    if (!strcmp(name,"remove")) return xl6ListValueRemoveV1(list,INT64_MIN);
    if (!strcmp(name,"clear")) return xl6ListValueClearV1(list);
    if (!strcmp(name,"trim")) return xl6ListValueTrimV1(list,0)>=0;
    assert(0); return false;
}
static bool attempt(const char* name,size_t point,bool scopes)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    xvalue* list=create(true),*needle=query(),*result=NULL; assert(needle);
    set(list,INT64_MIN); set(list,INT64_MAX);
    xl6_error_scope outer={0}; assert(xl6ErrorScopeBegin(&outer));
    if (scopes) { scopeSeen=0; scopeCountdown=(int)point+1; }
    else assert(xrtMemDebugFailAfter(point));
    bool success=operation(name,list,needle,&result);
    bool hit=scopes ? scopeSeen>=scopeCountdown : xrtMemDebugFailTriggered();
    scopeCountdown=0; xrtMemDebugFailClear();
    xl6_error_result outcome=xl6ErrorScopeEnd(&outer);
    assert(hit ? !success && !outcome.ok && !result : success && outcome.ok);
    if (hit && (!strcmp(name,"take") || !strcmp(name,"contains") || !strcmp(name,"boxed-count")))
        assert(xl6ListValueCountV1(list)==2);
    xrtErrorFree(outcome.error); xrtValueRelease(result); xrtValueRelease(needle); xrtValueRelease(list);
    xrtClearError(); assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    printf("List method %s %s point=%zu hit=%d balanced\n",name,scopes ? "scope" : "allocation",point,hit);
    return hit;
}
int main(int argc,char** argv)
{
    assert(argc==2 || argc==3);
    if (argc==3) {
        bool scopes=!strcmp(argv[2],"scopes"); assert(scopes || !strcmp(argv[2],"oom"));
        for (size_t point=0;point<4096;++point) {
            if (!attempt(argv[1],point,scopes)) {
                printf("List method closed-batch runs=%zu first-no-hit=%zu\n",point+1,point); return 0;
            }
        }
        assert(!"Unclosed List method failure prefix");
    }
    xl6ProductMemoryBegin(SIZE_MAX);
    if (!strcmp(argv[1],"vectors")) vectors();
    else if (!strcmp(argv[1],"profiles")) profiles();
    else if (!strcmp(argv[1],"rejected")) rejected(); else assert(0);
    assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    puts("List method vectors balanced"); return 0;
}
