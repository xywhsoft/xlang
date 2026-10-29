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
        assert(!(header?xl6IrCgenEmitHeader(&out,ir,"BAD_NATIVE_CELL",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(!bytes);xrtFree(text);xl6OutputDiscard(&out);}
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
    XL6_IrModule* ir=&unit.ir;assert(xl6IrLowerModule(ir,&unit.parse.diags) && xl6IrVerifyModule(ir,NULL));
    assert(ir->abi.cellStorageAbi==XL6_IR_CELL_STORAGE_RESIDENT_NATIVE_LEAF_V2);
    ir->abi.cellStorageAbi=XL6_IR_CELL_STORAGE_RESIDENT_TRANSFER_V1;refused(ir);
    ir->abi.cellStorageAbi=XL6_IR_CELL_STORAGE_RESIDENT_NATIVE_LEAF_V2;
    unsigned captures=0,exchanges=0;
    for(size_t i=0;i<ir->closureCaptureCount;++i){XL6_IrClosureCapture* c=&ir->closureCaptures[i];
        if(c->abi!=XL6_IR_CLOSURE_CAPTURE_RETAIN_CELL || !xl6IrNativeValuePlanForType(xl6IrTypeGet(&ir->types,c->type)))continue;
        ++captures;XL6_IrClosureCapture saved=*c;c->abi=XL6_IR_CLOSURE_CAPTURE_COPY_NATIVE_VALUE;refused(ir);*c=saved;
    }
    for(size_t i=0;i<ir->instructionCount;++i){XL6_IrInstruction* ins=&ir->instructions[i];
        if(ins->kind!=XL6_IR_INSTR_CELL_EXCHANGE || !xl6IrNativeValuePlanForType(xl6IrTypeGet(&ir->types,ins->type)))continue;
        ++exchanges;XL6_IrInstruction saved=*ins;ins->kind=XL6_IR_INSTR_CELL_STORE;refused(ir);*ins=saved;
    }
    assert(captures>=4 && exchanges>=2 && xl6IrVerifyModule(ir,NULL));
    XL6_ArtifactRuntimePlan plan={0};assert(xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_CELL_NATIVE));
    printf("native cells captures=%u exchanges=%u mutations=%u\n",captures,exchanges,mutations);xl6CompileUnitFree(&unit);return 0;
}
