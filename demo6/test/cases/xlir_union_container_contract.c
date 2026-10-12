#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/cgen/xl6_ir_cgen.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array_erased.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned mutations;
static void refused(const XL6_IrModule* module,XL6_IrTypeId id)
{
    assert(!xl6IrSequenceConstructionRecipe(module,id,XL6_IR_SEQUENCE_ROLE_ARRAY,NULL));
    assert(!xl6IrVerifyModule(module,NULL));
    for(unsigned header=0;header<2;++header) {
        XL6_Output out; size_t size=SIZE_MAX; assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,module,"BAD_UNION_ABI",NULL) : xl6IrCgenEmitC(&out,module,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&size); assert(!size);
        xrtFree(text); xl6OutputDiscard(&out);
    }
    ++mutations;
}
enum { OWNER=123, ELEMENT=456, OPTIONAL=789 };
#define LAYOUT OWNER,ELEMENT,3,sizeof(xvalue*),xrtTypeValue()
static xvalue* boxed(size_t size,uint64_t identity)
{
    unsigned char data[16]={42}; assert(size<=sizeof(data));
    xvalue* value=xrtValueBytes((xbytesview){data,size}); assert(value);
    assert(xrtValueTypeIdBind(value,identity)); return value;
}
static void nativeDomain(void)
{
    xvalue* owner=xl6SequenceUnionCreateV1(1,OWNER,ELEMENT,ELEMENT,8,false); assert(owner);
    xvalue* valid=boxed(8,ELEMENT), *wrongSize=boxed(7,ELEMENT), *wrongId=boxed(8,OPTIONAL);
    assert(xl6ArrayPushV1(owner,LAYOUT,&valid));
    assert(!xl6ArrayPushV1(owner,LAYOUT,&wrongSize)); assert(xrtErrorKind(xrtGetError())==XERR_TYPE); xrtClearError();
    assert(!xl6ArrayPushV1(owner,LAYOUT,&wrongId)); assert(xrtErrorKind(xrtGetError())==XERR_TYPE); xrtClearError();
    xvalue* empty=NULL;
    assert(!xl6ArrayPushV1(owner,LAYOUT,&empty)); xrtClearError();
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(owner,XL6_SEQUENCE_ARRAY_V1,LAYOUT); assert(pin);
    XL6_SequenceAdmissionV3 policy={0}; assert(xl6SequenceAdmissionCopyV3(pin,&policy));
    xrtSetErrorKind(XERR_STATE); const xerror* pending=xrtGetError(); assert(pending);
    assert(!policy.ops->Value(policy.data,wrongSize) && xrtGetError()==pending);
    assert(!policy.ops->Value(policy.data,wrongId) && xrtGetError()==pending);
    assert(policy.ops->Value(policy.data,valid) && xrtGetError()==pending); xrtClearError();
    xl6SequenceAdmissionDropV3(&policy); xl6SequenceEndV1(pin);
    assert(xl6ArrayResizeV1(owner,LAYOUT,4));
    xvalue* read=NULL; assert(xl6ArrayReadV1(owner,LAYOUT,3,&read));
    xbytesview bytes={0}; assert(xrtValueGetBytes(read,&bytes) && bytes.Size==8 && xrtValueTypeId(read)==ELEMENT);
    for(size_t i=0;i<bytes.Size;++i) assert(bytes.Data[i]==0);
    xrtValueRelease(read);
    xvalue* clone=xl6ArrayCloneV1(owner,LAYOUT); assert(clone);
    xrtValueRelease(owner); /* schema and bytes outlive the original owner */
    xvalue* erased=xl6ArrayEraseV1(clone,LAYOUT,NULL); assert(erased);
    read=xl6ArrayErasedReadV1(erased,0); assert(read && xrtValueTypeId(read)==ELEMENT);
    assert(!xl6ArrayErasedSetV1(erased,0,wrongSize)); xrtClearError();
    xrtValueRelease(read); xrtValueRelease(erased); xrtValueRelease(clone);
    xrtValueRelease(valid); xrtValueRelease(wrongSize); xrtValueRelease(wrongId);
    owner=xl6SequenceUnionCreateV1(1,OWNER,OPTIONAL,ELEMENT,8,true); assert(owner);
    assert(xl6ArrayResizeV1(owner,OWNER,OPTIONAL,3,sizeof(xvalue*),xrtTypeValue(),2));
    read=NULL; assert(xl6ArrayReadV1(owner,OWNER,OPTIONAL,3,sizeof(xvalue*),xrtTypeValue(),1,&read));
    assert(!read || xrtValueType(read)==XVALUE_NULL); xrtValueRelease(read); xrtValueRelease(owner);
    assert(!xl6SequenceUnionCreateV1(2,OWNER,ELEMENT,ELEMENT,8,false)); xrtClearError();
    assert(!xl6SequenceUnionCreateV1(1,OWNER,ELEMENT,ELEMENT,0,false)); xrtClearError();
    assert(!xl6SequenceUnionCreateV1(1,OWNER,ELEMENT,ELEMENT,8,true)); xrtClearError();
    assert(!xrtGetError());
}
static bool nativePrefix(size_t point)
{
    xl6ProductMemoryBegin(point);
    xvalue* owner=xl6SequenceUnionCreateV1(1,OWNER,ELEMENT,ELEMENT,8,false), *clone=NULL;
    bool success=owner && xl6ArrayResizeV1(owner,LAYOUT,4);
    if(success) { clone=xl6ArrayCloneV1(owner,LAYOUT); success=clone!=NULL; }
    bool hit=xl6ProductMemoryEnd(); xerror* error=xrtTakeError();
    assert(hit ? !success && xrtErrorKind(error)==XERR_MEMORY : success && !error);
    xrtValueRelease(clone); xrtValueRelease(owner); xrtErrorFree(error); xrtClearError();
    xl6ProductMemoryVerify(); return hit;
}
int main(int argc,char** argv)
{
    assert(argc==2 || argc==3);
    if(!strcmp(argv[1],"--native")) {
        xl6ProductMemoryBegin(SIZE_MAX); nativeDomain(); xl6ProductMemoryEnd(); xl6ProductMemoryVerify();
        size_t point=0; while(point<1000 && nativePrefix(point)) ++point; assert(point<1000);
        printf("union native domain and defaults full prefixes=%zu balanced\n",point+1); return 0;
    }
    XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir; assert(xl6IrVerifyModule(module,NULL)); unsigned domains=0;
    for(size_t n=0;n<module->types.count;++n) {
        XL6_IrType* type=&module->types.items[n]; XL6_IrTypeId id=(XL6_IrTypeId)(n+1);
        if(type->sequenceLayout.abi!=XL6_IR_SEQUENCE_LAYOUT_UNION_VALUE_V1) continue;
        assert(xl6IrSequenceConstructionRecipe(module,id,XL6_IR_SEQUENCE_ROLE_ARRAY,NULL));
        assert(type->sequenceLayout.slotAbi==XL6_IR_NATIVE_SLOT_XVALUE_CARRIER &&
            type->sequenceLayout.codecAbi==XL6_IR_SEQUENCE_CODEC_UNION_VALUE_V1 &&
            type->sequenceLayout.transferAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN);
        XL6_IrSequenceLayout saved=type->sequenceLayout;
#define MUTATE(Field,Value) do { type->sequenceLayout.Field=(Value); refused(module,id); type->sequenceLayout=saved; } while(0)
        MUTATE(codecAbi,XL6_IR_SEQUENCE_CODEC_DIRECT); MUTATE(slotAbi,XL6_IR_NATIVE_SLOT_I64);
        MUTATE(transferAbi,XL6_IR_RUNTIME_RESOURCE_PAYLOAD_TRIVIAL_COPY);
        MUTATE(admissionAbi,XL6_VALUE_ADMIT_ANY); MUTATE(baseElementType,0); MUTATE(elementType,0);
        MUTATE(nullable,!saved.nullable); MUTATE(defaultAbi,XL6_IR_SEQUENCE_DEFAULT_FORBIDDEN);
        MUTATE(role,XL6_IR_SEQUENCE_ROLE_STACK); MUTATE(abi,XL6_IR_SEQUENCE_LAYOUT_REFERENCE_V2);
#undef MUTATE
        XL6_IrUnionLayout* layout=(XL6_IrUnionLayout*)xl6IrUnionLayoutGet(module,saved.baseElementType); assert(layout);
        XL6_IrUnionLayout prior=*layout; layout->fieldCount=0; refused(module,id); *layout=prior;
        layout->firstField=SIZE_MAX; refused(module,id); *layout=prior;
        XL6_IrUnionField* field=&module->unionFields[layout->firstField]; XL6_IrTypeId owner=field->ownerType;
        field->ownerType=0; refused(module,id); field->ownerType=owner; ++domains;
    }
    assert(domains>=2 && mutations>=26 && xl6IrVerifyModule(module,NULL));
    XL6_ArtifactRuntimePlan plan={0}; assert(xl6ArtifactRuntimePlanBuild(module,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_UNION_ADMISSION));
    xl6CompileUnitFree(&unit);
    assert(argc==3); xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[2],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    assert(xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_UNION_ADMISSION));
    xl6CompileUnitFree(&unit);
    printf("union collection domains=%u mutations=%u; exact owned recipe verified\n",domains,mutations); return 0;
}
