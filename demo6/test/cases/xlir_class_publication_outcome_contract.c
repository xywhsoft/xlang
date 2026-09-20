#include "xlir_continuation_owner_test.h"
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    size_t publications = 0;
    assert(argc == 3); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_class_publication_outcome.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "class_publication_outcome") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) || !xl6IrLowerModule(&ir, &parse.diags)) {
        for (size_t i = 0; i < parse.diags.count; ++i)
            fprintf(stderr, "%d:%d %s\n", parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
        return 2;
    }
    verify(&ir);
    for (size_t i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction *producer = &ir.instructions[i], *test, saved;
        XL6_IrValueId value; XL6_IrTypeId payload;
        if (producer->kind != XL6_IR_INSTR_RUNTIME_CALL || producer->runtimeOp != XL6_IR_RUNTIME_XVALUE_CLASS_CONSTRUCTION_COMMIT) continue;
        ++publications;
        assert(xl6IrCallInstructionErrorAbi(&ir, producer) == XL6_IR_ERROR_ABI_MAY_RAISE &&
            xl6IrCallInstructionErrorPhysicalAbi(&ir, producer) == XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT);
        assert(i + 1 < ir.instructionCount); test = &ir.instructions[i + 1]; saved = *test;
        assert(test->kind == XL6_IR_INSTR_ERROR_TEST && test->operandCount == 1 && !test->targetSymbol &&
            ir.operands[test->firstOperand] == producer->result);
        test->falseTarget = test->trueTarget; reject(&ir, NULL); *test = saved;
        test->kind = XL6_IR_INSTR_DISCARD; reject(&ir, NULL); *test = saved;
        test->kind = XL6_IR_INSTR_BRANCH; reject(&ir, NULL); *test = saved;
        test->falseTarget = XL6_IR_BLOCK_INVALID; reject(&ir, NULL); *test = saved;
        value = ir.operands[test->firstOperand]; ir.operands[test->firstOperand] = 0;
        reject(&ir, NULL); ir.operands[test->firstOperand] = value;
        payload = producer->payloadType; producer->payloadType = XL6_IR_TYPE_INVALID;
        reject(&ir, NULL); producer->payloadType = payload;
    }
    /* The three user classes and both expanded DynamicFields births commit
     * code ownership; destructor-free classes are no longer omitted. */
    assert(publications == 5 && mutations == 30); verify(&ir);
    assert(xl6OutputOpenFile(&out, argv[1]) && xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]) && xl6IrCgenEmitHeader(&out, &ir, "CLASS_PUBLICATION", &parse.diags)); xl6OutputClose(&out);
    printf("Class publication: %llu checked consuming bindings, %llu rejected C/header-gated mutations\n",
        (unsigned long long)publications, (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
