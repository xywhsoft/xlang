#include "xlir_continuation_owner_test.h"
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; size_t i, n;
    XL6_IrAsyncMachine* machine; char binding[1024];
    assert(argc == 4); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_async_frame_completion.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "frame_completion") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) || !xl6IrLowerModule(&ir, &parse.diags)) {
        for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%s\n", parse.diags.items[i].message);
        return 2;
    }
    verify(&ir); assert(ir.asyncMachineCount == 1); machine = &ir.asyncMachines[0];
    assert(machine->completionAbi == XL6_IR_ASYNC_COMPLETION_RETIRE_BEFORE_PUBLISH);
    assert(machine->destroyActionCount >= 2);
    machine->completionAbi = XL6_IR_ASYNC_COMPLETION_INVALID; reject(&ir, "completion cleanup barrier");
    machine->completionAbi = (XL6_IrAsyncCompletionAbi)99; reject(&ir, "completion cleanup barrier");
    machine->completionAbi = XL6_IR_ASYNC_COMPLETION_RETIRE_BEFORE_PUBLISH;
    for (i = 0; i < machine->destroyActionCount; ++i) {
        XL6_IrAsyncFrameAction* action = &ir.asyncFrameActions[machine->firstDestroyAction + i];
        XL6_IrAsyncFrameAction saved = *action;
        assert(action->kind == XL6_IR_ASYNC_FRAME_ACTION_DESTROY_IF_INITIALIZED);
        action->kind = XL6_IR_ASYNC_FRAME_ACTION_INVALID; reject(&ir, NULL); *action = saved;
        action->frameSlot = ir.asyncMachineFrameSlotCount; reject(&ir, NULL); *action = saved;
    }
    n = machine->destroyActionCount; --machine->destroyActionCount; reject(&ir, NULL); machine->destroyActionCount = n;
    verify(&ir);
    assert(xl6OutputOpenFile(&out, argv[1]) && xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]) && xl6IrCgenEmitHeader(&out, &ir, "FRAME_COMPLETION", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    assert(snprintf(binding, sizeof(binding), "%s.bind.h", argv[2]) > 0 && xl6OutputOpenFile(&out, binding));
    xl6OutputPrintf(&out, "#define FRAME_COMPLETION_RUN xlir_async_start_%u\n", machine->targetSymbol); xl6OutputClose(&out);
    printf("async frame completion: explicit retirement barrier, %llu C/header-gated mutations\n", (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
