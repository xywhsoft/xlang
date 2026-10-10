#include "src/runtime/xl6_list.h"
#include "src/runtime/xl6_list_value_iterator.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static const int64_t keys[]={INT64_MIN,0,INT64_MAX};
static const xstrview text={"a\0\xE4\xBD\xA0",5};
static XL6_ListLayoutV1 layout(void)
{ return (XL6_ListLayoutV1){UINT64_MAX,{1,7,2,sizeof(xstrview),xl6StringTypeV1()}}; }
static xvalue* create(bool projected)
{
    XL6_ListLayoutV1 t=layout();
    xvalue* witness=xl6SequencePrimitiveCreateV1(1,t.value.identity,t.value.elementIdentity,
        t.value.transfer,t.value.size,t.value.type,XL6_VALUE_ADMIT_STRING,false,t.value.elementIdentity);
    assert(witness); xvalue* result=xl6ListCreateTakeV1(&t,witness); assert(result);
    if (projected) result=xl6ListProjectionTakeV1(result,&t,xl6StringConverterV1());
    assert(result);
    for (unsigned i=0;i<3;++i) assert(xl6ListSetV1(result,&t,keys[i],&text));
    return result;
}
static const XL6_ValueIteratorRuntimeV2* api(void) { return xl6ValueIteratorRuntimeV2Get(); }
static void check(XL6_ValueIterator* iterator,unsigned ordinal)
{
    const XL6_ValueIteratorView* view=api()->View(iterator); xstrview actual={0};
    assert(view && view->item && view->ordinal==ordinal && view->key.Type==XVALUE_KEY_INT &&
        view->key.Integer==keys[ordinal] && xrtValueGetString(view->item,&actual) &&
        actual.Size==text.Size && !memcmp(actual.Data,text.Data,text.Size) && xrtValueTypeId(view->item)==7);
}
static bool trace(xrtownershipref child,void* context)
{ assert(child.Data && child.Ops); ++*(unsigned*)context; return true; }
static void vectors(void)
{
    for (unsigned erased=0;erased<2;++erased) {
        xvalue* list=create(true);
        XL6_ValueIterator* iterator=erased ? xl6CollectionValueIteratorCreateV1(list) : xl6ListValueIteratorCreateV1(list);
        assert(iterator); XL6_ListLayoutV1 t=layout();
        assert(xl6ListClearV1(list,&t)); xrtValueRelease(list);
        xrtownershipref root=api()->Ownership(iterator); unsigned count=0;
        assert(xl6ValueIteratorOwnershipAdapterV1(root) && root.Ops->Trace(root.Data,trace,&count) && count==1);
        bool found=false;
        for (unsigned i=0;i<3;++i) {
            assert(api()->Advance(iterator,&found) && found); check(iterator,i);
            count=0; assert(root.Ops->Trace(root.Data,trace,&count) && count==2);
        }
        for (unsigned i=0;i<3;++i) assert(api()->Advance(iterator,&found) && !found && !api()->View(iterator)->item);
        api()->Destroy(iterator);
    }
    /* Selected adapter falls through to the genuine Array/wire providers. */
    xvalue* wire=xrtValueIntMap(); assert(wire && xrtValueIntMapSetNew(wire,INT64_MIN,xrtValueInt(9)));
    XL6_ValueIterator* iterator=xl6ListValueIteratorCreateV1(wire); assert(iterator); bool found=false;
    assert(api()->Advance(iterator,&found) && found && api()->View(iterator)->key.Integer==INT64_MIN);
    api()->Destroy(iterator); xrtValueRelease(wire);
    wire=xrtValueSet(); assert(wire && xrtValueSetAddNew(wire,xrtValueInt(9)));
    iterator=xl6SetValueIteratorCreateCheckedV1(wire); assert(iterator);
    assert(api()->Advance(iterator,&found) && found); api()->Destroy(iterator);
    assert(!xl6DictValueIteratorCreateCheckedV1(wire) && xrtGetError()); xrtClearError(); xrtValueRelease(wire);
    wire=xrtValueObject(); assert(wire && xrtValueObjectSetNew(wire,(xstrview){"a\0b",3},xrtValueInt(9)));
    iterator=xl6DictValueIteratorCreateCheckedV1(wire); assert(iterator);
    assert(api()->Advance(iterator,&found) && found && api()->View(iterator)->key.Type==XVALUE_KEY_STRING &&
        api()->View(iterator)->key.String.Size==3 && !memcmp(api()->View(iterator)->key.String.Data,"a\0b",3));
    api()->Destroy(iterator);
    assert(!xl6SetValueIteratorCreateCheckedV1(wire) && xrtGetError()); xrtClearError(); xrtValueRelease(wire);
    wire=xrtValueArray(); assert(wire && xrtValueArrayAppendNew(wire,xrtValueInt(9)));
    iterator=xl6CollectionValueIteratorCreateV1(wire); assert(iterator);
    assert(api()->Advance(iterator,&found) && found && api()->View(iterator)->key.Type==XVALUE_KEY_INDEX);
    api()->Destroy(iterator); xrtValueRelease(wire);
    xvalue* native=xl6SequencePrimitiveCreateV1(1,0xA1,0xA2,1,sizeof(int64_t),xrtTypeInt64(),XL6_VALUE_ADMIT_I64,false,0xA2);
    assert(native); int64_t n=9; assert(xl6ArrayPushV1(native,0xA1,0xA2,1,sizeof(n),xrtTypeInt64(),&n));
    native=xl6ArrayProjectionTakeV1(native,0xA1,0xA2,1,sizeof(n),xrtTypeInt64(),NULL); assert(native);
    assert(!xl6SetValueIteratorCreateCheckedV1(native) && xrtGetError()); xrtClearError();
    assert(!xl6DictValueIteratorCreateCheckedV1(native) && xrtGetError()); xrtClearError();
    iterator=xl6ArrayValueIteratorCreateCheckedV1(native); assert(iterator);
    assert(api()->Advance(iterator,&found) && found && api()->View(iterator)->key.Type==XVALUE_KEY_INDEX);
    api()->Destroy(iterator);
    iterator=xl6CollectionValueIteratorCreateV1(native); assert(iterator);
    assert(api()->Advance(iterator,&found) && found && api()->View(iterator)->key.Type==XVALUE_KEY_INDEX);
    api()->Destroy(iterator); xrtValueRelease(native);
}
static bool foreignRetain(ptr p,ptr* out,ptr user) { (void)user; *out=p; return true; }
static void foreignDrop(ptr p,ptr user) { (void)p; (void)user; }
static const xvaluehandleops foreignOps={foreignRetain,foreignDrop,NULL,NULL};
static void error(xerrkind kind)
{ assert(xrtGetError() && xrtErrorKind(xrtGetError())==kind); xrtClearError(); }
static void rejected(void)
{
    xvalue* list=create(false); assert(!xl6ListValueIteratorCreateV1(list)); error(XERR_STATE); xrtValueRelease(list);
    list=create(true); XL6_ListLayoutV1 t=layout();
    xvalue* cursor=xl6ListCursorCreateV1(list,&t,17,false,false,0); assert(cursor);
    assert(!xl6CollectionValueIteratorCreateV1(cursor)); error(XERR_TYPE); xrtValueRelease(cursor);
    xvalue* output=NULL; XL6_ListLayoutV1 unchanged={0},out=unchanged; uint64_t identity=99;
    assert(xl6ListIteratorSnapshotErasedV1(list,&output,&out,&identity)==1 && output && identity &&
        identity!=t.identity && identity!=t.value.identity && out.identity==t.identity);
    xrtValueRelease(output); xrtValueRelease(list);
    void* pointer=(void*)1; list=xrtValueHandleTake(&pointer,&foreignOps,NULL); assert(list);
    assert(xrtValueTypeIdBind(list,UINT64_MAX) && !xl6CollectionValueIteratorCreateV1(list)); error(XERR_TYPE);
    assert(!xl6ArrayValueIteratorCreateCheckedV1(list)); error(XERR_TYPE);
    assert(!xl6ListValueIteratorCreateV1(list)); error(XERR_TYPE); xrtValueRelease(list);
    assert(!xl6CollectionValueIteratorCreateV1(xrtValueNull())); error(XERR_TYPE);
    list=create(true); assert(!xl6ArrayValueIteratorCreateCheckedV1(list)); error(XERR_TYPE); xrtValueRelease(list);
    list=xrtValueIntMap(); assert(list && !xl6ArrayValueIteratorCreateCheckedV1(list)); error(XERR_TYPE); xrtValueRelease(list);
}
static int scopeCountdown,scopeSeen;
extern bool __real_xl6ErrorScopeBegin(xl6_error_scope*);
bool __wrap_xl6ErrorScopeBegin(xl6_error_scope* scope)
{
    if (scopeCountdown && ++scopeSeen==scopeCountdown) { xrtClearError(); return false; }
    return __real_xl6ErrorScopeBegin(scope);
}
static bool attempt(const char* operation,size_t point,bool scopes)
{
    xl6ProductMemoryBegin(SIZE_MAX); xvalue* list=create(true);
    bool constructing=!strcmp(operation,"create"),found=false;
    XL6_ValueIterator* iterator=constructing ? NULL : xl6ListValueIteratorCreateV1(list); assert(constructing || iterator);
    xl6_error_scope boundary={0}; assert(xl6ErrorScopeBegin(&boundary));
    if (scopes) { scopeSeen=0; scopeCountdown=(int)point+1; }
    else assert(xrtMemDebugFailAfter(point));
    bool success;
    if (constructing) { iterator=xl6ListValueIteratorCreateV1(list); success=iterator!=NULL; }
    else success=api()->Advance(iterator,&found);
    bool hit=scopes ? scopeSeen>=scopeCountdown : xrtMemDebugFailTriggered();
    scopeCountdown=0; xrtMemDebugFailClear();
    if (!success) xl6ErrorScopeCaptureFailure(&boundary);
    xl6_error_result result=xl6ErrorScopeEnd(&boundary);
    assert(success==!hit && result.ok==!hit);
    if (!constructing && success) { assert(found); check(iterator,0); }
    if (!constructing && !success) {
        bool retry=true; assert(!api()->Advance(iterator,&retry) && !retry); xrtClearError();
        assert(!api()->View(iterator)); xrtClearError();
    }
    xrtErrorFree(result.error); api()->Destroy(iterator); xrtValueRelease(list); xrtClearError();
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    printf("List iterator %s point=%zu hit=%d balanced\n",operation,point,hit); return hit;
}
int main(int argc,char** argv)
{
    assert(argc==2 || argc==3);
    if (argc==3) {
        bool scopes=!strcmp(argv[2],"scopes"); assert(scopes || !strcmp(argv[2],"oom"));
        for(size_t point=0;point<4096;++point) if (!attempt(argv[1],point,scopes)) {
            printf("List iterator closed-batch runs=%zu first-no-hit=%zu\n",point+1,point); return 0;
        }
        assert(!"Unclosed prefix");
    }
    xl6ProductMemoryBegin(SIZE_MAX);
    if (!strcmp(argv[1],"vectors")) vectors(); else { assert(!strcmp(argv[1],"rejected")); rejected(); }
    assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify(); puts("List iterator balanced");
}
