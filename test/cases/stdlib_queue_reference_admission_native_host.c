#include "src/runtime/xl6_concurrent_queue.h"
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

typedef struct Family {
    xvalue* (*Create)(uint64_t,xvalue*,int64_t);
    xvalue* (*Schema)(unsigned int,uint64_t,uint64_t,uint64_t,xvalue*,bool);
    bool (*Admits)(const xvalue*,uint64_t,const XL6_SequenceLayoutV1*,const XL6_SequenceAdmissionV3*);
    xqueueresult (*Push)(const xvalue*,uint64_t,uint64_t,size_t,const xrttype*,const void*);
    bool (*Reset)(const xvalue*,uint64_t);
} Family;
#define FAMILY(Name) {xl6##Name##CreateTakeV1,xl6Sequence##Name##CreateTakeV1, \
    xl6##Name##AdmitsSchemaV1,xl6##Name##PushV1,xl6##Name##ResetV1}
static const Family families[]={FAMILY(SpscQueue),FAMILY(MpscQueue),FAMILY(MpmcQueue)};
enum { QUEUE=701,SEED=702,ITEM=703,PARENT=704,OPTIONAL=705 };
#define TUPLE PARENT,QUEUE,3,sizeof(xvalue*),xrtTypeValue()
static xvalue* seed(uint64_t identity,unsigned int rule)
{
    return xl6SequenceReferenceCreateV1(XL6_SEQUENCE_ARRAY_V1,identity,ITEM,rule,false,
        rule==XL6_VALUE_ADMIT_ANY ? 0 : ITEM);
}
static void error(xerrkind kind)
{ assert(xrtGetError() && xrtErrorKind(xrtGetError())==kind); xrtClearError(); }
static void foreignDrop(void* data,void* context) { (void)data; (void)context; }
static void vectors(void)
{
    for (size_t f=0;f<3;++f) {
        const Family* family=&families[f];
        xvalue* array=family->Schema(XL6_SEQUENCE_ARRAY_V1,PARENT,QUEUE,QUEUE,seed(SEED,XL6_VALUE_ADMIT_ANY),false);
        xvalue* good=family->Create(QUEUE,seed(SEED,XL6_VALUE_ADMIT_ANY),4);
        xvalue* wrong=family->Create(QUEUE,seed(SEED,XL6_VALUE_ADMIT_BYTES),8);
        xvalue* otherSchema=family->Create(QUEUE,seed(SEED+100,XL6_VALUE_ADMIT_ANY),4);
        xvalue* otherFamily=families[(f+1)%3].Create(QUEUE,seed(SEED,XL6_VALUE_ADMIT_ANY),4);
        assert(array && good && wrong && otherSchema && otherFamily);
        assert(xl6ArrayPushV1(array,TUPLE,&good));
        assert(xrtMemDebugFailAfter(0));
        assert(!xl6ArrayPushV1(array,TUPLE,&wrong)); error(XERR_TYPE);
        assert(!xl6ArrayPushV1(array,TUPLE,&otherSchema)); error(XERR_TYPE);
        assert(!xl6ArrayPushV1(array,TUPLE,&otherFamily)); error(XERR_TYPE);
        assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
        static const xvaluehandleops foreign={.Drop=foreignDrop}; void* pointer=(void*)(uintptr_t)1;
        xvalue* forged=xrtValueHandleTake(&pointer,&foreign,NULL); assert(forged && xrtValueTypeIdBind(forged,QUEUE));
        assert(!xl6ArrayPushV1(array,TUPLE,&forged)); error(XERR_TYPE); xrtValueRelease(forged);
        forged=xrtValueArray(); assert(forged && xrtValueTypeIdBind(forged,QUEUE));
        assert(!xl6ArrayPushV1(array,TUPLE,&forged)); error(XERR_TYPE); xrtValueRelease(forged);
        assert(!xl6ArrayResizeV1(array,TUPLE,3)); error(XERR_TYPE);
        assert(xl6ArraySizeV1(array,TUPLE,false)==1);
        xvalue* clone=xl6ArrayCloneV1(array,TUPLE); assert(clone);
        xrtValueRelease(array); array=clone;
        xvalue* optional=family->Schema(XL6_SEQUENCE_ARRAY_V1,PARENT,OPTIONAL,QUEUE,seed(SEED,XL6_VALUE_ADMIT_ANY),true);
        assert(optional && xl6ArrayResizeV1(optional,PARENT,OPTIONAL,3,sizeof(xvalue*),xrtTypeValue(),3));
        xvalue* erased=xl6ArrayEraseV1(optional,PARENT,OPTIONAL,3,sizeof(xvalue*),xrtTypeValue(),NULL); assert(erased==optional);
        xvalue* item=xl6ArrayErasedReadV1(optional,0); assert(item && xrtValueType(item)==XVALUE_NULL);
        xrtValueRelease(item); xrtValueRelease(erased);
        XL6_SequenceRoleV1 role=0; XL6_SequenceLayoutV1 layout={0}; XL6_SequenceAdmissionV3 policy={0};
        xvalue* witness=seed(SEED,XL6_VALUE_ADMIT_ANY);
        assert(xl6SequenceSchemaCopyV1(witness,&role,&layout,&policy)); xrtValueRelease(witness);
        xrtSetErrorKind(XERR_TIMEOUT); const xerror* prior=xrtGetError(); assert(xrtMemDebugFailAfter(0));
        assert(family->Admits(good,QUEUE,&layout,&policy) && !family->Admits(wrong,QUEUE,&layout,&policy));
        assert(xrtGetError()==prior && !xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); xrtClearError();
        xl6SequenceAdmissionDropV3(&policy);
        witness=seed(SEED,XL6_VALUE_ADMIT_ANY); item=xrtValueNull();
        assert(xl6ArrayPushV1(witness,SEED,ITEM,3,sizeof(xvalue*),xrtTypeValue(),&item));
        assert(!family->Schema(XL6_SEQUENCE_ARRAY_V1,PARENT,QUEUE,QUEUE,witness,false)); error(XERR_ARGUMENT);
        xrtValueRelease(optional); xrtValueRelease(otherFamily); xrtValueRelease(otherSchema);
        xrtValueRelease(wrong); xrtValueRelease(good); xrtValueRelease(array);
    }
}
static xvalue* keys(const XL6_TreeLayoutV2* tree)
{
    return xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,tree->key.identity,tree->key.elementIdentity,
        1,sizeof(int64_t),xrtTypeInt64(),XL6_VALUE_ADMIT_I64,false,tree->key.elementIdentity);
}
static xvalue* chain(unsigned int rule)
{
    xvalue* result=seed(100,rule);
    for (unsigned int depth=1;result && depth<=64;++depth) {
        XL6_SequenceRoleV1 role=0; XL6_SequenceLayoutV1 child={0}; XL6_SequenceAdmissionV3 policy={0};
        assert(xl6SequenceSchemaCopyV1(result,&role,&child,&policy)); xl6SequenceAdmissionDropV3(&policy);
        if (depth%4==0) result=xl6SequenceNestedCreateTakeV2(XL6_SEQUENCE_ARRAY_V1,100+depth,child.identity,result,false);
        else if (depth%4==1) {
            XL6_TreeLayoutV2 tree={10000+depth,2,{20000+depth,500,1,sizeof(int64_t),xrtTypeInt64()},child,NULL};
            result=xl6SequenceTreeCreateTakeV1(XL6_SEQUENCE_ARRAY_V1,100+depth,tree.identity,&tree,keys(&tree),result,false);
        } else result=families[depth%3].Schema(XL6_SEQUENCE_ARRAY_V1,100+depth,10000+depth,10000+depth,result,false);
    }
    return result;
}
static void depth(void)
{
    xvalue* good=chain(XL6_VALUE_ADMIT_ANY),*same=chain(XL6_VALUE_ADMIT_ANY),*wrong=chain(XL6_VALUE_ADMIT_BYTES);
    assert(good && same && wrong);
    XL6_SequenceRoleV1 role=0; XL6_SequenceLayoutV1 layout={0}; XL6_SequenceAdmissionV3 policy={0};
    assert(xl6SequenceSchemaCopyV1(good,&role,&layout,&policy) && xl6ResidentAdmissionDepthV1(&policy)==64);
    xrtSetErrorKind(XERR_TIMEOUT); const xerror* prior=xrtGetError(); assert(xrtMemDebugFailAfter(0));
    assert(xl6SequenceAdmitsSchemaV1(same,role,&layout,&policy) && !xl6SequenceAdmitsSchemaV1(wrong,role,&layout,&policy));
    assert(xrtGetError()==prior && !xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); xrtClearError();
    for (size_t f=0;f<3;++f) {
        assert(!families[f].Schema(XL6_SEQUENCE_ARRAY_V1,165,9000,9000,xrtValueRetain(good),false)); error(XERR_RANGE);
    }
    xrtValueRelease(good); xrtValueRelease(same); xrtValueRelease(wrong);
    xvalue* restored=xl6SequenceCreateAdmittedV1(role,layout.identity,&policy); assert(restored); xrtValueRelease(restored);
}
static void fault(const char* operation,size_t point)
{
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope)); assert(xrtMemDebugFailAfter(point));
    xvalue* result=!strcmp(operation,"deep") ? chain(XL6_VALUE_ADMIT_ANY) :
        families[strtoul(operation,NULL,10)%3].Schema(XL6_SEQUENCE_ARRAY_V1,PARENT,QUEUE,QUEUE,seed(SEED,XL6_VALUE_ADMIT_ANY),false);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(hit ? !result && !outcome.ok : result && outcome.ok);
    xrtErrorFree(outcome.error); xrtValueRelease(result); xrtClearError();
    printf("queue reference %s point=%zu hit=%d complete-prefix balanced\n",operation,point,hit);
}
int main(int argc,char** argv)
{
    assert(argc==1 || argc==3); xl6ProductMemoryBegin(SIZE_MAX);
    if (argc==3) fault(argv[1],(size_t)strtoull(argv[2],NULL,10)); else { vectors(); depth(); }
    assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    if (argc==1) puts("Queue reference: genuine three-family schemas, nullable and mixed depth64 balanced");
    return 0;
}
