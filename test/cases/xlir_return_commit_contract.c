#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static XL6_AstNode* stringType(void)
{
    XL6_AstNode* type = xl6AstNewText(XL6_AST_TYPE, "string", 6, NULL);
    type->semanticTypeId = 2;
    type->semanticTypeAbiId = xl6TypeAbiIdText("xrt.string");
    return type;
}

static void reject(XL6_IrModule* ir)
{
    XL6_DiagList diags = {0};
    XL6_Output out;
    char* text;
    assert(!xl6IrVerifyModule(ir, &diags) && diags.count != 0);
    assert(xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL);
    assert(text == NULL || text[0] == '\0');
    xrtFree(text);
    xl6DiagFree(&diags);
}

static void runContract(int product)
{
    XL6_AstNode* root = xl6AstNew(XL6_AST_MODULE, NULL);
    XL6_AstNode* fn = xl6AstNewText(XL6_AST_EXPORT_FUNC, "result_contract", 15, NULL);
    XL6_AstNode* body = xl6AstNew(XL6_AST_BLOCK, NULL);
    XL6_AstNode* parameters = xl6AstNew(XL6_AST_LIST, NULL);
    XL6_AstNode* parameter = xl6AstNewText(XL6_AST_PARAM, "input", 5, NULL);
    XL6_AstNode* returns = xl6AstNew(XL6_AST_RETURN_TYPES, NULL);
    XL6_AstNode* ret = xl6AstNew(XL6_AST_RETURN, NULL);
    XL6_AstNode* value = xl6AstNewText(XL6_AST_STRING, "\"owned\"", 7, NULL);
    XL6_IrModule ir;
    XL6_DiagList diags = {0};
    size_t i, index = 0;
    XL6_IrInstruction* move;
    XL6_IrInstruction saved;
    XL6_IrValueId savedOperand;
    fn->semanticSymbolId = 10;
    parameter->semanticSymbolId = 11;
    parameter->inferredType = stringType();
    xl6AstAddChild(parameter, stringType());
    xl6AstAddChild(parameters, parameter);
    value->inferredType = stringType();
    value->semanticOwnership = XL6_SEM_OWNERSHIP_COPY;
    ret->semanticExitKind = XL6_SEM_EXIT_RETURN;
    xl6AstAddChild(fn, xl6AstNewText(XL6_AST_NAME, "result_contract", 15, NULL));
    xl6AstAddChild(fn, parameters);
    xl6AstAddChild(returns, stringType());
    if (product) xl6AstAddChild(returns, stringType());
    xl6AstAddChild(fn, returns);
    xl6AstAddChild(ret, value);
    if (product) {
        XL6_AstNode* second = xl6AstNewText(XL6_AST_STRING, "\"second\"", 8, NULL);
        second->inferredType = stringType();
        second->semanticOwnership = XL6_SEM_OWNERSHIP_COPY;
        xl6AstAddChild(ret, second);
    }
    xl6AstAddChild(body, ret);
    xl6AstAddChild(fn, body); xl6AstAddChild(root, fn);
    xl6IrModuleInit(&ir);
    assert(xl6IrBuildModuleFromAst(&ir, root, &diags));
    assert(xl6IrLowerModule(&ir, &diags));
    assert(xl6IrVerifyModule(&ir, &diags));
    for (i = 0; i < ir.instructionCount; ++i)
        if (ir.instructions[i].kind == XL6_IR_INSTR_RETURN && ir.instructions[i].operandCount == 1)
            index = i;
    assert(index >= 2 && ir.instructions[index - 2].kind == XL6_IR_INSTR_LOAD);
    move = &ir.instructions[index - 1]; saved = *move;
    assert(move->kind == XL6_IR_INSTR_MOVE);
    move->kind = XL6_IR_INSTR_RELEASE; move->operandCount = 0; reject(&ir); *move = saved;
    move->targetSymbol = 0; reject(&ir); *move = saved;
    move->type = XL6_IR_TYPE_INVALID; reject(&ir); *move = saved;
    move->operandCount = 0; reject(&ir); *move = saved;
    savedOperand = ir.operands[move->firstOperand];
    ir.operands[move->firstOperand] = ir.instructions[index - 2].result - 1;
    reject(&ir); ir.operands[move->firstOperand] = savedOperand;
    savedOperand = ir.operands[ir.instructions[index].firstOperand];
    ir.operands[ir.instructions[index].firstOperand] = ir.instructions[index - 2].result - 1;
    reject(&ir); ir.operands[ir.instructions[index].firstOperand] = savedOperand;
    {
        XL6_IrInstruction parameterRelease = ir.instructions[index - 3];
        assert(parameterRelease.kind == XL6_IR_INSTR_RELEASE && parameterRelease.targetSymbol == 11);
        ir.instructions[index - 3] = *move;
        *move = parameterRelease;
        reject(&ir);
        ir.instructions[index - 3] = parameterRelease;
        *move = saved;
    }
    assert(xl6IrVerifyModule(&ir, NULL));
    xl6IrModuleFree(&ir); xl6AstFree(root); xl6DiagFree(&diags);
}

int main(void)
{
    runContract(0);
    runContract(1);
    puts("return commit contract: 14 single/product early/malformed ownership commits rejected before C emission");
    return 0;
}
