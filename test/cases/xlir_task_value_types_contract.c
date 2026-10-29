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

/* Language admission is distinct from backend completeness. The two generator
 * signatures are language-valid; they are not counted as lowered or runnable. */
static int check_admission(void)
{
    static const struct { const char* suffix; const char* diagnostic; int lower; } cases[] = {
        { "arg_array", NULL, 1 },
        { "arg_byref", "task.spawn cannot capture byref or out caller storage", 0 },
        { "arg_channel", NULL, 1 },
        { "arg_class", NULL, 1 },
        { "arg_count", "task.spawn argument count does not match func signature", 0 },
        { "arg_count_many", "task.spawn argument count does not match func signature", 0 },
        { "arg_count_none", "task.spawn requires at least a func argument", 0 },
        { "arg_dict", NULL, 1 },
        { "arg_function", NULL, 1 },
        { "arg_future", NULL, 1 },
        { "arg_generator", NULL, 0 },
        { "arg_list", NULL, 1 },
        { "arg_multi_return", "多返回值函数调用不能作为单个实参", 0 },
        { "arg_out", "task.spawn cannot capture byref or out caller storage", 0 },
        { "arg_set", NULL, 1 },
        { "arg_sync", NULL, 1 },
        { "arg_thread", NULL, 1 },
        { "arg_type_mismatch", "task.spawn argument type does not match func signature", 0 },
        { "arg_typed_array", NULL, 1 },
        { "arg_typed_dict", NULL, 1 },
        { "arg_typed_list", NULL, 1 },
        { "arg_typed_set", NULL, 1 },
        { "bare_name", "task.spawn first argument must be a func value with a complete signature", 0 },
        { "dict_vararg", "task.spawn does not support *** func values yet", 0 },
        { "first_arg_non_func", "task.spawn first argument must be a func value with a complete signature", 0 },
        { "multi_return", "task.spawn currently supports only no-return or single-return func values", 0 },
        { "named_argument", "task.spawn does not accept named arguments", 0 },
        { "return_array", NULL, 1 },
        { "return_channel", "不可复制类型不能作为返回值类型", 0 },
        { "return_class", NULL, 1 },
        { "return_dict", NULL, 1 },
        { "return_function", NULL, 1 },
        { "return_future", NULL, 1 },
        { "return_generator", NULL, 0 },
        { "return_list", NULL, 1 },
        { "return_set", NULL, 1 },
        { "return_sync", "不可复制类型不能作为返回值类型", 0 },
        { "return_thread", "不可复制类型不能作为返回值类型", 0 },
        { "return_typed_array", NULL, 1 },
        { "return_typed_dict", NULL, 1 },
        { "return_typed_list", NULL, 1 },
        { "return_typed_set", NULL, 1 },
        { "return_var", NULL, 1 },
    };
    unsigned accepted = 0, rejected = 0, lowered = 0, semanticOnly = 0;
    size_t n;
    for (n = 0; n < sizeof(cases) / sizeof(cases[0]); ++n) {
        XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
        char path[512]; int ok, found = 0; size_t d;
        snprintf(path, sizeof(path), "test/cases/sem_task_spawn_%s.xl", cases[n].suffix);
        xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
        if (!xl6ParseFile(path, &parse) || !xl6StdlibExpandModule(parse.root, &parse.diags)) {
            fprintf(stderr, "task admission parse/expand failed: %s\n", path); abort();
        }
        ok = xl6SemCheckModule(parse.root, &parse.diags, "task_type_admission");
        if (cases[n].diagnostic) {
            for (d = 0; d < parse.diags.count; ++d)
                if (strstr(parse.diags.items[d].message, cases[n].diagnostic)) found = 1;
            if (ok || !found) {
                fprintf(stderr, "task admission missing expected rejection: %s: %s\n", path, cases[n].diagnostic);
                goto failed;
            }
            ++rejected;
        } else {
            if (!ok || parse.diags.count) {
                fprintf(stderr, "language-valid task signature rejected: %s\n", path);
                goto failed;
            }
            ++accepted;
            if (cases[n].lower) {
                if (!xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
                    !xl6IrVerifyModule(&ir, &parse.diags) ||
                    !xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) {
                    fprintf(stderr, "task admission lowering failed: %s\n", path); goto failed;
                }
                assert(xl6OutputOpenMemory(&out));
                if (!xl6IrCgenEmitC(&out, &ir, &parse.diags)) {
                    xl6OutputClose(&out); goto failed;
                }
                xl6OutputClose(&out);
                assert(xl6OutputOpenMemory(&out));
                if (!xl6IrCgenEmitHeader(&out, &ir, "TASK_ADMISSION", &parse.diags)) {
                    xl6OutputClose(&out); goto failed;
                }
                xl6OutputClose(&out); ++lowered;
            } else ++semanticOnly;
        }
        xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); continue;
failed:
        for (d = 0; d < parse.diags.count; ++d)
            fprintf(stderr, "%s:%d:%d: %s\n", path, parse.diags.items[d].line,
                parse.diags.items[d].column, parse.diags.items[d].message);
        xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 1;
    }
    assert(accepted == 28 && rejected == 15 && lowered == 26 && semanticOnly == 2);
    printf("task language admission: %u accepted, %u rejected, %u C/header generations, %u generator signatures semantic-only (backend incomplete)\n",
        accepted, rejected, lowered, semanticOnly);
    return 0;
}

