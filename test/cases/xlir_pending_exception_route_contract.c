#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t mutations, plans, regions, releases, ordered;
static void reject(XL6_IrModule* ir, const char* field)
{
    XL6_DiagList diags = {0};
    if (xl6IrVerifyModule(ir, &diags)) {
        fprintf(stderr, "pending exception route mutation escaped: %s\n", field); assert(0);
    }
    assert(diags.count != 0);
    if (ir->stage == XL6_IR_STAGE_LOWERED) {
        XL6_Output out;
        char* text;
        assert(xl6OutputOpenMemory(&out));
        assert(!xl6IrCgenEmitC(&out, ir, &diags));
        text = xl6OutputTakeMemoryText(&out, NULL);
        assert(text == NULL || text[0] == '\0'); xrtFree(text);
        assert(xl6OutputOpenMemory(&out));
        assert(!xl6IrCgenEmitHeader(&out, ir, "REJECTED_PENDING_ROUTE", &diags));
        text = xl6OutputTakeMemoryText(&out, NULL);
        assert(text == NULL || text[0] == '\0'); xrtFree(text);
    }
    xl6DiagFree(&diags); ++mutations;
}
#define MUTATE(Type, item, field, value) do { \
    Type saved = *(item); (item)->field = (value); \
    reject(ir, #item "." #field "=" #value); *(item) = saved; \
} while (0)

static void verify_plans(XL6_IrModule* ir)
{
    size_t i, j, selected = 0;
    for (i = 0; i < ir->exitPlanCount; ++i) {
        XL6_IrExitPlan* plan = &ir->exitPlans[i];
        const XL6_IrNode* managed = NULL;
        const XL6_IrNode* function;
        XL6_IrNodeId owner;
        const char* functionName;
        const char* expected;
        static const char letters[] = "?SCFILPT";
        if (plan->point != XL6_IR_EXIT_POINT_AFTER_TRY_EXCEPTION) continue;
        ++plans; ++selected;
        assert(xl6IrTryExceptionPlanGet(ir, plan->origin) == plan);
        owner = plan->origin;
        while ((function = xl6IrNodeGet(ir, owner))->kind != XL6_IR_NODE_FUNC_DECL) owner = function->parent;
        functionName = xl6IrNodeGet(ir, ir->children[function->firstChild])->text;
        assert(functionName != NULL);
        for (j = 0; j < plan->cleanupCount; ++j) {
            const XL6_IrCleanupAction* action = &ir->cleanupActions[plan->firstCleanup + j];
            if (action->kind != XL6_IR_CLEANUP_RELEASE) continue;
            assert(managed == NULL); managed = xl6IrNodeGet(ir, action->origin);
        }
        assert(managed != NULL && managed->text != NULL);
        /* The outer try can only own `outer`; inner tries own `middle`
         * (or `inner` in the nested-finally probe). Loop probes must close
         * item, cursor, then middle, in that literal boundary sequence. */
        if (strcmp(managed->text, "outer") == 0) expected = "SPT";
        else if (strcmp(functionName, "probe_loop_typed") == 0 || strcmp(functionName, "probe_loop_finally") == 0) {
            assert(strcmp(managed->text, "middle") == 0); expected = "SILST";
        } else {
            assert(strcmp(managed->text, "middle") == 0 || strcmp(managed->text, "inner") == 0);
            expected = "ST";
        }
        assert(plan->stepCount == strlen(expected));
        for (j = 0; j < plan->stepCount; ++j) {
            XL6_IrExitStep* step = &ir->exitSteps[plan->firstStep + j];
            assert(letters[step->kind] == expected[j]);
            MUTATE(XL6_IrExitStep, step, kind, XL6_IR_EXIT_STEP_INVALID);
            MUTATE(XL6_IrExitStep, step, origin, plan->origin);
        }
        MUTATE(XL6_IrExitPlan, plan, point, XL6_IR_EXIT_POINT_BEFORE_NODE);
        MUTATE(XL6_IrExitPlan, plan, kind, XL6_SEM_EXIT_NONE);
        MUTATE(XL6_IrExitPlan, plan, targetScopeDepth, plan->targetScopeDepth + 1);
        MUTATE(XL6_IrExitPlan, plan, targetRegion, plan->targetRegion + 1);
        MUTATE(XL6_IrExitPlan, plan, cleanupCount, 0);
        MUTATE(XL6_IrExitPlan, plan, stepCount, 0);
    }
    assert(selected == 25 && xl6IrVerifyModule(ir, NULL));
}

