#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static size_t mutations;
static void reject(XL6_IrModule* ir)
{
    XL6_DiagList diagnostics = {0};
    assert(!xl6IrVerifyModule(ir, &diagnostics) && diagnostics.count);
    for (unsigned header = 0; header < 2; ++header) {
        XL6_Output output; size_t size;
        assert(xl6OutputOpenMemory(&output));
        assert(!(header ? xl6IrCgenEmitHeader(&output, ir, "BAD_INTEGER_DEFAULT", &diagnostics)
                        : xl6IrCgenEmitC(&output, ir, &diagnostics)));
        char* text = xl6OutputTakeMemoryText(&output, &size); assert(!size); xrtFree(text);
    }
    xl6DiagFree(&diagnostics); ++mutations;
}
static void mutateLiteral(XL6_IrModule* ir, char** literal)
{
    char* saved = *literal;
    assert(saved);
    *literal = !strcmp(saved, "0ULL") ? "1ULL" : "0ULL";
    reject(ir); *literal = saved;
}
int main(int argc, char** argv)
{
    assert(argc == 2); XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    if (!xl6CompileUnitLoadFile(&unit, argv[1], 1) ||
        !xl6IrLowerModule(&unit.ir, &unit.parse.diags) ||
        !xl6IrVerifyModule(&unit.ir, &unit.parse.diags)) {
        xl6DiagPrintText(&unit.parse.diags, argv[1]); return 1;
    }
    size_t defaults = 0, boundaries = 0, closures = 0;
    for (size_t i = 0; i < unit.ir.dynamicMethodCount; ++i) {
        XL6_IrDynamicMethod* method = &unit.ir.dynamicMethods[i];
        int boundary = method->name &&
            (!strcmp(method->name, "side") || !strcmp(method->name, "wraps"));
        if (boundary) {
            assert(method->adapterAbi == XL6_IR_DYNAMIC_METHOD_ADAPTER_UNSUPPORTED);
            assert(!xl6IrDynamicMethodSourceSupported(&unit.ir, method)); ++boundaries;
            method->adapterAbi = XL6_IR_DYNAMIC_METHOD_ADAPTER_VERIFIED;
            reject(&unit.ir); method->adapterAbi = XL6_IR_DYNAMIC_METHOD_ADAPTER_UNSUPPORTED;
        }
        for (size_t p = 0; p < method->parameterCount; ++p) {
            XL6_IrDynamicMethodParameter* item = &unit.ir.dynamicMethodParameters[method->firstParameter + p];
            if (!item->defaultExpression) continue;
            const XL6_IrNode* expression = xl6IrNodeGet(&unit.ir, item->defaultExpression);
            if (!expression || expression->kind != XL6_IR_NODE_CALL) continue;
            char* canonical = xl6IrCanonicalIntegerDefault(&unit.ir, item->defaultExpression, item->type);
            if (boundary) { assert(!canonical); continue; }
            if (item->defaultAbi != XL6_IR_DYNAMIC_METHOD_DEFAULT_I64 &&
                item->defaultAbi != XL6_IR_DYNAMIC_METHOD_DEFAULT_U64) { xrtFree(canonical); continue; }
            assert(canonical && !strcmp(canonical, item->defaultLiteral)); xrtFree(canonical);
            assert(!xl6IrCanonicalContextualIntegerLiteral(&unit.ir, item->defaultExpression, item->type));
            ++defaults; mutateLiteral(&unit.ir, &item->defaultLiteral);
            XL6_IrDynamicMethodDefaultAbi saved = item->defaultAbi;
            item->defaultAbi = saved == XL6_IR_DYNAMIC_METHOD_DEFAULT_I64
                ? XL6_IR_DYNAMIC_METHOD_DEFAULT_U64 : XL6_IR_DYNAMIC_METHOD_DEFAULT_I64;
            reject(&unit.ir); item->defaultAbi = saved;
            XL6_IrIntrinsicCall* call = (XL6_IrIntrinsicCall*)xl6IrIntrinsicCallGet(&unit.ir, item->defaultExpression);
            assert(call && call->admission == XL6_IR_INTRINSIC_ADMISSION_VERIFIED);
            XL6_IrIntrinsicAdmission admission = call->admission;
            call->admission = XL6_IR_INTRINSIC_ADMISSION_UNSUPPORTED_ABI; reject(&unit.ir); call->admission = admission;
            XL6_IrTypeId resultType = call->resultType;
            call->resultType = XL6_IR_TYPE_INVALID; reject(&unit.ir); call->resultType = resultType;
            XL6_IrIntrinsicOperand* operand = &unit.ir.intrinsicOperands[call->firstOperand];
            XL6_IrNodeId operandExpression = operand->expression;
            operand->expression = item->defaultExpression; reject(&unit.ir); operand->expression = operandExpression;
        }
    }
    for (size_t i = 0; i < unit.ir.closureTargetCount; ++i) {
        XL6_IrClosureTarget* target = &unit.ir.closureTargets[i];
        for (size_t p = 0; p < target->callParameterCount; ++p) {
            if (target->callParameterDefaultAbis[p] != XL6_IR_DYNAMIC_METHOD_DEFAULT_I64 &&
                target->callParameterDefaultAbis[p] != XL6_IR_DYNAMIC_METHOD_DEFAULT_U64) continue;
            ++closures; mutateLiteral(&unit.ir, &target->callParameterDefaultLiterals[p]);
        }
    }
    assert(defaults || boundaries);
    assert(!boundaries || boundaries == 2);
    assert(mutations == defaults * 5 + closures + boundaries);
    printf("integer defaults=%zu boundaries=%zu closures=%zu mutations=%zu rejected before output\n",
        defaults, boundaries, closures, mutations);
    xl6CompileUnitFree(&unit); return 0;
}
