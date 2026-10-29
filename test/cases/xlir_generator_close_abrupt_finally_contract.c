#include "xlir_continuation_owner_test.h"

int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    XL6_IrInstruction* saved;
    size_t i, j, guarded = 0, internal = 0, checks = 0;
    assert(argc == 4); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_generator_close_abrupt_finally.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "generator_close_abrupt_finally") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) || !xl6IrLowerModule(&ir, &parse.diags)) {
        for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%d:%d %s\n",
            parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
        return 2;
    }
    verify(&ir); assert(ir.generatorMachineCount == 3);
    saved = xrtMalloc(ir.instructionCount * sizeof(*saved)); assert(saved);
    memcpy(saved, ir.instructions, ir.instructionCount * sizeof(*saved));
    for (i = 0; i < ir.nodeCount; ++i) {
        XL6_IrNode* node = &ir.nodes[i];
        const XL6_IrNode* owner = node;
        const XL6_IrCallTarget* target;
        XL6_IrGeneratorClosePointAbi abi = node->generatorClosePointAbi;
        size_t normalPlans = 0, closePlans = 0, removed = 0;
        if (node->kind != XL6_IR_NODE_BREAK && node->kind != XL6_IR_NODE_CONTINUE) continue;
        while (owner != NULL && owner->kind != XL6_IR_NODE_FUNC_DECL) owner = xl6IrNodeGet(&ir, owner->parent);
        target = owner != NULL ? xl6IrCallTargetGet(&ir, owner->symbol) : NULL;
        if (target == NULL || target->kind != XL6_IR_CALL_TARGET_INTERNAL_GENERATOR) {
            assert(abi == XL6_IR_GENERATOR_CLOSE_POINT_INVALID); continue;
        }
        for (j = 0; j < ir.exitPlanCount; ++j) {
            XL6_IrExitPlan* plan = &ir.exitPlans[j];
            XL6_SemExitKind kind = plan->kind;
            if (plan->origin != i + 1) continue;
            if (plan->point == XL6_IR_EXIT_POINT_GENERATOR_CLOSE) {
                assert(kind == XL6_SEM_EXIT_RETURN); ++closePlans;
                plan->kind = node->exitKind; reject(&ir, NULL); plan->kind = kind;
            } else if (plan->point == XL6_IR_EXIT_POINT_BEFORE_NODE) {
                assert(kind == node->exitKind); ++normalPlans;
            }
        }
        assert(normalPlans == 1);
        if (abi == XL6_IR_GENERATOR_CLOSE_POINT_INVALID) {
            assert(closePlans == 0); ++internal; continue;
        }
        assert(abi == XL6_IR_GENERATOR_CLOSE_POINT_ABRUPT_UNWIND && closePlans == 1); ++guarded;
        node->generatorClosePointAbi = XL6_IR_GENERATOR_CLOSE_POINT_INVALID;
        reject(&ir, NULL); node->generatorClosePointAbi = abi;
        /* Delete every lowered copy of this source boundary, including its
         * exceptional/deferred finally clones, and repair all machine caches.
         * The closing CFG, not the existence of another copy, must reject it. */
        for (j = 0; j < ir.instructionCount; ++j) {
            XL6_IrInstruction* instruction = &ir.instructions[j];
            if (instruction->kind != XL6_IR_INSTR_GENERATOR_CLOSE_TEST || instruction->origin != i + 1) continue;
            assert(instruction->generatorClosePointAbi == abi); ++removed;
            instruction->kind = XL6_IR_INSTR_CONST;
            instruction->immediateText = xrtStrDup("false"); assert(instruction->immediateText);
            instruction->generatorClosePointAbi = XL6_IR_GENERATOR_CLOSE_POINT_INVALID;
            instruction->generatorClosePlan = 0;
        }
        assert(removed != 0); checks += removed;
        assert(xl6IrBuildGeneratorMachines(&ir, &parse.diags));
        reject(&ir, "closing path escapes its cleanup boundary");
        for (j = 0; j < ir.instructionCount; ++j)
            if (ir.instructions[j].immediateText != saved[j].immediateText) xrtFree(ir.instructions[j].immediateText);
        memcpy(ir.instructions, saved, ir.instructionCount * sizeof(*saved));
        assert(xl6IrBuildGeneratorMachines(&ir, &parse.diags)); verify(&ir);
    }
    assert(guarded == 4 && internal == 2); xrtFree(saved);
    assert(xl6OutputOpenFile(&out, argv[1]) && xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]) && xl6IrCgenEmitHeader(&out, &ir, "GENERATOR_CLOSE_ABRUPT", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir);
    assert(!xl6OutputHasFailed(&out)); xl6OutputClose(&out);
    printf("abrupt close: %llu guarded exits, %llu internal exits, %llu lowered checks, %llu C/header-gated mutations\n",
        (unsigned long long)guarded, (unsigned long long)internal, (unsigned long long)checks, (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
