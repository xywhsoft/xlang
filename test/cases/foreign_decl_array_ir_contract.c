#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>

static unsigned mutations;
static void refused(XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir, NULL));
    for (unsigned header = 0; header < 2; ++header) {
        XL6_Output out; size_t bytes = SIZE_MAX;
        assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out, ir, "BAD_C_ARRAY", NULL) :
            xl6IrCgenEmitC(&out, ir, NULL)));
        char* text = xl6OutputTakeMemoryText(&out, &bytes);
        assert(text && bytes == 0);
        xrtFree(text); xl6OutputDiscard(&out);
    }
    ++mutations;
}

int main(int argc, char** argv)
{
    assert(argc == 2);
    XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit, argv[1], 1));
    assert(xl6IrLowerModule(&unit.ir, &unit.parse.diags));
    XL6_IrModule* ir = &unit.ir;
    assert(xl6IrVerifyModule(ir, NULL));
    unsigned seen = 0;
    for (size_t i = 0; i < ir->instructionCount; ++i) {
        XL6_IrInstruction* ins = &ir->instructions[i];
        if (ins->kind < XL6_IR_INSTR_C_ARRAY_BOUNDS || ins->kind > XL6_IR_INSTR_C_ARRAY_SET) continue;
        unsigned bit = 1u << (ins->kind - XL6_IR_INSTR_C_ARRAY_BOUNDS);
        if (seen & bit) continue;
        seen |= bit;
        XL6_IrInstruction saved = *ins;
#define BAD(F,V) do { ins->F = (V); refused(ir); *ins = saved; } while (0)
        BAD(operandCount, 0); BAD(type, 0); BAD(targetSymbol, 1);
        BAD(elementIndex, 0); BAD(immediateText, "0"); BAD(runtimeOp, 1);
#undef BAD
        XL6_IrValueId* operands = &ir->operands[ins->firstOperand];
        XL6_IrValueId key = operands[1]; operands[1] = operands[0]; refused(ir); operands[1] = key;
        if (ins->kind != XL6_IR_INSTR_C_ARRAY_BOUNDS) {
            size_t proofIndex = ins->operandCount - 1;
            XL6_IrValueId proof = operands[proofIndex];
            operands[proofIndex] = key; refused(ir); operands[proofIndex] = proof;
            unsigned edges = 0;
            for (size_t j = 0; j < ir->instructionCount; ++j) {
                XL6_IrInstruction* branch = &ir->instructions[j];
                if (branch->kind != XL6_IR_INSTR_BRANCH || branch->operandCount != 1 ||
                    ir->operands[branch->firstOperand] != proof) continue;
                XL6_IrBlockId target = branch->trueTarget;
                branch->trueTarget = branch->falseTarget;
                branch->falseTarget = target;
                refused(ir);
                branch->falseTarget = branch->trueTarget;
                branch->trueTarget = target;
                ++edges;
            }
            assert(edges);
        }
        assert(xl6IrVerifyModule(ir, NULL));
    }
    assert(seen == 7);
    unsigned types = 0;
    for (size_t i = 0; i < ir->types.count; ++i) {
        XL6_IrType* type = &ir->types.items[i];
        if (type->kind != XL6_IR_TYPE_C_ARRAY) continue;
        uint64 saved = type->nativeArrayExtent;
        type->nativeArrayExtent = 0; refused(ir);
        type->nativeArrayExtent = saved + 1; refused(ir);
        type->nativeArrayExtent = saved;
        ++types;
    }
    assert(types >= 2 && xl6IrVerifyModule(ir, NULL));
    printf("fixed C arrays: %u types, %u invalid IR mutations refused before C/header output\n", types, mutations);
    xl6CompileUnitFree(&unit);
    return 0;
}
