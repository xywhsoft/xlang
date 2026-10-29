#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static size_t mutations;
static void reject(XL6_IrModule* ir)
{
    XL6_DiagList diagnostics = {0};
    assert(!xl6IrVerifyModule(ir, &diagnostics) && diagnostics.count);
    for (unsigned header = 0; header < 2; ++header) {
        XL6_Output output; size_t size;
        assert(xl6OutputOpenMemory(&output));
        assert(!(header ? xl6IrCgenEmitHeader(&output, ir, "BAD_FUNC_SLOT", &diagnostics)
                        : xl6IrCgenEmitC(&output, ir, &diagnostics)));
        char* text = xl6OutputTakeMemoryText(&output, &size); assert(!size); xrtFree(text);
    }
    xl6DiagFree(&diagnostics); ++mutations;
}
int main(int argc, char** argv)
{
    assert(argc == 2); XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    if (!xl6CompileUnitLoadFile(&unit, argv[1], 1) ||
        !xl6IrLowerModule(&unit.ir, &unit.parse.diags) ||
        !xl6IrVerifyModule(&unit.ir, &unit.parse.diags)) {
        xl6DiagPrintText(&unit.parse.diags, argv[1]); return 1;
    }
    size_t futures = 0, nullable = 0, casts=0, methods=0;
    for (size_t i=0;i<unit.ir.instructionCount;++i) {
        XL6_IrInstruction* instruction=&unit.ir.instructions[i];
        if(instruction->runtimeOp!=XL6_IR_RUNTIME_XVALUE_FUTURE_CAST_CHECKED)continue;
        ++casts; XL6_IrTypeId payload=instruction->payloadType;
        instruction->payloadType=0; reject(&unit.ir); instruction->payloadType=payload;
        XL6_IrCastAbi abi=instruction->castAbi; instruction->castAbi=XL6_IR_CAST_XVALUE_TO_STRING_CHECKED;
        reject(&unit.ir); instruction->castAbi=abi;
        instruction->calleeType=payload; reject(&unit.ir); instruction->calleeType=0;
    }
    for(size_t i=0;i<unit.ir.dynamicMethodParameterCount;++i) {
        XL6_IrDynamicMethodParameter* parameter=&unit.ir.dynamicMethodParameters[i];
        if(parameter->matchAbi!=XL6_IR_DYNAMIC_METHOD_MATCH_FUTURE)continue;
        ++methods; parameter->matchAbi=XL6_IR_DYNAMIC_METHOD_MATCH_EXACT_IDENTITY;
        reject(&unit.ir); parameter->matchAbi=XL6_IR_DYNAMIC_METHOD_MATCH_FUTURE;
    }
    for (size_t i = 0; i < unit.ir.callableBridgeParameterCount; ++i) {
        XL6_IrCallableBridgeParameter* slot=&unit.ir.callableBridgeParameters[i];
        if (slot->decodeAbi != XL6_IR_CALLABLE_BRIDGE_FUTURE_COPY) continue;
        ++futures;
        assert(slot->encodeAbi==XL6_IR_CALLABLE_BRIDGE_FUTURE_REF &&
            slot->decodedDestroyAbi==XL6_IR_ASYNC_FRAME_DESTROY_DESTROY_FUTURE);
        XL6_IrCallableBridgeValueAbi encode=slot->encodeAbi, decode=slot->decodeAbi;
        slot->encodeAbi=XL6_IR_CALLABLE_BRIDGE_XVALUE_RETAIN; reject(&unit.ir); slot->encodeAbi=encode;
        slot->decodeAbi=XL6_IR_CALLABLE_BRIDGE_XVALUE_BORROW; reject(&unit.ir); slot->decodeAbi=decode;
        XL6_IrAsyncFrameDestroyAbi destroy=slot->decodedDestroyAbi;
        slot->decodedDestroyAbi=XL6_IR_ASYNC_FRAME_DESTROY_NONE; reject(&unit.ir); slot->decodedDestroyAbi=destroy;
        XL6_IrTypeId payload=slot->valueType;
        slot->valueType=XL6_IR_TYPE_INVALID; reject(&unit.ir); slot->valueType=payload;
        XL6_IrCallbackInvokeAbi invoke=slot->invokeAbi;
        slot->invokeAbi=XL6_IR_CALLBACK_INVOKE_VALUE; reject(&unit.ir); slot->invokeAbi=invoke;
        if (slot->presenceAbi!=XL6_IR_CALLABLE_PRESENCE_NONE) {
            ++nullable; assert(slot->presenceAbi==XL6_IR_CALLABLE_PRESENCE_POINTER);
            slot->presenceAbi=XL6_IR_CALLABLE_PRESENCE_NONE; reject(&unit.ir);
            slot->presenceAbi=XL6_IR_CALLABLE_PRESENCE_POINTER;
        }
    }
    for(size_t i=0;i<unit.ir.callableBridgeResultCount;++i) {
        XL6_IrCallableBridgeResult* result=&unit.ir.callableBridgeResults[i];
        if(result->decodeAbi!=XL6_IR_CALLABLE_BRIDGE_FUTURE_COPY)continue;
        ++futures; assert(result->encodeAbi==XL6_IR_CALLABLE_BRIDGE_FUTURE_REF);
        XL6_IrAsyncFrameDestroyAbi destroy=result->destroyAbi;
        result->destroyAbi=XL6_IR_ASYNC_FRAME_DESTROY_NONE; reject(&unit.ir); result->destroyAbi=destroy;
    }
    assert((futures || casts || methods) && mutations);
    printf("Future slots=%zu nullable=%zu casts=%zu methods=%zu mutations=%zu rejected before output\n",futures,nullable,casts,methods,mutations);
    xl6CompileUnitFree(&unit); return 0;
}
