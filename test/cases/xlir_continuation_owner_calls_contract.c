#include "xlir_continuation_owner_test.h"
#include "xlir_internal_owned_params.h"
typedef struct OwnerCallRegion { XL6_IrValueId root, alias; } OwnerCallRegion;
static XL6_IrInstruction* valueDefinition(XL6_IrModule* ir, XL6_IrValueId value)
{
    size_t i; for (i = 0; i < ir->instructionCount; ++i) if (ir->instructions[i].result == value && value) return &ir->instructions[i];
    assert(0); return NULL;
}
static const char* functionName(XL6_IrModule* ir, const XL6_IrFunction* fn)
{
    const XL6_IrNode* decl = xl6IrNodeGet(ir, fn->declaration);
    const XL6_IrNode* name = decl && decl->childCount ? xl6IrNodeGet(ir, ir->children[decl->firstChild]) : NULL;
    assert(name && name->text); return name->text;
}
/* Replacing a checked producer with an infallible alias/constant must also
 * retire that producer's outcome test. Preserve its success continuation and
 * arena positions; the now-unreachable rollback is not an alternate path. */
static void retireReplacedOutcome(XL6_IrModule* ir, XL6_IrInstruction* producer)
{
    size_t index = (size_t)(producer - ir->instructions);
    XL6_IrInstruction* test;
    if (index + 1 >= ir->instructionCount) return;
    test = &ir->instructions[index + 1];
    if (test->kind != XL6_IR_INSTR_ERROR_TEST) return;
    assert(test->operandCount == 1 && ir->operands[test->firstOperand] == producer->result);
    test->kind = XL6_IR_INSTR_JUMP; test->targetSymbol = 0;
    test->operandCount = 0; test->falseTarget = 0;
}
static void relocateFresh(XL6_IrModule* ir, XL6_IrFunction* fn, XL6_IrValueId firstFresh, size_t freshCount)
{
    XL6_IrValueId first = ir->valueCount + 1, oldFirst = fn->firstValue;
    size_t oldCount = fn->valueCount, i, p;
    unsigned char* seen = xrtCalloc(ir->operandCount, 1); assert(seen);
    for (i = fn->firstInstruction; i < fn->firstInstruction + fn->instructionCount; ++i) {
        XL6_IrInstruction* x = &ir->instructions[i]; XL6_IrValueId value = x->result;
        if (value) {
            if (value >= firstFresh && value - firstFresh < freshCount) x->result = first + (XL6_IrValueId)oldCount + value - firstFresh;
            else { assert(value >= oldFirst && value - oldFirst < oldCount); x->result = first + value - oldFirst; }
        }
        for (p = x->firstOperand; p < x->firstOperand + x->operandCount; ++p) if (!seen[p]) {
            seen[p] = 1; value = ir->operands[p];
            if (value >= firstFresh && value - firstFresh < freshCount) ir->operands[p] = first + (XL6_IrValueId)oldCount + value - firstFresh;
            else { assert(value >= oldFirst && value - oldFirst < oldCount); ir->operands[p] = first + value - oldFirst; }
        }
    }
    fn->firstValue = first; fn->valueCount = oldCount + freshCount;
    ir->valueCount = first + (XL6_IrValueId)fn->valueCount - 1; xrtFree(seen);
}
/* Convert one private byval staging chain into a pure SSA owner/view region.
 * The final CALL still has the same verified parameter/consume ABI. */
