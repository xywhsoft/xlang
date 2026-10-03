#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>

static const XL6_IrInstruction* definition(const XL6_IrModule* ir, XL6_IrValueId value)
{
    for (size_t i = 0; i < ir->instructionCount; ++i)
        if (ir->instructions[i].result == value) return &ir->instructions[i];
    return NULL;
}

int main(int argc, char** argv)
{
    XL6_ParseResult parse;
    XL6_IrModule ir;
    XL6_Output output;
    unsigned gates = 0, branches = 0, mutations = 0;
    int ok = 0;
    assert(argc == 3);
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile(argv[1], &parse) || !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "safe_index_contract") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrLowerModule(&ir, &parse.diags)) goto done;
    assert(xl6IrVerifyModule(&ir, NULL));
    for (size_t i = 0; i < ir.nodeCount; ++i) {
        XL6_IrNode* node = &ir.nodes[i];
        XL6_IrIndexReceiverGate saved = node->indexReceiverGate;
        if (saved == XL6_IR_INDEX_RECEIVER_UNCONDITIONAL) continue;
        node->indexReceiverGate = XL6_IR_INDEX_RECEIVER_UNCONDITIONAL;
        assert(!xl6IrVerifyModule(&ir, NULL)); ++mutations;
        node->indexReceiverGate = (XL6_IrIndexReceiverGate)99;
        assert(!xl6IrVerifyModule(&ir, NULL)); ++mutations;
        node->indexReceiverGate = saved;
        ++gates;
    }
    for (size_t i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* branch = &ir.instructions[i];
        const XL6_IrNode* node = xl6IrNodeGet(&ir, branch->origin);
        const XL6_IrInstruction* test;
        if (branch->kind != XL6_IR_INSTR_BRANCH || branch->operandCount != 1 || node == NULL ||
            node->indexReceiverGate == XL6_IR_INDEX_RECEIVER_UNCONDITIONAL) continue;
        test = definition(&ir, ir.operands[branch->firstOperand]);
        if (test == NULL || test->kind != XL6_IR_INSTR_NULL_TEST || test->origin != branch->origin) continue;
        XL6_IrBlockId saved = branch->trueTarget;
        branch->trueTarget = branch->falseTarget; branch->falseTarget = saved;
        assert(!xl6IrVerifyModule(&ir, NULL)); ++mutations;
        branch->falseTarget = branch->trueTarget; branch->trueTarget = saved;
        ++branches;
    }
    assert(gates != 0 && branches != 0);
    assert(xl6IrVerifyModule(&ir, NULL));
    if (!xl6OutputOpenFile(&output, argv[2])) goto done;
    ok = xl6IrCgenEmitC(&output, &ir, &parse.diags);
    xl6OutputClose(&output);
    printf("safe index closed contract: %u gates, %u branches, %u rejected mutations\n", gates, branches, mutations);
done:
    if (!ok) xl6DiagPrintText(&parse.diags, argv[1]);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    return ok ? 0 : 1;
}
