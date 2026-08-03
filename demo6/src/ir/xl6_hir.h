#ifndef XL6_HIR_H
#define XL6_HIR_H

#include "src/ast/xl6_ast.h"
#include "src/diag/xl6_diag.h"
#include "src/ir/xl6_hir_type.h"

typedef uint32 XL6_HirNodeId;

#define XL6_HIR_NODE_INVALID 0u

typedef enum XL6_HirNodeKind {
    XL6_HIR_NODE_MODULE = 1,
    XL6_HIR_NODE_LIST,
    XL6_HIR_NODE_IMPORT,
    XL6_HIR_NODE_FUNC_DECL,
    XL6_HIR_NODE_TYPE_DECL,
    XL6_HIR_NODE_ENUM_DECL,
    XL6_HIR_NODE_ENUM_VARIANT,
    XL6_HIR_NODE_FIELD_DECL,
    XL6_HIR_NODE_PARAM_DECL,
    XL6_HIR_NODE_VALUE_DECL,
    XL6_HIR_NODE_BLOCK,
    XL6_HIR_NODE_RETURN,
    XL6_HIR_NODE_YIELD,
    XL6_HIR_NODE_THROW,
    XL6_HIR_NODE_ASSERT,
    XL6_HIR_NODE_BREAK,
    XL6_HIR_NODE_CONTINUE,
    XL6_HIR_NODE_IF,
    XL6_HIR_NODE_LOOP,
    XL6_HIR_NODE_TRY,
    XL6_HIR_NODE_MATCH,
    XL6_HIR_NODE_MATCH_CASE,
    XL6_HIR_NODE_PATTERN,
    XL6_HIR_NODE_NAME,
    XL6_HIR_NODE_LITERAL,
    XL6_HIR_NODE_CALL,
    XL6_HIR_NODE_MEMBER,
    XL6_HIR_NODE_INDEX,
    XL6_HIR_NODE_ASSIGN,
    XL6_HIR_NODE_CONDITIONAL,
    XL6_HIR_NODE_BINARY,
    XL6_HIR_NODE_UNARY,
    XL6_HIR_NODE_AWAIT,
    XL6_HIR_NODE_COLLECTION,
    XL6_HIR_NODE_OTHER
} XL6_HirNodeKind;

typedef enum XL6_HirValueCategory {
    XL6_HIR_VALUE_NONE = 0,
    XL6_HIR_VALUE_RVALUE,
    XL6_HIR_VALUE_LVALUE
} XL6_HirValueCategory;

typedef enum XL6_HirOwnership {
    XL6_HIR_OWNERSHIP_NONE = 0,
    XL6_HIR_OWNERSHIP_COPY,
    XL6_HIR_OWNERSHIP_BORROW,
    XL6_HIR_OWNERSHIP_MOVE,
    XL6_HIR_OWNERSHIP_SHARED
} XL6_HirOwnership;

typedef struct XL6_HirNode {
    XL6_HirNodeKind kind;
    XL6_AstKind sourceKind;
    unsigned int flags;
    /*
        type 是控制流位置上的语义类型，storageType 是值在 ABI 中的真实存储类型。
        例如 var 在 `is bool` 分支内的 type 为 bool，但 storageType 始终为 var。
    */
    XL6_HirTypeId type;
    XL6_HirTypeId storageType;
    XL6_SymbolId symbol;
    XL6_SymbolId targetSymbol;
    XL6_SemCallKind callKind;
    XL6_SemLvalueKind lvalueKind;
    XL6_SemExitKind exitKind;
    uint32 scopeDepth;
    uint32 targetScopeDepth;
    uint32 region;
    uint32 targetRegion;
    XL6_HirValueCategory valueCategory;
    XL6_HirOwnership ownership;
    int ownershipPlanned;
    size_t firstChild;
    size_t childCount;
    const XL6_AstNode* source;
} XL6_HirNode;

typedef struct XL6_HirSourceIndexEntry {
    const XL6_AstNode* source;
    XL6_HirNodeId node;
} XL6_HirSourceIndexEntry;

typedef struct XL6_HirStats {
    size_t nodeCount;
    size_t valueNodeCount;
    size_t typedValueNodeCount;
    size_t typeCount;
    size_t symbolBindingCount;
    size_t resolvedCallCount;
    size_t lvaluePlanCount;
    size_t ownershipPlanCount;
    size_t exitPlanCount;
    size_t hirNodeCount;
} XL6_HirStats;

typedef struct XL6_HirModule {
    const XL6_AstNode* sourceRoot;
    XL6_HirTypePool types;
    XL6_HirNode* nodes;
    size_t nodeCount;
    size_t nodeCapacity;
    XL6_HirSourceIndexEntry* sourceIndex;
    size_t sourceIndexCount;
    size_t sourceIndexCapacity;
    XL6_HirNodeId* symbolNodes;
    size_t symbolNodeCapacity;
    XL6_HirNodeId* children;
    size_t childCount;
    size_t childCapacity;
    XL6_HirNodeId root;
    XL6_HirStats stats;
    int ready;
} XL6_HirModule;

void xl6HirModuleInit(XL6_HirModule* module);
void xl6HirModuleFree(XL6_HirModule* module);
int xl6HirBuildModule(
    XL6_HirModule* module,
    XL6_AstNode* root,
    XL6_DiagList* diags
);
int xl6HirVerifyModule(
    const XL6_HirModule* module,
    XL6_DiagList* diags
);
XL6_HirTypeId xl6HirSourceType(
    const XL6_HirModule* module,
    const XL6_AstNode* source
);
XL6_HirTypeId xl6HirSourceStorageType(
    const XL6_HirModule* module,
    const XL6_AstNode* source
);
XL6_HirNodeId xl6HirSourceNode(
    const XL6_HirModule* module,
    const XL6_AstNode* source
);
const XL6_HirNode* xl6HirNodeGet(
    const XL6_HirModule* module,
    XL6_HirNodeId nodeId
);
const XL6_HirNode* xl6HirSourceNodeGet(
    const XL6_HirModule* module,
    const XL6_AstNode* source
);
const XL6_HirNode* xl6HirSymbolNodeGet(
    const XL6_HirModule* module,
    XL6_SymbolId symbol
);
int xl6HirFinalizeModule(XL6_AstNode* root, XL6_DiagList* diags, XL6_HirStats* stats);

#endif
