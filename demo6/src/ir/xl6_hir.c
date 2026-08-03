#include "src/ir/xl6_hir.h"

#include <stdio.h>
#include <string.h>

static void xl6HirTypeDiag(
    XL6_DiagList* diags,
    const XL6_AstNode* node,
    const XL6_AstNode* type,
    const char* message
)
{
    str detail;

    detail = xrtFormat(
        "%s: node=%s '%s', type='%s', typeId=%u, abiId=%llu",
        message != NULL ? message : "typed HIR type error",
        node != NULL ? xl6AstKindName(node->kind) : "<null>",
        node != NULL && node->text != NULL ? node->text : "",
        type != NULL && type->text != NULL ? type->text : "<missing>",
        type != NULL ? (unsigned int)type->semanticTypeId : 0u,
        type != NULL ? (unsigned long long)type->semanticTypeAbiId : 0ull
    );
    xl6DiagAdd(
        diags,
        node != NULL ? node->line : 0,
        node != NULL ? node->column : 0,
        detail != NULL ? (const char*)detail : message
    );
    xrtFree(detail);
}

/* ------------------------------------ 模块生命周期 ------------------------------------ */

void xl6HirModuleInit(XL6_HirModule* module)
{
    if ( module == NULL ) {
        return;
    }
    memset(module, 0, sizeof(*module));
    xl6HirTypePoolInit(&module->types);
}

void xl6HirModuleFree(XL6_HirModule* module)
{
    if ( module == NULL ) {
        return;
    }
    xl6HirTypePoolFree(&module->types);
    xrtFree(module->nodes);
    xrtFree(module->sourceIndex);
    xrtFree(module->symbolNodes);
    xrtFree(module->children);
    memset(module, 0, sizeof(*module));
}

/* ------------------------------------ 源节点与符号索引 ------------------------------------ */

static size_t xl6HirSourceHash(const XL6_AstNode* source)
{
    uintptr_t value;

    value = (uintptr_t)source;
    value ^= value >> 17;
    value *= (uintptr_t)0xed5ad4bbU;
    value ^= value >> 11;
    return (size_t)value;
}

static int xl6HirSourceIndexGrow(XL6_HirModule* module, size_t capacity)
{
    XL6_HirSourceIndexEntry* oldItems;
    XL6_HirSourceIndexEntry* newItems;
    size_t oldCapacity;
    size_t i;

    newItems = (XL6_HirSourceIndexEntry*)xrtCalloc(capacity, sizeof(*newItems));
    if ( newItems == NULL ) {
        return 0;
    }
    oldItems = module->sourceIndex;
    oldCapacity = module->sourceIndexCapacity;
    module->sourceIndex = newItems;
    module->sourceIndexCapacity = capacity;
    module->sourceIndexCount = 0;
    for ( i = 0; i < oldCapacity; ++i ) {
        size_t slot;

        if ( oldItems[i].source == NULL ) {
            continue;
        }
        slot = xl6HirSourceHash(oldItems[i].source) & (capacity - 1);
        while ( newItems[slot].source != NULL ) {
            slot = (slot + 1) & (capacity - 1);
        }
        newItems[slot] = oldItems[i];
        module->sourceIndexCount += 1;
    }
    xrtFree(oldItems);
    return 1;
}

static int xl6HirSourceIndexAdd(
    XL6_HirModule* module,
    const XL6_AstNode* source,
    XL6_HirNodeId node
)
{
    size_t slot;

    if ( module->sourceIndexCapacity == 0 ) {
        if ( !xl6HirSourceIndexGrow(module, 256) ) {
            return 0;
        }
    } else if ( (module->sourceIndexCount + 1) * 4 >=
                module->sourceIndexCapacity * 3 ) {
        if ( !xl6HirSourceIndexGrow(module, module->sourceIndexCapacity * 2) ) {
            return 0;
        }
    }
    slot = xl6HirSourceHash(source) & (module->sourceIndexCapacity - 1);
    while ( module->sourceIndex[slot].source != NULL ) {
        if ( module->sourceIndex[slot].source == source ) {
            module->sourceIndex[slot].node = node;
            return 1;
        }
        slot = (slot + 1) & (module->sourceIndexCapacity - 1);
    }
    module->sourceIndex[slot].source = source;
    module->sourceIndex[slot].node = node;
    module->sourceIndexCount += 1;
    return 1;
}

static int xl6HirSymbolIndexAdd(
    XL6_HirModule* module,
    XL6_SymbolId symbol,
    XL6_HirNodeId node
)
{
    XL6_HirNodeId* newItems;
    size_t newCapacity;

    if ( symbol == XL6_SYMBOL_ID_INVALID ) {
        return 1;
    }
    if ( (size_t)symbol >= module->symbolNodeCapacity ) {
        newCapacity = module->symbolNodeCapacity == 0 ? 256 : module->symbolNodeCapacity;
        while ( newCapacity <= (size_t)symbol ) {
            if ( newCapacity > SIZE_MAX / 2 ) {
                return 0;
            }
            newCapacity *= 2;
        }
        newItems = (XL6_HirNodeId*)xrtRealloc(
            module->symbolNodes,
            newCapacity * sizeof(*newItems)
        );
        if ( newItems == NULL ) {
            return 0;
        }
        memset(
            newItems + module->symbolNodeCapacity,
            0,
            (newCapacity - module->symbolNodeCapacity) * sizeof(*newItems)
        );
        module->symbolNodes = newItems;
        module->symbolNodeCapacity = newCapacity;
    }
    /*
     * 声明总是在它的引用之前进入 HIR。保留第一次绑定，避免后续引用覆盖声明。
     */
    if ( module->symbolNodes[symbol] == XL6_HIR_NODE_INVALID ) {
        module->symbolNodes[symbol] = node;
    }
    return 1;
}

