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
    unsigned sites = 0, mutations = 0;
    int ok = 0;
    assert(argc == 3);
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile(argv[1], &parse) || !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "nullable_getter_contract") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrLowerModule(&ir, &parse.diags)) goto done;
    assert(xl6IrVerifyModule(&ir, NULL));
    for (size_t i = 0; i < ir.callSiteCount; ++i) {
        XL6_IrCallSite* site = &ir.callSites[i];
        XL6_IrPropertyAccessorSignature signature;
        if (site->receiverGate != XL6_IR_CALL_RECEIVER_NULLABLE_GETTER) continue;
        assert(xl6IrGetPropertyAccessorSignature(&ir, site, &signature));
        site->receiverGate = XL6_IR_CALL_RECEIVER_UNCONDITIONAL;
        assert(!xl6IrVerifyModule(&ir, NULL)); ++mutations;
        site->receiverGate = (XL6_IrCallReceiverGate)99;
        assert(!xl6IrVerifyModule(&ir, NULL)); ++mutations;
        site->receiverGate = XL6_IR_CALL_RECEIVER_NULLABLE_GETTER;
        size_t order = site->receiverEvaluationIndex;
        site->receiverEvaluationIndex = SIZE_MAX;
        assert(!xl6IrVerifyModule(&ir, NULL)); ++mutations;
        site->receiverEvaluationIndex = order;
        XL6_IrNodeId receiver = site->receiver;
        site->receiver = XL6_IR_NODE_INVALID;
        assert(!xl6IrVerifyModule(&ir, NULL)); ++mutations;
        site->receiver = receiver;
        if (site->resultType != signature.resultType) {
            XL6_IrTypeId result = site->resultType;
            XL6_IrNode* origin = &ir.nodes[site->origin - 1];
            XL6_IrTypeId nodeResult = origin->storageType;
            /* Corrupt both node and site consistently: only the lifted
             * declaration contract can reject this loss of nullable. */
            site->resultType = origin->storageType = signature.resultType;
            assert(!xl6IrVerifyModule(&ir, NULL)); ++mutations;
            site->resultType = result; origin->storageType = nodeResult;
        }
        assert(xl6IrVerifyModule(&ir, NULL)); ++sites;
    }
    assert(sites != 0 && mutations >= sites * 4);
    if (!xl6OutputOpenFile(&output, argv[2])) goto done;
    ok = xl6IrCgenEmitC(&output, &ir, &parse.diags);
    xl6OutputClose(&output);
    printf("nullable getter closed contract: %u sites, %u rejected mutations\n", sites, mutations);
done:
    if (!ok) xl6DiagPrintText(&parse.diags, argv[1]);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    return ok ? 0 : 1;
}
