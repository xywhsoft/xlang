#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int rejections;
static void reject(XL6_IrModule* ir)
{
    XL6_DiagList diags = {0};
    XL6_Output out;
    char* text;
    assert(!xl6IrVerifyModule(ir, &diags) && diags.count != 0);
    assert(xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL);
    assert(text == NULL || text[0] == '\0');
    xrtFree(text); xl6DiagFree(&diags); ++rejections;
}

int main(void)
{
    XL6_ParseResult parse;
    XL6_IrModule ir;
    XL6_IrInstruction *rollback = NULL, *test = NULL, *take = NULL, *other = NULL;
    XL6_IrInstruction saved;
    XL6_IrValueId raw = 0;
    size_t i;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_foreign_result_rollback.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "rollback_contract") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) {
        for (i = 0; i < parse.diags.count; ++i) puts(parse.diags.items[i].message);
        return 1;
    }
    assert(xl6IrCgenVerifyEmittable(&ir, &parse.diags));
    {
        XL6_Output out;
        char* text;
        assert(xl6OutputOpenMemory(&out));
        assert(xl6IrCgenEmitC(&out, &ir, &parse.diags));
        text = xl6OutputTakeMemoryText(&out, NULL);
        assert(text != NULL && strstr(text, "xlir_owned") != NULL);
        xrtFree(text);
    }
    for (i = 0; i < ir.instructionCount; ++i)
        if (ir.instructions[i].kind == XL6_IR_INSTR_FOREIGN_RESULT_ROLLBACK &&
            strcmp(xl6IrCallTargetGet(&ir, ir.instructions[i].targetSymbol)->cName,
                "rollback_owned_close") == 0) {
            rollback = &ir.instructions[i]; break;
        }
    assert(rollback != NULL); raw = ir.operands[rollback->firstOperand];
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i];
        if (instruction->operandCount != 1 || ir.operands[instruction->firstOperand] != raw) continue;
        if (instruction->kind == XL6_IR_INSTR_ERROR_TEST) test = instruction;
        if (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_NATIVE_HANDLE_TAKE) take = instruction;
    }
    assert(test != NULL && take != NULL);
    saved = *rollback;
    rollback->kind = XL6_IR_INSTR_ERROR_CLEAR; rollback->type = 0;
    rollback->targetSymbol = 0; rollback->operandCount = 0; reject(&ir); *rollback = saved;
    rollback->targetSymbol = test->targetSymbol; reject(&ir); *rollback = saved;
    rollback->targetSymbol = 0; reject(&ir); *rollback = saved;
    rollback->type = 0; reject(&ir); *rollback = saved;
    rollback->operandCount = 0; reject(&ir); *rollback = saved;
    rollback->result = raw; reject(&ir); *rollback = saved;
    rollback->opcode = XL6_IR_OPCODE_ADD; reject(&ir); *rollback = saved;
    rollback->trueTarget = test->trueTarget; reject(&ir); *rollback = saved;
    saved = *test;
    test->falseTarget = test->trueTarget; reject(&ir); *test = saved;
    test->trueTarget = saved.falseTarget; test->falseTarget = saved.trueTarget; reject(&ir); *test = saved;
    test->falseTarget = 0; reject(&ir); *test = saved;
    saved = *take;
    take->runtimeOp = XL6_IR_RUNTIME_XVALUE_RAW_POINTER_BORROW; reject(&ir); *take = saved;
    take->targetSymbol = test->targetSymbol; reject(&ir); *take = saved;
    /* A second owner consumer on the success edge is not a permitted use of
     * a physical pointer, even though it has exactly the right destructor. */
    *take = *rollback; reject(&ir); *take = saved;
    {
        XL6_IrBlock* block = &ir.blocks[test->trueTarget - 1];
        other = &ir.instructions[block->firstInstruction + block->instructionCount - 1];
        saved = *other;
        other->kind = XL6_IR_INSTR_JUMP; other->trueTarget = test->falseTarget;
        other->operandCount = 0; reject(&ir); *other = saved;
        *other = *rollback; reject(&ir); *other = saved;
    }
    /* A borrowed factory has the same physical point representation. Neither
     * that coincidence nor an otherwise valid destructor grants ownership. */
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* borrowedTest = &ir.instructions[i];
        const XL6_IrCallTarget* target;
        XL6_IrInstruction* entry;
        XL6_IrValueId previous;
        if (borrowedTest->kind != XL6_IR_INSTR_ERROR_TEST) continue;
        target = xl6IrCallTargetGet(&ir, borrowedTest->targetSymbol);
        if (target == NULL || strcmp(target->cName, "rollback_borrowed_open") != 0) continue;
        entry = &ir.instructions[ir.blocks[borrowedTest->falseTarget - 1].firstInstruction];
        saved = *entry; *entry = *rollback;
        entry->firstOperand = borrowedTest->firstOperand;
        reject(&ir); *entry = saved;
        previous = ir.operands[rollback->firstOperand];
        ir.operands[rollback->firstOperand] = ir.operands[borrowedTest->firstOperand];
        reject(&ir); ir.operands[rollback->firstOperand] = previous;
        break;
    }
    assert(i < ir.instructionCount);
    assert(xl6IrVerifyModule(&ir, NULL));
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    printf("foreign result ownership: %d malformed/missing/duplicate/wrong-edge rollback mutations rejected before any C output\n", rejections);
    return rejections == 18 ? 0 : 1;
}
