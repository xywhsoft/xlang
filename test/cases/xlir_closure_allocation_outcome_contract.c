#include "xlir_continuation_owner_test.h"

static void suspension(const char* prefix)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    size_t i, nullableStrings = 0, before = mutations; char path[1024];
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_closure_allocation_suspension.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "closure_allocation_suspension") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) || !xl6IrLowerModule(&ir, &parse.diags)) {
        for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%d:%d %s\n",
            parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
        assert(0);
    }
    verify(&ir); assert(ir.asyncMachineCount && ir.generatorMachineCount);
    for (i = 0; i < ir.closureCaptureCount; ++i) {
        XL6_IrClosureCapture* capture = &ir.closureCaptures[i];
        if (capture->abi != XL6_IR_CLOSURE_CAPTURE_COPY_NULLABLE_STRING) continue;
        ++nullableStrings;
        capture->abi = XL6_IR_CLOSURE_CAPTURE_COPY_STRING; reject(&ir, NULL);
        capture->abi = XL6_IR_CLOSURE_CAPTURE_COPY_NULLABLE_STRING;
    }
    assert(nullableStrings == 3); verify(&ir);
    assert(snprintf(path, sizeof(path), "%s.suspension.c", prefix) > 0 && xl6OutputOpenFile(&out, path));
    assert(xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(snprintf(path, sizeof(path), "%s.suspension.h", prefix) > 0 && xl6OutputOpenFile(&out, path));
    assert(xl6IrCgenEmitHeader(&out, &ir, "CLOSURE_SUSPENSION", &parse.diags)); xl6OutputClose(&out);
    assert(snprintf(path, sizeof(path), "%s.suspension.json", prefix) > 0 && xl6OutputOpenFile(&out, path));
    xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    printf("closure suspension: %llu nullable capture contracts, %llu rejected C/header-gated mutations\n",
        (unsigned long long)nullableStrings, (unsigned long long)(mutations - before));
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
}

int main(int argc, char** argv)
{
    const char* names[] = {"closureNamed", "closureEmpty", "closureString", "closureMixed",
        "closureDefaultNamed", "closureDefaultAnonymous", "closureConditional", "closureNullable", "closureField", "closureCaught", "closureNullableString"};
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    XL6_SymbolId symbols[11] = {0};
    size_t i, n, allocations = 0, creatorBoundaries = 0, nullableStrings = 0; char binding[1024];
    assert(argc == 4); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_closure_allocation_outcome.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "closure_allocation_outcome") ||
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
        for (n = 0; n < 11; ++n) if (name && name->text && !strcmp(name->text, names[n])) {
            assert(!symbols[n]); symbols[n] = ir.functions[i].symbol;
            assert(ir.functions[i].errorAbi == (n == 9 ? XL6_IR_ERROR_ABI_NO_RAISE : XL6_IR_ERROR_ABI_MAY_RAISE));
        }
    }
    for (n = 0; n < 11; ++n) assert(symbols[n]);
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i];
        XL6_IrInstruction *test, saved; XL6_IrNode* origin; XL6_IrErrorAbi effect;
        if (instruction->kind != XL6_IR_INSTR_CLOSURE_MAKE) continue;
        ++allocations;
        assert(xl6IrCallInstructionErrorAbi(&ir, instruction) == XL6_IR_ERROR_ABI_MAY_RAISE);
        assert(xl6IrCallInstructionErrorPhysicalAbi(&ir, instruction) == XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT);
        assert(instruction->origin && instruction->origin <= ir.nodeCount);
        origin = &ir.nodes[instruction->origin - 1]; effect = origin->constructionErrorAbi;
        assert(effect == XL6_IR_ERROR_ABI_MAY_RAISE);
        origin->constructionErrorAbi = XL6_IR_ERROR_ABI_NO_RAISE; reject(&ir, NULL); origin->constructionErrorAbi = effect;
        assert(i + 1 < ir.instructionCount); test = &ir.instructions[i + 1]; saved = *test;
        assert(test->kind == XL6_IR_INSTR_ERROR_TEST && test->operandCount == 1 && !test->targetSymbol &&
            ir.operands[test->firstOperand] == instruction->result);
        test->falseTarget = test->trueTarget; reject(&ir, NULL); *test = saved;
        test->kind = XL6_IR_INSTR_BRANCH; reject(&ir, NULL); *test = saved;
        test->kind = XL6_IR_INSTR_DISCARD; reject(&ir, NULL); *test = saved;
        test->targetSymbol = instruction->targetSymbol; reject(&ir, NULL); *test = saved;
        {
            XL6_IrValueId value = ir.operands[test->firstOperand];
            ir.operands[test->firstOperand] = 0; reject(&ir, NULL); ir.operands[test->firstOperand] = value;
        }
        for (n = 0; n < ir.exitPlanCount; ++n) {
            XL6_IrExitPlan* plan = &ir.exitPlans[n]; XL6_IrExitPlan savedPlan;
            if (plan->origin != instruction->origin || plan->point != XL6_IR_EXIT_POINT_BEFORE_NODE ||
                plan->kind != XL6_SEM_EXIT_THROW) continue;
            savedPlan = *plan;
            if (origin->sourceKind == XL6_AST_ANON_FUNC &&
                (plan->targetScopeDepth != origin->scopeDepth || plan->targetRegion != origin->region)) {
                ++creatorBoundaries;
                plan->targetScopeDepth = origin->scopeDepth; plan->targetRegion = origin->region;
                reject(&ir, NULL); *plan = savedPlan;
            }
            if (plan->cleanupCount) {
                XL6_IrCleanupAction* action = &ir.cleanupActions[plan->firstCleanup];
                XL6_IrNodeId owner = action->ownerDeclaration;
                action->ownerDeclaration = instruction->origin; reject(&ir, NULL); action->ownerDeclaration = owner;
            }
        }
    }
    for (i = 0; i < ir.closureCaptureCount; ++i) {
        XL6_IrClosureCapture* capture = &ir.closureCaptures[i];
        XL6_IrClosureCaptureAbi abi = capture->abi;
        if (abi == XL6_IR_CLOSURE_CAPTURE_COPY_STRING) {
            capture->abi = XL6_IR_CLOSURE_CAPTURE_COPY_NULLABLE_STRING; reject(&ir, NULL); capture->abi = abi;
        } else if (abi == XL6_IR_CLOSURE_CAPTURE_COPY_NULLABLE_STRING) {
            ++nullableStrings;
            capture->abi = XL6_IR_CLOSURE_CAPTURE_COPY_STRING; reject(&ir, NULL); capture->abi = abi;
        }
    }
    assert(allocations >= 10 && creatorBoundaries && nullableStrings); verify(&ir);
    assert(xl6OutputOpenFile(&out, argv[1]) && xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]) && xl6IrCgenEmitHeader(&out, &ir, "CLOSURE_OUTCOME", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    assert(snprintf(binding, sizeof(binding), "%s.bind.h", argv[2]) > 0 && xl6OutputOpenFile(&out, binding));
    for (n = 0; n < 11; ++n) xl6OutputPrintf(&out, "#define CLOSURE_OUTCOME_%lu xlir_fn_%u\n", (unsigned long)n, symbols[n]);
    xl6OutputClose(&out);
    printf("closure allocation outcome: %llu checked closures, %llu creator boundaries, %llu rejected C/header-gated mutations\n",
        (unsigned long long)allocations, (unsigned long long)creatorBoundaries, (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    suspension(argv[1]); return 0;
}
