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
        assert(!(header ? xl6IrCgenEmitHeader(&output, ir, "BAD_RUNTIME_DEFAULT", &diagnostics)
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
    size_t defaults = 0;
    for (size_t i = 0; i < unit.ir.dynamicMethodParameterCount; ++i) {
        XL6_IrDynamicMethodParameter* item = &unit.ir.dynamicMethodParameters[i];
        if (item->defaultAbi != XL6_IR_DYNAMIC_METHOD_DEFAULT_PROGRAM) continue;
        ++defaults; assert(!item->defaultLiteral && item->defaultFunction);
        uint32 index = item->defaultFunction;
        item->defaultFunction = 0; reject(&unit.ir); item->defaultFunction = index;
        XL6_IrFunction* function = &unit.ir.functions[index - 1];
        XL6_IrFunctionKind kind = function->kind;
        function->kind = XL6_IR_FUNCTION_USER; reject(&unit.ir); function->kind = kind;
        XL6_IrErrorAbi error = function->errorAbi;
        function->errorAbi = XL6_IR_ERROR_ABI_NO_RAISE; reject(&unit.ir); function->errorAbi = error;
        size_t count = function->parameterCount;
        function->parameterCount = 0; reject(&unit.ir); function->parameterCount = count;
        XL6_IrTypeId result = function->returnType;
        function->returnType = XL6_IR_TYPE_INVALID; reject(&unit.ir); function->returnType = result;
        XL6_IrNodeId declaration = function->declaration;
        function->declaration = unit.ir.root; reject(&unit.ir); function->declaration = declaration;
        item->defaultAbi = XL6_IR_DYNAMIC_METHOD_DEFAULT_XVALUE_NULL; reject(&unit.ir);
        item->defaultAbi = XL6_IR_DYNAMIC_METHOD_DEFAULT_PROGRAM;
        XL6_IrReachability reachability = function->reachability;
        if (reachability != XL6_IR_REACHABILITY_UNREACHABLE) {
            function->reachability = XL6_IR_REACHABILITY_UNREACHABLE; reject(&unit.ir); function->reachability = reachability;
        }
    }
    assert(defaults && mutations >= defaults * 7);
    printf("runtime defaults=%zu mutations=%zu rejected before output\n", defaults, mutations);
    xl6CompileUnitFree(&unit); return 0;
}
