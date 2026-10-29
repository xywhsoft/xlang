#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations,producers,awaits;
static void refused(XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));XL6_ArtifactRuntimePlan plan={0};assert(!xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
    for(unsigned header=0;header<2;++header) {
        XL6_Output out;size_t bytes=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_NATIVE_FUTURE",NULL) : xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(!bytes);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));
    for(size_t i=0;i<ir->callTargetCount;++i) {
        XL6_IrCallTarget* target=&ir->callTargets[i];
        if(target->asyncResultAbi!=XL6_IR_ASYNC_RESULT_OWNED_NATIVE_VALUE)continue;
        ++producers;XL6_IrCallTarget saved=*target;
        target->asyncResultAbi=XL6_IR_ASYNC_RESULT_COPY_I64;refused(ir);*target=saved;
        target->workerReturnType=XL6_IR_TYPE_INVALID;refused(ir);*target=saved;
    }
    for(size_t i=0;i<ir->instructionCount;++i) {
        XL6_IrInstruction* instruction=&ir->instructions[i];
        if(instruction->kind!=XL6_IR_INSTR_AWAIT || instruction->asyncResultAbi!=XL6_IR_ASYNC_RESULT_OWNED_NATIVE_VALUE)continue;
        ++awaits;XL6_IrInstruction saved=*instruction;
        instruction->asyncResultAbi=XL6_IR_ASYNC_RESULT_RETAINED_XVALUE;refused(ir);*instruction=saved;
        instruction->awaitOutcomeAbi=XL6_IR_AWAIT_OUTCOME_INVALID;refused(ir);*instruction=saved;
        instruction->type=XL6_IR_TYPE_INVALID;refused(ir);*instruction=saved;
    }
    assert(producers==4 && awaits>=6 && xl6IrVerifyModule(ir,NULL));
    XL6_ArtifactRuntimePlan plan={0};assert(xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_NATIVE_FUTURE_PAYLOAD));
    printf("native Future producers=%u awaits=%u mutations=%u\n",producers,awaits,mutations);
    xl6CompileUnitFree(&unit);return 0;
}
