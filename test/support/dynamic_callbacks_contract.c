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
        assert(!(header ? xl6IrCgenEmitHeader(&output, ir, "BAD_DYNAMIC_CALLBACK", &diagnostics)
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
    size_t callable = 0;
    size_t native = 0;
    for (size_t i = 0; i < unit.ir.dynamicMethodCount; ++i) {
        XL6_IrDynamicMethod* method = &unit.ir.dynamicMethods[i];
        if (method->adapterAbi != XL6_IR_DYNAMIC_METHOD_ADAPTER_VERIFIED) continue;
        for (size_t p = 0; p < method->parameterCount; ++p) {
            XL6_IrDynamicMethodParameter* item = &unit.ir.dynamicMethodParameters[method->firstParameter + p];
            if (item->matchAbi != XL6_IR_DYNAMIC_METHOD_MATCH_CALLABLE) continue;
            const XL6_IrType* type = xl6IrTypeGet(&unit.ir.types, item->type);
            assert(type && type->kind == XL6_IR_TYPE_FUNCTION && type->typeValueId);
            ++callable;
            item->matchAbi = XL6_IR_DYNAMIC_METHOD_MATCH_ANY_XVALUE; reject(&unit.ir);
            item->matchAbi = XL6_IR_DYNAMIC_METHOD_MATCH_EXACT_IDENTITY; reject(&unit.ir);
            item->matchAbi = XL6_IR_DYNAMIC_METHOD_MATCH_CALLABLE;
            XL6_IrParameterAbi saved = item->abi;
            item->abi = XL6_IR_PARAMETER_ABI_VALUE; reject(&unit.ir); item->abi = saved;
            XL6_IrCallTarget* target = (XL6_IrCallTarget*)xl6IrCallTargetGet(&unit.ir, method->targetSymbol);
            if (target->parameterPhysicalAbis[p + 1] == XL6_IR_CALL_PARAMETER_PHYSICAL_CHECKED_CLOSURE_BORROW) {
                target->parameterPhysicalAbis[p + 1] = XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT; reject(&unit.ir);
                target->parameterPhysicalAbis[p + 1] = XL6_IR_CALL_PARAMETER_PHYSICAL_CHECKED_CLOSURE_BORROW;
                ++native;
            }
        }
    }
    assert(callable && mutations == callable * 3 + native);
    printf("dynamic callable parameters=%zu native=%zu mutations=%zu rejected before output\n", callable, native, mutations);
    xl6CompileUnitFree(&unit); return 0;
}
