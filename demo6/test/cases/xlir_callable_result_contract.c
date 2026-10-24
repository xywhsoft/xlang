#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "xlir_callable_bridge_mutations.h"
static unsigned mutations;
static void check_language_boundary(void)
{
    const char* cases[] = {
        "func consume(byref int value) as int\nreturn value\nend\n"
        "func factory() as func(byref int) as int\nreturn consume\nend\nvar erased = xcall(factory)\n",
        "func consume(out int value)\nvalue = 1\nend\n"
        "func factory() as func(out int)\nreturn consume\nend\nvar erased = xcall(factory)\n",
        "func consume(future<int> value) as int\nreturn 42\nend\n"
        "func factory() as func(future<int>) as int\nreturn consume\nend\nvar erased = xcall(factory)\n"
    };
    size_t i;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        XL6_ParseResult rejected; xl6ParseResultInit(&rejected);
        if (i < 2) {
            /* Parameter modifiers were removed from the language. Rejection
             * is now lexical/grammar, not an xcall-specific semantic rule. */
            assert(!xl6ParseSource(cases[i], "callable_result_invalid", &rejected) && rejected.diags.count);
            xl6ParseResultFree(&rejected); continue;
        }
        if (!xl6ParseSource(cases[i], "callable_result_invalid", &rejected)) {
            size_t d;
            for (d = 0; d < rejected.diags.count; ++d) fprintf(stderr, "negative %lu: %s\n",
                (unsigned long)i, rejected.diags.items[d].message);
            assert(!"language boundary fixture must parse before rejection");
        }
        assert(xl6StdlibExpandModule(rejected.root, &rejected.diags));
        assert(!xl6SemCheckModule(rejected.root, &rejected.diags, "callable_result_invalid") && rejected.diags.count);
        {
            size_t d; int found = 0;
            for (d = 0; d < rejected.diags.count; ++d)
                if (strstr(rejected.diags.items[d].message, "xcall 返回值类型")) found = 1;
            assert(found);
        }
        xl6ParseResultFree(&rejected);
    }
}
static void reject(const XL6_IrModule* ir)
{
    XL6_DiagList diags = {0}; XL6_Output out; char* text;
    assert(!xl6IrVerifyModule(ir, &diags) && diags.count);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitHeader(&out, ir, "BAD_BYVAL", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; size_t i;
    XL6_IrTypeId signature = 0; unsigned bindings = 0; char path[4096];
    assert(argc == 4); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    check_language_boundary();
    if (!xl6ParseFile("test/cases/xlir_callable_result.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "callable_result") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrVerifyModule(&ir, &parse.diags) ||
        !xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    xlirCallableBridgeMutations(&ir, reject);
    assert(xl6IrVerifyModule(&ir, &parse.diags));
    assert(xl6OutputOpenFile(&out, argv[1]));
    assert(xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]));
    assert(xl6IrCgenEmitHeader(&out, &ir, "CALLABLE_RESULT", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    assert(snprintf(path, sizeof(path), "%s.bind.h", argv[2]) > 0);
    assert(xl6OutputOpenFile(&out, path));
    for (i = 0; i < ir.storageCount; ++i) {
        const XL6_IrStorage* storage = &ir.storages[i];
        const XL6_IrNode* declaration = xl6IrNodeGet(&ir, storage->declaration);
        const char* macro = NULL;
        if (storage->kind != XL6_IR_STORAGE_GLOBAL || !declaration || !declaration->text) continue;
        if (!strcmp(declaration->text, "managedRestored")) { macro = "RESULT_RESTORED"; signature = storage->type; }
        else if (!strcmp(declaration->text, "managedNative")) macro = "RESULT_NATIVE";
        else if (!strcmp(declaration->text, "managedErased")) macro = "RESULT_ERASED";
        else if (!strcmp(declaration->text, "answerErased")) macro = "RESULT_ANSWER";
        else if (!strcmp(declaration->text, "manyErased")) macro = "RESULT_MANY";
        if (macro) { xl6OutputPrintf(&out, "#define %s s%u\n", macro, (unsigned)storage->symbol); ++bindings; }
    }
    assert(bindings == 5 && signature);
    {
        const XL6_IrCallableBridge* bridge = xl6IrCallableBridgeGet(&ir, signature);
        const XL6_IrCallableBridgeResult* slots;
        const XL6_IrType* errorType;
        assert(bridge && bridge->reverseSupported && bridge->parameterCount == 0 && bridge->resultCount == 4);
        slots = &ir.callableBridgeResults[bridge->firstResult];
        assert(slots[0].decodeAbi == XL6_IR_CALLABLE_BRIDGE_STRING_COPY &&
            slots[0].encodeAbi == XL6_IR_CALLABLE_BRIDGE_STRING_MOVE);
        assert(slots[1].decodeAbi == XL6_IR_CALLABLE_BRIDGE_XVALUE_RETAIN &&
            slots[1].encodeAbi == XL6_IR_CALLABLE_BRIDGE_XVALUE_MOVE);
        assert(slots[2].decodeAbi == XL6_IR_CALLABLE_BRIDGE_ERROR_REF);
        assert(slots[3].decodeAbi == XL6_IR_CALLABLE_BRIDGE_CLOSURE_COPY &&
            slots[3].encodeAbi == XL6_IR_CALLABLE_BRIDGE_CLOSURE_REF &&
            slots[3].destroyAbi == XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_CLOSURE);
        errorType = xl6IrTypeGet(&ir.types, slots[2].type); assert(errorType);
        xl6OutputPrintf(&out, "#define RESULT_ENTRY xlir_xvalue_closure_entry_t%u\n"
            "#define RESULT_FORWARD xlir_xcall_entry_t%u\n"
            "#define RESULT_TYPE xlir_product_t%u\n"
            "#define RESULT_DROP xlir_product_drop_t%u\n"
            "#define RESULT_ANSWER_ENTRY xlir_xvalue_closure_entry_t%u\n"
            "#define RESULT_ERROR_TYPE 0x%016llXULL\n",
            (unsigned)signature, (unsigned)signature, (unsigned)bridge->resultType,
            (unsigned)bridge->resultType, (unsigned)slots[3].type, (unsigned long long)errorType->typeValueId);
    }
    xl6OutputClose(&out);
    printf("callable result contract: string, xvalue, error and closure result plans verified; %u rejected mutations\n", mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
failed:
    for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%d:%d: %s\n",
        parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 1;
}
