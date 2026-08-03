/*
 * nullable 的规范类型与运行时描述集中在这里收集。
 * 只有 tagged nullable 需要额外 C 布局；指针和 xvalue 形态仍需要
 * 独立 TypeDesc，不能借用内部 T 的类型身份。
 */

static int xl6CgenNullableContainsOpenType(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_TYPE &&
         node->semanticTypeId != XL6_TYPE_ID_INVALID &&
         node->semanticTypeAbiId == XL6_TYPE_ABI_ID_INVALID ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNullableContainsOpenType(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenAddNullableType(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode)
{
    XL6_AstNode* canonical;
    XL6_CgenNullableType* resized;
    unsigned long long typeId;
    size_t capacity;
    size_t i;

    if ( ctx == NULL || !xl6CgenIsNullableType(typeNode) ) {
        return 1;
    }
    canonical = xl6CgenCloneCanonicalTypeNode(ctx, typeNode);
    if ( canonical == NULL ) {
        xl6CgenError(ctx, typeNode, "nullable type canonicalization failed");
        return 0;
    }
    if ( xl6CgenNullableContainsOpenType(canonical) ) {
        xl6AstFree(canonical);
        return 1;
    }

    typeId = xl6CgenNullableTypeId(canonical);
    for ( i = 0; i < ctx->nullableTypeCount; ++i ) {
        if ( ctx->nullableTypes[i].typeId == typeId ) {
            xl6AstFree(canonical);
            return 1;
        }
    }

    if ( ctx->nullableTypeCount == ctx->nullableTypeCapacity ) {
        capacity = ctx->nullableTypeCapacity == 0 ? 8 : ctx->nullableTypeCapacity * 2;
        resized = (XL6_CgenNullableType*)xrtRealloc(
            ctx->nullableTypes,
            capacity * sizeof(XL6_CgenNullableType));
        if ( resized == NULL ) {
            xl6AstFree(canonical);
            xl6CgenError(ctx, typeNode, "out of memory while collecting nullable types");
            return 0;
        }
        ctx->nullableTypes = resized;
        ctx->nullableTypeCapacity = capacity;
    }

    ctx->nullableTypes[ctx->nullableTypeCount].typeNode = canonical;
    ctx->nullableTypes[ctx->nullableTypeCount].typeId = typeId;
    ctx->nullableTypes[ctx->nullableTypeCount].runtimeCastHelperEmitted = 0;
    ctx->nullableTypeCount += 1;
    return 1;
}

static void xl6CgenCollectNullableTypesNode(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node)
{
    size_t i;

    if ( ctx == NULL || node == NULL ) {
        return;
    }
    if ( xl6CgenIsNullableType(node) ) {
        xl6CgenAddNullableType(ctx, node);
    }
    for ( i = 0; i < node->childCount; ++i ) {
        xl6CgenCollectNullableTypesNode(ctx, node->children[i]);
    }
}

/*
 * auto、safe navigation 和运行时 cast 会在语义阶段产生源码中没有显式写出的 T?。
 * typed HIR 才是这些结果类型的权威来源，nullable 声明收集不能只遍历 AST。
 */
static void xl6CgenCollectNullableTypesFromHir(XL6_CgenContext* ctx)
{
    size_t i;

    if ( ctx == NULL || ctx->hir == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->hir->nodeCount; ++i ) {
        const XL6_HirNode* node = &ctx->hir->nodes[i];
        XL6_AstNode* typeNode;

        if ( node->type == XL6_HIR_TYPE_INVALID ) {
            continue;
        }
        typeNode = xl6HirTypeCloneAst(&ctx->hir->types, node->type);
        if ( typeNode != NULL ) {
            xl6CgenAddNullableType(ctx, typeNode);
            xl6AstFree(typeNode);
        }
    }
}

static void xl6CgenCollectNullableTypes(XL6_CgenContext* ctx)
{
    const XL6_AstNode* previousParams;
    const XL6_AstNode* previousArgs;
    size_t i;

    if ( ctx == NULL ) {
        return;
    }
    xl6CgenCollectNullableTypesFromHir(ctx);
    xl6CgenCollectNullableTypesNode(ctx, ctx->root);

    previousParams = ctx->currentGenericParams;
    previousArgs = ctx->currentGenericArgs;
    for ( i = 0; i < ctx->genericInstanceCount; ++i ) {
        ctx->currentGenericParams = xl6CgenFuncTypeParams(ctx->genericInstances[i].func);
        ctx->currentGenericArgs = ctx->genericInstances[i].typeArgs;
        xl6CgenCollectNullableTypesNode(ctx, ctx->genericInstances[i].func);
    }
    ctx->currentGenericParams = previousParams;
    ctx->currentGenericArgs = previousArgs;
}

/*
 * tagged nullable 可以包含需要显式管理生命周期的值。
 * 当前第一种是 XL6_Function；以后 tagged enum payload 也从这里扩展，
 * 不允许各个声明、赋值和挂起帧分别猜测所有权规则。
 */
static int xl6CgenNullableHasManagedInner(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode)
{
    const XL6_AstNode* inner;

    if ( ctx == NULL || !xl6CgenNullableUsesTaggedValue(typeNode) ) {
        return 0;
    }
    inner = xl6CgenNullableInnerType(typeNode);
    inner = xl6CgenResolveAliasType(
        ctx,
        xl6CgenSubstituteGenericType(ctx, inner));
    return xl6CgenIsFunctionType(inner) ||
           xl6CgenIsTaggedEnumType(ctx, inner);
}

static void xl6CgenNullableCopyHelperName(
    char* buffer,
    size_t capacity,
    const XL6_AstNode* typeNode)
{
    snprintf(
        buffer,
        capacity,
        "xl6_nullable_copy_%016llx",
        xl6CgenNullableTypeId(typeNode));
}

static void xl6CgenNullableTypeDescSymbol(
    XL6_Output* out,
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode)
{
    if ( out == NULL || ctx == NULL || typeNode == NULL ) {
        return;
    }
    xl6OutputPrintf(
        out,
        "xl6_runtime_type_desc_%016llX",
        xl6CgenTypeIdForTypeNode(ctx, typeNode));
}

static void xl6CgenEmitNullableTypeDescRef(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode)
{
    xl6CgenNullableTypeDescSymbol(ctx->out, ctx, typeNode);
    xl6OutputPuts("()", ctx->out);
}

/*
 * typed container 描述符会在 nullable 的完整定义之前引用元素描述符，
 * 因此先发射稳定原型；完整操作表等 class/enum 前置声明就绪后再生成。
 */
static void xl6CgenEmitNullableTypeDescProtos(XL6_CgenContext* ctx)
{
    size_t i;

    if ( ctx == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->nullableTypeCount; ++i ) {
        xl6OutputPuts("static const xrt_type_desc* ", ctx->out);
        xl6CgenNullableTypeDescSymbol(
            ctx->out,
            ctx,
            ctx->nullableTypes[i].typeNode);
        xl6OutputPuts("(void);\n", ctx->out);
    }
    if ( ctx->nullableTypeCount > 0 ) {
        xl6OutputPutc('\n', ctx->out);
    }
}

static void xl6CgenNullableDropHelperName(
    char* buffer,
    size_t capacity,
    const XL6_AstNode* typeNode)
{
    snprintf(
        buffer,
        capacity,
        "xl6_nullable_drop_%016llx",
        xl6CgenNullableTypeId(typeNode));
}

static void xl6CgenNullableUnwrapHelperName(
    char* buffer,
    size_t capacity,
    const XL6_AstNode* typeNode)
{
    snprintf(
        buffer,
        capacity,
        "xl6_nullable_unwrap_%016llx",
        xl6CgenNullableTypeId(typeNode));
}

static void xl6CgenNullableBoxHelperName(
    char* buffer,
    size_t capacity,
    const XL6_AstNode* typeNode)
{
    snprintf(
        buffer,
        capacity,
        "xl6_nullable_box_%016llx",
        xl6CgenNullableTypeId(typeNode));
}

static void xl6CgenNullableFromXValueHelperName(
    char* buffer,
    size_t capacity,
    const XL6_AstNode* typeNode)
{
    snprintf(
        buffer,
        capacity,
        "xl6_nullable_from_xvalue_%016llx",
        xl6CgenNullableTypeId(typeNode));
}

static void xl6CgenNullableAwaitCopyHelperName(
    char* buffer,
    size_t capacity,
    const XL6_AstNode* typeNode)
{
    snprintf(
        buffer,
        capacity,
        "xl6_future_await_nullable_%016llx_copy",
        xl6CgenNullableTypeId(typeNode));
}

static void xl6CgenNullableAwaitReleaseHelperName(
    char* buffer,
    size_t capacity,
    const XL6_AstNode* typeNode)
{
    snprintf(
        buffer,
        capacity,
        "xl6_future_await_nullable_%016llx_release",
        xl6CgenNullableTypeId(typeNode));
}

static void xl6CgenEmitNullableTypeDecls(XL6_CgenContext* ctx, int hasAsyncRuntime)
{
    size_t i;

    if ( ctx == NULL || ctx->nullableTypeCount == 0 ) {
        return;
    }
    for ( i = 0; i < ctx->nullableTypeCount; ++i ) {
        const XL6_AstNode* typeNode = ctx->nullableTypes[i].typeNode;
        const XL6_AstNode* inner = xl6CgenNullableInnerType(typeNode);
        char cTypeName[64];
        char unwrapName[64];
        const char* innerCType;

        if ( !xl6CgenNullableUsesTaggedValue(typeNode) ) {
            continue;
        }
        xl6CgenNullableCTypeName(cTypeName, sizeof(cTypeName), typeNode);
        xl6CgenNullableUnwrapHelperName(
            unwrapName,
            sizeof(unwrapName),
            typeNode);
        innerCType = xl6CgenTypeName(ctx, inner);
        xl6OutputPrintf(ctx->out, "typedef struct %s {\n", cTypeName);
        xl6OutputPuts("    bool hasValue;\n    ", ctx->out);
        if ( xl6CgenIsTypedContainerType(ctx, inner) ) {
            xl6OutputPuts(xl6CgenTypedContainerCType(ctx, inner), ctx->out);
        } else {
            xl6OutputPuts(xl6CgenTypeName(ctx, inner), ctx->out);
        }
        xl6OutputPrintf(ctx->out, " value;\n} %s;\n", cTypeName);

        /*
         * `T? as T` 是一次会失败的显式解包。helper 按值接收 nullable，
         * 因而表达式只求值一次；受管理的内部值则由 copy 参数统一决定借用或移动。
         */
        xl6OutputPrintf(
            ctx->out,
            "static %s %s(%s source, bool copy, const char* file, int line, int column)\n"
            "{\n"
            "    %s result;\n"
            "    memset(&result, 0, sizeof(result));\n"
            "    if ( !source.hasValue ) {\n"
            "        xl6_error_raise(\"TypeError\", \"cannot cast null nullable value to a non-null type\", file, line, column);\n"
            "        return result;\n"
            "    }\n",
            innerCType,
            unwrapName,
            cTypeName,
            innerCType);
        if ( xl6CgenNullableHasManagedInner(ctx, typeNode) ) {
            if ( xl6CgenIsFunctionType(inner) ) {
                xl6OutputPuts(
                    "    return copy ? xl6_function_copy_value(source.value) : source.value;\n",
                    ctx->out);
            } else {
                xl6OutputPuts(
                    "    if ( !copy ) { return source.value; }\n"
                    "    if ( !xrtTypeCopyValue(",
                    ctx->out);
                xl6CgenEmitXrtTypeDescRef(ctx, inner);
                xl6OutputPuts(
                    ", &result, &source.value) ) { xl6_runtime_fatal(3); }\n"
                    "    return result;\n",
                    ctx->out);
            }
        } else {
            xl6OutputPuts("    (void)copy;\n    return source.value;\n", ctx->out);
        }
        xl6OutputPuts("}\n", ctx->out);
        if ( xl6CgenNullableHasManagedInner(ctx, typeNode) ) {
            char copyName[64];
            char dropName[64];
            char awaitCopyName[72];
            char awaitReleaseName[72];

            xl6CgenNullableCopyHelperName(
                copyName,
                sizeof(copyName),
                typeNode);
            xl6CgenNullableDropHelperName(
                dropName,
                sizeof(dropName),
                typeNode);
            xl6CgenNullableAwaitCopyHelperName(
                awaitCopyName,
                sizeof(awaitCopyName),
                typeNode);
            xl6CgenNullableAwaitReleaseHelperName(
                awaitReleaseName,
                sizeof(awaitReleaseName),
                typeNode);
            xl6OutputPrintf(
                ctx->out,
                "static %s %s(%s source)\n"
                "{\n"
                "    %s result;\n"
                "    memset(&result, 0, sizeof(result));\n"
                "    result.hasValue = source.hasValue;\n"
                "    if ( source.hasValue ) {\n",
                cTypeName,
                copyName,
                cTypeName,
                cTypeName);
            if ( xl6CgenIsFunctionType(inner) ) {
                xl6OutputPuts(
                    "        result.value = xl6_function_copy_value(source.value);\n",
                    ctx->out);
            } else {
                xl6OutputPuts("        if (!xrtTypeCopyValue(", ctx->out);
                xl6CgenEmitXrtTypeDescRef(ctx, inner);
                xl6OutputPuts(
                    ", &result.value, &source.value)) { xl6_runtime_fatal(3); }\n",
                    ctx->out);
            }
            xl6OutputPuts("    }\n    return result;\n}\n", ctx->out);

            xl6OutputPrintf(
                ctx->out,
                "static void %s(%s* value)\n"
                "{\n"
                "    if ( value == 0 || !value->hasValue ) {\n"
                "        return;\n"
                "    }\n",
                dropName,
                cTypeName);
            if ( xl6CgenIsFunctionType(inner) ) {
                xl6OutputPuts(
                    "    xl6_function_release_value(value->value);\n",
                    ctx->out);
            } else {
                xl6OutputPuts("    xrtTypeDropValue(", ctx->out);
                xl6CgenEmitXrtTypeDescRef(ctx, inner);
                xl6OutputPuts(", &value->value);\n", ctx->out);
            }
            xl6OutputPuts(
                "    memset(value, 0, sizeof(*value));\n"
                "}\n",
                ctx->out);
            if ( hasAsyncRuntime ) {
                xl6OutputPrintf(
                    ctx->out,
                    "static %s %s(xfuture* future, const char* file, int line, int column)\n"
                    "{\n"
                    "    %s result;\n"
                    "    void* value;\n"
                    "    memset(&result, 0, sizeof(result));\n"
                    "    value = xl6_future_await_value(future, file, line, column, 1, sizeof(result));\n"
                    "    if ( value != 0 && !xl6_error_has() ) {\n"
                    "        result = %s(*(%s*)value);\n"
                    "    }\n"
                    "    return result;\n"
                    "}\n",
                    cTypeName,
                    awaitCopyName,
                    cTypeName,
                    copyName,
                    cTypeName);
                xl6OutputPrintf(
                    ctx->out,
                    "static %s %s(xfuture* future, const char* file, int line, int column)\n"
                    "{\n"
                    "    %s result = %s(future, file, line, column);\n"
                    "    xl6_future_release_if_needed(future);\n"
                    "    return result;\n"
                    "}\n",
                    cTypeName,
                    awaitReleaseName,
                    cTypeName,
                    awaitCopyName);
            }
        }

        /*
         * var 可以承载任意 nullable：空分支成为 xvalue null，值分支由
         * XRT TypeDesc 统一装箱。帮助函数按值接收 tagged nullable，保证
         * 安全导航、函数调用等复合表达式只求值一次。
         */
        {
            char boxName[64];
            char fromXValueName[64];

            xl6CgenNullableBoxHelperName(
                boxName,
                sizeof(boxName),
                typeNode);
            xl6CgenNullableFromXValueHelperName(
                fromXValueName,
                sizeof(fromXValueName),
                typeNode);
            xl6OutputPrintf(
                ctx->out,
                "static xvalue %s(%s source)\n"
                "{\n"
                "    xvalue result;\n"
                "    if ( !source.hasValue ) { return xvoCreateNull(); }\n",
                boxName,
                cTypeName);
            if ( xl6CgenIsFunctionType(inner) ) {
                xl6OutputPuts(
                    "    return xl6_function_to_xvalue(source.value);\n",
                    ctx->out);
            } else {
                xl6OutputPuts("    result = xrtTypeBoxValue(", ctx->out);
                xl6CgenEmitXrtTypeDescRef(ctx, inner);
                xl6OutputPuts(
                    ", &source.value);\n"
                    "    if ( result == 0 ) { xl6_runtime_fatal(2); }\n"
                    "    return result;\n",
                    ctx->out);
            }
            xl6OutputPuts("}\n", ctx->out);

            xl6OutputPrintf(
                ctx->out,
                "static %s %s(xvalue source)\n"
                "{\n"
                "    %s result;\n"
                "    memset(&result, 0, sizeof(result));\n"
                "    if ( source != 0 && !xvoIsNull(source) ) {\n",
                cTypeName,
                fromXValueName,
                cTypeName);
            if ( xl6CgenIsFunctionType(inner) ) {
                xl6OutputPuts(
                    "        result.value = xl6_function_from_callable(xvoGetCallable(source));\n",
                    ctx->out);
            } else {
                xl6OutputPuts("        if (!xrtTypeUnboxValue(", ctx->out);
                xl6CgenEmitXrtTypeDescRef(ctx, inner);
                xl6OutputPuts(
                    ", source, &result.value)) { xl6_runtime_fatal(3); }\n",
                    ctx->out);
            }
            xl6OutputPuts(
                "        result.hasValue = true;\n"
                "    }\n"
                "    if ( source != 0 ) { xvoUnref(source); }\n"
                "    return result;\n"
                "}\n",
                ctx->out);
        }
    }
    xl6OutputPutc('\n', ctx->out);
}

static void xl6CgenNullableOpsPrefix(
    char* buffer,
    size_t capacity,
    const XL6_AstNode* typeNode)
{
    snprintf(
        buffer,
        capacity,
        "xl6_nullable_ops_%016llx",
        xl6CgenNullableTypeId(typeNode));
}

/*
 * typed container 只借出元素地址。所有 nullable 读取都从这里建立独立所有权，
 * 避免 string/function/enum 等受管理内部值被浅拷贝后与容器重复释放。
 */
static void xl6CgenNullableOwnedCopyHelperName(
    char* buffer,
    size_t capacity,
    const XL6_AstNode* typeNode)
{
    char prefix[80];

    xl6CgenNullableOpsPrefix(prefix, sizeof(prefix), typeNode);
    snprintf(buffer, capacity, "%s_owned_copy", prefix);
}

static void xl6CgenEmitNullableInnerTypeDesc(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode)
{
    xl6CgenEmitXrtTypeDescRef(ctx, xl6CgenNullableInnerType(typeNode));
}

static int xl6CgenNullableStorageIsXValue(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode)
{
    const char* cType = xl6CgenTypeName(ctx, typeNode);
    return cType != NULL && strcmp(cType, "xvalue") == 0;
}

/* xvalue 形态保留 nullable 自身的类型身份，同时复用 XRT 引用计数。 */
static void xl6CgenEmitNullableXValueOps(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* prefix)
{
    XL6_Output* out = ctx->out;

    xl6OutputPrintf(out, "static void %s_init(void* pObj) { if (pObj != 0) { *(xvalue*)pObj = 0; } }\n", prefix);
    xl6OutputPrintf(out, "static void %s_copy(void* pDst, const void* pSrc)\n{\n", prefix);
    xl6OutputPuts("    xvalue value = pSrc != 0 ? *(xvalue const*)pSrc : 0;\n", out);
    xl6OutputPuts("    if (pDst == 0) { return; }\n", out);
    xl6OutputPuts("    if (value != 0 && !xvoIsNull(value)) { xvoAddRef(value); *(xvalue*)pDst = value; }\n", out);
    xl6OutputPuts("    else { *(xvalue*)pDst = 0; }\n}\n", out);
    xl6OutputPrintf(out, "static void %s_move(void* pDst, void* pSrc) { if (pDst != 0) { *(xvalue*)pDst = pSrc != 0 ? *(xvalue*)pSrc : 0; } if (pSrc != 0) { *(xvalue*)pSrc = 0; } }\n", prefix);
    xl6OutputPrintf(out, "static void %s_drop(void* pObj) { xvalue value = pObj != 0 ? *(xvalue*)pObj : 0; if (value != 0) { xvoUnref(value); *(xvalue*)pObj = 0; } }\n", prefix);
    xl6OutputPrintf(out, "static int %s_compare(const void* pA, const void* pB)\n{\n", prefix);
    xl6OutputPuts("    xvalue a = pA != 0 ? *(xvalue const*)pA : 0;\n", out);
    xl6OutputPuts("    xvalue b = pB != 0 ? *(xvalue const*)pB : 0;\n", out);
    xl6OutputPuts("    bool an = a == 0 || xvoIsNull(a);\n", out);
    xl6OutputPuts("    bool bn = b == 0 || xvoIsNull(b);\n", out);
    xl6OutputPuts("    if (an || bn) { return an == bn ? 0 : (an ? -1 : 1); }\n", out);
    xl6OutputPuts("    return xrtTypeCompareValue(", out);
    xl6CgenEmitNullableInnerTypeDesc(ctx, typeNode);
    xl6OutputPuts(", &a, &b);\n}\n", out);
    xl6OutputPrintf(out, "static uint64 %s_hash(const void* pObj)\n{\n", prefix);
    xl6OutputPuts("    xvalue value = pObj != 0 ? *(xvalue const*)pObj : 0;\n", out);
    xl6OutputPuts("    if (value == 0 || xvoIsNull(value)) { return 0; }\n", out);
    xl6OutputPuts("    return xrtTypeHashValue(", out);
    xl6CgenEmitNullableInnerTypeDesc(ctx, typeNode);
    xl6OutputPuts(", &value);\n}\n", out);
    xl6OutputPrintf(out, "static str %s_to_string(const void* pObj, uint32* pSize)\n{\n", prefix);
    xl6OutputPuts("    xvalue value = pObj != 0 ? *(xvalue const*)pObj : 0;\n", out);
    xl6OutputPuts("    if (value == 0 || xvoIsNull(value)) { if (pSize != 0) { *pSize = 4u; } return xrtCopyStr((str)\"null\", 4u); }\n", out);
    xl6OutputPuts("    return xvoToString(value, pSize);\n}\n", out);
    xl6OutputPrintf(out, "static xvalue %s_box(const void* pObj, const xrt_type_desc* pType)\n{\n", prefix);
    xl6OutputPuts("    xvalue value = pObj != 0 ? *(xvalue const*)pObj : 0; (void)pType;\n", out);
    xl6OutputPuts("    if (value == 0 || xvoIsNull(value)) { return xvoCreateNull(); }\n", out);
    xl6OutputPuts("    xvoAddRef(value); return value;\n}\n", out);
    xl6OutputPrintf(out, "static bool %s_unbox(xvalue value, void* pOut, const xrt_type_desc* pType)\n{\n", prefix);
    xl6OutputPuts("    const xrt_type_desc* actual; const xrt_type_desc* target; bool compatible;\n", out);
    xl6OutputPuts("    if (pOut == 0) { return false; } *(xvalue*)pOut = 0;\n", out);
    xl6OutputPuts("    if (value == 0 || xvoIsNull(value)) { return true; }\n", out);
    xl6OutputPuts("    actual = xvoTypeDesc(value); target = xrtTypeGenericArg(pType, 0u);\n", out);
    xl6OutputPuts("    compatible = actual != 0 && target != 0 && (target->Kind == XL6_XRT_TYPE_KIND_PROTOCOL ? xrtProtocolLookup(target->TypeId, actual->TypeId) != 0 : xrtTypeIsA(actual, target));\n", out);
    xl6OutputPuts("    if (!compatible) { return false; } xvoAddRef(value); *(xvalue*)pOut = value; return true;\n}\n", out);
    xl6OutputPrintf(
        out,
        "static const xrt_type_ops %s_table = { %s_init, %s_copy, %s_move, %s_drop, %s_compare, %s_hash, %s_to_string, %s_box, %s_unbox, %s_copy };\n",
        prefix, prefix, prefix, prefix, prefix, prefix, prefix, prefix, prefix, prefix, prefix);
}

/* 指针形态以 NULL 表示空值，非空值的生命周期全部委托给内部 TypeDesc。 */
static void xl6CgenEmitNullablePointerOps(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* prefix,
    const char* cType)
{
    XL6_Output* out = ctx->out;

    xl6OutputPrintf(out, "static void %s_init(void* pObj) { if (pObj != 0) { *(%s*)pObj = 0; } }\n", prefix, cType);
    xl6OutputPrintf(out, "static void %s_copy(void* pDst, const void* pSrc)\n{\n", prefix);
    xl6OutputPrintf(out, "    %s value = pSrc != 0 ? *(%s const*)pSrc : 0;\n", cType, cType);
    xl6OutputPuts("    if (pDst == 0) { return; } if (value == 0) { *(", out);
    xl6OutputPrintf(out, "%s*)pDst = 0; return; }\n", cType);
    xl6OutputPuts("    if (!xrtTypeCopyValue(", out);
    xl6CgenEmitNullableInnerTypeDesc(ctx, typeNode);
    xl6OutputPuts(", pDst, &value)) { xl6_runtime_fatal(3); }\n}\n", out);
    xl6OutputPrintf(out, "static void %s_move(void* pDst, void* pSrc) { if (pDst != 0) { *(%s*)pDst = pSrc != 0 ? *(%s*)pSrc : 0; } if (pSrc != 0) { *(%s*)pSrc = 0; } }\n", prefix, cType, cType, cType);
    xl6OutputPrintf(out, "static void %s_drop(void* pObj) { if (pObj != 0 && *(%s*)pObj != 0) { xrtTypeDropValue(", prefix, cType);
    xl6CgenEmitNullableInnerTypeDesc(ctx, typeNode);
    xl6OutputPrintf(out, ", pObj); *(%s*)pObj = 0; } }\n", cType);
    xl6OutputPrintf(out, "static int %s_compare(const void* pA, const void* pB)\n{\n", prefix);
    xl6OutputPrintf(out, "    %s a = pA != 0 ? *(%s const*)pA : 0; %s b = pB != 0 ? *(%s const*)pB : 0;\n", cType, cType, cType, cType);
    xl6OutputPuts("    if (a == 0 || b == 0) { return a == b ? 0 : (a == 0 ? -1 : 1); } return xrtTypeCompareValue(", out);
    xl6CgenEmitNullableInnerTypeDesc(ctx, typeNode);
    xl6OutputPuts(", &a, &b);\n}\n", out);
    xl6OutputPrintf(out, "static uint64 %s_hash(const void* pObj) { %s value = pObj != 0 ? *(%s const*)pObj : 0; return value != 0 ? xrtTypeHashValue(", prefix, cType, cType);
    xl6CgenEmitNullableInnerTypeDesc(ctx, typeNode);
    xl6OutputPuts(", &value) : 0; }\n", out);
    xl6OutputPrintf(out, "static str %s_to_string(const void* pObj, uint32* pSize) { %s value = pObj != 0 ? *(%s const*)pObj : 0; if (value == 0) { if (pSize != 0) { *pSize = 4u; } return xrtCopyStr((str)\"null\", 4u); } return xrtTypeToStringValue(", prefix, cType, cType);
    xl6CgenEmitNullableInnerTypeDesc(ctx, typeNode);
    xl6OutputPuts(", &value, pSize); }\n", out);
    xl6OutputPrintf(out, "static xvalue %s_box(const void* pObj, const xrt_type_desc* pType) { %s value = pObj != 0 ? *(%s const*)pObj : 0; (void)pType; return value != 0 ? xrtTypeBoxValue(", prefix, cType, cType);
    xl6CgenEmitNullableInnerTypeDesc(ctx, typeNode);
    xl6OutputPuts(", &value) : xvoCreateNull(); }\n", out);
    xl6OutputPrintf(out, "static bool %s_unbox(xvalue value, void* pOut, const xrt_type_desc* pType) { (void)pType; if (pOut == 0) { return false; } memset(pOut, 0, sizeof(%s)); if (value == 0 || xvoIsNull(value)) { return true; } return xrtTypeUnboxValue(", prefix, cType);
    xl6CgenEmitNullableInnerTypeDesc(ctx, typeNode);
    xl6OutputPuts(", value, pOut); }\n", out);
    xl6OutputPrintf(out, "static void %s_clone(void* pDst, const void* pSrc) { if (pDst == 0) { return; } if (pSrc == 0 || *(%s const*)pSrc == 0) { *(%s*)pDst = 0; return; } if (!xrtTypeCloneValue(", prefix, cType, cType);
    xl6CgenEmitNullableInnerTypeDesc(ctx, typeNode);
    xl6OutputPuts(", pDst, pSrc)) { xl6_runtime_fatal(3); } }\n", out);
    xl6OutputPrintf(
        out,
        "static const xrt_type_ops %s_table = { %s_init, %s_copy, %s_move, %s_drop, %s_compare, %s_hash, %s_to_string, %s_box, %s_unbox, %s_clone };\n",
        prefix, prefix, prefix, prefix, prefix, prefix, prefix, prefix, prefix, prefix, prefix);
}

static void xl6CgenEmitNullableTaggedOps(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* prefix,
    const char* cType)
{
    const XL6_AstNode* inner = xl6CgenNullableInnerType(typeNode);
    XL6_Output* out = ctx->out;
    int functionInner = xl6CgenIsFunctionType(inner);

    xl6OutputPrintf(out, "static void %s_init(void* pObj) { if (pObj != 0) { memset(pObj, 0, sizeof(%s)); } }\n", prefix, cType);
    xl6OutputPrintf(out, "static void %s_copy(void* pDst, const void* pSrc)\n{\n", prefix);
    xl6OutputPrintf(out, "    %s* d = (%s*)pDst; const %s* s = (const %s*)pSrc; if (d == 0) { return; } memset(d, 0, sizeof(*d)); if (s == 0 || !s->hasValue) { return; } d->hasValue = true;\n", cType, cType, cType, cType);
    if ( functionInner ) {
        xl6OutputPuts("    d->value = xl6_function_copy_value(s->value);\n", out);
    } else {
        xl6OutputPuts("    if (!xrtTypeCopyValue(", out);
        xl6CgenEmitNullableInnerTypeDesc(ctx, typeNode);
        xl6OutputPuts(", &d->value, &s->value)) { xl6_runtime_fatal(3); }\n", out);
    }
    xl6OutputPuts("}\n", out);
    xl6OutputPrintf(out, "static void %s_move(void* pDst, void* pSrc) { if (pDst != 0) { if (pSrc != 0) { memcpy(pDst, pSrc, sizeof(%s)); } else { memset(pDst, 0, sizeof(%s)); } } if (pSrc != 0) { memset(pSrc, 0, sizeof(%s)); } }\n", prefix, cType, cType, cType);
    xl6OutputPrintf(out, "static void %s_drop(void* pObj) { %s* value = (%s*)pObj; if (value == 0 || !value->hasValue) { return; } ", prefix, cType, cType);
    if ( functionInner ) {
        xl6OutputPuts("xl6_function_release_value(value->value); ", out);
    } else {
        xl6OutputPuts("xrtTypeDropValue(", out);
        xl6CgenEmitNullableInnerTypeDesc(ctx, typeNode);
        xl6OutputPuts(", &value->value); ", out);
    }
    xl6OutputPuts("memset(value, 0, sizeof(*value)); }\n", out);
    xl6OutputPrintf(out, "static int %s_compare(const void* pA, const void* pB)\n{\n", prefix);
    xl6OutputPrintf(out, "    const %s* a = (const %s*)pA; const %s* b = (const %s*)pB; bool ah = a != 0 && a->hasValue; bool bh = b != 0 && b->hasValue; if (!ah || !bh) { return ah == bh ? 0 : (ah ? 1 : -1); }\n", cType, cType, cType, cType);
    if ( functionInner ) {
        xl6OutputPuts("    if (a->value.signature != b->value.signature) { return a->value.signature < b->value.signature ? -1 : 1; } return a->value.env == b->value.env ? 0 : ((uintptr_t)a->value.env < (uintptr_t)b->value.env ? -1 : 1);\n", out);
    } else {
        xl6OutputPuts("    return xrtTypeCompareValue(", out);
        xl6CgenEmitNullableInnerTypeDesc(ctx, typeNode);
        xl6OutputPuts(", &a->value, &b->value);\n", out);
    }
    xl6OutputPuts("}\n", out);
    xl6OutputPrintf(out, "static uint64 %s_hash(const void* pObj) { const %s* value = (const %s*)pObj; if (value == 0 || !value->hasValue) { return 0; } ", prefix, cType, cType);
    if ( functionInner ) {
        xl6OutputPuts("uint64 hash = xrtHash64(&value->value.signature, sizeof(value->value.signature)); return hash ^ xrtHash64(&value->value.env, sizeof(value->value.env));", out);
    } else {
        xl6OutputPuts("return xrtTypeHashValue(", out);
        xl6CgenEmitNullableInnerTypeDesc(ctx, typeNode);
        xl6OutputPuts(", &value->value);", out);
    }
    xl6OutputPuts(" }\n", out);
    xl6OutputPrintf(out, "static str %s_to_string(const void* pObj, uint32* pSize) { const %s* value = (const %s*)pObj; if (value == 0 || !value->hasValue) { if (pSize != 0) { *pSize = 4u; } return xrtCopyStr((str)\"null\", 4u); } ", prefix, cType, cType);
    if ( functionInner ) {
        xl6OutputPuts("if (pSize != 0) { *pSize = 8u; } return xrtCopyStr((str)\"function\", 8u);", out);
    } else {
        xl6OutputPuts("return xrtTypeToStringValue(", out);
        xl6CgenEmitNullableInnerTypeDesc(ctx, typeNode);
        xl6OutputPuts(", &value->value, pSize);", out);
    }
    xl6OutputPuts(" }\n", out);
    xl6OutputPrintf(out, "static xvalue %s_box(const void* pObj, const xrt_type_desc* pType) { const %s* value = (const %s*)pObj; (void)pType; if (value == 0 || !value->hasValue) { return xvoCreateNull(); } ", prefix, cType, cType);
    if ( functionInner ) {
        xl6OutputPuts("return xl6_function_to_xvalue(value->value);", out);
    } else {
        xl6OutputPuts("return xrtTypeBoxValue(", out);
        xl6CgenEmitNullableInnerTypeDesc(ctx, typeNode);
        xl6OutputPuts(", &value->value);", out);
    }
    xl6OutputPuts(" }\n", out);
    xl6OutputPrintf(out, "static bool %s_unbox(xvalue value, void* pOut, const xrt_type_desc* pType) { %s* result = (%s*)pOut; (void)pType; if (result == 0) { return false; } memset(result, 0, sizeof(*result)); if (value == 0 || xvoIsNull(value)) { return true; } ", prefix, cType, cType);
    if ( functionInner ) {
        xl6OutputPuts("if (xvoGetCallable(value) == 0) { return false; } result->value = xl6_function_from_callable(xvoGetCallable(value)); result->hasValue = true; return true;", out);
    } else {
        xl6OutputPuts("if (!xrtTypeUnboxValue(", out);
        xl6CgenEmitNullableInnerTypeDesc(ctx, typeNode);
        xl6OutputPuts(", value, &result->value)) { return false; } result->hasValue = true; return true;", out);
    }
    xl6OutputPuts(" }\n", out);
    xl6OutputPrintf(out, "static void %s_clone(void* pDst, const void* pSrc)\n{\n", prefix);
    xl6OutputPrintf(out, "    %s* d = (%s*)pDst; const %s* s = (const %s*)pSrc; if (d == 0) { return; } memset(d, 0, sizeof(*d)); if (s == 0 || !s->hasValue) { return; } d->hasValue = true;\n", cType, cType, cType, cType);
    if ( functionInner ) {
        xl6OutputPuts("    d->value = xl6_function_copy_value(s->value);\n", out);
    } else {
        xl6OutputPuts("    if (!xrtTypeCloneValue(", out);
        xl6CgenEmitNullableInnerTypeDesc(ctx, typeNode);
        xl6OutputPuts(", &d->value, &s->value)) { xl6_runtime_fatal(3); }\n", out);
    }
    xl6OutputPuts("}\n", out);
    xl6OutputPrintf(
        out,
        "static const xrt_type_ops %s_table = { %s_init, %s_copy, %s_move, %s_drop, %s_compare, %s_hash, %s_to_string, %s_box, %s_unbox, %s_clone };\n",
        prefix, prefix, prefix, prefix, prefix, prefix, prefix, prefix, prefix, prefix, prefix);
}

static void xl6CgenEmitNullableTypeDesc(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode)
{
    const XL6_AstNode* inner = xl6CgenNullableInnerType(typeNode);
    const char* cType = xl6CgenTypeName(ctx, typeNode);
    char prefix[80];
    char typeText[512];
    unsigned long long typeId = xl6CgenTypeIdForTypeNode(ctx, typeNode);

    xl6CgenNullableOpsPrefix(prefix, sizeof(prefix), typeNode);
    if ( xl6CgenNullableStorageIsXValue(ctx, typeNode) ) {
        xl6CgenEmitNullableXValueOps(ctx, typeNode, prefix);
    } else if ( xl6CgenNullableUsesPointer(typeNode) ) {
        xl6CgenEmitNullablePointerOps(ctx, typeNode, prefix, cType);
    } else {
        xl6CgenEmitNullableTaggedOps(ctx, typeNode, prefix, cType);
    }

    /*
     * C 表达式路径需要一个返回值形式的复制入口；实际复制仍完全委托给
     * nullable 自己的 type ops，保证索引、循环和装箱遵守同一规则。
     */
    xl6OutputPrintf(
        ctx->out,
        "static %s %s_owned_copy(%s source)\n"
        "{\n"
        "    %s result;\n"
        "    memset(&result, 0, sizeof(result));\n"
        "    %s_copy(&result, &source);\n"
        "    return result;\n"
        "}\n",
        cType,
        prefix,
        cType,
        cType,
        prefix);

    typeText[0] = '\0';
    xl6CgenTypeText(ctx, typeNode, typeText, sizeof(typeText));
    xl6OutputPuts("static const xrt_type_desc* ", ctx->out);
    xl6CgenNullableTypeDescSymbol(ctx->out, ctx, typeNode);
    xl6OutputPuts("_args[1u];\n", ctx->out);
    xl6OutputPuts("static const xrt_generic_type ", ctx->out);
    xl6CgenNullableTypeDescSymbol(ctx->out, ctx, typeNode);
    xl6OutputPuts("_generic = { 1u, ", ctx->out);
    xl6CgenNullableTypeDescSymbol(ctx->out, ctx, typeNode);
    xl6OutputPuts("_args };\n", ctx->out);
    xl6OutputPuts("static const xrt_type_desc ", ctx->out);
    xl6CgenNullableTypeDescSymbol(ctx->out, ctx, typeNode);
    xl6OutputPuts("_value = {\n", ctx->out);
    xl6OutputPrintf(ctx->out, "    0x%08XU, XL6_XRT_TYPE_KIND_NULLABLE, ", (unsigned int)(typeId & 0xFFFFFFFFULL));
    xl6CgenCStringLiteral(ctx->out, typeText);
    xl6OutputPrintf(ctx->out, ", %uu, ", (unsigned)strlen(typeText));
    xl6CgenCStringLiteral(ctx->out, cType);
    xl6OutputPrintf(ctx->out, ", %uu, sizeof(%s), _Alignof(%s), &%s_table, 0, &", (unsigned)strlen(cType), cType, cType, prefix);
    xl6CgenNullableTypeDescSymbol(ctx->out, ctx, typeNode);
    xl6OutputPuts("_generic, 0\n};\n", ctx->out);
    xl6OutputPuts("static xrt_once ", ctx->out);
    xl6CgenNullableTypeDescSymbol(ctx->out, ctx, typeNode);
    xl6OutputPuts("_once = XRT_ONCE_INIT;\nstatic void ", ctx->out);
    xl6CgenNullableTypeDescSymbol(ctx->out, ctx, typeNode);
    xl6OutputPuts("_init(void* pContext) { (void)pContext; ", ctx->out);
    xl6CgenNullableTypeDescSymbol(ctx->out, ctx, typeNode);
    xl6OutputPuts("_args[0] = ", ctx->out);
    xl6CgenEmitXrtTypeDescRef(ctx, inner);
    xl6OutputPuts("; }\nstatic const xrt_type_desc* ", ctx->out);
    xl6CgenNullableTypeDescSymbol(ctx->out, ctx, typeNode);
    xl6OutputPuts("(void) { xrtOnceRunGraph(&", ctx->out);
    xl6CgenNullableTypeDescSymbol(ctx->out, ctx, typeNode);
    xl6OutputPuts("_once, ", ctx->out);
    xl6CgenNullableTypeDescSymbol(ctx->out, ctx, typeNode);
    xl6OutputPuts("_init, 0); return &", ctx->out);
    xl6CgenNullableTypeDescSymbol(ctx->out, ctx, typeNode);
    xl6OutputPuts("_value; }\n\n", ctx->out);
}

static void xl6CgenEmitNullableTypeDescs(XL6_CgenContext* ctx)
{
    size_t i;

    if ( ctx == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->nullableTypeCount; ++i ) {
        xl6CgenEmitNullableTypeDesc(ctx, ctx->nullableTypes[i].typeNode);
    }
}

/*
    tagged nullable 的构造只在这里定义。
    null 生成全零值，T 自动提升为 { true, value }，T? 则保持原值。
*/
static void xl6CgenNullableTaggedValueExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* targetType,
    const XL6_AstNode* expr)
{
    const XL6_AstNode* innerType;
    XL6_AstNode* sourceType;
    char cTypeName[64];
    char copyName[64];
    int managed;

    if ( ctx == NULL || !xl6CgenNullableUsesTaggedValue(targetType) ) {
        xl6CgenExpr(ctx, expr);
        return;
    }

    xl6CgenNullableCTypeName(cTypeName, sizeof(cTypeName), targetType);
    if ( expr == NULL || expr->kind == XL6_AST_NULL ) {
        xl6OutputPrintf(ctx->out, "(%s){0}", cTypeName);
        return;
    }

    innerType = xl6CgenNullableInnerType(targetType);
    managed = xl6CgenNullableHasManagedInner(ctx, targetType);
    xl6CgenNullableCopyHelperName(copyName, sizeof(copyName), targetType);
    sourceType = xl6CgenCloneExprTypeForInference(ctx, expr);
    if ( sourceType != NULL && xl6CgenTypeNodeEqual(sourceType, targetType) ) {
        if ( managed &&
             xl6CgenHirOwnership(ctx, expr) != XL6_HIR_OWNERSHIP_MOVE ) {
            xl6OutputPrintf(ctx->out, "%s(", copyName);
            xl6CgenExpr(ctx, expr);
            xl6OutputPutc(')', ctx->out);
        } else {
            xl6CgenExpr(ctx, expr);
        }
        xl6AstFree(sourceType);
        return;
    }
    xl6AstFree(sourceType);

    xl6OutputPrintf(ctx->out, "(%s){ true, ", cTypeName);
    if ( managed && xl6CgenIsFunctionType(innerType) ) {
        if ( xl6CgenExprIsFunctionBorrow(ctx, expr) ) {
            xl6OutputPuts("xl6_function_copy_value(", ctx->out);
            xl6CgenFunctionValueExpr(ctx, expr, innerType);
            xl6OutputPutc(')', ctx->out);
        } else {
            xl6CgenFunctionValueExpr(ctx, expr, innerType);
        }
    } else if ( managed &&
                xl6CgenIsTaggedEnumType(ctx, innerType) ) {
        xl6CgenTaggedEnumValueExpr(ctx, innerType, expr);
    } else {
        xl6CgenExpr(ctx, expr);
    }
    xl6OutputPuts(" }", ctx->out);
}

static int xl6CgenNullableTaggedDecl(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node,
    const XL6_AstNode* typeNode,
    const char* cName,
    const XL6_AstNode* initExpr)
{
    char cTypeName[64];

    if ( ctx == NULL || node == NULL || cName == NULL ||
         !xl6CgenNullableUsesTaggedValue(typeNode) ) {
        return 0;
    }

    xl6CgenNullableCTypeName(cTypeName, sizeof(cTypeName), typeNode);
    if ( initExpr != NULL &&
         xl6CgenDeclTryStaticNullCoalesceInit(
             ctx,
             typeNode,
             cTypeName,
             cName,
             initExpr) ) {
        xl6CgenAddVarType(ctx, node->text, typeNode);
        return 1;
    }
    xl6OutputPrintf(ctx->out, "%s %s = ", cTypeName, cName);
    xl6CgenNullableTaggedValueExpr(ctx, typeNode, initExpr);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenAddVarType(ctx, node->text, typeNode);
    return 1;
}

static int xl6CgenWeakDecl(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node,
    const XL6_AstNode* typeNode,
    const char* cName,
    const XL6_AstNode* initExpr)
{
    XL6_AstNode* sourceType;

    if ( ctx == NULL || node == NULL || cName == NULL || !xl6CgenIsWeakType(typeNode) ) {
        return 0;
    }

    xl6OutputPrintf(ctx->out, "xrt_value_weak %s = {0};\n", cName);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvoWeakInit(&%s);\n", cName);
    if ( initExpr != NULL && initExpr->kind != XL6_AST_NULL ) {
        sourceType = xl6CgenCloneExprTypeForInference(ctx, initExpr);
        xl6CgenIndent(ctx);
        if ( xl6CgenIsWeakType(sourceType) ) {
            xl6OutputPrintf(ctx->out, "xvoWeakCopy(&%s, &(", cName);
            xl6CgenExpr(ctx, initExpr);
            xl6OutputPuts("));\n", ctx->out);
        } else {
            xl6OutputPrintf(ctx->out, "xvoWeakSet(&%s, ", cName);
            xl6CgenExpr(ctx, initExpr);
            xl6OutputPuts(");\n", ctx->out);
        }
        xl6AstFree(sourceType);
    }

    xl6CgenAddVarType(ctx, node->text, typeNode);
    xl6CgenAddWeakCleanup(ctx, node->text, typeNode);
    return 1;
}

/*
    生成一个拥有独立弱引用计数的值。
    该表达式用于按值参数和返回值，接收方必须最终调用 xvoWeakUnit。
*/
static void xl6CgenWeakValueExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* expr)
{
    XL6_AstNode* sourceType;

    if ( expr == NULL || expr->kind == XL6_AST_NULL ) {
        xl6OutputPuts("(xrt_value_weak){0}", ctx->out);
        return;
    }
    sourceType = xl6CgenCloneExprTypeForInference(ctx, expr);
    if ( xl6CgenIsWeakType(sourceType) ) {
        xl6OutputPuts("xvoWeakClone(&(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts("))", ctx->out);
    } else {
        xl6OutputPuts("xvoWeakCreate(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPutc(')', ctx->out);
    }
    xl6AstFree(sourceType);
}

