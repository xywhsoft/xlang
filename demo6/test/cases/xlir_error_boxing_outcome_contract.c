#include "xlir_continuation_owner_test.h"

int main(int argc, char** argv)
{
    XL6_ParseResult parse;
    XL6_IrModule ir;
    XL6_Output out;
    size_t i, boxes = 0, initializers = 0;
    assert(argc == 5);
    xl6ParseResultInit(&parse);
    xl6IrModuleInit(&ir);
    if (!xl6ParseFile(argv[1], &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "error_boxing_outcome") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrLowerModule(&ir, &parse.diags)) {
        xl6DiagPrintText(&parse.diags, argv[1]);
        return 2;
    }
    verify(&ir);
    for (i = 0; i < ir.valueInitializerCount; ++i) {
        XL6_IrValueInitializer* initializer = &ir.valueInitializers[i];
        if (initializer->runtimeOp != XL6_IR_RUNTIME_XVALUE_ERROR_REF) continue;
        assert(initializer->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE);
        ++initializers;
        initializer->errorAbi = XL6_IR_ERROR_ABI_NO_RAISE;
        reject(&ir, NULL);
        initializer->errorAbi = XL6_IR_ERROR_ABI_MAY_RAISE;
    }
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* box = &ir.instructions[i];
        XL6_IrInstruction *test, saved;
        XL6_IrTypeId identity;
        XL6_IrValueId operand;
        if (box->kind != XL6_IR_INSTR_RUNTIME_CALL ||
            box->runtimeOp != XL6_IR_RUNTIME_XVALUE_ERROR_REF) continue;
        ++boxes;
        assert(xl6IrCallInstructionErrorAbi(&ir, box) == XL6_IR_ERROR_ABI_MAY_RAISE);
        assert(xl6IrCallInstructionErrorPhysicalAbi(&ir, box) == XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT);
        identity = box->runtimeIdentityType;
        assert(identity && xl6IrAllocatingBoxOpForType(&ir, identity) == XL6_IR_RUNTIME_XVALUE_ERROR_REF);
        box->runtimeIdentityType = XL6_IR_TYPE_INVALID;
        reject(&ir, NULL);
        box->runtimeIdentityType = identity;
        assert(i + 1 < ir.instructionCount);
        test = &ir.instructions[i + 1];
        saved = *test;
        assert(test->kind == XL6_IR_INSTR_ERROR_TEST && test->operandCount == 1 &&
            ir.operands[test->firstOperand] == box->result);
        test->kind = XL6_IR_INSTR_BRANCH;
        reject(&ir, NULL);
        *test = saved;
        test->falseTarget = test->trueTarget;
        reject(&ir, NULL);
        *test = saved;
        operand = ir.operands[test->firstOperand];
        ir.operands[test->firstOperand] = XL6_IR_VALUE_INVALID;
        reject(&ir, NULL);
        ir.operands[test->firstOperand] = operand;
    }
    assert(boxes);
    verify(&ir);
    assert(xl6OutputOpenFile(&out, argv[2]) && xl6IrCgenEmitC(&out, &ir, &parse.diags));
    xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3]) && xl6IrCgenEmitHeader(&out, &ir, "ERROR_BOX_OUTCOME", &parse.diags));
    xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[4]));
    xl6IrDumpModuleJson(&out, &ir);
    xl6OutputClose(&out);
    printf("Error boxing outcome: %llu checked boxes, %llu contextual initializers, %llu rejected C/header mutations\n",
        (unsigned long long)boxes, (unsigned long long)initializers, (unsigned long long)mutations);
    xl6IrModuleFree(&ir);
    xl6ParseResultFree(&parse);
    return 0;
}
