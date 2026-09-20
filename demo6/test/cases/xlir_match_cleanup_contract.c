#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    unsigned effects = 0, transactions = 0, results = 0, loopRoutes = 0, substitutions = 0, bindings = 0, cells = 0, frameBindings = 0;
    assert(argc == 4 || argc == 5); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile(argc == 5 ? argv[4] : "test/cases/xlir_match_cleanup.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "match_cleanup") ||
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
    for (i = 0; i < ir.instructionCount; ++i)
        assert(ir.instructions[i].targetValue == XL6_IR_VALUE_INVALID);
    for (i = 0; i < ir.asyncMachineFrameSlotCount; ++i) {
        XL6_IrAsyncMachineFrameSlot* slot = &ir.asyncMachineFrameSlots[i];
        const XL6_IrStorage* storage = xl6IrStorageGet(&ir, slot->storageSymbol);
        XL6_IrAsyncMachineFrameSlot saved = *slot;
        if (!storage || !xl6IrPatternBindingPayload(&ir, storage->declaration)) continue;
        slot->storageSymbol = 0; reject(&ir); *slot = saved;
        slot->destroyAbi = XL6_IR_ASYNC_FRAME_DESTROY_INVALID; reject(&ir); *slot = saved;
        slot->initBit = SIZE_MAX; reject(&ir); *slot = saved;
        slot->residence = XL6_IR_ASYNC_FRAME_RESIDENCE_SSA_SPILL; reject(&ir); *slot = saved;
        ++frameBindings;
    }
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
    for (i = 0; i < ir.storageCount; ++i) {
        XL6_IrStorage* storage = &ir.storages[i];
        const XL6_IrEnumPayload* payload = xl6IrPatternBindingPayload(&ir, storage->declaration);
        XL6_IrStorageKind kind = storage->kind;
        if (payload == NULL || kind != XL6_IR_STORAGE_TEMPORARY) continue;
        storage->kind = XL6_IR_STORAGE_LOCAL; reject(&ir); storage->kind = kind;
        if (storage->cellType != XL6_IR_TYPE_INVALID) {
            XL6_IrTypeId cell = storage->cellType;
            storage->cellType = storage->type; reject(&ir); storage->cellType = cell;
            storage->cellType = XL6_IR_TYPE_INVALID; reject(&ir); storage->cellType = cell;
            ++cells;
        }
        for (j = 0; j < ir.instructionCount; ++j) {
            XL6_IrInstruction* member = &ir.instructions[j];
            size_t s;
            if (member->targetSymbol != storage->symbol ||
                (member->kind != XL6_IR_INSTR_RELEASE && member->kind != XL6_IR_INSTR_CELL_RELEASE)) continue;
            for (s = 0; s < ir.storageCount; ++s) {
                const XL6_IrStorage* other = &ir.storages[s];
                if (other->symbol == storage->symbol || other->type != storage->type ||
                    other->cellType != storage->cellType || other->owner != storage->owner ||
                    other->kind != XL6_IR_STORAGE_TEMPORARY) continue;
                member->targetSymbol = other->symbol; reject(&ir); member->targetSymbol = storage->symbol;
                break;
            }
        }
        ++bindings;
    }
    if (argc == 4) assert(effects >= 10 && transactions >= 10 && results >= 5 && substitutions >= 8 && bindings >= 12 && cells >= 1);
    else assert(effects >= 3 && transactions >= 1 && results >= 1 && bindings >= 2 && frameBindings >= 8);
    assert(xl6IrVerifyModule(&ir, &parse.diags));
    assert(xl6OutputOpenFile(&out, argv[1])); assert(xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2])); assert(xl6IrCgenEmitHeader(&out, &ir, "MATCH_CLEANUP", &parse.diags));
    xl6OutputClose(&out);
    if (argc == 5) {
        char bindingPath[4096];
        unsigned entries = 0;
        int length = snprintf(bindingPath, sizeof(bindingPath), "%s.bind.h", argv[2]);
        assert(length > 0 && (size_t)length < sizeof(bindingPath) && xl6OutputOpenFile(&out, bindingPath));
        for (i = 0; i < ir.callTargetCount; ++i) {
            const XL6_IrCallTarget* target = &ir.callTargets[i];
            const XL6_IrNode* declaration = xl6IrNodeGet(&ir, target->declaration);
            const XL6_IrNode* name = declaration && declaration->childCount
                ? xl6IrNodeGet(&ir, ir.children[declaration->firstChild]) : NULL;
            if (target->kind == XL6_IR_CALL_TARGET_INTERNAL_ASYNC && name && name->text && strcmp(name->text, "matchAsync") == 0) {
                xl6OutputPrintf(&out, "#define MATCH_ASYNC_START xlir_async_start_%u\n", (unsigned int)target->symbol);
                ++entries;
            }
        }
        assert(entries == 1);
        xl6OutputClose(&out);
    }
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    printf("match cleanup contract: %u effects, %u transactions, %u pending results, %u pre-loop routes, %u same-function owner substitutions, %u rejected mutations\n",
        effects, transactions, results, loopRoutes, substitutions, mutations);
    printf("match binding contract: %u evaluation-lifetime bindings, %u promoted cells\n", bindings, cells);
    if (argc == 5) printf("match frame contract: %u explicit binding lifetimes\n", frameBindings);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
failed:
    for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%d:%d: %s\n",
        parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
    abort();
}
