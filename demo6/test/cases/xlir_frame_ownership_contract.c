#include "xlir_continuation_owner_test.h"
int main(void)
{
    const char* fixtures[] = {"test/cases/xlir_frame_ownership.xl", "test/cases/xlir_cell_continuation_outcome.xl"};
    size_t slots = 0, families[2] = {0}, ownershipMutations = 0; unsigned kinds = 0;
    for (size_t f = 0; f < 2; ++f) {
        XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; char* text;
        xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
        if (!xl6ParseFile(fixtures[f], &parse) || !xl6StdlibExpandModule(parse.root, &parse.diags) ||
            !xl6SemCheckModule(parse.root, &parse.diags, "frame_ownership") ||
            !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) || !xl6IrLowerModule(&ir, &parse.diags)) {
            for (size_t i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%s\n", parse.diags.items[i].message);
            return 1;
        }
        verify(&ir);
        for (size_t i = 0; i < ir.asyncMachineCount; ++i) {
            XL6_IrAsyncMachine* machine = &ir.asyncMachines[i];
            assert(machine->ownershipAbi == XL6_IR_ASYNC_OWNERSHIP_STABLE_PRODUCER_WATCH_V1);
            machine->ownershipAbi = XL6_IR_ASYNC_OWNERSHIP_INVALID;
            reject(&ir, "producer/Watch ownership ABI is unresolved");
            machine->ownershipAbi = (XL6_IrAsyncOwnershipAbi)99;
            reject(&ir, "producer/Watch ownership ABI is unresolved");
            machine->ownershipAbi = XL6_IR_ASYNC_OWNERSHIP_STABLE_PRODUCER_WATCH_V1;
            ownershipMutations += 2;
            assert(machine->cancellationAbi == XL6_IR_ASYNC_CANCELLATION_RESIDENT_AWAIT_ONCE_V2);
            machine->cancellationAbi = XL6_IR_ASYNC_CANCELLATION_INVALID;
            reject(&ir, "cancellation delivery transport");
            machine->cancellationAbi = XL6_IR_ASYNC_CANCELLATION_AWAIT_ONCE;
            reject(&ir, "cancellation delivery transport");
            machine->cancellationAbi = (XL6_IrAsyncCancellationAbi)99;
            reject(&ir, "cancellation delivery transport");
            machine->cancellationAbi = XL6_IR_ASYNC_CANCELLATION_RESIDENT_AWAIT_ONCE_V2;
            ownershipMutations += 3;
        }
        assert(xl6OutputOpenMemory(&out)); xl6IrDumpModuleText(&out, &ir);
        text = xl6OutputTakeMemoryText(&out, NULL); assert(text && strstr(text, "async-ownership 0: 1")); xrtFree(text);
        assert(xl6OutputOpenMemory(&out)); xl6IrDumpModuleJson(&out, &ir);
        text = xl6OutputTakeMemoryText(&out, NULL); assert(text && strstr(text, "\"ownershipAbi\":1}")); xrtFree(text);
        for (size_t family = 0; family < 2; ++family) {
            XL6_IrContinuationFrameSlot* frames = family ? ir.generatorFrameSlots : ir.asyncMachineFrameSlots;
            size_t count = family ? ir.generatorFrameSlotCount : ir.asyncMachineFrameSlotCount;
            families[family] += count;
            for (size_t i = 0; i < count; ++i) {
                XL6_IrContinuationFrameSlot saved = frames[i]; ++slots;
                assert(saved.traceAbi > XL6_IR_FRAME_TRACE_INVALID && saved.traceAbi <= XL6_IR_FRAME_TRACE_GENERATOR);
                kinds |= 1u << saved.traceAbi;
                frames[i].traceAbi = XL6_IR_FRAME_TRACE_INVALID; reject(&ir, "source/lifetime ABI"); frames[i] = saved;
                frames[i].traceAbi = saved.traceAbi == XL6_IR_FRAME_TRACE_NONE ? XL6_IR_FRAME_TRACE_XVALUE : XL6_IR_FRAME_TRACE_NONE;
                reject(&ir, "source/lifetime ABI"); frames[i] = saved;
                frames[i].traceAbi = (XL6_IrContinuationFrameTraceAbi)99; reject(&ir, "source/lifetime ABI"); frames[i] = saved;
            }
        }
        verify(&ir);
        assert(xl6OutputOpenMemory(&out) && xl6IrCgenEmitC(&out, &ir, &parse.diags));
        text = xl6OutputTakeMemoryText(&out, NULL);
        assert(text && strstr(text, "xl6ContinuationFrameRuntimeV3Get()->Bind(frame)") &&
            strstr(text,"xl6ContinuationFrameRuntimeV4Get()->Observe(frame, &input)") && !strstr(text,"cancelObserved") &&
            strstr(text, "XL6_ContinuationFrameLayoutV2") && !strstr(text,"xlir_async_machine_frame_trace_"));
        xrtFree(text); xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    }
    assert(families[0] && families[1] && ownershipMutations && mutations == 3 * slots + ownershipMutations);
    assert((kinds & 126u) == 126u); /* NONE, Value, Future, closure, cell, error */
    printf("Frame ownership IR: %zu slots (%zu async, %zu generator), %zu C/header-gated mutations, kinds=%u\n",
        slots, families[0], families[1], mutations, kinds);
    return 0;
}
