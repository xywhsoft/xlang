/* Offline compiler contract test against the real compiler library. */
#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>

static void rejected(XL6_IrModule* ir, const char* reason)
{
    XL6_DiagList diagnostics;
    xl6DiagInit(&diagnostics);
    int valid = xl6IrVerifyModule(ir, &diagnostics);
    if (valid) fprintf(stderr, "unexpectedly accepted %s\n", reason);
    assert(!valid);
    assert(diagnostics.count);
    xl6DiagFree(&diagnostics);
}

int main(int argc, char** argv)
{
    XL6_CompileUnit unit;
    size_t checked = 0;
    assert(argc == 2);
    xl6CompileUnitInit(&unit);
    if (!xl6CompileUnitLoadFile(&unit, argv[1], 1)) {
        xl6DiagPrintText(&unit.parse.diags, argv[1]);
        return 2;
    }
    XL6_IrModule* ir = &unit.ir;
    assert(xl6IrVerifyModule(ir, NULL));
    for (size_t i = 0; i < ir->nodeCount; ++i) {
        XL6_IrNode* operation = &ir->nodes[i];
        const XL6_IrType* target = xl6IrTypeGet(&ir->types, operation->operatorExecutionType);
        if (!target || target->integerTarget != XL6_IR_INTEGER_TARGET_UNSIGNED_8 ||
            operation->sourceKind != XL6_AST_BINARY || operation->childCount != 2) continue;
        for (size_t index = 0; index < 2; ++index) {
            if (operation->operatorOperandAbis[index] != XL6_IR_OPERATOR_OPERAND_INTEGER_LITERAL) continue;
            XL6_IrNodeId literalId = ir->children[operation->firstChild + index];
            assert(literalId != XL6_IR_NODE_INVALID && literalId <= ir->nodeCount);
            XL6_IrNode* literal = &ir->nodes[literalId - 1];
            if (literal->sourceKind != XL6_AST_INT) continue;
            char* text = literal->text;
            literal->text = "256";
            rejected(ir, "out-of-range literal");
            literal->text = text;
            assert(xl6IrVerifyModule(ir, NULL));
            XL6_IrOperatorOperandAbi saved = operation->operatorOperandAbis[index];
            operation->operatorOperandAbis[index] = XL6_IR_OPERATOR_OPERAND_SCALAR_CONVERT;
            rejected(ir, "implicit scalar narrowing");
            operation->operatorOperandAbis[index] = saved;
            assert(xl6IrVerifyModule(ir, NULL));
            ++checked;
        }
    }
    assert(checked >= 4);
    assert(xl6IrLowerModule(ir, &unit.parse.diags));
    assert(xl6IrVerifyModule(ir, NULL));
    xl6CompileUnitFree(&unit);
    printf("narrow contextual literal contracts: %zu checked\n", checked);
    return 0;
}
