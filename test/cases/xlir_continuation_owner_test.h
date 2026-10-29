#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static size_t mutations;
static void reject(XL6_IrModule* ir, const char* requiredDiagnostic)
{
    XL6_DiagList diags = {0}; XL6_Output out; char* text; size_t i; int found = !requiredDiagnostic;
    assert(!xl6IrVerifyModule(ir, &diags) && diags.count);
    for (i = 0; i < diags.count; ++i)
        if (requiredDiagnostic && strstr(diags.items[i].message, requiredDiagnostic)) found = 1;
    if (!found) {
        fprintf(stderr, "Mutation %lu expected diagnostic: %s\n", (unsigned long)(mutations + 1), requiredDiagnostic);
        xl6DiagPrintText(&diags, "mutated XLIR");
    }
    assert(found);
    assert(xl6OutputOpenMemory(&out) && !xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    assert(xl6OutputOpenMemory(&out) && !xl6IrCgenEmitHeader(&out, ir, "BAD_OWNER_VIEW", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}
static void verify(XL6_IrModule* ir)
{
    XL6_DiagList diags = {0}; size_t i; int valid = xl6IrVerifyModule(ir, &diags);
    if (!valid) for (i = 0; i < diags.count; ++i) fprintf(stderr, "%s\n", diags.items[i].message);
    assert(valid); xl6DiagFree(&diags);
}
static void asAlias(XL6_IrModule* ir, XL6_IrInstruction* x, XL6_IrValueId value, XL6_IrValueId input)
{
    ir->operands = (XL6_IrValueId*)xrtRealloc(ir->operands, (ir->operandCount + 1) * sizeof(*ir->operands));
    assert(ir->operands); ir->operandCapacity = ir->operandCount + 1;
    x->kind = XL6_IR_INSTR_VALUE; x->result = value;
    x->firstOperand = ir->operandCount++; x->operandCount = 1;
    ir->operands[x->firstOperand] = input;
    x->targetSymbol = 0; x->runtimeOp = XL6_IR_RUNTIME_NONE;
    x->generatorYieldAbi = XL6_IR_GENERATOR_YIELD_INVALID;
    x->generatorClosePointAbi = XL6_IR_GENERATOR_CLOSE_POINT_INVALID;
}
#include "xlir_synthetic_owner_edit.h"
static void removeActionIndex(size_t* first, size_t removed)
{
    if (*first != SIZE_MAX && *first > removed) --*first;
}
/* Delete a real live alias and its matching project command, coherently
 * repairing every arena slice. Shape consistency is insufficient: the
 * verifier must recompute liveness from the original instruction operands. */
static void missingView(XL6_IrModule* ir, size_t state, size_t liveIndex, size_t actionIndex)
{
    size_t i, liveCount = ir->generatorLiveSlotCount, actionCount = ir->generatorFrameActionCount;
    XL6_IrContinuationLiveSlot* lives = xrtMalloc(liveCount * sizeof(*lives));
    XL6_IrContinuationFrameAction* actions = xrtMalloc(actionCount * sizeof(*actions));
    XL6_IrGeneratorSuspension* states = xrtMalloc(ir->generatorSuspensionCount * sizeof(*states));
    XL6_IrGeneratorMachine* machines = xrtMalloc(ir->generatorMachineCount * sizeof(*machines));
    assert(lives && actions && states && machines);
    memcpy(lives, ir->generatorLiveSlots, liveCount * sizeof(*lives));
    memcpy(actions, ir->generatorFrameActions, actionCount * sizeof(*actions));
    memcpy(states, ir->generatorSuspensions, ir->generatorSuspensionCount * sizeof(*states));
    memcpy(machines, ir->generatorMachines, ir->generatorMachineCount * sizeof(*machines));
    memmove(&ir->generatorLiveSlots[liveIndex], &ir->generatorLiveSlots[liveIndex + 1],
        (liveCount - liveIndex - 1) * sizeof(*lives)); --ir->generatorLiveSlotCount;
    memmove(&ir->generatorFrameActions[actionIndex], &ir->generatorFrameActions[actionIndex + 1],
        (actionCount - actionIndex - 1) * sizeof(*actions)); --ir->generatorFrameActionCount;
    for (i = 0; i < ir->generatorSuspensionCount; ++i) {
        XL6_IrGeneratorSuspension* s = &ir->generatorSuspensions[i];
        removeActionIndex(&s->firstLiveSlot, liveIndex);
        removeActionIndex(&s->firstSpillAction, actionIndex);
        removeActionIndex(&s->firstResumeAction, actionIndex);
        if (i == state) { --s->liveSlotCount; --s->resumeActionCount; }
    }
    for (i = 0; i < ir->generatorMachineCount; ++i) {
        XL6_IrGeneratorMachine* m = &ir->generatorMachines[i];
        removeActionIndex(&m->firstAction, actionIndex);
        removeActionIndex(&m->firstPrologueAction, actionIndex);
        removeActionIndex(&m->firstDestroyAction, actionIndex);
        removeActionIndex(&m->firstTransferAction, actionIndex);
        if (i == states[state].machine) --m->actionCount;
    }
    reject(ir, "live views do not match instruction/CFG liveness");
    ir->generatorLiveSlotCount = liveCount; ir->generatorFrameActionCount = actionCount;
    memcpy(ir->generatorLiveSlots, lives, liveCount * sizeof(*lives));
    memcpy(ir->generatorFrameActions, actions, actionCount * sizeof(*actions));
    memcpy(ir->generatorSuspensions, states, ir->generatorSuspensionCount * sizeof(*states));
    memcpy(ir->generatorMachines, machines, ir->generatorMachineCount * sizeof(*machines));
    xrtFree(lives); xrtFree(actions); xrtFree(states); xrtFree(machines);
}
static void checkOwnerViews(XL6_IrModule* ir, XL6_SymbolId target, int aliased)
{
    size_t releaseIndex;
    for (releaseIndex = 0; releaseIndex < ir->instructionCount; ++releaseIndex) {
        XL6_IrInstruction* release = &ir->instructions[releaseIndex];
        XL6_IrInstruction saved = *release;
        if (release->kind != XL6_IR_INSTR_VALUE_RELEASE) continue;
        release->kind = XL6_IR_INSTR_DISCARD; reject(ir, NULL); *release = saved;
        release->targetSymbol = UINT32_MAX; reject(ir, NULL); *release = saved;
        release->operandCount = 0; reject(ir, NULL); *release = saved;
        release->type = XL6_IR_TYPE_INVALID; reject(ir, NULL); *release = saved;
        release->errorScope = SIZE_MAX; reject(ir, NULL); *release = saved;
    }
    size_t m, f, i, owners = 0, owner = SIZE_MAX, aliases = 0;
    for (m = 0; m < ir->generatorMachineCount; ++m) if (ir->generatorMachines[m].targetSymbol == target) break;
    assert(m < ir->generatorMachineCount);
    XL6_IrGeneratorMachine* machine = &ir->generatorMachines[m];
    for (f = machine->firstFrameSlot; f < machine->firstFrameSlot + machine->frameSlotCount; ++f)
        if (ir->generatorFrameSlots[f].transferAbi == XL6_IR_ASYNC_FRAME_TRANSFER_MOVE_OWNED) { ++owners; owner = f; }
    assert(owners == 1);
    {
        size_t normal = 0;
        for (i = machine->firstTransferAction; i < machine->firstTransferAction + machine->transferActionCount; ++i) {
            XL6_IrContinuationFrameAction* transfer = &ir->generatorFrameActions[i];
            assert(transfer->frameSlot == owner && transfer->instruction < ir->instructionCount);
            if (syntheticOwnerDisposal(&ir->instructions[transfer->instruction]))
                assert(transfer->kind == XL6_IR_ASYNC_FRAME_ACTION_TRANSFER_BEFORE_INSTRUCTION);
            else { assert(transfer->kind == XL6_IR_ASYNC_FRAME_ACTION_TRANSFER_ON_YIELD_COMMIT); ++normal; }
        }
        assert(normal == 1);
    }
    for (i = machine->firstAction; i < machine->firstAction + machine->actionCount; ++i) {
        XL6_IrContinuationFrameAction* action = &ir->generatorFrameActions[i];
        XL6_IrContinuationFrameAction saved = *action;
        action->value = saved.value ? 0 : ir->generatorFrameSlots[owner].value; reject(ir, NULL); *action = saved;
        action->instruction = saved.instruction == SIZE_MAX ? 0 : SIZE_MAX; reject(ir, NULL); *action = saved;
        action->frameSlot = ir->generatorFrameSlotCount; reject(ir, NULL); *action = saved;
        if (action->kind == XL6_IR_ASYNC_FRAME_ACTION_TRANSFER_ON_YIELD_COMMIT) {
            action->kind = XL6_IR_ASYNC_FRAME_ACTION_TRANSFER_BEFORE_INSTRUCTION; reject(ir, NULL); *action = saved;
        }
    }
    XL6_IrGeneratorSuspension* first = &ir->generatorSuspensions[machine->firstSuspension];
    assert(first->spillActionCount == 1 && first->resumeActionCount == (aliased ? 2u : 1u));
    for (i = first->firstLiveSlot; i < first->firstLiveSlot + first->liveSlotCount; ++i) {
        XL6_IrContinuationLiveSlot* live = &ir->generatorLiveSlots[i];
        XL6_IrContinuationLiveSlot saved = *live;
        if (live->frameSlot != owner) continue;
        live->value = 0; reject(ir, NULL); *live = saved;
        if (live->value != ir->generatorFrameSlots[owner].value) {
            size_t action;
            ++aliases;
            for (action = first->firstResumeAction; action < first->firstResumeAction + first->resumeActionCount; ++action)
                if (ir->generatorFrameActions[action].value == live->value) break;
            assert(action < first->firstResumeAction + first->resumeActionCount);
            missingView(ir, machine->firstSuspension, i, action);
            XL6_IrValueId resident = ir->generatorFrameSlots[owner].value;
            ir->generatorFrameSlots[owner].value = live->value; reject(ir, NULL);
            ir->generatorFrameSlots[owner].value = resident;
        }
    }
    assert(aliases == (aliased ? 1u : 0u)); verify(ir);
}
static void relocateOwnerViews(XL6_IrModule* ir, XL6_IrFunction* fn, XL6_IrValueId alias)
{
    XL6_IrInstruction* x = &ir->instructions[fn->firstInstruction];
    XL6_IrValueId first = ir->valueCount + 1, oldFirst = fn->firstValue;
    size_t count = fn->valueCount, n, p;
    unsigned char* seen = (unsigned char*)xrtCalloc(ir->operandCount, 1); assert(seen);
    for (n = 0; n < fn->instructionCount; ++n) {
        XL6_IrValueId value = x[n].result;
        if (value == alias) x[n].result = first + (XL6_IrValueId)count;
        else if (value) { assert(value >= oldFirst && value - oldFirst < count); x[n].result = first + value - oldFirst; }
        for (p = x[n].firstOperand; p < x[n].firstOperand + x[n].operandCount; ++p) {
            if (seen[p]) continue;
            seen[p] = 1; value = ir->operands[p];
            if (value == alias) ir->operands[p] = first + (XL6_IrValueId)count;
            else { assert(value >= oldFirst && value - oldFirst < count); ir->operands[p] = first + value - oldFirst; }
        }
    }
    fn->firstValue = first; fn->valueCount = count + 1; ir->valueCount = first + (XL6_IrValueId)count;
    xrtFree(seen);
}

static void missingAction(XL6_IrModule* ir, int generator, size_t machineIndex, size_t removed, size_t handoffState)
{
    XL6_IrModule copy = *ir;
    size_t count = generator ? ir->generatorFrameActionCount : ir->asyncFrameActionCount;
    size_t machineCount = generator ? ir->generatorMachineCount : ir->asyncMachineCount;
    size_t stateCount = generator ? ir->generatorSuspensionCount : ir->asyncSuspensionCount;
    XL6_IrContinuationFrameAction* actions = xrtMalloc(count * sizeof(*actions));
    size_t m, s;
    XL6_IrAsyncFrameActionKind removedKind = (generator ? ir->generatorFrameActions : ir->asyncFrameActions)[removed].kind;
    assert(actions && removed < count);
    memcpy(actions, generator ? ir->generatorFrameActions : ir->asyncFrameActions, count * sizeof(*actions));
    memmove(actions + removed, actions + removed + 1, (count - removed - 1) * sizeof(*actions));
    if (generator) {
        copy.generatorFrameActions = actions; --copy.generatorFrameActionCount;
        copy.generatorMachines = xrtMalloc(machineCount * sizeof(*copy.generatorMachines));
        copy.generatorSuspensions = xrtMalloc(stateCount * sizeof(*copy.generatorSuspensions));
        assert(copy.generatorMachines && copy.generatorSuspensions);
        memcpy(copy.generatorMachines, ir->generatorMachines, machineCount * sizeof(*copy.generatorMachines));
        memcpy(copy.generatorSuspensions, ir->generatorSuspensions, stateCount * sizeof(*copy.generatorSuspensions));
    } else {
        copy.asyncFrameActions = actions; --copy.asyncFrameActionCount;
        copy.asyncMachines = xrtMalloc(machineCount * sizeof(*copy.asyncMachines));
        copy.asyncSuspensions = xrtMalloc(stateCount * sizeof(*copy.asyncSuspensions));
        assert(copy.asyncMachines && copy.asyncSuspensions);
        memcpy(copy.asyncMachines, ir->asyncMachines, machineCount * sizeof(*copy.asyncMachines));
        memcpy(copy.asyncSuspensions, ir->asyncSuspensions, stateCount * sizeof(*copy.asyncSuspensions));
    }
    for (m = 0; m < machineCount; ++m) {
        XL6_IrAsyncMachine* a = generator ? NULL : &copy.asyncMachines[m];
        XL6_IrGeneratorMachine* g = generator ? &copy.generatorMachines[m] : NULL;
#define FIX_RANGE(field) removeActionIndex(generator ? &g->field : &a->field, removed)
        FIX_RANGE(firstAction); FIX_RANGE(firstPrologueAction); FIX_RANGE(firstDestroyAction); FIX_RANGE(firstTransferAction);
#undef FIX_RANGE
        if (m == machineIndex) {
            if (generator) { --g->actionCount; --g->transferActionCount; }
            else { --a->actionCount; if (handoffState == SIZE_MAX) --a->transferActionCount; }
        }
    }
    for (s = 0; s < stateCount; ++s) {
        if (generator) {
            removeActionIndex(&copy.generatorSuspensions[s].firstSpillAction, removed);
            removeActionIndex(&copy.generatorSuspensions[s].firstResumeAction, removed);
        } else {
            removeActionIndex(&copy.asyncSuspensions[s].firstSpillAction, removed);
            removeActionIndex(&copy.asyncSuspensions[s].firstResumeAction, removed);
            removeActionIndex(&copy.asyncSuspensions[s].firstHandoffAction, removed);
            removeActionIndex(&copy.asyncSuspensions[s].firstRollbackAction, removed);
            if (s == handoffState) {
                if (removedKind == XL6_IR_ASYNC_FRAME_ACTION_ROLLBACK_HANDOFF_RESIDENT) {
                    if (!--copy.asyncSuspensions[s].rollbackActionCount) copy.asyncSuspensions[s].firstRollbackAction = SIZE_MAX;
                } else if (!--copy.asyncSuspensions[s].handoffActionCount)
                    copy.asyncSuspensions[s].firstHandoffAction = SIZE_MAX;
            }
        }
    }
    reject(&copy, "owner/view/transfer actions are not canonical");
    if (generator) { xrtFree(copy.generatorMachines); xrtFree(copy.generatorSuspensions); }
    else { xrtFree(copy.asyncMachines); xrtFree(copy.asyncSuspensions); }
    xrtFree(actions);
}
