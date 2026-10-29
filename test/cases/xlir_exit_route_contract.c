#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t mutations, routes, steps, exceptionalRoutes;
static void reject(XL6_IrModule* ir, const char* field)
{
    XL6_DiagList diags = {0};
    if (xl6IrVerifyModule(ir, &diags)) {
        fprintf(stderr, "exit route mutation escaped: %s\n", field); assert(0);
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
        assert(!xl6IrCgenEmitHeader(&out, ir, "REJECTED_EXIT_ROUTE", &diags));
        text = xl6OutputTakeMemoryText(&out, NULL);
        assert(text == NULL || text[0] == '\0'); xrtFree(text);
    }
    xl6DiagFree(&diags); ++mutations;
}
#define MUTATE(Type, item, field, value) do { \
    Type saved = *(item); (item)->field = (value); \
    reject(ir, #item "." #field "=" #value); *(item) = saved; \
} while (0)

static const XL6_IrNode* owner(const XL6_IrModule* ir, XL6_IrNodeId origin)
{
    size_t depth = 0;
    while (origin != 0 && ++depth <= ir->nodeCount) {
        const XL6_IrNode* node = xl6IrNodeGet(ir, origin);
        assert(node != NULL);
        if (node->kind == XL6_IR_NODE_FUNC_DECL) return node;
        origin = node->parent;
    }
    return NULL;
}

/* Literal expected sequences are deliberately independent of the planner's
 * parent walk. The letters describe whole boundaries, including empty ones. */
static void expect(const XL6_IrModule* ir, const XL6_IrExitPlan* plan, const char* sequence)
{
    static const char letters[] = "?SCFILPT";
    size_t i;
    assert(plan->stepCount == strlen(sequence));
    for (i = 0; i < plan->stepCount; ++i) {
        const XL6_IrExitStep* step = &ir->exitSteps[plan->firstStep + i];
        assert(step->kind > XL6_IR_EXIT_STEP_INVALID && step->kind <= XL6_IR_EXIT_STEP_TERMINAL);
        if (letters[step->kind] != sequence[i]) {
            fprintf(stderr, "route n%u step %lu expected %c got %c\n", plan->origin,
                (unsigned long)i, sequence[i], letters[step->kind]); assert(0);
        }
    }
}

