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
        fprintf(stderr, "parameter cleanup mutation escaped: %s\n", field); assert(0);
    }
    assert(diags.count != 0 && xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL);
    assert(text == NULL || text[0] == '\0'); xrtFree(text);
    assert(xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitHeader(&out, ir, "REJECTED_PARAMETER_CLEANUP", &diags));
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
    size_t i, j, regions = 0, pending = 0, cells = 0, rebound = 0, stable = 0;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_parameter_cleanup_outcome.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "parameter_cleanup_contract") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    for (i = 0; i < ir.callTargetCount; ++i) {
        XL6_IrCallTarget* target = &ir.callTargets[i];
        if (target->kind != XL6_IR_CALL_TARGET_INTERNAL || target->parameterLifetimes == NULL) continue;
        for (j = 0; j < target->parameterCount; ++j) {
            XL6_IrParameterLifetime* lifetime = &target->parameterLifetimes[j];
            MUTATE(XL6_IrParameterLifetime, lifetime, borrowReadOnly,
                !lifetime->borrowReadOnly);
            if (!xl6IrCallTargetParameterBindingOwnsStorage(&ir, target, j)) continue;
            rebound += lifetime->rebindable != 0; stable += lifetime->rebindable == 0;
            MUTATE(XL6_IrParameterLifetime, lifetime, rebindable, !lifetime->rebindable);
            MUTATE(XL6_IrParameterLifetime, lifetime, cleanupEffect,
                lifetime->cleanupEffect == XL6_IR_RELEASE_EFFECT_MAY_RAISE ? XL6_IR_RELEASE_EFFECT_NO_RAISE : XL6_IR_RELEASE_EFFECT_MAY_RAISE);
            MUTATE(XL6_IrParameterLifetime, lifetime, sourceSymbol, UINT32_MAX);
        }
        if (target->parameterCount != 0) MUTATE(XL6_IrCallTarget, target, parameterLifetimes, NULL);
    }
    for (i = 0; i < ir.functionCount; ++i) {
        XL6_IrFunction* function = &ir.functions[i];
        if (function->parameterCells == NULL) continue;
        for (j = 0; j < function->parameterCount; ++j) {
            XL6_SymbolId saved = function->parameterCells[j];
            if (saved == 0) continue;
            ++cells;
            function->parameterCells[j] = 0; reject(&ir, "missing promoted parameter cell");
            function->parameterCells[j] = ir.functionParameters[function->firstParameter + j];
            reject(&ir, "borrowed parameter is not its owned cell");
            function->parameterCells[j] = saved;
        }
        MUTATE(XL6_IrFunction, function, parameterCells, NULL);
    }
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* begin = &ir.instructions[i];
        XL6_IrInstruction *test, *success, *failure, *rollback;
        XL6_IrBlock *successBlock, *failureBlock;
        size_t firstRelease = SIZE_MAX;
        if (begin->kind != XL6_IR_INSTR_CLEANUP_BEGIN || begin->parameterScope == 0) continue;
        ++regions;
        MUTATE(XL6_IrInstruction, begin, parameterScope, 0);
        MUTATE(XL6_IrInstruction, begin, parameterScope, ir.instructionCount + 1);
        MUTATE(XL6_IrInstruction, begin, errorScope, 0);
        MUTATE(XL6_IrInstruction, begin, kind, XL6_IR_INSTR_ERROR_UNWIND_BEGIN);
        if (begin->pendingExit != 0) {
            ++pending;
            MUTATE(XL6_IrInstruction, begin, pendingExit, 0);
            MUTATE(XL6_IrInstruction, begin, pendingExit, UINT32_MAX);
        }
        for (j = i + 1; j < ir.instructionCount; ++j) {
            XL6_IrInstruction* member = &ir.instructions[j];
            if (member->kind == XL6_IR_INSTR_CLEANUP_END) break;
            MUTATE(XL6_IrInstruction, member, errorScope, 0);
            if (member->kind == XL6_IR_INSTR_CLEANUP_CAPTURE)
                MUTATE(XL6_IrInstruction, member, kind, XL6_IR_INSTR_ERROR_CLEAR);
            else {
                MUTATE(XL6_IrInstruction, member, targetSymbol, 0);
                if (firstRelease == SIZE_MAX) firstRelease = j;
                else {
                    XL6_IrInstruction saved = *member;
                    *member = ir.instructions[firstRelease]; ir.instructions[firstRelease] = saved;
                    reject(&ir, "parameter destruction order");
                    ir.instructions[firstRelease] = *member; *member = saved;
                }
            }
        }
        assert(j + 1 < ir.instructionCount); test = &ir.instructions[j + 1];
        MUTATE(XL6_IrInstruction, test, falseTarget, test->trueTarget);
        MUTATE(XL6_IrInstruction, test, kind, XL6_IR_INSTR_JUMP);
        successBlock = &ir.blocks[test->trueTarget - 1]; failureBlock = &ir.blocks[test->falseTarget - 1];
        success = &ir.instructions[successBlock->firstInstruction + successBlock->instructionCount - 1];
        failure = &ir.instructions[failureBlock->firstInstruction + failureBlock->instructionCount - 1];
        MUTATE(XL6_IrInstruction, &ir.instructions[failureBlock->firstInstruction], parameterScope, 0);
        MUTATE(XL6_IrInstruction, &ir.instructions[failureBlock->firstInstruction], parameterScope, ir.instructionCount + 1);
        MUTATE(XL6_IrInstruction, &ir.instructions[failureBlock->firstInstruction], cleanupPlan, 1);
        MUTATE(XL6_IrInstruction, success, parameterScope, 0);
        MUTATE(XL6_IrInstruction, failure, parameterScope, 0);
        MUTATE(XL6_IrInstruction, failure, kind, XL6_IR_INSTR_RETURN);
        if (begin->pendingExit != 0) {
            rollback = &ir.instructions[failureBlock->firstInstruction + 1];
            MUTATE(XL6_IrInstruction, rollback, targetSymbol, 0);
            MUTATE(XL6_IrInstruction, rollback, kind, XL6_IR_INSTR_ERROR_CLEAR);
        }
    }
    assert(regions >= 10 && pending >= 6 && cells == 2 && rebound >= 20 && stable >= 1 && mutations >= 100);
    assert(xl6IrVerifyModule(&ir, NULL));
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    printf("parameter cleanup contract: %lu regions, %lu pending returns, %lu promoted cells, %lu rebound/%lu stable bindings, %lu mutations rejected before C/header output\n",
        (unsigned long)regions, (unsigned long)pending, (unsigned long)cells,
        (unsigned long)rebound, (unsigned long)stable, (unsigned long)mutations);
    return 0;
failed:
    for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%s\n", parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 1;
}
