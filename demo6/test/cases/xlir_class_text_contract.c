#include "xlir_continuation_owner_test.h"

int main(int argc, char** argv)
{
    XL6_ParseResult parse;
    XL6_IrModule ir;
    XL6_Output out;
    size_t conversions = 0, layouts = 0;
    assert(argc == 5);
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile(argv[1], &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "class_text_contract") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrLowerModule(&ir, &parse.diags)) {
        xl6DiagPrintText(&parse.diags, argv[1]); return 2;
    }
    verify(&ir);
    for (size_t i = 0; i < ir.classLayoutCount; ++i) {
        XL6_IrClassLayout* layout = &ir.classLayouts[i];
        XL6_IrClassLayout saved = *layout;
        if (layout->stringAbi != XL6_IR_CLASS_STRING_ABI_RUNTIME_DERIVED) continue;
        ++layouts;
        assert(layout->runtimeTypeTag && !layout->stringTarget &&
            layout->inheritedFieldCount <= layout->fieldCount);
        layout->runtimeTypeTag = 0; reject(&ir, NULL); *layout = saved;
        layout->stringTarget = UINT32_MAX; reject(&ir, NULL); *layout = saved;
        layout->inheritedFieldCount = layout->fieldCount + 1; reject(&ir, NULL); *layout = saved;
        layout->firstField = SIZE_MAX; reject(&ir, NULL); *layout = saved;
        if (layout->fieldCount) {
            XL6_IrClassField* field = &ir.classFields[layout->firstField];
            int included = field->stringDeriveIncluded;
            field->stringDeriveIncluded = 2; reject(&ir, NULL);
            field->stringDeriveIncluded = included;
        }
    }
    for (size_t i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* operation = &ir.instructions[i];
        XL6_IrInstruction saved = *operation;
        if (operation->kind != XL6_IR_INSTR_RUNTIME_CALL ||
            operation->runtimeOp != XL6_IR_RUNTIME_CLASS_DERIVED_TO_STRING) continue;
        ++conversions;
        assert(xl6IrCallInstructionErrorAbi(&ir, operation) == XL6_IR_ERROR_ABI_MAY_RAISE &&
            xl6IrCallInstructionErrorPhysicalAbi(&ir, operation) == XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT);
        operation->payloadType = XL6_IR_TYPE_INVALID; reject(&ir, NULL); *operation = saved;
        operation->runtimeIdentityType = operation->payloadType; reject(&ir, NULL); *operation = saved;
        operation->operandCount = 0; reject(&ir, NULL); *operation = saved;
        assert(i + 1 < ir.instructionCount);
        XL6_IrInstruction* test = &ir.instructions[i+1];
        XL6_IrInstruction savedTest = *test;
        assert(test->kind == XL6_IR_INSTR_ERROR_TEST && test->operandCount == 1 &&
            ir.operands[test->firstOperand] == operation->result);
        test->kind = XL6_IR_INSTR_BRANCH; reject(&ir, NULL); *test = savedTest;
        test->falseTarget = test->trueTarget; reject(&ir, NULL); *test = savedTest;
        XL6_IrValueId operand = ir.operands[test->firstOperand];
        ir.operands[test->firstOperand] = XL6_IR_VALUE_INVALID; reject(&ir, NULL);
        ir.operands[test->firstOperand] = operand;
    }
    assert(layouts && conversions); verify(&ir);
    assert(xl6OutputOpenFile(&out, argv[2]) && xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3]) && xl6IrCgenEmitHeader(&out, &ir, "CLASS_TEXT", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[4])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    printf("class text contract: %zu checked conversions, %zu closed layouts, %zu rejected C/header mutations\n",
        conversions, layouts, mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    return 0;
}
