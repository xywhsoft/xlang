#include "src/ir/xl6_ir.h"

#include <stdio.h>

static XL6_AstNode* testType(const char* name, XL6_TypeId id, const char* abi)
{
    XL6_AstNode* type = xl6AstNewText(XL6_AST_TYPE, name, strlen(name), NULL);
    type->semanticTypeId = id;
    type->semanticTypeAbiId = xl6TypeAbiIdText(abi);
    return type;
}

int main(void)
{
    XL6_IrModule ir;
    XL6_DiagList diags;
    XL6_AstNode* root = xl6AstNew(XL6_AST_MODULE, NULL);
    XL6_AstNode* func = xl6AstNew(XL6_AST_FUNC, NULL);
    XL6_AstNode* loop = xl6AstNew(XL6_AST_WHILE, NULL);
    XL6_IrFunction* lowered;
    int ok;

    memset(&diags, 0, sizeof(diags));
    xl6IrModuleInit(&ir);
    if ( root == NULL || func == NULL || loop == NULL ) return 1;
    func->semanticSymbolId = 13;
    xl6AstAddChild(func, xl6AstNewText(XL6_AST_NAME, "spin", 4, NULL));
    func->children[0]->inferredType = testType("int", 1, "xrt.int64");
    xl6AstAddChild(func, xl6AstNew(XL6_AST_LIST, NULL));
    {
        XL6_AstNode* returns = xl6AstNew(XL6_AST_RETURN_TYPES, NULL);
        xl6AstAddChild(func, returns);
    }
    loop->semanticRegionId = 9;
    {
        XL6_AstNode* condition = xl6AstNewText(XL6_AST_BOOL, "false", 5, NULL);
        condition->inferredType = testType("bool", 2, "xrt.bool");
        condition->semanticOwnership = XL6_SEM_OWNERSHIP_COPY;
        xl6AstAddChild(loop, condition);
    }
    {
        XL6_AstNode* loopBody = xl6AstNew(XL6_AST_BLOCK, NULL);
        XL6_AstNode* exit = xl6AstNew(XL6_AST_BREAK, NULL);
        exit->semanticExitKind = XL6_SEM_EXIT_BREAK;
        exit->semanticTargetRegionId = 9;
        xl6AstAddChild(loopBody, exit);
        xl6AstAddChild(loop, loopBody);
    }
    {
        XL6_AstNode* body = xl6AstNew(XL6_AST_BLOCK, NULL);
        xl6AstAddChild(body, loop);
        xl6AstAddChild(func, body);
    }
    xl6AstAddChild(root, func);
    ok = xl6IrBuildModuleFromAst(&ir, root, &diags) &&
         xl6IrLowerModule(&ir, &diags) && xl6IrVerifyModule(&ir, &diags) &&
         ir.functionCount == 3 && ir.abi.finalizerFunction == 3 &&
         ir.functions[2].kind == XL6_IR_FUNCTION_MODULE_FINI &&
         ir.functions[2].blockCount == 1 && ir.blockCount - ir.functions[2].blockCount == 5;
    lowered = ok ? &ir.functions[1] : NULL;
    ok = ok && lowered->symbol == 13 && lowered->blockCount == 4 && lowered->instructionCount == 5 &&
          ir.instructions[lowered->firstInstruction].kind == XL6_IR_INSTR_JUMP &&
          ir.instructions[lowered->firstInstruction + 2].kind == XL6_IR_INSTR_JUMP &&
          ir.instructions[lowered->firstInstruction + 2].trueTarget == lowered->entryBlock + 3 &&
          ir.instructions[lowered->firstInstruction + 2].falseTarget == XL6_IR_BLOCK_INVALID &&
          ir.instructions[lowered->firstInstruction + 3].kind == XL6_IR_INSTR_JUMP &&
          ir.instructions[lowered->firstInstruction + 3].trueTarget == lowered->entryBlock + 3 &&
          ir.instructions[lowered->firstInstruction + 4].kind == XL6_IR_INSTR_RETURN;
    if ( !ok ) {
        fprintf(stderr, "XLIR loop lowering test failed: funcs=%lu blocks=%lu inst=%lu diag=%s\n",
            (unsigned long)ir.functionCount, (unsigned long)ir.blockCount, (unsigned long)ir.instructionCount,
            diags.count > 0 && diags.items[0].message != NULL ? diags.items[0].message : "<none>");
        if ( ir.functionCount > 1 && ir.instructionCount >= 4 ) {
            fprintf(stderr, "entry=%u last-kind=%d last-target=%u\n", (unsigned)ir.functions[1].entryBlock,
                    (int)ir.instructions[3].kind, (unsigned)ir.instructions[3].trueTarget);
        }
    }
    xl6IrModuleFree(&ir); xl6AstFree(root); xrtFree(diags.items);
    if ( !ok ) return 1;
    puts("XLIR loop lowering test ok");
    return 0;
}