static XL6_HirNodeKind xl6HirNodeKindFromAst(XL6_AstKind kind)
{
    switch ( kind ) {
    case XL6_AST_MODULE: return XL6_HIR_NODE_MODULE;
    case XL6_AST_LIST:
    case XL6_AST_RETURN_TYPES:
    case XL6_AST_TYPE_PARAMS:
    case XL6_AST_FOR_TARGETS:
        return XL6_HIR_NODE_LIST;
    case XL6_AST_IMPORT: return XL6_HIR_NODE_IMPORT;
    case XL6_AST_FUNC:
    case XL6_AST_ASYNC_FUNC:
    case XL6_AST_ANON_FUNC:
    case XL6_AST_EXTERN_FUNC:
    case XL6_AST_EXPORT_FUNC:
    case XL6_AST_GET:
    case XL6_AST_LET:
    case XL6_AST_CREATE:
    case XL6_AST_NATIVE_EXTERN_SPECIAL:
    case XL6_AST_NATIVE_EXTERN_FUNC:
        return XL6_HIR_NODE_FUNC_DECL;
    case XL6_AST_CLASS:
    case XL6_AST_PROTOCOL:
    case XL6_AST_UNION:
    case XL6_AST_TYPE_ALIAS:
    case XL6_AST_EXTERN_TYPE_ALIAS:
        return XL6_HIR_NODE_TYPE_DECL;
    case XL6_AST_ENUM:
        return XL6_HIR_NODE_ENUM_DECL;
    case XL6_AST_ENUM_VARIANT:
        return XL6_HIR_NODE_ENUM_VARIANT;
    case XL6_AST_FIELD:
    case XL6_AST_FIELD_ATTR:
        return XL6_HIR_NODE_FIELD_DECL;
    case XL6_AST_PARAM:
    case XL6_AST_GENERIC_CONSTRAINT:
        return XL6_HIR_NODE_PARAM_DECL;
    case XL6_AST_VAR_DECL:
    case XL6_AST_MULTI_VAR_DECL:
    case XL6_AST_CONST_DECL:
    case XL6_AST_EXTERN_VAR:
    case XL6_AST_EXPORT_VAR:
    case XL6_AST_EXPORT_CONST:
        return XL6_HIR_NODE_VALUE_DECL;
    case XL6_AST_BLOCK: return XL6_HIR_NODE_BLOCK;
    case XL6_AST_RETURN: return XL6_HIR_NODE_RETURN;
    case XL6_AST_YIELD: return XL6_HIR_NODE_YIELD;
    case XL6_AST_THROW: return XL6_HIR_NODE_THROW;
    case XL6_AST_ASSERT: return XL6_HIR_NODE_ASSERT;
    case XL6_AST_BREAK: return XL6_HIR_NODE_BREAK;
    case XL6_AST_CONTINUE: return XL6_HIR_NODE_CONTINUE;
    case XL6_AST_IF:
    case XL6_AST_ELSEIF:
        return XL6_HIR_NODE_IF;
    case XL6_AST_WHILE:
    case XL6_AST_DO_WHILE:
    case XL6_AST_FOR:
        return XL6_HIR_NODE_LOOP;
    case XL6_AST_TRY:
    case XL6_AST_CATCH:
        return XL6_HIR_NODE_TRY;
    case XL6_AST_MATCH:
        return XL6_HIR_NODE_MATCH;
    case XL6_AST_MATCH_CASE:
        return XL6_HIR_NODE_MATCH_CASE;
    case XL6_AST_PATTERN_VARIANT:
    case XL6_AST_PATTERN_LITERAL:
    case XL6_AST_PATTERN_ELSE:
        return XL6_HIR_NODE_PATTERN;
    case XL6_AST_NAME: return XL6_HIR_NODE_NAME;
    case XL6_AST_INT:
    case XL6_AST_FLOAT:
    case XL6_AST_STRING:
    case XL6_AST_INTERP_STRING:
    case XL6_AST_INTERP_TEXT:
    case XL6_AST_TIME:
    case XL6_AST_BOOL:
    case XL6_AST_NULL:
        return XL6_HIR_NODE_LITERAL;
    case XL6_AST_CALL:
    case XL6_AST_GENERIC_INSTANT:
        return XL6_HIR_NODE_CALL;
    case XL6_AST_MEMBER:
    case XL6_AST_SAFE_MEMBER:
        return XL6_HIR_NODE_MEMBER;
    case XL6_AST_INDEX:
    case XL6_AST_SAFE_INDEX:
        return XL6_HIR_NODE_INDEX;
    case XL6_AST_ASSIGN:
    case XL6_AST_MULTI_ASSIGN:
        return XL6_HIR_NODE_ASSIGN;
    case XL6_AST_CONDITIONAL: return XL6_HIR_NODE_CONDITIONAL;
    case XL6_AST_BINARY:
    case XL6_AST_TYPE_IS:
    case XL6_AST_TYPE_AS:
        return XL6_HIR_NODE_BINARY;
    case XL6_AST_UNARY: return XL6_HIR_NODE_UNARY;
    case XL6_AST_AWAIT: return XL6_HIR_NODE_AWAIT;
    case XL6_AST_ARRAY_LITERAL:
    case XL6_AST_LIST_LITERAL:
    case XL6_AST_SET_LITERAL:
    case XL6_AST_DICT_LITERAL:
    case XL6_AST_TYPED_LITERAL:
    case XL6_AST_KEY_VALUE:
        return XL6_HIR_NODE_COLLECTION;
    default:
        return XL6_HIR_NODE_OTHER;
    }
}

