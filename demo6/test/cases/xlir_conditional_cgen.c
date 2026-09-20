#include "src/ir/xl6_hir.h"
#include "src/ir/xl6_ir.h"
#include "src/cgen/xl6_ir_cgen.h"

#include <stdio.h>
#include <string.h>

static XL6_AstNode* testType(const char* name, XL6_TypeId id, const char* abi)
{
    XL6_AstNode* type = xl6AstNewText(XL6_AST_TYPE, name, strlen(name), NULL);
    if ( type != NULL ) { type->semanticTypeId = id; type->semanticTypeAbiId = xl6TypeAbiIdText(abi); }
    return type;
}

int main(void)
{
    XL6_HirModule hir;
    XL6_IrModule ir;
    XL6_DiagList diags;
    XL6_Output out;
    XL6_AstNode* root = xl6AstNew(XL6_AST_MODULE, NULL);
    XL6_AstNode* func = xl6AstNew(XL6_AST_EXPORT_FUNC, NULL);
    XL6_AstNode* body = xl6AstNew(XL6_AST_BLOCK, NULL);
    XL6_AstNode* ret = xl6AstNew(XL6_AST_RETURN, NULL);
    XL6_AstNode* choose = xl6AstNew(XL6_AST_CONDITIONAL, NULL);
    XL6_AstNode* condition = xl6AstNewText(XL6_AST_BOOL, "false", 5, NULL);
    XL6_AstNode* thenValue = xl6AstNewText(XL6_AST_INT, "40", 2, NULL);
    XL6_AstNode* elseValue = xl6AstNewText(XL6_AST_INT, "2", 1, NULL);
    XL6_AstNode* returns = xl6AstNew(XL6_AST_RETURN_TYPES, NULL);
    char* text = NULL;
    int ok;

    memset(&diags, 0, sizeof(diags)); memset(&out, 0, sizeof(out));
    xl6HirModuleInit(&hir); xl6IrModuleInit(&ir);
    if ( root == NULL || func == NULL || body == NULL || ret == NULL || choose == NULL ||
         condition == NULL || thenValue == NULL || elseValue == NULL || returns == NULL ) return 1;
    func->semanticSymbolId = 31;
    xl6AstAddChild(func, xl6AstNewText(XL6_AST_NAME, "choose", 6, NULL));
    func->children[0]->inferredType = testType("int", 1, "xrt.int64");
    xl6AstAddChild(func, xl6AstNew(XL6_AST_LIST, NULL));
    xl6AstAddChild(returns, testType("int", 1, "xrt.int64"));
    xl6AstAddChild(func, returns);
    condition->inferredType = testType("bool", 2, "xrt.bool");
    thenValue->inferredType = testType("int", 1, "xrt.int64");
    elseValue->inferredType = testType("int", 1, "xrt.int64");
    choose->inferredType = testType("int", 1, "xrt.int64");
    condition->semanticOwnership = XL6_SEM_OWNERSHIP_COPY;
    thenValue->semanticOwnership = XL6_SEM_OWNERSHIP_COPY;
    elseValue->semanticOwnership = XL6_SEM_OWNERSHIP_COPY;
    ret->semanticExitKind = XL6_SEM_EXIT_RETURN;
    xl6AstAddChild(choose, condition); xl6AstAddChild(choose, thenValue); xl6AstAddChild(choose, elseValue);
    xl6AstAddChild(ret, choose); xl6AstAddChild(body, ret); xl6AstAddChild(func, body); xl6AstAddChild(root, func);
    ok = xl6HirBuildModule(&hir, root, &diags) && xl6IrBuildModuleFromAst(&ir, root, &diags) &&
         xl6IrLowerModule(&ir, &diags) && xl6IrVerifyModule(&ir, &diags) &&
         ir.functionCount == 3 && ir.abi.finalizerFunction == 3 &&
         ir.functions[2].kind == XL6_IR_FUNCTION_MODULE_FINI &&
         ir.functions[1].blockCount == 4 && ir.functions[1].instructionCount == 10 &&
         xl6OutputOpenMemory(&out) && xl6IrCgenEmitC(&out, &ir, &diags);
    text = xl6OutputTakeMemoryText(&out, NULL);
    ok = ok && text != NULL && strstr(text, "static int64_t xlir_fn_31(void)") != NULL &&
         strstr(text, "int64_t s4294967295;") != NULL &&
         strstr(text, "if (v1) goto b3; else goto b4;") != NULL &&
         strstr(text, "s4294967295 = v2;") != NULL && strstr(text, "s4294967295 = v3;") != NULL &&
         strstr(text, "v4 = s4294967295;") != NULL && strstr(text, "return v4;") != NULL;
    if ( ok ) {
        FILE* generated = fopen("build/test/xlir_conditional_generated.c", "wb");
        if ( generated == NULL || fputs(text, generated) < 0 || fclose(generated) != 0 ) ok = 0;
    }
    if ( !ok ) fprintf(stderr, "XLIR conditional C emitter test failed: %s output=%s\n",
                       diags.count > 0 ? diags.items[0].message : "<none>", text != NULL ? text : "<null>");
    xrtFree(text); xl6HirModuleFree(&hir); xl6IrModuleFree(&ir); xl6AstFree(root); xrtFree(diags.items);
    if ( !ok ) return 1;
    puts("XLIR conditional C emitter test ok");
    return 0;
}
