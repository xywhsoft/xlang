#include "src/ir/xl6_hir.h"
#include "src/ir/xl6_ir.h"
#include "src/cgen/xl6_ir_cgen.h"

#include <stdio.h>
#include <string.h>

static XL6_AstNode* testBoolType(void)
{
    XL6_AstNode* type = xl6AstNewText(XL6_AST_TYPE, "bool", 4, NULL);
    if ( type != NULL ) {
        type->semanticTypeId = 1;
        type->semanticTypeAbiId = xl6TypeAbiIdText("xrt.bool");
    }
    return type;
}

static XL6_AstNode* testShortCircuitFunction(
    XL6_SymbolId symbol, const char* name, const char* op, const char* leftText, const char* rightText)
{
    XL6_AstNode* func = xl6AstNew(XL6_AST_EXPORT_FUNC, NULL);
    XL6_AstNode* body = xl6AstNew(XL6_AST_BLOCK, NULL);
    XL6_AstNode* ret = xl6AstNew(XL6_AST_RETURN, NULL);
    XL6_AstNode* binary = xl6AstNewText(XL6_AST_BINARY, op, strlen(op), NULL);
    XL6_AstNode* left = xl6AstNewText(XL6_AST_BOOL, leftText, strlen(leftText), NULL);
    XL6_AstNode* right = xl6AstNewText(XL6_AST_BOOL, rightText, strlen(rightText), NULL);
    XL6_AstNode* returns = xl6AstNew(XL6_AST_RETURN_TYPES, NULL);
    if ( func == NULL || body == NULL || ret == NULL || binary == NULL || left == NULL || right == NULL || returns == NULL ) return NULL;
    func->semanticSymbolId = symbol;
    xl6AstAddChild(func, xl6AstNewText(XL6_AST_NAME, name, strlen(name), NULL));
    func->children[0]->inferredType = testBoolType();
    xl6AstAddChild(func, xl6AstNew(XL6_AST_LIST, NULL));
    xl6AstAddChild(returns, testBoolType());
    xl6AstAddChild(func, returns);
    binary->inferredType = testBoolType();
    left->inferredType = testBoolType();
    right->inferredType = testBoolType();
    ret->semanticExitKind = XL6_SEM_EXIT_RETURN;
    xl6AstAddChild(binary, left);
    xl6AstAddChild(binary, right);
    xl6AstAddChild(ret, binary);
    xl6AstAddChild(body, ret);
    xl6AstAddChild(func, body);
    return func;
}

static int testFunctionUsesOnlyCfg(const char* text, const char* signature)
{
    const char* begin = text != NULL ? strstr(text, signature) : NULL;
    const char* end = begin != NULL ? strstr(begin, "\n}\n") : NULL;
    const char* logicalAnd = begin != NULL ? strstr(begin, "&&") : NULL;
    const char* logicalOr = begin != NULL ? strstr(begin, "||") : NULL;
    /* Host lifecycle guards may use C logical operators. The source
     * short-circuit bodies themselves must remain explicit verified CFG. */
    return end != NULL && (logicalAnd == NULL || logicalAnd > end) &&
           (logicalOr == NULL || logicalOr > end);
}

int main(void)
{
    XL6_HirModule hir;
    XL6_IrModule ir;
    XL6_DiagList diags;
    XL6_Output out;
    XL6_AstNode* root = xl6AstNew(XL6_AST_MODULE, NULL);
    char* text = NULL;
    int ok;

    memset(&diags, 0, sizeof(diags));
    memset(&out, 0, sizeof(out));
    xl6HirModuleInit(&hir);
    xl6IrModuleInit(&ir);
    if ( root == NULL ) return 1;
    xl6AstAddChild(root, testShortCircuitFunction(21, "andValue", "and", "false", "true"));
    xl6AstAddChild(root, testShortCircuitFunction(22, "orValue", "or", "true", "false"));
    ok = root->childCount == 2 && root->children[0] != NULL && root->children[1] != NULL &&
         xl6HirBuildModule(&hir, root, &diags) && xl6IrBuildModuleFromAst(&ir, root, &diags) &&
         xl6IrLowerModule(&ir, &diags) && xl6IrVerifyModule(&ir, &diags) &&
         ir.functionCount == 4 && ir.abi.finalizerFunction == 4 &&
         ir.functions[3].kind == XL6_IR_FUNCTION_MODULE_FINI &&
         ir.functions[1].blockCount == 3 && ir.functions[2].blockCount == 3 &&
         xl6OutputOpenMemory(&out) && xl6IrCgenEmitC(&out, &ir, &diags);
    text = xl6OutputTakeMemoryText(&out, NULL);
    ok = ok && text != NULL && strstr(text, "static bool xlir_fn_21(void)") != NULL &&
         strstr(text, "static bool xlir_fn_22(void)") != NULL &&
         strstr(text, "v2 = v1;") != NULL &&
         strstr(text, "if (v1) goto b3; else goto b4;") != NULL &&
         strstr(text, "if (v4) goto b7; else goto b6;") != NULL &&
         testFunctionUsesOnlyCfg(text, "static bool xlir_fn_21(void)\n{") &&
         testFunctionUsesOnlyCfg(text, "static bool xlir_fn_22(void)\n{");
    if ( ok ) {
        FILE* generated = fopen("build/test/xlir_short_circuit_generated.c", "wb");
        if ( generated == NULL || fputs(text, generated) < 0 || fclose(generated) != 0 ) ok = 0;
    }
    if ( !ok ) fprintf(stderr, "XLIR short-circuit C emitter test failed: %s output=%s\n",
                       diags.count > 0 ? diags.items[0].message : "<none>", text != NULL ? text : "<null>");
    xrtFree(text);
    xl6HirModuleFree(&hir);
    xl6IrModuleFree(&ir);
    xl6AstFree(root);
    xrtFree(diags.items);
    if ( !ok ) return 1;
    puts("XLIR short-circuit C emitter test ok");
    return 0;
}
