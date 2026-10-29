#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned mutations;
static void reject(const XL6_IrModule* ir)
{
    XL6_DiagList diags = {0}; XL6_Output out; char* text;
    assert(!xl6IrVerifyModule(ir, &diags) && diags.count);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitHeader(&out, ir, "BAD_CALLBACK", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}
#define BAD_FIELD(record, field, invalid) do { \
    (record)->field = (invalid); reject(&ir); *(record) = saved; \
} while (0)
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; size_t i;
    char path[4096]; int length; unsigned bindings = 0;
    int futureOnly = argc == 5 && !strcmp(argv[4], "--future-only");
    assert(argc == 4 || futureOnly); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile(futureOnly ? "test/cases/xlir_callback_boundary_future_only.xl" : "test/cases/xlir_callback_boundary.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "callback_boundary") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrVerifyModule(&ir, &parse.diags) ||
        !xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    assert(ir.callbackBoundaryCount == (futureOnly ? 1u : 5u) && ir.callbackCaptureCount == (futureOnly ? 2u : 12u));
    for (i = 0; i < ir.callbackBoundaryCount; ++i) {
        XL6_IrCallbackBoundary* b = &ir.callbackBoundaries[i], saved = *b;
        XL6_IrInstruction* op = &ir.instructions[b->instruction];
        size_t identity = op->callbackBoundary;
        BAD_FIELD(b, instruction, SIZE_MAX);
        BAD_FIELD(b, firstCapture, SIZE_MAX);
        BAD_FIELD(b, captureCount, 0);
        BAD_FIELD(b, transportAbi, XL6_IR_CALLBACK_TRANSPORT_INVALID);
        BAD_FIELD(b, transportAbi, saved.transportAbi == XL6_IR_CALLBACK_TRANSPORT_FUTURE_WATCH
            ? XL6_IR_CALLBACK_TRANSPORT_TASK_POOL : XL6_IR_CALLBACK_TRANSPORT_FUTURE_WATCH);
        BAD_FIELD(b, cleanupAbi, XL6_IR_CALLBACK_CLEANUP_INVALID);
        BAD_FIELD(b, cleanupAbi, (XL6_IrCallbackCleanupAbi)99);
        BAD_FIELD(b, errorAbi, XL6_IR_CALLBACK_ERROR_INVALID);
        BAD_FIELD(b, errorAbi, saved.errorAbi == XL6_IR_CALLBACK_ERROR_PRESERVE
            ? XL6_IR_CALLBACK_ERROR_ASYNC_CAUSE : XL6_IR_CALLBACK_ERROR_PRESERVE);
        BAD_FIELD(b, forwardAbi, XL6_IR_CALLBACK_FORWARD_INVALID);
        BAD_FIELD(b, forwardAbi, saved.forwardAbi == XL6_IR_CALLBACK_FORWARD_NONE
            ? XL6_IR_CALLBACK_FORWARD_SOURCE_OWNER : XL6_IR_CALLBACK_FORWARD_NONE);
        op->callbackBoundary = 0; reject(&ir);
        op->callbackBoundary = ir.callbackBoundaryCount + 1; reject(&ir);
        op->callbackBoundary = identity;
    }
    for (i = 0; i < ir.callbackCaptureCount; ++i) {
        XL6_IrCallbackCapture* c = &ir.callbackCaptures[i], saved = *c;
        BAD_FIELD(c, operand, SIZE_MAX);
        BAD_FIELD(c, value, XL6_IR_VALUE_INVALID);
        BAD_FIELD(c, type, XL6_IR_TYPE_INVALID);
        BAD_FIELD(c, role, XL6_IR_CALLBACK_CAPTURE_INVALID);
        BAD_FIELD(c, invokeAbi, XL6_IR_CALLBACK_INVOKE_INVALID);
        BAD_FIELD(c, invokeAbi, saved.invokeAbi == XL6_IR_CALLBACK_INVOKE_TAKE
            ? XL6_IR_CALLBACK_INVOKE_BORROW : XL6_IR_CALLBACK_INVOKE_TAKE);
        BAD_FIELD(c, transferAbi, XL6_IR_ASYNC_FRAME_TRANSFER_INVALID);
        BAD_FIELD(c, transferAbi, XL6_IR_ASYNC_FRAME_TRANSFER_MOVE_OWNED);
        BAD_FIELD(c, destroyAbi, XL6_IR_ASYNC_FRAME_DESTROY_INVALID);
        BAD_FIELD(c, destroyAbi, saved.destroyAbi == XL6_IR_ASYNC_FRAME_DESTROY_NONE
            ? XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_CLOSURE : XL6_IR_ASYNC_FRAME_DESTROY_NONE);
    }
    for (i = 0; i < ir.instructionCount; ++i) if (!ir.instructions[i].callbackBoundary) {
        ir.instructions[i].callbackBoundary = 1; reject(&ir); ir.instructions[i].callbackBoundary = 0; break;
    }
    {
        size_t count = ir.callbackBoundaryCount;
        ir.callbackBoundaryCount = 0; reject(&ir); ir.callbackBoundaryCount = count;
        count = ir.callbackCaptureCount;
        --ir.callbackCaptureCount; reject(&ir); ir.callbackCaptureCount = count;
    }
    assert(xl6IrVerifyModule(&ir, &parse.diags));
    assert(xl6OutputOpenFile(&out, argv[1])); assert(xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2])); assert(xl6IrCgenEmitHeader(&out, &ir, "CALLBACK_BOUNDARY", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    assert(xl6OutputOpenMemory(&out)); xl6IrDumpModuleText(&out, &ir);
    {
        char* dump = xl6OutputTakeMemoryText(&out, NULL);
        assert(dump && strstr(dump, "callback-boundary 1 instruction=") && strstr(dump, "callback-capture 0 operand="));
        length = snprintf(path, sizeof(path), "%s.txt", argv[3]);
        assert(length > 0 && (size_t)length < sizeof(path) && xl6OutputOpenFile(&out, path));
        xl6OutputPuts(dump, &out); xl6OutputClose(&out); xrtFree(dump);
    }
    length = snprintf(path, sizeof(path), "%s.bind.h", argv[2]);
    assert(length > 0 && (size_t)length < sizeof(path) && xl6OutputOpenFile(&out, path));
    for (i = 0; i < ir.callTargetCount; ++i) {
        const XL6_IrCallTarget* target = &ir.callTargets[i];
        const XL6_IrNode* declaration = xl6IrNodeGet(&ir, target->declaration);
        const XL6_IrNode* name = declaration && declaration->childCount
            ? xl6IrNodeGet(&ir, ir.children[declaration->firstChild]) : NULL;
        const char* macro = NULL;
        if (target->kind != XL6_IR_CALL_TARGET_INTERNAL || !name || !name->text) continue;
        if (!strcmp(name->text, "callbackThen")) macro = "CALLBACK_THEN";
        else if (!strcmp(name->text, "callbackCatch")) macro = "CALLBACK_CATCH";
        else if (!strcmp(name->text, "callbackFinal")) macro = "CALLBACK_FINALLY";
        else if (!strcmp(name->text, "callbackTask")) macro = "CALLBACK_TASK";
        else if (!strcmp(name->text, "callbackOwned")) macro = "CALLBACK_OWNED";
        if (macro) { xl6OutputPrintf(&out, "#define %s xlir_fn_%u\n", macro, (unsigned)target->symbol); ++bindings; }
    }
    assert(bindings == (futureOnly ? 1u : 5u)); xl6OutputClose(&out);
    printf("callback boundary contract: %u activations, %u captures, %u rejected mutations\n",
        (unsigned)ir.callbackBoundaryCount, (unsigned)ir.callbackCaptureCount, mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
failed:
    for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%d:%d: %s\n",
        parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
    abort();
}
