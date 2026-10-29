#include "xlir_continuation_owner_test.h"

int main(int argc, char** argv)
{
    const char* names[] = {"scalarDefault", "ownedDefault", "productDefault", "voidDefault", "failingDefault"};
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    XL6_SymbolId targets[5] = {0}, adapters[5] = {0};
    XL6_IrTypeId product = 0;
    size_t i, j, n, boundaries = 0, cleanups = 0, pending = 0;
    size_t adapterInstantiations = 0;
    char binding[1024];
    assert(argc == 4); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_adapter_cleanup_outcome.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "adapter_cleanup_outcome") ||
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
        for (n = 0; n < 5; ++n) if (name && name->text && !strcmp(name->text, names[n])) {
            assert(!targets[n]); targets[n] = ir.functions[i].symbol;
            if (n == 2) product = ir.functions[i].returnType;
        }
    }
    for (i = 0; i < ir.closureTargetCount; ++i) for (n = 0; n < 5; ++n)
        if (ir.closureTargets[i].adaptedTargetSymbol == targets[n]) {
            assert(!adapters[n]); adapters[n] = ir.closureTargets[i].targetSymbol;
        }
    for (n = 0; n < 5; ++n) assert(targets[n] && adapters[n]);
    assert(xl6IrCallTargetGet(&ir, targets[0])->errorAbi == XL6_IR_ERROR_ABI_NO_RAISE);
    assert(xl6IrCallTargetGet(&ir, targets[3])->errorAbi == XL6_IR_ERROR_ABI_NO_RAISE);
    assert(product);
    for (i = 0; i < ir.instantiationCount; ++i) {
        XL6_IrInstantiation saved = ir.instantiations[i];
        XL6_IrFunction* function = &ir.functions[saved.function - 1];
        XL6_IrInstruction* commit;
        const XL6_IrInstruction* receiver;
        if (function->kind != XL6_IR_FUNCTION_NAMED_ADAPTER) continue;
        ++adapterInstantiations;
        commit = &ir.instructions[saved.commitInstruction];
        receiver = NULL;
        for (j = 0; j < ir.instructionCount; ++j)
            if (ir.instructions[j].result == saved.receiver) {
                receiver = &ir.instructions[j];
                break;
            }
        /* The default expression remains the diagnostic/invocation source,
         * while the generated adapter is its independent execution owner. */
        assert(saved.site != function->declaration && receiver &&
            receiver->origin == saved.site && commit->origin != saved.site &&
            xl6IrInstructionExecutionOrigin(&ir, commit) == function->declaration);
        ir.instantiations[i].site = function->declaration;
        reject(&ir, NULL);
        ir.instantiations[i] = saved;
    }
    assert(adapterInstantiations == 10);
    for (i = 0; i < ir.exitPlanCount; ++i) {
        XL6_IrExitPlan* plan = &ir.exitPlans[i], saved;
        XL6_IrExitStep step;
        if (plan->point != XL6_IR_EXIT_POINT_FUNCTION_FAILURE) continue;
        ++boundaries; saved = *plan;
        assert(plan->kind == XL6_SEM_EXIT_THROW && plan->stepCount == 2 && !plan->cleanupCount);
        assert(ir.exitSteps[plan->firstStep].kind == XL6_IR_EXIT_STEP_PARAMETERS &&
            ir.exitSteps[plan->firstStep].failureTarget == plan->origin &&
            ir.exitSteps[plan->firstStep + 1].kind == XL6_IR_EXIT_STEP_TERMINAL);
        plan->point = XL6_IR_EXIT_POINT_BEFORE_NODE; reject(&ir, NULL); *plan = saved;
        plan->targetRegion = 1; reject(&ir, NULL); *plan = saved;
        plan->origin = ir.root; reject(&ir, NULL); *plan = saved;
        plan->stepCount = 1; reject(&ir, NULL); *plan = saved;
        step = ir.exitSteps[plan->firstStep];
        ir.exitSteps[plan->firstStep].failureTarget = ir.root; reject(&ir, NULL);
        ir.exitSteps[plan->firstStep] = step;
    }
    for (i = 0; i < ir.functionCount; ++i) {
        XL6_IrFunction* function = &ir.functions[i];
        if (function->kind != XL6_IR_FUNCTION_NAMED_ADAPTER) continue;
        {
            XL6_IrFunction saved = *function;
            function->errorAbi = XL6_IR_ERROR_ABI_NO_RAISE; reject(&ir, NULL); *function = saved;
            function->firstInstruction = ir.instructionCount + 1; reject(&ir, NULL); *function = saved;
            function->instructionCount = SIZE_MAX; reject(&ir, NULL); *function = saved;
        }
        for (j = function->firstInstruction; j < function->firstInstruction + function->instructionCount; ++j) {
            XL6_IrInstruction* member = &ir.instructions[j], saved = *member;
            if (member->cleanupPlan) {
                const XL6_IrExitPlan* plan = &ir.exitPlans[member->cleanupPlan - 1];
                assert(plan->point == XL6_IR_EXIT_POINT_FUNCTION_FAILURE && plan->origin == function->declaration);
                for (n = 0; n < ir.exitPlanCount; ++n)
                    if (ir.exitPlans[n].point == XL6_IR_EXIT_POINT_BEFORE_NODE &&
                        ir.exitPlans[n].kind == XL6_SEM_EXIT_THROW &&
                        ir.exitPlans[n].origin == function->declaration && ir.exitPlans[n].targetRegion != 0) break;
                assert(n < ir.exitPlanCount);
                member->cleanupPlan = n + 1; reject(&ir, NULL); *member = saved;
                member->cleanupPlan = 0; reject(&ir, NULL); *member = saved;
            }
            if (member->kind == XL6_IR_INSTR_RELEASE &&
                xl6IrTypeReleaseEffect(&ir, member->type) == XL6_IR_RELEASE_EFFECT_MAY_RAISE) {
                assert(member->errorScope);
                member->errorScope = 0; reject(&ir, NULL); *member = saved;
            }
            if (member->kind == XL6_IR_INSTR_CLEANUP_BEGIN && member->evaluationScope) {
                ++cleanups; pending += member->evaluationResult != 0;
                if (member->evaluationResult) {
                    member->evaluationResult = 0; reject(&ir, NULL); *member = saved;
                }
            }
        }
    }
    assert(boundaries == 5 && cleanups == 5 && pending == 3); verify(&ir);
    assert(xl6OutputOpenFile(&out, argv[1]) && xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]) && xl6IrCgenEmitHeader(&out, &ir, "ADAPTER_CLEANUP", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    assert(snprintf(binding, sizeof(binding), "%s.bind.h", argv[2]) > 0 && xl6OutputOpenFile(&out, binding));
    for (n = 0; n < 5; ++n) xl6OutputPrintf(&out, "#define ADAPTER_CLEANUP_%lu xlir_fn_%u\n", (unsigned long)n, adapters[n]);
    xl6OutputPrintf(&out, "#define ADAPTER_CLEANUP_PRODUCT xlir_result_t%u\n", product); xl6OutputClose(&out);
    printf("adapter cleanup: %llu independent boundaries, %llu default-field instances, %llu cleanup transactions, %llu pending results, %llu rejected C/header-gated mutations\n",
        (unsigned long long)boundaries, (unsigned long long)adapterInstantiations,
        (unsigned long long)cleanups, (unsigned long long)pending,
        (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
