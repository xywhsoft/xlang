#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void reject(XL6_IrModule* ir)
{
    XL6_DiagList diags = {0};
    XL6_Output out;
    char* text;
    assert(!xl6IrVerifyModule(ir, &diags) && diags.count != 0);
    assert(xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL);
    assert(text == NULL || text[0] == '\0'); xrtFree(text);
    assert(xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitHeader(&out, ir, "REJECTED_UNWIND", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL);
    assert(text == NULL || text[0] == '\0'); xrtFree(text);
    xl6DiagFree(&diags);
}

static void release_effect_contract(void)
{
    XL6_IrModule module = {0};
    XL6_IrType types[7] = {0};
    XL6_IrTypeId elements[2] = {1, 2};
    XL6_IrTypeId cellElement = 1, nestedCell = 6;
    XL6_IrCAbi owners[] = {XL6_IR_C_ABI_RUNTIME_XVALUE_PTR,
        XL6_IR_C_ABI_RUNTIME_FUTURE_PTR, XL6_IR_C_ABI_RUNTIME_GENERATOR_PTR,
        XL6_IR_C_ABI_CLOSURE_PTR, XL6_IR_C_ABI_RUNTIME_VALUE_ITERATOR_PTR};
    size_t i;
    module.types.items = types; module.types.count = 7;
    types[0].ownershipAbi = XL6_IR_OWNERSHIP_ABI_TRIVIAL;
    types[0].cAbi = XL6_IR_C_ABI_I64;
    types[1].ownershipAbi = XL6_IR_OWNERSHIP_ABI_MANAGED;
    types[1].cAbi = XL6_IR_C_ABI_MANAGED_STRING;
    types[2].ownershipAbi = XL6_IR_OWNERSHIP_ABI_MANAGED;
    types[2].cAbi = XL6_IR_C_ABI_RUNTIME_ERROR_PTR;
    types[3].ownershipAbi = XL6_IR_OWNERSHIP_ABI_MANAGED;
    types[4].ownershipAbi = XL6_IR_OWNERSHIP_ABI_MANAGED;
    types[4].kind = XL6_IR_TYPE_TUPLE; types[4].cAbi = XL6_IR_C_ABI_PRODUCT;
    types[4].params = elements; types[4].paramCount = 2;
    assert(xl6IrTypeReleaseEffect(NULL, 1) == XL6_IR_RELEASE_EFFECT_INVALID);
    assert(xl6IrTypeReleaseEffect(&module, 0) == XL6_IR_RELEASE_EFFECT_INVALID);
    assert(xl6IrTypeReleaseEffect(&module, 8) == XL6_IR_RELEASE_EFFECT_INVALID);
    for (i = 1; i <= 3; ++i)
        assert(xl6IrTypeReleaseEffect(&module, (XL6_IrTypeId)i) == XL6_IR_RELEASE_EFFECT_NO_RAISE);
    assert(xl6IrTypeReleaseEffect(&module, 5) == XL6_IR_RELEASE_EFFECT_NO_RAISE);
    elements[1] = 4;
    for (i = 0; i < sizeof(owners) / sizeof(owners[0]); ++i) {
        types[3].cAbi = owners[i];
        assert(xl6IrTypeReleaseEffect(&module, 4) == XL6_IR_RELEASE_EFFECT_MAY_RAISE);
        assert(xl6IrTypeReleaseEffect(&module, 5) == XL6_IR_RELEASE_EFFECT_MAY_RAISE);
    }
    elements[1] = 5;
    assert(xl6IrTypeReleaseEffect(&module, 5) == XL6_IR_RELEASE_EFFECT_INVALID);
    elements[1] = 0;
    assert(xl6IrTypeReleaseEffect(&module, 5) == XL6_IR_RELEASE_EFFECT_INVALID);
    types[4].params = NULL;
    assert(xl6IrTypeReleaseEffect(&module, 5) == XL6_IR_RELEASE_EFFECT_INVALID);
    types[5].kind = types[6].kind = XL6_IR_TYPE_CELL;
    types[5].ownershipAbi = types[6].ownershipAbi = XL6_IR_OWNERSHIP_ABI_MANAGED;
    types[5].cAbi = types[6].cAbi = XL6_IR_C_ABI_RUNTIME_CELL_PTR;
    types[5].paramCount = types[6].paramCount = 1;
    types[5].params = &cellElement; types[6].params = &nestedCell;
    assert(xl6IrTypeReleaseEffect(&module, 6) == XL6_IR_RELEASE_EFFECT_NO_RAISE);
    assert(xl6IrTypeReleaseEffect(&module, 7) == XL6_IR_RELEASE_EFFECT_NO_RAISE);
    cellElement = 4;
    assert(xl6IrTypeReleaseEffect(&module, 6) == XL6_IR_RELEASE_EFFECT_MAY_RAISE);
    assert(xl6IrTypeReleaseEffect(&module, 7) == XL6_IR_RELEASE_EFFECT_MAY_RAISE);
    cellElement = 7;
    assert(xl6IrTypeReleaseEffect(&module, 6) == XL6_IR_RELEASE_EFFECT_INVALID);
    types[5].params = NULL;
    assert(xl6IrTypeReleaseEffect(&module, 6) == XL6_IR_RELEASE_EFFECT_INVALID);
}

int main(int argc, char** argv)
{
    XL6_ParseResult parse;
    XL6_IrModule ir;
    XL6_Output out;
    size_t i, regions = 0, normalRegions = 0, mutations = 0, members = 0, outcomes = 0, pendingFailures = 0;
    size_t selectedOperators = 0;
    release_effect_contract();
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile(argc == 3 ? argv[1] : "test/cases/xlir_error_unwind.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "unwind_contract") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) {
        for (i = 0; i < parse.diags.count; ++i)
            printf("%d:%d: %s\n", parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
        return 1;
    }
    if (argc == 2 || argc == 3) {
        assert(xl6OutputOpenFile(&out, argv[argc - 1]));
        if (!xl6IrCgenEmitC(&out, &ir, &parse.diags)) {
            for (i = 0; i < parse.diags.count; ++i) puts(parse.diags.items[i].message);
            xl6OutputClose(&out); xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 1;
        }
        xl6OutputClose(&out);
    } else assert(argc == 1);
    if (argc == 3) { xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0; }
    for (i = 0; i < ir.nodeCount; ++i) {
        XL6_IrNode* node = &ir.nodes[i];
        XL6_IrErrorAbi effect = node->operatorErrorAbi;
        const XL6_IrCallTarget* target;
        if (node->compoundOperatorSymbol == 0) continue;
        target = xl6IrCallTargetGet(&ir, node->compoundOperatorSymbol);
        assert(effect == XL6_IR_ERROR_ABI_NO_RAISE && target != NULL &&
            target->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE);
        /* Do not duplicate the selected call's real checked failure as a
         * dynamic-box intrinsic effect because its class uses xvalue storage. */
        node->operatorErrorAbi = XL6_IR_ERROR_ABI_MAY_RAISE;
        reject(&ir); node->operatorErrorAbi = effect;
        ++selectedOperators; ++mutations;
    }
    assert(selectedOperators >= 2);
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* begin = &ir.instructions[i];
        XL6_IrInstruction *end, *test, *cancel;
        XL6_IrInstruction saved;
        XL6_SymbolId pending;
        XL6_DiagList lifetime = {0};
        size_t cursor;
        if (begin->kind != XL6_IR_INSTR_CLEANUP_BEGIN || begin->pendingExit == 0 ||
            begin->exitStep != 0 || begin->parameterScope != 0) continue;
        pending = begin->pendingExit;
        for (cursor = i + 1; cursor < ir.instructionCount; ++cursor)
            if (ir.instructions[cursor].kind == XL6_IR_INSTR_CLEANUP_END &&
                ir.instructions[cursor].errorScope == begin->errorScope) break;
        assert(cursor + 1 < ir.instructionCount);
        end = &ir.instructions[cursor]; test = end + 1;
        assert(test->kind == XL6_IR_INSTR_ERROR_TEST);
        cursor = ir.blocks[test->falseTarget - 1].firstInstruction;
        assert(ir.instructions[cursor].kind == XL6_IR_INSTR_ERROR_UNWIND_BEGIN);
        cancel = &ir.instructions[cursor + 1];
        assert(cancel->kind == XL6_IR_INSTR_RELEASE && cancel->targetSymbol == pending);
        saved = *cancel;
        cancel->kind = XL6_IR_INSTR_ERROR_CLEAR; cancel->targetSymbol = 0;
        cancel->type = XL6_IR_TYPE_INVALID;
        /* Check the lifetime proof independently too: rejecting this omitted
         * owner must not depend only on the lexical-region spelling check. */
        assert(!xl6IrVerifyValueLifetimes(&ir, &lifetime) && lifetime.count != 0);
        xl6DiagFree(&lifetime);
        reject(&ir); ++mutations; *cancel = saved;
        begin->pendingExit = UINT32_MAX; reject(&ir); ++mutations; begin->pendingExit = pending;
        assert(xl6IrVerifyModule(&ir, NULL)); ++pendingFailures;
    }
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i];
        size_t scope = instruction->errorScope;
        const XL6_IrCallTarget* target = xl6IrCallTargetGet(&ir, instruction->targetSymbol);
        if (instruction->kind == XL6_IR_INSTR_CLOSURE_CALL ||
            ((instruction->kind == XL6_IR_INSTR_CALL || instruction->kind == XL6_IR_INSTR_ASYNC_START) &&
             target != NULL && target->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE)) {
            XL6_DiagList diags = {0};
            XL6_IrInstruction* test = &ir.instructions[i + 1];
            size_t d;
            int found = 0;
            assert(i + 1 < ir.instructionCount && test->kind == XL6_IR_INSTR_ERROR_TEST);
            test->kind = XL6_IR_INSTR_ERROR_CLEAR;
            assert(!xl6IrVerifyModule(&ir, &diags));
            for (d = 0; d < diags.count; ++d)
                if (strstr(diags.items[d].message, "call outcome must be tested") != NULL) found = 1;
            assert(found);
            xl6DiagFree(&diags);
            reject(&ir); ++outcomes; ++mutations;
            test->kind = XL6_IR_INSTR_ERROR_TEST;
        }
        if (instruction->kind == XL6_IR_INSTR_ERROR_UNWIND_BEGIN || instruction->kind == XL6_IR_INSTR_CLEANUP_BEGIN ||
            instruction->kind == XL6_IR_INSTR_CLEANUP_INHERIT_BEGIN) {
            ++regions;
            if (instruction->kind == XL6_IR_INSTR_CLEANUP_BEGIN) ++normalRegions;
            instruction->errorScope = 0; reject(&ir); ++mutations;
            instruction->errorScope = scope + 1; reject(&ir); ++mutations;
            instruction->errorScope = scope;
        } else if (instruction->kind == XL6_IR_INSTR_ERROR_UNWIND_END || instruction->kind == XL6_IR_INSTR_CLEANUP_END) {
            XL6_IrInstructionKind kind = instruction->kind;
            instruction->errorScope = 0; reject(&ir); ++mutations;
            instruction->errorScope = i + 1; reject(&ir); ++mutations;
            instruction->errorScope = scope;
            instruction->kind = XL6_IR_INSTR_ERROR_CLEAR; reject(&ir); ++mutations;
            instruction->kind = kind;
            if (kind == XL6_IR_INSTR_CLEANUP_END) {
                XL6_IrTypeId type = instruction->type;
                XL6_IrValueId value = instruction->result;
                instruction->type = XL6_IR_TYPE_INVALID; reject(&ir); ++mutations;
                instruction->type = type;
                instruction->result = XL6_IR_VALUE_INVALID; reject(&ir); ++mutations;
                instruction->result = value;
                instruction->kind = XL6_IR_INSTR_ERROR_UNWIND_END; reject(&ir); ++mutations;
                instruction->kind = kind;
                ir.instructions[i + 1].kind = XL6_IR_INSTR_JUMP; reject(&ir); ++mutations;
                ir.instructions[i + 1].kind = XL6_IR_INSTR_ERROR_TEST;
            }
        } else if (scope != 0) {
            ++members;
            instruction->errorScope = 0; reject(&ir); ++mutations;
            instruction->errorScope = scope;
            if (instruction->kind == XL6_IR_INSTR_RELEASE) {
                XL6_IrInstruction saved = *instruction;
                /* ERROR_CLEAR is valid elsewhere, but this region admits
                 * only owner cleanup, not arbitrary error-state mutation. */
                instruction->kind = XL6_IR_INSTR_ERROR_CLEAR;
                instruction->type = XL6_IR_TYPE_INVALID;
                instruction->targetSymbol = 0;
                reject(&ir); ++mutations; *instruction = saved;
            }
        } else if (instruction->kind == XL6_IR_INSTR_AWAIT || instruction->kind == XL6_IR_INSTR_YIELD) {
            size_t begin;
            for (begin = 0; begin < ir.instructionCount; ++begin)
                if (ir.instructions[begin].kind == XL6_IR_INSTR_ERROR_UNWIND_BEGIN) break;
            assert(begin < ir.instructionCount);
            instruction->errorScope = begin + 1; reject(&ir); ++mutations;
            instruction->errorScope = 0;
        }
    }
    assert(regions >= 10 && normalRegions >= 8 && members >= 10 && pendingFailures >= 2 && xl6IrVerifyModule(&ir, NULL));
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    printf("error unwind contract: %lu regions, %lu cleanup members, %lu region/identity/membership mutations rejected before output\n",
        (unsigned long)regions, (unsigned long)members, (unsigned long)mutations);
    printf("call completion contract: %lu immediate outcome mutations rejected before output\n", (unsigned long)outcomes);
    printf("normal cleanup contract: %lu isolated regions with exclusive bool outcomes\n", (unsigned long)normalRegions);
    printf("pending return cleanup: %lu normal finally failure transactions preserve success and cancel failure\n",
        (unsigned long)pendingFailures);
    printf("selected compound operators: %lu independent checked call effects, duplicate intrinsic effects rejected\n",
        (unsigned long)selectedOperators);
    return 0;
}
