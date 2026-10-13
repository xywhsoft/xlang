#include "xlir_continuation_owner_test.h"
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    size_t i, p, workers = 0, retained = 0; char binding[1024];
    assert(argc == 4); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_async_worker_completion.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "worker_completion") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) || !xl6IrLowerModule(&ir, &parse.diags)) {
        for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%s\n", parse.diags.items[i].message);
        return 2;
    }
    verify(&ir); assert(ir.asyncMachineCount == 0 && ir.asyncSuspensionCount == 0);
    for (i = 0; i < ir.callTargetCount; ++i) {
        XL6_IrCallTarget* target = &ir.callTargets[i];
        XL6_IrAsyncCompletionAbi completion = target->asyncCompletionAbi;
        if (target->kind != XL6_IR_CALL_TARGET_INTERNAL_ASYNC) {
            assert(completion == XL6_IR_ASYNC_COMPLETION_INVALID);
            target->asyncCompletionAbi = XL6_IR_ASYNC_COMPLETION_RETIRE_BEFORE_PUBLISH;
            reject(&ir, "completion cleanup barrier"); target->asyncCompletionAbi = completion;
            continue;
        }
        ++workers;
        assert(completion == XL6_IR_ASYNC_COMPLETION_RETIRE_BEFORE_PUBLISH &&
            target->asyncDispatch == XL6_IR_ASYNC_DISPATCH_XRT_MODULE_TASK_POOL && target->asyncMachine == SIZE_MAX);
        assert(target->parameterCount == 3 && target->workerErrorAbi == XL6_IR_ERROR_ABI_MAY_RAISE);
        target->asyncCompletionAbi = XL6_IR_ASYNC_COMPLETION_INVALID; reject(&ir, "completion cleanup barrier");
        target->asyncCompletionAbi = (XL6_IrAsyncCompletionAbi)99; reject(&ir, "completion cleanup barrier");
        target->asyncCompletionAbi = completion;
        for (p = 0; p < 2; ++p) {
            XL6_IrAsyncCaptureAbi capture = target->asyncCaptureAbis[p];
            /* No source parameter modifiers remain. Each ordinary class
             * argument independently retains the shared object identity. */
            assert(capture == XL6_IR_ASYNC_CAPTURE_RETAIN_XVALUE);++retained;
            target->asyncCaptureAbis[p] = XL6_IR_ASYNC_CAPTURE_COPY_VALUE;
            reject(&ir, NULL); target->asyncCaptureAbis[p] = capture;
        }
    }
    assert(workers == 6 && retained == 12); verify(&ir);
    assert(xl6OutputOpenFile(&out, argv[1]) && xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]) && xl6IrCgenEmitHeader(&out, &ir, "WORKER_COMPLETION", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    assert(snprintf(binding, sizeof(binding), "%s.bind.h", argv[2]) > 0 && xl6OutputOpenFile(&out, binding));
    workers = 0;
    for (i = 0; i < ir.callTargetCount; ++i) if (ir.callTargets[i].kind == XL6_IR_CALL_TARGET_INTERNAL_ASYNC)
        xl6OutputPrintf(&out, "#define WORKER_COMPLETION_%lu xlir_async_start_%u\n", (unsigned long)workers++, ir.callTargets[i].symbol);
    xl6OutputClose(&out);
    printf("async worker completion: 6 task workers, 5 result ABIs, no suspension, %llu C/header-gated mutations\n", (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
