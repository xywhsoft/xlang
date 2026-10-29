#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>

static void reject(XL6_IrModule* ir)
{
    XL6_DiagList diags = {0};
    XL6_Output out;
    char* text;
    assert(!xl6IrVerifyModule(ir, &diags) && diags.count != 0);
    assert(xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL);
    assert(text == NULL || text[0] == '\0'); xrtFree(text);
    assert(xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitHeader(&out, ir, "REJECTED_EXPORT", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL);
    assert(text == NULL || text[0] == '\0'); xrtFree(text);
    xl6DiagFree(&diags);
}

int main(int argc, char** argv)
{
    XL6_ParseResult parse;
    XL6_IrModule ir;
    XL6_Output out;
    char* text;
    size_t i;
    int mutations = 0;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_export_transaction.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "export_contract") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) {
        for (i = 0; i < parse.diags.count; ++i) puts(parse.diags.items[i].message);
        return 1;
    }
    assert(ir.exportCount == 13);
    /* Optional fresh-source integration artifacts keep debugging independent
     * from the last compiler DLL. The normal gate still regenerates with the
     * authoritative DLL before all runtime lanes. */
    if (argc == 3) {
        assert(xl6OutputOpenFile(&out, argv[1]));
        assert(xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
        assert(xl6OutputOpenFile(&out, argv[2]));
        assert(xl6IrCgenEmitHeader(&out, &ir, "XLIR_EXPORT_TRANSACTION_TEST_H", &parse.diags));
        xl6OutputClose(&out);
    } else assert(argc == 1);
    assert(xl6OutputOpenMemory(&out));
    assert(xl6IrCgenEmitC(&out, &ir, &parse.diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(text != NULL && text[0]); xrtFree(text);
    for (i = 0; i < ir.exportCount; ++i) {
        XL6_IrExport* entry = &ir.exports[i];
        XL6_IrErrorAbi body = entry->bodyErrorAbi;
        assert(entry->executionAbi == XL6_IR_EXPORT_EXECUTION_RESIDENT_CODE_SCOPE_V1);
        entry->executionAbi = XL6_IR_EXPORT_EXECUTION_INVALID; reject(&ir); ++mutations;
        entry->executionAbi = (XL6_IrExportExecutionAbi)2; reject(&ir); ++mutations;
        entry->executionAbi = XL6_IR_EXPORT_EXECUTION_RESIDENT_CODE_SCOPE_V1;
        assert(entry->errorAbi == XL6_IR_EXPORT_ERROR_SHARED_SCOPE_TRANSACTION);
        entry->errorAbi = XL6_IR_EXPORT_ERROR_INVALID; reject(&ir); ++mutations;
        entry->errorAbi = XL6_IR_EXPORT_ERROR_SHARED_SCOPE_TRANSACTION;
        entry->bodyErrorAbi = body == XL6_IR_ERROR_ABI_NO_RAISE
            ? XL6_IR_ERROR_ABI_MAY_RAISE : XL6_IR_ERROR_ABI_NO_RAISE;
        reject(&ir); ++mutations; entry->bodyErrorAbi = body;
    }
    assert(xl6IrVerifyModule(&ir, NULL));
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    printf("export error contract: %d missing/mismatched transaction mutations rejected before C or header output\n", mutations);
    return 0;
}
