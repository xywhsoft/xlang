/* Reuse the real per-profile scalar/text slots and fault oracle, not a
 * generated-code simulation. The added lane selects explicit Value snapshots. */
#define main xl6ProjectionPrimitiveMain
#include "test/cases/stdlib_array_primitive_admission_native_host.c"
#undef main
#include "src/runtime/xl6_array_value.h"

static void projected(const Spec* s,const xvalue* value)
{
    assert(value && xrtValueTypeId(value)==projectionIdentity(s,value));
    assert(xl6ValueAdmitsV1(value,s->rule,s->nullable,s->identity));
}
static xvalue* populated(const Spec* s)
{
    xvalue* owner=create(s),*good=element(s); assert(owner && good);
    Slot slot={0}; toSlot(s,good,&slot);
    assert(xl6ArrayPushV1(owner,LAYOUT(s),&slot));
    xrtTypeDropValue(s->type,&slot); xrtValueRelease(good);
    xvalue* alias=xl6ArrayEraseV1(owner,LAYOUT(s),s->converter);
    assert(alias==owner); xrtValueRelease(alias); return owner;
}
static void snapshotVectors(const Spec* s)
{
    xvalue* owner=populated(s);
    xvalue* clone=xl6ArrayErasedCloneV1(owner); assert(clone);
    xvalue* cursor=xl6ArrayIteratorCreateV1(owner,LAYOUT(s),999); assert(cursor);
    XL6_Sequence* pin=xl6SequenceIteratorBeginV1(cursor,999,s->element,s->transfer,s->type->Size,s->type);
    assert(pin && xl6SequenceBoxedIdentityV1(pin)==s->identity); xl6SequenceEndV1(pin);
    xrtValueRelease(owner); owner=clone;
    /* Only self-owned metadata remains; no source shell/context is available. */
    xvalue* snapshot=xl6ArrayToValueV1(owner,LAYOUT(s),s->converter);
    assert(snapshot && xrtValueCount(snapshot)==1); projected(s,xrtValueArrayAt(snapshot,0));
    xvalue* read=xl6ArrayErasedReadV1(owner,0); projected(s,read);
    assert(xrtValueEqual(read,xrtValueArrayAt(snapshot,0))); xrtValueRelease(read);
    read=xl6ArrayErasedTakeV1(owner,0); projected(s,read);
    assert(xrtValueEqual(read,xrtValueArrayAt(snapshot,0)) && xl6ArrayErasedSizeV1(owner)==0);
    xrtValueRelease(read);
    assert(xl6ArrayResizeV1(owner,LAYOUT(s),1));
    read=xl6ArrayErasedReadV1(owner,0); projected(s,read);
    if (s->nullable) assert(xrtValueType(read)==XVALUE_NULL);
    xrtValueRelease(read); xrtValueRelease(cursor); xrtValueRelease(owner);
    /* Explicit snapshot is independently owned after its typed source dies. */
    projected(s,xrtValueArrayAt(snapshot,0)); xrtValueRelease(snapshot);
}
static void metadataVectors(const Spec* s)
{
    xvalue* source=populated(s);
    XL6_SequenceAdmissionV2 metadata={0};
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(source,XL6_SEQUENCE_ARRAY_V1,LAYOUT(s));
    assert(pin && xl6SequenceAdmissionCopyV2(pin,&metadata)); xl6SequenceEndV1(pin);
    assert(metadata.boxedIdentity==s->identity);
    xvalue* target=xl6ArrayCreateV1(LAYOUT(s)); assert(target);
    pin=xl6SequenceBeginRoleV1(target,XL6_SEQUENCE_ARRAY_V1,LAYOUT(s)); assert(pin);
    metadata.boxedIdentity=0;
    /* Malformed data remains the native caller's responsibility. */
    assert(!xl6SequenceAdmissionTakeV2(pin,&metadata)); error(XERR_ARGUMENT);
    assert(metadata.ops && metadata.data);
    metadata.boxedIdentity=s->identity;
    assert(xl6SequenceAdmissionTakeV2(pin,&metadata) && !metadata.ops && !metadata.data);
    assert(xl6SequenceBoxedIdentityV1(pin)==s->identity); xl6SequenceEndV1(pin);
    assert(xl6ArrayAppendV1(target,source,LAYOUT(s)));
    xvalue* alias=xl6ArrayEraseV1(target,LAYOUT(s),s->converter); assert(alias);
    xvalue* value=xl6ArrayErasedReadV1(alias,0); projected(s,value);
    xrtValueRelease(value); xrtValueRelease(alias); xrtValueRelease(target);
    /* A mismatched projection identity must fail before destructive take.
     * bool/null stay canonical singletons and cannot carry such metadata. */
    if (s->rule!=XL6_VALUE_ADMIT_BOOL) {
        pin=xl6SequenceBeginRoleV1(source,XL6_SEQUENCE_ARRAY_V1,LAYOUT(s));
        assert(pin && xl6SequenceAdmissionCopyV2(pin,&metadata)); xl6SequenceEndV1(pin);
        ++metadata.boxedIdentity;
        target=xl6ArrayCreateV1(LAYOUT(s)); assert(target);
        pin=xl6SequenceBeginRoleV1(target,XL6_SEQUENCE_ARRAY_V1,LAYOUT(s)); assert(pin);
        assert(xl6SequenceAdmissionTakeV2(pin,&metadata)); xl6SequenceEndV1(pin);
        xvalue* good=element(s); Slot slot={0}; toSlot(s,good,&slot);
        assert(xl6ArrayPushV1(target,LAYOUT(s),&slot)); xrtTypeDropValue(s->type,&slot);
        alias=xl6ArrayEraseV1(target,LAYOUT(s),s->converter); assert(alias);
        assert(!xl6ArrayErasedReadV1(alias,0)); error(XERR_TYPE);
        assert(!xl6ArrayErasedTakeV1(alias,0)); error(XERR_TYPE);
        assert(!xl6ArrayToValueV1(target,LAYOUT(s),s->converter)); error(XERR_TYPE);
        assert(xl6ArraySizeV1(target,LAYOUT(s),false)==1);
        Slot read={0}; assert(xl6ArrayReadV1(target,LAYOUT(s),0,&read));
        value=xrtValueFromTyped(s->type,&read,s->converter); assert(value && xrtValueEqual(value,good));
        xrtValueRelease(value); xrtTypeDropValue(s->type,&read); xrtValueRelease(good);
        xrtValueRelease(alias); xrtValueRelease(target);
    }
    xrtValueRelease(source);
}
static void snapshotFault(const Spec* s,size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue* owner=populated(s); assert(xrtMemDebugFailAfter(point));
    xvalue* result=xl6ArrayToValueV1(owner,LAYOUT(s),s->converter);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    if (!result) { assert(hit && xrtGetError()); xrtClearError(); }
    else { projected(s,xrtValueArrayAt(result,0)); assert(!xrtGetError()); }
    assert(xl6ArraySizeV1(owner,LAYOUT(s),false)==1);
    xvalue* read=xl6ArrayErasedReadV1(owner,0); projected(s,read);
    xrtValueRelease(read); xrtValueRelease(result); xrtValueRelease(owner);
    balanced(&before); printf("profile=%d operation=snapshot point=%zu hit=%d balanced\n",PROFILE,point,(int)hit);
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true));
    if (argc==3 && !strcmp(argv[1],"snapshot")) {
        Spec s=spec(rules[0]); snapshotFault(&s,(size_t)strtoull(argv[2],NULL,10)); return 0;
    }
    if (argc==3) return xl6ProjectionPrimitiveMain(argc,argv);
    assert(argc==1);
    for (size_t i=0;i<sizeof(rules)/sizeof(*rules);++i) {
        Spec s=spec(rules[i]); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
        vectors(&s); policyVectors(&s); snapshotVectors(&s); metadataVectors(&s); balanced(&before);
    }
    printf("profile=%d owned projection identities and snapshots balanced\n",PROFILE); return 0;
}
