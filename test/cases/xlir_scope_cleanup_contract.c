#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>

static size_t mutations;
static void reject(XL6_IrModule* ir, const char* field)
{
    XL6_DiagList diags = {0};
    XL6_Output out;
    char* text;
    if (xl6IrVerifyModule(ir, &diags)) {
        fprintf(stderr, "scope cleanup mutation escaped: %s\n", field); assert(0);
    }
    assert(diags.count != 0 && xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL);
    assert(text == NULL || text[0] == '\0'); xrtFree(text);
    assert(xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitHeader(&out, ir, "REJECTED_SCOPE_CLEANUP", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL);
    assert(text == NULL || text[0] == '\0'); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}
#define MUTATE(Type, item, field, value) do { \
    Type saved = *(item); (item)->field = (value); \
    reject(&ir, #item "." #field "=" #value); *(item) = saved; \
} while (0)

int main(void)
{
    XL6_ParseResult parse;
    XL6_IrModule ir;
    size_t i, j, plans = 0, regions = 0, remaining = 0, cells = 0;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_scope_cleanup_outcome.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "scope_cleanup_contract") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) {
        for (i = 0; i < parse.diags.count; ++i) puts(parse.diags.items[i].message);
        return 1;
    }
    /* Semantic walks must never run ahead of structural verification. */
    {
        XL6_IrNode* root = &ir.nodes[ir.root - 1];
        XL6_IrNodeId savedChild;
        assert(root->childCount != 0);
        MUTATE(XL6_IrNode, root, parent, ir.root);
        MUTATE(XL6_IrNode, root, firstChild, ir.childCount + 1);
        MUTATE(XL6_IrNode, root, childCount, ir.childCount + 1);
        savedChild = ir.children[root->firstChild];
        ir.children[root->firstChild] = ir.root;
        reject(&ir, "root child cycle");
        ir.children[root->firstChild] = savedChild;
    }
    {
        XL6_IrModule malformed = ir;
        XL6_IrNode nodes[3] = {{0}};
        XL6_IrNodeId children[2] = {3, 2};
        nodes[0].kind = nodes[1].kind = nodes[2].kind = XL6_IR_NODE_BLOCK;
        nodes[1].parent = 3; nodes[1].childCount = 1;
        nodes[2].parent = 2; nodes[2].firstChild = 1; nodes[2].childCount = 1;
        malformed.root = 1; malformed.nodes = nodes; malformed.nodeCount = 3;
        malformed.children = children; malformed.childCount = 2;
        reject(&malformed, "disconnected cycle with consistent parents");
    }
    for (i = 0; i < ir.exitPlanCount; ++i) {
        XL6_IrExitPlan* plan = &ir.exitPlans[i];
        XL6_IrNode* origin;
        if (plan->point != XL6_IR_EXIT_POINT_AFTER_SCOPE_FAILURE) continue;
        ++plans; origin = &ir.nodes[plan->origin - 1];
        MUTATE(XL6_IrExitPlan, plan, point, XL6_IR_EXIT_POINT_AFTER_SCOPE);
        MUTATE(XL6_IrExitPlan, plan, kind, XL6_SEM_EXIT_RETURN);
        MUTATE(XL6_IrExitPlan, plan, targetScopeDepth, plan->targetScopeDepth + 1);
        MUTATE(XL6_IrExitPlan, plan, targetRegion, plan->targetRegion + 1);
        MUTATE(XL6_IrNode, origin, firstChild, ir.childCount + 1);
        MUTATE(XL6_IrNode, origin, childCount, ir.childCount + 1);
        if (plan->cleanupCount != 0) {
            MUTATE(XL6_IrExitPlan, plan, cleanupCount, plan->cleanupCount - 1);
            for (j = 0; j < plan->cleanupCount; ++j) {
                XL6_IrCleanupAction* action = &ir.cleanupActions[plan->firstCleanup + j];
                ++remaining;
                cells += action->kind == XL6_IR_CLEANUP_CELL_RELEASE;
                MUTATE(XL6_IrCleanupAction, action, origin, plan->origin);
                MUTATE(XL6_IrCleanupAction, action, targetSymbol, 0);
                if (j + 1 < plan->cleanupCount) {
                    XL6_IrCleanupAction saved = *action;
                    *action = action[1]; action[1] = saved;
                    reject(&ir, "remaining owner order");
                    action[1] = *action; *action = saved;
                }
            }
        }
    }
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* begin = &ir.instructions[i];
        const XL6_IrNode* origin = xl6IrNodeGet(&ir, begin->origin);
        XL6_IrInstruction *end, *test, *route;
        size_t scope = begin->errorScope;
        if (begin->kind != XL6_IR_INSTR_CLEANUP_BEGIN || origin == NULL || origin->kind != XL6_IR_NODE_BLOCK)
            continue;
        ++regions;
        MUTATE(XL6_IrInstruction, begin, errorScope, 0);
        MUTATE(XL6_IrInstruction, begin, kind, XL6_IR_INSTR_ERROR_CLEAR);
        for (j = i + 1; j < ir.instructionCount; ++j) {
            XL6_IrInstruction* member = &ir.instructions[j];
            if (member->kind == XL6_IR_INSTR_CLEANUP_END && member->errorScope == scope) break;
            MUTATE(XL6_IrInstruction, member, errorScope, 0);
            if (member->kind == XL6_IR_INSTR_CLEANUP_CAPTURE)
                MUTATE(XL6_IrInstruction, member, kind, XL6_IR_INSTR_ERROR_CLEAR);
            else if (member->kind == XL6_IR_INSTR_RELEASE || member->kind == XL6_IR_INSTR_CELL_RELEASE)
                MUTATE(XL6_IrInstruction, member, targetSymbol, 0);
        }
        assert(j + 2 < ir.instructionCount);
        end = &ir.instructions[j]; test = end + 1;
        MUTATE(XL6_IrInstruction, end, kind, XL6_IR_INSTR_ERROR_UNWIND_END);
        MUTATE(XL6_IrInstruction, end, result, XL6_IR_VALUE_INVALID);
        MUTATE(XL6_IrInstruction, test, falseTarget, test->trueTarget);
        MUTATE(XL6_IrInstruction, test, kind, XL6_IR_INSTR_JUMP);
        assert(end[2].kind == XL6_IR_INSTR_ERROR_UNWIND_BEGIN);
        scope = end[2].errorScope;
        for (j += 3; j < ir.instructionCount; ++j)
            if (ir.instructions[j].kind == XL6_IR_INSTR_ERROR_UNWIND_END && ir.instructions[j].errorScope == scope) break;
        assert(j + 1 < ir.instructionCount); route = &ir.instructions[j + 1];
        if (route->kind == XL6_IR_INSTR_JUMP) {
            MUTATE(XL6_IrInstruction, route, trueTarget, test->trueTarget);
            MUTATE(XL6_IrInstruction, route, kind, XL6_IR_INSTR_THROW);
        } else {
            assert(route->kind == XL6_IR_INSTR_THROW);
            MUTATE(XL6_IrInstruction, route, kind, XL6_IR_INSTR_RETURN);
        }
    }
    assert(plans >= 20 && regions >= 10 && remaining >= 5 && cells >= 1);
    assert(xl6IrVerifyModule(&ir, NULL));
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    printf("scope cleanup contract: %lu failure plans, %lu cleanup regions, %lu remaining bindings, %lu cell actions, %lu mutations rejected before C/header output\n",
        (unsigned long)plans, (unsigned long)regions, (unsigned long)remaining,
        (unsigned long)cells, (unsigned long)mutations);
    return 0;
}
