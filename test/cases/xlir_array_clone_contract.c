#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>

static void rejected(const XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir, NULL));
    for (unsigned header = 0; header < 2; ++header) {
        XL6_Output output;
        XL6_DiagList diagnostics;
        size_t size = SIZE_MAX;
        xl6DiagInit(&diagnostics);
        assert(xl6OutputOpenMemory(&output));
        assert(!(header ? xl6IrCgenEmitHeader(&output, ir, "REJECTED_CLONE", &diagnostics) :
            xl6IrCgenEmitC(&output, ir, &diagnostics)));
        char* text = xl6OutputTakeMemoryText(&output, &size);
        assert(size == 0);
        xrtFree(text); xl6OutputDiscard(&output); xl6DiagFree(&diagnostics);
    }
}

int main(int argc, char** argv)
{
    assert(argc == 3);
    XL6_ParseResult parse;
    XL6_IrModule ir;
    XL6_Output output;
    unsigned producers = 0, mutations = 0;
    int ok = 0;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile(argv[1], &parse) || !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "array_clone_contract") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrLowerModule(&ir, &parse.diags)) goto done;
    assert(xl6IrVerifyModule(&ir, NULL));
    for (size_t i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i];
        if (instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ||
            instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_DEEP_CLONE) continue;
        assert(xl6IrCallInstructionErrorAbi(&ir, instruction) == XL6_IR_ERROR_ABI_MAY_RAISE);
        assert(xl6IrCallInstructionErrorPhysicalAbi(&ir, instruction) == XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT);
        assert(instruction->operandCount == 1 && i + 1 < ir.instructionCount);
        XL6_IrInstruction saved = *instruction;
#define MUTATE(Field, Value) do { instruction->Field = (Value); rejected(&ir); *instruction = saved; ++mutations; } while (0)
        MUTATE(type, XL6_IR_TYPE_INVALID);
        MUTATE(result, XL6_IR_VALUE_INVALID);
        MUTATE(operandCount, 0);
        MUTATE(operandCount, 2);
        MUTATE(immediateText, "ambiguous-source");
#undef MUTATE
        XL6_IrInstruction outcome = ir.instructions[i + 1];
        assert(outcome.kind == XL6_IR_INSTR_ERROR_TEST && outcome.operandCount == 1 &&
            ir.operands[outcome.firstOperand] == instruction->result &&
            outcome.trueTarget && outcome.falseTarget && outcome.trueTarget != outcome.falseTarget);
        ir.instructions[i + 1].kind = XL6_IR_INSTR_CONST;
        rejected(&ir); ir.instructions[i + 1] = outcome; ++mutations;
        ir.instructions[i + 1].falseTarget = outcome.trueTarget;
        rejected(&ir); ir.instructions[i + 1] = outcome; ++mutations;
        ++producers;
    }
    assert(producers >= 6 && mutations == producers * 7 && xl6IrVerifyModule(&ir, NULL));
    assert(xl6OutputOpenFile(&output, argv[2]));
    ok = xl6IrCgenEmitC(&output, &ir, &parse.diags); xl6OutputClose(&output);
    printf("Array clone: %u checked producers; %u mutations refused before C/header\n", producers, mutations);
done:
    if (!ok) xl6DiagPrintText(&parse.diags, argv[1]);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    return ok ? 0 : 1;
}
