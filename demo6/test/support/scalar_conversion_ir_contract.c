#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static size_t mutations;
static void reject(XL6_IrModule* module)
{
    XL6_DiagList diagnostics = {0}; assert(!xl6IrVerifyModule(module, &diagnostics) && diagnostics.count);
    for (unsigned header = 0; header < 2; ++header) {
        XL6_Output output; size_t size; assert(xl6OutputOpenMemory(&output));
        assert(!(header ? xl6IrCgenEmitHeader(&output, module, "INVALID_SCALAR", &diagnostics) : xl6IrCgenEmitC(&output, module, &diagnostics)));
        char* data = xl6OutputTakeMemoryText(&output, &size); assert(!size); xrtFree(data);
    }
    xl6DiagFree(&diagnostics); ++mutations;
}
int main(int argc, char** argv)
{
    assert(argc == 2); XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit, argv[1], 1) && xl6IrLowerModule(&unit.ir, &unit.parse.diags));
    XL6_IrRuntimeTypeMap mapping = unit.ir.runtimeTypeMap;
    unit.ir.runtimeTypeMap.typeTypeValueId = 0; reject(&unit.ir); unit.ir.runtimeTypeMap = mapping;
    unit.ir.runtimeTypeMap.typeTypeValueId = mapping.u64TypeValueId; reject(&unit.ir); unit.ir.runtimeTypeMap = mapping;
    size_t converted = 0;
    for (size_t i = 0; i < unit.ir.instructionCount; ++i) {
        XL6_IrInstruction* item = &unit.ir.instructions[i];
        if (item->runtimeOp != XL6_IR_RUNTIME_INTEGER_CONVERT_CHECKED &&
            item->runtimeOp != XL6_IR_RUNTIME_STRING_TO_F64_CHECKED &&
            item->runtimeOp != XL6_IR_RUNTIME_STRING_TO_F32_EXPLICIT &&
            item->runtimeOp != XL6_IR_RUNTIME_XVALUE_TO_F32_EXPLICIT) continue;
        if (item->runtimeOp == XL6_IR_RUNTIME_INTEGER_CONVERT_CHECKED &&
            (item->integerTarget == XL6_IR_INTEGER_TARGET_UNICODE_SCALAR ||
             (item->integerSourceAbi != XL6_IR_INTEGER_SOURCE_MANAGED_STRING &&
              item->integerSourceAbi != XL6_IR_INTEGER_SOURCE_XVALUE))) continue;
        assert(xl6IrCallInstructionErrorPhysicalAbi(&unit.ir, item) == XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT);
        ++converted; XL6_IrInstruction saved = *item;
        item->operandCount = 0; reject(&unit.ir); *item = saved;
        item->integerTarget = XL6_IR_INTEGER_TARGET_INVALID; item->integerSourceAbi = XL6_IR_INTEGER_SOURCE_U64;
        reject(&unit.ir); *item = saved;
        assert(i + 1 < unit.ir.instructionCount && unit.ir.instructions[i + 1].kind == XL6_IR_INSTR_ERROR_TEST);
        XL6_IrInstruction status = unit.ir.instructions[i + 1];
        unit.ir.instructions[i + 1].operandCount = 0; reject(&unit.ir); unit.ir.instructions[i + 1] = status;
    }
    assert(converted && mutations);
    for (size_t i = 0; i < unit.ir.intrinsicCallCount; ++i) {
        XL6_IrIntrinsicCall* item = &unit.ir.intrinsicCalls[i];
        if (item->kind != XL6_IR_INTRINSIC_CHECKED_INTEGER_CONVERT &&
            item->kind != XL6_IR_INTRINSIC_CHECKED_FLOAT_CONVERT) continue;
        XL6_IrIntrinsicCall saved = *item;
        item->errorAbi = XL6_IR_ERROR_ABI_NO_RAISE; reject(&unit.ir); *item = saved;
        item->resultAbi = XL6_IR_INTRINSIC_RESULT_OWNED; reject(&unit.ir); *item = saved;
    }
    printf("language scalar status-outputs=%zu mutations=%zu rejected before output\n", converted, mutations);
    xl6CompileUnitFree(&unit); return 0;
}
