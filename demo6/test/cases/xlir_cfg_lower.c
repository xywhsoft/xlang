#include "src/ir/xl6_ir.h"
#include "src/cgen/xl6_ir_cgen.h"

#include <stdio.h>

static XL6_AstNode* testType(const char* name, XL6_TypeId id, const char* abi)
{
    XL6_AstNode* type = xl6AstNewText(XL6_AST_TYPE, name, strlen(name), NULL);
    if ( type != NULL ) {
        type->semanticTypeId = id;
        type->semanticTypeAbiId = xl6TypeAbiIdText(abi);
    }
    return type;
}

static XL6_AstNode* testReturn(const char* text)
{
    XL6_AstNode* ret = xl6AstNew(XL6_AST_RETURN, NULL);
    XL6_AstNode* value = xl6AstNewText(XL6_AST_INT, text, strlen(text), NULL);
    value->inferredType = testType("int", 1, "xrt.int64");
    value->semanticOwnership = XL6_SEM_OWNERSHIP_COPY;
    ret->semanticExitKind = XL6_SEM_EXIT_RETURN;
    xl6AstAddChild(ret, value);
    return ret;
}

int main(void)
{
    XL6_IrModule ir;
    XL6_DiagList diags;
    XL6_Output out;
    XL6_AstNode* root = xl6AstNew(XL6_AST_MODULE, NULL);
    /* Keep the CFG callable in Cgen's verified emission closure.  An ordinary
     * unreferenced function is intentionally pruned and cannot test CFG->C. */
    XL6_AstNode* func = xl6AstNew(XL6_AST_EXPORT_FUNC, NULL);
    XL6_AstNode* body = xl6AstNew(XL6_AST_BLOCK, NULL);
    XL6_AstNode* branch = xl6AstNew(XL6_AST_IF, NULL);
    XL6_AstNode* condition = xl6AstNewText(XL6_AST_BOOL, "true", 4, NULL);
    XL6_IrFunction* lowered;
    char* cText;
    int ok;

    memset(&diags, 0, sizeof(diags)); memset(&out, 0, sizeof(out));
    xl6IrModuleInit(&ir);
    if ( root == NULL || func == NULL || body == NULL || branch == NULL || condition == NULL ) return 1;
    func->semanticSymbolId = 12;
    condition->inferredType = testType("bool", 2, "xrt.bool");
    condition->semanticOwnership = XL6_SEM_OWNERSHIP_COPY;
    xl6AstAddChild(func, xl6AstNewText(XL6_AST_NAME, "choose", 6, NULL));
    func->children[0]->inferredType = testType("int", 1, "xrt.int64");
    xl6AstAddChild(func, xl6AstNew(XL6_AST_LIST, NULL));
    {
        XL6_AstNode* returns = xl6AstNew(XL6_AST_RETURN_TYPES, NULL);
        xl6AstAddChild(returns, testType("int", 1, "xrt.int64"));
        xl6AstAddChild(func, returns);
    }
    {
        XL6_AstNode* thenBlock = xl6AstNew(XL6_AST_BLOCK, NULL);
        XL6_AstNode* elseIfs = xl6AstNew(XL6_AST_LIST, NULL);
        XL6_AstNode* elseBlock = xl6AstNew(XL6_AST_BLOCK, NULL);
        xl6AstAddChild(thenBlock, testReturn("1"));
        xl6AstAddChild(elseBlock, testReturn("2"));
        xl6AstAddChild(branch, condition);
        xl6AstAddChild(branch, thenBlock);
        xl6AstAddChild(branch, elseIfs);
        xl6AstAddChild(branch, elseBlock);
    }
    xl6AstAddChild(body, branch);
    xl6AstAddChild(func, body);
    xl6AstAddChild(root, func);
    ok = xl6IrBuildModuleFromAst(&ir, root, &diags) &&
         xl6IrLowerModule(&ir, &diags) && xl6IrVerifyModule(&ir, &diags) &&
         ir.functionCount == 3 && ir.abi.finalizerFunction == 3 &&
         ir.functions[2].kind == XL6_IR_FUNCTION_MODULE_FINI &&
         ir.functions[2].blockCount == 1 && ir.blockCount - ir.functions[2].blockCount == 5;
    lowered = ok ? &ir.functions[1] : NULL;
    ok = ok && lowered->symbol == 12 && lowered->blockCount == 4 && lowered->instructionCount == 6 &&
         ir.instructions[lowered->firstInstruction + 1].kind == XL6_IR_INSTR_BRANCH &&
         ir.instructions[lowered->firstInstruction + 1].trueTarget != XL6_IR_BLOCK_INVALID &&
         ir.instructions[lowered->firstInstruction + 1].falseTarget != XL6_IR_BLOCK_INVALID;
    cText = NULL;
    if ( ok && xl6OutputOpenMemory(&out) ) {
        ok = xl6IrCgenEmitC(&out, &ir, &diags);
        cText = xl6OutputTakeMemoryText(&out, NULL);
        ok = ok && cText != NULL && strstr(cText, "if (v1) goto b") != NULL;
        if ( ok ) {
            FILE* generated = fopen("build/test/xlir_cfg_generated.c", "wb");
            if ( generated == NULL || fputs(cText, generated) < 0 || fclose(generated) != 0 ) ok = 0;
        }
    } else ok = 0;
    if ( ok ) {
        XL6_IrInstruction saved = ir.instructions[lowered->firstInstruction + 1];
        ir.instructions[lowered->firstInstruction + 1].trueTarget = ir.functions[0].entryBlock;
        ok = !xl6IrVerifyModule(&ir, NULL);
        ir.instructions[lowered->firstInstruction + 1] = saved;
    }
    if ( ok ) {
        XL6_IrBlock saved = ir.blocks[lowered->firstBlock + 1];
        ir.blocks[lowered->firstBlock + 1].firstInstruction = lowered->firstInstruction;
        ok = !xl6IrVerifyModule(&ir, NULL);
        ir.blocks[lowered->firstBlock + 1] = saved;
    }
    if ( !ok ) {
        size_t i;
        fprintf(stderr, "XLIR CFG lowering test failed: diags=%lu blocks=%lu instructions=%lu %s\n", (unsigned long)diags.count,
                (unsigned long)ir.blockCount, (unsigned long)ir.instructionCount,
                diags.count > 0 && diags.items[0].message != NULL ? diags.items[0].message : "<none>");
        for ( i = 0; i < ir.blockCount; ++i ) fprintf(stderr, "block %lu first=%lu count=%lu\n", (unsigned long)i,
            (unsigned long)ir.blocks[i].firstInstruction, (unsigned long)ir.blocks[i].instructionCount);
        for ( i = 0; i < ir.functionCount; ++i ) fprintf(stderr,
            "function %lu symbol=%u firstBlock=%lu blocks=%lu firstInstruction=%lu instructions=%lu\n",
            (unsigned long)i, (unsigned int)ir.functions[i].symbol,
            (unsigned long)ir.functions[i].firstBlock, (unsigned long)ir.functions[i].blockCount,
            (unsigned long)ir.functions[i].firstInstruction,
            (unsigned long)ir.functions[i].instructionCount);
        for ( i = 0; i < ir.instructionCount; ++i ) fprintf(stderr,
            "instruction %lu kind=%d result=%u true=%u false=%u\n",
            (unsigned long)i, (int)ir.instructions[i].kind,
            (unsigned int)ir.instructions[i].result,
            (unsigned int)ir.instructions[i].trueTarget,
            (unsigned int)ir.instructions[i].falseTarget);
        if ( cText != NULL ) fprintf(stderr, "generated C:\n%s\n", cText);
    }
    xrtFree(cText); xl6IrModuleFree(&ir); xl6AstFree(root); xrtFree(diags.items);
    if ( !ok ) return 1;
    puts("XLIR CFG lowering test ok");
    return 0;
}
