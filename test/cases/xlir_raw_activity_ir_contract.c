#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned symbols;
static int visit(void* context, const char* name)
{
    (void)context;
    if (!strcmp(name, "xl6CodeActivityEnterV1") || !strcmp(name, "xl6CodeActivityLeaveV1")) ++symbols;
    return 1;
}
static void reject(XL6_IrModule* ir)
{
    XL6_DiagList diags = {0}; XL6_Output out; char* text;
    assert(!xl6IrVerifyModule(ir, &diags) && diags.count);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitHeader(&out, ir, "REJECTED_RAW_ACTIVITY", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    xl6DiagFree(&diags);
}
int main(void)
{
    XL6_ParseResult parse; XL6_IrModule ir; xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/xlir_raw_activity.xl", &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags) && xl6SemCheckModule(parse.root, &parse.diags, "raw_activity"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    for (size_t i = 0; i < ir.closureTargetCount; ++i) assert(ir.closureTargets[i].rawExecutionAbi == XL6_IR_CLOSURE_RAW_EXECUTION_NONE);
    assert(xl6IrLowerModule(&ir, &parse.diags) && xl6IrVerifyModule(&ir, &parse.diags));
    unsigned direct = 0, checked = 0, mutations = 0;
    for (size_t i = 0; i < ir.closureTargetCount; ++i) {
        XL6_IrClosureTarget* closure = &ir.closureTargets[i];
        if (!xl6IrClosureRawCallbackAbiHasEntry(closure->rawCallbackAbi)) continue;
        assert(closure->reachability != XL6_IR_REACHABILITY_UNREACHABLE && !closure->captureCount);
        assert(closure->rawExecutionAbi == XL6_IR_CLOSURE_RAW_EXECUTION_RESIDENT_BORROWED_ACTIVITY_V1);
        direct += closure->rawCallbackAbi == XL6_IR_CLOSURE_RAW_CALLBACK_DIRECT_THUNK;
        checked += closure->rawCallbackAbi == XL6_IR_CLOSURE_RAW_CALLBACK_FOREIGN_SCOPE_THUNK;
        closure->rawExecutionAbi = XL6_IR_CLOSURE_RAW_EXECUTION_NONE; reject(&ir); ++mutations;
        closure->rawExecutionAbi = (XL6_IrClosureRawExecutionAbi)2; reject(&ir); ++mutations;
        closure->rawExecutionAbi = XL6_IR_CLOSURE_RAW_EXECUTION_RESIDENT_BORROWED_ACTIVITY_V1;
    }
    assert(direct == 6 && checked == 6 && mutations == 24);
    assert(xl6IrCgenVisitResidentSymbols(&ir, &parse.diags, NULL, visit, NULL) && symbols == 2);
    assert(xl6IrVerifyModule(&ir, NULL)); xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    puts("raw activity IR: six direct and six checked entries, 24 missing/unknown mutations rejected before C/header, exactly two resident symbols");
    return 0;
}
