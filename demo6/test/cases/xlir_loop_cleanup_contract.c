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
        fprintf(stderr, "loop cleanup mutation escaped: %s\n", field); assert(0);
    }
    assert(diags.count != 0 && xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL);
    assert(text == NULL || text[0] == '\0'); xrtFree(text);
    assert(xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitHeader(&out, ir, "REJECTED_LOOP_CLEANUP", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL);
    assert(text == NULL || text[0] == '\0'); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}
#define MUTATE(Type, item, field, value) do { \
    Type saved = *(item); (item)->field = (value); \
    reject(&ir, #item "." #field "=" #value); *(item) = saved; \
} while (0)

int main(int argc, char** argv)
{
    XL6_ParseResult parse;
    XL6_IrModule ir;
    XL6_Output out;
    char typedPath[1024];
    size_t i, j, regions = 0, releases = 0, ordered = 0, next = 0;
    assert(argc == 1 || argc == 4);
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_loop_cleanup_outcome.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "loop_cleanup_contract") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    if (argc == 4) {
        assert(snprintf(typedPath, sizeof(typedPath), "%s.typed.json", argv[3]) > 0);
        assert(xl6OutputOpenFile(&out, typedPath)); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    }
    if (!xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    for (i = 0; i < ir.loopOwnerCount; ++i) {
        XL6_IrLoopOwner* owner = &ir.loopOwners[i];
        MUTATE(XL6_IrLoopOwner, owner, origin, 0);
        MUTATE(XL6_IrLoopOwner, owner, owner, 0);
        MUTATE(XL6_IrLoopOwner, owner, symbol, 0);
        MUTATE(XL6_IrLoopOwner, owner, type, 0);
        MUTATE(XL6_IrLoopOwner, owner, target, SIZE_MAX);
    }
    MUTATE(XL6_IrModule, &ir, loopOwnerCount, 0);
    MUTATE(XL6_IrModule, &ir, loopOwners, NULL);
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* begin = &ir.instructions[i];
        XL6_IrInstruction* test;
        if (begin->loopOwner != 0) {
            ++releases;
            MUTATE(XL6_IrInstruction, begin, loopOwner, 0);
            MUTATE(XL6_IrInstruction, begin, loopOwner, ir.loopOwnerCount + 1);
            MUTATE(XL6_IrInstruction, begin, targetSymbol, 0);
            MUTATE(XL6_IrInstruction, begin, kind, XL6_IR_INSTR_ERROR_CLEAR);
        }
        if (begin->kind == XL6_IR_INSTR_GENERATOR_NEXT) {
            ++next;
            test = &ir.instructions[i + 1];
            assert(test->kind == XL6_IR_INSTR_ERROR_TEST);
            MUTATE(XL6_IrInstruction, test, falseTarget, test->trueTarget);
            MUTATE(XL6_IrInstruction, test, kind, XL6_IR_INSTR_JUMP);
        }
        if (begin->kind != XL6_IR_INSTR_CLEANUP_BEGIN || begin->cleanupPlan == 0) continue;
        ++regions;
        MUTATE(XL6_IrInstruction, begin, cleanupPlan, 0);
        MUTATE(XL6_IrInstruction, begin, cleanupPlan, ir.exitPlanCount + 1);
        MUTATE(XL6_IrInstruction, begin, errorScope, 0);
        for (j = i + 1; j < ir.instructionCount; ++j) {
            XL6_IrInstruction* member = &ir.instructions[j];
            if (member->kind == XL6_IR_INSTR_CLEANUP_END) break;
            MUTATE(XL6_IrInstruction, member, errorScope, 0);
            if (member->kind == XL6_IR_INSTR_CLEANUP_CAPTURE)
                MUTATE(XL6_IrInstruction, member, kind, XL6_IR_INSTR_ERROR_CLEAR);
        }
        assert(j + 2 < ir.instructionCount);
        test = &ir.instructions[j + 1];
        MUTATE(XL6_IrInstruction, test, falseTarget, test->trueTarget);
        MUTATE(XL6_IrInstruction, test, kind, XL6_IR_INSTR_JUMP);
        {
            XL6_IrInstruction* unwind = &ir.instructions[j + 2];
            size_t first = SIZE_MAX, end;
            MUTATE(XL6_IrInstruction, unwind, cleanupPlan, 0);
            for (end = j + 3; end < ir.instructionCount; ++end) {
                XL6_IrInstruction* member = &ir.instructions[end];
                if (member->kind == XL6_IR_INSTR_ERROR_UNWIND_END && member->errorScope == unwind->errorScope) break;
                if (member->kind != XL6_IR_INSTR_RELEASE || member->loopOwner == 0) continue;
                if (first == SIZE_MAX) first = end;
                else if (ir.instructions[first].loopOwner != 0 && member->loopOwner != 0) {
                    XL6_IrInstruction saved = *member;
                    *member = ir.instructions[first]; ir.instructions[first] = saved;
                    reject(&ir, "enclosing loop cleanup order"); ++ordered;
                    ir.instructions[first] = *member; *member = saved;
                }
            }
            assert(end + 1 < ir.instructionCount);
            if (ir.instructions[end + 1].kind == XL6_IR_INSTR_JUMP)
                MUTATE(XL6_IrInstruction, &ir.instructions[end + 1], trueTarget, test->trueTarget);
        }
    }
    printf("loop cleanup contract: %lu physical owners, %lu transactions, %lu release sites, %lu ordered-route mutations, %lu generator next carriers, %lu mutations rejected before C/header output\n",
        (unsigned long)ir.loopOwnerCount, (unsigned long)regions, (unsigned long)releases,
        (unsigned long)ordered, (unsigned long)next, (unsigned long)mutations);
    assert(regions >= 20 && releases >= 40 && ordered > 0 && next >= 4 && mutations >= 200);
    assert(xl6IrVerifyModule(&ir, NULL));
    if (argc == 4) {
        assert(xl6OutputOpenFile(&out, argv[1]) && xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
        assert(xl6OutputOpenFile(&out, argv[2]) && xl6IrCgenEmitHeader(&out, &ir, "LOOP_CLEANUP", &parse.diags)); xl6OutputClose(&out);
        assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    }
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
failed:
    for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%s\n", parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 1;
}
