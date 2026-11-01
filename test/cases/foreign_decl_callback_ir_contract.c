#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>

static unsigned mutations;
static void refused(XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module, NULL));
    for (unsigned header = 0; header < 2; ++header) {
        XL6_Output out; size_t bytes = SIZE_MAX;
        assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out, module, "BAD_CALLBACK", NULL) : xl6IrCgenEmitC(&out, module, NULL)));
        char* text = xl6OutputTakeMemoryText(&out, &bytes);
        assert(text && !bytes); xrtFree(text); xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc, char** argv)
{
    assert(argc == 2);
    XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit, argv[1], 1) && xl6IrLowerModule(&unit.ir, &unit.parse.diags));
    XL6_IrModule* module = &unit.ir;
    assert(xl6IrVerifyModule(module, NULL));
    unsigned addresses = 0;
    for (size_t i = 0; i < module->instructionCount; ++i) {
        XL6_IrInstruction* address = &module->instructions[i];
        if (address->kind != XL6_IR_INSTR_C_FUNCTION_ADDRESS) continue;
        ++addresses;
        XL6_IrInstruction saved = *address;
#define BAD(F,V) do { address->F = (V); refused(module); *address = saved; } while (0)
        BAD(type, 0); BAD(targetSymbol, 0); BAD(opcode, XL6_IR_OPCODE_ADD);
        BAD(kind, XL6_IR_INSTR_CONST); BAD(elementIndex, 0); BAD(payloadType, saved.type);
        BAD(runtimeOp, XL6_IR_RUNTIME_C_PRINT); BAD(targetValue, saved.result);
#undef BAD
        XL6_IrType* pointer = &module->types.items[saved.type - 1];
        XL6_IrType oldType = *pointer;
#define BADTYPE(F,V) do { pointer->F = (V); refused(module); *pointer = oldType; } while (0)
        BADTYPE(cAbi, XL6_IR_C_ABI_RAW_POINTER);
        BADTYPE(kind, XL6_IR_TYPE_FUNCTION);
        BADTYPE(ownershipAbi, XL6_IR_OWNERSHIP_ABI_MANAGED);
        BADTYPE(nullAbi, XL6_IR_NULL_ABI_NONE);
        BADTYPE(paramCount, 0); BADTYPE(nominalSymbol, saved.targetSymbol);
#undef BADTYPE
        XL6_IrCallTarget* target = NULL;
        for (size_t t = 0; t < module->callTargetCount; ++t)
            if (module->callTargets[t].symbol == saved.targetSymbol) target = &module->callTargets[t];
        assert(target);
        XL6_IrCallTarget oldTarget = *target;
        target->errorAbi = XL6_IR_ERROR_ABI_MAY_RAISE; refused(module); *target = oldTarget;
        target->returnType = 0; refused(module); *target = oldTarget;
        for (size_t u = 0; u < module->instructionCount; ++u) {
            XL6_IrInstruction* use = &module->instructions[u];
            for (size_t p = 0; p < use->operandCount; ++p) {
                if (module->operands[use->firstOperand + p] != saved.result) continue;
                assert(use->kind == XL6_IR_INSTR_CALL);
                XL6_IrInstruction oldUse = *use;
                use->kind = XL6_IR_INSTR_RUNTIME_CALL; refused(module); *use = oldUse;
            }
        }
        assert(xl6IrVerifyModule(module, NULL));
    }
    assert(addresses >= 2);
    for (size_t t = 0; t < module->callTargetCount; ++t) {
        XL6_IrCallTarget* target = &module->callTargets[t];
        for (size_t p = 0; p < target->parameterCount; ++p) {
            if (target->parameterPhysicalAbis[p] != XL6_IR_CALL_PARAMETER_PHYSICAL_STATIC_C_CALLBACK) continue;
            target->parameterPhysicalAbis[p] = XL6_IR_CALL_PARAMETER_PHYSICAL_RAW_C_CALLBACK; refused(module);
            target->parameterPhysicalAbis[p] = XL6_IR_CALL_PARAMETER_PHYSICAL_STATIC_C_CALLBACK;
        }
    }
    printf("static C addresses=%u mutations=%u zero-output=%u\n", addresses, mutations, mutations * 2);
    xl6CompileUnitFree(&unit); return 0;
}
