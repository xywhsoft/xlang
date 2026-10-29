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
        fprintf(stderr, "abrupt cleanup mutation escaped: %s\n", field); assert(0);
    }
    assert(diags.count != 0 && xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL);
    assert(text == NULL || text[0] == '\0'); xrtFree(text);
    assert(xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitHeader(&out, ir, "REJECTED_ABRUPT_CLEANUP", &diags));
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
    size_t i, j, regions = 0, partial = 0, copies = 0, copyTests = 0, cancellations = 0;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_abrupt_cleanup_outcome.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "abrupt_cleanup_contract") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* begin = &ir.instructions[i];
        XL6_IrInstruction *end, *test, *route;
        const XL6_IrExitStep* step;
        const XL6_IrExitPlan* normal;
        size_t scope;
        if (begin->kind != XL6_IR_INSTR_CLEANUP_BEGIN || begin->exitStep == 0) continue;
        ++regions; step = &ir.exitSteps[begin->exitStep - 1];
        normal = xl6IrScopeExitPlanGet(&ir, step->origin);
        assert(normal != NULL);
        MUTATE(XL6_IrInstruction, begin, exitStep, ir.exitStepCount + 1);
        MUTATE(XL6_IrInstruction, begin, kind, XL6_IR_INSTR_ERROR_UNWIND_BEGIN);
        MUTATE(XL6_IrInstruction, begin, origin, 0);
        MUTATE(XL6_IrInstruction, begin, errorScope, 0);
        if (begin->pendingExit != 0) {
            ++cancellations;
            MUTATE(XL6_IrInstruction, begin, pendingExit, 0);
            MUTATE(XL6_IrInstruction, begin, pendingExit, UINT32_MAX);
            MUTATE(XL6_IrInstruction, begin, exitStep, 0);
        }
        if (step->cleanupCount < normal->cleanupCount) {
            ++partial; MUTATE(XL6_IrInstruction, begin, exitStep, 0);
        }
        scope = begin->errorScope;
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
        MUTATE(XL6_IrInstruction, end, result, 0);
        MUTATE(XL6_IrInstruction, end, kind, XL6_IR_INSTR_ERROR_UNWIND_END);
        MUTATE(XL6_IrInstruction, test, falseTarget, test->trueTarget);
        MUTATE(XL6_IrInstruction, test, kind, XL6_IR_INSTR_JUMP);
        assert(end[2].kind == XL6_IR_INSTR_ERROR_UNWIND_BEGIN);
        scope = end[2].errorScope;
        for (j += 3; j < ir.instructionCount; ++j)
            if (ir.instructions[j].kind == XL6_IR_INSTR_ERROR_UNWIND_END && ir.instructions[j].errorScope == scope) break;
        assert(j + 1 < ir.instructionCount); route = &ir.instructions[j + 1];
        if (route->kind == XL6_IR_INSTR_JUMP) {
            MUTATE(XL6_IrInstruction, route, trueTarget, test->trueTarget);
            MUTATE(XL6_IrInstruction, route, kind, XL6_IR_INSTR_RETURN);
        } else {
            assert(route->kind == XL6_IR_INSTR_THROW);
            MUTATE(XL6_IrInstruction, route, kind, XL6_IR_INSTR_RETURN);
        }
    }
    for (i = 0; i < ir.intrinsicCallCount; ++i) {
        XL6_IrIntrinsicCall* intrinsic = &ir.intrinsicCalls[i];
        if (intrinsic->kind != XL6_IR_INTRINSIC_COPY && intrinsic->kind != XL6_IR_INTRINSIC_CLONE) continue;
        assert(intrinsic->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE); ++copies;
        MUTATE(XL6_IrIntrinsicCall, intrinsic, errorAbi, XL6_IR_ERROR_ABI_NO_RAISE);
        for (j = 0; j < ir.instructionCount; ++j) {
            XL6_IrInstruction* call = &ir.instructions[j];
            XL6_IrInstruction* test;
            if (call->kind != XL6_IR_INSTR_CALL || call->origin != intrinsic->origin) continue;
            assert(j + 1 < ir.instructionCount); test = call + 1;
            assert(test->kind == XL6_IR_INSTR_ERROR_TEST); ++copyTests;
            MUTATE(XL6_IrInstruction, test, kind, XL6_IR_INSTR_JUMP);
            MUTATE(XL6_IrInstruction, test, falseTarget, test->trueTarget);
        }
    }
    assert(regions >= 15 && partial >= 1 && copies == 3 && copyTests == 3 && cancellations >= 1);
    assert(xl6IrVerifyModule(&ir, NULL));
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    printf("abrupt cleanup contract: %lu route regions, %lu partial scopes, %lu fallible copy/clone calls, %lu pending cancellations, %lu mutations rejected before C/header output\n",
        (unsigned long)regions, (unsigned long)partial, (unsigned long)copies,
        (unsigned long)cancellations, (unsigned long)mutations);
    return 0;
failed:
    for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%s\n", parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 1;
}
