#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv)
{
    XL6_ParseResult parse;
    XL6_IrModule ir;
    XL6_Output output;
    int ok = 0;
    assert(argc == 3);
    xl6ParseResultInit(&parse);
    xl6IrModuleInit(&ir);
    if (!xl6ParseFile(argv[1], &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "logging_contract") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrLowerModule(&ir, &parse.diags) ||
        !xl6IrVerifyModule(&ir, &parse.diags)) goto done;
    assert(xl6OutputOpenFile(&output,argv[2]));
    ok = xl6IrCgenEmitC(&output,&ir,&parse.diags);
    xl6OutputClose(&output);
    if (ok) {
        unsigned checked = 0;
        for (size_t i=0;i<ir.callTargetCount;++i) {
            XL6_IrCallTarget* target = &ir.callTargets[i];
            if (target->runtimeBridge != XL6_IR_RUNTIME_BRIDGE_LOG) continue;
            assert(xl6IrRuntimeLogBridgeTargetMatches(&ir,target));
            target->effectAbi = XL6_IR_CALL_EFFECT_PURE;
            assert(!xl6IrRuntimeLogBridgeTargetMatches(&ir,target));
            target->effectAbi = XL6_IR_CALL_EFFECT_CONSERVATIVE;
            target->executionBoundaryAbi = XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING;
            assert(!xl6IrRuntimeLogBridgeTargetMatches(&ir,target));
            target->executionBoundaryAbi = XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK;
            XL6_IrErrorAbi error = target->errorAbi;
            target->errorAbi = error == XL6_IR_ERROR_ABI_NO_RAISE
                ? XL6_IR_ERROR_ABI_MAY_RAISE : XL6_IR_ERROR_ABI_NO_RAISE;
            assert(!xl6IrRuntimeLogBridgeTargetMatches(&ir,target));
            target->errorAbi = error;
            for (size_t p=0;p<target->parameterCount;++p) {
                if (target->parameterAbis[p] == XL6_IR_PARAMETER_ABI_BORROW) {
                    target->parameterAbis[p] = XL6_IR_PARAMETER_ABI_VALUE;
                    assert(!xl6IrRuntimeLogBridgeTargetMatches(&ir,target));
                    target->parameterAbis[p] = XL6_IR_PARAMETER_ABI_BORROW;
                    ++checked;
                }
            }
            assert(xl6IrRuntimeLogBridgeTargetMatches(&ir,target));
        }
        assert(checked > 0);
    }
done:
    for (size_t i=0;i<parse.diags.count;++i)
        fprintf(stderr,"%d:%d: %s\n",parse.diags.items[i].line,parse.diags.items[i].column,parse.diags.items[i].message);
    xl6IrModuleFree(&ir);
    xl6ParseResultFree(&parse);
    return !ok;
}
