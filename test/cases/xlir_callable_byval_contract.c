#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "xlir_callable_bridge_mutations.h"
#include "xlir_internal_owned_params.h"
static unsigned mutations;
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
    if (!xl6ParseFile("test/cases/xlir_callable_byval.xl", &parse)) goto failed;
    assert(xlirTestMarkOwnedNamedParams(parse.root, "bridgeConsume", 7) == 3);
    assert(xlirTestMarkOwnedNamedParams(parse.root, "bridgeRaise", 7) == 3);
    assert(xlirTestMarkOwnedGlobalFunctionTypes(parse.root, NULL) == 15);
    if (!xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "callable_byval") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrVerifyModule(&ir, &parse.diags) ||
        !xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    xlirCallableBridgeMutations(&ir, reject);
    assert(xl6IrVerifyModule(&ir, &parse.diags));
    assert(xl6OutputOpenFile(&out, argv[1]));
    assert(xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]));
    assert(xl6IrCgenEmitHeader(&out, &ir, "CALLABLE_BYVAL", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    assert(snprintf(path, sizeof(path), "%s.bind.h", argv[2]) > 0);
    assert(xl6OutputOpenFile(&out, path));
    for (i = 0; i < ir.storageCount; ++i) {
        const XL6_IrStorage* storage = &ir.storages[i];
        const XL6_IrNode* declaration = xl6IrNodeGet(&ir, storage->declaration);
        const char* macro = NULL;
        if (storage->kind != XL6_IR_STORAGE_GLOBAL || !declaration || !declaration->text) continue;
        if (!strcmp(declaration->text, "restoredGood")) { macro = "BRIDGE_GOOD"; signature = storage->type; }
        else if (!strcmp(declaration->text, "restoredBad")) macro = "BRIDGE_BAD";
        else if (!strcmp(declaration->text, "erasedGood")) macro = "BRIDGE_ERASED_GOOD";
        else if (!strcmp(declaration->text, "erasedBad")) macro = "BRIDGE_ERASED_BAD";
        if (macro) { xl6OutputPrintf(&out, "#define %s s%u\n", macro, (unsigned)storage->symbol); ++bindings; }
    }
    assert(bindings == 4 && signature);
    {
        const XL6_IrCallableBridge* bridge = xl6IrCallableBridgeGet(&ir, signature);
        const XL6_IrCallableBridgeParameter *string, *value, *error;
        const XL6_IrType* errorType;
        assert(bridge && bridge->reverseSupported && bridge->parameterCount == 3);
        string = xl6IrCallableBridgeParameterGet(&ir, signature, 0);
        value = xl6IrCallableBridgeParameterGet(&ir, signature, 1);
        error = xl6IrCallableBridgeParameterGet(&ir, signature, 2);
        assert(string && value && error);
        assert(string->decodeAbi == XL6_IR_CALLABLE_BRIDGE_STRING_COPY &&
            string->inputDestroyAbi == XL6_IR_ASYNC_FRAME_DESTROY_FREE_STRING);
        assert(value->decodeAbi == XL6_IR_CALLABLE_BRIDGE_XVALUE_RETAIN &&
            value->encodeAbi == XL6_IR_CALLABLE_BRIDGE_XVALUE_MOVE &&
            value->decodedDestroyAbi == XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_XVALUE);
        assert(error->decodeAbi == XL6_IR_CALLABLE_BRIDGE_ERROR_REF &&
            error->inputDestroyAbi == XL6_IR_ASYNC_FRAME_DESTROY_FREE_ERROR);
        assert(string->invokeAbi == XL6_IR_CALLBACK_INVOKE_TAKE &&
            value->invokeAbi == XL6_IR_CALLBACK_INVOKE_TAKE && error->invokeAbi == XL6_IR_CALLBACK_INVOKE_TAKE);
        errorType = xl6IrTypeGet(&ir.types, error->type); assert(errorType && errorType->typeValueId);
        xl6OutputPrintf(&out, "#define BRIDGE_ENTRY xlir_xvalue_closure_entry_t%u\n#define BRIDGE_ERROR_TYPE 0x%016llXULL\n",
            (unsigned)signature, (unsigned long long)errorType->typeValueId);
    }
    xl6OutputClose(&out);
    printf("callable BYVAL contract: string, xvalue and error bidirectional ownership plans verified; %u rejected mutations\n", mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
failed:
    for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%d:%d: %s\n",
        parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 1;
}
