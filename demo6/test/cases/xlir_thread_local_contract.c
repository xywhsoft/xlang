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
        assert(!(header ? xl6IrCgenEmitHeader(&output, module, "BAD_LOCAL", &diagnostics) :
            xl6IrCgenEmitC(&output, module, &diagnostics)));
        char* text = xl6OutputTakeMemoryText(&output, &size);
        assert(size == 0); xrtFree(text); xl6OutputDiscard(&output); xl6DiagFree(&diagnostics);
    }
    ++refusals;
}
int main(int argc, char** argv)
{
    assert(argc == 2);
    XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit, argv[1], 1));
    assert(xl6IrLowerModule(&unit.ir, &unit.parse.diags));
    XL6_IrModule* module = &unit.ir;
    assert(xl6IrVerifyModule(module, NULL));
    size_t checked = 0;
    for (size_t i = 0; i < module->callTargetCount; ++i) {
        XL6_IrCallTarget* target = &module->callTargets[i];
        if (!target->cName || strncmp(target->cName, "xl6ThreadLocal", 14)) continue;
        XL6_IrCallTarget saved = *target;
        assert(xl6IrRuntimeDirectBridgeTargetMatches(module, target));
        target->effectAbi = XL6_IR_CALL_EFFECT_PURE; reject(module); *target = saved;
        target->errorAbi = saved.errorAbi == XL6_IR_ERROR_ABI_NO_RAISE ?
            XL6_IR_ERROR_ABI_MAY_RAISE : XL6_IR_ERROR_ABI_NO_RAISE;
        reject(module); *target = saved;
        ++checked;
    }
    assert(checked == 9);
    size_t projections = 0, characters = 0;
    for (size_t i = 0; i < module->instructionCount; ++i) {
        XL6_IrInstruction* instruction = &module->instructions[i];
        if (instruction->castAbi == XL6_IR_CAST_XVALUE_TO_NULLABLE_CHAR_TRY) {
            XL6_IrInstruction saved = *instruction;
            instruction->castAbi = XL6_IR_CAST_XVALUE_TO_NULLABLE_BOOL_TRY;
            reject(module); *instruction = saved; ++characters;
        }
        if (instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_CAST_NULLABLE_BYTES) continue;
        XL6_IrInstruction saved = *instruction;
        instruction->payloadType = XL6_IR_TYPE_INVALID; reject(module); *instruction = saved;
        instruction->castAbi = XL6_IR_CAST_INVALID; reject(module); *instruction = saved;
        instruction->runtimeIdentityType = saved.payloadType; reject(module); *instruction = saved;
        ++projections;
    }
    assert(projections >= 1 && characters >= 1);
    assert(xl6IrVerifyModule(module, NULL)); xl6CompileUnitFree(&unit);
    printf("thread-local contracts: %zu entries, %zu bytes projections, %zu refusals\n",
        checked, projections, refusals);
    return 0;
}