static void verify_fixture(XL6_IrModule* ir)
{
    size_t i, j, selected = 0, overrides = 0, anonymous = 0;
    for (i = 0; i < ir->exitPlanCount; ++i) {
        XL6_IrExitPlan* plan = &ir->exitPlans[i];
        const XL6_IrNode* function;
        const char* name;
        XL6_IrExitStep* route;
        if (plan->stepCount == 0) {
            MUTATE(XL6_IrExitPlan, plan, firstStep, 1);
            MUTATE(XL6_IrExitPlan, plan, stepCount, 1);
            continue;
        }
        /* Literal sequences below describe normal return/break/continue.
         * Exceptional routes now have their own ordered topology and are
         * exercised by the scope/loop cleanup contracts. */
        if (plan->kind == XL6_SEM_EXIT_THROW) {
            ++exceptionalRoutes;
            MUTATE(XL6_IrExitPlan, plan, firstStep, ir->exitStepCount + 1);
            MUTATE(XL6_IrExitPlan, plan, stepCount, 0);
            MUTATE(XL6_IrExitPlan, plan, targetRegion, plan->targetRegion + 1);
            for (j = 0; j < plan->stepCount; ++j) {
                XL6_IrExitStep* step = &ir->exitSteps[plan->firstStep + j];
                MUTATE(XL6_IrExitStep, step, origin, 0);
                MUTATE(XL6_IrExitStep, step, kind, XL6_IR_EXIT_STEP_INVALID);
                MUTATE(XL6_IrExitStep, step, failureRegion, step->failureRegion + 1);
            }
            continue;
        }
        ++routes; steps += plan->stepCount;
        function = owner(ir, plan->origin); assert(function != NULL);
        name = function->text;
        if (function->childCount != 0 && function->sourceKind != XL6_AST_ANON_FUNC)
            name = xl6IrNodeGet(ir, ir->children[function->firstChild])->text;
        route = &ir->exitSteps[plan->firstStep];
        if (name != NULL && strcmp(name, "routeReturn") == 0) {
            expect(ir, plan, "SFSFSPT"); ++selected;
            assert(route[0].failureTarget == route[1].origin);
            assert(route[1].failureTarget == route[3].origin);
            assert(route[2].failureTarget == route[3].origin);
            assert(route[3].failureTarget == route[6].origin);
            for (j = 0; j <= 4; j += 2) assert(route[j].cleanupCount == 1);
            /* The declaration following return is not initialized, and the
             * outer binding cannot occur before either finally. */
            assert(plan->cleanupCount == 3);
        } else if (name != NULL && strcmp(name, "routeBreak") == 0 &&
                   plan->kind == XL6_SEM_EXIT_BREAK) {
            expect(ir, plan, "SFSILSILT"); ++selected;
            assert(route[3].origin == route[4].origin && route[6].origin == route[7].origin);
            assert(route[8].origin == route[7].origin && route[3].origin != route[8].origin);
        } else if (name != NULL && strcmp(name, "routeContinue") == 0 &&
                   plan->kind == XL6_SEM_EXIT_CONTINUE) {
            expect(ir, plan, "SFSILSIT"); ++selected;
            assert(route[3].origin == route[4].origin && route[6].origin == route[7].origin);
            assert(route[3].origin != route[7].origin);
        } else if (name != NULL && strcmp(name, "routeCatch") == 0) {
            expect(ir, plan, "SCFSPT"); ++selected;
            assert(route[0].failureTarget == route[2].origin);
            assert(route[1].failureTarget == route[2].origin);
            assert(route[2].failureTarget == route[5].origin);
        } else if (name != NULL && strcmp(name, "routeOverride") == 0) {
            expect(ir, plan, overrides++ == 0 ? "SFSPT" : "SSPT"); ++selected;
        } else if (function->sourceKind == XL6_AST_ANON_FUNC) {
            expect(ir, plan, "SPT"); ++anonymous;
            assert(plan->cleanupCount == 1);
            assert(strcmp(xl6IrNodeGet(ir, ir->cleanupActions[plan->firstCleanup].origin)->text, "inner") == 0);
        }
        MUTATE(XL6_IrExitPlan, plan, firstStep, ir->exitStepCount + 1);
        MUTATE(XL6_IrExitPlan, plan, stepCount, 0);
        MUTATE(XL6_IrExitPlan, plan, stepCount, plan->stepCount - 1);
        MUTATE(XL6_IrExitPlan, plan, stepCount, SIZE_MAX);
        for (j = 0; j < plan->stepCount; ++j) {
            XL6_IrExitStep* step = &route[j];
            MUTATE(XL6_IrExitStep, step, kind, XL6_IR_EXIT_STEP_INVALID);
            MUTATE(XL6_IrExitStep, step, origin, 0);
            MUTATE(XL6_IrExitStep, step, firstCleanup, ir->cleanupActionCount + 1);
            MUTATE(XL6_IrExitStep, step, cleanupCount, SIZE_MAX);
            MUTATE(XL6_IrExitStep, step, failureTarget, ir->nodeCount + 1);
            MUTATE(XL6_IrExitStep, step, failureScopeDepth, step->failureScopeDepth + 1);
            MUTATE(XL6_IrExitStep, step, failureRegion, step->failureRegion + 1);
            if (j + 1 < plan->stepCount) {
                XL6_IrExitStep saved = *step;
                *step = step[1]; step[1] = saved;
                reject(ir, "exit steps reordered");
                step[1] = *step; *step = saved;
            }
        }
        for (j = 0; j < plan->cleanupCount; ++j) {
            XL6_IrCleanupAction* action = &ir->cleanupActions[plan->firstCleanup + j];
            MUTATE(XL6_IrCleanupAction, action, targetSymbol, 0);
            MUTATE(XL6_IrCleanupAction, action, origin, plan->origin);
            MUTATE(XL6_IrCleanupAction, action, kind, XL6_IR_CLEANUP_DROP);
        }
    }
    if (selected != 6 || overrides != 2 || anonymous != 1)
        fprintf(stderr, "route fixture coverage selected=%lu overrides=%lu anonymous=%lu\n",
            (unsigned long)selected, (unsigned long)overrides, (unsigned long)anonymous);
    assert(selected == 6 && overrides == 2 && anonymous == 1);
    MUTATE(XL6_IrModule, ir, exitStepCount, 0);
    MUTATE(XL6_IrModule, ir, exitStepCapacity, 0);
    MUTATE(XL6_IrModule, ir, exitSteps, NULL);
    assert(xl6IrVerifyModule(ir, NULL));
}

int main(void)
{
    XL6_ParseResult parse;
    XL6_IrModule ir;
    size_t i;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_exit_route.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "exit_route_contract") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    verify_fixture(&ir);
    if (!xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    verify_fixture(&ir);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    printf("exit route topology: %lu normal typed/lowered routes, %lu normal steps, %lu exceptional routes, %lu mutations rejected; CFG outcomes are tested separately\n",
        (unsigned long)routes, (unsigned long)steps, (unsigned long)exceptionalRoutes, (unsigned long)mutations);
    return 0;
failed:
    for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%s\n", parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    return 1;
}