static OwnerCallRegion argumentRegion(XL6_IrModule* ir, const XL6_IrFunction* fn,
    XL6_IrInstruction* call, size_t operand, XL6_IrValueId replacement)
{
    XL6_IrValueId input = ir->operands[call->firstOperand + operand];
    XL6_IrInstruction *load = valueDefinition(ir, input), *store = NULL, *move = NULL, *copy;
    size_t i; OwnerCallRegion result;
    assert(load->kind == XL6_IR_INSTR_LOAD && load->targetSymbol);
    const XL6_IrStorage* storage = xl6IrStorageGet(ir, load->targetSymbol);
    assert(storage && storage->kind == XL6_IR_STORAGE_TEMPORARY && storage->owner == fn->declaration);
    for (i = fn->firstInstruction; i < (size_t)(call - ir->instructions); ++i) {
        XL6_IrInstruction* x = &ir->instructions[i];
        if (x->targetSymbol != load->targetSymbol) continue;
        if (x->kind == XL6_IR_INSTR_STORE) { assert(!store); store = x; }
        if (x->kind == XL6_IR_INSTR_MOVE) { assert(!move); move = x; }
    }
    assert(store && move && move->operandCount == 1 && ir->operands[move->firstOperand] == input);
    copy = valueDefinition(ir, ir->operands[store->firstOperand]);
    assert(copy->kind == XL6_IR_INSTR_RUNTIME_CALL && copy->runtimeOp == XL6_IR_RUNTIME_STRING_COPY);
    result.root = replacement ? replacement : copy->result;
    if (replacement) { asAlias(ir, copy, copy->result, replacement); retireReplacedOutcome(ir, copy); }
    result.alias = ++ir->valueCount;
    asAlias(ir, store, result.alias, copy->result);
    asAlias(ir, load, load->result, result.alias);
    asAlias(ir, move, ++ir->valueCount, load->result);
    return result;
}
static void checkCallMachine(XL6_IrModule* ir, int generator, size_t index, int aliased)
{
    const XL6_IrAsyncMachine* a = generator ? NULL : &ir->asyncMachines[index];
    const XL6_IrGeneratorMachine* g = generator ? &ir->generatorMachines[index] : NULL;
    XL6_IrContinuationFrameSlot* frames = generator ? ir->generatorFrameSlots : ir->asyncMachineFrameSlots;
    XL6_IrContinuationFrameAction* actions = generator ? ir->generatorFrameActions : ir->asyncFrameActions;
    XL6_IrContinuationLiveSlot* live = generator ? ir->generatorLiveSlots : ir->asyncLiveSlots;
    size_t firstFrame = generator ? g->firstFrameSlot : a->firstFrameSlot;
    size_t frameCount = generator ? g->frameSlotCount : a->frameSlotCount;
    size_t firstTransfer = generator ? g->firstTransferAction : a->firstTransferAction;
    size_t transferCount = generator ? g->transferActionCount : a->transferActionCount;
    size_t firstState = generator ? g->firstSuspension : a->firstSuspension;
    size_t firstLive = generator ? ir->generatorSuspensions[firstState].firstLiveSlot : ir->asyncSuspensions[firstState].firstLiveSlot;
    size_t liveCount = generator ? ir->generatorSuspensions[firstState].liveSlotCount : ir->asyncSuspensions[firstState].liveSlotCount;
    size_t i, owners = 0, ownerViews = 0, normalTransfers[2], normalCount = 0;
    for (i = firstFrame; i < firstFrame + frameCount; ++i)
        if (frames[i].transferAbi == XL6_IR_ASYNC_FRAME_TRANSFER_MOVE_OWNED) ++owners;
    assert(owners == (aliased ? 1u : 2u));
    for (i = firstTransfer; i < firstTransfer + transferCount; ++i)
        if (!syntheticOwnerDisposal(&ir->instructions[actions[i].instruction])) {
            assert(normalCount < 2); normalTransfers[normalCount++] = i;
        }
    assert(normalCount == 2);
    for (i = firstLive; i < firstLive + liveCount; ++i)
        if (frames[live[i].frameSlot].transferAbi == XL6_IR_ASYNC_FRAME_TRANSFER_MOVE_OWNED) {
            XL6_IrValueId saved = live[i].value;
            ++ownerViews;
            live[i].value = 0; reject(ir, NULL); live[i].value = saved;
        }
    /* Generator closing consumes the two resident roots directly; ordinary
     * continuation consumes their two call aliases. Async has no close edge. */
    assert(ownerViews == (generator ? 4u : 2u));
    assert((actions[normalTransfers[0]].instruction == actions[normalTransfers[1]].instruction) == !aliased);
    assert((actions[normalTransfers[0]].frameSlot == actions[normalTransfers[1]].frameSlot) == aliased);
    for (i = firstTransfer; i < firstTransfer + transferCount; ++i) {
        XL6_IrContinuationFrameAction* action = &actions[i], saved = *action;
        XL6_IrValueId* operands = &ir->operands[ir->instructions[action->instruction].firstOperand];
        assert(action->kind == XL6_IR_ASYNC_FRAME_ACTION_TRANSFER_BEFORE_INSTRUCTION);
        if (syntheticOwnerDisposal(&ir->instructions[action->instruction])) assert(action->value == operands[0]);
        else {
            assert(ir->instructions[action->instruction].kind == XL6_IR_INSTR_CALL);
            assert(action->value == operands[0] || (!aliased && action->value == operands[1]));
        }
        action->kind = XL6_IR_ASYNC_FRAME_ACTION_TRANSFER_ON_YIELD_COMMIT; reject(ir, NULL); *action = saved;
        action->instruction = SIZE_MAX; reject(ir, NULL); *action = saved;
        if (saved.value != frames[saved.frameSlot].value) {
            action->value = frames[saved.frameSlot].value; reject(ir, NULL); *action = saved;
        }
        action->value = 0; reject(ir, NULL); *action = saved;
        action->frameSlot = firstFrame + frameCount; reject(ir, NULL); *action = saved;
        missingAction(ir, generator, index, i, SIZE_MAX);
    }
    if (!aliased) {
        XL6_IrInstruction* call = &ir->instructions[actions[normalTransfers[0]].instruction];
        XL6_IrValueId saved = ir->operands[call->firstOperand + 1];
        ir->operands[call->firstOperand + 1] = ir->operands[call->firstOperand];
        reject(ir, NULL); ir->operands[call->firstOperand + 1] = saved;
    }
    if (!generator) {
        size_t state;
        XL6_IrAsyncResultCommitAbi savedCommit = a->resultCommitAbi;
        assert(savedCommit == XL6_IR_ASYNC_RESULT_COMMIT_OWNED_TRANSACTION);
        ir->asyncMachines[index].resultCommitAbi = XL6_IR_ASYNC_RESULT_COMMIT_INVALID;
        reject(ir, "result commit transaction"); ir->asyncMachines[index].resultCommitAbi = savedCommit;
        for (state = a->firstSuspension; state < a->firstSuspension + a->suspensionCount; ++state) {
            XL6_IrAsyncSuspension* suspension = &ir->asyncSuspensions[state];
            size_t expected = 0, uniqueViews = 0, n, p;
            for (n = 0; n < suspension->liveSlotCount; ++n) {
                size_t frame = live[suspension->firstLiveSlot + n].frameSlot;
                for (p = 0; p < n; ++p) if (live[suspension->firstLiveSlot + p].frameSlot == frame) break;
                if (p == n) ++uniqueViews;
            }
            for (n = firstFrame; n < firstFrame + frameCount; ++n) {
                if (frames[n].initBit != SIZE_MAX) { ++expected; continue; }
                for (p = 0; p < suspension->liveSlotCount; ++p)
                    if (live[suspension->firstLiveSlot + p].frameSlot == n) { ++expected; break; }
            }
            assert(expected == suspension->rollbackActionCount);
            {
                size_t saved = suspension->setupFailureEntry;
                assert(ir->asyncMachineEntries[saved].kind == XL6_IR_ASYNC_MACHINE_ENTRY_RESUME_FAILURE &&
                    ir->asyncMachineEntries[saved].suspension == state);
                suspension->setupFailureEntry = SIZE_MAX; reject(ir, "setup failure");
                suspension->setupFailureEntry = saved - 1; reject(ir, "setup failure");
                suspension->setupFailureEntry = saved;
            }
            assert(expected == suspension->handoffActionCount &&
                suspension->liveSlotCount - uniqueViews == (aliased ? 1u : 0u));
            for (n = 0; n < suspension->handoffActionCount; ++n) {
                size_t actionIndex = suspension->firstHandoffAction + n;
                XL6_IrAsyncFrameAction* action = &actions[actionIndex], saved = *action;
                assert(action->kind == XL6_IR_ASYNC_FRAME_ACTION_HANDOFF_RESIDENT && action->suspension == state);
                for (p = 0; p < n; ++p) assert(actions[suspension->firstHandoffAction + p].frameSlot != action->frameSlot);
                action->value = saved.value ? 0 : ir->valueCount + 1; reject(ir, NULL); *action = saved;
                action->kind = XL6_IR_ASYNC_FRAME_ACTION_PROJECT_VALUE; reject(ir, NULL); *action = saved;
                action->instruction = suspension->awaitInstruction; reject(ir, NULL); *action = saved;
                missingAction(ir, 0, index, actionIndex, state);
            }
            for (n = 0; n < suspension->rollbackActionCount; ++n) {
                size_t actionIndex = suspension->firstRollbackAction + n;
                XL6_IrAsyncFrameAction* action = &actions[actionIndex], saved = *action;
                const XL6_IrAsyncFrameAction* forward = &actions[suspension->firstHandoffAction + expected - 1 - n];
                assert(action->kind == XL6_IR_ASYNC_FRAME_ACTION_ROLLBACK_HANDOFF_RESIDENT &&
                    action->frameSlot == forward->frameSlot && action->value == forward->value);
                action->kind = XL6_IR_ASYNC_FRAME_ACTION_HANDOFF_RESIDENT; reject(ir, NULL); *action = saved;
                action->frameSlot = firstFrame + frameCount; reject(ir, NULL); *action = saved;
                action->instruction = suspension->awaitInstruction; reject(ir, NULL); *action = saved;
                missingAction(ir, 0, index, actionIndex, state);
            }
        }
    }
    verify(ir);
}
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; size_t i, f, changed = 0;
    XL6_SymbolId placeholder = 0, pair = 0, targets[3] = {0};
    XL6_IrInstruction zero = {0};
    assert(argc == 4); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/xlir_continuation_owner_calls.xl", &parse));
    /* This pass fixture needs consuming physical call edges, not the ordinary
     * read-only language parameter ABI. Construct that internal contract
     * explicitly; do not disable production copy elision or revive syntax. */
    assert(xlirTestMarkOwnedNamedParams(parse.root,"ownerCallPlaceholder",1)==1);
    assert(xlirTestMarkOwnedNamedParams(parse.root,"ownerCallPair",3)==2);
    assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    assert(xl6SemCheckModule(parse.root, &parse.diags, "owner_calls"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    assert(xl6IrLowerModule(&ir, &parse.diags)); verify(&ir);
    for (f = 0; f < ir.functionCount; ++f) {
        const char* name = functionName(&ir, &ir.functions[f]);
        if (!strcmp(name, "ownerCallPlaceholder")) placeholder = ir.functions[f].symbol;
        if (!strcmp(name, "ownerCallPair")) pair = ir.functions[f].symbol;
    }
    for (i = 0; i < ir.instructionCount; ++i)
        if (ir.instructions[i].kind == XL6_IR_INSTR_CONST && ir.instructions[i].immediateText &&
            xl6IrTypeGet(&ir.types, ir.instructions[i].type)->cAbi == XL6_IR_C_ABI_I64) { zero = ir.instructions[i]; break; }
    assert(placeholder && pair && zero.kind == XL6_IR_INSTR_CONST);
    for (f = 0; f < ir.functionCount; ++f) {
        XL6_IrFunction* fn = &ir.functions[f]; const char* name = functionName(&ir, fn);
        size_t kind, boundary = SIZE_MAX, acquired = 0, sinks = 0;
        XL6_IrValueId firstFresh = ir.valueCount + 1; OwnerCallRegion owners[2];
        if (!strcmp(name, "ownerCallGenerator")) kind = 0;
        else if (!strcmp(name, "ownerCallAlias")) kind = 1;
        else if (!strcmp(name, "ownerCallAsyncPair")) kind = 2;
        else continue;
        for (i = fn->firstInstruction; i < fn->firstInstruction + fn->instructionCount; ++i)
            if (ir.instructions[i].kind == XL6_IR_INSTR_AWAIT || ir.instructions[i].kind == XL6_IR_INSTR_YIELD) { boundary = i; break; }
        assert(boundary != SIZE_MAX);
        for (i = fn->firstInstruction; i < fn->firstInstruction + fn->instructionCount; ++i) {
            XL6_IrInstruction* call = &ir.instructions[i];
            if (call->kind != XL6_IR_INSTR_CALL || (call->targetSymbol != placeholder && call->targetSymbol != pair)) continue;
            if (i < boundary) {
                XL6_IrInstruction original = *call;
                XL6_IrValueId result = call->result; XL6_IrNodeId origin = call->origin; XL6_IrTypeId type = call->type;
                assert(call->targetSymbol == placeholder && acquired < 2 && type == zero.type);
                owners[acquired++] = argumentRegion(&ir, fn, call, 0, 0);
                *call = zero; call->immediateText = xrtStrDup(zero.immediateText); assert(call->immediateText);
                call->result = result; call->origin = origin;
                call->sourceSpan = original.sourceSpan;
                retireReplacedOutcome(&ir, call);
            } else if (kind == 1) {
                assert(acquired == 1 && call->targetSymbol == placeholder && sinks < 2);
                argumentRegion(&ir, fn, call, 0, sinks++ ? owners[0].alias : owners[0].root);
            } else {
                assert(acquired == 2 && call->targetSymbol == pair && !sinks++);
                argumentRegion(&ir, fn, call, 0, owners[0].alias);
                argumentRegion(&ir, fn, call, 1, owners[1].alias);
            }
        }
        assert(acquired == (kind == 1 ? 1u : 2u) && sinks == (kind == 1 ? 2u : 1u));
        for (i = 0; i < acquired; ++i) syntheticCloseOwnerExitsEx(&ir, fn, owners[i].root, 1);
        relocateFresh(&ir, fn, firstFresh, ir.valueCount - firstFresh + 1);
        targets[kind] = fn->symbol; ++changed;
    }
    assert(changed == 3 && xl6IrBuildAsyncMachines(&ir, &parse.diags) && xl6IrBuildGeneratorMachines(&ir, &parse.diags));
    verify(&ir);
    for (i = 0; i < ir.asyncMachineCount; ++i)
        checkCallMachine(&ir, 0, i, ir.asyncMachines[i].targetSymbol == targets[1]);
    assert(ir.asyncMachineCount == 2 && ir.asyncSuspensionCount == 4 && ir.generatorMachineCount == 1);
    checkCallMachine(&ir, 1, 0, 0);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[1]));
    if (!xl6IrCgenEmitC(&out, &ir, &parse.diags)) {
        xl6OutputClose(&out);
        for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%s\n", parse.diags.items[i].message);
        xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 3;
    }
    xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]) && xl6IrCgenEmitHeader(&out, &ir, "OWNER_CALLS", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    {
        char binding[1024]; assert(snprintf(binding, sizeof(binding), "%s.bind.h", argv[2]) > 0);
        assert(xl6OutputOpenFile(&out, binding));
        xl6OutputPrintf(&out, "#define OWNER_CALLS_GENERATOR xlir_generator_start_%u\n#define OWNER_CALLS_ALIAS xlir_async_start_%u\n#define OWNER_CALLS_PAIR xlir_async_start_%u\n", targets[0], targets[1], targets[2]);
        xl6OutputClose(&out);
    }
    printf("continuation owner calls: 3 verified SSA owner regions, 6 exact consumer actions, %llu mutations rejected\n", (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