static void verify_lowered(XL6_IrModule* ir)
{
    size_t i, j;
    for (i = 0; i < ir->instructionCount; ++i) {
        XL6_IrInstruction* begin = &ir->instructions[i];
        XL6_IrInstruction* route;
        size_t first = SIZE_MAX, end;
        if (begin->kind != XL6_IR_INSTR_ERROR_UNWIND_BEGIN || begin->cleanupPlan == 0 ||
            ir->exitPlans[begin->cleanupPlan - 1].point != XL6_IR_EXIT_POINT_AFTER_TRY_EXCEPTION) continue;
        ++regions;
        MUTATE(XL6_IrInstruction, begin, cleanupPlan, 0);
        MUTATE(XL6_IrInstruction, begin, cleanupPlan, ir->exitPlanCount + 1);
        MUTATE(XL6_IrInstruction, begin, origin, ir->root);
        for (j = 0; j < ir->exitPlanCount; ++j) {
            if (ir->exitPlans[j].point != XL6_IR_EXIT_POINT_AFTER_TRY_EXCEPTION || j + 1 == begin->cleanupPlan) continue;
            MUTATE(XL6_IrInstruction, begin, cleanupPlan, j + 1); break;
        }
        for (end = i + 1; end < ir->instructionCount; ++end) {
            XL6_IrInstruction* member = &ir->instructions[end];
            if (member->kind == XL6_IR_INSTR_ERROR_UNWIND_END && member->errorScope == begin->errorScope) break;
            if (member->kind != XL6_IR_INSTR_RELEASE) continue;
            ++releases;
            MUTATE(XL6_IrInstruction, member, kind, XL6_IR_INSTR_ERROR_CLEAR);
            MUTATE(XL6_IrInstruction, member, errorScope, 0);
            if (first == SIZE_MAX) first = end;
            else {
                XL6_IrInstruction saved = *member;
                *member = ir->instructions[first]; ir->instructions[first] = saved;
                reject(ir, "remaining owners reordered"); ++ordered;
                ir->instructions[first] = *member; *member = saved;
            }
        }
        assert(end + 1 < ir->instructionCount);
        route = &ir->instructions[end + 1];
        if (route->kind == XL6_IR_INSTR_JUMP) {
            XL6_IrBlockId wrong = XL6_IR_BLOCK_INVALID;
            for (j = 0; j < ir->blockCount; ++j)
                if (i >= ir->blocks[j].firstInstruction &&
                    i < ir->blocks[j].firstInstruction + ir->blocks[j].instructionCount) {
                    wrong = (XL6_IrBlockId)(j + 1); break;
                }
            assert(wrong != XL6_IR_BLOCK_INVALID && wrong != route->trueTarget);
            MUTATE(XL6_IrInstruction, route, trueTarget, wrong);
        }
    }
    assert(regions >= 12 && releases >= 12 && ordered >= 4 && xl6IrVerifyModule(ir, NULL));
}

int main(void)
{
    XL6_ParseResult parse;
    XL6_IrModule ir;
    size_t i;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_pending_exception_outcome.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "pending_route_contract") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags)) goto failed;
    verify_plans(&ir);
    if (!xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    verify_plans(&ir); verify_lowered(&ir);
    printf("pending exception route contract: %lu typed/lowered plans, %lu unwind regions, %lu releases, %lu order mutations, %lu mutations rejected before C/header output\n",
        (unsigned long)plans, (unsigned long)regions, (unsigned long)releases, (unsigned long)ordered, (unsigned long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
failed:
    for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%s\n", parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 1;
}
