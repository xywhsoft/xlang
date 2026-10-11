/*
    XRT 专用数据结构的语义入口。

    这些类型保留静态泛型信息，但底层始终是带引用计数的 XRT 句柄。
    编译器只负责类型检查与方法分派，不复制栈、树或队列算法。
*/

static int xl6SemIsSpecialQueueTypeName(const char* name)
{
    name = xl6TypeCanonicalBuiltinName(name);
    return xl6TextEquals(name, "SpscQueue") ||
           xl6TextEquals(name, "MpscQueue") ||
           xl6TextEquals(name, "MpmcQueue") ||
           xl6TextEquals(name, "WaitQueue");
}

static int xl6SemIsSpecialCollectionTypeName(const char* name)
{
    return xl6TextEquals(name, "SlotMap") || xl6TextEquals(name, "Stack") ||
           xl6TextEquals(name, "OrderedMap") ||
           xl6SemIsSpecialQueueTypeName(name);
}

static int xl6SemIsSpecialCollectionGenericInstant(const XL6_AstNode* node)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* typeArgs;
    size_t expected;

    if ( node == NULL || node->kind != XL6_AST_GENERIC_INSTANT || node->childCount < 2 ) {
        return 0;
    }
    nameNode = node->children[0];
    typeArgs = node->children[1];
    if ( nameNode == NULL || nameNode->kind != XL6_AST_NAME ||
         !xl6SemIsSpecialCollectionTypeName(nameNode->text) || typeArgs == NULL ) {
        return 0;
    }
    expected = xl6TextEquals(nameNode->text, "OrderedMap") ? 2u : 1u;
    return typeArgs->childCount == expected;
}

static XL6_AstNode* xl6SemCloneSpecialCollectionReceiverType(
    XL6_SemContext* ctx,
    XL6_AstNode* member)
{
    XL6_AstNode* receiver;

    if ( member == NULL || member->kind != XL6_AST_MEMBER || member->childCount == 0 ) {
        return NULL;
    }
    receiver = member->children[0];
    /* Observe the proven flow type, not the nullable binding's storage type.
     * InferAutoTypeNode preserves that observation on the expression for IR.
     * An unproven nullable receiver is deliberately not unwrapped here. */
    return xl6SemInferAutoTypeNode(ctx, receiver);
}

static const XL6_AstNode* xl6SemSpecialCollectionTypeArg(
    XL6_SemContext* ctx,
    const XL6_AstNode* typeNode,
    size_t index)
{
    const XL6_AstNode* concreteType = xl6SemResolveAliasType(ctx, typeNode);
    const XL6_AstNode* args = xl6SemTypeNodeGenericArgs(concreteType);

    return args != NULL && index < args->childCount ? args->children[index] : NULL;
}

static XL6_AstNode* xl6SemSpecialCollectionArgExpr(XL6_AstNode* args, size_t index)
{
    XL6_AstNode* arg;

    if ( args == NULL || index >= args->childCount ) {
        return NULL;
    }
    arg = args->children[index];
    return arg != NULL && arg->kind == XL6_AST_ARG && arg->childCount > 0 ? arg->children[0] : NULL;
}

static int xl6SemCheckSpecialCollectionValueArg(
    XL6_SemContext* ctx,
    XL6_AstNode* expr,
    const XL6_AstNode* targetType,
    const char* message)
{
    if ( expr != NULL && targetType != NULL && xl6SemExprCanMatchParamType(ctx, expr, targetType) ) {
        /* A generic method is an expected-type boundary just like a resolved
         * parameter. Preserve the validated literal type before Typed IR;
         * otherwise a fitting int32 literal is left as int64 and rejected. */
        const XL6_AstNode* concrete = xl6SemResolveAliasType(ctx, targetType);
        if ( concrete != NULL &&
             (expr->kind == XL6_AST_NULL ||
              xl6SemExprIntegerLiteralFitsType(expr, concrete->text) ||
              (expr->kind == XL6_AST_FLOAT && xl6SemIsFloatTypeName(concrete->text))) ) {
            XL6_AstNode* expected = xl6SemCloneTypeNode(targetType);
            if ( expected == NULL ) return 0;
            xl6AstFree(expr->inferredType);
            expr->inferredType = expected;
        }
        return 1;
    }
    xl6SemTypeMismatchError(
        ctx,
        expr != NULL ? expr : (XL6_AstNode*)targetType,
        message,
        targetType,
        xl6SemExprStaticTypeName(ctx, expr));
    return 0;
}

static int xl6SemCheckSpecialCollectionOutArg(
    XL6_SemContext* ctx,
    XL6_AstNode* expr,
    const XL6_AstNode* targetType,
    const char* message)
{
    XL6_Symbol* sym;

    if ( expr == NULL || expr->kind != XL6_AST_NAME ) {
        xl6SemErrorNode(ctx, expr != NULL ? expr : targetType, message);
        return 0;
    }
    sym = xl6ScopeFind(ctx->scope, expr->text);
    if ( sym == NULL || sym->type == NULL ) {
        xl6SemErrorNode(ctx, expr, "输出目标变量未声明");
        return 0;
    }
    if ( xl6SemSymbolIsConst(sym) ) {
        xl6SemErrorNode(ctx, expr, "const 不能作为输出目标");
        return 0;
    }
    if ( !xl6SemCheckBoundarySymbolUse(ctx, expr, sym, 1) ) {
        return 0;
    }
    if ( targetType == NULL || !xl6SemTypeEquals(ctx, sym->type, targetType) ) {
        xl6SemTypeMismatchError(ctx, expr, message, targetType, xl6SemDisplayTypeName(ctx, sym->type));
        return 0;
    }
    return 1;
}