static XL6_HirOwnership xl6HirOwnershipFromSemantic(
    XL6_SemOwnershipAction ownership
)
{
    switch ( ownership ) {
    case XL6_SEM_OWNERSHIP_COPY:
        return XL6_HIR_OWNERSHIP_COPY;
    case XL6_SEM_OWNERSHIP_BORROW:
        return XL6_HIR_OWNERSHIP_BORROW;
    case XL6_SEM_OWNERSHIP_MOVE:
        return XL6_HIR_OWNERSHIP_MOVE;
    case XL6_SEM_OWNERSHIP_SHARE:
        return XL6_HIR_OWNERSHIP_SHARED;
    default:
        return XL6_HIR_OWNERSHIP_NONE;
    }
}

static XL6_HirNodeId xl6HirModuleAddNode(
    XL6_HirModule* module,
    const XL6_AstNode* source
)
{
    XL6_HirNode* newItems;
    XL6_HirNode* item;

    if ( module == NULL || source == NULL ||
         module->nodeCount >= (size_t)UINT32_MAX ) {
        return XL6_HIR_NODE_INVALID;
    }
    if ( module->nodeCount == module->nodeCapacity ) {
        size_t newCapacity = module->nodeCapacity == 0 ? 128 : module->nodeCapacity * 2;

        newItems = (XL6_HirNode*)xrtRealloc(
            module->nodes,
            newCapacity * sizeof(*newItems)
        );
        if ( newItems == NULL ) {
            return XL6_HIR_NODE_INVALID;
        }
        module->nodes = newItems;
        module->nodeCapacity = newCapacity;
    }
    item = &module->nodes[module->nodeCount];
    memset(item, 0, sizeof(*item));
    item->kind = xl6HirNodeKindFromAst(source->kind);
    item->sourceKind = source->kind;
    item->flags = source->flags;
    item->symbol = source->semanticSymbolId;
    item->targetSymbol = source->semanticTargetSymbolId;
    item->callKind = source->semanticCallKind;
    item->lvalueKind = source->semanticLvalueKind;
    item->exitKind = source->semanticExitKind;
    item->scopeDepth = source->semanticScopeDepth;
    item->targetScopeDepth = source->semanticTargetScopeDepth;
    item->region = source->semanticRegionId;
    item->targetRegion = source->semanticTargetRegionId;
    item->ownership = xl6HirOwnershipFromSemantic(source->semanticOwnership);
    item->ownershipPlanned =
        source->semanticOwnership != XL6_SEM_OWNERSHIP_NONE;
    item->source = source;
    module->nodeCount += 1;
    if ( !xl6HirSourceIndexAdd(
            module,
            source,
            (XL6_HirNodeId)module->nodeCount
         ) ||
         !xl6HirSymbolIndexAdd(
            module,
            source->semanticSymbolId,
            (XL6_HirNodeId)module->nodeCount
         ) ) {
        module->nodeCount -= 1;
        return XL6_HIR_NODE_INVALID;
    }
    return (XL6_HirNodeId)module->nodeCount;
}

static int xl6HirModuleAppendChildren(
    XL6_HirModule* module,
    const XL6_HirNodeId* children,
    size_t childCount,
    size_t* outFirstChild
)
{
    XL6_HirNodeId* newItems;

    *outFirstChild = module->childCount;
    if ( childCount == 0 ) {
        return 1;
    }
    if ( module->childCount + childCount > module->childCapacity ) {
        size_t newCapacity = module->childCapacity == 0 ? 256 : module->childCapacity;

        while ( newCapacity < module->childCount + childCount ) {
            newCapacity *= 2;
        }
        newItems = (XL6_HirNodeId*)xrtRealloc(
            module->children,
            newCapacity * sizeof(*newItems)
        );
        if ( newItems == NULL ) {
            return 0;
        }
        module->children = newItems;
        module->childCapacity = newCapacity;
    }
    memcpy(
        module->children + module->childCount,
        children,
        childCount * sizeof(*children)
    );
    module->childCount += childCount;
    return 1;
}

