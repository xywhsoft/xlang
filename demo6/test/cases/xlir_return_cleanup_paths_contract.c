/* Exercise the verifier's internal CFG proof without admitting a test-only
 * public compiler API. Other compiler components are linked normally. */
#include "src/ir/xl6_ir.c"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    XL6_IrModule module = {0};
    XL6_IrFunction function = {0};
    XL6_IrBlock blocks[4] = {{0}};
    XL6_IrInstruction instructions[7] = {{0}};
    XL6_IrInstruction saved;
    module.blocks = blocks; module.blockCount = 4;
    module.instructions = instructions; module.instructionCount = 7;
    function.blockCount = 4; function.instructionCount = 7;
    blocks[0].firstInstruction = 0; blocks[0].instructionCount = 1;
    blocks[1].firstInstruction = 1; blocks[1].instructionCount = 3;
    blocks[2].firstInstruction = 4; blocks[2].instructionCount = 2;
    blocks[3].firstInstruction = 6; blocks[3].instructionCount = 1;
    instructions[0].kind = XL6_IR_INSTR_ERROR_TEST;
    instructions[0].trueTarget = 2; instructions[0].falseTarget = 3;
    instructions[1].kind = XL6_IR_INSTR_STORE; instructions[1].targetSymbol = 20;
    instructions[2].kind = XL6_IR_INSTR_RELEASE; instructions[2].targetSymbol = 11; instructions[2].type = 1;
    instructions[3].kind = XL6_IR_INSTR_JUMP; instructions[3].trueTarget = 4;
    instructions[4] = instructions[2];
    instructions[5].kind = XL6_IR_INSTR_THROW;
    instructions[6].kind = XL6_IR_INSTR_RETURN;
    assert(xl6IrBorrowedTemporaryReleaseSuccessors(&module, &function, 0, 11, 1));
    saved = instructions[4];
    instructions[4].kind = XL6_IR_INSTR_RETURN;
    assert(!xl6IrBorrowedTemporaryReleaseSuccessors(&module, &function, 0, 11, 1)); instructions[4] = saved;
    instructions[2].type = 2;
    assert(!xl6IrBorrowedTemporaryReleaseSuccessors(&module, &function, 0, 11, 1)); instructions[2].type = 1;
    instructions[1].targetSymbol = 11;
    assert(!xl6IrBorrowedTemporaryReleaseSuccessors(&module, &function, 0, 11, 1)); instructions[1].targetSymbol = 20;
    instructions[4].kind = XL6_IR_INSTR_MOVE;
    assert(!xl6IrBorrowedTemporaryReleaseSuccessors(&module, &function, 0, 11, 1)); instructions[4] = saved;
    instructions[0].falseTarget = 0;
    assert(!xl6IrBorrowedTemporaryReleaseSuccessors(&module, &function, 0, 11, 1)); instructions[0].falseTarget = 3;
    instructions[4].kind = XL6_IR_INSTR_JUMP; instructions[4].targetSymbol = 0; instructions[4].trueTarget = 3;
    assert(!xl6IrBorrowedTemporaryReleaseSuccessors(&module, &function, 0, 11, 1)); instructions[4] = saved;
    instructions[4].kind = XL6_IR_INSTR_JUMP; instructions[4].targetSymbol = 0; instructions[4].trueTarget = 4;
    assert(!xl6IrBorrowedTemporaryReleaseSuccessors(&module, &function, 0, 11, 1)); instructions[4] = saved;
    blocks[1].instructionCount = 1;
    assert(!xl6IrBorrowedTemporaryReleaseSuccessors(&module, &function, 0, 11, 1)); blocks[1].instructionCount = 3;
    instructions[4].kind = XL6_IR_INSTR_JUMP; instructions[4].targetSymbol = 0; instructions[4].trueTarget = 2;
    assert(xl6IrBorrowedTemporaryReleaseSuccessors(&module, &function, 0, 11, 1)); instructions[4] = saved;
    assert(xl6IrBorrowedTemporaryReleaseSuccessors(&module, &function, 0, 11, 1));
    puts("return cleanup paths: both successors and shared cleanup accepted; 8 missing/conflicting/cyclic lifetime proofs rejected");
    return 0;
}
