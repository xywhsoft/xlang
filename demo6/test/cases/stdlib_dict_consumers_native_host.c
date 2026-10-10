#include "src/runtime/xl6_dict.h"
#include "src/runtime/xl6_dict_graph_access.h"
#include "src/runtime/xl6_dict_value_iterator.h"
#include "src/runtime/xl6_list_value_iterator.h"
#include "src/runtime/xl6_set.h"
#include "src/runtime/xl6_list.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_collection_count_all.h"
#include "src/runtime/xl6_value_copy.h"
#include "src/runtime/xl6_value_contains.h"
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
static const xstrview keys[]={{"",0},{"a\0\xE4\xBD\xA0",5},{"a",1}};
static XL6_DictLayoutV1 tuple(bool reference)
{
    const xrttype* type=reference ? xrtTypeValue() : xl6StringTypeV1();
    return (XL6_DictLayoutV1){0xF810,{0xF811,0xF812,reference ? 3u : 2u,type->Size,type}};
}
static xvalue* create(bool reference)
{
    XL6_DictLayoutV1 t=tuple(reference);
    xvalue* witness=reference ? xl6SequenceReferenceCreateV1(1,t.value.identity,t.value.elementIdentity,
        XL6_VALUE_ADMIT_ANY,true,t.value.elementIdentity) :
        xl6SequencePrimitiveCreateV1(1,t.value.identity,t.value.elementIdentity,2,t.value.size,t.value.type,
            XL6_VALUE_ADMIT_STRING,false,t.value.elementIdentity);
    assert(witness);
    xvalue* dict=xl6DictProjectionTakeV1(xl6DictCreateTakeV1(&t,witness),&t,
        reference ? NULL : xl6StringConverterV1()); assert(dict); return dict;
}
static xvalue* input(void)
{
    xvalue* dict=create(false); XL6_DictLayoutV1 t=tuple(false);
    for (size_t i=0;i<3;++i) assert(xl6DictSetV1(dict,&t,keys[i],&keys[i]));
    return dict;
}
static bool visit(xrtownershipref ref,void* context)
{ assert(ref.Data && ref.Ops); ++*(size_t*)context; return true; }
static void iteratorObserved(XL6_ValueIterator* iter,bool native)
{
    assert(iter);
    const XL6_ValueIteratorRuntimeV2* api=xl6ValueIteratorRuntimeV2Get(); bool found=false;
    xstrview escaped={0};
    for (size_t i=0;i<3;++i) {
        assert(api->Advance(iter,&found) && found); const XL6_ValueIteratorView* v=api->View(iter);
        assert(v && v->ordinal==i && v->key.Type==XVALUE_KEY_STRING && v->key.String.Data &&
            v->key.String.Size==keys[i].Size && !memcmp(v->key.String.Data,keys[i].Data,keys[i].Size));
        xstrview item={0}; assert(xrtValueGetString(v->item,&item) && item.Size==keys[i].Size &&
            !memcmp(item.Data,keys[i].Data,item.Size));
        if (i==1) assert(xrtTypeCopyValue(xl6StringTypeV1(),&escaped,&v->key.String));
        /* Native owns cursor + current item; wire owns one ValueCursor. */
        xrtownershipref owner=api->Ownership(iter); size_t edges=0;
        assert(owner.Ops->Trace(owner.Data,visit,&edges) && edges==(native ? 2u : 1u));
    }
    assert(api->Advance(iter,&found) && !found && api->Advance(iter,&found) && !found);
    assert(api->View(iter) && !api->View(iter)->item && !api->View(iter)->key.Type);
    api->Destroy(iter);
    assert(escaped.Size==5 && !memcmp(escaped.Data,keys[1].Data,5));
    xrtTypeDropValue(xl6StringTypeV1(),&escaped);
}
static xvalue* mixed(void)
{
    xvalue* dict=create(true),*child=xrtValueObject(); assert(child);
    assert(xrtValueObjectSetNew(child,XRT_STR_LITERAL("n"),xrtValueInt(17)));
    assert(xl6DictErasedSetV1(dict,keys[1],child) && xl6DictErasedSetV1(dict,keys[2],child));
    const xrttype* type=xrtTypeValue();
    XL6_ListLayoutV1 l={0xF820,{0xF821,0xF822,3,type->Size,type}};
    xvalue* witness=xl6SequenceReferenceCreateV1(1,l.value.identity,l.value.elementIdentity,
        XL6_VALUE_ADMIT_ANY,true,l.value.elementIdentity); assert(witness);
    xvalue* list=xl6ListProjectionTakeV1(xl6ListCreateTakeV1(&l,witness),&l,NULL); assert(list);
    assert(xl6ListErasedSetV1(list,-9,dict));
    XL6_SetLayoutV1 s={0xF830,{0xF831,0xF832,3,type->Size,type}};
    witness=xl6SequenceReferenceCreateV1(1,s.value.identity,s.value.elementIdentity,
        XL6_VALUE_ADMIT_ANY,true,s.value.elementIdentity); assert(witness);
    xvalue* set=xl6SetProjectionTakeV1(xl6SetCreateTakeV1(&s,witness),&s,NULL); assert(set);
    bool added=false; assert(xl6SetErasedKeyV1(set,dict,XL6_SET_ADD_V1,&added) && added);
    /* Mutating contents after insertion must not change Set's identity key. */
    assert(xrtValueObjectSetNew(child,XRT_STR_LITERAL("n"),xrtValueInt(18)));
    bool present=false; assert(xl6SetErasedKeyV1(set,dict,XL6_SET_HAS_V1,&present) && present);
    xvalue* root=xrtValueArray(); assert(root && xrtValueArrayAppend(root,dict) &&
        xrtValueArrayAppend(root,list) && xrtValueArrayAppend(root,set) && xrtValueArrayAppend(root,child));
    xrtValueRelease(child); xrtValueRelease(set); xrtValueRelease(list); xrtValueRelease(dict); return root;
}
static void graphObserved(const xvalue* source,const xvalue* copy,bool data)
{
    const xvalue* dict=xrtValueArrayAt(copy,0),*child=xrtValueArrayAt(copy,3);
    assert(dict && dict!=xrtValueArrayAt(source,0) && child!=xrtValueArrayAt(source,3));
    if (data) {
        assert(xrtValueType(dict)==XVALUE_OBJECT && xrtValueCount(dict)==2);
        assert(xrtValueObjectGet(dict,keys[1])==child && xrtValueObjectGet(dict,keys[2])==child);
        assert(xrtValueArrayAt(xrtValueArrayAt(copy,1),0)==dict);
        assert(xrtValueArrayAt(xrtValueArrayAt(copy,2),0)==dict);
    } else {
        assert(xl6DictErasedProbeV1(dict)==1 && xl6CollectionCountAllV1(dict)==2);
        xvalue* read=NULL; bool found=false;
        assert(xl6DictErasedReadV1(dict,keys[1],false,&read,&found) && found && read==child); xrtValueRelease(read); read=NULL;
        assert(xl6DictErasedReadV1(dict,keys[2],false,&read,&found) && found && read==child); xrtValueRelease(read); read=NULL;
        assert(xl6ListErasedReadV1(xrtValueArrayAt(copy,1),-9,false,&read,&found) && found && read==dict); xrtValueRelease(read);
        assert(xl6SetErasedKeyV1(xrtValueArrayAt(copy,2),dict,XL6_SET_HAS_V1,&found) && found);
    }
    assert(xl6DictErasedProbeV1(xrtValueArrayAt(source,0))==1);
}
static void vectors(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    xvalue* source=input(),*query=xrtValueString(keys[1]); assert(query);
    xvalue* copy=xl6ValueCopyV1(source); assert(copy && copy!=source);
    assert(xl6DictErasedClearV1(copy) && xl6CollectionCountAllV1(source)==3 && xl6ValueContainsV1(source,query));
    xrtValueRelease(copy);
    const XL6_ValueIteratorRuntimeV2* api=xl6ValueIteratorRuntimeV2Get();
    iteratorObserved(xl6DictValueIteratorCreateCheckedV1(source),true);
    iteratorObserved(xl6CollectionValueIteratorCreateV1(source),true);
    XL6_ValueIterator* iter=xl6DictValueIteratorCreateCheckedV1(source); assert(iter);
    assert(xl6DictErasedClearV1(source)); iteratorObserved(iter,true);
    /* Wrong key type is checked failure, not a normal missing lookup. */
    xvalue* wrong=xrtValueInt(3); assert(wrong && !xl6ValueContainsV1(source,wrong) && xrtGetError());
    xrtClearError(); xrtValueRelease(wrong);
    ptr raw=NULL,user=NULL; const xvaluehandleops* ops=NULL; XL6_DictGraphViewV1 pin={0};
    assert(xrtValueGetHandle(source,&raw,&ops,&user) && xl6DictGraphBeginV1(ops,raw,user,xrtValueTypeId(source),&pin)==1);
    assert(!xl6ValueCopyV1(source)); xrtClearError();
    assert(!xl6ValueCloneCollectionsV1(source)); xrtClearError();
    assert(!xl6CollectionValueIteratorCreateV1(source)); xrtClearError();
    assert(!xl6ValueContainsV1(source,query)); xrtClearError(); xl6DictGraphEndV1(&pin);
    const xvaluehandleops foreign={0};
    assert(xl6DictGraphBeginV1(&foreign,(ptr)(uintptr_t)1,NULL,0xF810,&pin)==0 && !pin.owner);
    iter=xl6DictValueIteratorCreateCheckedV1(source); assert(iter);
    bool found=true; scopeArmed=true; scopeSeen=scopePoint=0;
    assert(!api->Advance(iter,&found)); scopeArmed=false; xrtClearError();
    assert(!api->Advance(iter,&found)); xrtClearError(); api->Destroy(iter);
    assert(xl6DictErasedSetV1(source,keys[1],query));
    xvalue* result=xl6DynamicCollectionCallTakeV1(source,XL6_DYNAMIC_COLLECTION_RESERVE,xrtValueInt(40),NULL);
    assert(result && xrtValueTruthy(result)); xrtValueRelease(result);
    result=xl6DynamicCollectionCallTakeV1(source,XL6_DYNAMIC_COLLECTION_TAKE,xrtValueRetain(query),NULL);
    xstrview item={0}; assert(result && xrtValueGetString(result,&item) && item.Size==5); xrtValueRelease(result);
    result=xl6DynamicCollectionCallTakeV1(source,XL6_DYNAMIC_COLLECTION_TAKE,xrtValueRetain(query),NULL);
    assert(result && xrtValueType(result)==XVALUE_NULL); xrtValueRelease(result);
    result=xl6DynamicCollectionCallTakeV1(source,XL6_DYNAMIC_COLLECTION_REMOVE,xrtValueRetain(query),NULL);
    assert(result && !xrtValueTruthy(result) && !xrtGetError()); xrtValueRelease(result);
    assert(xl6DictErasedSetV1(source,keys[1],query));
    xl6_error_scope outer={0}; assert(xl6ErrorScopeBegin(&outer));
    scopeArmed=true; scopeSeen=scopePoint=0; cleanupRefused=true;
    assert(!xl6DynamicCollectionCallTakeV1(source,XL6_DYNAMIC_COLLECTION_TAKE,xrtValueRetain(query),NULL));
    scopeArmed=false; assert(!cleanupRefused); xl6_error_result failure=xl6ErrorScopeEnd(&outer);
    assert(!failure.ok); xrtErrorFree(failure.error); xrtClearError();
    xrtValueRelease(source);
    source=mixed(); copy=xl6ValueCloneCollectionsV1(source); assert(copy); graphObserved(source,copy,false); xrtValueRelease(copy);
    const XL6_PersistencePlanV1 plan={.size=sizeof(plan)};
    copy=xl6PersistenceProjectCollectionsV1(source,&plan); assert(copy); graphObserved(source,copy,true); xrtValueRelease(copy);
    xrtValueRelease(source);
    source=input(); copy=xl6ValueCloneCollectionsV1(source); assert(copy && xl6DictErasedProbeV1(copy)==1);
    iteratorObserved(xl6DictValueIteratorCreateCheckedV1(copy),true); xrtValueRelease(copy);
    copy=xl6PersistenceProjectCollectionsV1(source,&plan); assert(copy && xrtValueType(copy)==XVALUE_OBJECT);
    iteratorObserved(xl6DictValueIteratorCreateCheckedV1(copy),false); xrtValueRelease(copy);
    xrtSetErrorKind(XERR_TIMEOUT); const xerror* prior=xrtGetError();
    copy=xl6ValueCloneCollectionsV1(source); assert(copy && xrtGetError()==prior); xrtClearError(); xrtValueRelease(copy);
    xrtValueRelease(source);
    /* The shared graph copier rejects active ancestor cycles; no retaining
     * an active Value shell, recursive fallback or partial result publication. */
    source=create(true); assert(xl6DictErasedSetV1(source,keys[1],source));
    assert(!xl6ValueCloneCollectionsV1(source) && xrtGetError()); xrtClearError();
    assert(!xl6PersistenceProjectCollectionsV1(source,&plan) && xrtGetError()); xrtClearError();
    assert(xl6DictErasedClearV1(source)); xrtValueRelease(source); xrtValueRelease(query);
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
}
static bool attempt(unsigned lane,size_t point,bool scopes)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    xvalue* source=(lane==1 || lane==2) ? mixed() : input(),*query=xrtValueString(keys[1]); assert(query);
    const XL6_PersistencePlanV1 plan={.size=sizeof(plan)};
    xl6_error_scope boundary={0}; assert(xl6ErrorScopeBegin(&boundary));
    scopeSeen=0; scopePoint=point; scopeArmed=scopes;
    if (!scopes) assert(xrtMemDebugFailAfter(point));
    xvalue* result=NULL; XL6_ValueIterator* iter=NULL; bool ok=false;
    switch (lane) {
    case 0: result=xl6ValueCopyV1(source); break;
    case 1: result=xl6ValueCloneCollectionsV1(source); break;
    case 2: result=xl6PersistenceProjectCollectionsV1(source,&plan); break;
    case 3: iter=xl6DictValueIteratorCreateCheckedV1(source); break;
    case 4: iter=xl6CollectionValueIteratorCreateV1(source); break;
    case 5: result=xl6DynamicCollectionCallTakeV1(source,XL6_DYNAMIC_COLLECTION_TAKE,query,NULL); query=NULL; break;
    case 6: ok=xl6ValueContainsV1(source,query); break;
    case 7: result=xl6CollectionCountAllOrNullV1(source); break;
    default: assert(false);
    }
    bool hit=scopes ? scopeSeen>point : xrtMemDebugFailTriggered();
    scopeArmed=false; xrtMemDebugFailClear();
    bool success=result || iter || ok;
    if (!success) xl6ErrorScopeCaptureFailure(&boundary);
    xl6_error_result outcome=xl6ErrorScopeEnd(&boundary);
    assert(success==!hit && outcome.ok==!hit); xrtErrorFree(outcome.error);
    if (success && (lane==1 || lane==2)) graphObserved(source,result,lane==2);
    if (iter) iteratorObserved(iter,true);
    xrtValueRelease(result); xrtValueRelease(query); xrtValueRelease(source); xrtClearError();
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    printf("Dict consumers lane=%u mode=%s point=%zu hit=%d balanced\n",lane,scopes ? "scopes" : "oom",point,hit);
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
            assert(point<4096); printf("Dict consumers closed lane=%u mode=%s runs=%zu first-no-hit=%zu\n",
                lane,scopes ? "scopes" : "oom",point+1,point);
        }
    }
    puts("[PASS] native Dict consumers"); return 0;
}