/* 这些表达式在任何合法上下文中都必须产生且只产生一个静态类型。 */
static int xl6HirNodeRequiresType(
    const XL6_AstNode* node,
    const XL6_AstNode* parent,
    size_t childIndex
)
{
    if ( node == NULL ) {
        return 0;
    }
    if ( parent != NULL && parent->kind == XL6_AST_CALL && childIndex == 0 &&
         (node->kind == XL6_AST_MEMBER ||
           node->kind == XL6_AST_GENERIC_INSTANT) ) {
        return 0;
    }
    if ( parent != NULL && parent->kind == XL6_AST_MEMBER && childIndex == 0 &&
         node->kind == XL6_AST_GENERIC_INSTANT ) {
        return 0;
    }
    /*
        数值 enum 的归一化值属于声明元数据，不是运行期表达式。
        语义阶段已经验证其常量性、唯一性和底层类型范围，HIR 不应要求
        这类声明子节点再携带一次表达式静态类型。
    */
    if ( parent != NULL && parent->kind == XL6_AST_ENUM_VARIANT &&
         node->kind == XL6_AST_INT ) {
        return 0;
    }
    if ( parent != NULL &&
         (parent->kind == XL6_AST_INDEX ||
          parent->kind == XL6_AST_SAFE_INDEX) &&
         childIndex == 0 &&
         node->kind == XL6_AST_MEMBER &&
         (node->semanticCallKind == XL6_SEM_CALL_PROPERTY_GET ||
          parent->semanticCallKind == XL6_SEM_CALL_PROPERTY_GET ||
          parent->semanticCallKind == XL6_SEM_CALL_PROPERTY_SET ||
          parent->semanticLvalueKind == XL6_SEM_LVALUE_PROPERTY) ) {
        return 0;
    }
    if ( node->kind == XL6_AST_CALL &&
         parent != NULL && parent->kind == XL6_AST_EXPR_STMT ) {
        return 0;
    }
    switch ( node->kind ) {
    case XL6_AST_INT:
    case XL6_AST_FLOAT:
    case XL6_AST_STRING:
    case XL6_AST_INTERP_STRING:
    case XL6_AST_TIME:
    case XL6_AST_BOOL:
    case XL6_AST_NULL:
    case XL6_AST_CALL:
    case XL6_AST_MEMBER:
    case XL6_AST_SAFE_MEMBER:
    case XL6_AST_GENERIC_INSTANT:
    case XL6_AST_ANON_FUNC:
    case XL6_AST_INDEX:
    case XL6_AST_SAFE_INDEX:
    case XL6_AST_CONDITIONAL:
    case XL6_AST_BINARY:
    case XL6_AST_TYPE_IS:
    case XL6_AST_TYPE_AS:
    case XL6_AST_UNARY:
    case XL6_AST_AWAIT:
    case XL6_AST_MATCH:
    case XL6_AST_ARRAY_LITERAL:
    case XL6_AST_LIST_LITERAL:
    case XL6_AST_SET_LITERAL:
    case XL6_AST_DICT_LITERAL:
    case XL6_AST_TYPED_LITERAL:
        return node->kind != XL6_AST_MATCH ||
               (node->flags & XL6_AST_FLAG_MATCH_EXPR) != 0;
    default:
        return 0;
    }
}

static const XL6_AstNode* xl6HirDeclaredStorageType(const XL6_AstNode* node)
{
    const XL6_AstNode* typeNode;

    if ( node == NULL || node->childCount == 0 ) {
        return NULL;
    }
    typeNode = node->children[0];
    if ( typeNode == NULL || typeNode->kind != XL6_AST_TYPE ) {
        return NULL;
    }
    /* auto 在语义完成后由节点的规范推断类型承担真实存储表示。 */
    if ( typeNode->text != NULL && strcmp(typeNode->text, "auto") == 0 ) {
        return NULL;
    }
    switch ( node->kind ) {
    case XL6_AST_VAR_DECL:
    case XL6_AST_CONST_DECL:
    case XL6_AST_PARAM:
    case XL6_AST_FIELD:
    case XL6_AST_EXTERN_VAR:
    case XL6_AST_EXPORT_VAR:
    case XL6_AST_EXPORT_CONST:
        return typeNode;
    default:
        return NULL;
    }
}

/* ------------------------------------ AST 到规范类型绑定 ------------------------------------ */

