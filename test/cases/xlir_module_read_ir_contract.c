#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>

static unsigned mutations;
static void reject(XL6_IrModule* ir)
{
    XL6_DiagList diags = {0}; XL6_Output out; char* text;
    assert(!xl6IrVerifyModule(ir, &diags) && diags.count);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitHeader(&out, ir, "REJECT_READ_EXECUTION", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}
#define MUTATE(Field, Type) do { Type saved = ir.abi.Field; \
    assert(saved != 0); ir.abi.Field = (Type)0; reject(&ir); \
    ir.abi.Field = (Type)99; reject(&ir); ir.abi.Field = saved; } while (0)
int main(void)
{
    XL6_ParseResult parse; XL6_IrModule ir; xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/xlir_raw_activity.xl", &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags) && xl6SemCheckModule(parse.root, &parse.diags, "module_read"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    assert(ir.abi.readExecutionAbi == XL6_IR_MODULE_READ_EXECUTION_NONE);
    ir.abi.readExecutionAbi = XL6_IR_MODULE_READ_EXECUTION_TRACKED_SLOTS_V1; reject(&ir);
    ir.abi.readExecutionAbi = XL6_IR_MODULE_READ_EXECUTION_NONE;
    assert(xl6IrLowerModule(&ir, &parse.diags) && xl6IrVerifyModule(&ir, &parse.diags));
    MUTATE(readExecutionAbi, XL6_IrModuleReadExecutionAbi);
    MUTATE(codeOwnerAbi, XL6_IrModuleCodeOwnerAbi);
    MUTATE(hostExecutionAbi, XL6_IrModuleHostExecutionAbi);
    MUTATE(closureStorageAbi, XL6_IrClosureStorageAbi);
    MUTATE(boundMethodStorageAbi, XL6_IrBoundMethodStorageAbi);
    MUTATE(classStorageAbi, XL6_IrClassStorageAbi);
    MUTATE(classIdentityAbi, XL6_IrClassIdentityAbi);
    MUTATE(asyncStorageAbi, XL6_IrAsyncStorageAbi);
    MUTATE(taskStorageAbi, XL6_IrTaskStorageAbi);
    MUTATE(generatorStorageAbi, XL6_IrGeneratorStorageAbi);
    MUTATE(slotTransferAbi, XL6_IrSlotTransferAbi);
    for (size_t i = 0; i < ir.closureTargetCount; ++i) {
        XL6_IrClosureTarget* target = &ir.closureTargets[i];
        assert(target->rawExecutionAbi == XL6_IR_CLOSURE_RAW_EXECUTION_RESIDENT_BORROWED_ACTIVITY_V1);
        target->rawExecutionAbi = XL6_IR_CLOSURE_RAW_EXECUTION_NONE; reject(&ir);
        target->rawExecutionAbi = (XL6_IrClosureRawExecutionAbi)99; reject(&ir);
        target->rawExecutionAbi = XL6_IR_CLOSURE_RAW_EXECUTION_RESIDENT_BORROWED_ACTIVITY_V1;
    }
    assert(ir.exportCount == 1);
    XL6_IrExportExecutionAbi saved = ir.exports[0].executionAbi;
    ir.exports[0].executionAbi = (XL6_IrExportExecutionAbi)0; reject(&ir);
    ir.exports[0].executionAbi = (XL6_IrExportExecutionAbi)99; reject(&ir);
    ir.exports[0].executionAbi = saved;
    assert(mutations == 49 && xl6IrVerifyModule(&ir, NULL));
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    puts("module read certificate: 49 typed/module/export/raw/storage contract mutations rejected before C/header");
    return 0;
}
