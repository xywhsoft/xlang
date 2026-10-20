/* Offline: deliberately corrupt nominal operator facts at both IR boundaries. */
#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t refusals;
static void refused(XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir, NULL));
    for (unsigned header = 0; header < 2; ++header) {
        XL6_Output output;
        XL6_DiagList diagnostics;
        size_t size = SIZE_MAX;
        xl6DiagInit(&diagnostics);
        assert(xl6OutputOpenMemory(&output));
        assert(!(header ? xl6IrCgenEmitHeader(&output, ir, "BAD_ENUM", &diagnostics)
                        : xl6IrCgenEmitC(&output, ir, &diagnostics)));
        char* text = xl6OutputTakeMemoryText(&output, &size);
        assert(size == 0);
        xrtFree(text);
        xl6OutputDiscard(&output);
        xl6DiagFree(&diagnostics);
    }
    ++refusals;
}

int main(int argc, char** argv)
{
    XL6_CompileUnit unit;
    assert(argc == 2);
    xl6CompileUnitInit(&unit);
    if (!xl6CompileUnitLoadFile(&unit, argv[1], 1)) {
        xl6DiagPrintText(&unit.parse.diags, argv[1]);
        return 2;
    }
    XL6_IrModule* ir = &unit.ir;
    XL6_IrTypeId other = XL6_IR_TYPE_INVALID;
    size_t typed = 0, lowered = 0, masks = 0;
    assert(xl6IrVerifyModule(ir, NULL));
    for (size_t i = 0; i < ir->types.count; ++i)
        if (ir->types.items[i].kind == XL6_IR_TYPE_NUMERIC_ENUM &&
            !strcmp(ir->types.items[i].name, "Other")) other = (XL6_IrTypeId)i + 1;
    assert(other);
    for (size_t i = 0; i < ir->nodeCount; ++i) {
        XL6_IrNode* node = &ir->nodes[i];
        if (node->sourceKind != XL6_AST_BINARY && node->sourceKind != XL6_AST_ASSIGN &&
            node->sourceKind != XL6_AST_UNARY) continue;
        if (node->opcode != XL6_IR_OPCODE_BIT_AND && node->opcode != XL6_IR_OPCODE_BIT_OR &&
            node->opcode != XL6_IR_OPCODE_BIT_XOR && node->opcode != XL6_IR_OPCODE_BIT_NOT) continue;
        XL6_IrNode* left = &ir->nodes[ir->children[node->firstChild] - 1];
        const XL6_IrType* type = xl6IrTypeGet(&ir->types, left->type);
        if (!type || type->kind != XL6_IR_TYPE_NUMERIC_ENUM) continue;
        XL6_IrOpcode opcode = node->opcode;
        node->opcode = node->sourceKind == XL6_AST_UNARY ? XL6_IR_OPCODE_NEG : XL6_IR_OPCODE_ADD;
        refused(ir); node->opcode = opcode;
        XL6_IrTypeId storage = node->storageType;
        if (node->sourceKind != XL6_AST_ASSIGN) {
            node->storageType = other; refused(ir); node->storageType = storage;
            XL6_IrTypeId logical = node->type;
            node->type = other; refused(ir); node->type = logical;
        }
        XL6_IrTypeId original = left->type;
        left->type = other; refused(ir); left->type = original;
        if (node->childCount == 2) {
            XL6_IrNode* right = &ir->nodes[ir->children[node->firstChild + 1] - 1];
            original = right->type; right->type = other; refused(ir); right->type = original;
        }
        assert(xl6IrVerifyModule(ir, NULL));
        ++typed;
    }
    assert(typed == 7);
    assert(xl6IrLowerModule(ir, &unit.parse.diags));
    assert(xl6IrVerifyModule(ir, NULL));
    for (size_t i = 0; i < ir->instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir->instructions[i];
        const XL6_IrType* type = xl6IrTypeGet(&ir->types, instruction->type);
        if (!type || type->kind != XL6_IR_TYPE_NUMERIC_ENUM) continue;
        if (instruction->kind == XL6_IR_INSTR_CONST && instruction->immediateText &&
            !strcmp(instruction->immediateText, "255LL")) ++masks;
        if (instruction->kind != XL6_IR_INSTR_BINARY) continue;
        XL6_IrOpcode opcode = instruction->opcode;
        for (size_t j = 0; j < 3; ++j) {
            const XL6_IrOpcode invalid[] = {XL6_IR_OPCODE_ADD, XL6_IR_OPCODE_MOD, XL6_IR_OPCODE_SHL};
            instruction->opcode = invalid[j]; refused(ir);
        }
        instruction->opcode = opcode;
        XL6_IrTypeId original = instruction->type;
        instruction->type = other; refused(ir); instruction->type = original;
        if (instruction->origin && ir->nodes[instruction->origin - 1].opcode == XL6_IR_OPCODE_BIT_NOT) {
            XL6_IrInstruction saved = *instruction;
            instruction->kind = XL6_IR_INSTR_UNARY;
            instruction->opcode = XL6_IR_OPCODE_BIT_NOT;
            instruction->operandCount = 1;
            refused(ir); *instruction = saved;
        }
        assert(xl6IrVerifyModule(ir, NULL));
        ++lowered;
    }
    assert(lowered == 7 && masks == 1);
    xl6CompileUnitFree(&unit);
    printf("XLIR: nominal enum bitwise: %zu typed, %zu lowered, %zu width mask, %zu refusals\n",
        typed, lowered, masks, refusals);
    return 0;
}