static XL6_HirNodeId xl6HirBuildNode(
    XL6_HirModule* module,
    XL6_AstNode* node,
    const XL6_AstNode* parent,
    size_t childIndex,
    XL6_DiagList* diags,
    int* outOk
)
{
    XL6_HirNodeId nodeId;
    XL6_HirNodeId* childIds;
    XL6_HirNode* hirNode;
    XL6_HirTypeId sourceType;
    size_t firstChild;
    size_t i;
    int ok;

    if ( node == NULL ) {
        return XL6_HIR_NODE_INVALID;
    }
    nodeId = xl6HirModuleAddNode(module, node);
    if ( nodeId == XL6_HIR_NODE_INVALID ) {
        xl6DiagAdd(diags, node->line, node->column, "typed HIR node arena is out of memory");
        *outOk = 0;
        return XL6_HIR_NODE_INVALID;
    }
    module->stats.nodeCount += 1;
    ok = 1;
    sourceType = XL6_HIR_TYPE_INVALID;
    if ( node->kind == XL6_AST_TYPE &&
         (node->text == NULL || strcmp(node->text, "auto") != 0) &&
         !(parent != NULL &&
           parent->kind == XL6_AST_GENERIC_CONSTRAINT &&
           node->semanticTypeId == XL6_TYPE_ID_INVALID) ) {
        sourceType = xl6HirTypeInternAst(&module->types, module->sourceRoot, node);
        if ( sourceType == XL6_HIR_TYPE_INVALID ) {
            xl6HirTypeDiag(diags, node, node, "typed HIR cannot intern source type");
            ok = 0;
        }
    }
    {
        int requiresType = xl6HirNodeRequiresType(node, parent, childIndex);
        XL6_HirTypeId typeId;

        if ( requiresType ) {
            module->stats.valueNodeCount += 1;
        }
        if ( node->inferredType != NULL &&
             node->inferredType->kind == XL6_AST_TYPE &&
             node->inferredType->text != NULL &&
             node->inferredType->text[0] != '\0' ) {
            typeId = xl6HirTypeInternAst(
                &module->types,
                module->sourceRoot,
                node->inferredType
            );
            if ( typeId == XL6_HIR_TYPE_INVALID ) {
                xl6HirTypeDiag(
                    diags,
                    node,
                    node->inferredType,
                    "typed HIR cannot bind expression type"
                );
                ok = 0;
            } else {
                sourceType = typeId;
            }
        } else if ( node->kind == XL6_AST_NAME &&
                    node->semanticSymbolId != XL6_SYMBOL_ID_INVALID ) {
            const XL6_HirNode* declaration = xl6HirSymbolNodeGet(
                module,
                node->semanticSymbolId
            );

            /*
                赋值目标等左值位置不需要再次求值，语义阶段可能不会重复写入
                inferredType；名称仍必须从已解析符号继承完整类型，不能留给后端猜测。
            */
            if ( declaration != NULL && declaration->source != node ) {
                sourceType = declaration->type != XL6_HIR_TYPE_INVALID
                    ? declaration->type
                    : declaration->storageType;
            }
        }
        if ( requiresType && sourceType != XL6_HIR_TYPE_INVALID ) {
            module->stats.typedValueNodeCount += 1;
        } else if ( requiresType ) {
            char message[416];
            const XL6_AstNode* callee =
                node->kind == XL6_AST_CALL && node->childCount > 0
                    ? node->children[0]
                    : NULL;

            snprintf(
                message,
                sizeof(message),
                "typed HIR value expression '%s' text='%s' under '%s' at %d:%d is missing its static type; callee='%s', callKind=%d, targetSymbol=%llu",
                xl6AstKindName(node->kind),
                node->text != NULL ? node->text : "",
                parent != NULL ? xl6AstKindName(parent->kind) : "<root>",
                parent != NULL ? parent->line : 0,
                parent != NULL ? parent->column : 0,
                callee != NULL && callee->text != NULL ? callee->text : "",
                (int)node->semanticCallKind,
                (unsigned long long)node->semanticTargetSymbolId
            );
            xl6DiagAdd(diags, node->line, node->column, message);
            ok = 0;
        }
    }

    childIds = node->childCount > 0
        ? (XL6_HirNodeId*)xrtCalloc(node->childCount, sizeof(*childIds))
        : NULL;
    if ( node->childCount > 0 && childIds == NULL ) {
        xl6DiagAdd(diags, node->line, node->column, "typed HIR child list is out of memory");
        ok = 0;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( childIds == NULL ) {
            break;
        }
        childIds[i] = xl6HirBuildNode(
            module,
            node->children[i],
            node,
            i,
            diags,
            &ok
        );
        if ( node->children[i] != NULL && childIds[i] == XL6_HIR_NODE_INVALID ) {
            ok = 0;
        }
    }
    firstChild = 0;
    if ( childIds != NULL &&
         !xl6HirModuleAppendChildren(module, childIds, node->childCount, &firstChild) ) {
        xl6DiagAdd(diags, node->line, node->column, "typed HIR operand arena is out of memory");
        ok = 0;
    }
    xrtFree(childIds);

    hirNode = &module->nodes[(size_t)nodeId - 1];
    hirNode->type = sourceType;
    hirNode->storageType = sourceType;
    {
        const XL6_AstNode* declaredStorageType = xl6HirDeclaredStorageType(node);

        if ( declaredStorageType != NULL ) {
            XL6_HirTypeId declaredType = xl6HirTypeInternAst(
                &module->types,
                module->sourceRoot,
                declaredStorageType
            );

            if ( declaredType == XL6_HIR_TYPE_INVALID ) {
                xl6HirTypeDiag(
                    diags,
                    node,
                    declaredStorageType,
                    "typed HIR cannot intern declaration storage type"
                );
                ok = 0;
            } else {
                hirNode->storageType = declaredType;
            }
        }
    }
    if ( node->kind == XL6_AST_NAME &&
         node->semanticSymbolId != XL6_SYMBOL_ID_INVALID ) {
        const XL6_HirNode* declaration = xl6HirSymbolNodeGet(
            module,
            node->semanticSymbolId
        );

        /*
            名称在当前控制流位置可能已被收窄，但生成代码必须服从声明的存储表示。
            声明节点在引用进入 HIR 前已经完成，因此这里可以稳定取得其类型。
        */
        if ( declaration != NULL &&
             declaration != hirNode &&
             declaration->storageType != XL6_HIR_TYPE_INVALID ) {
            hirNode->storageType = declaration->storageType;
        }
    }
    hirNode->firstChild = firstChild;
    hirNode->childCount = node->childCount;
    if ( node->semanticSymbolId != XL6_SYMBOL_ID_INVALID ) {
        module->stats.symbolBindingCount += 1;
    }
    if ( node->semanticCallKind != XL6_SEM_CALL_NONE ) {
        module->stats.resolvedCallCount += 1;
    }
    if ( node->semanticLvalueKind != XL6_SEM_LVALUE_NONE ) {
        module->stats.lvaluePlanCount += 1;
    }
    if ( node->semanticOwnership != XL6_SEM_OWNERSHIP_NONE ) {
        module->stats.ownershipPlanCount += 1;
    }
    if ( node->semanticExitKind != XL6_SEM_EXIT_NONE ) {
        module->stats.exitPlanCount += 1;
    }
    if ( node->kind == XL6_AST_NAME ||
         node->kind == XL6_AST_MEMBER ||
         node->kind == XL6_AST_INDEX ) {
        hirNode->valueCategory = XL6_HIR_VALUE_LVALUE;
        if ( hirNode->ownership == XL6_HIR_OWNERSHIP_NONE ) {
            hirNode->ownership = XL6_HIR_OWNERSHIP_BORROW;
        }
    } else if ( sourceType != XL6_HIR_TYPE_INVALID ) {
        hirNode->valueCategory = XL6_HIR_VALUE_RVALUE;
        if ( hirNode->ownership == XL6_HIR_OWNERSHIP_NONE ) {
            hirNode->ownership = XL6_HIR_OWNERSHIP_COPY;
        }
    }
    if ( !ok ) {
        *outOk = 0;
    }
    return nodeId;
}

/* ------------------------------------ HIR 完整性验证 ------------------------------------ */

