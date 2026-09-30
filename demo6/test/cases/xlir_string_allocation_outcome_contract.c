#include "xlir_continuation_owner_test.h"

int main(int argc, char** argv)
{
    const char* names[] = {"concatPair", "concatNested", "concatInterpolation", "concatCompound", "concatDefault", "concatInout",
        "allocationLiteral", "allocationInteger", "allocationPrefix", "allocationEmpty", "allocationBool", "allocationUnsigned",
        "allocationFloat", "allocationTime", "allocationType", "allocationDynamic", "allocationDerived", "allocationNullable", "allocationProtocol"};
    const XL6_IrRuntimeOp operations[] = {XL6_IR_RUNTIME_STRING_DUP, XL6_IR_RUNTIME_STRING_COPY, XL6_IR_RUNTIME_STRING_NULLABLE_COPY,
        XL6_IR_RUNTIME_STRING_CONCAT, XL6_IR_RUNTIME_BOOL_TO_STRING, XL6_IR_RUNTIME_I64_TO_STRING, XL6_IR_RUNTIME_U64_TO_STRING,
        XL6_IR_RUNTIME_F64_TO_STRING, XL6_IR_RUNTIME_TIME_TO_STRING, XL6_IR_RUNTIME_TYPE_ID_TO_STRING,
        XL6_IR_RUNTIME_XVALUE_TO_STRING, XL6_IR_RUNTIME_CLASS_DERIVED_TO_STRING,
        XL6_IR_RUNTIME_STRING_FROM_UTF8};
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    XL6_SymbolId symbols[19] = {0}, adapter = 0;
    size_t i, n, allocations = 0; unsigned operationMask = 0; char binding[1024];
    assert(argc == 4); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_string_allocation_outcome.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "string_allocation_outcome") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) || !xl6IrLowerModule(&ir, &parse.diags)) {
        for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%d:%d %s\n",
            parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
        return 2;
    }
    verify(&ir);
    for (i = 0; i < ir.functionCount; ++i) {
        const XL6_IrNode* declaration = xl6IrNodeGet(&ir, ir.functions[i].declaration);
        const XL6_IrNode* name = declaration && declaration->childCount
            ? xl6IrNodeGet(&ir, ir.children[declaration->firstChild]) : NULL;
        for (n = 0; n < 19; ++n) if (name && name->text && !strcmp(name->text, names[n])) {
            assert(!symbols[n]); symbols[n] = ir.functions[i].symbol;
        }
    }
    for (n = 0; n < 19; ++n) assert(symbols[n]);
    for (i = 0; i < ir.closureTargetCount; ++i)
        if (ir.closureTargets[i].adaptedTargetSymbol == symbols[4]) {
            assert(!adapter); adapter = ir.closureTargets[i].targetSymbol;
        }
    assert(adapter);
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i];
        XL6_IrInstruction *test, saved;
        XL6_IrCallErrorPhysicalAbi physical;
        if (instruction->kind != XL6_IR_INSTR_RUNTIME_CALL) continue;
        physical = xl6IrRuntimeStringAllocationErrorPhysicalAbi(instruction->runtimeOp);
        if (physical == XL6_IR_CALL_ERROR_PHYSICAL_INVALID) continue;
        for (n = 0; n < 13; ++n) if (instruction->runtimeOp == operations[n]) break;
        assert(n < 13); operationMask |= 1u << n; ++allocations;
        assert(xl6IrCallInstructionErrorAbi(&ir, instruction) == XL6_IR_ERROR_ABI_MAY_RAISE);
        assert(xl6IrCallInstructionErrorPhysicalAbi(&ir, instruction) == physical);
        assert(physical == (instruction->runtimeOp == XL6_IR_RUNTIME_STRING_NULLABLE_COPY
            ? XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT : XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT));
        assert(i + 1 < ir.instructionCount); test = &ir.instructions[i + 1]; saved = *test;
        assert(test->kind == XL6_IR_INSTR_ERROR_TEST && test->operandCount == 1 &&
            ir.operands[test->firstOperand] == instruction->result);
        test->falseTarget = test->trueTarget; reject(&ir, NULL); *test = saved;
        test->kind = XL6_IR_INSTR_BRANCH; reject(&ir, NULL); *test = saved;
        test->kind = XL6_IR_INSTR_DISCARD; reject(&ir, NULL); *test = saved;
        {
            XL6_IrValueId value = ir.operands[test->firstOperand];
            ir.operands[test->firstOperand] = instruction->operandCount ? ir.operands[instruction->firstOperand] : 0;
            reject(&ir, NULL); ir.operands[test->firstOperand] = value;
        }
    }
    for (i = 0; i < ir.intrinsicCallCount; ++i) {
        XL6_IrIntrinsicCall* intrinsic = &ir.intrinsicCalls[i];
        if (intrinsic->kind != XL6_IR_INTRINSIC_STRING_CONVERT) continue;
        assert(intrinsic->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE);
        intrinsic->errorAbi = XL6_IR_ERROR_ABI_NO_RAISE; reject(&ir, NULL);
        intrinsic->errorAbi = XL6_IR_ERROR_ABI_MAY_RAISE;
    }
    assert(operationMask == 8191u && allocations >= 25); verify(&ir);
    assert(xl6OutputOpenFile(&out, argv[1]) && xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]) && xl6IrCgenEmitHeader(&out, &ir, "STRING_OUTCOME", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    assert(snprintf(binding, sizeof(binding), "%s.bind.h", argv[2]) > 0 && xl6OutputOpenFile(&out, binding));
    for (n = 0; n < 19; ++n) xl6OutputPrintf(&out, "#define STRING_OUTCOME_%lu xlir_fn_%u\n", (unsigned long)n, symbols[n]);
    xl6OutputPrintf(&out, "#define STRING_OUTCOME_ADAPTER xlir_fn_%u\n", adapter); xl6OutputClose(&out);
    printf("string allocation outcome: 13 operation families, %llu checked allocations, %llu rejected C/header-gated mutations\n",
        (unsigned long long)allocations, (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
