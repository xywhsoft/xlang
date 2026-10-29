/* Distinct from the .xl stem: this is a compiler test, never an auto sidecar. */
#include "src/cgen/xl6_ir_cgen_native_header.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t bindings, mutations;
static int binding(void* context, XL6_SymbolId symbol, const char* name,
    const char* kind, size_t start, size_t end)
{
    (void)context; (void)kind;
    assert(symbol && start < end && strncmp(name, "native_global_", 14) != 0);
    ++bindings; return 1;
}
static char* emit(const XL6_IrModule* ir, int native, int mapped)
{
    XL6_Output out; assert(xl6OutputOpenMemory(&out));
    assert(native ? xl6IrCgenEmitNativeContractHeader(&out, ir, "GLOBAL_CONTRACT", NULL) :
        mapped ? xl6IrCgenEmitHeaderWithBindings(&out, ir, "GLOBAL_CONTRACT", NULL, binding, NULL) :
        xl6IrCgenEmitHeader(&out, ir, "GLOBAL_CONTRACT", NULL));
    char* text = xl6OutputTakeMemoryText(&out, NULL); assert(text);
    xl6OutputDiscard(&out); return text;
}
static void reject(const XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir, NULL));
    for (unsigned mode = 0; mode < 3; ++mode) {
        XL6_Output out; size_t length = SIZE_MAX; assert(xl6OutputOpenMemory(&out));
        assert(!(mode == 0 ? xl6IrCgenEmitC(&out, ir, NULL) :
            mode == 1 ? xl6IrCgenEmitHeader(&out, ir, "BAD_GLOBAL", NULL) :
            xl6IrCgenEmitNativeContractHeader(&out, ir, "BAD_GLOBAL", NULL)));
        char* text = xl6OutputTakeMemoryText(&out, &length);
        assert(!length); xrtFree(text); xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir;
    assert(argc == 4); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile(argv[1], &parse) || !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "native_global_contract") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    assert(ir.externGlobalCount == 3);
    char* public = emit(&ir, 0, 0);
    char* mapped = emit(&ir, 0, 1);
    char* native = emit(&ir, 1, 0);
    assert(bindings && !strcmp(public, mapped) && strstr(public, "xl_native_global_read"));
    assert(!strstr(public, "native_global_counter") && !strstr(public, "native_global_enabled") &&
        !strstr(public, "native_global_size"));
    assert(strstr(native, "extern int64_t native_global_counter;") &&
        strstr(native, "extern bool native_global_enabled;") &&
        strstr(native, "extern size_t native_global_size;"));
    for (size_t i = 0; i < ir.externGlobalCount; ++i) {
        XL6_IrExternGlobal* global = &ir.externGlobals[i];
        XL6_IrExternGlobal saved = *global;
        global->type = 0; reject(&ir); *global = saved;
        global->storageSymbol = 0; reject(&ir); *global = saved;
        global->accessAbi = 0; reject(&ir); *global = saved;
        global->cName = NULL; reject(&ir); *global = saved;
    }
    assert(xl6IrVerifyModule(&ir, NULL));
    const char* texts[] = {public, native};
    for (unsigned i = 0; i < 2; ++i) {
        FILE* output = fopen(argv[i+2], "wb"); assert(output);
        size_t size = strlen(texts[i]); assert(fwrite(texts[i], 1, size, output) == size);
        assert(!fclose(output));
    }
    printf("native global header: 3 exact storage declarations, public bindings isolated, %zu mutations rejected before C/public/native output\n", mutations);
    xrtFree(public); xrtFree(mapped); xrtFree(native);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
failed:
    for (size_t i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%s\n", parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 1;
}
