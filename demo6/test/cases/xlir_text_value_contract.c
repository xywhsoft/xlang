#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>

int main(int argc, char** argv)
{
    XL6_ParseResult parse;
    XL6_IrModule ir;
    XL6_Output output;
    int ok = 0;
    unsigned checked = 0;
    assert(argc == 3);
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile(argv[1], &parse) || !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "text_contract") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrLowerModule(&ir, &parse.diags)) goto done;
    for (size_t i=0; i<ir.callTargetCount; ++i) {
        XL6_IrCallTarget* t = &ir.callTargets[i];
        if (t->runtimeBridge != XL6_IR_RUNTIME_BRIDGE_TEXT_VALUE) continue;
        assert(xl6IrRuntimeDirectBridgeTargetMatches(&ir, t));
        assert(t->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE);
        assert(t->errorPhysicalAbi == XL6_IR_CALL_ERROR_PHYSICAL_XRT_THREAD_ERROR);
        t->errorAbi = XL6_IR_ERROR_ABI_NO_RAISE;
        assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir, t));
        t->errorAbi = XL6_IR_ERROR_ABI_MAY_RAISE;
        t->effectAbi = XL6_IR_CALL_EFFECT_PURE;
        assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir, t));
        t->effectAbi = XL6_IR_CALL_EFFECT_CONSERVATIVE;
        t->executionBoundaryAbi = XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING;
        assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir, t));
        t->executionBoundaryAbi = XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK;
        t->errorPhysicalAbi = XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE;
        assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir, t));
        t->errorPhysicalAbi = XL6_IR_CALL_ERROR_PHYSICAL_XRT_THREAD_ERROR;
        size_t count = t->parameterCount;
        --t->parameterCount;
        assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir, t));
        t->parameterCount = count;
        for (size_t p=0; p<count; ++p) {
            if (t->parameterAbis[p] != XL6_IR_PARAMETER_ABI_BORROW) continue;
            t->parameterAbis[p] = XL6_IR_PARAMETER_ABI_VALUE;
            assert(!xl6IrRuntimeDirectBridgeTargetMatches(&ir, t));
            t->parameterAbis[p] = XL6_IR_PARAMETER_ABI_BORROW;
        }
        assert(xl6IrRuntimeDirectBridgeTargetMatches(&ir, t)); ++checked;
    }
    assert(checked);
    if (!xl6OutputOpenFile(&output, argv[2])) goto done;
    ok = xl6IrCgenEmitC(&output, &ir, &parse.diags);
    xl6OutputClose(&output);
    printf("text contract: %u fixed ABI/effect/error/borrow signatures\n", checked);
done:
    for (size_t i=0; i<parse.diags.count; ++i)
        fprintf(stderr,"%d:%d: %s\n",parse.diags.items[i].line,parse.diags.items[i].column,parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    return !ok;
}
