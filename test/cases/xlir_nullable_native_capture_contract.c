#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned mutations;
static void refused(XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));XL6_ArtifactRuntimePlan plan={0};
    assert(!xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
    for(unsigned header=0;header<2;++header){XL6_Output out;size_t bytes=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header?xl6IrCgenEmitHeader(&out,ir,"BAD_OPTIONAL_CAPTURE",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(!bytes);xrtFree(text);xl6OutputDiscard(&out);}
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
    XL6_IrModule* ir=&unit.ir;assert(xl6IrLowerModule(ir,&unit.parse.diags) && xl6IrVerifyModule(ir,NULL));
    assert(ir->abi.closureStorageAbi==XL6_IR_CLOSURE_STORAGE_RESIDENT_LAYOUT_V3);
    ir->abi.closureStorageAbi=XL6_IR_CLOSURE_STORAGE_RESIDENT_LAYOUT_V2;refused(ir);
    ir->abi.closureStorageAbi=XL6_IR_CLOSURE_STORAGE_RESIDENT_LAYOUT_V3;
    unsigned types=0,captures=0,awaits=0,lexical=0;
    for(size_t i=0;i<ir->types.count;++i){XL6_IrType* type=&ir->types.items[i];
        const XL6_IrNativeValuePlan* plan=xl6IrNullableNativeValuePlan(&ir->types,type);if(!plan)continue;
        ++types;assert(type->typeValueId==XL6_NULLABLE_SLOT_MAP_KEY_LANGUAGE_VALUE_ID);
        assert(plan->captureHeader && !strcmp(plan->captureProfileSymbol,"xl6NullableSlotMapKeyCaptureProfileV1Get"));
        XL6_IrType saved=*type;type->ownershipAbi=XL6_IR_OWNERSHIP_ABI_TRIVIAL;refused(ir);*type=saved;
        type->nativeValueAbi=XL6_IR_NATIVE_VALUE_SLOT_MAP_KEY_V1;refused(ir);*type=saved;
    }
    for(size_t i=0;i<ir->callbackCaptureCount;++i){XL6_IrCallbackCapture* capture=&ir->callbackCaptures[i];
        if(!xl6IrNullableNativeValuePlan(&ir->types,xl6IrTypeGet(&ir->types,capture->type)))continue;
        ++captures;assert(capture->transferAbi==XL6_IR_ASYNC_FRAME_TRANSFER_COPY_NATIVE_VALUE && capture->destroyAbi==XL6_IR_ASYNC_FRAME_DESTROY_NATIVE_VALUE);
        XL6_IrCallbackCapture saved=*capture;capture->destroyAbi=XL6_IR_ASYNC_FRAME_DESTROY_NONE;refused(ir);*capture=saved;
        capture->transferAbi=XL6_IR_ASYNC_FRAME_TRANSFER_COPY_VALUE;refused(ir);*capture=saved;
    }
    for(size_t i=0;i<ir->closureCaptureCount;++i){XL6_IrClosureCapture* capture=&ir->closureCaptures[i];
        if(!xl6IrNullableNativeValuePlan(&ir->types,xl6IrTypeGet(&ir->types,capture->type)))continue;
        ++lexical;assert(capture->abi==XL6_IR_CLOSURE_CAPTURE_COPY_NATIVE_VALUE);
        XL6_IrClosureCapture saved=*capture;capture->abi=XL6_IR_CLOSURE_CAPTURE_COPY_VALUE;refused(ir);*capture=saved;
    }
    for(size_t i=0;i<ir->instructionCount;++i){XL6_IrInstruction* ins=&ir->instructions[i];
        if(ins->kind!=XL6_IR_INSTR_AWAIT || !xl6IrNullableNativeValuePlan(&ir->types,xl6IrTypeGet(&ir->types,ins->type)))continue;
        ++awaits;XL6_IrInstruction saved=*ins;ins->asyncResultAbi=XL6_IR_ASYNC_RESULT_COPY_TAGGED;refused(ir);*ins=saved;
    }
    assert(types==1 && captures && awaits && lexical && xl6IrVerifyModule(ir,NULL));
    XL6_ArtifactRuntimePlan plan={0};assert(xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_NULLABLE_SLOT_MAP_KEY_CAPTURE));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_CLOSURE_CAPTURE_V3));
    XL6_Output out;size_t bytes=0;assert(xl6OutputOpenMemory(&out) && xl6IrCgenEmitC(&out,ir,NULL));
    char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(bytes && strstr(text,"xl6NullableSlotMapKeyCaptureProfileV1Get"));xrtFree(text);xl6OutputDiscard(&out);
    printf("nullable native captures types=%u lexical=%u callback-slots=%u awaits=%u mutations=%u\n",types,lexical,captures,awaits,mutations);
    xl6CompileUnitFree(&unit);return 0;
}
