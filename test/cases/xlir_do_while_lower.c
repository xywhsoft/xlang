#include "src/cgen/xl6_ir_cgen.h"

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
    XL6_Output out;
    XL6_AstNode* root;
    XL6_AstNode* func;
    XL6_AstNode* loop;
    XL6_AstNode* body;
    XL6_AstNode* cont;
    XL6_AstNode* condition;
    char* text;
    int ok;

    memset(&diags, 0, sizeof(diags));
    xl6IrModuleInit(&ir);
    root = xl6AstNew(XL6_AST_MODULE, NULL);
    /* This fixture inspects generated C, so keep the callable in the verified
     * emission closure instead of relying on dead functions being emitted. */
    func = xl6AstNew(XL6_AST_EXPORT_FUNC, NULL);
    loop = xl6AstNew(XL6_AST_DO_WHILE, NULL);
    body = xl6AstNew(XL6_AST_BLOCK, NULL);
    cont = xl6AstNew(XL6_AST_CONTINUE, NULL);
    condition = xl6AstNewText(XL6_AST_BOOL, "false", 5, NULL);
    if ( root == NULL || func == NULL || loop == NULL || body == NULL || cont == NULL || condition == NULL ||
         !xl6OutputOpenMemory(&out) ) return 1;
    func->semanticSymbolId = 24;
    xl6AstAddChild(func, xl6AstNewText(XL6_AST_NAME, "spinOnce", 8, NULL));
    func->children[0]->inferredType = testType("int", 1, "xrt.int64");
    xl6AstAddChild(func, xl6AstNew(XL6_AST_LIST, NULL));
    xl6AstAddChild(func, xl6AstNew(XL6_AST_RETURN_TYPES, NULL));
    loop->semanticRegionId = 9;
    cont->semanticExitKind = XL6_SEM_EXIT_CONTINUE;
    cont->semanticTargetRegionId = 9;
    condition->inferredType = testType("bool", 2, "xrt.bool");
    condition->semanticOwnership = XL6_SEM_OWNERSHIP_COPY;
    xl6AstAddChild(body, cont);
    xl6AstAddChild(loop, body);
    xl6AstAddChild(loop, condition);
    xl6AstAddChild(func, loop);
    xl6AstAddChild(root, func);

    ok = xl6IrBuildModuleFromAst(&ir, root, &diags) &&
         xl6IrLowerModule(&ir, &diags) && xl6IrVerifyModule(&ir, &diags) &&
         xl6IrCgenEmitC(&out, &ir, &diags);
    text = xl6OutputTakeMemoryText(&out, NULL);
    ok = ok && ir.functions[1].blockCount == 4 && text != NULL &&
         strstr(text, "goto b3;") != NULL && strstr(text, "goto b5;") != NULL &&
         strstr(text, "if (v1) goto b3; else goto b5;") == NULL;
    if ( ok ) {
        FILE* generated = fopen("build/test/xlir_do_while_generated.c", "wb");
        if ( generated == NULL || fputs(text, generated) < 0 || fclose(generated) != 0 ) ok = 0;
    }
    if ( !ok ) fprintf(stderr, "XLIR do-while lowering test failed: %s\n", diags.count > 0 ? diags.items[0].message : "<none>");
    xrtFree(text);
    xl6IrModuleFree(&ir);
    xl6AstFree(root);
    xrtFree(diags.items);
    if ( !ok ) return 1;
    puts("XLIR do-while lowering test ok");
    return 0;
}
