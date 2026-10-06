#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_array_value.h"
#include "src/runtime/xl6_array_iterator.h"
#include "src/runtime/xl6_stack.h"
#include "src/runtime/xl6_ownership_plan.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REF(id,element) id,element,3,sizeof(xvalue*),xrtTypeValue()
typedef struct Spec { unsigned kind; bool nullable; uint64_t owner,element,child; unsigned role; } Spec;
static Spec spec(unsigned profile)
{
    assert(profile<16); uint64_t child=1001+(profile%8)/2;
    return (Spec){(profile%8)/2,!!(profile%2),2001+profile,profile%2 ? 3001+profile : child,child,1+profile/8};
}
#define LAYOUT(s) REF((s)->owner,(s)->element)
#define CHILD(s) (s)->role,(s)->child,(s)->kind==3 ? 6001 : 5001+(s)->kind, \
    (s)->kind==0 ? 1u : (s)->kind==1 ? 2u : 3u, \
    (s)->kind==0 ? sizeof(int64_t) : (s)->kind==1 ? sizeof(xstrview) : sizeof(xvalue*), \
    (s)->kind==0 ? xrtTypeInt64() : (s)->kind==1 ? xl6StringTypeV1() : xrtTypeValue()
static void error(xerrkind kind)
{ assert(xrtGetError() && xrtErrorKind(xrtGetError())==kind); xrtClearError(); }
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        after.InvalidFreeCount==before->InvalidFreeCount && after.DoubleFreeCount==before->DoubleFreeCount &&
        after.UseAfterFreeCount==before->UseAfterFreeCount && !xrtGetError());
}
static xvalue* childSchema(const Spec* s,uint64_t identity)
{
    if (s->kind==0) return xl6SequencePrimitiveCreateV1(s->role,identity,5001,1,sizeof(int64_t),
        xrtTypeInt64(),XL6_VALUE_ADMIT_I8,false,5001);
    if (s->kind==1) return xl6SequencePrimitiveCreateV1(s->role,identity,5002,2,sizeof(xstrview),
        xl6StringTypeV1(),XL6_VALUE_ADMIT_STRING,false,5002);
    if (s->kind==2) return xl6SequenceReferenceCreateV1(s->role,identity,5003,XL6_VALUE_ADMIT_ANY,false,0);
    return xl6SequenceNestedCreateTakeV2(s->role,identity,6001,
        xl6SequenceReferenceCreateV1(s->role,6001,6002,XL6_VALUE_ADMIT_ANY,false,0),false);
}
static xvalue* create(const Spec* s)
{ return xl6SequenceNestedCreateTakeV2(XL6_SEQUENCE_ARRAY_V1,s->owner,s->element,childSchema(s,s->child),s->nullable); }
static bool push(const Spec* s,xvalue* value,uint64_t identity,uint64_t element,
    unsigned int transfer,size_t size,const xrttype* type,const void* item)
{ return s->role==XL6_SEQUENCE_ARRAY_V1 ? xl6ArrayPushV1(value,identity,element,transfer,size,type,item)
    : xl6StackPushV1(value,identity,element,transfer,size,type,item); }
