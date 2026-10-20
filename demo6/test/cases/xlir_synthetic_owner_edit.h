/* Test-only edits of complete Lowered XLIR. They preserve instruction arenas
 * and control-resource identities; callers rebuild derived machines afterwards.
 * This is not a production ownership repair pass. Synthetic owner regions must
 * provide their own exact sinks, including checked allocation failure edges. */
#ifndef XLIR_SYNTHETIC_OWNER_EDIT_H
#define XLIR_SYNTHETIC_OWNER_EDIT_H

static size_t syntheticDefinition(const XL6_IrModule* ir, XL6_IrValueId value)
{
    size_t i;
    for (i = 0; i < ir->instructionCount; ++i)
        if (value && ir->instructions[i].result == value) return i;
    assert(0); return SIZE_MAX;
}
static int syntheticViewOf(const XL6_IrModule* ir, XL6_IrValueId value, XL6_IrValueId root)
{
    size_t depth;
    for (depth = 0; depth <= ir->instructionCount; ++depth) {
        const XL6_IrInstruction* x;
        if (value == root) return 1;
        x = &ir->instructions[syntheticDefinition(ir, value)];
        if (x->kind != XL6_IR_INSTR_VALUE || x->operandCount != 1 || x->targetValue) return 0;
        value = ir->operands[x->firstOperand];
    }
    assert(0); return 0;
}
static void syntheticRetireOutcome(XL6_IrModule* ir, size_t producer)
{
    XL6_IrInstruction* test;
    assert(producer + 1 < ir->instructionCount);
    test = &ir->instructions[producer + 1];
    assert(test->kind == XL6_IR_INSTR_ERROR_TEST && test->operandCount == 1 &&
        ir->operands[test->firstOperand] == ir->instructions[producer].result);
    test->kind = XL6_IR_INSTR_JUMP; test->targetSymbol = 0;
    test->operandCount = 0; test->falseTarget = 0;
}
static int syntheticOwnerDisposal(const XL6_IrInstruction* x)
{
    return x->kind == XL6_IR_INSTR_VALUE_RELEASE;
}
static void syntheticShiftIdentity(size_t* value, size_t at, size_t count)
{
    if (*value && *value > at) *value += count;
}
static void syntheticInsert(XL6_IrModule* ir, XL6_IrFunction* fn, size_t block,
    size_t at, const XL6_IrInstruction* additions, size_t count)
{
    size_t i;
    assert(block >= fn->firstBlock && block < fn->firstBlock + fn->blockCount &&
        at >= ir->blocks[block].firstInstruction &&
        at < ir->blocks[block].firstInstruction + ir->blocks[block].instructionCount);
    ir->instructions = xrtRealloc(ir->instructions, (ir->instructionCount + count) * sizeof(*ir->instructions));
    assert(ir->instructions); ir->instructionCapacity = ir->instructionCount + count;
    memmove(ir->instructions + at + count, ir->instructions + at,
        (ir->instructionCount - at) * sizeof(*ir->instructions));
    memcpy(ir->instructions + at, additions, count * sizeof(*additions));
    ir->instructionCount += count;
    ir->stats.instructionCount = ir->instructionCount;
    for (i = 0; i < ir->instructionCount; ++i) {
        XL6_IrInstruction* x = &ir->instructions[i];
        syntheticShiftIdentity(&x->errorScope, at, count);
        syntheticShiftIdentity(&x->parameterScope, at, count);
        syntheticShiftIdentity(&x->evaluationScope, at, count);
    }
    for (i = 0; i < ir->blockCount; ++i) {
        if (i == block) ir->blocks[i].instructionCount += count;
        else if (ir->blocks[i].firstInstruction >= at) ir->blocks[i].firstInstruction += count;
    }
    for (i = 0; i < ir->functionCount; ++i) {
        if (&ir->functions[i] == fn) ir->functions[i].instructionCount += count;
        else if (ir->functions[i].firstInstruction >= at) ir->functions[i].firstInstruction += count;
    }
    for (i = 0; i < ir->pendingExceptionCount; ++i)
        syntheticShiftIdentity(&ir->pendingExceptions[i].capture, at, count);
    for (i = 0; i < ir->callbackBoundaryCount; ++i)
        if (ir->callbackBoundaries[i].instruction >= at) ir->callbackBoundaries[i].instruction += count;
}
static void syntheticRollbackEx(XL6_IrModule* ir, XL6_IrFunction* fn,
    size_t block, XL6_IrValueId owner, int checkedTerminal)
{
    XL6_IrInstruction release = {0};
    const XL6_IrInstruction* producer = &ir->instructions[syntheticDefinition(ir, owner)];
    size_t at = ir->blocks[block].firstInstruction;
    const XL6_IrInstruction* last = &ir->instructions[at + ir->blocks[block].instructionCount - 1];
    /* The synthetic region owns text on both allocation rollback and explicit
     * generator-close edges. Dispose the SSA owner directly; no source STORE
     * is inserted into a cleanup-only continuation. */
    assert(last->kind == XL6_IR_INSTR_THROW || last->kind == XL6_IR_INSTR_RETURN);
    assert(xl6IrTypeGet(&ir->types, producer->type)->cAbi == XL6_IR_C_ABI_MANAGED_STRING);
    release.kind = XL6_IR_INSTR_VALUE_RELEASE; release.type = producer->type;
    release.origin = last->kind == XL6_IR_INSTR_RETURN ? last->origin : producer->origin;
    release.sourceSpan = last->kind == XL6_IR_INSTR_RETURN ? last->sourceSpan : producer->sourceSpan;
    release.elementIndex = release.dynamicCallSite = release.enumVariant = release.enumPayload = SIZE_MAX;
    release.firstOperand = ir->operandCount; release.operandCount = 1;
    ir->operands = xrtRealloc(ir->operands, (ir->operandCount + 1) * sizeof(*ir->operands));
    assert(ir->operands); ir->operandCapacity = ir->operandCount + 1;
    ir->operands[ir->operandCount++] = owner;
    /* Preserve preceding observations and the mandatory END/terminal pair.
     * Join the existing failure cleanup scope when one ends at this exit. */
    if (checkedTerminal) {
        at += ir->blocks[block].instructionCount - 1;
        if (at > ir->blocks[block].firstInstruction && ir->instructions[at - 1].kind == XL6_IR_INSTR_ERROR_UNWIND_END) {
            --at; release.errorScope = ir->instructions[at].errorScope;
        }
    }
    syntheticInsert(ir, fn, block, at, &release, 1);
}
static void syntheticMergeState(int* states, size_t block, int state, int* changed)
{
    if (states[block] < 0) { states[block] = state; *changed = 1; }
    else assert(states[block] == state);
}
static void syntheticCloseOwnerExitsEx(XL6_IrModule* ir, XL6_IrFunction* fn, XL6_IrValueId root, int checkedTerminal)
{
    int *states = xrtMalloc(fn->blockCount * sizeof(*states)), changed = 1;
    unsigned char* rollback = xrtCalloc(fn->blockCount, 1);
    size_t b, pass;
    assert(states && rollback);
    for (b = 0; b < fn->blockCount; ++b) states[b] = -1;
    states[fn->entryBlock - 1 - fn->firstBlock] = 0;
    for (pass = 0; changed && pass <= fn->blockCount; ++pass) {
        changed = 0;
        for (b = 0; b < fn->blockCount; ++b) {
            const XL6_IrBlock* block = &ir->blocks[fn->firstBlock + b];
            const XL6_IrInstruction* last;
            size_t i, p; int state = states[b], failedState;
            if (state < 0) continue;
            for (i = block->firstInstruction; i < block->firstInstruction + block->instructionCount; ++i) {
                const XL6_IrInstruction* x = &ir->instructions[i];
                if (x->result == root) { assert(!state); state = 1; }
                for (p = 0; p < x->operandCount; ++p)
                    if (xl6IrInstructionOperandOwnership(ir, x, p) == XL6_IR_OPERAND_OWNERSHIP_CONSUME &&
                        syntheticViewOf(ir, ir->operands[x->firstOperand + p], root)) { assert(state); state = 0; }
            }
            assert(block->instructionCount);
            last = &ir->instructions[block->firstInstruction + block->instructionCount - 1];
            if (last->kind == XL6_IR_INSTR_RETURN || last->kind == XL6_IR_INSTR_THROW) {
                /* The replacement removed lowering's temporary owner. Give
                 * this synthetic SSA region an explicit sink on every live
                 * terminal edge, including checked cleanup added by callers. */
                if (checkedTerminal && state) rollback[b] = 1;
                else assert(!state);
                continue;
            }
            assert(last->kind == XL6_IR_INSTR_JUMP || last->kind == XL6_IR_INSTR_BRANCH || last->kind == XL6_IR_INSTR_ERROR_TEST);
            assert(last->trueTarget > fn->firstBlock && last->trueTarget <= fn->firstBlock + fn->blockCount);
            if (last->kind == XL6_IR_INSTR_BRANCH && state &&
                ir->instructions[syntheticDefinition(ir, ir->operands[last->firstOperand])].kind == XL6_IR_INSTR_GENERATOR_CLOSE_TEST) {
                rollback[last->trueTarget - 1 - fn->firstBlock] = 1;
                syntheticMergeState(states, last->trueTarget - 1 - fn->firstBlock, 0, &changed);
            } else syntheticMergeState(states, last->trueTarget - 1 - fn->firstBlock, state, &changed);
            if (last->kind == XL6_IR_INSTR_JUMP) continue;
            failedState = state;
            if (last->kind == XL6_IR_INSTR_ERROR_TEST && state) {
                const XL6_IrInstruction* producer = &ir->instructions[syntheticDefinition(ir, ir->operands[last->firstOperand])];
                if (producer->kind == XL6_IR_INSTR_RUNTIME_CALL &&
                    xl6IrRuntimeStringAllocationErrorPhysicalAbi(producer->runtimeOp) != XL6_IR_CALL_ERROR_PHYSICAL_INVALID) {
                    if (producer->result != root) rollback[last->falseTarget - 1 - fn->firstBlock] = 1;
                    failedState = 0;
                }
            }
            assert(last->falseTarget > fn->firstBlock && last->falseTarget <= fn->firstBlock + fn->blockCount);
            syntheticMergeState(states, last->falseTarget - 1 - fn->firstBlock, failedState, &changed);
        }
    }
    assert(!changed);
    for (b = 0; b < fn->blockCount; ++b)
        if (rollback[b]) syntheticRollbackEx(ir, fn, fn->firstBlock + b, root, checkedTerminal);
    xrtFree(states); xrtFree(rollback);
}
static void syntheticCloseOwnerExits(XL6_IrModule* ir, XL6_IrFunction* fn, XL6_IrValueId root)
{
    syntheticCloseOwnerExitsEx(ir, fn, root, 0);
}
#endif
