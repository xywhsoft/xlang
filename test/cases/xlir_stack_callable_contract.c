#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));XL6_ArtifactRuntimePlan plan={0};
    assert(!xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
    for(unsigned header=0;header<2;++header){XL6_Output out;size_t bytes=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header?xl6IrCgenEmitHeader(&out,ir,"BAD_CALLABLE_STACK",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(!bytes);xrtFree(text);xl6OutputDiscard(&out);}
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
    XL6_IrModule* ir=&unit.ir;assert(xl6IrLowerModule(ir,&unit.parse.diags) && xl6IrVerifyModule(ir,NULL));
    unsigned stacks=0,calls=0;
    for(size_t i=0;i<ir->types.count;++i){XL6_IrType* type=&ir->types.items[i];
        if(type->runtimeResourceAbi!=XL6_IR_RUNTIME_RESOURCE_STACK || type->sequenceLayout.abi!=XL6_IR_SEQUENCE_LAYOUT_CALLABLE_REFERENCE_V1)continue;
        ++stacks;assert(xl6IrStackConstructionContract(ir,(XL6_IrTypeId)(i+1),NULL));
        XL6_IrSequenceLayout saved=type->sequenceLayout;
#define BAD_LAYOUT(F,V) do{type->sequenceLayout.F=(V);refused(ir);type->sequenceLayout=saved;}while(0)
        BAD_LAYOUT(codecAbi,XL6_IR_SEQUENCE_CODEC_DIRECT);BAD_LAYOUT(slotAbi,XL6_IR_NATIVE_SLOT_FUTURE_PTR);
        BAD_LAYOUT(transferAbi,XL6_IR_RUNTIME_RESOURCE_PAYLOAD_FUTURE_REF);BAD_LAYOUT(nullable,!saved.nullable);
#undef BAD_LAYOUT
    }
    for(size_t i=0;i<ir->instructionCount;++i){XL6_IrInstruction* ins=&ir->instructions[i];
        const XL6_IrType* payload=xl6IrTypeGet(&ir->types,ins->payloadType);
        if(ins->runtimeOp<XL6_IR_RUNTIME_STACK_CREATE || ins->runtimeOp>XL6_IR_RUNTIME_STACK_CAPACITY ||
            !payload || payload->cAbi!=XL6_IR_C_ABI_CLOSURE_PTR)continue;
        ++calls;XL6_IrInstruction saved=*ins;
        ins->runtimeResourcePayloadAbi=XL6_IR_RUNTIME_RESOURCE_PAYLOAD_FUTURE_REF;refused(ir);*ins=saved;
        ins->payloadType=XL6_IR_TYPE_INVALID;refused(ir);*ins=saved;
    }
    assert(stacks==2 && calls>=20 && xl6IrVerifyModule(ir,NULL));
    XL6_ArtifactRuntimePlan plan={0};assert(xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_STACK_CALLABLE));
    printf("callable Stack domains=%u calls=%u mutations=%u\n",stacks,calls,mutations);xl6CompileUnitFree(&unit);return 0;
}