static xvalue* child(const Spec* s,uint64_t identity)
{
    xvalue* result=childSchema(s,identity); assert(result);
    if (s->kind==0) {
        int64_t number=7; assert(result && push(s,result,identity,5001,1,sizeof(number),xrtTypeInt64(),&number));
    } else if (s->kind==1) {
        xstrview text={"a\0你",5}; assert(result && push(s,result,identity,5002,2,sizeof(text),xl6StringTypeV1(),&text));
    } else {
        xvalue* item;
        if (s->kind==2) {
            item=xrtValueInt(7);
            assert(result && item && push(s,result,REF(identity,5003),&item));
        } else {
            item=xl6SequenceReferenceCreateV1(s->role,6001,6002,XL6_VALUE_ADMIT_ANY,false,0);
            assert(result && item && push(s,result,REF(identity,6001),&item));
        }
        xrtValueRelease(item);
    }
    return result;
}
static void refused(const Spec* s,xvalue* owner,xvalue* wrong)
{
    int64_t count=xl6ArrayErasedSizeV1(owner);
    assert(xrtMemDebugFailAfter(0));
    assert(!xl6ArrayPushV1(owner,LAYOUT(s),&wrong)); error(XERR_TYPE);
    assert(!xl6ArraySetV1(owner,LAYOUT(s),0,&wrong)); error(XERR_TYPE);
    assert(!xl6ArrayInsertV1(owner,LAYOUT(s),0,&wrong)); error(XERR_TYPE);
    if (wrong) {
        assert(!xl6ArrayErasedPushV1(owner,wrong)); error(XERR_TYPE);
        assert(!xl6ArrayErasedSetV1(owner,0,wrong)); error(XERR_TYPE);
        assert(!xl6ArrayErasedInsertV1(owner,0,wrong)); error(XERR_TYPE);
    }
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    assert(xl6ArrayErasedSizeV1(owner)==count);
}
static void foreignDrop(void* data,void* user) { (void)data; (void)user; }
static void vectors(const Spec* s)
{
    assert(!xl6SequenceNestedCreateTakeV2(XL6_SEQUENCE_ARRAY_V1,0,s->element,childSchema(s,s->child),s->nullable)); error(XERR_ARGUMENT);
    assert(!xl6SequenceNestedCreateTakeV2(XL6_SEQUENCE_ARRAY_V1,s->owner,0,childSchema(s,s->child),s->nullable)); error(XERR_ARGUMENT);
    assert(!xl6SequenceNestedCreateTakeV2(XL6_SEQUENCE_ARRAY_V1,s->owner,s->element,NULL,s->nullable)); error(XERR_ARGUMENT);
    assert(!xl6SequenceNestedCreateTakeV2(XL6_SEQUENCE_ARRAY_V1,s->owner,s->child,childSchema(s,s->child),true)); error(XERR_ARGUMENT);
    assert(!xl6SequenceNestedCreateTakeV2(XL6_SEQUENCE_ARRAY_V1,s->owner,s->child+1,childSchema(s,s->child),false)); error(XERR_ARGUMENT);
    xvalue* owner=create(s),*good=child(s,s->child); assert(owner && good);
    assert(xl6ArrayPushV1(owner,LAYOUT(s),&good));
    xvalue* alias=xl6ArrayEraseV1(owner,LAYOUT(s),NULL); assert(alias==owner);
    xvalue* wrong=child(s,s->child+10); refused(s,alias,wrong); xrtValueRelease(wrong);
    /* Same logical owner identity AND published admission are insufficient:
     * the immutable element/transfer/descriptor tuple must also agree. */
    wrong=xl6SequenceReferenceCreateV1(s->role,s->child,9001,XL6_VALUE_ADMIT_ANY,false,0);
    assert(wrong); refused(s,alias,wrong); xrtValueRelease(wrong);
    /* Even the exact SAME role/identity/physical tuple is not a schema: a
     * wider scalar range, different boxed identity, bytes-vs-ANY domain or
     * different grandchild policy must all be refused before allocation. */
    if (s->kind==0) wrong=xl6SequencePrimitiveCreateV1(s->role,s->child,5001,1,sizeof(int64_t),
        xrtTypeInt64(),XL6_VALUE_ADMIT_I64,false,5001);
    else if (s->kind==1) wrong=xl6SequencePrimitiveCreateV1(s->role,s->child,5002,2,sizeof(xstrview),
        xl6StringTypeV1(),XL6_VALUE_ADMIT_STRING,false,9002);
    else if (s->kind==2) wrong=xl6SequenceReferenceCreateV1(s->role,s->child,5003,XL6_VALUE_ADMIT_BYTES,false,5003);
    else wrong=xl6SequenceNestedCreateTakeV2(s->role,s->child,6001,
        xl6SequenceReferenceCreateV1(s->role,6001,6002,XL6_VALUE_ADMIT_BYTES,false,6002),false);
    assert(wrong && xl6SequenceAdmitsRoleV1(wrong,CHILD(s)));
    refused(s,alias,wrong); xrtValueRelease(wrong);
    wrong=xl6SequencePrimitiveCreateV1(s->role==1 ? 2 : 1,s->child,5001,1,sizeof(int64_t),xrtTypeInt64(),XL6_VALUE_ADMIT_I8,false,5001);
    assert(wrong); refused(s,alias,wrong); xrtValueRelease(wrong);
    wrong=xrtValueArray(); assert(wrong && xrtValueTypeIdBind(wrong,s->child));
    refused(s,alias,wrong); xrtValueRelease(wrong);
    wrong=xl6ArrayCreateV1(s->child,7001,1,sizeof(int64_t),xrtTypeInt64()); assert(wrong);
    refused(s,alias,wrong); xrtValueRelease(wrong); /* Correct ID, no domain publication. */
    wrong=xl6StackCreateV1(REF(s->child,7001)); assert(wrong);
    refused(s,alias,wrong); xrtValueRelease(wrong);
    wrong=xl6ArrayIteratorCreateV1(owner,LAYOUT(s),s->child); assert(wrong);
    refused(s,alias,wrong); xrtValueRelease(wrong);
    static const xvaluehandleops foreign={.Drop=foreignDrop};
    void* foreignPointer=(void*)(uintptr_t)1;
    wrong=xrtValueHandleTake(&foreignPointer,&foreign,NULL); assert(wrong && xrtValueTypeIdBind(wrong,s->child));
    refused(s,alias,wrong); xrtValueRelease(wrong); /* Never dereference foreign pointer. */
    if (!s->nullable) { refused(s,alias,NULL); refused(s,alias,xrtValueNull()); }
    else {
        xvalue* absent=NULL;
        assert(xl6ArrayPushV1(owner,LAYOUT(s),&absent) && xl6ArrayErasedPushV1(alias,xrtValueNull()));
        for (int64_t i=1;i<3;++i) {
            xvalue* value=xl6ArrayErasedReadV1(alias,i); assert(value && xrtValueType(value)==XVALUE_NULL); xrtValueRelease(value);
        }
    }
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(owner,XL6_SEQUENCE_ARRAY_V1,LAYOUT(s)); assert(pin);
    XL6_SequenceRoleV1 schemaRole=0; XL6_SequenceLayoutV1 schemaLayout={0};
    XL6_SequenceAdmissionV3 schema={0};
    assert(xl6SequenceSchemaCopyV1(good,&schemaRole,&schemaLayout,&schema));
    XL6_Sequence* childPin=xl6SequenceBeginRoleV1(good,(XL6_SequenceRoleV1)s->role,
        s->child,schemaLayout.elementIdentity,schemaLayout.transfer,schemaLayout.size,schemaLayout.type);
    assert(childPin);
    xrtSetErrorKind(XERR_TIMEOUT); const xerror* pending=xrtGetError(); assert(pending);
    assert(xrtMemDebugFailAfter(0));
    assert(xl6SequenceAdmitsArrayV1(owner,s->owner) && xl6SequenceAdmitsRoleV1(good,CHILD(s)) &&
        !xl6SequenceAdmitsArrayV1(owner,s->child) && !xl6SequenceAdmitsArrayV1(NULL,s->child) &&
        xl6SequenceAdmitsSchemaV1(good,schemaRole,&schemaLayout,&schema) &&
        !xl6SequenceAdmitsSchemaV1(owner,schemaRole,&schemaLayout,&schema));
    assert(xrtGetError()==pending && !xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    xl6SequenceEndV1(pin); assert(xrtGetError()==pending); xrtClearError();
    xl6SequenceEndV1(childPin);
    xvalue* clone=xl6ArrayErasedCloneV1(alias),*snapshot=xl6ArrayToValueV1(owner,LAYOUT(s),NULL);
    xvalue* cursor=xl6ArrayIteratorCreateV1(owner,LAYOUT(s),999); assert(clone && snapshot && cursor);
    assert(xrtValueArrayAt(snapshot,0)==good);
    xrtValueRelease(alias); xrtValueRelease(owner); xrtValueRelease(good);
    xvalue* observed=xl6ArrayErasedReadV1(clone,0); assert(observed==good && xl6SequenceAdmitsRoleV1(observed,CHILD(s)));
    assert(xl6SequenceAdmitsSchemaV1(observed,schemaRole,&schemaLayout,&schema));
    xl6SequenceAdmissionDropV3(&schema);
    if (s->kind==1) {
        xstrview text={0};
        if (s->role==XL6_SEQUENCE_ARRAY_V1)
            xl6ArrayReadV1(observed,s->child,5002,2,sizeof(text),xl6StringTypeV1(),0,&text);
        else assert(xl6StackTakeV1(observed,s->child,5002,2,sizeof(text),xl6StringTypeV1(),0,&text,false));
        assert(!xrtGetError() && text.Data && text.Size==5 && !memcmp(text.Data,"a\0你",5));
        xrtTypeDropValue(xl6StringTypeV1(),&text);
    }
    assert(xl6ArrayIteratorAdvanceV1(cursor,REF(999,s->element)));
    xvalue* item=NULL; assert(xl6ArrayIteratorReadV1(cursor,REF(999,s->element),&item) && item==good); xrtValueRelease(item);
    bool grown=xl6ArrayResizeV1(clone,LAYOUT(s),7);
    if (s->nullable) assert(grown);
    else { assert(!grown); error(XERR_TYPE); }
    xvalue* moved=xl6ArrayErasedTakeV1(clone,0); assert(moved==good); xrtValueRelease(moved);
    xrtValueRelease(observed); xrtValueRelease(cursor); xrtValueRelease(clone); xrtValueRelease(snapshot);
}
static const xrtownershipadapterv1* resolve(xrtownershipref ref,void* context)
{ (void)context; const xrtownershipadapterv1* a=xl6SequenceOwnershipAdapterV1(ref); return a ? a : xrtValueOwnershipAdapterV1(ref); }
static void graph(void)
{
    xvalue* inner=xl6SequenceReferenceCreateV1(XL6_SEQUENCE_ARRAY_V1,11,33,XL6_VALUE_ADMIT_ANY,false,0);
    xvalue* outer=xl6SequenceNestedCreateTakeV2(XL6_SEQUENCE_ARRAY_V1,22,11,
        xrtValueRetain(inner),false); assert(inner && outer);
    assert(xl6ArrayPushV1(outer,REF(22,11),&inner) && xl6ArrayPushV1(inner,REF(11,33),&outer));
    xvalue* cursor=xl6ArrayIteratorCreateV1(outer,REF(22,11),44); assert(cursor);
    assert(xl6ArrayPushV1(inner,REF(11,33),&cursor));
    xvalue* wi=xrtValueWeakRef(inner),*wo=xrtValueWeakRef(outer),*wc=xrtValueWeakRef(cursor); assert(wi && wo && wc);
    xrtValueRelease(inner); xrtValueRelease(outer); xrtValueRelease(cursor);
    XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreate(xrtValueOwnership(cursor),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY && xl6OwnershipPlanFinish(plan));
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xrtValueWeakRefExpired(wi) && xrtValueWeakRefExpired(wo) && xrtValueWeakRefExpired(wc));
    xrtValueRelease(wi); xrtValueRelease(wo); xrtValueRelease(wc);
}
static xvalue* schemaChain(unsigned int rule)
{
    xvalue* exemplar=xl6SequenceReferenceCreateV1(XL6_SEQUENCE_ARRAY_V1,100,200,rule,false,
        rule==XL6_VALUE_ADMIT_ANY ? 0 : 200);
    for (uint64_t depth=1;exemplar && depth<=64;++depth)
        exemplar=xl6SequenceNestedCreateTakeV2(XL6_SEQUENCE_ARRAY_V1,100+depth,99+depth,exemplar,false);
    return exemplar;
}
static void deepSchema(void)
{
    xvalue* exemplar=schemaChain(XL6_VALUE_ADMIT_ANY); assert(exemplar && !xrtGetError());
    XL6_SequenceRoleV1 role=0; XL6_SequenceLayoutV1 layout={0}; XL6_SequenceAdmissionV3 policy={0};
    assert(xl6SequenceSchemaCopyV1(exemplar,&role,&layout,&policy));
    assert(!xl6SequenceNestedCreateTakeV2(XL6_SEQUENCE_ARRAY_V1,165,164,xrtValueRetain(exemplar),false)); error(XERR_RANGE);
    assert(xl6SequenceAdmitsSchemaV1(exemplar,role,&layout,&policy));
    xvalue* equivalent=schemaChain(XL6_VALUE_ADMIT_ANY),*different=schemaChain(XL6_VALUE_ADMIT_BYTES);
    assert(equivalent && different && xl6SequenceAdmitsRoleV1(different,role,layout.identity,
        layout.elementIdentity,layout.transfer,layout.size,layout.type));
    xrtSetErrorKind(XERR_TIMEOUT); const xerror* pending=xrtGetError();
    assert(xrtMemDebugFailAfter(0));
    assert(xl6SequenceAdmitsSchemaV1(equivalent,role,&layout,&policy));
    assert(!xl6SequenceAdmitsSchemaV1(different,role,&layout,&policy));
    assert(xrtGetError()==pending && !xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); xrtClearError();
    xrtValueRelease(equivalent); xrtValueRelease(different);
    xrtValueRelease(exemplar); /* The full chain lives solely through metadata. */
    xvalue* restored=xl6SequenceCreateAdmittedV1(role,layout.identity,&policy); assert(restored && !policy.ops);
    xrtValueRelease(restored);
}
static void deepFault(size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    assert(xrtMemDebugFailAfter(point)); xvalue* result=schemaChain(XL6_VALUE_ADMIT_ANY);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    if (hit) { assert(!result); error(XERR_MEMORY); }
    else assert(result && !xrtGetError());
    xrtValueRelease(result); balanced(&before);
    printf("operation=deep-create point=%zu hit=%d balanced\n",point,(int)hit);
}
static void captureFailures(void)
{
    XL6_SequenceRoleV1 role=XL6_SEQUENCE_CURSOR_V1;
    XL6_SequenceLayoutV1 layout={.identity=777}; XL6_SequenceAdmissionV3 policy={0};
    xvalue* unprofiled=xl6ArrayCreateV1(101,201,1,sizeof(int64_t),xrtTypeInt64()); assert(unprofiled);
    assert(!xl6SequenceSchemaCopyV1(unprofiled,&role,&layout,&policy)); error(XERR_STATE);
    assert(role==XL6_SEQUENCE_CURSOR_V1 && layout.identity==777 && !policy.ops && !policy.data);
    xvalue* weak=xrtValueWeakRef(unprofiled); assert(weak);
    assert(!xl6SequenceNestedCreateTakeV2(XL6_SEQUENCE_ARRAY_V1,102,101,unprofiled,false)); error(XERR_STATE);
    assert(xrtValueWeakRefExpired(weak)); xrtValueRelease(weak);
    static const xvaluehandleops foreign={.Drop=foreignDrop}; void* pointer=(void*)(uintptr_t)1;
    xvalue* invalid=xrtValueHandleTake(&pointer,&foreign,NULL); assert(invalid);
    assert(!xl6SequenceSchemaCopyV1(invalid,&role,&layout,&policy)); error(XERR_ARGUMENT);
    assert(role==XL6_SEQUENCE_CURSOR_V1 && layout.identity==777 && !policy.ops && !policy.data);
    assert(!xl6SequenceNestedCreateTakeV2(XL6_SEQUENCE_ARRAY_V1,102,101,invalid,false)); error(XERR_ARGUMENT);
    xvalue* valid=xl6SequenceReferenceCreateV1(XL6_SEQUENCE_ARRAY_V1,101,201,XL6_VALUE_ADMIT_ANY,false,0); assert(valid);
    weak=xrtValueWeakRef(valid); assert(weak);
    assert(!xl6SequenceNestedCreateTakeV2(XL6_SEQUENCE_CURSOR_V1,102,101,valid,false)); error(XERR_ARGUMENT);
    assert(xrtValueWeakRefExpired(weak)); xrtValueRelease(weak);
    xrtSetErrorKind(XERR_TIMEOUT); const xerror* prior=xrtGetError();
    assert(!xl6SequenceNestedCreateTakeV2(XL6_SEQUENCE_ARRAY_V1,102,101,NULL,false));
    assert(xrtGetError()==prior); xrtClearError();
}
static void fault(const Spec* s,const char* operation,size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue* good=child(s,s->child),*owner=NULL,*alias=NULL,*result=NULL;
    if (strcmp(operation,"create")) {
        owner=create(s); assert(owner && xl6ArrayPushV1(owner,LAYOUT(s),&good));
        alias=xl6ArrayEraseV1(owner,LAYOUT(s),NULL); assert(alias);
    }
    assert(xrtMemDebugFailAfter(point)); bool ok;
    if (!strcmp(operation,"create")) { result=create(s); ok=result!=NULL; }
    else if (!strcmp(operation,"clone")) { result=xl6ArrayCloneV1(owner,LAYOUT(s)); ok=result!=NULL; }
    else if (!strcmp(operation,"erased-clone")) { result=xl6ArrayErasedCloneV1(alias); ok=result!=NULL; }
    else if (!strcmp(operation,"cursor")) { result=xl6ArrayIteratorCreateV1(owner,LAYOUT(s),999); ok=result!=NULL; }
    else if (!strcmp(operation,"snapshot")) { result=xl6ArrayToValueV1(owner,LAYOUT(s),NULL); ok=result!=NULL; }
    else if (!strcmp(operation,"read")) { result=xl6ArrayErasedReadV1(alias,0); ok=result!=NULL; }
    else if (!strcmp(operation,"take")) { result=xl6ArrayErasedTakeV1(alias,0); ok=result!=NULL; }
    else if (!strcmp(operation,"push")) ok=xl6ArrayErasedPushV1(alias,good);
    else if (!strcmp(operation,"set")) ok=xl6ArrayErasedSetV1(alias,0,good);
    else if (!strcmp(operation,"insert")) ok=xl6ArrayErasedInsertV1(alias,0,good);
    else if (!strcmp(operation,"reserve")) ok=xl6ArrayReserveV1(owner,LAYOUT(s),257);
    else { assert(!strcmp(operation,"resize") && s->nullable); ok=xl6ArrayResizeV1(owner,LAYOUT(s),257); }
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    if (hit) {
        assert(!ok); error(XERR_MEMORY);
        if (owner) {
            assert(xl6ArraySizeV1(owner,LAYOUT(s),false)==1 && xl6SequenceAdmitsArrayV1(owner,s->owner));
            xvalue* value=xl6ArrayErasedReadV1(alias,0); assert(value==good); xrtValueRelease(value);
        }
    } else assert(ok && !xrtGetError());
    xrtValueRelease(result); xrtValueRelease(alias); xrtValueRelease(owner); xrtValueRelease(good);
    balanced(&before); printf("operation=%s point=%zu hit=%d balanced\n",operation,point,(int)hit);
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true));
    if (argc==3) { assert(!strcmp(argv[1],"deep-create")); deepFault((size_t)strtoull(argv[2],NULL,10)); return 0; }
    if (argc==4) { Spec s=spec((unsigned)strtoul(argv[1],NULL,10)); fault(&s,argv[2],(size_t)strtoull(argv[3],NULL,10)); return 0; }
    assert(argc==1);
    for (unsigned i=0;i<16;++i) { Spec s=spec(i); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before); vectors(&s); balanced(&before); }
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before); graph(); deepSchema(); captureFailures(); balanced(&before);
    puts("16 nested Sequence domains, recursive schema, precise NUL text, nullable defaults, identity/role/publication and cyclic owners balanced"); return 0;
}