static int xl6HirVerifyError(
    XL6_DiagList* diags,
    const XL6_AstNode* source,
    const char* message
)
{
    xl6DiagAdd(
        diags,
        source != NULL ? source->line : 0,
        source != NULL ? source->column : 0,
        message != NULL ? message : "typed HIR verification failed"
    );
    return 0;
}

static int xl6HirVerifyTypeId(const XL6_HirModule* module, XL6_HirTypeId typeId)
{
    return typeId == XL6_HIR_TYPE_INVALID ||
           (module != NULL && (size_t)typeId <= module->types.count);
}

int xl6HirVerifyModule(const XL6_HirModule* module, XL6_DiagList* diags)
{
    uint32* incoming;
    size_t sourceIndexCount;
    size_t valueNodeCount;
    size_t typedValueNodeCount;
    size_t symbolBindingCount;
    size_t resolvedCallCount;
    size_t lvaluePlanCount;
    size_t ownershipPlanCount;
    size_t exitPlanCount;
    size_t i;
    int ok;

    if ( module == NULL || module->sourceRoot == NULL ) {
        return xl6HirVerifyError(diags, NULL, "typed HIR module has no source root");
    }
    if ( module->nodeCount == 0 || module->nodes == NULL ||
         module->nodeCount > module->nodeCapacity ) {
        return xl6HirVerifyError(diags, module->sourceRoot, "typed HIR node arena is inconsistent");
    }
    if ( module->root == XL6_HIR_NODE_INVALID ||
         (size_t)module->root > module->nodeCount ) {
        return xl6HirVerifyError(diags, module->sourceRoot, "typed HIR root node is invalid");
    }
    if ( module->childCount > module->childCapacity ||
         (module->childCount > 0 && module->children == NULL) ) {
        return xl6HirVerifyError(diags, module->sourceRoot, "typed HIR child arena is inconsistent");
    }
    if ( module->types.count > module->types.capacity ||
         (module->types.count > 0 && module->types.items == NULL) ) {
        return xl6HirVerifyError(diags, module->sourceRoot, "typed HIR type pool is inconsistent");
    }
    for ( i = 0; i < module->types.count; ++i ) {
        const XL6_HirType* type = &module->types.items[i];
        size_t j;

        if ( type->kind < XL6_HIR_TYPE_BUILTIN ||
             type->kind > XL6_HIR_TYPE_TAGGED_ENUM ||
             (type->paramCount > 0 && type->params == NULL) ||
             (type->returnCount > 0 && type->returns == NULL) ) {
            return xl6HirVerifyError(diags, module->sourceRoot, "typed HIR contains an invalid canonical type");
        }
        for ( j = 0; j < type->paramCount; ++j ) {
            if ( !xl6HirVerifyTypeId(module, type->params[j]) ||
                 type->params[j] == XL6_HIR_TYPE_INVALID ) {
                return xl6HirVerifyError(diags, module->sourceRoot, "typed HIR type parameter id is invalid");
            }
        }
        for ( j = 0; j < type->returnCount; ++j ) {
            if ( !xl6HirVerifyTypeId(module, type->returns[j]) ||
                 type->returns[j] == XL6_HIR_TYPE_INVALID ) {
                return xl6HirVerifyError(diags, module->sourceRoot, "typed HIR return type id is invalid");
            }
        }
    }

    incoming = (uint32*)xrtCalloc(module->nodeCount, sizeof(*incoming));
    if ( incoming == NULL ) {
        return xl6HirVerifyError(diags, module->sourceRoot, "typed HIR verifier is out of memory");
    }
    ok = 1;
    valueNodeCount = 0;
    typedValueNodeCount = 0;
    symbolBindingCount = 0;
    resolvedCallCount = 0;
    lvaluePlanCount = 0;
    ownershipPlanCount = 0;
    exitPlanCount = 0;

    for ( i = 0; ok && i < module->nodeCount; ++i ) {
        const XL6_HirNode* node = &module->nodes[i];
        size_t j;

        if ( node->source == NULL ||
             node->kind < XL6_HIR_NODE_MODULE ||
             node->kind > XL6_HIR_NODE_OTHER ||
             node->sourceKind != node->source->kind ||
             node->kind != xl6HirNodeKindFromAst(node->source->kind) ) {
            ok = xl6HirVerifyError(diags, node->source, "typed HIR node/source identity is inconsistent");
            break;
        }
        if ( !xl6HirVerifyTypeId(module, node->type) ||
             !xl6HirVerifyTypeId(module, node->storageType) ) {
            ok = xl6HirVerifyError(diags, node->source, "typed HIR node contains an invalid type id");
            break;
        }
        if ( node->callKind < XL6_SEM_CALL_NONE ||
             node->callKind > XL6_SEM_CALL_BASE_CONSTRUCTOR ||
             node->lvalueKind < XL6_SEM_LVALUE_NONE ||
             node->lvalueKind > XL6_SEM_LVALUE_DISCARD ||
             node->exitKind < XL6_SEM_EXIT_NONE ||
             node->exitKind > XL6_SEM_EXIT_THROW ||
             node->valueCategory < XL6_HIR_VALUE_NONE ||
             node->valueCategory > XL6_HIR_VALUE_LVALUE ||
             node->ownership < XL6_HIR_OWNERSHIP_NONE ||
             node->ownership > XL6_HIR_OWNERSHIP_SHARED ) {
            ok = xl6HirVerifyError(diags, node->source, "typed HIR semantic plan is invalid");
            break;
        }
        if ( node->childCount != node->source->childCount ||
             node->firstChild > module->childCount ||
             node->childCount > module->childCount - node->firstChild ) {
            ok = xl6HirVerifyError(diags, node->source, "typed HIR child slice is invalid");
            break;
        }
        for ( j = 0; j < node->childCount; ++j ) {
            XL6_HirNodeId childId = module->children[node->firstChild + j];
            const XL6_AstNode* sourceChild = node->source->children[j];
            const XL6_HirNode* child;

            if ( sourceChild == NULL ) {
                if ( childId != XL6_HIR_NODE_INVALID ) {
                    ok = xl6HirVerifyError(diags, node->source, "typed HIR null child mapping is invalid");
                }
                continue;
            }
            if ( childId == XL6_HIR_NODE_INVALID ||
                 (size_t)childId > module->nodeCount ) {
                ok = xl6HirVerifyError(diags, sourceChild, "typed HIR child node id is invalid");
                break;
            }
            child = &module->nodes[(size_t)childId - 1];
            if ( child->source != sourceChild || ++incoming[(size_t)childId - 1] != 1 ) {
                ok = xl6HirVerifyError(diags, sourceChild, "typed HIR child ownership is inconsistent");
                break;
            }
            if ( xl6HirNodeRequiresType(sourceChild, node->source, j) ) {
                valueNodeCount += 1;
                if ( child->type == XL6_HIR_TYPE_INVALID ) {
                    ok = xl6HirVerifyError(diags, sourceChild, "typed HIR value node has no canonical type");
                    break;
                }
                if ( child->storageType == XL6_HIR_TYPE_INVALID ) {
                    ok = xl6HirVerifyError(diags, sourceChild, "typed HIR value node has no canonical storage type");
                    break;
                }
                typedValueNodeCount += 1;
            }
        }
        if ( node->symbol != XL6_SYMBOL_ID_INVALID ) {
            symbolBindingCount += 1;
        }
        if ( node->callKind != XL6_SEM_CALL_NONE ) {
            resolvedCallCount += 1;
        }
        if ( node->lvalueKind != XL6_SEM_LVALUE_NONE ) {
            lvaluePlanCount += 1;
        }
        if ( node->ownershipPlanned ) {
            ownershipPlanCount += 1;
        }
        if ( node->exitKind != XL6_SEM_EXIT_NONE ) {
            exitPlanCount += 1;
        }
    }
    if ( ok && xl6HirNodeRequiresType(module->sourceRoot, NULL, 0) ) {
        valueNodeCount += 1;
        if ( module->nodes[(size_t)module->root - 1].type == XL6_HIR_TYPE_INVALID ) {
            ok = xl6HirVerifyError(diags, module->sourceRoot, "typed HIR root value has no canonical type");
        } else if ( module->nodes[(size_t)module->root - 1].storageType == XL6_HIR_TYPE_INVALID ) {
            ok = xl6HirVerifyError(diags, module->sourceRoot, "typed HIR root value has no canonical storage type");
        } else {
            typedValueNodeCount += 1;
        }
    }
    if ( ok ) {
        for ( i = 0; i < module->nodeCount; ++i ) {
            uint32 expected = ((XL6_HirNodeId)(i + 1) == module->root) ? 0u : 1u;

            if ( incoming[i] != expected ) {
                ok = xl6HirVerifyError(diags, module->nodes[i].source, "typed HIR tree contains an orphan or cycle");
                break;
            }
        }
    }
    xrtFree(incoming);
    if ( !ok ) {
        return 0;
    }

    if ( module->sourceIndexCapacity == 0 || module->sourceIndex == NULL ||
         module->sourceIndexCount != module->nodeCount ||
         module->sourceIndexCount >= module->sourceIndexCapacity ||
         (module->sourceIndexCapacity & (module->sourceIndexCapacity - 1)) != 0 ) {
        return xl6HirVerifyError(diags, module->sourceRoot, "typed HIR source index is inconsistent");
    }
    sourceIndexCount = 0;
    for ( i = 0; i < module->sourceIndexCapacity; ++i ) {
        const XL6_HirSourceIndexEntry* entry = &module->sourceIndex[i];

        if ( entry->source == NULL ) {
            continue;
        }
        sourceIndexCount += 1;
        if ( entry->node == XL6_HIR_NODE_INVALID ||
             (size_t)entry->node > module->nodeCount ||
             module->nodes[(size_t)entry->node - 1].source != entry->source ) {
            return xl6HirVerifyError(diags, entry->source, "typed HIR source index entry is invalid");
        }
    }
    if ( sourceIndexCount != module->sourceIndexCount ) {
        return xl6HirVerifyError(diags, module->sourceRoot, "typed HIR source index count is invalid");
    }
    for ( i = 0; i < module->nodeCount; ++i ) {
        const XL6_HirNode* node = &module->nodes[i];

        if ( xl6HirSourceNode(module, node->source) != (XL6_HirNodeId)(i + 1) ) {
            return xl6HirVerifyError(diags, node->source, "typed HIR source lookup is inconsistent");
        }
        if ( node->symbol != XL6_SYMBOL_ID_INVALID ) {
            const XL6_HirNode* symbolNode;

            if ( (size_t)node->symbol >= module->symbolNodeCapacity ) {
                return xl6HirVerifyError(diags, node->source, "typed HIR symbol id exceeds the symbol index");
            }
            symbolNode = xl6HirSymbolNodeGet(module, node->symbol);
            if ( symbolNode == NULL || symbolNode->symbol != node->symbol ) {
                return xl6HirVerifyError(diags, node->source, "typed HIR symbol index is inconsistent");
            }
        }
        if ( node->targetSymbol != XL6_SYMBOL_ID_INVALID ) {
            const XL6_HirNode* targetNode;

            if ( (size_t)node->targetSymbol >= module->symbolNodeCapacity ) {
                return xl6HirVerifyError(diags, node->source, "typed HIR target symbol id exceeds the symbol index");
            }
            targetNode = xl6HirSymbolNodeGet(module, node->targetSymbol);
            if ( targetNode == NULL || targetNode->symbol != node->targetSymbol ) {
                return xl6HirVerifyError(diags, node->source, "typed HIR target symbol index is inconsistent");
            }
        }
    }
    if ( module->symbolNodeCapacity > 0 && module->symbolNodes == NULL ) {
        return xl6HirVerifyError(diags, module->sourceRoot, "typed HIR symbol arena is inconsistent");
    }
    for ( i = 0; i < module->symbolNodeCapacity; ++i ) {
        XL6_HirNodeId nodeId = module->symbolNodes[i];

        if ( nodeId != XL6_HIR_NODE_INVALID &&
             ((size_t)nodeId > module->nodeCount ||
              module->nodes[(size_t)nodeId - 1].symbol != (XL6_SymbolId)i) ) {
            return xl6HirVerifyError(diags, module->sourceRoot, "typed HIR symbol index entry is invalid");
        }
    }
    if ( module->stats.nodeCount != module->nodeCount ||
         module->stats.hirNodeCount != module->nodeCount ||
         module->stats.typeCount != module->types.count ||
         module->stats.valueNodeCount != valueNodeCount ||
         module->stats.typedValueNodeCount != typedValueNodeCount ||
         module->stats.symbolBindingCount != symbolBindingCount ||
         module->stats.resolvedCallCount != resolvedCallCount ||
         module->stats.lvaluePlanCount != lvaluePlanCount ||
         module->stats.ownershipPlanCount != ownershipPlanCount ||
         module->stats.exitPlanCount != exitPlanCount ) {
        return xl6HirVerifyError(diags, module->sourceRoot, "typed HIR statistics are inconsistent");
    }
    return 1;
}

