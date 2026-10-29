#include "xlir_continuation_owner_test.h"

static const XL6_IrRuntimeOp boxingOperations[] = {
    XL6_IR_RUNTIME_XVALUE_I64, XL6_IR_RUNTIME_XVALUE_U64,
    XL6_IR_RUNTIME_XVALUE_F64, XL6_IR_RUNTIME_XVALUE_FOREIGN_SCALAR,
    XL6_IR_RUNTIME_XVALUE_TIME, XL6_IR_RUNTIME_XVALUE_STRING_COPY,
    XL6_IR_RUNTIME_XVALUE_NULLABLE_STRING_BOX,
    XL6_IR_RUNTIME_XVALUE_TAGGED_NULLABLE_BOX, XL6_IR_RUNTIME_XVALUE_UNION_BOX
};
static const XL6_IrRuntimeOp containerOperations[] = {
    XL6_IR_RUNTIME_XVALUE_ARRAY_NEW, XL6_IR_RUNTIME_XVALUE_INT_MAP_NEW,
    XL6_IR_RUNTIME_XVALUE_SET_NEW, XL6_IR_RUNTIME_XVALUE_OBJECT_NEW,
    XL6_IR_RUNTIME_XVALUE_CLONE, XL6_IR_RUNTIME_XVALUE_DEEP_CLONE,
    XL6_IR_RUNTIME_XVALUE_ARRAY_APPEND_NEW, XL6_IR_RUNTIME_XVALUE_SET_ADD_NEW
};

int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    size_t i, j, boxes = 0, containers = 0, effects = 0, initializers = 0, constructions = 0, copies = 0;
    unsigned coverage = 0, containerCoverage = 0;
    assert(argc == 4); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_boxing_allocation_outcome.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "boxing_allocation_outcome") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) || !xl6IrLowerModule(&ir, &parse.diags)) {
        for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%d:%d %s\n",
            parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
        return 2;
    }
    verify(&ir);
    for (i = 0; i < ir.nodeCount; ++i) {
        XL6_IrNode* node = &ir.nodes[i];
        if (node->constructionErrorAbi == XL6_IR_ERROR_ABI_MAY_RAISE) {
            ++constructions; node->constructionErrorAbi = XL6_IR_ERROR_ABI_NO_RAISE;
            reject(&ir, NULL); node->constructionErrorAbi = XL6_IR_ERROR_ABI_MAY_RAISE;
        }
        if (node->implicitConversionErrorAbi != XL6_IR_ERROR_ABI_MAY_RAISE) continue;
        ++effects; node->implicitConversionErrorAbi = XL6_IR_ERROR_ABI_NO_RAISE;
        reject(&ir, NULL); node->implicitConversionErrorAbi = XL6_IR_ERROR_ABI_MAY_RAISE;
    }
    for (i = 0; i < ir.valueInitializerCount; ++i) {
        XL6_IrValueInitializer* initializer = &ir.valueInitializers[i];
        XL6_IrRuntimeOp operation = initializer->runtimeOp;
        for (j = 0; j < sizeof(boxingOperations) / sizeof(*boxingOperations); ++j)
            if (operation == boxingOperations[j]) break;
        if (j == sizeof(boxingOperations) / sizeof(*boxingOperations) &&
            initializer->abi != XL6_IR_INITIALIZER_XVALUE_CONTAINER_EMPTY) continue;
        assert(initializer->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE); ++initializers;
        initializer->runtimeOp = XL6_IR_RUNTIME_NONE; reject(&ir, NULL); initializer->runtimeOp = operation;
        initializer->errorAbi = XL6_IR_ERROR_ABI_NO_RAISE; reject(&ir, NULL);
        initializer->errorAbi = XL6_IR_ERROR_ABI_MAY_RAISE;
    }
    for (i = 0; i < ir.intrinsicCallCount; ++i) {
        XL6_IrIntrinsicCall* intrinsic = &ir.intrinsicCalls[i];
        if ((intrinsic->kind != XL6_IR_INTRINSIC_COPY && intrinsic->kind != XL6_IR_INTRINSIC_CLONE) ||
            intrinsic->errorAbi != XL6_IR_ERROR_ABI_MAY_RAISE) continue;
        ++copies; intrinsic->errorAbi = XL6_IR_ERROR_ABI_NO_RAISE; reject(&ir, NULL);
        intrinsic->errorAbi = XL6_IR_ERROR_ABI_MAY_RAISE;
    }
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i];
        XL6_IrInstruction *test, saved;
        if (instruction->kind != XL6_IR_INSTR_RUNTIME_CALL) continue;
        for (j = 0; j < sizeof(boxingOperations) / sizeof(*boxingOperations); ++j)
            if (instruction->runtimeOp == boxingOperations[j]) break;
        if (j != sizeof(boxingOperations) / sizeof(*boxingOperations)) {
            coverage |= 1u << j; ++boxes;
        } else {
            for (j = 0; j < sizeof(containerOperations) / sizeof(*containerOperations); ++j)
                if (instruction->runtimeOp == containerOperations[j]) break;
            if (j == sizeof(containerOperations) / sizeof(*containerOperations)) continue;
            containerCoverage |= 1u << j; ++containers;
        }
        assert(xl6IrCallInstructionErrorAbi(&ir, instruction) == XL6_IR_ERROR_ABI_MAY_RAISE);
        assert(xl6IrCallInstructionErrorPhysicalAbi(&ir, instruction) ==
            (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_ARRAY_APPEND_NEW ||
             instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_SET_ADD_NEW
                 ? XL6_IR_CALL_ERROR_PHYSICAL_SCOPED_BOOL_RESULT : XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT));
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
        if (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_UNION_BOX) {
            XL6_IrTypeId type = instruction->payloadType;
            instruction->payloadType = 0; reject(&ir, NULL); instruction->payloadType = type;
        }
        if (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_FOREIGN_SCALAR) {
            XL6_IrTypeId type = instruction->runtimeIdentityType;
            instruction->runtimeIdentityType = 0; reject(&ir, NULL); instruction->runtimeIdentityType = type;
        }
    }
    assert(coverage == 511u && containerCoverage == 255u && effects >= 10 && initializers >= 13 && constructions >= 10 && copies >= 4); verify(&ir);
    assert(xl6OutputOpenFile(&out, argv[1]) && xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]) && xl6IrCgenEmitHeader(&out, &ir, "BOXING_OUTCOME", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    printf("boxing allocation outcome: nine boxing and eight container families, %llu checked boxes, %llu checked container operations, %llu contextual effects, %llu initializers, %llu constructions, %llu copies, %llu rejected C/header-gated mutations\n",
        (unsigned long long)boxes, (unsigned long long)containers, (unsigned long long)effects,
        (unsigned long long)initializers, (unsigned long long)constructions, (unsigned long long)copies, (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
