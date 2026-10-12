#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations,producers,yields,takes;
static void refused(XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));
    XL6_ArtifactRuntimePlan plan={0};assert(!xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
    for(unsigned header=0;header<2;++header){
        XL6_Output out;size_t bytes=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header?xl6IrCgenEmitHeader(&out,ir,"NATIVE_ITEM_REFUSED",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(!bytes);xrtFree(text);xl6OutputDiscard(&out);
    }++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));
    for(size_t m=0;m<ir->generatorMachineCount;++m){
        XL6_IrGeneratorMachine* machine=&ir->generatorMachines[m];
        if(machine->yieldAbi!=XL6_IR_GENERATOR_YIELD_MOVE_NATIVE_VALUE)continue;
        assert(xl6IrNativeValuePlanForType(xl6IrTypeGet(&ir->types,machine->itemType)) && machine->itemTraceAbi==XL6_IR_FRAME_TRACE_NONE);
        XL6_IrGeneratorMachine saved=*machine;++producers;
#define BAD(Field,Value) do{machine->Field=(Value);refused(ir);*machine=saved;}while(0)
        BAD(yieldAbi,XL6_IR_GENERATOR_YIELD_COPY_RAW_POINTER);BAD(itemTraceAbi,XL6_IR_FRAME_TRACE_XVALUE);BAD(itemType,0);
#undef BAD
    }
    for(size_t i=0;i<ir->instructionCount;++i){
        XL6_IrInstruction* ins=&ir->instructions[i];
        if(ins->generatorYieldAbi!=XL6_IR_GENERATOR_YIELD_MOVE_NATIVE_VALUE)continue;
        XL6_IrInstruction saved=*ins;
        if(ins->kind==XL6_IR_INSTR_YIELD)++yields;
        else if(ins->kind==XL6_IR_INSTR_GENERATOR_ITEM_TAKE){
            ++takes;ins->kind=XL6_IR_INSTR_GENERATOR_ITEM_GET;refused(ir);*ins=saved;
        }else assert(false);
        ins->generatorYieldAbi=XL6_IR_GENERATOR_YIELD_COPY_RAW_POINTER;refused(ir);*ins=saved;
        ins->type=0;refused(ir);*ins=saved;
        assert(xl6IrVerifyModule(ir,NULL));
    }
    assert(producers && yields && takes);
    XL6_ArtifactRuntimePlan plan={0};assert(xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SLOT_MAP_KEY_CAPTURE));
    printf("native generator producers=%u yields=%u takes=%u mutations=%u\n",producers,yields,takes,mutations);
    xl6CompileUnitFree(&unit);return 0;
}
