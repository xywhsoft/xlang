/* Exercise closed Lowered IR, not a source byref feature. Taking an unused,
 * exact internal storage address makes the loan planner conservative, forcing
 * independent native SSA acquisition. The instruction/CFG verifier is never
 * bypassed; all continuation metadata is rebuilt from the edited instruction. */
#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/cgen/xl6_ir_cgen.h"
#include "src/compile/xl6_xrt_trim.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned mutations;
static void refused(XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));
    XL6_ArtifactRuntimePlan plan={0};
    assert(!xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
    for(unsigned header=0;header<2;++header){
        XL6_Output out;size_t bytes=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header?xl6IrCgenEmitHeader(&out,ir,"BAD_NATIVE_SSA",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(!bytes);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==5 && (!strcmp(argv[3],"loan") || !strcmp(argv[3],"copy")));
    XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    int loaded=xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags);
    if(!loaded){xl6DiagPrintText(&unit.parse.diags,argv[1]);fflush(stdout);}assert(loaded);
    XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));
    XL6_IrValueId candidate=0;XL6_IrTypeId type=0;
    for(size_t i=0;i<ir->asyncMachineFrameSlotCount;++i){
        const XL6_IrContinuationFrameSlot* f=&ir->asyncMachineFrameSlots[i];
        if(f->residence==XL6_IR_ASYNC_FRAME_RESIDENCE_SSA_SPILL &&
           f->transferAbi==XL6_IR_ASYNC_FRAME_TRANSFER_BORROW_STORAGE &&
           xl6IrNativeValuePlanForType(xl6IrTypeGet(&ir->types,f->type))){
            assert(!candidate);candidate=f->value;type=f->type;
        }
    }
    assert(candidate);
    if(!strcmp(argv[3],"copy")){
        size_t at=SIZE_MAX,marker=SIZE_MAX;XL6_IrFunction* fn=NULL;
        for(size_t i=0;i<ir->instructionCount;++i)if(ir->instructions[i].result==candidate)at=i;
        assert(at!=SIZE_MAX && ir->instructions[at].kind==XL6_IR_INSTR_LOAD);
        for(size_t i=0;i<ir->functionCount;++i)
            if(at>=ir->functions[i].firstInstruction && at<ir->functions[i].firstInstruction+ir->functions[i].instructionCount) fn=&ir->functions[i];
        assert(fn);
        for(size_t i=fn->firstInstruction;i<at;++i)
            if(ir->instructions[i].kind==XL6_IR_INSTR_CONST &&
               ir->instructions[i+1].kind==XL6_IR_INSTR_DISCARD &&
               ir->instructions[i+1].operandCount==1 &&
               ir->operands[ir->instructions[i+1].firstOperand]==ir->instructions[i].result) marker=i;
        assert(marker!=SIZE_MAX);
        XL6_IrInstruction* address=&ir->instructions[marker];
        address->kind=XL6_IR_INSTR_STORAGE_ADDRESS;address->type=xl6IrTypeInternStorageRef(ir,type);
        assert(address->type);address->targetSymbol=ir->instructions[at].targetSymbol;
        xrtFree(address->immediateText);address->immediateText=NULL;address->elementIndex=SIZE_MAX;
        ir->instructions[marker+1].type=address->type;ir->stats.typeCount=ir->types.count;
        assert(xl6IrBuildAsyncMachines(ir,&unit.parse.diags));
    }
    if(!xl6IrVerifyModule(ir,&unit.parse.diags)){xl6DiagPrintText(&unit.parse.diags,argv[1]);assert(0);}
    XL6_IrContinuationFrameSlot* selected=NULL;
    for(size_t i=0;i<ir->asyncMachineFrameSlotCount;++i)
        if(ir->asyncMachineFrameSlots[i].residence==XL6_IR_ASYNC_FRAME_RESIDENCE_SSA_SPILL &&
           ir->asyncMachineFrameSlots[i].value==candidate) selected=&ir->asyncMachineFrameSlots[i];
    assert(selected);
    if(!strcmp(argv[3],"copy")){
        assert(selected->transferAbi==XL6_IR_ASYNC_FRAME_TRANSFER_COPY_NATIVE_VALUE &&
            selected->destroyAbi==XL6_IR_ASYNC_FRAME_DESTROY_NATIVE_VALUE && selected->initBit!=SIZE_MAX &&
            selected->borrowFrameSlot==SIZE_MAX);
        XL6_IrContinuationFrameSlot saved=*selected;
#define BAD(F,V) do{selected->F=(V);refused(ir);*selected=saved;}while(0)
        BAD(transferAbi,XL6_IR_ASYNC_FRAME_TRANSFER_COPY_VALUE);
        BAD(destroyAbi,XL6_IR_ASYNC_FRAME_DESTROY_NONE);
        BAD(initBit,SIZE_MAX);
        BAD(borrowFrameSlot,0);
#undef BAD
    }else assert(selected->transferAbi==XL6_IR_ASYNC_FRAME_TRANSFER_BORROW_STORAGE && selected->initBit==SIZE_MAX);
    XL6_ArtifactRuntimePlan plan={0};assert(xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SLOT_MAP_KEY_CAPTURE));
    XL6_Output out;assert(xl6OutputOpenFile(&out,argv[2]));
    int emitted=xl6IrCgenEmitC(&out,ir,&unit.parse.diags);
    if(!emitted)xl6DiagPrintText(&unit.parse.diags,argv[1]);
    assert(emitted);xl6OutputClose(&out);
    char* native=NULL;
    assert(xl6XrtTrimBuildImplementationSourceFromArtifact(&unit.importDeps,0,plan.nativeModules,plan.nativeModuleCount,&native));
    assert(xl6OutputOpenFile(&out,argv[4]));
    xl6OutputPuts("#define XRT_MODULE_MEMORY_DEBUG\n",&out);xl6OutputPuts(native,&out);
    assert(!xl6OutputHasFailed(&out));xl6OutputClose(&out);xrtFree(native);
    for(size_t i=0;i<plan.sourceCount;++i)printf("source:%s\n",plan.sources[i]);
    printf("native SSA mode=%s verified=1 mutations=%u\n",argv[3],mutations);
    xl6CompileUnitFree(&unit);return 0;
}
