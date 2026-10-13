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
    for(unsigned header=0;header<2;++header) {
        XL6_Output out;size_t bytes=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header?xl6IrCgenEmitHeader(&out,ir,"BAD_WAIT_QUEUE",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(!bytes);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1));XL6_IrModule* ir=&unit.ir;
    if(!xl6IrLowerModule(ir,&unit.parse.diags)){xl6DiagPrintText(&unit.parse.diags,argv[1]);return 1;}
    assert(xl6IrVerifyModule(ir,NULL));unsigned calls=0,receives=0;
    for(size_t i=0;i<ir->instructionCount;++i) {
        XL6_IrInstruction* ins=&ir->instructions[i];XL6_IrTypeId ownerId=xl6IrConcurrentQueueInstructionType(ir,ins);
        const XL6_IrType* owner=xl6IrTypeGet(&ir->types,ownerId);
        if(!owner || owner->runtimeResourceAbi!=XL6_IR_RUNTIME_RESOURCE_WAIT_QUEUE)continue;
        assert(xl6IrConcurrentQueueInstructionCompatible(ir,ins));
        assert(xl6IrCallInstructionErrorAbi(ir,ins)==XL6_IR_ERROR_ABI_MAY_RAISE &&
            xl6IrCallInstructionErrorPhysicalAbi(ir,ins)==XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT);
        XL6_IrInstruction saved=*ins;
#define BAD(F,V) do{ins->F=(V);refused(ir);*ins=saved;}while(0)
        BAD(payloadType,ownerId);BAD(runtimeResourcePayloadAbi,XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID);
        BAD(operandCount,3);BAD(targetSymbol,UINT32_MAX);
        if(xl6IrWaitQueueReceiveRuntime(ins->runtimeOp)) {
            const XL6_IrType* result=xl6IrTypeGet(&ir->types,ins->type);assert(result && result->kind==XL6_IR_TYPE_TUPLE);
            assert(xl6IrValueOwnershipGet(ir,ins->result)==(result->ownershipAbi==XL6_IR_OWNERSHIP_ABI_MANAGED?
                XL6_IR_OWNERSHIP_MOVE:XL6_IR_OWNERSHIP_COPY));
            BAD(runtimeOp,XL6_IR_RUNTIME_SPECIAL_QUEUE_POP_INTO);++receives;
        }
#undef BAD
        ++calls;
    }
    assert(calls>0 && receives>0 && xl6IrVerifyModule(ir,NULL));XL6_ArtifactRuntimePlan plan={0};
    assert(xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_WAIT_QUEUE_NATIVE));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_TYPED_CHANNEL));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_TYPED_QUEUE_MPMC));
    printf("WaitQueue closed calls=%u receives=%u mutations=%u\n",calls,receives,mutations);
    xl6CompileUnitFree(&unit);return 0;
}
