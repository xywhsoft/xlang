/* Independently verify presence/pruning and reject forged binding metadata
 * BEFORE C/header output. The appended getter is test-only, not a language API. */
#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static size_t mutations;
static void reject(const XL6_IrModule* module)
{
    XL6_DiagList diags = {0}; XL6_Output out; char* text;
    assert(!xl6IrVerifyModule(module, &diags) && diags.count);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitC(&out, module, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitHeader(&out, module, "BAD_READ_BIND", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}
int main(int argc, char** argv)
{
    assert(argc == 5);
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    bool needed = strstr(argv[1], "pruned") == NULL;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile(argv[1], &parse)); assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    if (!xl6SemCheckModule(parse.root, &parse.diags, "restore_read_binding")) goto failed;
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    assert(ir.abi.readBoundaryAbi == XL6_IR_MODULE_READ_BOUNDARY_NONE);
    ir.abi.readBoundaryAbi = XL6_IR_MODULE_READ_BOUNDARY_RESIDENT_HOST_BIND_V1; reject(&ir);
    ir.abi.readBoundaryAbi = XL6_IR_MODULE_READ_BOUNDARY_NONE;
    if (!xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    assert(needed == (ir.persistenceRestoreRootCount != 0));
    XL6_IrModuleReadBoundaryAbi actual = ir.abi.readBoundaryAbi;
    assert(actual == (needed ? XL6_IR_MODULE_READ_BOUNDARY_RESIDENT_HOST_BIND_V1 : XL6_IR_MODULE_READ_BOUNDARY_NONE));
    ir.abi.readBoundaryAbi = needed ? XL6_IR_MODULE_READ_BOUNDARY_NONE : XL6_IR_MODULE_READ_BOUNDARY_RESIDENT_HOST_BIND_V1;
    reject(&ir); ir.abi.readBoundaryAbi = (XL6_IrModuleReadBoundaryAbi)99; reject(&ir); ir.abi.readBoundaryAbi = actual;
    XL6_IrModuleReadExecutionAbi execution = ir.abi.readExecutionAbi;
    ir.abi.readExecutionAbi = XL6_IR_MODULE_READ_EXECUTION_NONE; reject(&ir); ir.abi.readExecutionAbi = execution;
    assert(ir.exportCount); char* name = ir.exports[0].cName;
    ir.exports[0].cName = "xl_script_bind_ownership_read_v1"; reject(&ir); ir.exports[0].cName = name;
    assert(xl6IrVerifyModule(&ir, &parse.diags));
    assert(xl6OutputOpenFile(&out, argv[2]));
    XL6_IrCgenOptions options = {.emitScriptEntry = 1};
    if (!xl6IrCgenEmitCEx(&out, &ir, &parse.diags, &options)) goto failed;
    if (needed) xl6OutputPuts("\n/* Gate-only inspection; never a shipped entry. */\n"
        "const XL6_OwnershipReadBoundaryV1* read_binding_bound_cap(void)\n"
        "{ return xlir_ownership_read_boundary.size ? &xlir_ownership_read_boundary : NULL; }\n", &out);
    xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3]));
    if (!xl6IrCgenEmitHeader(&out, &ir, "RESTORE_READ_BINDING", &parse.diags)) goto failed;
    xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[4])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    printf("restore read binding: %zu metadata mutations rejected, needed=%d\n", mutations, needed); return 0;
failed:
    for (size_t i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%s\n", parse.diags.items[i].message);
    return 2;
}
