#include "src/compile/xl6_compile_unit.h"
#include <assert.h>
#include <stdio.h>

int main(int argc, char** argv)
{
    assert(argc == 2);
    XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit, argv[1], 1) && xl6IrLowerModule(&unit.ir, &unit.parse.diags));
    XL6_IrModule* module = &unit.ir;
    assert(xl6IrVerifyModule(module, NULL));
    unsigned plans = 0, refusals = 0;
    for (size_t f = 0; f < module->functionCount; ++f) {
        XL6_IrFunction* function = &module->functions[f];
        const XL6_IrInstruction* call = xl6IrDirectCForwardingCall(module, function);
        if (!call) continue;
        ++plans;
        XL6_IrCallTarget* target = NULL;
        for (size_t t = 0; t < module->callTargetCount; ++t)
            if (module->callTargets[t].symbol == call->targetSymbol) target = &module->callTargets[t];
        assert(target && target->parameterCount);
        XL6_IrCallTarget saved = *target;
#define REFUSE(F,V) do { target->F = (V); assert(!xl6IrDirectCForwardingCall(module, function)); *target = saved; ++refusals; } while (0)
        REFUSE(errorAbi, XL6_IR_ERROR_ABI_MAY_RAISE);
        REFUSE(errorPhysicalAbi, XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER);
        REFUSE(nativeServicesRequired, 1);
        REFUSE(returnType, 0);
        REFUSE(variadicAbi, XL6_IR_CALL_VARIADIC_C);
#undef REFUSE
        XL6_IrCallParameterPhysicalAbi physical = target->parameterPhysicalAbis[0];
        target->parameterPhysicalAbis[0] = XL6_IR_CALL_PARAMETER_PHYSICAL_RAW_C_CALLBACK;
        assert(!xl6IrDirectCForwardingCall(module, function)); ++refusals;
        target->parameterPhysicalAbis[0] = physical;
        size_t count = function->blockCount;
        function->blockCount = 2; assert(!xl6IrDirectCForwardingCall(module, function)); ++refusals;
        function->blockCount = count;
        XL6_IrInstruction* load = &module->instructions[function->firstInstruction];
        XL6_IrInstructionKind kind = load->kind;
        load->kind = XL6_IR_INSTR_STORE; assert(!xl6IrDirectCForwardingCall(module, function)); ++refusals;
        load->kind = kind;
        assert(xl6IrDirectCForwardingCall(module, function) == call && xl6IrVerifyModule(module, NULL));
    }
    assert(plans);
    printf("exact C forwarding plans=%u unsafe placements refused=%u\n", plans, refusals);
    xl6CompileUnitFree(&unit); return 0;
}