/* 检查专用集合方法的参数数量，并在入口处给出统一诊断。 */
static int xl6SemCheckSpecialCollectionArgCount(
    XL6_SemContext* ctx,
    XL6_AstNode* member,
    size_t actual,
    size_t expected)
{
    if ( actual == expected ) {
        return 1;
    }
    xl6SemErrorNode(ctx, member, "[SC-ARG-COUNT] 专用集合方法参数数量不匹配");
    return 0;
}



/* 检查映射到容量、深度或超时参数的 xlang 整数。 */
static int xl6SemCheckSpecialCollectionIntArg(
    XL6_SemContext* ctx,
    XL6_AstNode* expr,
    const char* message)
{
    XL6_AstNode intType;

    memset(&intType, 0, sizeof(intType));
    intType.kind = XL6_AST_TYPE;
    intType.text = "int";
    return xl6SemCheckSpecialCollectionValueArg(ctx, expr, &intType, message);
}



/* 检查专用集合布尔配置参数。 */
static int xl6SemCheckSpecialCollectionBoolArg(
    XL6_SemContext* ctx,
    XL6_AstNode* expr,
    const char* message)
{
    XL6_AstNode boolType;

    memset(&boolType, 0, sizeof(boolType));
    boolType.kind = XL6_AST_TYPE;
    boolType.text = "bool";
    return xl6SemCheckSpecialCollectionValueArg(ctx, expr, &boolType, message);
}



/* 检查批量队列参数是否为相同元素类型的原生数组。 */
static int xl6SemCheckSpecialCollectionArrayArg(
    XL6_SemContext* ctx,
    XL6_AstNode* expr,
    const XL6_AstNode* itemType,
    const char* message)
{
    XL6_AstNode* typeNode;
    const XL6_AstNode* concreteType;
    const XL6_AstNode* typeArgs;
    int ok;

    /* Literal arguments get the method's declared array<T> context, exactly
     * like an ordinary typed parameter. Existing Array owners are never
     * retagged or implicitly imported to another element domain. */
    XL6_AstNode* literal=expr;
    if (literal && literal->kind==XL6_AST_TYPED_LITERAL && literal->childCount)
        literal=literal->children[0];
    if (literal && literal->kind==XL6_AST_ARRAY_LITERAL && itemType) {
        XL6_AstNode* expected=xl6SemNewGenericContainerType("array",xl6SemCloneTypeNode(itemType),expr);
        if (!expected || !xl6SemContainerLiteralMatchesType(ctx,literal,expected)) {
            xl6AstFree(expected); xl6SemErrorNode(ctx,expr,message); return 0;
        }
        XL6_AstNode* inner=literal!=expr ? xl6SemCloneTypeNode(expected) : NULL;
        if (literal!=expr && !inner) {
            xl6AstFree(expected); xl6SemErrorNode(ctx,expr,"out of memory recording array argument type"); return 0;
        }
        xl6SemBindInferredTypeIdentity(ctx,expected);
        xl6AstFree(expr->inferredType); expr->inferredType=expected;
        if (inner) { xl6AstFree(literal->inferredType); literal->inferredType=inner; }
        return 1;
    }
    typeNode = xl6SemInferAutoTypeNode(ctx, expr);
    concreteType = xl6SemResolveAliasType(ctx, typeNode);
    typeArgs = concreteType != NULL ? xl6SemTypeNodeGenericArgs(concreteType) : NULL;
    ok = concreteType != NULL && xl6TextEquals(concreteType->text, "array") &&
         typeArgs != NULL && typeArgs->childCount == 1u && itemType != NULL &&
         xl6SemTypeEquals(ctx, typeArgs->children[0], itemType);
    if ( !ok ) {
        xl6SemErrorNode(ctx, expr, message);
    }
    xl6AstFree(typeNode);
    return ok;
}



static int xl6SemCheckSpecialCollectionCtorCall(XL6_SemContext* ctx, XL6_AstNode* node)
{
    const XL6_AstNode* callee = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    const XL6_AstNode* nameNode = callee != NULL && callee->childCount > 0 ? callee->children[0] : NULL;
    const XL6_AstNode* typeArgs = callee != NULL && callee->childCount > 1 ? callee->children[1] : NULL;
    XL6_AstNode* args = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    size_t i;
    size_t maxArgs;
    int ok = 1;

    if ( !xl6SemIsSpecialCollectionGenericInstant(callee) ) {
        return -1;
    }
    for ( i = 0; typeArgs != NULL && i < typeArgs->childCount; ++i ) {
        if ( !xl6SemCheckType(ctx, typeArgs->children[i]) ) {
            ok = 0;
        }
    }
    if ( nameNode != NULL && xl6TextEquals(nameNode->text, "OrderedMap") &&
         typeArgs != NULL && typeArgs->childCount > 0 &&
         !xl6SemTypeHasComparableProtocol(ctx, typeArgs->children[0]) ) {
        xl6SemErrorNode(ctx, typeArgs->children[0], "OrderedMap<K, V> 的键类型必须满足 comparable");
        ok = 0;
    }

    maxArgs = nameNode != NULL && xl6SemIsSpecialQueueTypeName(nameNode->text) ? 1u : 0u;
    if ( args != NULL && args->childCount > maxArgs ) {
        xl6SemErrorNode(ctx, args, maxArgs == 0 ? "该构造函数不接收参数" : "队列构造函数最多接收一个容量参数");
        ok = 0;
    }
    for ( i = 0; args != NULL && i < args->childCount; ++i ) {
        XL6_AstNode* arg = args->children[i];
        XL6_AstNode* expr = xl6SemSpecialCollectionArgExpr(args, i);
        XL6_AstNode intType;

        if ( arg == NULL || arg->kind != XL6_AST_ARG ) {
            xl6SemErrorNode(ctx, arg != NULL ? arg : args, "构造函数不接受命名参数");
            ok = 0;
            continue;
        }
        memset(&intType, 0, sizeof(intType));
        intType.kind = XL6_AST_TYPE;
        intType.text = "int";
        if ( expr == NULL || !xl6SemExprCanMatchParamType(ctx, expr, &intType) ) {
            xl6SemErrorNode(ctx, expr != NULL ? expr : arg, "队列容量必须是整数");
            ok = 0;
        }
    }
    return ok;
}