int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; size_t i;
    char path[4096]; int length;
    if (argc == 2 && !strcmp(argv[1], "--admission")) return check_admission();
    assert(argc == 4); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_task_value_types.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "task_value_types") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrVerifyModule(&ir, &parse.diags) ||
        !xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    assert(ir.callbackBoundaryCount == 16 && ir.callbackCaptureCount == 31);
    {
        XL6_IrTypeId box = 0, other = 0, dynamic = 0, scalar = 0, array = 0, dict = 0, resource = 0;
        for (i = 0; i < ir.types.count; ++i) {
            const XL6_IrType* t = xl6IrTypeGet(&ir.types, (XL6_IrTypeId)(i + 1));
            if (!t) continue;
            if (t->name && !strcmp(t->name, "TaskBox")) box = (XL6_IrTypeId)(i + 1);
            if (t->name && !strcmp(t->name, "TaskOther")) other = (XL6_IrTypeId)(i + 1);
            if (t->typeValueId == ir.runtimeTypeMap.fallbackTypeValueId && t->kind == XL6_IR_TYPE_BUILTIN)
                dynamic = (XL6_IrTypeId)(i + 1);
            if (t->cAbi == XL6_IR_C_ABI_I64) scalar = (XL6_IrTypeId)(i + 1);
            if (t->collectionAbi == XL6_IR_COLLECTION_ARRAY) array = (XL6_IrTypeId)(i + 1);
            if (t->collectionAbi == XL6_IR_COLLECTION_DICT) dict = (XL6_IrTypeId)(i + 1);
            if (t->runtimeResourceAbi == XL6_IR_RUNTIME_RESOURCE_SEM) resource = (XL6_IrTypeId)(i + 1);
        }
        assert(box && other && dynamic && scalar && array && dict && resource);
        assert(xl6IrTaskSpawnArgumentCanBind(&ir, box, box));
        assert(xl6IrTaskSpawnArgumentCanBind(&ir, box, dynamic));
        assert(xl6IrTaskSpawnArgumentCanBind(&ir, scalar, dynamic));
        assert(!xl6IrTaskSpawnArgumentCanBind(&ir, dynamic, box));
        assert(!xl6IrTaskSpawnArgumentCanBind(&ir, scalar, box));
        assert(!xl6IrTaskSpawnArgumentCanBind(&ir, box, other));
        assert(!xl6IrTaskSpawnArgumentCanBind(&ir, box, resource));
        assert(!xl6IrTaskSpawnArgumentCanBind(&ir, array, dict));
    }
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
    assert(xl6OutputOpenFile(&out, argv[2])); assert(xl6IrCgenEmitHeader(&out, &ir, "TASK_VALUE_TYPES", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    assert(xl6OutputOpenMemory(&out)); xl6IrDumpModuleText(&out, &ir);
    {
        char* dump = xl6OutputTakeMemoryText(&out, NULL);
        assert(dump && strstr(dump, "callback-boundary 1 instruction=") && strstr(dump, "callback-capture 0 operand="));
        length = snprintf(path, sizeof(path), "%s.txt", argv[3]);
        assert(length > 0 && (size_t)length < sizeof(path) && xl6OutputOpenFile(&out, path));
        xl6OutputPuts(dump, &out); xl6OutputClose(&out); xrtFree(dump);
    }
    printf("task value types contract: %u activations, %u captures, %u rejected mutations\n",
        (unsigned)ir.callbackBoundaryCount, (unsigned)ir.callbackCaptureCount, mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
failed:
    for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%d:%d: %s\n",
        parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
    abort();
}
