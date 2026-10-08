#include "src/runtime/xl6_tree.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_resident_reference_schema.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { TREE=7001,KEY_ARRAY=7002,VALUE_ARRAY=7003,KEY=501,ITEM=502,PARENT=9001,OPTIONAL=9002 };
static XL6_TreeLayoutV1 layout(void)
{
    return (XL6_TreeLayoutV1){TREE,2,
        {KEY_ARRAY,KEY,1,sizeof(int64_t),xrtTypeInt64()},
        {VALUE_ARRAY,ITEM,3,sizeof(xvalue*),xrtTypeValue()}};
}
static xvalue* keys(const XL6_TreeLayoutV1* tuple)
{
    return xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,tuple->key.identity,tuple->key.elementIdentity,
        1,sizeof(int64_t),xrtTypeInt64(),XL6_VALUE_ADMIT_I64,false,tuple->key.elementIdentity);
}
static xvalue* values(const XL6_TreeLayoutV1* tuple,unsigned int rule)
{
    return xl6SequenceReferenceCreateV1(XL6_SEQUENCE_ARRAY_V1,tuple->value.identity,tuple->value.elementIdentity,
        rule,false,rule==XL6_VALUE_ADMIT_ANY ? 0 : tuple->value.elementIdentity);
}
static xvalue* tree(const XL6_TreeLayoutV1* tuple,unsigned int rule)
{ return xl6TreeCreateTakeV1(tuple,keys(tuple),values(tuple,rule)); }
static xvalue* parent(const XL6_TreeLayoutV1* tuple,unsigned int rule,bool nullable)
{
    return xl6SequenceTreeCreateTakeV1(XL6_SEQUENCE_ARRAY_V1,PARENT,nullable ? OPTIONAL : TREE,
        tuple,keys(tuple),values(tuple,rule),nullable);
}
#define PARENT_TUPLE(element) PARENT,element,3,sizeof(xvalue*),xrtTypeValue()
static void clearError(xerrkind kind)
{ assert(xrtGetError() && xrtErrorKind(xrtGetError())==kind); xrtClearError(); }
static void foreignDrop(void* data,void* context) { (void)data; (void)context; }
typedef struct Reentry { xvalue* array; xvalue* tree; unsigned int drops; } Reentry;
static bool retained(void* data,void** out,void* context) { (void)context; *out=data; return true; }
static void reentrant(void* data,void* context)
{
    Reentry* state=data; assert(state==context); ++state->drops;
    /* Tree is operation-pinned/BUSY. Its immutable schema must still be
     * admitted without reading storage or trying to start a second operation. */
    assert(xl6ArrayPushV1(state->array,PARENT_TUPLE(TREE),&state->tree));
}
static void vectors(void)
{
    XL6_TreeLayoutV1 tuple=layout();
    xvalue* array=parent(&tuple,XL6_VALUE_ADMIT_ANY,false);
    xvalue* good=tree(&tuple,XL6_VALUE_ADMIT_ANY),*wrong=tree(&tuple,XL6_VALUE_ADMIT_BYTES);
    assert(array && good && wrong && xl6ArrayPushV1(array,PARENT_TUPLE(TREE),&good));
    assert(xrtMemDebugFailAfter(0));
    assert(!xl6ArrayPushV1(array,PARENT_TUPLE(TREE),&wrong)); clearError(XERR_TYPE);
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    assert(xl6ArraySizeV1(array,PARENT_TUPLE(TREE),false)==1);
    static const xvaluehandleops foreign={.Drop=foreignDrop}; void* pointer=(void*)(uintptr_t)1;
    xvalue* forged=xrtValueHandleTake(&pointer,&foreign,NULL); assert(forged && xrtValueTypeIdBind(forged,TREE));
    assert(!xl6ArrayPushV1(array,PARENT_TUPLE(TREE),&forged)); clearError(XERR_TYPE); xrtValueRelease(forged);
    forged=xrtValueArray(); assert(forged && xrtValueTypeIdBind(forged,TREE));
    assert(!xl6ArrayPushV1(array,PARENT_TUPLE(TREE),&forged)); clearError(XERR_TYPE); xrtValueRelease(forged);
    xvalue* clone=xl6ArrayCloneV1(array,PARENT_TUPLE(TREE)); assert(clone);
    xrtValueRelease(array); array=clone;
    Reentry state={array,good,0}; pointer=&state;
    static const xvaluehandleops callbacks={retained,reentrant,NULL,NULL};
    xvalue* callback=xrtValueHandleTake(&pointer,&callbacks,&state); assert(callback);
    int64_t key=1; assert(xl6TreeSetV1(good,&tuple,&key,&callback)); xrtValueRelease(callback);
    xvalue* absent=xrtValueNull(); assert(xl6TreeSetV1(good,&tuple,&key,&absent));
    assert(state.drops==1 && xl6ArraySizeV1(array,PARENT_TUPLE(TREE),false)==2);
    xvalue* optional=parent(&tuple,XL6_VALUE_ADMIT_ANY,true); assert(optional);
    assert(xl6ArrayResizeV1(optional,PARENT_TUPLE(OPTIONAL),3));
    xvalue* erased=xl6ArrayEraseV1(optional,PARENT_TUPLE(OPTIONAL),NULL); assert(erased==optional);
    xvalue* item=xl6ArrayErasedReadV1(optional,0); assert(item && xrtValueType(item)==XVALUE_NULL); xrtValueRelease(item);
    xrtValueRelease(erased);
    assert(xl6ArrayPushV1(optional,PARENT_TUPLE(OPTIONAL),&good));
    XL6_SequenceAdmissionV3 k={0},v={0}; xvalue* keySeed=keys(&tuple),*valueSeed=values(&tuple,XL6_VALUE_ADMIT_ANY);
    assert(xl6TreeDomainsCopyV1(&tuple,keySeed,valueSeed,&k,&v));
    xrtValueRelease(keySeed); xrtValueRelease(valueSeed);
    xrtSetErrorKind(XERR_TIMEOUT); const xerror* prior=xrtGetError();
    assert(xrtMemDebugFailAfter(0));
    assert(xl6TreeAdmitsSchemaV1(good,&tuple,&k,&v) && !xl6TreeAdmitsSchemaV1(wrong,&tuple,&k,&v));
    assert(xrtGetError()==prior && !xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); xrtClearError();
    xl6SequenceAdmissionDropV3(&v); xl6SequenceAdmissionDropV3(&k);
    xrtValueRelease(optional); xrtValueRelease(wrong); xrtValueRelease(good); xrtValueRelease(array);
}
static xvalue* chain(unsigned int rule)
{
    xvalue* result=xl6SequenceReferenceCreateV1(XL6_SEQUENCE_ARRAY_V1,100,200,rule,false,
        rule==XL6_VALUE_ADMIT_ANY ? 0 : 200);
    for (unsigned int depth=1;result && depth<=64;++depth) {
        XL6_SequenceRoleV1 role=0; XL6_SequenceLayoutV1 child={0}; XL6_SequenceAdmissionV3 policy={0};
        assert(xl6SequenceSchemaCopyV1(result,&role,&child,&policy));
        xl6SequenceAdmissionDropV3(&policy);
        if (depth%2) {
            XL6_TreeLayoutV1 tuple=layout(); tuple.identity=10000+depth; tuple.value=child;
            result=xl6SequenceTreeCreateTakeV1(XL6_SEQUENCE_ARRAY_V1,100+depth,tuple.identity,
                &tuple,keys(&tuple),result,false);
        } else result=xl6SequenceNestedCreateTakeV2(XL6_SEQUENCE_ARRAY_V1,100+depth,child.identity,result,false);
    }
    return result;
}
static void depth(void)
{
    xvalue* good=chain(XL6_VALUE_ADMIT_ANY),*same=chain(XL6_VALUE_ADMIT_ANY),*different=chain(XL6_VALUE_ADMIT_BYTES);
    assert(good && same && different);
    XL6_SequenceRoleV1 role=0; XL6_SequenceLayoutV1 tuple={0}; XL6_SequenceAdmissionV3 policy={0};
    assert(xl6SequenceSchemaCopyV1(good,&role,&tuple,&policy) && xl6ResidentAdmissionDepthV1(&policy)==64);
    xrtSetErrorKind(XERR_TIMEOUT); const xerror* prior=xrtGetError(); assert(xrtMemDebugFailAfter(0));
    assert(xl6SequenceAdmitsSchemaV1(same,role,&tuple,&policy) && !xl6SequenceAdmitsSchemaV1(different,role,&tuple,&policy));
    assert(xrtGetError()==prior && !xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); xrtClearError();
    assert(!xl6SequenceNestedCreateTakeV2(XL6_SEQUENCE_ARRAY_V1,165,164,xrtValueRetain(good),false)); clearError(XERR_RANGE);
    XL6_TreeLayoutV1 treeTuple=layout(); treeTuple.identity=20000; treeTuple.value=tuple;
    assert(!xl6SequenceTreeCreateTakeV1(XL6_SEQUENCE_ARRAY_V1,166,treeTuple.identity,
        &treeTuple,keys(&treeTuple),xrtValueRetain(good),false)); clearError(XERR_RANGE);
    xrtValueRelease(good); xrtValueRelease(same); xrtValueRelease(different);
    xvalue* restored=xl6SequenceCreateAdmittedV1(role,tuple.identity,&policy); assert(restored);
    xrtValueRelease(restored);
}
static void fault(const char* operation,size_t point)
{
    XL6_TreeLayoutV1 tuple=layout(); xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
    assert(xrtMemDebugFailAfter(point));
    xvalue* result=!strcmp(operation,"deep") ? chain(XL6_VALUE_ADMIT_ANY) : parent(&tuple,XL6_VALUE_ADMIT_ANY,false);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(hit ? !result && !outcome.ok : result && outcome.ok);
    xrtErrorFree(outcome.error); xrtValueRelease(result); xrtClearError();
    printf("tree admission %s point=%zu hit=%d complete-prefix balanced\n",operation,point,hit);
}
int main(int argc,char** argv)
{
    assert(argc==1 || argc==3); xl6ProductMemoryBegin(SIZE_MAX);
    if (argc==3) fault(argv[1],(size_t)strtoull(argv[2],NULL,10));
    else { vectors(); depth(); }
    assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    if (argc==1) puts("Tree admission: full domains, genuine provenance, BUSY, nullable and mixed depth64 balanced");
    return 0;
}
