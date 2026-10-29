#include "xlir_continuation_owner_test.h"
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    XL6_IrAsyncMachine* machine; size_t i, cells = 0, origins = 0; char binding[1024];
    XL6_IrNodeId awaitOrigins[3] = {0};
    assert(argc == 4); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_cell_cancel_race.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "cell_cancel_race") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) || !xl6IrLowerModule(&ir, &parse.diags)) {
        for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%d:%d %s\n",
            parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
        return 2;
    }
    verify(&ir); assert(ir.asyncMachineCount == 1); machine = &ir.asyncMachines[0];
    // The finally await has separate normal and exceptional CFG instances.
    assert(machine->suspensionCount == 3 && machine->cancellationAbi == XL6_IR_ASYNC_CANCELLATION_RESIDENT_AWAIT_ONCE_V2);
    machine->cancellationAbi = XL6_IR_ASYNC_CANCELLATION_INVALID; reject(&ir, "cancellation delivery transport");
    machine->cancellationAbi = XL6_IR_ASYNC_CANCELLATION_RESIDENT_AWAIT_ONCE_V2;
    machine->completionAbi = XL6_IR_ASYNC_COMPLETION_INVALID; reject(&ir, "completion cleanup barrier");
    machine->completionAbi = XL6_IR_ASYNC_COMPLETION_RETIRE_BEFORE_PUBLISH;
    for (i = machine->firstFrameSlot; i < machine->firstFrameSlot + machine->frameSlotCount; ++i) {
        XL6_IrAsyncMachineFrameSlot* slot = &ir.asyncMachineFrameSlots[i], saved = *slot;
        if (slot->destroyAbi != XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_CELL) continue;
        ++cells;
        slot->destroyAbi = XL6_IR_ASYNC_FRAME_DESTROY_NONE; reject(&ir, NULL); *slot = saved;
        slot->initBit = SIZE_MAX; reject(&ir, NULL); *slot = saved;
    }
    for (i = machine->firstSuspension; i < machine->firstSuspension + machine->suspensionCount; ++i) {
        const XL6_IrAsyncSuspension* state = &ir.asyncSuspensions[i];
        XL6_IrNodeId origin = ir.instructions[state->awaitInstruction].origin; size_t n, a, handoffs = 0;
        for (n = 0; n < origins; ++n) if (awaitOrigins[n] == origin) break;
        if (n == origins) { assert(origins < 3); awaitOrigins[origins++] = origin; }
        for (a = 0; a < state->handoffActionCount; ++a) {
            size_t action = state->firstHandoffAction + a;
            size_t slot = ir.asyncFrameActions[action].frameSlot;
            if (ir.asyncMachineFrameSlots[slot].destroyAbi == XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_CELL) {
                missingAction(&ir, 0, 0, action, i); ++handoffs;
            }
        }
        assert(handoffs == 1);
    }
    assert(cells == 1 && origins == 2); verify(&ir);
    assert(xl6OutputOpenFile(&out, argv[1]) && xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]) && xl6IrCgenEmitHeader(&out, &ir, "CELL_CANCEL_RACE", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    assert(snprintf(binding, sizeof(binding), "%s.bind.h", argv[2]) > 0 && xl6OutputOpenFile(&out, binding));
    xl6OutputPrintf(&out, "#define CELL_CANCEL_RUN xlir_async_start_%u\n", machine->targetSymbol); xl6OutputClose(&out);
    printf("cell cancel race: 2 source awaits, 3 normal/exceptional suspension states, 1 managed cell resident, %llu C/header-gated mutations\n", (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
