#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>

static size_t mutations;
static void reject(XL6_IrModule* ir)
{
    XL6_DiagList diags = {0};
    assert(!xl6IrVerifyModule(ir, &diags) && diags.count);
    for (unsigned header = 0; header < 2; ++header) {
        XL6_Output out; size_t size;
        assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out, ir, "REFUSED_GENERIC_GRAPH", &diags)
                        : xl6IrCgenEmitC(&out, ir, &diags)));
        char* text = xl6OutputTakeMemoryText(&out, &size);
        assert(size == 0); xrtFree(text);
    }
    xl6DiagFree(&diags); ++mutations;
}
int main(int argc, char** argv)
{
    assert(argc == 2); XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit, argv[1], 1));
    if (!xl6IrLowerModule(&unit.ir, &unit.parse.diags)) {
        xl6DiagPrintText(&unit.parse.diags, argv[1]); return 1;
    }
    assert(xl6IrVerifyModule(&unit.ir, &unit.parse.diags));
    size_t methods = 0, instances = unit.ir.genericSpecializationCount;
    assert(instances > 0 && instances <= 1024);
    for (size_t i = 0; i < unit.ir.dynamicMethodCount; ++i) {
        XL6_IrDynamicMethod* row = &unit.ir.dynamicMethods[i];
        if (row->adapterAbi != XL6_IR_DYNAMIC_METHOD_ADAPTER_VERIFIED) continue;
        const XL6_IrCallTarget* target = xl6IrCallTargetGet(&unit.ir, row->targetSymbol);
        assert(target && target->returnType == row->returnType);
        XL6_SymbolId saved = row->targetSymbol;
        row->targetSymbol = 0; reject(&unit.ir); row->targetSymbol = saved;
        ++methods;
    }
    assert(methods && mutations == methods);
    size_t binaryDefaults = 0;
    for (size_t i = 0; i < unit.ir.classFieldCount; ++i) {
        XL6_IrClassField* row = &unit.ir.classFields[i];
        if (row->initializerAbi != XL6_IR_INITIALIZER_BYTES_EMPTY) continue;
        row->initializerAbi = XL6_IR_INITIALIZER_XVALUE_NULL; reject(&unit.ir);
        row->initializerAbi = XL6_IR_INITIALIZER_BYTES_EMPTY; ++binaryDefaults;
        XL6_IrErrorAbi saved = row->initializerErrorAbi;
        row->initializerErrorAbi = XL6_IR_ERROR_ABI_NO_RAISE; reject(&unit.ir);
        row->initializerErrorAbi = saved;
    }
    for (size_t i = 0; i < unit.ir.instructionCount; ++i) {
        const XL6_IrInstruction* row = &unit.ir.instructions[i];
        if (row->kind == XL6_IR_INSTR_CALL)
            assert(xl6IrCallTargetGet(&unit.ir, row->targetSymbol));
    }
    printf("generic graph instances=%zu methods=%zu binary-defaults=%zu mutations=%zu rejected before output\n",
        instances, methods, binaryDefaults, mutations);
    xl6CompileUnitFree(&unit); return 0;
}
