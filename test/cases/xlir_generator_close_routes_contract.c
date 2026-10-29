#include "xlir_continuation_owner_test.h"

int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    size_t i, checks = 0, drains = 0, plans = 0;
    assert(argc == 4 || argc == 5); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile(argc == 5 ? argv[4] : "test/cases/xlir_generator_close_routes.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "generator_close_routes") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) || !xl6IrLowerModule(&ir, &parse.diags)) {
        for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%d:%d %s\n",
            parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
        return 2;
    }
    verify(&ir); assert(ir.generatorMachineCount == (argc == 5 ? 3u : 4u));
    for (i = 0; i < ir.nodeCount; ++i) {
        XL6_IrGeneratorClosePointAbi saved = ir.nodes[i].generatorClosePointAbi;
        if (saved == XL6_IR_GENERATOR_CLOSE_POINT_INVALID) continue;
        ir.nodes[i].generatorClosePointAbi = XL6_IR_GENERATOR_CLOSE_POINT_INVALID;
        reject(&ir, NULL); ir.nodes[i].generatorClosePointAbi = saved;
    }
    for (i = 0; i < ir.exitPlanCount; ++i) {
        XL6_IrExitPlan* plan = &ir.exitPlans[i], saved = *plan;
        if (plan->point != XL6_IR_EXIT_POINT_GENERATOR_CLOSE) continue;
        ++plans;
        plan->kind = XL6_SEM_EXIT_THROW; reject(&ir, NULL); *plan = saved;
        plan->point = XL6_IR_EXIT_POINT_BEFORE_NODE; reject(&ir, NULL); *plan = saved;
        plan->targetScopeDepth = 1; reject(&ir, NULL); *plan = saved;
        plan->stepCount = 0; reject(&ir, NULL); *plan = saved;
    }
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i], saved = *instruction;
        if (instruction->kind == XL6_IR_INSTR_YIELD) {
            drains += instruction->generatorClosePointAbi == XL6_IR_GENERATOR_CLOSE_POINT_YIELD_DRAIN_FINALLY;
            instruction->generatorClosePointAbi = XL6_IR_GENERATOR_CLOSE_POINT_INVALID;
            reject(&ir, NULL); *instruction = saved;
        }
        if (instruction->kind == XL6_IR_INSTR_GENERATOR_CLOSE_TEST) {
            XL6_IrInstruction* branch = &ir.instructions[i + 1], savedBranch = *branch;
            ++checks; assert(branch->kind == XL6_IR_INSTR_BRANCH);
            instruction->generatorClosePlan = 0; reject(&ir, NULL); *instruction = saved;
            instruction->generatorClosePointAbi = XL6_IR_GENERATOR_CLOSE_POINT_YIELD_DRAIN_FINALLY;
            reject(&ir, NULL); *instruction = saved;
            instruction->type = XL6_IR_TYPE_INVALID; reject(&ir, NULL); *instruction = saved;
            branch->trueTarget = branch->falseTarget; reject(&ir, NULL); *branch = savedBranch;
        }
    }
    for (i = 0; i < ir.generatorMachineCount; ++i) {
        XL6_IrGeneratorCloseAbi saved = ir.generatorMachines[i].closeAbi;
        ir.generatorMachines[i].closeAbi = XL6_IR_GENERATOR_CLOSE_INVALID;
        reject(&ir, NULL); ir.generatorMachines[i].closeAbi = saved;
    }
    for (i = 0; i < ir.generatorSuspensionCount; ++i) {
        XL6_IrGeneratorClosePointAbi saved = ir.generatorSuspensions[i].closePointAbi;
        ir.generatorSuspensions[i].closePointAbi = XL6_IR_GENERATOR_CLOSE_POINT_INVALID;
        reject(&ir, NULL); ir.generatorSuspensions[i].closePointAbi = saved;
    }
    /* Rebuild every cached machine target after reversing the first close
     * branch. Rejection must prove the source-independent unwind prefix,
     * not just notice stale machine metadata. */
    for (i = 0; i < ir.instructionCount; ++i) {
        if (ir.instructions[i].kind == XL6_IR_INSTR_GENERATOR_CLOSE_TEST &&
            ir.blocks[ir.instructions[i + 1].falseTarget - 1].instructionCount != 0 &&
            ir.instructions[ir.blocks[ir.instructions[i + 1].falseTarget - 1].firstInstruction].kind == XL6_IR_INSTR_CONST) {
            XL6_IrInstruction* branch = &ir.instructions[i + 1], saved = *branch;
            branch->trueTarget = saved.falseTarget; branch->falseTarget = saved.trueTarget;
            assert(xl6IrBuildGeneratorMachines(&ir, &parse.diags));
            reject(&ir, "cleanup-only unwind prefix");
            *branch = saved; assert(xl6IrBuildGeneratorMachines(&ir, &parse.diags));
            break;
        }
    }
    assert(i < ir.instructionCount);
    /* Removing the boundary after a suspended finally must not let closing
     * resume ordinary source work. Repair the cached machine as an attacker
     * could: mere shape/cached-target agreement is not proof of this contract. */
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i], saved = *instruction;
        if (instruction->kind == XL6_IR_INSTR_GENERATOR_CLOSE_TEST &&
            instruction->generatorClosePointAbi == XL6_IR_GENERATOR_CLOSE_POINT_AFTER_TRY) {
            char* constant = xrtStrDup("false"); assert(constant);
            instruction->kind = XL6_IR_INSTR_CONST;
            instruction->generatorClosePointAbi = XL6_IR_GENERATOR_CLOSE_POINT_INVALID;
            instruction->generatorClosePlan = 0; instruction->immediateText = constant;
            assert(xl6IrBuildGeneratorMachines(&ir, &parse.diags));
            fputs("Checking coherent deletion of after-finally close boundary\n", stderr);
            reject(&ir, NULL);
            *instruction = saved; xrtFree(constant);
            assert(xl6IrBuildGeneratorMachines(&ir, &parse.diags));
            break;
        }
    }
    assert(i < ir.instructionCount);
    assert(checks > 3 && drains > 3 && plans > 3); verify(&ir);
    assert(xl6OutputOpenFile(&out, argv[1]) && xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]) && xl6IrCgenEmitHeader(&out, &ir, "GENERATOR_CLOSE_ROUTES", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir);
    assert(!xl6OutputHasFailed(&out)); xl6OutputClose(&out);
    printf("generator close: %llu checks, %llu cleanup yields, %llu plans, %llu C/header-gated mutations\n",
        (unsigned long long)checks, (unsigned long long)drains, (unsigned long long)plans, (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
