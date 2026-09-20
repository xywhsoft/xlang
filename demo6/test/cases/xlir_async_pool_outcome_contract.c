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
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitHeader(&out, ir, "BAD_TERMINAL", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; size_t i;
    unsigned asyncTargets = 0, nonAsyncTargets = 0, poolTargets = 0, readyTargets = 0;
    char path[4096]; int length; unsigned bindings = 0;
    assert(argc == 4); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_async_pool_outcome.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "pool_outcome") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrVerifyModule(&ir, &parse.diags) ||
        !xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    for (i = 0; i < ir.callTargetCount; ++i) {
        XL6_IrCallTarget* target = &ir.callTargets[i];
        if (target->kind == XL6_IR_CALL_TARGET_INTERNAL_ASYNC) {
            assert(target->futureOutcomeAbi == XL6_IR_FUTURE_OUTCOME_XRT_ERROR_KIND);
            target->futureOutcomeAbi = XL6_IR_FUTURE_OUTCOME_INVALID; reject(&ir);
            target->futureOutcomeAbi = (XL6_IrFutureOutcomeAbi)99; reject(&ir);
            target->futureOutcomeAbi = XL6_IR_FUTURE_OUTCOME_XRT_ERROR_KIND;
            assert(target->asyncMachine == SIZE_MAX);
            if (target->asyncDispatch == XL6_IR_ASYNC_DISPATCH_XRT_MODULE_TASK_POOL) ++poolTargets;
            else if (target->asyncDispatch == XL6_IR_ASYNC_DISPATCH_XRT_READY_FUTURE) ++readyTargets;
            ++asyncTargets;
        } else {
            assert(target->futureOutcomeAbi == XL6_IR_FUTURE_OUTCOME_INVALID);
            target->futureOutcomeAbi = XL6_IR_FUTURE_OUTCOME_XRT_ERROR_KIND; reject(&ir);
            target->futureOutcomeAbi = XL6_IR_FUTURE_OUTCOME_INVALID; ++nonAsyncTargets;
        }
    }
    assert(asyncTargets == 4 && poolTargets == 3 && readyTargets == 1 && nonAsyncTargets >= 3);
    assert(ir.asyncMachineCount == 0 && xl6IrVerifyModule(&ir, &parse.diags));
    assert(xl6OutputOpenFile(&out, argv[1])); assert(xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2])); assert(xl6IrCgenEmitHeader(&out, &ir, "POOL_OUTCOME", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    assert(xl6OutputOpenMemory(&out)); xl6IrDumpModuleText(&out, &ir);
    {
        char* dump = xl6OutputTakeMemoryText(&out, NULL);
        assert(dump && strstr(dump, "dispatch=1 future-outcome=1") &&
            strstr(dump, "dispatch=2 future-outcome=1") && strstr(dump, "future-outcome=0"));
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
        if (target->kind != XL6_IR_CALL_TARGET_INTERNAL_ASYNC || !name || !name->text) continue;
        if (!strcmp(name->text, "poolScalar")) macro = "POOL_SCALAR";
        else if (!strcmp(name->text, "poolText")) macro = "POOL_TEXT";
        else if (!strcmp(name->text, "poolOwner")) macro = "POOL_OWNER";
        else if (!strcmp(name->text, "poolReady")) macro = "POOL_READY";
        if (macro) { xl6OutputPrintf(&out, "#define %s xlir_async_start_%u\n", macro, (unsigned)target->symbol); ++bindings; }
    }
    assert(bindings == 4); xl6OutputClose(&out);
    printf("pool outcome contract: %u pool, %u ready, %u non-async targets, %u rejected mutations\n", poolTargets, readyTargets, nonAsyncTargets, mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
failed:
    for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%d:%d: %s\n",
        parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
    abort();
}
