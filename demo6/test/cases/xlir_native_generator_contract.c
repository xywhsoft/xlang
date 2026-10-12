#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations,owners,borrowCopies;
static void refused(XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));
    XL6_ArtifactRuntimePlan plan={0};assert(!xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
    for(unsigned header=0;header<2;++header){
        XL6_Output out;size_t bytes=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header?xl6IrCgenEmitHeader(&out,ir,"NATIVE_GENERATOR_REFUSED",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(!bytes);xrtFree(text);xl6OutputDiscard(&out);
    }++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));
    assert(ir->abi.generatorStorageAbi==XL6_IR_GENERATOR_STORAGE_RESIDENT_CLOSED_FRAME_V3);
    ir->abi.generatorStorageAbi=XL6_IR_GENERATOR_STORAGE_RESIDENT_CLOSED_FRAME_V2;refused(ir);
    ir->abi.generatorStorageAbi=XL6_IR_GENERATOR_STORAGE_RESIDENT_CLOSED_FRAME_V3;
    for(size_t i=0;i<ir->generatorFrameSlotCount;++i){
        XL6_IrContinuationFrameSlot* f=&ir->generatorFrameSlots[i];
        if(f->destroyAbi!=XL6_IR_ASYNC_FRAME_DESTROY_NATIVE_VALUE)continue;
        XL6_IrContinuationFrameSlot saved=*f;XL6_IrType* type=&ir->types.items[f->type-1];XL6_IrType savedType=*type;
        const XL6_IrNativeValuePlan* native=xl6IrNativeValuePlanForType(type);assert(native && native->captureProfileSymbol && native->captureHeader);
        assert(f->traceAbi==XL6_IR_FRAME_TRACE_NONE && f->initBit!=SIZE_MAX);++owners;
        if(f->transferAbi==XL6_IR_ASYNC_FRAME_TRANSFER_COPY_NATIVE_VALUE)++borrowCopies;
#define BAD(Field,Value) do{f->Field=(Value);refused(ir);*f=saved;}while(0)
        BAD(destroyAbi,XL6_IR_ASYNC_FRAME_DESTROY_NONE);BAD(traceAbi,XL6_IR_FRAME_TRACE_XVALUE);
        BAD(initBit,SIZE_MAX);BAD(type,0);BAD(transferAbi,XL6_IR_ASYNC_FRAME_TRANSFER_COPY_VALUE);
        BAD(residence,XL6_IR_ASYNC_FRAME_RESIDENCE_INVALID);
#undef BAD
        type->nativeValueAbi=XL6_IR_NATIVE_VALUE_NONE;refused(ir);*type=savedType;
        type->cAbi=XL6_IR_C_ABI_RAW_POINTER;refused(ir);*type=savedType;
        assert(xl6IrVerifyModule(ir,NULL));
    }
    assert(owners>=3 && borrowCopies);
    XL6_ArtifactRuntimePlan plan={0};assert(xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SLOT_MAP_KEY_CAPTURE));
    printf("native frame owners=%u borrowed-copies=%u mutations=%u\n",owners,borrowCopies,mutations);xl6CompileUnitFree(&unit);return 0;
}
