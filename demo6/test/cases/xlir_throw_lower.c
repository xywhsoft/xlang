#include "src/ir/xl6_hir.h"
#include "src/ir/xl6_ir.h"
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
    XL6_HirModule hir;
    XL6_IrModule ir;
    XL6_DiagList diags;
    XL6_AstNode* root;
    XL6_AstNode* func;
    XL6_AstNode* body;
    XL6_AstNode* thrown;
    XL6_AstNode* value;
    int ok;

    memset(&diags, 0, sizeof(diags));
    xl6HirModuleInit(&hir);
    xl6IrModuleInit(&ir);
    root = xl6AstNew(XL6_AST_MODULE, NULL);
    func = xl6AstNew(XL6_AST_FUNC, NULL);
    body = xl6AstNew(XL6_AST_BLOCK, NULL);
    thrown = xl6AstNew(XL6_AST_THROW, NULL);
    value = xl6AstNewText(XL6_AST_INT, "9", 1, NULL);
    if ( root == NULL || func == NULL || body == NULL || thrown == NULL || value == NULL ) return 1;
    func->semanticSymbolId = 21;
    xl6AstAddChild(func, xl6AstNewText(XL6_AST_NAME, "fail", 4, NULL));
    func->children[0]->inferredType = testType("int", 1, "xrt.int64");
    xl6AstAddChild(func, xl6AstNew(XL6_AST_LIST, NULL));
    xl6AstAddChild(func, xl6AstNew(XL6_AST_RETURN_TYPES, NULL));
    value->inferredType = testType("int", 1, "xrt.int64");
    value->semanticOwnership = XL6_SEM_OWNERSHIP_COPY;
    thrown->semanticExitKind = XL6_SEM_EXIT_THROW;
    xl6AstAddChild(thrown, value);
    xl6AstAddChild(body, thrown);
    xl6AstAddChild(func, body);
    xl6AstAddChild(root, func);

    /* A scalar payload has no error-state ABI.  The lowering boundary must
     * reject it before Cgen, rather than constructing an untyped raise. */
    ok = xl6HirBuildModule(&hir, root, &diags) && xl6IrBuildModuleFromAst(&ir, root, &diags) &&
         !xl6IrLowerModule(&ir, &diags) && diags.count > 0 &&
         ir.stage == XL6_IR_STAGE_TYPED;
    if ( !ok ) fprintf(stderr, "XLIR throw lowering test failed: %s\n", diags.count > 0 ? diags.items[0].message : "<none>");
    xl6HirModuleFree(&hir);
    xl6IrModuleFree(&ir);
    xl6AstFree(root);
    xrtFree(diags.items);
    if ( !ok ) return 1;
    puts("XLIR throw lowering test ok");
    return 0;
}
