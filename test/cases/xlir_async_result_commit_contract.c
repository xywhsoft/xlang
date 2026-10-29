#include "xlir_continuation_owner_test.h"
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    const char* names[] = {"resultCommitInt", "resultCommitString", "resultCommitObject", "resultCommitClosure", "resultCommitError"};
    XL6_IrAsyncResultAbi abis[] = {XL6_IR_ASYNC_RESULT_COPY_I64, XL6_IR_ASYNC_RESULT_OWNED_STRING,
        XL6_IR_ASYNC_RESULT_RETAINED_XVALUE, XL6_IR_ASYNC_RESULT_RETAINED_CLOSURE,
        XL6_IR_ASYNC_RESULT_RETAINED_ERROR};
    XL6_SymbolId targets[5] = {0}; size_t i, n; char binding[1024];
    assert(argc == 4); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_async_result_commit.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "result_commit") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) || !xl6IrLowerModule(&ir, &parse.diags)) {
        for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%s\n", parse.diags.items[i].message);
        return 2;
    }
    verify(&ir); assert(ir.asyncMachineCount == 5);
    for (i = 0; i < ir.asyncMachineCount; ++i) {
        XL6_IrAsyncMachine* machine = &ir.asyncMachines[i];
        const XL6_IrFunction* fn = &ir.functions[machine->function];
        const XL6_IrNode* declaration = xl6IrNodeGet(&ir, fn->declaration);
        const XL6_IrNode* name = declaration && declaration->childCount ? xl6IrNodeGet(&ir, ir.children[declaration->firstChild]) : NULL;
        XL6_IrCallTarget* target = NULL;
        for (n = 0; n < ir.callTargetCount; ++n) if (ir.callTargets[n].symbol == machine->targetSymbol) target = &ir.callTargets[n];
        assert(target && name && name->text);
        for (n = 0; n < 5; ++n) if (!strcmp(name->text, names[n])) break;
        assert(n < 5 && !targets[n] && target->asyncResultAbi == abis[n]); targets[n] = target->symbol;
        assert(machine->resultCommitAbi == XL6_IR_ASYNC_RESULT_COMMIT_OWNED_TRANSACTION);
        machine->resultCommitAbi = XL6_IR_ASYNC_RESULT_COMMIT_INVALID; reject(&ir, "result commit transaction");
        machine->resultCommitAbi = (XL6_IrAsyncResultCommitAbi)99; reject(&ir, "result commit transaction");
        machine->resultCommitAbi = XL6_IR_ASYNC_RESULT_COMMIT_OWNED_TRANSACTION;
        target->asyncResultAbi = XL6_IR_ASYNC_RESULT_INVALID; reject(&ir, NULL); target->asyncResultAbi = abis[n];
    }
    verify(&ir);
    assert(xl6OutputOpenFile(&out, argv[1]));
    assert(xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]));
    assert(xl6IrCgenEmitHeader(&out, &ir, "ASYNC_RESULT_COMMIT", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    assert(snprintf(binding, sizeof(binding), "%s.bind.h", argv[2]) > 0 && xl6OutputOpenFile(&out, binding));
    for (n = 0; n < 5; ++n) { assert(targets[n]); xl6OutputPrintf(&out, "#define RESULT_COMMIT_%lu xlir_async_start_%u\n", (unsigned long)n, targets[n]); }
    xl6OutputClose(&out);
    /* The source language deliberately prohibits directly declaring an async
     * future/task return. Keep that existing boundary; nested Future task
     * payloads need their separate worker contract, not a source workaround. */
    {
        XL6_ParseResult negative; int found = 0;
        xl6ParseResultInit(&negative);
        assert(xl6ParseFile("test/cases/sem_async_return_future.xl", &negative));
        assert(!xl6SemCheckModule(negative.root, &negative.diags, "nested_async_return"));
        for (i = 0; i < negative.diags.count; ++i)
            if (strstr(negative.diags.items[i].message, "future/task")) found = 1;
        assert(found); xl6ParseResultFree(&negative);
    }
    printf("async result commit: 5 source-supported payload ABIs, %llu C/header-gated mutations; explicit async Future return rejection preserved\n", (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
