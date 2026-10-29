#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t refusals;
static void reject(XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module, NULL));
    for (int header = 0; header != 2; ++header) {
        XL6_Output output; XL6_DiagList diagnostics; size_t size = SIZE_MAX;
        xl6DiagInit(&diagnostics); assert(xl6OutputOpenMemory(&output));
        assert(!(header ? xl6IrCgenEmitHeader(&output, module, "BAD_SYNC", &diagnostics) :
            xl6IrCgenEmitC(&output, module, &diagnostics)));
        char* text = xl6OutputTakeMemoryText(&output, &size);
        assert(size == 0); xrtFree(text); xl6OutputDiscard(&output); xl6DiagFree(&diagnostics);
    }
    ++refusals;
}
int main(int argc, char** argv)
{
    assert(argc == 3);
    size_t checked = 0, closures = 0;
    for (int input = 1; input != argc; ++input) {
    XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit, argv[input], 1));
    assert(xl6IrLowerModule(&unit.ir, &unit.parse.diags));
    XL6_IrModule* module = &unit.ir;
    assert(xl6IrVerifyModule(module, NULL));
    for (size_t i = 0; i < module->callTargetCount; ++i) {
        XL6_IrCallTarget* target = &module->callTargets[i];
        if (!target->cName || (strncmp(target->cName, "xl6Once", 7) && strncmp(target->cName, "xl6Event", 8))) continue;
        XL6_IrCallTarget saved = *target;
        assert(xl6IrRuntimeDirectBridgeTargetMatches(module, target));
        target->effectAbi = XL6_IR_CALL_EFFECT_PURE; reject(module); *target = saved;
        target->errorAbi = saved.errorAbi == XL6_IR_ERROR_ABI_NO_RAISE ?
            XL6_IR_ERROR_ABI_MAY_RAISE : XL6_IR_ERROR_ABI_NO_RAISE;
        reject(module); *target = saved;
        if (strstr(target->cName, "Run")) {
            XL6_IrCallParameterPhysicalAbi physical = target->parameterPhysicalAbis[1];
            target->parameterPhysicalAbis[1] = XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;
            reject(module); target->parameterPhysicalAbis[1] = physical;
            XL6_IrType* callable = &module->types.items[target->parameterTypes[1]-1];
            XL6_IrCallableErasureAbi erasure = callable->callableErasureAbi;
            callable->callableErasureAbi = XL6_IR_CALLABLE_ERASURE_INVALID;
            reject(module); callable->callableErasureAbi = erasure;
            ++closures;
        }
        ++checked;
    }
    assert(xl6IrVerifyModule(module, NULL)); xl6CompileUnitFree(&unit);
    }
    assert(checked == 10 && closures == 2);
    printf("once/event contracts: %zu entries, %zu checked closures, %zu refusals\n", checked, closures, refusals);
    return 0;
}