/*
    weak<T>.lock() 按值接收弱引用，因而变量、字段和返回弱引用的表达式都能
    使用同一条生成路径。返回的 xvalue 已拥有一个强引用。
*/
static int xl6CgenWeakLockCall(
    XL6_CgenContext* ctx,
    const XL6_AstNode* call,
    const XL6_AstNode* callee)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* args;
    XL6_AstNode* receiverType;
    int isWeak;

    if ( ctx == NULL || call == NULL || callee == NULL ||
         callee->kind != XL6_AST_MEMBER ||
         !xl6CTextEquals(callee->text, "lock") ||
         callee->childCount == 0 ) {
        return 0;
    }

    receiver = callee->children[0];
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    isWeak = xl6CgenIsWeakType(receiverType);
    xl6AstFree(receiverType);
    if ( !isWeak ) {
        return 0;
    }

    args = call->childCount > 1 ? call->children[1] : NULL;
    if ( args != NULL && args->childCount != 0 ) {
        xl6CgenError(ctx, call, "weak<T>.lock() does not accept arguments");
        xl6OutputPuts("0", ctx->out);
        return 1;
    }

    xl6OutputPuts("xvoWeakLockValue(", ctx->out);
    xl6CgenExpr(ctx, receiver);
    xl6OutputPutc(')', ctx->out);
    return 1;
}

