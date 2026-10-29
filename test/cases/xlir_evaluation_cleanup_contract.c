#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned mutations;
static void reject(const XL6_IrModule* ir)
{
    XL6_DiagList diags = {0}; XL6_Output out; char* text;
    assert(!xl6IrVerifyModule(ir, &diags) && diags.count);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitHeader(&out, ir, "BAD_EVALUATION", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; size_t i, j;
    unsigned effects = 0, transactions = 0, results = 0, loopRoutes = 0, substitutions = 0;
    assert(argc == 4 || argc == 5); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile(argc == 5 ? argv[4] : "test/cases/xlir_evaluation_cleanup.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "evaluation_cleanup") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags)) goto failed;
    assert(xl6IrVerifyModule(&ir, &parse.diags));
    for (i = 0; i < ir.nodeCount; ++i) {
        XL6_IrNode* node = &ir.nodes[i];
        if (node->evaluationErrorAbi != XL6_IR_ERROR_ABI_MAY_RAISE) continue;
        node->evaluationErrorAbi = XL6_IR_ERROR_ABI_INVALID; reject(&ir);
        node->evaluationErrorAbi = XL6_IR_ERROR_ABI_NO_RAISE; reject(&ir);
        node->evaluationErrorAbi = XL6_IR_ERROR_ABI_MAY_RAISE; ++effects;
    }
    if (!xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction *begin = &ir.instructions[i], saved, *test, *unwind;
        size_t end, unwindIndex, plan;
        XL6_IrBlockId success, failure;
        if (begin->kind != XL6_IR_INSTR_CLEANUP_BEGIN || begin->evaluationScope == 0) continue;
        saved = *begin; plan = begin->cleanupPlan;
        begin->evaluationScope = 0; reject(&ir); *begin = saved;
        begin->evaluationScope = i + 2; reject(&ir); *begin = saved;
        begin->cleanupPlan = 0; reject(&ir); *begin = saved;
        begin->parameterScope = i + 1; reject(&ir); *begin = saved;
        for (end = i + 1; end < ir.instructionCount; ++end)
            if (ir.instructions[end].kind == XL6_IR_INSTR_CLEANUP_END && ir.instructions[end].errorScope == i + 1) break;
        assert(end + 1 < ir.instructionCount); test = &ir.instructions[end + 1];
        assert(test->kind == XL6_IR_INSTR_ERROR_TEST);
        success = test->trueTarget; failure = test->falseTarget;
        test->falseTarget = success; reject(&ir); test->falseTarget = failure;
        test->trueTarget = failure; test->falseTarget = success; reject(&ir);
        test->trueTarget = success; test->falseTarget = failure;
        for (unwindIndex = end + 2; unwindIndex < ir.instructionCount; ++unwindIndex)
            if (ir.instructions[unwindIndex].kind == XL6_IR_INSTR_ERROR_UNWIND_BEGIN &&
                ir.instructions[unwindIndex].evaluationScope == i + 1) break;
        assert(unwindIndex < ir.instructionCount); unwind = &ir.instructions[unwindIndex];
        unwind->evaluationScope = 0; reject(&ir); unwind->evaluationScope = i + 1;
        unwind->cleanupPlan = 0; reject(&ir); unwind->cleanupPlan = plan;
        if (begin->evaluationResult != 0) {
            XL6_SymbolId symbol = begin->evaluationResult;
            begin->evaluationResult = 0; reject(&ir); begin->evaluationResult = symbol;
            for (j = 0; j < ir.instructionCount; ++j) {
                XL6_IrInstruction* member = &ir.instructions[j];
                if (member->evaluationScope != i + 1 || member->targetSymbol != symbol) continue;
                member->evaluationScope = 0; reject(&ir); member->evaluationScope = i + 1;
                /* Substituting another same-typed temporary must not change
                 * which result is committed or rolled back. */
                {
                    size_t s;
                    for (s = 0; s < ir.storageCount; ++s) {
                        const XL6_IrStorage* other = &ir.storages[s];
                        if (other->symbol == symbol || other->type != member->type ||
                            other->owner != xl6IrStorageGet(&ir, symbol)->owner ||
                            other->kind != XL6_IR_STORAGE_TEMPORARY) continue;
                        member->targetSymbol = other->symbol; reject(&ir); member->targetSymbol = symbol;
                        ++substitutions;
                        break;
                    }
                }
            }
            ++results;
        }
        ++transactions;
    }
    for (i = 0; i < ir.exitPlanCount; ++i) {
        XL6_IrExitPlan* plan = &ir.exitPlans[i];
        const XL6_IrNode* source = xl6IrNodeGet(&ir, plan->origin);
        const XL6_IrNode* parent = source != NULL ? xl6IrNodeGet(&ir, source->parent) : NULL;
        if (parent == NULL || parent->kind != XL6_IR_NODE_LOOP || parent->sourceKind != XL6_AST_FOR ||
            ir.children[parent->firstChild + 1] != plan->origin) continue;
        for (j = 0; j < plan->stepCount; ++j) {
            XL6_IrExitStep* step = &ir.exitSteps[plan->firstStep + j];
            assert(step->origin != source->parent);
        }
        if (plan->stepCount != 0) {
            XL6_IrExitStep saved = ir.exitSteps[plan->firstStep];
            ir.exitSteps[plan->firstStep].kind = XL6_IR_EXIT_STEP_LOOP_RESOURCE;
            ir.exitSteps[plan->firstStep].origin = source->parent;
            reject(&ir); ir.exitSteps[plan->firstStep] = saved;
            ++loopRoutes;
        }
    }
    if (argc == 4) assert(effects >= 10 && transactions >= 10 && results >= 5 && loopRoutes >= 1 && substitutions >= 8);
    else assert(effects >= 3 && transactions >= 3 && results >= 3);
    assert(xl6IrVerifyModule(&ir, &parse.diags));
    assert(xl6OutputOpenFile(&out, argv[1])); assert(xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2])); assert(xl6IrCgenEmitHeader(&out, &ir, "EVALUATION_CLEANUP", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    printf("evaluation cleanup contract: %u effects, %u transactions, %u pending results, %u pre-loop routes, %u same-function owner substitutions, %u rejected mutations\n",
        effects, transactions, results, loopRoutes, substitutions, mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
failed:
    for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%d:%d: %s\n",
        parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
    abort();
}
