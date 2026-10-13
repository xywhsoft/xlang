#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/compile/xl6_xrt_trim.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned mutations;
static void refused(XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));XL6_ArtifactRuntimePlan plan={0};
    assert(!xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
    for(unsigned h=0;h<2;++h){XL6_Output out;size_t bytes=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(h?xl6IrCgenEmitHeader(&out,ir,"BAD_NATIVE_PREPARE",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(!bytes);xrtFree(text);xl6OutputDiscard(&out);}
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==5 && (!strcmp(argv[3],"loan") || !strcmp(argv[3],"copy")));
    XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    int ok=xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags);
    if(!ok)xl6DiagPrintText(&unit.parse.diags,argv[1]);
    assert(ok);XL6_IrModule* ir=&unit.ir;
    assert(ir->generatorMachineCount==2 && ir->generatorSuspensionCount==2 && xl6IrVerifyModule(ir,NULL));
    XL6_IrGeneratorMachine* machine=&ir->generatorMachines[0];XL6_IrFunction* fn=&ir->functions[machine->function];
    XL6_IrGeneratorSuspension* s=&ir->generatorSuspensions[0];XL6_IrValueId loan=0;XL6_SymbolId owner=0;size_t marker=SIZE_MAX;
    XL6_IrTypeId type=0;
    for(size_t i=fn->firstInstruction;i<s->yieldInstruction;++i){
        XL6_IrInstruction* x=&ir->instructions[i];
        if(x->kind==XL6_IR_INSTR_LOAD && xl6IrNativeValuePlanForType(xl6IrTypeGet(&ir->types,x->type)) && !loan)
            {loan=x->result;owner=x->targetSymbol;type=x->type;}
        if(x->kind==XL6_IR_INSTR_CONST && ir->instructions[i+1].kind==XL6_IR_INSTR_DISCARD)marker=i;
    }
    assert(loan && owner && marker!=SIZE_MAX);
    /* Reuse a private parameter observation after yield. The source fixture
     * ordinarily reloads it. This valid closed-IR edit creates a real SSA
     * loan spanning suspension; no frame metadata is directly fabricated. */
    unsigned aliases=0;
    for(size_t i=s->yieldInstruction+1;i<fn->firstInstruction+fn->instructionCount;++i){
        XL6_IrInstruction* x=&ir->instructions[i];
        if(x->kind!=XL6_IR_INSTR_LOAD || x->targetSymbol!=owner || aliases)continue;
        ir->operands=xrtRealloc(ir->operands,(ir->operandCount+1)*sizeof(*ir->operands));assert(ir->operands);
        ir->operandCapacity=ir->operandCount+1;x->firstOperand=ir->operandCount++;ir->operands[x->firstOperand]=loan;
        x->operandCount=1;x->kind=XL6_IR_INSTR_VALUE;x->targetSymbol=0;++aliases;
    }
    assert(aliases==1);
    if(!strcmp(argv[3],"copy")){
        XL6_IrInstruction* address=&ir->instructions[marker];address->kind=XL6_IR_INSTR_STORAGE_ADDRESS;
        address->type=xl6IrTypeInternStorageRef(ir,type);address->targetSymbol=owner;
        xrtFree(address->immediateText);address->immediateText=NULL;address->elementIndex=SIZE_MAX;
        ir->instructions[marker+1].type=address->type;ir->stats.typeCount=ir->types.count;
    }
    assert(xl6IrBuildGeneratorMachines(ir,&unit.parse.diags));
    ok=xl6IrVerifyModule(ir,&unit.parse.diags);if(!ok)xl6DiagPrintText(&unit.parse.diags,argv[1]);assert(ok);
    XL6_IrContinuationFrameSlot* spill=NULL;
    for(size_t i=0;i<ir->generatorFrameSlotCount;++i)
        if(ir->generatorFrameSlots[i].residence==XL6_IR_ASYNC_FRAME_RESIDENCE_SSA_SPILL && ir->generatorFrameSlots[i].value==loan)spill=&ir->generatorFrameSlots[i];
    assert(spill && spill->transferAbi==(!strcmp(argv[3],"copy")?XL6_IR_ASYNC_FRAME_TRANSFER_COPY_NATIVE_VALUE:XL6_IR_ASYNC_FRAME_TRANSFER_BORROW_STORAGE));
    s=&ir->generatorSuspensions[0];size_t saved=s->prepareInstruction;s->prepareInstruction=s->yieldInstruction;refused(ir);s->prepareInstruction=saved;
    XL6_IrInstruction* prepare=&ir->instructions[saved];XL6_IrInstruction original=*prepare;
    prepare->payloadType=0;refused(ir);*prepare=original;
    XL6_IrInstruction* guard=prepare+1;XL6_IrInstruction originalGuard=*guard;
    guard->trueTarget=originalGuard.falseTarget;guard->falseTarget=originalGuard.trueTarget;refused(ir);*guard=originalGuard;
    assert(xl6IrVerifyModule(ir,NULL));XL6_ArtifactRuntimePlan plan={0};assert(xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
    XL6_Output out;assert(xl6OutputOpenFile(&out,argv[2]));
    ok=xl6IrCgenEmitC(&out,ir,&unit.parse.diags);if(!ok)xl6DiagPrintText(&unit.parse.diags,argv[1]);assert(ok);xl6OutputClose(&out);
    char* native=NULL;assert(xl6XrtTrimBuildImplementationSourceFromArtifact(&unit.importDeps,0,plan.nativeModules,plan.nativeModuleCount,&native));
    assert(xl6OutputOpenFile(&out,argv[4]));xl6OutputPuts("#define XRT_MODULE_MEMORY_DEBUG\n",&out);xl6OutputPuts(native,&out);
    assert(!xl6OutputHasFailed(&out));xl6OutputClose(&out);xrtFree(native);
    for(size_t i=0;i<plan.sourceCount;++i)printf("source:%s\n",plan.sources[i]);
    printf("generator SSA mode=%s verified=1 mutations=%u\n",argv[3],mutations);xl6CompileUnitFree(&unit);return 0;
}
