#include "xlir_continuation_owner_test.h"
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; size_t i;
    XL6_IrAsyncMachine* machine; char binding[1024]; size_t residentOnly = 0;
    assert(argc == 4); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_async_cancel_watch.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "cancel_watch") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) || !xl6IrLowerModule(&ir, &parse.diags)) {
        for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%d:%d %s\n", parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
        return 2;
    }
    verify(&ir); assert(ir.asyncMachineCount == 1); machine = &ir.asyncMachines[0];
    assert(machine->transportAbi == XL6_IR_CALLBACK_TRANSPORT_FUTURE_WATCH);
    assert(machine->cancellationAbi == XL6_IR_ASYNC_CANCELLATION_RESIDENT_AWAIT_ONCE_V2 && machine->suspensionCount >= 2);
    machine->transportAbi = XL6_IR_CALLBACK_TRANSPORT_INVALID; reject(&ir, "cancellation delivery transport");
    machine->transportAbi = XL6_IR_CALLBACK_TRANSPORT_TASK_POOL; reject(&ir, "cancellation delivery transport");
    machine->transportAbi = (XL6_IrCallbackTransportAbi)99; reject(&ir, "cancellation delivery transport");
    machine->transportAbi = XL6_IR_CALLBACK_TRANSPORT_FUTURE_WATCH;
    machine->cancellationAbi = XL6_IR_ASYNC_CANCELLATION_INVALID; reject(&ir, "cancellation delivery transport");
    machine->cancellationAbi = XL6_IR_ASYNC_CANCELLATION_AWAIT_ONCE; reject(&ir, "cancellation delivery transport");
    machine->cancellationAbi = (XL6_IrAsyncCancellationAbi)99; reject(&ir, "cancellation delivery transport");
    machine->cancellationAbi = XL6_IR_ASYNC_CANCELLATION_RESIDENT_AWAIT_ONCE_V2;
    machine->completionAbi = XL6_IR_ASYNC_COMPLETION_INVALID; reject(&ir, "completion cleanup barrier");
    machine->completionAbi = XL6_IR_ASYNC_COMPLETION_RETIRE_BEFORE_PUBLISH;
    for (i = machine->firstSuspension; i < machine->firstSuspension + machine->suspensionCount; ++i) {
        const XL6_IrAsyncSuspension* state = &ir.asyncSuspensions[i]; size_t f, a, v;
        for (f = machine->firstFrameSlot; f < machine->firstFrameSlot + machine->frameSlotCount; ++f) {
            if (ir.asyncMachineFrameSlots[f].initBit == SIZE_MAX) continue;
            for (a = 0; a < state->handoffActionCount; ++a)
                if (ir.asyncFrameActions[state->firstHandoffAction + a].frameSlot == f) break;
            assert(a < state->handoffActionCount);
            for (v = 0; v < state->liveSlotCount; ++v)
                if (ir.asyncLiveSlots[state->firstLiveSlot + v].frameSlot == f) break;
            if (v == state->liveSlotCount) {
                ++residentOnly;
                missingAction(&ir, 0, 0, state->firstHandoffAction + a, i);
            }
        }
    }
    assert(residentOnly > 0);
    verify(&ir);
    assert(xl6OutputOpenFile(&out, argv[1]) && xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]) && xl6IrCgenEmitHeader(&out, &ir, "CANCEL_WATCH", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    assert(snprintf(binding, sizeof(binding), "%s.bind.h", argv[2]) > 0 && xl6OutputOpenFile(&out, binding));
    xl6OutputPrintf(&out, "#define CANCEL_WATCH_RUN xlir_async_start_%u\n", machine->targetSymbol); xl6OutputClose(&out);
    printf("async cancel watch: canonical watch transport, once-only cancellation, %llu unread owner handoffs, %llu C/header-gated mutations\n", (unsigned long long)residentOnly, (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
