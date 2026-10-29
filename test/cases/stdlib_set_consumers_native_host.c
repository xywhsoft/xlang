#include "src/runtime/xl6_set.h"
#include "src/runtime/xl6_set_graph_access.h"
#include "src/runtime/xl6_list.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_value_copy.h"
#include "src/runtime/xl6_value_contains.h"
#include "src/runtime/xl6_list_value_iterator.h"
#include "src/runtime/xl6_set_value_iterator.h"
#include "src/runtime/xl6_persistence_project.h"
#include "src/runtime/xl6_dynamic_collection.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t scopePoint,scopeSeen;
static bool scopeArmed,cleanupRefused;
extern bool __real_xl6ErrorScopeBegin(xl6_error_scope*);
extern bool __real_xl6ErrorScopeBeginCleanup(xl6_error_scope*,bool);
bool __wrap_xl6ErrorScopeBegin(xl6_error_scope* scope)
{
    if (scopeArmed && scopeSeen++==scopePoint) { xrtClearError(); return false; }
    return __real_xl6ErrorScopeBegin(scope);
}
bool __wrap_xl6ErrorScopeBeginCleanup(xl6_error_scope* scope,bool failed)
{
    if (cleanupRefused) { cleanupRefused=false; xrtClearError(); return false; }
    return __real_xl6ErrorScopeBeginCleanup(scope,failed);
}
static XL6_SetLayoutV1 tuple(bool reference)
{
    const xrttype* type=reference ? xrtTypeValue() : xl6StringTypeV1();
    return (XL6_SetLayoutV1){0xF710,{0xF711,0xF712,reference ? 3u : 2u,type->Size,type}};
}
static xvalue* create(bool reference)
{
    XL6_SetLayoutV1 t=tuple(reference);
    xvalue* witness=reference ? xl6SequenceReferenceCreateV1(1,t.value.identity,t.value.elementIdentity,
        XL6_VALUE_ADMIT_ANY,true,t.value.elementIdentity) :
        xl6SequencePrimitiveCreateV1(1,t.value.identity,t.value.elementIdentity,2,t.value.size,t.value.type,
            XL6_VALUE_ADMIT_STRING,false,t.value.elementIdentity);
    assert(witness);
    xvalue* set=xl6SetProjectionTakeV1(xl6SetCreateTakeV1(&t,witness),&t,
        reference ? NULL : xl6StringConverterV1()); assert(set); return set;
}
static void add(xvalue* set,const xvalue* item)
{ bool added=false; assert(xl6SetErasedKeyV1(set,item,XL6_SET_ADD_V1,&added) && added); }
static xvalue* text(void)
{ return xrtValueString((xstrview){"a\0\xE4\xBD\xA0",5}); }
static xvalue* input(bool reference)
{
    xvalue* set=create(reference);
    xvalue* a=reference ? xrtValueObject() : text();
    xvalue* b=reference ? xrtValueObject() : xrtValueString(XRT_STR_LITERAL("second"));
    assert(a && b); add(set,a); add(set,b); xrtValueRelease(a); xrtValueRelease(b); return set;
}
static xvalue* mixed(void)
{
    xvalue* set=input(true);
    const xrttype* type=xrtTypeValue();
    XL6_ListLayoutV1 t={0xF720,{0xF721,0xF722,3,type->Size,type}};
    xvalue* witness=xl6SequenceReferenceCreateV1(1,t.value.identity,t.value.elementIdentity,
        XL6_VALUE_ADMIT_ANY,true,t.value.elementIdentity); assert(witness);
    xvalue* list=xl6ListProjectionTakeV1(xl6ListCreateTakeV1(&t,witness),&t,NULL); assert(list);
    assert(xl6ListErasedSetV1(list,-9,set));
    xvalue* root=xrtValueArray(); assert(root && xrtValueArrayAppend(root,set) &&
        xrtValueArrayAppend(root,list) && xrtValueArrayAppend(root,set));
    xrtValueRelease(list); xrtValueRelease(set); return root;
}
static void graphObserved(const xvalue* source,const xvalue* result,bool data)
{
    const xvalue* first=xrtValueArrayAt(result,0);
    assert(first && first==xrtValueArrayAt(result,2) && first!=xrtValueArrayAt(source,0));
    if (data) {
        assert(xrtValueType(first)==XVALUE_ARRAY && xrtValueCount(first)==2);
        assert(xrtValueArrayAt(xrtValueArrayAt(result,1),0)==first);
    } else {
        assert(xl6SetErasedProbeV1(first)==1); int64_t count=0;
        assert(xl6SetErasedCountV1(first,false,&count) && count==2);
        xvalue* child=NULL; bool found=false;
        assert(xl6ListErasedReadV1(xrtValueArrayAt(result,1),-9,false,&child,&found) && found && child==first);
        xrtValueRelease(child);
        const XL6_ValueIteratorRuntimeV2* api=xl6ValueIteratorRuntimeV2Get();
        XL6_ValueIterator* iter=xl6SetValueIteratorCreateCheckedV1(first); assert(iter);
        const xvalue* prior=NULL;
        for (unsigned i=0;i<2;++i) {
            assert(api->Advance(iter,&found) && found); const XL6_ValueIteratorView* view=api->View(iter);
            assert(view && view->item && view->item!=prior && view->key.Type==0);
            /* Distinct equal-content objects stay distinct identity keys. */
            assert(xrtValueType(view->item)==XVALUE_OBJECT); prior=view->item;
        }
        assert(api->Advance(iter,&found) && !found); api->Destroy(iter);
    }
}
static void iteratorObserved(XL6_ValueIterator* iter)
{
    const XL6_ValueIteratorRuntimeV2* api=xl6ValueIteratorRuntimeV2Get(); bool found=false;
    for (unsigned i=0;i<2;++i) {
        assert(api->Advance(iter,&found) && found); const XL6_ValueIteratorView* view=api->View(iter);
        assert(view && view->ordinal==i && view->key.Type==0);
        xstrview item={0}; assert(xrtValueGetString(view->item,&item));
        assert(item.Size==(i ? 6 : 5));
        if (!i) assert(!memcmp(item.Data,"a\0\xE4\xBD\xA0",5));
    }
    assert(api->Advance(iter,&found) && !found && api->Advance(iter,&found) && !found);
}
static void vectors(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    xvalue* source=input(false),*query=text(); bool found=false;
    xvalue* copy=xl6ValueCopyV1(source); assert(copy && copy!=source);
    assert(xl6SetErasedRelationV1(source,copy,XL6_SET_EQUALS_V1,false,&found) && found);
    assert(xl6SetErasedClearV1(copy) && xl6ValueContainsV1(source,query)); xrtValueRelease(copy);
    const XL6_ValueIteratorRuntimeV2* api=xl6ValueIteratorRuntimeV2Get();
    for (unsigned erased=0;erased<2;++erased) {
        XL6_ValueIterator* iter=erased ? xl6CollectionValueIteratorCreateV1(source) :
            xl6SetValueIteratorCreateCheckedV1(source); assert(iter);
        iteratorObserved(iter); api->Destroy(iter);
    }
    XL6_ValueIterator* iter=xl6SetValueIteratorCreateCheckedV1(source); assert(iter);
    assert(xl6SetErasedClearV1(source)); iteratorObserved(iter); api->Destroy(iter);
    assert(!xl6ListValueIteratorCreateV1(source)); xrtClearError();
    assert(!xl6ArrayValueIteratorCreateCheckedV1(source)); xrtClearError();
    ptr raw=NULL,user=NULL; const xvaluehandleops* ops=NULL; XL6_SetGraphViewV1 pin={0};
    assert(xrtValueGetHandle(source,&raw,&ops,&user) &&
        xl6SetGraphBeginV1(ops,raw,user,xrtValueTypeId(source),&pin)==1);
    assert(!xl6ValueCopyV1(source)); xrtClearError();
    assert(!xl6ValueCloneCollectionsV1(source)); xrtClearError();
    assert(!xl6CollectionValueIteratorCreateV1(source)); xrtClearError();
    assert(!xl6ValueContainsV1(source,query)); xrtClearError(); xl6SetGraphEndV1(&pin);
    /* An unauthenticated payload is never cast, even with a copied identity. */
    const xvaluehandleops foreign={0};
    assert(xl6SetGraphBeginV1(&foreign,(ptr)(uintptr_t)1,NULL,0xF710,&pin)==0 && !pin.owner);
    iter=xl6SetValueIteratorCreateCheckedV1(source); assert(iter);
    scopeArmed=true; scopeSeen=scopePoint=0;
    assert(!api->Advance(iter,&found)); scopeArmed=false; xrtClearError();
    assert(!api->Advance(iter,&found)); xrtClearError(); api->Destroy(iter);
    xvalue* result=xl6DynamicCollectionCallTakeV1(source,XL6_DYNAMIC_COLLECTION_ADD,xrtValueRetain(query),NULL);
    assert(result && xrtValueTruthy(result)); xrtValueRelease(result);
    result=xl6DynamicCollectionCallTakeV1(source,XL6_DYNAMIC_COLLECTION_CONTAINS,xrtValueRetain(query),NULL);
    assert(result && xrtValueTruthy(result)); xrtValueRelease(result);
    result=xl6DynamicCollectionCallTakeV1(source,XL6_DYNAMIC_COLLECTION_RESERVE,xrtValueInt(40),NULL);
    assert(result && xrtValueTruthy(result)); xrtValueRelease(result);
    result=xl6DynamicCollectionCallTakeV1(source,XL6_DYNAMIC_COLLECTION_TAKE,xrtValueRetain(query),NULL);
    xstrview item={0}; assert(result && xrtValueGetString(result,&item) && item.Size==5); xrtValueRelease(result);
    result=xl6DynamicCollectionCallTakeV1(source,XL6_DYNAMIC_COLLECTION_TAKE,xrtValueRetain(query),NULL);
    assert(result && xrtValueType(result)==XVALUE_NULL); xrtValueRelease(result);
    xvalue* owned=text(); scopeArmed=true; scopeSeen=scopePoint=0; cleanupRefused=true;
    xl6_error_scope outer={0}; scopeArmed=false; assert(xl6ErrorScopeBegin(&outer)); scopeArmed=true;
    assert(!xl6DynamicCollectionCallTakeV1(source,XL6_DYNAMIC_COLLECTION_ADD,owned,NULL));
    scopeArmed=false; assert(!cleanupRefused); xl6_error_result failure=xl6ErrorScopeEnd(&outer);
    assert(!failure.ok); xrtErrorFree(failure.error); xrtClearError();
    xrtValueRelease(query); xrtValueRelease(source);
    source=mixed(); copy=xl6ValueCloneCollectionsV1(source); assert(copy); graphObserved(source,copy,false); xrtValueRelease(copy);
    const XL6_PersistencePlanV1 plan={.size=sizeof(plan)};
    copy=xl6PersistenceProjectCollectionsV1(source,&plan); assert(copy); graphObserved(source,copy,true); xrtValueRelease(copy);
    xrtSetErrorKind(XERR_TIMEOUT); const xerror* prior=xrtGetError();
    copy=xl6ValueCloneCollectionsV1(source); assert(copy && xrtGetError()==prior); xrtClearError(); xrtValueRelease(copy);
    xrtValueRelease(source);
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
}
static bool attempt(unsigned lane,size_t point,bool scopes)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    xvalue* source=(lane==1 || lane==2) ? mixed() : input(false);
    xvalue* query=text(),*argument=xrtValueString(XRT_STR_LITERAL("third")); assert(query && argument);
    const XL6_PersistencePlanV1 plan={.size=sizeof(plan)};
    xl6_error_scope boundary={0}; assert(xl6ErrorScopeBegin(&boundary));
    scopeSeen=0; scopePoint=point; scopeArmed=scopes;
    if (!scopes) assert(xrtMemDebugFailAfter(point));
    xvalue* result=NULL; XL6_ValueIterator* iter=NULL; bool ok=false;
    switch (lane) {
    case 0: result=xl6ValueCopyV1(source); break;
    case 1: result=xl6ValueCloneCollectionsV1(source); break;
    case 2: result=xl6PersistenceProjectCollectionsV1(source,&plan); break;
    case 3: iter=xl6SetValueIteratorCreateCheckedV1(source); break;
    case 4: iter=xl6CollectionValueIteratorCreateV1(source); break;
    case 5: result=xl6DynamicCollectionCallTakeV1(source,XL6_DYNAMIC_COLLECTION_ADD,argument,NULL); argument=NULL; break;
    case 6: ok=xl6ValueContainsV1(source,query); break;
    case 7: result=xl6DynamicCollectionCallTakeV1(source,XL6_DYNAMIC_COLLECTION_TAKE,query,NULL); query=NULL; break;
    default: assert(false);
    }
    bool hit=scopes ? scopeSeen>point : xrtMemDebugFailTriggered();
    scopeArmed=false; xrtMemDebugFailClear();
    bool success=result || iter || ok;
    if (!success) xl6ErrorScopeCaptureFailure(&boundary);
    xl6_error_result outcome=xl6ErrorScopeEnd(&boundary);
    assert(success==!hit && outcome.ok==!hit); xrtErrorFree(outcome.error);
    if (success && (lane==1 || lane==2)) graphObserved(source,result,lane==2);
    if (iter) { iteratorObserved(iter); xl6ValueIteratorRuntimeV2Get()->Destroy(iter); }
    xrtValueRelease(result); xrtValueRelease(query); xrtValueRelease(argument); xrtValueRelease(source); xrtClearError();
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    printf("Set consumers lane=%u mode=%s point=%zu hit=%d balanced\n",lane,scopes ? "scopes" : "oom",point,hit);
    return hit;
}
int main(int argc,char** argv)
{
    assert(argc==2);
    if (!strcmp(argv[1],"vectors")) vectors();
    else {
        bool scopes=!strcmp(argv[1],"scopes"); assert(scopes || !strcmp(argv[1],"oom"));
        for (unsigned lane=0;lane<8;++lane) {
            size_t point=0; while(point<4096 && attempt(lane,point,scopes)) ++point;
            assert(point<4096); printf("Set consumers closed lane=%u mode=%s runs=%zu first-no-hit=%zu\n",
                lane,scopes ? "scopes" : "oom",point+1,point);
        }
    }
    puts("[PASS] native Set consumers"); return 0;
}
