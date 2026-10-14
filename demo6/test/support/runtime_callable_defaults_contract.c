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
        assert(!(header ? xl6IrCgenEmitHeader(&output, ir, "BAD_CALLABLE_DEFAULT", &diagnostics)
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
    size_t defaults = 0, captures = 0, prefixes = 0;
    for (size_t i = 0; i < unit.ir.closureTargetCount; ++i) {
        XL6_IrClosureTarget* closure = &unit.ir.closureTargets[i];
        for (size_t p = 0; p < closure->callParameterCount; ++p) {
            if (closure->callParameterDefaultAbis[p] != XL6_IR_DYNAMIC_METHOD_DEFAULT_PROGRAM) continue;
            ++defaults; captures += closure->captureCount != 0; prefixes += p != 0;
            uint32 index = closure->callParameterDefaultFunctions[p];
            assert(index && !closure->callParameterDefaultLiterals[p]);
            closure->callParameterDefaultFunctions[p] = 0; reject(&unit.ir);
            closure->callParameterDefaultFunctions[p] = index;
            XL6_IrFunction* function = &unit.ir.functions[index - 1];
            XL6_IrFunctionKind kind = function->kind;
            function->kind = XL6_IR_FUNCTION_USER; reject(&unit.ir); function->kind = kind;
            XL6_IrErrorAbi error = function->errorAbi;
            function->errorAbi = XL6_IR_ERROR_ABI_NO_RAISE; reject(&unit.ir); function->errorAbi = error;
            size_t count = function->parameterCount;
            function->parameterCount = count + 1; reject(&unit.ir); function->parameterCount = count;
            XL6_IrTypeId result = function->returnType;
            function->returnType = XL6_IR_TYPE_INVALID; reject(&unit.ir); function->returnType = result;
            XL6_IrNodeId declaration = function->declaration;
            function->declaration = unit.ir.root; reject(&unit.ir); function->declaration = declaration;
            XL6_SymbolId context = function->defaultClosureSymbol;
            function->defaultClosureSymbol = context ? 0 : UINT32_MAX; reject(&unit.ir);
            function->defaultClosureSymbol = context;
            for (size_t input = 0; input < function->parameterCount; ++input) {
                XL6_SymbolId cell = function->parameterCells ? function->parameterCells[input] : 0;
                if (!cell) continue;
                function->parameterCells[input] = 0; reject(&unit.ir);
                function->parameterCells[input] = cell;
            }
            closure->callParameterDefaultAbis[p] = XL6_IR_DYNAMIC_METHOD_DEFAULT_XVALUE_NULL;
            reject(&unit.ir); closure->callParameterDefaultAbis[p] = XL6_IR_DYNAMIC_METHOD_DEFAULT_PROGRAM;
            XL6_IrReachability reachability = function->reachability;
            if (reachability != XL6_IR_REACHABILITY_UNREACHABLE) {
                function->reachability = XL6_IR_REACHABILITY_UNREACHABLE; reject(&unit.ir);
                function->reachability = reachability;
            }
        }
    }
    assert(defaults && unit.ir.callableBridgeCount);
    for (size_t i = 0; i < unit.ir.callableBridgeCount; ++i) {
        XL6_IrCallableBridge* bridge = &unit.ir.callableBridges[i];
        assert(bridge->argumentBindingAbi == XL6_IR_CALLABLE_BIND_PROVIDED_THEN_DEFAULTS);
        bridge->argumentBindingAbi = XL6_IR_CALLABLE_BIND_INVALID; reject(&unit.ir);
        bridge->argumentBindingAbi = XL6_IR_CALLABLE_BIND_PROVIDED_THEN_DEFAULTS;
    }
    assert(mutations >= defaults * 8);
    printf("callable defaults=%zu captures=%zu prefixes=%zu mutations=%zu rejected before output\n",
        defaults, captures, prefixes, mutations);
    xl6CompileUnitFree(&unit); return 0;
}
