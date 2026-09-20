#include "xlir_continuation_owner_test.h"
int main(void)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; char* text;
    size_t i, targets = 0, results = 0, sources = 0, captures = 0;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/xlir_future_ownership.xl", &parse) &&
        xl6StdlibExpandModule(parse.root, &parse.diags) &&
        xl6SemCheckModule(parse.root, &parse.diags, "future_ownership") &&
        xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) &&
        xl6IrLowerModule(&ir, &parse.diags));
    verify(&ir);
    for (i = 0; i < ir.callTargetCount; ++i) {
        XL6_IrCallTarget* target = &ir.callTargets[i];
        XL6_IrAsyncResultAbi saved = target->asyncResultAbi;
        if (saved == XL6_IR_ASYNC_RESULT_INVALID || saved == XL6_IR_ASYNC_RESULT_VOID) continue;
        ++targets;
        target->asyncResultAbi = XL6_IR_ASYNC_RESULT_INVALID; reject(&ir, NULL);
        target->asyncResultAbi = (XL6_IrAsyncResultAbi)99; reject(&ir, NULL);
        target->asyncResultAbi = saved == XL6_IR_ASYNC_RESULT_RETAINED_CLOSURE ?
            XL6_IR_ASYNC_RESULT_RETAINED_XVALUE : XL6_IR_ASYNC_RESULT_RETAINED_CLOSURE; reject(&ir, NULL);
        target->asyncResultAbi = saved;
    }
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i];
        XL6_IrInstruction saved = *instruction;
        if (saved.asyncResultAbi != XL6_IR_ASYNC_RESULT_INVALID && saved.asyncResultAbi != XL6_IR_ASYNC_RESULT_VOID) {
            ++results;
            instruction->asyncResultAbi = XL6_IR_ASYNC_RESULT_INVALID; reject(&ir, NULL);
            instruction->asyncResultAbi = (XL6_IrAsyncResultAbi)99; reject(&ir, NULL);
            instruction->asyncResultAbi = saved.asyncResultAbi == XL6_IR_ASYNC_RESULT_RETAINED_CLOSURE ?
                XL6_IR_ASYNC_RESULT_RETAINED_XVALUE : XL6_IR_ASYNC_RESULT_RETAINED_CLOSURE; reject(&ir, NULL);
            *instruction = saved;
        }
        if (saved.sourceAsyncResultAbi != XL6_IR_ASYNC_RESULT_INVALID) {
            ++sources;
            instruction->sourceAsyncResultAbi = XL6_IR_ASYNC_RESULT_INVALID; reject(&ir, NULL);
            instruction->sourceAsyncResultAbi = XL6_IR_ASYNC_RESULT_RETAINED_XVALUE; reject(&ir, NULL);
            *instruction = saved;
        }
    }
    for (i = 0; i < ir.closureCaptureCount; ++i) {
        XL6_IrClosureCapture* capture = &ir.closureCaptures[i];
        XL6_IrClosureCapture saved = *capture;
        if (capture->abi != XL6_IR_CLOSURE_CAPTURE_RETAIN_FUTURE) continue;
        ++captures;
        capture->abi = XL6_IR_CLOSURE_CAPTURE_RETAIN_XVALUE; reject(&ir, NULL); *capture = saved;
        capture->abi = XL6_IR_CLOSURE_CAPTURE_COPY_VALUE; reject(&ir, NULL); *capture = saved;
    }
    assert(targets && results && sources && captures); verify(&ir);
    assert(xl6OutputOpenMemory(&out) && xl6IrCgenEmitC(&out, &ir, &parse.diags));
    text = xl6OutputTakeMemoryText(&out, NULL);
    assert(text && strstr(text, "xl6TaskSubmitPayloadV1") && strstr(text, "xl6PayloadResolveOwned") &&
        strstr(text, "xlir_async_pool_submit_language(frame, NULL, XL6_FUTURE_PAYLOAD_CLOSURE)") &&
        !strstr(text, "xlir_async_closure_trace") && !strstr(text, "xrtPromiseResolveOwnedTraced") &&
        strstr(text, "xrtFutureOwnership"));
    xrtFree(text);
    printf("Future ownership IR: %zu targets, %zu results, %zu sources, %zu captures, %zu C/header-gated mutations\n",
        targets, results, sources, captures, mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