int xl6HirBuildModule(
    XL6_HirModule* module,
    XL6_AstNode* root,
    XL6_DiagList* diags
)
{
    int ok;

    if ( module == NULL ) {
        return 0;
    }
    xl6HirModuleFree(module);
    xl6HirModuleInit(module);
    module->sourceRoot = root;
    ok = root != NULL;
    if ( ok ) {
        module->root = xl6HirBuildNode(module, root, NULL, 0, diags, &ok);
        if ( module->root == XL6_HIR_NODE_INVALID ) {
            ok = 0;
        }
    }
    module->stats.typeCount = module->types.count;
    module->stats.hirNodeCount = module->nodeCount;
    if ( ok && !xl6HirVerifyModule(module, diags) ) {
        ok = 0;
    }
    module->ready = ok;
    return ok;
}

XL6_HirTypeId xl6HirSourceType(
    const XL6_HirModule* module,
    const XL6_AstNode* source
)
{
    const XL6_HirNode* node;

    if ( module == NULL || source == NULL ) {
        return XL6_HIR_TYPE_INVALID;
    }
    node = xl6HirSourceNodeGet(module, source);
    return node != NULL ? node->type : XL6_HIR_TYPE_INVALID;
}

XL6_HirTypeId xl6HirSourceStorageType(
    const XL6_HirModule* module,
    const XL6_AstNode* source
)
{
    const XL6_HirNode* node;

    node = xl6HirSourceNodeGet(module, source);
    return node != NULL ? node->storageType : XL6_HIR_TYPE_INVALID;
}

