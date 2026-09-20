/* Shared source-contract assertion: replacement publishes before releasing
 * the detached old owner. This is an IR assertion, not generated-C matching. */
static void checkStorageReplacement(const XL6_IrModule* ir, const XL6_IrInstruction* write)
{
    const XL6_IrType* type = xl6IrTypeGet(&ir->types, write->type);
    const XL6_IrStorage* storage = xl6IrStorageGet(ir, write->targetSymbol);
    const XL6_IrInstruction* oldStore;
    XL6_IrValueId previous;
    size_t index = (size_t)(write - ir->instructions), i;
    int released = 0;
    assert(type && storage);
    if (type->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL) {
        assert(write->kind == (storage->cellType ? XL6_IR_INSTR_CELL_STORE : XL6_IR_INSTR_STORE));
        return;
    }
    if (storage->cellType) {
        assert(write->kind == XL6_IR_INSTR_CELL_EXCHANGE && write->result);
        previous = write->result;
    } else {
        const XL6_IrInstruction *read, *move;
        assert(index >= 2 && write->kind == XL6_IR_INSTR_STORE);
        read = write - 2; move = write - 1;
        assert(read->kind == XL6_IR_INSTR_LOAD && move->kind == XL6_IR_INSTR_MOVE);
        assert(read->targetSymbol == storage->symbol && move->targetSymbol == storage->symbol);
        assert(read->type == write->type && move->type == write->type);
        assert(move->operandCount == 1 && ir->operands[move->firstOperand] == read->result);
        previous = read->result;
    }
    assert(index + 1 < ir->instructionCount);
    oldStore = write + 1;
    assert(oldStore->kind == XL6_IR_INSTR_STORE && oldStore->type == write->type);
    assert(oldStore->targetSymbol != storage->symbol && oldStore->operandCount == 1);
    assert(ir->operands[oldStore->firstOperand] == previous);
    for (i = index + 2; i < ir->instructionCount; ++i) {
        const XL6_IrInstruction* step = &ir->instructions[i];
        if (step->kind == XL6_IR_INSTR_RELEASE && step->targetSymbol == oldStore->targetSymbol) {
            assert(step->origin == write->origin);
            released = 1; break;
        }
    }
    assert(released);
}
