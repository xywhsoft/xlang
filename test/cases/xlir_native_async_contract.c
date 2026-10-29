#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations,owners,copies;
static void refused(XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));XL6_ArtifactRuntimePlan plan={0};assert(!xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
    for(unsigned h=0;h<2;++h){XL6_Output out;size_t bytes=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(h?xl6IrCgenEmitHeader(&out,ir,"BAD_NATIVE_AWAIT",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(!bytes);xrtFree(text);xl6OutputDiscard(&out);}++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));
    assert(ir->abi.asyncStorageAbi==XL6_IR_ASYNC_STORAGE_RESIDENT_CLOSED_FRAME_V3);
    ir->abi.asyncStorageAbi=XL6_IR_ASYNC_STORAGE_RESIDENT_CLOSED_FRAME_V2;refused(ir);ir->abi.asyncStorageAbi=XL6_IR_ASYNC_STORAGE_RESIDENT_CLOSED_FRAME_V3;
    for(size_t i=0;i<ir->asyncMachineFrameSlotCount;++i){XL6_IrContinuationFrameSlot* f=&ir->asyncMachineFrameSlots[i];
        if(f->destroyAbi!=XL6_IR_ASYNC_FRAME_DESTROY_NATIVE_VALUE)continue;
        assert(f->traceAbi==XL6_IR_FRAME_TRACE_NONE && f->initBit!=SIZE_MAX);XL6_IrContinuationFrameSlot saved=*f;++owners;
        if(f->transferAbi==XL6_IR_ASYNC_FRAME_TRANSFER_COPY_NATIVE_VALUE)++copies;
#define BAD(F,V) do{f->F=(V);refused(ir);*f=saved;}while(0)
        BAD(type,0);BAD(initBit,SIZE_MAX);BAD(destroyAbi,XL6_IR_ASYNC_FRAME_DESTROY_NONE);BAD(traceAbi,XL6_IR_FRAME_TRACE_XVALUE);BAD(transferAbi,XL6_IR_ASYNC_FRAME_TRANSFER_COPY_VALUE);
#undef BAD
    }
    assert(owners && copies);assert(xl6IrVerifyModule(ir,NULL));XL6_ArtifactRuntimePlan plan={0};assert(xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SLOT_MAP_KEY_CAPTURE));
    printf("native async owners=%u copies=%u mutations=%u\n",owners,copies,mutations);xl6CompileUnitFree(&unit);return 0;
}
