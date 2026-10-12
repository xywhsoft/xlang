#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations,arguments,parameters;
static void refused(XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));XL6_ArtifactRuntimePlan plan={0};assert(!xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
    for(unsigned header=0;header<2;++header) {
        XL6_Output out;size_t bytes=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_NATIVE_CAPTURE",NULL) : xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(!bytes);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));
    assert(ir->abi.taskStorageAbi==XL6_IR_TASK_STORAGE_RESIDENT_LAYOUT_FRAME_POOL_V3);
    ir->abi.taskStorageAbi=XL6_IR_TASK_STORAGE_RESIDENT_LAYOUT_FRAME_POOL_V2;refused(ir);
    ir->abi.taskStorageAbi=XL6_IR_TASK_STORAGE_RESIDENT_LAYOUT_FRAME_POOL_V3;
    for(size_t i=0;i<ir->callbackBoundaryCount;++i) {
        XL6_IrCallbackBoundary* boundary=&ir->callbackBoundaries[i];XL6_IrCallbackStorageAbi saved=boundary->storageAbi;
        boundary->storageAbi=boundary->transportAbi==XL6_IR_CALLBACK_TRANSPORT_TASK_POOL ?
            XL6_IR_CALLBACK_STORAGE_RESIDENT_LAYOUT_TASK_FRAME_V2 : XL6_IR_CALLBACK_STORAGE_RESIDENT_LAYOUT_FUTURE_FRAME_V2;
        refused(ir);boundary->storageAbi=saved;
    }
    for(size_t i=0;i<ir->callbackCaptureCount;++i) {
        XL6_IrCallbackCapture* capture=&ir->callbackCaptures[i];
        if(capture->destroyAbi!=XL6_IR_ASYNC_FRAME_DESTROY_NATIVE_VALUE)continue;
        assert(capture->transferAbi==XL6_IR_ASYNC_FRAME_TRANSFER_COPY_NATIVE_VALUE && capture->traceAbi==XL6_IR_FRAME_TRACE_NONE);
        ++arguments;XL6_IrCallbackCapture saved=*capture;
        capture->transferAbi=XL6_IR_ASYNC_FRAME_TRANSFER_COPY_VALUE;refused(ir);*capture=saved;
        capture->destroyAbi=XL6_IR_ASYNC_FRAME_DESTROY_NONE;refused(ir);*capture=saved;
        capture->traceAbi=XL6_IR_FRAME_TRACE_XVALUE;refused(ir);*capture=saved;
        capture->type=XL6_IR_TYPE_INVALID;refused(ir);*capture=saved;
    }
    for(size_t i=0;i<ir->callTargetCount;++i) {
        XL6_IrCallTarget* target=&ir->callTargets[i];
        if(target->kind!=XL6_IR_CALL_TARGET_INTERNAL_ASYNC)continue;
        for(size_t p=0;p<target->parameterCount;++p) {
            if(target->asyncCaptureDestroyAbis[p]!=XL6_IR_ASYNC_FRAME_DESTROY_NATIVE_VALUE)continue;
            assert(target->asyncMachine==SIZE_MAX);++parameters;
            target->asyncCaptureAbis[p]=XL6_IR_ASYNC_CAPTURE_COPY_VALUE;refused(ir);
            target->asyncCaptureAbis[p]=XL6_IR_ASYNC_CAPTURE_COPY_NATIVE_VALUE;
        }
    }
    assert(arguments>=3 && parameters==1 && xl6IrVerifyModule(ir,NULL));
    XL6_ArtifactRuntimePlan plan={0};assert(xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SLOT_MAP_KEY_CAPTURE));
    printf("native callback arguments=%u parameters=%u mutations=%u\n",arguments,parameters,mutations);
    xl6CompileUnitFree(&unit);return 0;
}
