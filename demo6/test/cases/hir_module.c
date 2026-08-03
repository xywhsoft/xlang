#include "src/ir/xl6_hir.h"

#include <stdio.h>

static XL6_AstNode* testIntType(void)
{
    XL6_AstNode* type;

    type = xl6AstNewText(XL6_AST_TYPE, "int", 3, NULL);
    if ( type != NULL ) {
        type->semanticTypeId = 1;
        type->semanticTypeAbiId = xl6TypeAbiIdText("xrt.int64");
    }
    return type;
}

int main(void)
{
    XL6_HirModule hir;
    XL6_DiagList diags;
    XL6_AstNode* root;
    XL6_AstNode* decl;
    XL6_AstNode* autoType;
    XL6_AstNode* value;
    XL6_AstNode* call;
    XL6_AstNode* assign;
    XL6_AstNode* assignTarget;
    XL6_AstNode* assignValue;
    XL6_AstNode* returnNode;
    XL6_AstNode* returnValue;
    XL6_HirNodeId declId;
    XL6_HirNodeId valueId;
    XL6_HirNodeId callId;
    XL6_HirNodeId assignId;
    XL6_HirNodeId returnId;
    const XL6_HirNode* valueNode;
    const XL6_HirNode* callNode;
    const XL6_HirNode* assignNode;
    const XL6_HirNode* returnHirNode;
    const XL6_HirNode* symbolNode;
    XL6_HirTypeId savedType;
    XL6_HirTypeId savedStorageType;
    XL6_HirNodeId savedChild;
    XL6_AstKind savedSourceKind;
    XL6_HirNodeId savedSymbolNode;
    size_t savedResolvedCallCount;
    int baselineOk;
    int ok;

    memset(&diags, 0, sizeof(diags));
    xl6HirModuleInit(&hir);
    root = xl6AstNew(XL6_AST_MODULE, NULL);
    decl = xl6AstNewText(XL6_AST_VAR_DECL, "answer", 6, NULL);
    autoType = xl6AstNewText(XL6_AST_TYPE, "auto", 4, NULL);
    value = xl6AstNewText(XL6_AST_INT, "42", 2, NULL);
    call = xl6AstNew(XL6_AST_CALL, NULL);
    assign = xl6AstNewText(XL6_AST_ASSIGN, "=", 1, NULL);
    assignTarget = xl6AstNewText(XL6_AST_NAME, "answer", 6, NULL);
    assignValue = xl6AstNewText(XL6_AST_INT, "7", 1, NULL);
    returnNode = xl6AstNew(XL6_AST_RETURN, NULL);
    returnValue = xl6AstNewText(XL6_AST_INT, "0", 1, NULL);
    if ( root == NULL || decl == NULL || autoType == NULL || value == NULL ||
         call == NULL || assign == NULL || assignTarget == NULL ||
         assignValue == NULL || returnNode == NULL || returnValue == NULL ) {
        return 1;
    }
    decl->semanticSymbolId = 7;
    value->inferredType = testIntType();
    value->semanticOwnership = XL6_SEM_OWNERSHIP_COPY;
    xl6AstAddChild(decl, autoType);
    xl6AstAddChild(decl, value);
    xl6AstAddChild(root, decl);

    call->inferredType = testIntType();
    call->semanticCallKind = XL6_SEM_CALL_DIRECT;
    call->semanticTargetSymbolId = 7;
    call->semanticOwnership = XL6_SEM_OWNERSHIP_COPY;
    xl6AstAddChild(root, call);

    assign->semanticLvalueKind = XL6_SEM_LVALUE_LOCAL;
    assignTarget->inferredType = testIntType();
    assignTarget->semanticLvalueKind = XL6_SEM_LVALUE_LOCAL;
    assignTarget->semanticOwnership = XL6_SEM_OWNERSHIP_BORROW;
    assignValue->inferredType = testIntType();
    assignValue->semanticOwnership = XL6_SEM_OWNERSHIP_COPY;
    xl6AstAddChild(assign, assignTarget);
    xl6AstAddChild(assign, assignValue);
    xl6AstAddChild(root, assign);

    returnNode->semanticExitKind = XL6_SEM_EXIT_RETURN;
    returnNode->semanticScopeDepth = 3;
    returnNode->semanticTargetScopeDepth = 1;
    returnNode->semanticRegionId = 3;
    returnNode->semanticTargetRegionId = 2;
    returnValue->inferredType = testIntType();
    returnValue->semanticOwnership = XL6_SEM_OWNERSHIP_COPY;
    xl6AstAddChild(returnNode, returnValue);
    xl6AstAddChild(root, returnNode);

    ok = xl6HirBuildModule(&hir, root, &diags);
    declId = xl6HirSourceNode(&hir, decl);
    valueId = xl6HirSourceNode(&hir, value);
    callId = xl6HirSourceNode(&hir, call);
    assignId = xl6HirSourceNode(&hir, assign);
    returnId = xl6HirSourceNode(&hir, returnNode);
    valueNode = xl6HirNodeGet(&hir, valueId);
    callNode = xl6HirNodeGet(&hir, callId);
    assignNode = xl6HirNodeGet(&hir, assignId);
    returnHirNode = xl6HirNodeGet(&hir, returnId);
    symbolNode = xl6HirSymbolNodeGet(&hir, 7);
    baselineOk =
         xl6HirVerifyModule(&hir, &diags) &&
         diags.count == 0 &&
         hir.root != XL6_HIR_NODE_INVALID &&
         hir.nodeCount == 10 &&
         hir.stats.hirNodeCount == 10 &&
         hir.stats.symbolBindingCount == 1 &&
         hir.stats.resolvedCallCount == 1 &&
         hir.stats.lvaluePlanCount == 2 &&
         hir.stats.ownershipPlanCount == 5 &&
         hir.stats.exitPlanCount == 1 &&
         declId != XL6_HIR_NODE_INVALID &&
         symbolNode != NULL &&
         symbolNode->source == decl &&
         xl6HirSourceNodeGet(&hir, decl) == symbolNode &&
         xl6HirSymbolNodeGet(&hir, 99) == NULL &&
         valueId != XL6_HIR_NODE_INVALID &&
         valueNode != NULL &&
         valueNode->type != XL6_HIR_TYPE_INVALID &&
         valueNode->valueCategory == XL6_HIR_VALUE_RVALUE &&
         callNode != NULL &&
         callNode->callKind == XL6_SEM_CALL_DIRECT &&
         callNode->targetSymbol == 7 &&
         assignNode != NULL &&
         assignNode->lvalueKind == XL6_SEM_LVALUE_LOCAL &&
         returnHirNode != NULL &&
         returnHirNode->exitKind == XL6_SEM_EXIT_RETURN &&
         returnHirNode->scopeDepth == 3 &&
         returnHirNode->targetScopeDepth == 1 &&
         returnHirNode->region == 3 &&
         returnHirNode->targetRegion == 2;
    if ( !baselineOk ) {
        fprintf(
            stderr,
            "HIR baseline failed: build=%d verify=%d diags=%lu "
            "nodes=%lu/%lu symbols=%lu calls=%lu lvalues=%lu owners=%lu exits=%lu "
            "decl=%lu value=%lu call=%lu assign=%lu return=%lu diag=%s\n",
            ok,
            xl6HirVerifyModule(&hir, NULL),
            (unsigned long)diags.count,
            (unsigned long)hir.nodeCount,
            (unsigned long)hir.stats.hirNodeCount,
            (unsigned long)hir.stats.symbolBindingCount,
            (unsigned long)hir.stats.resolvedCallCount,
            (unsigned long)hir.stats.lvaluePlanCount,
            (unsigned long)hir.stats.ownershipPlanCount,
            (unsigned long)hir.stats.exitPlanCount,
            (unsigned long)declId,
            (unsigned long)valueId,
            (unsigned long)callId,
            (unsigned long)assignId,
            (unsigned long)returnId,
            diags.count > 0 && diags.items[0].message != NULL
                ? diags.items[0].message
                : "<none>");
    }
    ok = ok && baselineOk;

    /* verifier 必须拒绝 arena、索引、类型和统计量的任何局部损坏。 */
    savedType = hir.nodes[(size_t)valueId - 1].type;
    hir.nodes[(size_t)valueId - 1].type = (XL6_HirTypeId)(hir.types.count + 1);
    ok = ok && !xl6HirVerifyModule(&hir, NULL);
    hir.nodes[(size_t)valueId - 1].type = savedType;

    savedStorageType = hir.nodes[(size_t)valueId - 1].storageType;
    hir.nodes[(size_t)valueId - 1].storageType = XL6_HIR_TYPE_INVALID;
    ok = ok && !xl6HirVerifyModule(&hir, NULL);
    hir.nodes[(size_t)valueId - 1].storageType = savedStorageType;

    savedChild = hir.children[hir.nodes[(size_t)hir.root - 1].firstChild];
    hir.children[hir.nodes[(size_t)hir.root - 1].firstChild] = XL6_HIR_NODE_INVALID;
    ok = ok && !xl6HirVerifyModule(&hir, NULL);
    hir.children[hir.nodes[(size_t)hir.root - 1].firstChild] = savedChild;

    savedSourceKind = hir.nodes[(size_t)valueId - 1].sourceKind;
    hir.nodes[(size_t)valueId - 1].sourceKind = XL6_AST_STRING;
    ok = ok && !xl6HirVerifyModule(&hir, NULL);
    hir.nodes[(size_t)valueId - 1].sourceKind = savedSourceKind;

    savedSymbolNode = hir.symbolNodes[7];
    hir.symbolNodes[7] = XL6_HIR_NODE_INVALID;
    ok = ok && !xl6HirVerifyModule(&hir, NULL);
    hir.symbolNodes[7] = savedSymbolNode;

    savedResolvedCallCount = hir.stats.resolvedCallCount;
    hir.stats.resolvedCallCount += 1;
    ok = ok && !xl6HirVerifyModule(&hir, NULL);
    hir.stats.resolvedCallCount = savedResolvedCallCount;
    ok = ok && xl6HirVerifyModule(&hir, NULL);

    xl6HirModuleFree(&hir);
    xl6AstFree(root);
    xrtFree(diags.items);
    if ( !ok ) {
        fprintf(stderr, "HIR module test failed\n");
        return 1;
    }
    puts("HIR module test ok");
    return 0;
}