typedef struct XL6_SemTreeQuery { const char* name; bool search,pair,required; } XL6_SemTreeQuery;
static int xl6SemTreeQueryAppend(XL6_AstNode* parent,XL6_AstNode* child)
{
    if(!child) return 0;
    size_t count=parent->childCount;
    xl6AstAddChild(parent,child);
    if(parent->childCount!=count+1) { xl6AstFree(child); return 0; }
    return 1;
}
static const XL6_SemTreeQuery* xl6SemTreeQuery(const char* name)
{
    static const XL6_SemTreeQuery queries[]={
#define XL6_TREE_QUERY_OP(id,spelling,search,pair,required,native) {spelling,search,pair,required},
#include "src/abi/xl6_tree_query_ops.inc"
#undef XL6_TREE_QUERY_OP
    };
    for(size_t i=0;name && i<sizeof(queries)/sizeof(*queries);++i)
        if(xl6TextEquals(name,queries[i].name)) return &queries[i];
    return NULL;
}
typedef enum XL6_SemSlotMapRole {
    XL6_SEM_SLOT_MAP_NONE, XL6_SEM_SLOT_MAP_KEY, XL6_SEM_SLOT_MAP_ITEM,
    XL6_SEM_SLOT_MAP_INT, XL6_SEM_SLOT_MAP_BOOL, XL6_SEM_SLOT_MAP_OWNER,
    XL6_SEM_SLOT_MAP_ARRAY_KEY, XL6_SEM_SLOT_MAP_ARRAY_ITEM
} XL6_SemSlotMapRole;
typedef struct XL6_SemSlotMapMethod {
    const char* name; size_t count; XL6_SemSlotMapRole args[2], result;
} XL6_SemSlotMapMethod;
static const XL6_SemSlotMapMethod* xl6SemSlotMapMethod(const char* name)
{
    static const XL6_SemSlotMapMethod methods[]={
#define XL6_SLOT_MAP_OP(id, spelling, count, a, b, result, native, failure) \
        {spelling,count,{XL6_SEM_SLOT_MAP_##a,XL6_SEM_SLOT_MAP_##b},XL6_SEM_SLOT_MAP_##result},
#include "src/abi/xl6_slot_map_ops.inc"
#undef XL6_SLOT_MAP_OP
    };
    for(size_t i=1;name && i<sizeof(methods)/sizeof(*methods);++i)
        if (xl6TextEquals(name,methods[i].name)) return &methods[i];
    return NULL;
}
static int xl6SemCheckSlotMapMethod(XL6_SemContext* ctx,XL6_AstNode* member,
    XL6_AstNode* args,size_t count,const XL6_AstNode* item)
{
    const XL6_SemSlotMapMethod* method=xl6SemSlotMapMethod(member->text);
    if(!method) { xl6SemErrorNode(ctx,member,"unknown SlotMap method"); return 0; }
    if(!xl6SemCheckSpecialCollectionArgCount(ctx,member,count,method->count)) return 0;
    XL6_AstNode key={0}; key.kind=XL6_AST_TYPE; key.text="SlotMapKey";
    int ok=1;
    for(size_t i=0;i<count;++i) {
        XL6_AstNode* expr=xl6SemSpecialCollectionArgExpr(args,i);
        if(method->args[i]==XL6_SEM_SLOT_MAP_INT)
            ok=xl6SemCheckSpecialCollectionIntArg(ctx,expr,"SlotMap capacity must be int") && ok;
        else ok=xl6SemCheckSpecialCollectionValueArg(ctx,expr,
            method->args[i]==XL6_SEM_SLOT_MAP_KEY ? &key : item,"SlotMap argument type mismatch") && ok;
    }
    return ok;
}
static int xl6SemCheckSpecialCollectionMethodCall(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* member;
    XL6_AstNode* receiverType;
    const XL6_AstNode* concreteType;
    const XL6_AstNode* itemType;
    const XL6_AstNode* keyType;
    XL6_AstNode* args;
    size_t count;
    const char* typeName;
    const char* method;
    int ok = 1;

    if ( node == NULL || node->childCount == 0 || node->children[0] == NULL ||
         node->children[0]->kind != XL6_AST_MEMBER ) {
        return -1;
    }
    member = node->children[0];
    receiverType = xl6SemCloneSpecialCollectionReceiverType(ctx, member);
    concreteType = xl6SemResolveAliasType(ctx, receiverType);
    typeName = concreteType != NULL ? concreteType->text : NULL;
    if ( !xl6SemIsSpecialCollectionTypeName(typeName) ) {
        xl6AstFree(receiverType);
        return -1;
    }
    if ( member->children[0] == NULL ||
         (member->children[0]->kind != XL6_AST_NAME && member->children[0]->kind != XL6_AST_MEMBER) ) {
        xl6SemErrorNode(ctx, member->children[0],
                        "[SC-TEMP-RECEIVER] 专用数据结构方法必须通过具名变量或字段调用");
        xl6AstFree(receiverType);
        return 0;
    }
    method = member->text;
    args = node->childCount > 1 ? node->children[1] : NULL;
    count = args != NULL ? args->childCount : 0u;
    itemType = xl6SemSpecialCollectionTypeArg(ctx, receiverType, xl6TextEquals(typeName, "OrderedMap") ? 1u : 0u);
    keyType = xl6TextEquals(typeName, "OrderedMap") ? xl6SemSpecialCollectionTypeArg(ctx, receiverType, 0u) : NULL;

    if (xl6TextEquals(typeName,"SlotMap")) {
        ok=xl6SemCheckSlotMapMethod(ctx,member,args,count,itemType);
    } else if ( xl6TextEquals(typeName, "Stack") ) {
        if ( xl6TextEquals(method, "push") ) {
            ok = xl6SemCheckSpecialCollectionArgCount(ctx, member, count, 1u) &&
                 xl6SemCheckSpecialCollectionValueArg(
                    ctx, xl6SemSpecialCollectionArgExpr(args, 0), itemType,
                    "Stack.push 参数类型必须匹配 Stack<T>");
        } else if ( xl6TextEquals(method, "pop") || xl6TextEquals(method, "peek") ||
                    xl6TextEquals(method, "tryPop") || xl6TextEquals(method, "tryPeek") ) {
            ok = xl6SemCheckSpecialCollectionArgCount(ctx, member, count, 0u);
        } else if ( xl6TextEquals(method, "peekAt") || xl6TextEquals(method, "tryPeekAt") ) {
            ok = xl6SemCheckSpecialCollectionArgCount(ctx, member, count, 1u) &&
                 xl6SemCheckSpecialCollectionIntArg(
                    ctx, xl6SemSpecialCollectionArgExpr(args, 0),
                    "Stack.peekAt 深度必须是整数");
        } else if (xl6TextEquals(method,"pushBatch")) {
            ok=xl6SemCheckSpecialCollectionArgCount(ctx,member,count,1u) &&
                xl6SemCheckSpecialCollectionArrayArg(ctx,xl6SemSpecialCollectionArgExpr(args,0),itemType,
                    "Stack.pushBatch 参数必须是相同元素类型的 array");
        } else if (xl6TextEquals(method,"popBatch") || xl6TextEquals(method,"peekBatch")) {
            ok=xl6SemCheckSpecialCollectionArgCount(ctx,member,count,1u) &&
                xl6SemCheckSpecialCollectionIntArg(ctx,xl6SemSpecialCollectionArgExpr(args,0),
                    "Stack 批量数量必须是整数");
        } else if (xl6TextEquals(method,"peekBatchAt")) {
            ok=xl6SemCheckSpecialCollectionArgCount(ctx,member,count,2u) &&
                xl6SemCheckSpecialCollectionIntArg(ctx,xl6SemSpecialCollectionArgExpr(args,0),
                    "Stack.peekBatchAt 深度必须是整数") &&
                xl6SemCheckSpecialCollectionIntArg(ctx,xl6SemSpecialCollectionArgExpr(args,1),
                    "Stack.peekBatchAt 数量必须是整数");
        } else if ( xl6TextEquals(method, "reserve") ) {
            ok = xl6SemCheckSpecialCollectionArgCount(ctx, member, count, 1u) &&
                 xl6SemCheckSpecialCollectionIntArg(
                    ctx, xl6SemSpecialCollectionArgExpr(args, 0),
                    "Stack.reserve 容量必须是整数");
        } else if ( xl6TextEquals(method, "equals") ) {
            ok = xl6SemCheckSpecialCollectionArgCount(ctx, member, count, 1u) &&
                 xl6SemCheckSpecialCollectionValueArg(
                    ctx, xl6SemSpecialCollectionArgExpr(args, 0), receiverType,
                    "Stack.equals 参数必须是相同元素类型的 Stack");
        } else if ( xl6TextEquals(method, "clear") || xl6TextEquals(method, "count") ||
                    xl6TextEquals(method, "capacity") || xl6TextEquals(method, "trim") ||
                    xl6TextEquals(method, "clone") ) {
            ok = xl6SemCheckSpecialCollectionArgCount(ctx, member, count, 0u);
        } else {
            xl6SemErrorNode(ctx, member, "unknown Stack method");
            ok = 0;
        }
    } else if ( xl6TextEquals(typeName, "OrderedMap") ) {
        const XL6_SemTreeQuery* query=xl6SemTreeQuery(method);
        if (xl6TextEquals(method,"getInto") || xl6TextEquals(method,"takeInto") ||
            xl6TextEquals(method,"firstInto") || xl6TextEquals(method,"lastInto") ||
            xl6TextEquals(method,"lowerBoundInto") || xl6TextEquals(method,"upperBoundInto")) {
            xl6SemErrorNode(ctx,member,"[SC-TREE-OWNED] OrderedMap 查询返回拥有值；使用 get/take/first/last/lowerBound/upperBound 或对应 try 查询，不提供 Into 写回");
            ok=0;
        } else if(query) {
            ok=xl6SemCheckSpecialCollectionArgCount(ctx,member,count,query->search ? 1u : 0u) &&
                (!query->search || xl6SemCheckSpecialCollectionValueArg(ctx,
                    xl6SemSpecialCollectionArgExpr(args,0),keyType,"OrderedMap 查询键类型不匹配"));
        } else if ( xl6TextEquals(method, "set") ) {
            ok = xl6SemCheckSpecialCollectionArgCount(ctx, member, count, 2u) &&
                 xl6SemCheckSpecialCollectionValueArg(
                    ctx, xl6SemSpecialCollectionArgExpr(args, 0), keyType,
                    "OrderedMap.set 键类型不匹配") &&
                 xl6SemCheckSpecialCollectionValueArg(
                    ctx, xl6SemSpecialCollectionArgExpr(args, 1), itemType,
                    "OrderedMap.set 值类型不匹配");
        } else if ( xl6TextEquals(method, "contains") || xl6TextEquals(method, "remove") ) {
            ok = xl6SemCheckSpecialCollectionArgCount(ctx, member, count, 1u) &&
                 xl6SemCheckSpecialCollectionValueArg(
                    ctx, xl6SemSpecialCollectionArgExpr(args, 0), keyType,
                    "OrderedMap 键类型不匹配");
        } else if ( xl6TextEquals(method, "trimTo") ) {
            ok = xl6SemCheckSpecialCollectionArgCount(ctx, member, count, 1u) &&
                 xl6SemCheckSpecialCollectionIntArg(
                    ctx, xl6SemSpecialCollectionArgExpr(args, 0),
                    "OrderedMap.trimTo 保留数量必须是整数");
        } else if ( xl6TextEquals(method, "equals") ) {
            ok = xl6SemCheckSpecialCollectionArgCount(ctx, member, count, 1u) &&
                 xl6SemCheckSpecialCollectionValueArg(
                    ctx, xl6SemSpecialCollectionArgExpr(args, 0), receiverType,
                    "OrderedMap.equals 参数必须是相同键值类型的 OrderedMap");
        } else if ( xl6TextEquals(method, "merge") ) {
            ok = (count == 1u || count == 2u);
            if ( !ok ) {
                xl6SemErrorNode(ctx, member, "[SC-ARG-COUNT] OrderedMap.merge 需要一个或两个参数");
            } else {
                ok = xl6SemCheckSpecialCollectionValueArg(
                    ctx, xl6SemSpecialCollectionArgExpr(args, 0), receiverType,
                    "OrderedMap.merge 参数必须是相同键值类型的 OrderedMap") &&
                    (count == 1u || xl6SemCheckSpecialCollectionBoolArg(
                        ctx, xl6SemSpecialCollectionArgExpr(args, 1),
                        "OrderedMap.merge replace 参数必须是 bool"));
            }
        } else if ( xl6TextEquals(method, "clear") || xl6TextEquals(method, "count") ||
                    xl6TextEquals(method, "keys") || xl6TextEquals(method, "values") ||
                    xl6TextEquals(method, "trim") || xl6TextEquals(method, "clone") ) {
            ok = xl6SemCheckSpecialCollectionArgCount(ctx, member, count, 0u);
        } else {
            xl6SemErrorNode(ctx, member, "unknown OrderedMap method");
            ok = 0;
        }
    } else {
        if (!xl6TextEquals(typeName,"WaitQueue") &&
            (xl6TextEquals(method,"tryPopInto") || xl6TextEquals(method,"popBatchInto"))) {
            xl6SemErrorNode(ctx,member,"[SC-QUEUE-OWNED] 使用 tryPop()/popBatch(maximum) 的状态和拥有返回值；队列不提供 Into 写回");
            ok=0;
        } else if (!xl6TextEquals(typeName,"WaitQueue") && xl6TextEquals(method,"tryPop")) {
            ok=xl6SemCheckSpecialCollectionArgCount(ctx,member,count,0u);
        } else if (!xl6TextEquals(typeName,"WaitQueue") && xl6TextEquals(method,"popBatch")) {
            ok=xl6SemCheckSpecialCollectionArgCount(ctx,member,count,1u) &&
                xl6SemCheckSpecialCollectionIntArg(ctx,xl6SemSpecialCollectionArgExpr(args,0),
                    "队列批量返回容量必须是整数");
        } else if ( !xl6TextEquals(typeName, "WaitQueue") && xl6TextEquals(method, "pushBatch") ) {
            ok = xl6SemCheckSpecialCollectionArgCount(ctx, member, count, 1u) &&
                 xl6SemCheckSpecialCollectionArrayArg(
                    ctx, xl6SemSpecialCollectionArgExpr(args, 0), itemType,
                    "队列批量输入必须是相同元素类型的 array");
        } else if ( xl6TextEquals(typeName, "WaitQueue") &&
                    (xl6TextEquals(method, "pushBatch") ||
                     xl6TextEquals(method, "popBatchInto")) ) {
            xl6SemErrorNode(ctx, member, "[SC-WAIT-BATCH] WaitQueue 不支持无锁批量操作");
            ok = 0;
        } else if ( xl6TextEquals(method, "tryPush") ) {
            ok = xl6SemCheckSpecialCollectionArgCount(ctx, member, count, 1u) &&
                 xl6SemCheckSpecialCollectionValueArg(
                    ctx, xl6SemSpecialCollectionArgExpr(args, 0), itemType,
                    "队列元素类型不匹配");
        } else if ( xl6TextEquals(method, "tryPopInto") ||
                    (xl6TextEquals(typeName, "WaitQueue") && xl6TextEquals(method, "popInto")) ) {
            ok = xl6SemCheckSpecialCollectionArgCount(ctx, member, count, 1u) &&
                 xl6SemCheckSpecialCollectionOutArg(
                    ctx, xl6SemSpecialCollectionArgExpr(args, 0), itemType,
                    "队列输出目标类型不匹配");
        } else if ( xl6TextEquals(typeName, "WaitQueue") &&
                    xl6TextEquals(method, "popTimeoutInto") ) {
            ok = xl6SemCheckSpecialCollectionArgCount(ctx, member, count, 2u) &&
                 xl6SemCheckSpecialCollectionOutArg(
                    ctx, xl6SemSpecialCollectionArgExpr(args, 0), itemType,
                    "队列输出目标类型不匹配") &&
                 xl6SemCheckSpecialCollectionIntArg(
                    ctx, xl6SemSpecialCollectionArgExpr(args, 1),
                    "等待超时必须是整数毫秒");
        } else if ( xl6TextEquals(typeName, "WaitQueue") && xl6TextEquals(method, "reset") ) {
            xl6SemErrorNode(ctx, member, "[SC-WAIT-RESET] WaitQueue 不支持 reset");
            ok = 0;
        } else if ( xl6TextEquals(method, "reset") ) {
            ok = xl6SemCheckSpecialCollectionArgCount(ctx, member, count, 0u);
        } else if ( xl6TextEquals(method, "close") || xl6TextEquals(method, "count") ||
                    xl6TextEquals(method, "capacity") || xl6TextEquals(method, "isClosed") ||
                    xl6TextEquals(method, "isDrained") ) {
            ok = xl6SemCheckSpecialCollectionArgCount(ctx, member, count, 0u);
        } else {
            xl6SemErrorNode(ctx, member, "unknown queue method");
            ok = 0;
        }
    }
    xl6AstFree(receiverType);
    return ok;
}

static int xl6SemCheckSpecialCollectionCall(XL6_SemContext* ctx, XL6_AstNode* node)
{
    int result = xl6SemCheckSpecialCollectionCtorCall(ctx, node);
    return result >= 0 ? result : xl6SemCheckSpecialCollectionMethodCall(ctx, node);
}

static const char* xl6SemSpecialCollectionCallResultTypeName(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* member;
    XL6_AstNode* receiverType;
    const XL6_AstNode* concreteType;
    const char* method;
    const char* result = NULL;

    if ( node == NULL || node->childCount == 0 ) {
        return NULL;
    }
    if ( xl6SemIsSpecialCollectionGenericInstant(node->children[0]) ) {
        const XL6_AstNode* nameNode = node->children[0]->children[0];
        return nameNode != NULL ? nameNode->text : NULL;
    }
    member = node->children[0];
    if ( member == NULL || member->kind != XL6_AST_MEMBER ) {
        return NULL;
    }
    receiverType = xl6SemCloneSpecialCollectionReceiverType(ctx, member);
    concreteType = xl6SemResolveAliasType(ctx, receiverType);
    if ( concreteType == NULL || !xl6SemIsSpecialCollectionTypeName(concreteType->text) ) {
        xl6AstFree(receiverType);
        return NULL;
    }
    method = member->text;
    if(xl6TextEquals(concreteType->text,"SlotMap")) {
        const XL6_SemSlotMapMethod* spec=xl6SemSlotMapMethod(method);
        result=spec ? (spec->result==XL6_SEM_SLOT_MAP_KEY ? "SlotMapKey" :
            spec->result==XL6_SEM_SLOT_MAP_BOOL ? "bool" :
            spec->result==XL6_SEM_SLOT_MAP_INT ? "int" : NULL) : NULL;
        xl6AstFree(receiverType); return result;
    }
    if (xl6SemIsSpecialQueueTypeName(concreteType->text) &&
        !xl6TextEquals(concreteType->text,"WaitQueue") && xl6TextEquals(method,"tryPush")) {
        result="int";
    } else if (xl6SemIsSpecialQueueTypeName(concreteType->text) &&
        !xl6TextEquals(concreteType->text,"WaitQueue") && xl6TextEquals(method,"pushBatch")) {
        result=NULL;
    } else if (concreteType && xl6TextEquals(concreteType->text,"Stack") &&
        xl6TextEquals(method,"pushBatch")) {
        result="bool";
    } else if ( xl6TextEquals(method, "push") ||
         xl6TextEquals(method, "reserve") || xl6TextEquals(method, "set") ||
         xl6TextEquals(method, "contains") || xl6TextEquals(method, "remove") ||
         xl6TextEquals(method, "equals") || xl6TextEquals(method, "merge") ||
         xl6TextEquals(method, "tryPush") || xl6TextEquals(method, "reset") ||
         xl6TextEquals(method, "tryPopInto") || xl6TextEquals(method, "popInto") ||
         xl6TextEquals(method, "popTimeoutInto") || xl6TextEquals(method, "isClosed") ||
         xl6TextEquals(method, "isDrained") ) {
        result = "bool";
    } else if ( xl6TextEquals(method, "count") || xl6TextEquals(method, "capacity") ||
                xl6TextEquals(method, "pushBatch") || xl6TextEquals(method, "popBatchInto") ||
                xl6TextEquals(method, "trim") || xl6TextEquals(method, "trimTo") ) {
        result = xl6TextEquals(concreteType->text, "Stack") && xl6TextEquals(method, "trim")
            ? "bool"
            : "int";
    }
    xl6AstFree(receiverType);
    return result;
}

/* A resolved typed Array extraction transfers an owned result. Reuse this
 * full element-type contract for inference and fresh-owner checking; a class
 * method merely named pop/take and an indexed borrow never reach this path. */
static XL6_AstNode* xl6SemCloneContainerItemType(XL6_SemContext*,const XL6_AstNode*);
static XL6_AstNode* xl6SemCloneArrayExtractionCallType(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* member=node && node->kind==XL6_AST_CALL && node->childCount
        ? node->children[0] : NULL;
    if (!member || member->kind!=XL6_AST_MEMBER || !member->childCount ||
        (!xl6TextEquals(member->text,"pop") && !xl6TextEquals(member->text,"take"))) return NULL;
    XL6_AstNode* receiver=xl6SemInferAutoTypeNode(ctx,member->children[0]); int typed=0;
    XL6_SemContainerKind kind=xl6SemContainerKindFromType(ctx,receiver,&typed);
    XL6_AstNode* element=typed && kind==XL6_SEM_CONTAINER_ARRAY
        ? xl6SemCloneContainerItemType(ctx,receiver) : NULL;
    xl6AstFree(receiver); return element;
}

static XL6_AstNode* xl6SemCloneSpecialCollectionCallType(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* member;
    XL6_AstNode* receiverType;
    const XL6_AstNode* concreteType;
    const XL6_AstNode* itemType;
    const char* scalarType;

    scalarType = xl6SemSpecialCollectionCallResultTypeName(ctx, node);
    if ( scalarType != NULL ) {
        return xl6AstNewText(XL6_AST_TYPE, scalarType, strlen(scalarType), NULL);
    }
    if ( node == NULL || node->childCount == 0 || node->children[0] == NULL ||
         node->children[0]->kind != XL6_AST_MEMBER ) {
        return NULL;
    }
    member = node->children[0];
    receiverType=xl6SemCloneSpecialCollectionReceiverType(ctx,member);
    concreteType=xl6SemResolveAliasType(ctx,receiverType);
    if(concreteType && xl6TextEquals(concreteType->text,"OrderedMap")) {
        const XL6_SemTreeQuery* query=xl6SemTreeQuery(member->text);
        if(query) {
            XL6_AstNode* result=query->required && !query->pair
                ? xl6SemCloneTypeNode(xl6SemSpecialCollectionTypeArg(ctx,receiverType,1u)) : NULL;
            xl6AstFree(receiverType); return result;
        }
    }
    if(concreteType && xl6TextEquals(concreteType->text,"SlotMap")) {
        const XL6_SemSlotMapMethod* spec=xl6SemSlotMapMethod(member->text);
        XL6_AstNode* result=NULL;
        if(spec && (spec->result==XL6_SEM_SLOT_MAP_ITEM || spec->result==XL6_SEM_SLOT_MAP_ARRAY_ITEM)) {
            result=xl6SemCloneTypeNode(xl6SemSpecialCollectionTypeArg(ctx,receiverType,0u));
            if(spec->result==XL6_SEM_SLOT_MAP_ARRAY_ITEM && result)
                result=xl6SemNewGenericContainerType("array",result,member);
        } else if(spec && spec->result==XL6_SEM_SLOT_MAP_ARRAY_KEY)
            result=xl6SemNewGenericContainerType("array",
                xl6AstNewText(XL6_AST_TYPE,"SlotMapKey",10,NULL),member);
        xl6AstFree(receiverType); return result;
    }
    xl6AstFree(receiverType);
    if (xl6TextEquals(member->text,"popBatch") || xl6TextEquals(member->text,"peekBatch") ||
        xl6TextEquals(member->text,"peekBatchAt")) {
        receiverType=xl6SemCloneSpecialCollectionReceiverType(ctx,member);
        concreteType=xl6SemResolveAliasType(ctx,receiverType);
        XL6_AstNode* result=concreteType && xl6TextEquals(concreteType->text,"Stack")
            ? xl6SemNewGenericContainerType("array",
                xl6SemCloneTypeNode(xl6SemSpecialCollectionTypeArg(ctx,receiverType,0u)),member) : NULL;
        xl6AstFree(receiverType); return result;
    }
    if ( xl6TextEquals(member->text, "pop") || xl6TextEquals(member->text, "peek") ||
         xl6TextEquals(member->text, "peekAt") ) {
        receiverType = xl6SemCloneSpecialCollectionReceiverType(ctx, member);
        concreteType = xl6SemResolveAliasType(ctx, receiverType);
        XL6_AstNode* result = concreteType != NULL && xl6TextEquals(concreteType->text, "Stack")
            ? xl6SemCloneTypeNode(xl6SemSpecialCollectionTypeArg(ctx, receiverType, 0u)) : NULL;
        xl6AstFree(receiverType);
        return result;
    }
    if ( xl6TextEquals(member->text, "clone") ) {
        receiverType = xl6SemCloneSpecialCollectionReceiverType(ctx, member);
        concreteType = xl6SemResolveAliasType(ctx, receiverType);
        if ( concreteType != NULL &&
             (xl6TextEquals(concreteType->text, "Stack") ||
              xl6TextEquals(concreteType->text, "OrderedMap")) ) {
            return receiverType;
        }
        xl6AstFree(receiverType);
        return NULL;
    }
    if ( !xl6TextEquals(member->text, "keys") && !xl6TextEquals(member->text, "values") ) {
        return NULL;
    }
    receiverType = xl6SemCloneSpecialCollectionReceiverType(ctx, member);
    concreteType = xl6SemResolveAliasType(ctx, receiverType);
    if ( concreteType == NULL || !xl6TextEquals(concreteType->text, "OrderedMap") ) {
        xl6AstFree(receiverType);
        return NULL;
    }
    itemType = xl6SemSpecialCollectionTypeArg(
        ctx,
        receiverType,
        xl6TextEquals(member->text, "keys") ? 0u : 1u);
    {
        XL6_AstNode* result = xl6SemNewGenericContainerType("array", xl6SemCloneTypeNode(itemType), member);
        xl6AstFree(receiverType);
        return result;
    }
}

/* A successful nullable element may itself be null. The separate status
 * preserves that distinction without inventing a default non-null T. */
static XL6_AstNode* xl6SemCloneSpecialCollectionReturnTypes(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* member = node != NULL && node->kind == XL6_AST_CALL && node->childCount
        ? node->children[0] : NULL;
    if(member && member->kind==XL6_AST_MEMBER && xl6SemTreeQuery(member->text)) {
        XL6_AstNode* receiver=xl6SemCloneSpecialCollectionReceiverType(ctx,member);
        const XL6_AstNode* concrete=xl6SemResolveAliasType(ctx,receiver);
        const XL6_SemTreeQuery* query=concrete && xl6TextEquals(concrete->text,"OrderedMap")
            ? xl6SemTreeQuery(member->text) : NULL;
        if(query && (query->pair || !query->required)) {
            XL6_AstNode* result=xl6AstNew(XL6_AST_RETURN_TYPES,NULL);
            if(result && !query->required) {
                XL6_AstNode* status=xl6AstNewText(XL6_AST_TYPE,"bool",4,NULL);
                if(!xl6SemTreeQueryAppend(result,status)) { xl6AstFree(result); result=NULL; }
            }
            for(unsigned i=query->pair ? 0u : 1u;result && i<2u;++i) {
                XL6_AstNode* part=xl6SemCloneTypeNode(xl6SemSpecialCollectionTypeArg(ctx,receiver,i));
                if(part && !query->required) part=xl6SemLiftNullableType(part,member);
                if(!xl6SemTreeQueryAppend(result,part)) { xl6AstFree(result); result=NULL; break; }
            }
            xl6AstFree(receiver); return result;
        }
        xl6AstFree(receiver);
    }
    if ( member == NULL || member->kind != XL6_AST_MEMBER ||
         (!xl6TextEquals(member->text, "tryPop") && !xl6TextEquals(member->text, "tryPeek") &&
          !xl6TextEquals(member->text, "tryPeekAt") && !xl6TextEquals(member->text,"pushBatch") &&
          !xl6TextEquals(member->text,"popBatch")) ) return NULL;
    XL6_AstNode* receiver = xl6SemCloneSpecialCollectionReceiverType(ctx, member);
    const XL6_AstNode* concrete = xl6SemResolveAliasType(ctx, receiver);
    bool queue=concrete && xl6SemIsSpecialQueueTypeName(concrete->text) &&
        !xl6TextEquals(concrete->text,"WaitQueue");
    bool stack=concrete && xl6TextEquals(concrete->text,"Stack");
    if ((!queue && !stack) || (queue && !xl6TextEquals(member->text,"tryPop") &&
        !xl6TextEquals(member->text,"pushBatch") && !xl6TextEquals(member->text,"popBatch")) ||
        (stack && (xl6TextEquals(member->text,"pushBatch") || xl6TextEquals(member->text,"popBatch")))) {
        xl6AstFree(receiver); return NULL;
    }
    const XL6_AstNode* item = concrete != NULL
        ? xl6SemResolveAliasType(ctx, xl6SemSpecialCollectionTypeArg(ctx, receiver, 0u)) : NULL;
    XL6_AstNode* value = queue && xl6TextEquals(member->text,"pushBatch")
        ? xl6AstNewText(XL6_AST_TYPE,"int",3,NULL)
        : queue && xl6TextEquals(member->text,"popBatch")
        ? xl6SemNewGenericContainerType("array",xl6SemCloneTypeNode(item),member)
        : item ? xl6SemLiftNullableType(xl6SemCloneTypeNode(item),member) : NULL;
    xl6AstFree(receiver);
    if ( value == NULL ) return NULL;
    XL6_AstNode* returns = xl6AstNew(XL6_AST_RETURN_TYPES, NULL);
    XL6_AstNode* status = xl6AstNewText(XL6_AST_TYPE, queue ? "int" : "bool", queue ? 3 : 4, NULL);
    if ( returns == NULL || status == NULL ) {
        xl6AstFree(returns); xl6AstFree(status); xl6AstFree(value); return NULL;
    }
    xl6AstAddChild(returns, status); xl6AstAddChild(returns, value);
    return returns;
}
