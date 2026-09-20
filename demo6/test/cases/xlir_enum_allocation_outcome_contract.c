#include "xlir_continuation_owner_test.h"

int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    size_t i, creates = 0, inserts = 0, effects = 0, discardedPatterns = 0;
    assert(argc == 4); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_enum_allocation_outcome.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "enum_allocation_outcome") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) || !xl6IrLowerModule(&ir, &parse.diags)) {
        for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%d:%d %s\n",
            parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
        return 2;
    }
    verify(&ir);
    for (i = 0; i < ir.nodeCount; ++i) {
        XL6_IrNode* node = &ir.nodes[i];
        const XL6_IrType* type = xl6IrTypeGet(&ir.types, node->storageType);
        if (node->kind != XL6_IR_NODE_CALL || node->callKind != XL6_SEM_CALL_ENUM_VARIANT ||
            type == NULL || type->kind != XL6_IR_TYPE_TAGGED_ENUM) continue;
        assert(node->constructionErrorAbi == XL6_IR_ERROR_ABI_MAY_RAISE); ++effects;
        node->constructionErrorAbi = XL6_IR_ERROR_ABI_NO_RAISE; reject(&ir, NULL);
        node->constructionErrorAbi = XL6_IR_ERROR_ABI_MAY_RAISE;
    }
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i];
        XL6_IrInstruction *test, saved; size_t variant;
        int creation = instruction->runtimeOp == XL6_IR_RUNTIME_TAGGED_ENUM_NEW;
        if (instruction->kind == XL6_IR_INSTR_DISCARD &&
            xl6IrPatternBindingPayload(&ir, instruction->origin) != NULL) {
            XL6_IrValueId value = ir.operands[instruction->firstOperand];
            assert(instruction->targetSymbol == 0); ++discardedPatterns;
            ir.operands[instruction->firstOperand] = 0; reject(&ir, NULL);
            ir.operands[instruction->firstOperand] = value;
            saved = *instruction; instruction->origin = 0; reject(&ir, NULL); *instruction = saved;
        }
        if (instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ||
            (!creation && instruction->runtimeOp != XL6_IR_RUNTIME_TAGGED_ENUM_PAYLOAD_SET_NEW)) continue;
        if (creation) ++creates; else ++inserts;
        assert(xl6IrCallInstructionErrorAbi(&ir, instruction) == XL6_IR_ERROR_ABI_MAY_RAISE);
        assert(xl6IrCallInstructionErrorPhysicalAbi(&ir, instruction) ==
            (creation ? XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT : XL6_IR_CALL_ERROR_PHYSICAL_FALSE_RESULT));
        if (!creation) assert(xl6IrRuntimeOperandOwnership(instruction->runtimeOp, 1) == XL6_IR_OPERAND_OWNERSHIP_CONSUME);
        assert(i + 1 < ir.instructionCount); test = &ir.instructions[i + 1]; saved = *test;
        assert(test->kind == XL6_IR_INSTR_ERROR_TEST && test->operandCount == 1 &&
            ir.operands[test->firstOperand] == instruction->result);
        test->falseTarget = test->trueTarget; reject(&ir, NULL); *test = saved;
        test->kind = XL6_IR_INSTR_BRANCH; reject(&ir, NULL); *test = saved;
        test->kind = XL6_IR_INSTR_TRAP_IF_FALSE; reject(&ir, NULL); *test = saved;
        {
            XL6_IrValueId value = ir.operands[test->firstOperand];
            ir.operands[test->firstOperand] = 0; reject(&ir, NULL);
            ir.operands[test->firstOperand] = value;
        }
        variant = instruction->enumVariant; instruction->enumVariant = SIZE_MAX;
        reject(&ir, NULL); instruction->enumVariant = variant;
    }
    assert(creates == 7 && inserts == 11 && effects == 7 && discardedPatterns == 1); verify(&ir);
    assert(xl6OutputOpenFile(&out, argv[1]) && xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]) && xl6IrCgenEmitHeader(&out, &ir, "ENUM_OUTCOME", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    printf("enum allocation outcome: %llu checked creates, %llu checked inserts, %llu rejected C/header-gated mutations\n",
        (unsigned long long)creates, (unsigned long long)inserts, (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
