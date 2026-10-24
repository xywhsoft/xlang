#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void reject(XL6_IrModule* ir)
{
    XL6_DiagList diags = {0};
    assert(!xl6IrVerifyModule(ir, &diags));
    xl6DiagFree(&diags);
}

int main(int argc, char** argv)
{
    XL6_ParseResult parsed; XL6_IrModule ir; XL6_Output out;
    char path[4096]; const char* product = NULL; size_t checked = 0;
    assert(argc == 3);
    xl6ParseResultInit(&parsed); xl6IrModuleInit(&ir);
    int ok = xl6ParseFile(argv[1], &parsed) && xl6StdlibExpandModule(parsed.root, &parsed.diags) &&
        xl6SemCheckModule(parsed.root, &parsed.diags, "c_owned_exports") &&
        xl6IrBuildModuleFromAst(&ir, parsed.root, &parsed.diags) && xl6IrLowerModule(&ir, &parsed.diags);
    if (!ok) { xl6DiagPrintText(&parsed.diags, argv[1]); return 1; }
    for (size_t i = 0; i < ir.exportCount; ++i) {
        XL6_IrExport* item = &ir.exports[i];
        const XL6_IrCallTarget* body = xl6IrCallTargetGet(&ir, item->symbol);
        assert(body && item->resultBridgeAbi == XL6_IR_EXPORT_RESULT_C_OWNED_COPY);
        assert(item->returnType != body->returnType);
        XL6_IrTypeId saved = item->returnType;
        item->resultBridgeAbi = XL6_IR_EXPORT_RESULT_DIRECT; reject(&ir);
        item->resultBridgeAbi = XL6_IR_EXPORT_RESULT_C_OWNED_COPY;
        item->returnType = body->returnType; reject(&ir);
        item->returnType = saved;
        if (!strcmp(item->cName, "owned_pair")) product = xl6IrTypeGet(&ir.types, saved)->cName;
        assert(xl6IrVerifyModule(&ir, &parsed.diags)); ++checked;
    }
    assert(checked == 8 && product);
    assert(snprintf(path, sizeof(path), "%s.h", argv[2]) > 0);
    assert(xl6OutputOpenFile(&out, path));
    assert(xl6IrCgenEmitHeader(&out, &ir, "XL6_TEST_C_OWNED_EXPORTS_H", &parsed.diags));
    xl6OutputPrintf(&out, "\ntypedef %s XLTestOwnedProduct;\n", product);
    xl6OutputClose(&out);
    assert(snprintf(path, sizeof(path), "%s.c", argv[2]) > 0);
    assert(xl6OutputOpenFile(&out, path));
    assert(xl6IrCgenEmitC(&out, &ir, &parsed.diags));
    xl6OutputPrintf(&out, "\ntypedef %s XLTestOwnedProduct;\nint main(void) { return xlir_module_init() ? 0 : 3; }\n", product);
    xl6OutputClose(&out);
    assert(snprintf(path, sizeof(path), "%s.json", argv[2]) > 0);
    assert(xl6OutputOpenFile(&out, path)); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    printf("C owner exports: %zu recipes; wrong bridges and public carriers rejected\n", checked);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parsed); return 0;
}
