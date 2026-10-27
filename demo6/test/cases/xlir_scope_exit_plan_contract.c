#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static XL6_AstNode* typeNode(int managed)
{
    const char* name = managed ? "string" : "int";
    XL6_AstNode* type = xl6AstNewText(XL6_AST_TYPE, name, strlen(name), NULL);
    type->semanticTypeId = managed ? 2 : 1;
    type->semanticTypeAbiId = xl6TypeAbiIdText(managed ? "xrt.string" : "xrt.int64");
    return type;
}

static XL6_AstNode* local(const char* name, XL6_SymbolId symbol, int managed)
{
    XL6_AstNode* node = xl6AstNewText(XL6_AST_VAR_DECL, name, strlen(name), NULL);
    const char* text = managed ? "\"owned\"" : "42";
    XL6_AstNode* value = xl6AstNewText(managed ? XL6_AST_STRING : XL6_AST_INT, text, strlen(text), NULL);
    /* Direct AST clients provide the same decoded payload as the parser. */
    if (managed) assert(xl6AstSetLiteralData(value, "owned", 5));
    node->semanticSymbolId = symbol;
    node->inferredType = typeNode(managed);
    /* Ordinary scope ownership follows the IR block's direct bindings, not
     * the BEFORE_NODE numeric-depth filter. Direct AST clients can leave the
     * source exit annotations unset while still providing lexical structure. */
    node->semanticScopeDepth = value->semanticScopeDepth = 0;
    node->semanticRegionId = value->semanticRegionId = 7;
    value->inferredType = typeNode(managed);
    value->semanticOwnership = XL6_SEM_OWNERSHIP_COPY;
    xl6AstAddChild(node, typeNode(managed));
    xl6AstAddChild(node, value);
    return node;
}

static void reject(XL6_IrModule* ir)
{
    XL6_DiagList diags = {0};
    assert(!xl6IrVerifyModule(ir, &diags) && diags.count != 0);
    if (ir->stage == XL6_IR_STAGE_LOWERED) {
        XL6_Output out;
        char* text;
        assert(xl6OutputOpenMemory(&out));
        assert(!xl6IrCgenEmitC(&out, ir, &diags));
        text = xl6OutputTakeMemoryText(&out, NULL);
        assert(text == NULL || text[0] == '\0');
        xrtFree(text);
    }
    xl6DiagFree(&diags);
}

static void mutations(XL6_IrModule* ir, XL6_IrNodeId blockId)
{
    XL6_IrExitPlan* plan = (XL6_IrExitPlan*)xl6IrScopeExitPlanGet(ir, blockId);
    XL6_IrExitPlan saved;
    XL6_IrCleanupAction first, second;
    assert(plan != NULL && plan->cleanupCount >= 2);
    saved = *plan;
    first = ir->cleanupActions[plan->firstCleanup];
    second = ir->cleanupActions[plan->firstCleanup + 1];
    plan->point = XL6_IR_EXIT_POINT_BEFORE_NODE; reject(ir); *plan = saved;
    plan->point = (XL6_IrExitPoint)99; reject(ir); *plan = saved;
    plan->kind = XL6_SEM_EXIT_RETURN; reject(ir); *plan = saved;
    ++plan->targetScopeDepth; reject(ir); *plan = saved;
    plan->targetRegion = 7; reject(ir); *plan = saved;
    --plan->cleanupCount; reject(ir); *plan = saved;
    ir->cleanupActions[plan->firstCleanup] = second;
    ir->cleanupActions[plan->firstCleanup + 1] = first;
    reject(ir);
    ir->cleanupActions[plan->firstCleanup] = first;
    ir->cleanupActions[plan->firstCleanup + 1] = second;
    ir->cleanupActions[plan->firstCleanup].targetSymbol = 999; reject(ir);
    ir->cleanupActions[plan->firstCleanup] = first;
    assert(xl6IrVerifyModule(ir, NULL));
}

int main(void)
{
    XL6_AstNode* root = xl6AstNew(XL6_AST_MODULE, NULL);
    XL6_AstNode* function = xl6AstNewText(XL6_AST_EXPORT_FUNC, "scope_contract", 14, NULL);
    XL6_AstNode* block = xl6AstNew(XL6_AST_BLOCK, NULL);
    XL6_IrModule ir;
    XL6_DiagList diags = {0};
    XL6_IrNodeId blockId = 0;
    const XL6_IrExitPlan* plan;
    size_t i;
    function->semanticSymbolId = 10;
    block->semanticRegionId = 7;
    xl6AstAddChild(function, xl6AstNewText(XL6_AST_NAME, "scope_contract", 14, NULL));
    xl6AstAddChild(function, xl6AstNew(XL6_AST_LIST, NULL));
    xl6AstAddChild(function, xl6AstNew(XL6_AST_RETURN_TYPES, NULL));
    xl6AstAddChild(block, local("first", 11, 1));
    xl6AstAddChild(block, local("second", 12, 1));
    xl6AstAddChild(block, local("scalar", 13, 0));
    xl6AstAddChild(function, block);
    xl6AstAddChild(root, function);
    xl6IrModuleInit(&ir);
    assert(xl6IrBuildModuleFromAst(&ir, root, &diags));
    for (i = 0; i < ir.nodeCount; ++i)
        if (ir.nodes[i].kind == XL6_IR_NODE_BLOCK) blockId = (XL6_IrNodeId)(i + 1);
    plan = xl6IrScopeExitPlanGet(&ir, blockId);
    assert(plan != NULL && plan->cleanupCount == 3);
    assert(ir.cleanupActions[plan->firstCleanup].kind == XL6_IR_CLEANUP_END_STORAGE);
    assert(ir.cleanupActions[plan->firstCleanup + 1].targetSymbol == 12);
    assert(ir.cleanupActions[plan->firstCleanup + 2].targetSymbol == 11);
    mutations(&ir, blockId);
    if (!xl6IrLowerModule(&ir, &diags)) {
        for (i = 0; i < diags.count; ++i) fprintf(stderr, "%s\n", diags.items[i].message);
        assert(0 && "scope contract lowering failed");
    }
    assert(xl6IrVerifyModule(&ir, &diags));
    mutations(&ir, blockId);
    xl6IrModuleFree(&ir);
    xl6AstFree(root);
    xl6DiagFree(&diags);
    puts("scope exit contract: typed lifetime anchors and 16 verifier mutations (8 at Cgen boundary) rejected");
    return 0;
}
