#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int activitySymbols;
static int visit(void* context, const char* name)
{
    (void)context;
    if (!strcmp(name, "xl6CodeActivityEnterV1") || !strcmp(name, "xl6CodeActivityLeaveV1")) ++activitySymbols;
    return 1;
}
static void reject(XL6_IrModule* ir)
{
    XL6_DiagList diags = {0}; XL6_Output out; char* text;
    assert(!xl6IrVerifyModule(ir, &diags) && diags.count);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitHeader(&out, ir, "REJECTED_ACTIVITY", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    xl6DiagFree(&diags);
}
int main(int argc, char** argv)
{
    assert(argc == 1 || argc == 3);
    for (int synthetic = 0; synthetic < 2; ++synthetic) {
        XL6_ParseResult parse; XL6_IrModule ir; xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
        assert(xl6ParseFile(synthetic ? "test/cases/xlir_host_activity_synthetic.xl"
            : "test/cases/xlir_host_activity_main.xl", &parse));
        assert(xl6StdlibExpandModule(parse.root, &parse.diags) && xl6SemCheckModule(parse.root, &parse.diags, "activity"));
        assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
        assert(ir.abi.hostExecutionAbi == XL6_IR_MODULE_HOST_EXECUTION_NONE);
        assert(xl6IrLowerModule(&ir, &parse.diags) && xl6IrVerifyModule(&ir, &parse.diags));
        assert(ir.abi.hostExecutionAbi == XL6_IR_MODULE_HOST_EXECUTION_RESIDENT_BORROWED_ACTIVITY_V1);
        assert(!ir.exportCount);
        activitySymbols = 0; assert(xl6IrCgenVisitResidentSymbols(&ir, &parse.diags, NULL, visit, NULL));
        assert(activitySymbols == (synthetic ? 0 : 2));
        XL6_IrCgenOptions options = {0}; options.emitScriptEntry = 1;
        activitySymbols = 0; assert(xl6IrCgenVisitResidentSymbols(&ir, &parse.diags, &options, visit, NULL));
        assert(activitySymbols == 2);
        if (argc == 3) {
            XL6_Output out; assert(xl6OutputOpenFile(&out, argv[synthetic + 1]));
            assert(xl6IrCgenEmitCEx(&out, &ir, &parse.diags, &options)); xl6OutputClose(&out);
        }
        ir.abi.hostExecutionAbi = XL6_IR_MODULE_HOST_EXECUTION_NONE; reject(&ir);
        ir.abi.hostExecutionAbi = (XL6_IrModuleHostExecutionAbi)2; reject(&ir);
        ir.abi.hostExecutionAbi = XL6_IR_MODULE_HOST_EXECUTION_RESIDENT_BORROWED_ACTIVITY_V1;
        assert(xl6IrVerifyModule(&ir, NULL)); xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    }
    puts("host activity IR: four missing/unknown ABI mutations rejected before C/header, exact entry-dependent resident selection");
    return 0;
}