XL6_HirNodeId xl6HirSourceNode(
    const XL6_HirModule* module,
    const XL6_AstNode* source
)
{
    size_t slot;

    if ( module == NULL || source == NULL ||
         module->sourceIndexCapacity == 0 ) {
        return XL6_HIR_NODE_INVALID;
    }
    slot = xl6HirSourceHash(source) & (module->sourceIndexCapacity - 1);
    while ( module->sourceIndex[slot].source != NULL ) {
        if ( module->sourceIndex[slot].source == source ) {
            return module->sourceIndex[slot].node;
        }
        slot = (slot + 1) & (module->sourceIndexCapacity - 1);
    }
    return XL6_HIR_NODE_INVALID;
}

const XL6_HirNode* xl6HirNodeGet(
    const XL6_HirModule* module,
    XL6_HirNodeId nodeId
)
{
    size_t index;

    if ( module == NULL || nodeId == XL6_HIR_NODE_INVALID ) {
        return NULL;
    }
    index = (size_t)nodeId - 1;
    return index < module->nodeCount ? &module->nodes[index] : NULL;
}

const XL6_HirNode* xl6HirSourceNodeGet(
    const XL6_HirModule* module,
    const XL6_AstNode* source
)
{
    return xl6HirNodeGet(module, xl6HirSourceNode(module, source));
}

const XL6_HirNode* xl6HirSymbolNodeGet(
    const XL6_HirModule* module,
    XL6_SymbolId symbol
)
{
    if ( module == NULL || symbol == XL6_SYMBOL_ID_INVALID ||
         (size_t)symbol >= module->symbolNodeCapacity ) {
        return NULL;
    }
    return xl6HirNodeGet(module, module->symbolNodes[symbol]);
}

int xl6HirFinalizeModule(XL6_AstNode* root, XL6_DiagList* diags, XL6_HirStats* stats)
{
    XL6_HirModule module;
    int ok;

    xl6HirModuleInit(&module);
    ok = xl6HirBuildModule(&module, root, diags);
    if ( stats != NULL ) {
        *stats = module.stats;
    }
    xl6HirModuleFree(&module);
    return ok;
}