static int xl6CgenNullableNullComparison(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node)
{
    const XL6_AstNode* valueExpr;
    XL6_AstNode* valueType;
    int notEqual;

    if ( ctx == NULL || node == NULL || node->kind != XL6_AST_BINARY ||
         node->childCount < 2 ||
         (!xl6CTextEquals(node->text, "==") &&
          !xl6CTextEquals(node->text, "!=")) ) {
        return 0;
    }
    if ( node->children[0] != NULL && node->children[0]->kind == XL6_AST_NULL ) {
        valueExpr = node->children[1];
    } else if ( node->children[1] != NULL &&
                node->children[1]->kind == XL6_AST_NULL ) {
        valueExpr = node->children[0];
    } else {
        return 0;
    }

    valueType = xl6CgenCloneExprTypeForInference(ctx, valueExpr);
    if ( !xl6CgenIsNullableType(valueType) ) {
        xl6AstFree(valueType);
        return 0;
    }

    notEqual = xl6CTextEquals(node->text, "!=");
    xl6OutputPutc('(', ctx->out);
    if ( xl6CgenNullableUsesTaggedValue(valueType) ) {
        if ( !notEqual ) {
            xl6OutputPutc('!', ctx->out);
        }
        xl6OutputPutc('(', ctx->out);
        xl6CgenExpr(ctx, valueExpr);
        xl6OutputPuts(").hasValue", ctx->out);
    } else if ( xl6CgenNullableUsesXValue(valueType) ) {
        if ( notEqual ) {
            xl6OutputPutc('!', ctx->out);
        }
        xl6OutputPuts("xvoIsNull(", ctx->out);
        xl6CgenExpr(ctx, valueExpr);
        xl6OutputPutc(')', ctx->out);
    } else {
        xl6CgenExpr(ctx, valueExpr);
        xl6OutputPuts(notEqual ? " != 0" : " == 0", ctx->out);
    }
    xl6OutputPutc(')', ctx->out);
    xl6AstFree(valueType);
    return 1;
}

static int xl6CgenNullableNarrowedNameExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node)
{
    const XL6_AstNode* declaredType;
    const XL6_AstNode* innerType;
    XL6_AstNode* expressionType;
    int narrowed;

    if ( ctx == NULL || node == NULL || node->kind != XL6_AST_NAME ||
         node->text == NULL ) {
        return 0;
    }
    declaredType = xl6CgenFindVarType(ctx, node->text);
    if ( !xl6CgenNullableUsesTaggedValue(declaredType) ) {
        return 0;
    }
    innerType = xl6CgenNullableInnerType(declaredType);
    expressionType = xl6CgenCloneExprTypeForInference(ctx, node);
    narrowed = innerType != NULL && expressionType != NULL &&
               xl6CgenTypeNodeEqual(innerType, expressionType);
    xl6AstFree(expressionType);
    if ( !narrowed ) {
        return 0;
    }

    xl6OutputPutc('(', ctx->out);
    xl6CgenNamedValueReference(ctx, node->text);
    xl6OutputPuts(").value", ctx->out);
    return 1;
}

static void xl6CgenFreeNullableTypes(XL6_CgenContext* ctx)
{
    size_t i;

    if ( ctx == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->nullableTypeCount; ++i ) {
        xl6AstFree(ctx->nullableTypes[i].typeNode);
    }
    xrtFree(ctx->nullableTypes);
    ctx->nullableTypes = NULL;
    ctx->nullableTypeCount = 0;
    ctx->nullableTypeCapacity = 0;
}
