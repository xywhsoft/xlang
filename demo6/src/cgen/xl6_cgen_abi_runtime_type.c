static void xl6CgenEmitRuntimeTypeDescDependencyProto(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* args;
    const char* name;
    char cTypeName[256];
    size_t i;

    concreteType = xl6CgenSubstituteGenericType(ctx, typeNode);
    concreteType = xl6CgenResolveAliasType(ctx, concreteType);
    name = concreteType != NULL ? concreteType->text : NULL;

    if ( concreteType != NULL && xl6CgenIsRuntimeContainerTypeDesc(ctx, concreteType) ) {
        args = xl6CgenTypeNodeGenericArgs(concreteType);
        if ( args != NULL ) {
            for ( i = 0; i < args->childCount; ++i ) {
                xl6CgenEmitRuntimeTypeDescDependencyProto(ctx, args->children[i]);
            }
        }
        return;
    }

    /*
        容器描述初始化函数会直接读取元素类型描述。enum 的完整定义虽然稍后才发射，
        但函数原型必须先出现，递归 enum -> container<enum> 才不会产生隐式 int 声明。
    */
    if ( xl6CgenIsNumericEnumType(ctx, concreteType) ||
         xl6CgenIsTaggedEnumType(ctx, concreteType) ) {
        xl6CgenEnumTypeDescProto(ctx, concreteType);
        return;
    }
    if ( xl6CgenIsClassType(ctx, concreteType) ) {
        xl6CgenClassTypeRefName(cTypeName, sizeof(cTypeName), concreteType);
    } else if ( xl6CgenIsPlainRecordType(ctx, concreteType) ) {
        xl6CgenUnionTypeTextName(cTypeName, sizeof(cTypeName), name);
    } else if ( xl6CgenIsNativeBackedClassType(ctx, concreteType) ) {
        xl6CgenNativeBackedTypeRefName(cTypeName, sizeof(cTypeName), name);
    } else {
        return;
    }

    xl6OutputPuts("static const xrt_type_desc* ", ctx->out);
    xl6CgenTypeDescSymbol(ctx->out, cTypeName);
    xl6OutputPuts("(void);\n", ctx->out);
}

static void xl6CgenEmitRuntimeXValueTypeOps(XL6_CgenContext* ctx)
{
    XL6_Output* out;
    int hasSpecialCollections;
    size_t i;

    if ( ctx == NULL || ctx->out == NULL || ctx->runtimeTypeDescCount == 0 ) {
        return;
    }

    out = ctx->out;
    hasSpecialCollections = 0;
    for ( i = 0; i < ctx->runtimeTypeDescCount; ++i ) {
        if ( xl6CgenIsSpecialCollectionType(ctx, ctx->runtimeTypeDescs[i].typeNode) ) {
            hasSpecialCollections = 1;
            break;
        }
    }
    xl6OutputPuts("#ifndef XL6_XRT_TYPE_KIND_CLASS\n", out);
    xl6OutputPuts("#define XL6_XRT_TYPE_KIND_CLASS 17u\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("#ifndef XL6_RUNTIME_XVALUE_TYPE_OPS\n", out);
    xl6OutputPuts("#define XL6_RUNTIME_XVALUE_TYPE_OPS\n", out);
    xl6OutputPuts("static void xl6_runtime_xvalue_init(void* pObj)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( pObj != 0 ) {\n", out);
    xl6OutputPuts("        *(xvalue*)pObj = xvoCreateNull();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void xl6_runtime_xvalue_copy(void* pDst, const void* pSrc)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue value = pSrc != 0 ? *(xvalue const*)pSrc : 0;\n", out);
    xl6OutputPuts("    if ( pDst != 0 ) {\n", out);
    xl6OutputPuts("        if ( value != 0 ) { xvoAddRef(value); }\n", out);
    xl6OutputPuts("        *(xvalue*)pDst = value != 0 ? value : xvoCreateNull();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void xl6_runtime_xvalue_move(void* pDst, void* pSrc)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( pDst != 0 ) {\n", out);
    xl6OutputPuts("        *(xvalue*)pDst = pSrc != 0 ? *(xvalue*)pSrc : 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( pSrc != 0 ) {\n", out);
    xl6OutputPuts("        *(xvalue*)pSrc = 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void xl6_runtime_xvalue_drop(void* pObj)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue value;\n", out);
    xl6OutputPuts("    if ( pObj == 0 ) {\n", out);
    xl6OutputPuts("        return;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    value = *(xvalue*)pObj;\n", out);
    xl6OutputPuts("    if ( value != 0 ) {\n", out);
    xl6OutputPuts("        xvoUnref(value);\n", out);
    xl6OutputPuts("        *(xvalue*)pObj = 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_runtime_xvalue_compare_text(xvalue left, xvalue right)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    uint32 leftSize = 0;\n", out);
    xl6OutputPuts("    uint32 rightSize = 0;\n", out);
    xl6OutputPuts("    str leftText = xvoToString(left, &leftSize);\n", out);
    xl6OutputPuts("    str rightText = xvoToString(right, &rightSize);\n", out);
    xl6OutputPuts("    uint32 minSize = leftSize < rightSize ? leftSize : rightSize;\n", out);
    xl6OutputPuts("    int result = minSize > 0 ? memcmp(leftText, rightText, minSize) : 0;\n", out);
    xl6OutputPuts("    if ( result == 0 ) {\n", out);
    xl6OutputPuts("        result = (leftSize > rightSize) - (leftSize < rightSize);\n", out);
    xl6OutputPuts("    } else {\n", out);
    xl6OutputPuts("        result = result > 0 ? 1 : -1;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( leftText != 0 ) { xrtFree(leftText); }\n", out);
    xl6OutputPuts("    if ( rightText != 0 ) { xrtFree(rightText); }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_runtime_xvalue_compare(const void* pA, const void* pB)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue left = pA != 0 ? *(xvalue const*)pA : 0;\n", out);
    xl6OutputPuts("    xvalue right = pB != 0 ? *(xvalue const*)pB : 0;\n", out);
    xl6OutputPuts("    const xrt_type_desc* leftType;\n", out);
    xl6OutputPuts("    const xrt_type_desc* rightType;\n", out);
    xl6OutputPuts("    int cmp;\n", out);
    xl6OutputPuts("    if ( left == right ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    leftType = xvoTypeDesc(left);\n", out);
    xl6OutputPuts("    rightType = xvoTypeDesc(right);\n", out);
    xl6OutputPuts("    if ( !xrtTypeSame(leftType, rightType) ) {\n", out);
    xl6OutputPuts("        unsigned long long leftId = leftType != 0 ? (unsigned long long)leftType->TypeId : 0ULL;\n", out);
    xl6OutputPuts("        unsigned long long rightId = rightType != 0 ? (unsigned long long)rightType->TypeId : 0ULL;\n", out);
    xl6OutputPuts("        if ( leftId != rightId ) {\n", out);
    xl6OutputPuts("            return (leftId > rightId) - (leftId < rightId);\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    cmp = xvoBasicCompare(left, right);\n", out);
    xl6OutputPuts("    if ( cmp != 2 ) {\n", out);
    xl6OutputPuts("        return cmp;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return xl6_runtime_xvalue_compare_text(left, right);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static uint64 xl6_runtime_xvalue_hash(const void* pObj)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue value = pObj != 0 ? *(xvalue const*)pObj : 0;\n", out);
    xl6OutputPuts("    const xrt_type_desc* type = xvoTypeDesc(value);\n", out);
    xl6OutputPuts("    uint64 seed = type != 0 ? (uint64)type->TypeId : 0ULL;\n", out);
    xl6OutputPuts("    uint32 textSize = 0;\n", out);
    xl6OutputPuts("    str text = xvoToString(value, &textSize);\n", out);
    xl6OutputPuts("    uint64 result = xrtHash64(&seed, sizeof(seed));\n", out);
    xl6OutputPuts("    if ( text != 0 && textSize > 0 ) {\n", out);
    xl6OutputPuts("        result ^= xrtHash64(text, textSize);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( text != 0 ) { xrtFree(text); }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static str xl6_runtime_xvalue_to_string(const void* pObj, uint32* pSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue value = pObj != 0 ? *(xvalue const*)pObj : 0;\n", out);
    xl6OutputPuts("    return xvoToString(value, pSize);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xvalue xl6_runtime_xvalue_box(const void* pObj, const xrt_type_desc* pType)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue value = pObj != 0 ? *(xvalue const*)pObj : 0;\n", out);
    xl6OutputPuts("    (void)pType;\n", out);
    xl6OutputPuts("    if ( value != 0 ) { xvoAddRef(value); return value; }\n", out);
    xl6OutputPuts("    return xvoCreateNull();\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static bool xl6_runtime_xvalue_unbox(xvalue value, void* pOut, const xrt_type_desc* pType)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( pOut == 0 ) {\n", out);
    xl6OutputPuts("        return false;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    (void)pType;\n", out);
    xl6OutputPuts("    if ( value != 0 ) { xvoAddRef(value); }\n", out);
    xl6OutputPuts("    *(xvalue*)pOut = value != 0 ? value : xvoCreateNull();\n", out);
    xl6OutputPuts("    return true;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static const xrt_type_ops xl6_runtime_xvalue_ops = {\n", out);
    xl6OutputPuts("    xl6_runtime_xvalue_init,\n", out);
    xl6OutputPuts("    xl6_runtime_xvalue_copy,\n", out);
    xl6OutputPuts("    xl6_runtime_xvalue_move,\n", out);
    xl6OutputPuts("    xl6_runtime_xvalue_drop,\n", out);
    xl6OutputPuts("    xl6_runtime_xvalue_compare,\n", out);
    xl6OutputPuts("    xl6_runtime_xvalue_hash,\n", out);
    xl6OutputPuts("    xl6_runtime_xvalue_to_string,\n", out);
    xl6OutputPuts("    xl6_runtime_xvalue_box,\n", out);
    xl6OutputPuts("    xl6_runtime_xvalue_unbox\n", out);
    xl6OutputPuts("};\n", out);
    xl6OutputPuts("\nstatic void xl6_runtime_class_copy(void* pDst, const void* pSrc)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue value = pSrc != 0 ? *(xvalue const*)pSrc : 0;\n", out);
    xl6OutputPuts("    if ( value != 0 ) { xvoAddRef(value); }\n", out);
    xl6OutputPuts("    if ( pDst != 0 ) { *(xvalue*)pDst = value; }\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("#ifndef XL6_CLASS_TYPE_EXTRA_DEFINED\n", out);
    xl6OutputPuts("#define XL6_CLASS_TYPE_EXTRA_DEFINED\n", out);
    xl6OutputPuts("#define XL6_CLASS_TYPE_FLAG_VALUE_EQUALITY 0x00000001u\n", out);
    xl6OutputPuts("#define XL6_CLASS_TYPE_EXTRA_MAGIC 0x584C3643u\n", out);
    xl6OutputPuts("typedef struct xl6_class_type_extra { unsigned int magic; unsigned int flags; } xl6_class_type_extra;\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("static const xl6_class_type_extra* xl6_runtime_class_extra(const xrt_type_desc* type)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const xl6_class_type_extra* extra = type != 0 ? (const xl6_class_type_extra*)type->Extra : 0;\n", out);
    xl6OutputPuts("    return extra != 0 && extra->magic == XL6_CLASS_TYPE_EXTRA_MAGIC ? extra : 0;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static int xl6_runtime_class_compare(const void* pA, const void* pB)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue left = pA != 0 ? *(xvalue const*)pA : 0;\n", out);
    xl6OutputPuts("    xvalue right = pB != 0 ? *(xvalue const*)pB : 0;\n", out);
    xl6OutputPuts("    bool leftNull = left == 0 || xvoIsNull(left);\n", out);
    xl6OutputPuts("    bool rightNull = right == 0 || xvoIsNull(right);\n", out);
    xl6OutputPuts("    const xrt_type_desc* leftType;\n", out);
    xl6OutputPuts("    const xrt_type_desc* rightType;\n", out);
    xl6OutputPuts("    const xl6_class_type_extra* extra;\n", out);
    xl6OutputPuts("    if ( leftNull || rightNull ) { return leftNull == rightNull ? 0 : (leftNull ? -1 : 1); }\n", out);
    xl6OutputPuts("    leftType = xvoTypeDesc(left);\n", out);
    xl6OutputPuts("    rightType = xvoTypeDesc(right);\n", out);
    xl6OutputPuts("    extra = leftType != 0 && xrtTypeSame(leftType, rightType) ? xl6_runtime_class_extra(leftType) : 0;\n", out);
    xl6OutputPuts("    if ( extra != 0 && (extra->flags & XL6_CLASS_TYPE_FLAG_VALUE_EQUALITY) != 0 &&\n", out);
    xl6OutputPuts("         leftType->Ops != 0 && leftType->Ops->compare != 0 ) {\n", out);
    xl6OutputPuts("        return leftType->Ops->compare(xvoGetRecordData(left), xvoGetRecordData(right));\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return left == right ? 0 : ((uintptr_t)left < (uintptr_t)right ? -1 : 1);\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static uint64 xl6_runtime_class_hash(const void* pObj)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue value = pObj != 0 ? *(xvalue const*)pObj : 0;\n", out);
    xl6OutputPuts("    const xrt_type_desc* type;\n", out);
    xl6OutputPuts("    const xl6_class_type_extra* extra;\n", out);
    xl6OutputPuts("    if ( value == 0 || xvoIsNull(value) ) { return 0; }\n", out);
    xl6OutputPuts("    type = xvoTypeDesc(value);\n", out);
    xl6OutputPuts("    extra = xl6_runtime_class_extra(type);\n", out);
    xl6OutputPuts("    if ( extra != 0 && (extra->flags & XL6_CLASS_TYPE_FLAG_VALUE_EQUALITY) != 0 &&\n", out);
    xl6OutputPuts("         type->Ops != 0 && type->Ops->hash != 0 ) {\n", out);
    xl6OutputPuts("        return type->Ops->hash(xvoGetRecordData(value));\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return xrtHash64(&value, sizeof(value));\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static xvalue xl6_runtime_class_box(const void* pObj, const xrt_type_desc* pType)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue value = pObj != 0 ? *(xvalue const*)pObj : 0;\n", out);
    xl6OutputPuts("    (void)pType;\n", out);
    xl6OutputPuts("    if ( value != 0 ) { xvoAddRef(value); return value; }\n", out);
    xl6OutputPuts("    return xvoCreateNull();\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static bool xl6_runtime_class_unbox(xvalue value, void* pOut, const xrt_type_desc* pType)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const xrt_type_desc* actual;\n", out);
    xl6OutputPuts("    const xrt_type_desc* target;\n", out);
    xl6OutputPuts("    if ( pOut == 0 ) { return false; }\n", out);
    xl6OutputPuts("    if ( value == 0 || xvoIsNull(value) ) { *(xvalue*)pOut = xvoCreateNull(); return true; }\n", out);
    xl6OutputPuts("    actual = xvoTypeDesc(value);\n", out);
    xl6OutputPuts("    target = xrtTypeGenericArg(pType, 0u);\n", out);
    xl6OutputPuts("    if ( actual == 0 || target == 0 || !xrtTypeIsA(actual, target) ) { return false; }\n", out);
    xl6OutputPuts("    xvoAddRef(value);\n", out);
    xl6OutputPuts("    *(xvalue*)pOut = value;\n", out);
    xl6OutputPuts("    return true;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static const xrt_type_ops xl6_runtime_class_ops = {\n", out);
    xl6OutputPuts("    xl6_runtime_xvalue_init,\n", out);
    xl6OutputPuts("    xl6_runtime_class_copy,\n", out);
    xl6OutputPuts("    xl6_runtime_xvalue_move,\n", out);
    xl6OutputPuts("    xl6_runtime_xvalue_drop,\n", out);
    xl6OutputPuts("    xl6_runtime_class_compare,\n", out);
    xl6OutputPuts("    xl6_runtime_class_hash,\n", out);
    xl6OutputPuts("    xl6_runtime_xvalue_to_string,\n", out);
    xl6OutputPuts("    xl6_runtime_class_box,\n", out);
    xl6OutputPuts("    xl6_runtime_class_unbox\n", out);
    xl6OutputPuts("};\n", out);

    xl6OutputPuts("\nxtarray xrtTypedArrayClone(xtarray value);\n", out);
    xl6OutputPuts("xtlist xrtTypedListClone(xtlist value);\n", out);
    xl6OutputPuts("xtset xrtTypedSetClone(xtset value);\n", out);
    xl6OutputPuts("xtdict xrtTypedDictClone(xtdict value);\n", out);
    xl6OutputPuts("xtarray xrtTypedArrayMoveToShared(xtarray value);\n", out);
    xl6OutputPuts("xtlist xrtTypedListMoveToShared(xtlist value);\n", out);
    xl6OutputPuts("xtset xrtTypedSetMoveToShared(xtset value);\n", out);
    xl6OutputPuts("xtdict xrtTypedDictMoveToShared(xtdict value);\n", out);
    xl6OutputPuts("xtarray xrtTypedArrayRetain(xtarray value);\n", out);
    xl6OutputPuts("xtlist xrtTypedListRetain(xtlist value);\n", out);
    xl6OutputPuts("xtset xrtTypedSetRetain(xtset value);\n", out);
    xl6OutputPuts("xtdict xrtTypedDictRetain(xtdict value);\n", out);
    if ( hasSpecialCollections ) {
        xl6OutputPuts("xtstack xrtTypedStackRetain(xtstack value);\n", out);
        xl6OutputPuts("xtavltree xrtTypedAvlTreeRetain(xtavltree value);\n", out);
        xl6OutputPuts("xtqueue xrtTypedQueueRetain(xtqueue value);\n", out);
    }
    xl6OutputPuts("xvalue xrtTypedArrayToValue(xtarray value);\n", out);
    xl6OutputPuts("xvalue xrtTypedListToValue(xtlist value);\n", out);
    xl6OutputPuts("xvalue xrtTypedSetToValue(xtset value);\n", out);
    xl6OutputPuts("xvalue xrtTypedDictToValue(xtdict value);\n", out);
    xl6OutputPuts("xtarray xrtTypedArrayFromValue(xvalue value, const xrt_type_desc* itemType, unsigned int mode);\n", out);
    xl6OutputPuts("xtlist xrtTypedListFromValue(xvalue value, const xrt_type_desc* itemType, unsigned int mode);\n", out);
    xl6OutputPuts("xtset xrtTypedSetFromValue(xvalue value, const xrt_type_desc* itemType, unsigned int mode);\n", out);
    xl6OutputPuts("xtdict xrtTypedDictFromValue(xvalue value, const xrt_type_desc* itemType, unsigned int mode);\n", out);
    xl6OutputPuts("void xrtTypedArrayDestroy(xtarray value);\n", out);
    xl6OutputPuts("void xrtTypedListDestroy(xtlist value);\n", out);
    xl6OutputPuts("void xrtTypedSetDestroy(xtset value);\n", out);
    xl6OutputPuts("void xrtTypedDictDestroy(xtdict value);\n", out);
    if ( hasSpecialCollections ) {
        xl6OutputPuts("void xrtTypedStackRelease(xtstack value);\n", out);
        xl6OutputPuts("void xrtTypedAvlTreeRelease(xtavltree value);\n", out);
        xl6OutputPuts("void xrtTypedQueueRelease(xtqueue value);\n", out);
    }
    xl6OutputPuts("xvalue xvoCreateHandle(const xrt_type_desc* pType, void* pHandle, uint32 flags);\n", out);
    xl6OutputPuts("void* xvoGetHandleData(xvalue value);\n", out);
    xl6OutputPuts("static void xl6_runtime_handle_init(void* pObj) { if (pObj != 0) { *(void**)pObj = 0; } }\n", out);
    xl6OutputPuts("static void xl6_runtime_handle_move(void* pDst, void* pSrc) { if (pDst != 0) { *(void**)pDst = pSrc != 0 ? *(void**)pSrc : 0; } if (pSrc != 0) { *(void**)pSrc = 0; } }\n", out);
    xl6OutputPuts("static int xl6_runtime_handle_compare(const void* pA, const void* pB) { void* a = pA != 0 ? *(void* const*)pA : 0; void* b = pB != 0 ? *(void* const*)pB : 0; return a == b ? 0 : ((uintptr_t)a < (uintptr_t)b ? -1 : 1); }\n", out);
    xl6OutputPuts("static uint64 xl6_runtime_handle_hash(const void* pObj) { void* value = pObj != 0 ? *(void* const*)pObj : 0; return value != 0 ? xrtHash64(&value, sizeof(value)) : 0; }\n", out);
    xl6OutputPuts("static void xl6_runtime_array_copy(void* pDst, const void* pSrc) { if (pDst != 0) { xtarray value = pSrc != 0 ? *(xtarray const*)pSrc : 0; *(xtarray*)pDst = value != 0 ? xrtTypedArrayRetain(value) : 0; } }\n", out);
    xl6OutputPuts("static void xl6_runtime_list_copy(void* pDst, const void* pSrc) { if (pDst != 0) { xtlist value = pSrc != 0 ? *(xtlist const*)pSrc : 0; *(xtlist*)pDst = value != 0 ? xrtTypedListRetain(value) : 0; } }\n", out);
    xl6OutputPuts("static void xl6_runtime_set_copy(void* pDst, const void* pSrc) { if (pDst != 0) { xtset value = pSrc != 0 ? *(xtset const*)pSrc : 0; *(xtset*)pDst = value != 0 ? xrtTypedSetRetain(value) : 0; } }\n", out);
    xl6OutputPuts("static void xl6_runtime_dict_copy(void* pDst, const void* pSrc) { if (pDst != 0) { xtdict value = pSrc != 0 ? *(xtdict const*)pSrc : 0; *(xtdict*)pDst = value != 0 ? xrtTypedDictRetain(value) : 0; } }\n", out);
    xl6OutputPuts("static void xl6_runtime_array_drop(void* pObj) { if (pObj != 0 && *(xtarray*)pObj != 0) { xrtTypedArrayDestroy(*(xtarray*)pObj); *(xtarray*)pObj = 0; } }\n", out);
    xl6OutputPuts("static void xl6_runtime_list_drop(void* pObj) { if (pObj != 0 && *(xtlist*)pObj != 0) { xrtTypedListDestroy(*(xtlist*)pObj); *(xtlist*)pObj = 0; } }\n", out);
    xl6OutputPuts("static void xl6_runtime_set_drop(void* pObj) { if (pObj != 0 && *(xtset*)pObj != 0) { xrtTypedSetDestroy(*(xtset*)pObj); *(xtset*)pObj = 0; } }\n", out);
    xl6OutputPuts("static void xl6_runtime_dict_drop(void* pObj) { if (pObj != 0 && *(xtdict*)pObj != 0) { xrtTypedDictDestroy(*(xtdict*)pObj); *(xtdict*)pObj = 0; } }\n", out);
    xl6OutputPuts("static xvalue xl6_runtime_handle_box_owned(const void* pObj, const xrt_type_desc* pType, void* (*retain)(void*)) { void* value = pObj != 0 ? *(void* const*)pObj : 0; void* owned = value != 0 && retain != 0 ? retain(value) : 0; xvalue result = value != 0 ? xvoCreateHandle(pType, owned, 0x00000001u | 0x00000004u) : xvoCreateNull(); if (result == 0 && owned != 0 && pType != 0 && pType->Ops != 0 && pType->Ops->drop != 0) { pType->Ops->drop(&owned); } return result; }\n", out);
    if ( hasSpecialCollections ) {
        xl6OutputPuts("static void xl6_runtime_stack_copy(void* pDst, const void* pSrc) { if (pDst != 0) { xtstack value = pSrc != 0 ? *(xtstack const*)pSrc : 0; *(xtstack*)pDst = value != 0 ? xrtTypedStackRetain(value) : 0; } }\n", out);
        xl6OutputPuts("static void xl6_runtime_avl_copy(void* pDst, const void* pSrc) { if (pDst != 0) { xtavltree value = pSrc != 0 ? *(xtavltree const*)pSrc : 0; *(xtavltree*)pDst = value != 0 ? xrtTypedAvlTreeRetain(value) : 0; } }\n", out);
        xl6OutputPuts("static void xl6_runtime_queue_copy(void* pDst, const void* pSrc) { if (pDst != 0) { xtqueue value = pSrc != 0 ? *(xtqueue const*)pSrc : 0; *(xtqueue*)pDst = value != 0 ? xrtTypedQueueRetain(value) : 0; } }\n", out);
        xl6OutputPuts("static void xl6_runtime_stack_drop(void* pObj) { if (pObj != 0 && *(xtstack*)pObj != 0) { xrtTypedStackRelease(*(xtstack*)pObj); *(xtstack*)pObj = 0; } }\n", out);
        xl6OutputPuts("static void xl6_runtime_avl_drop(void* pObj) { if (pObj != 0 && *(xtavltree*)pObj != 0) { xrtTypedAvlTreeRelease(*(xtavltree*)pObj); *(xtavltree*)pObj = 0; } }\n", out);
        xl6OutputPuts("static void xl6_runtime_queue_drop(void* pObj) { if (pObj != 0 && *(xtqueue*)pObj != 0) { xrtTypedQueueRelease(*(xtqueue*)pObj); *(xtqueue*)pObj = 0; } }\n", out);
        xl6OutputPuts("static xvalue xl6_runtime_stack_box(const void* pObj, const xrt_type_desc* pType) { return xl6_runtime_handle_box_owned(pObj, pType, (void* (*)(void*))xrtTypedStackRetain); }\n", out);
        xl6OutputPuts("static xvalue xl6_runtime_avl_box(const void* pObj, const xrt_type_desc* pType) { return xl6_runtime_handle_box_owned(pObj, pType, (void* (*)(void*))xrtTypedAvlTreeRetain); }\n", out);
        xl6OutputPuts("static xvalue xl6_runtime_queue_box(const void* pObj, const xrt_type_desc* pType) { return xl6_runtime_handle_box_owned(pObj, pType, (void* (*)(void*))xrtTypedQueueRetain); }\n", out);
        xl6OutputPuts("static bool xl6_runtime_special_unbox(xvalue value, void* pOut, const xrt_type_desc* pType) { const xrt_type_desc* actual; void* handle; if (pOut == 0) { return false; } *(void**)pOut = 0; if (value == 0 || xvoIsNull(value)) { return true; } actual = xvoTypeDesc(value); if (pType == 0 || actual == 0 || !xrtTypeSame(actual, pType) || pType->Ops == 0 || pType->Ops->copy == 0) { return false; } handle = xvoGetHandleData(value); if (handle == 0) { return true; } pType->Ops->copy(pOut, &handle); return *(void**)pOut != 0; }\n", out);
    }
    xl6OutputPuts("static const xrt_type_desc* xl6_runtime_container_item_type(const xrt_type_desc* pType) { const xrt_generic_type* generic = pType != 0 ? (const xrt_generic_type*)pType->Extra : 0; return generic != 0 && generic->ArgCount == 1u ? generic->Args[0] : 0; }\n", out);
    xl6OutputPuts("static xvalue xl6_runtime_array_box(const void* pObj, const xrt_type_desc* pType) { return xl6_runtime_handle_box_owned(pObj, pType, (void* (*)(void*))xrtTypedArrayRetain); }\n", out);
    xl6OutputPuts("static xvalue xl6_runtime_list_box(const void* pObj, const xrt_type_desc* pType) { return xl6_runtime_handle_box_owned(pObj, pType, (void* (*)(void*))xrtTypedListRetain); }\n", out);
    xl6OutputPuts("static xvalue xl6_runtime_set_box(const void* pObj, const xrt_type_desc* pType) { return xl6_runtime_handle_box_owned(pObj, pType, (void* (*)(void*))xrtTypedSetRetain); }\n", out);
    xl6OutputPuts("static xvalue xl6_runtime_dict_box(const void* pObj, const xrt_type_desc* pType) { return xl6_runtime_handle_box_owned(pObj, pType, (void* (*)(void*))xrtTypedDictRetain); }\n", out);
    xl6OutputPuts("static bool xl6_runtime_container_unbox_handle(xvalue value, void* pOut, const xrt_type_desc* pType) { const xrt_type_desc* actual; void* handle; if (value == 0 || xvoIsNull(value)) { return false; } actual = xvoTypeDesc(value); if (actual == 0 || pType == 0 || !xrtTypeSame(actual, pType) || pType->Ops == 0 || pType->Ops->copy == 0) { return false; } handle = xvoGetHandleData(value); if (handle == 0) { return false; } pType->Ops->copy(pOut, &handle); return *(void**)pOut != 0; }\n", out);
    xl6OutputPuts("static bool xl6_runtime_array_unbox(xvalue value, void* pOut, const xrt_type_desc* pType) { const xrt_type_desc* itemType = xl6_runtime_container_item_type(pType); xtarray result; if (pOut == 0) { return false; } *(xtarray*)pOut = 0; if (value == 0 || xvoIsNull(value)) { return true; } if (xl6_runtime_container_unbox_handle(value, pOut, pType)) { return true; } result = itemType != 0 ? xrtTypedArrayFromValue(value, itemType, XRT_OBJMODE_SHARED) : 0; if (result == 0) { return false; } *(xtarray*)pOut = result; return true; }\n", out);
    xl6OutputPuts("static bool xl6_runtime_list_unbox(xvalue value, void* pOut, const xrt_type_desc* pType) { const xrt_type_desc* itemType = xl6_runtime_container_item_type(pType); xtlist result; if (pOut == 0) { return false; } *(xtlist*)pOut = 0; if (value == 0 || xvoIsNull(value)) { return true; } if (xl6_runtime_container_unbox_handle(value, pOut, pType)) { return true; } result = itemType != 0 ? xrtTypedListFromValue(value, itemType, XRT_OBJMODE_SHARED) : 0; if (result == 0) { return false; } *(xtlist*)pOut = result; return true; }\n", out);
    xl6OutputPuts("static bool xl6_runtime_set_unbox(xvalue value, void* pOut, const xrt_type_desc* pType) { const xrt_type_desc* itemType = xl6_runtime_container_item_type(pType); xtset result; if (pOut == 0) { return false; } *(xtset*)pOut = 0; if (value == 0 || xvoIsNull(value)) { return true; } if (xl6_runtime_container_unbox_handle(value, pOut, pType)) { return true; } result = itemType != 0 ? xrtTypedSetFromValue(value, itemType, XRT_OBJMODE_SHARED) : 0; if (result == 0) { return false; } *(xtset*)pOut = result; return true; }\n", out);
    xl6OutputPuts("static bool xl6_runtime_dict_unbox(xvalue value, void* pOut, const xrt_type_desc* pType) { const xrt_type_desc* itemType = xl6_runtime_container_item_type(pType); xtdict result; if (pOut == 0) { return false; } *(xtdict*)pOut = 0; if (value == 0 || xvoIsNull(value)) { return true; } if (xl6_runtime_container_unbox_handle(value, pOut, pType)) { return true; } result = itemType != 0 ? xrtTypedDictFromValue(value, itemType, XRT_OBJMODE_SHARED) : 0; if (result == 0) { return false; } *(xtdict*)pOut = result; return true; }\n", out);
    xl6OutputPuts("static const xrt_type_ops xl6_runtime_array_ops = { xl6_runtime_handle_init, xl6_runtime_array_copy, xl6_runtime_handle_move, xl6_runtime_array_drop, xl6_runtime_handle_compare, xl6_runtime_handle_hash, 0, xl6_runtime_array_box, xl6_runtime_array_unbox };\n", out);
    xl6OutputPuts("static const xrt_type_ops xl6_runtime_list_ops = { xl6_runtime_handle_init, xl6_runtime_list_copy, xl6_runtime_handle_move, xl6_runtime_list_drop, xl6_runtime_handle_compare, xl6_runtime_handle_hash, 0, xl6_runtime_list_box, xl6_runtime_list_unbox };\n", out);
    xl6OutputPuts("static const xrt_type_ops xl6_runtime_set_ops = { xl6_runtime_handle_init, xl6_runtime_set_copy, xl6_runtime_handle_move, xl6_runtime_set_drop, xl6_runtime_handle_compare, xl6_runtime_handle_hash, 0, xl6_runtime_set_box, xl6_runtime_set_unbox };\n", out);
    xl6OutputPuts("static const xrt_type_ops xl6_runtime_dict_ops = { xl6_runtime_handle_init, xl6_runtime_dict_copy, xl6_runtime_handle_move, xl6_runtime_dict_drop, xl6_runtime_handle_compare, xl6_runtime_handle_hash, 0, xl6_runtime_dict_box, xl6_runtime_dict_unbox };\n", out);
    if ( hasSpecialCollections ) {
        xl6OutputPuts("static const xrt_type_ops xl6_runtime_stack_ops = { xl6_runtime_handle_init, xl6_runtime_stack_copy, xl6_runtime_handle_move, xl6_runtime_stack_drop, xl6_runtime_handle_compare, xl6_runtime_handle_hash, 0, xl6_runtime_stack_box, xl6_runtime_special_unbox };\n", out);
        xl6OutputPuts("static const xrt_type_ops xl6_runtime_avl_ops = { xl6_runtime_handle_init, xl6_runtime_avl_copy, xl6_runtime_handle_move, xl6_runtime_avl_drop, xl6_runtime_handle_compare, xl6_runtime_handle_hash, 0, xl6_runtime_avl_box, xl6_runtime_special_unbox };\n", out);
        xl6OutputPuts("static const xrt_type_ops xl6_runtime_queue_ops = { xl6_runtime_handle_init, xl6_runtime_queue_copy, xl6_runtime_handle_move, xl6_runtime_queue_drop, xl6_runtime_handle_compare, xl6_runtime_handle_hash, 0, xl6_runtime_queue_box, xl6_runtime_special_unbox };\n", out);
    }
    xl6OutputPuts("#endif\n\n", out);
}

static void xl6CgenEmitRuntimeTypeDesc(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    char typeText[512];
    unsigned long long typeId;
    const XL6_AstNode* args;
    size_t i;

    typeText[0] = '\0';
    xl6CgenTypeText(ctx, typeNode, typeText, sizeof(typeText));
    typeId = xl6CgenTypeIdForTypeNode(ctx, typeNode);
    args = xl6CgenTypeNodeGenericArgs(typeNode);

    if ( args != NULL && args->childCount > 0 ) {
        xl6OutputPuts("static const xrt_type_desc* ", ctx->out);
        xl6CgenRuntimeTypeDescSymbol(ctx, ctx->out, typeNode);
        xl6OutputPrintf(ctx->out, "_args[%uu];\n", (unsigned)args->childCount);
        xl6OutputPuts("static const xrt_generic_type ", ctx->out);
        xl6CgenRuntimeTypeDescSymbol(ctx, ctx->out, typeNode);
        xl6OutputPrintf(ctx->out, "_generic = { %uu, ", (unsigned)args->childCount);
        xl6CgenRuntimeTypeDescSymbol(ctx, ctx->out, typeNode);
        xl6OutputPuts("_args };\n", ctx->out);
    }
    xl6OutputPuts("static const xrt_type_desc ", ctx->out);
    xl6CgenRuntimeTypeDescSymbol(ctx, ctx->out, typeNode);
    xl6OutputPuts("_value = {\n", ctx->out);
    xl6OutputPrintf(ctx->out, "    0x%08XU,\n", (unsigned int)(typeId & 0xFFFFFFFFULL));
    xl6OutputPrintf(ctx->out, "    %s,\n", xl6CgenRuntimeTypeDescKind(typeNode));
    xl6OutputPuts("    ", ctx->out);
    xl6CgenCStringLiteral(ctx->out, typeText);
    xl6OutputPuts(",\n", ctx->out);
    xl6OutputPrintf(ctx->out, "    %uu,\n", (unsigned)strlen(typeText));
    xl6OutputPuts("    \"xvalue\",\n", ctx->out);
    xl6OutputPuts("    6u,\n", ctx->out);
    xl6OutputPuts("    sizeof(void*),\n", ctx->out);
    xl6OutputPuts("    sizeof(void*),\n", ctx->out);
    if ( xl6CTextEquals(typeNode->text, "__class_handle") ) {
        xl6OutputPuts("    &xl6_runtime_class_ops,\n", ctx->out);
    } else if ( xl6CTextEquals(typeNode->text, "array") ) {
        xl6OutputPuts("    &xl6_runtime_array_ops,\n", ctx->out);
    } else if ( xl6CTextEquals(typeNode->text, "list") ) {
        xl6OutputPuts("    &xl6_runtime_list_ops,\n", ctx->out);
    } else if ( xl6CTextEquals(typeNode->text, "set") ) {
        xl6OutputPuts("    &xl6_runtime_set_ops,\n", ctx->out);
    } else if ( xl6CTextEquals(typeNode->text, "dict") ) {
        xl6OutputPuts("    &xl6_runtime_dict_ops,\n", ctx->out);
    } else if ( xl6CTextEquals(typeNode->text, "Stack") ) {
        xl6OutputPuts("    &xl6_runtime_stack_ops,\n", ctx->out);
    } else if ( xl6CTextEquals(typeNode->text, "AvlTree") ) {
        xl6OutputPuts("    &xl6_runtime_avl_ops,\n", ctx->out);
    } else if ( xl6CgenIsSpecialQueueTypeName(typeNode->text) ) {
        xl6OutputPuts("    &xl6_runtime_queue_ops,\n", ctx->out);
    } else {
        xl6OutputPuts("    &xl6_runtime_xvalue_ops,\n", ctx->out);
    }
    xl6OutputPuts("    0,\n", ctx->out);
    if ( args != NULL && args->childCount > 0 ) {
        xl6OutputPuts("    &", ctx->out);
        xl6CgenRuntimeTypeDescSymbol(ctx, ctx->out, typeNode);
        xl6OutputPuts("_generic\n", ctx->out);
    } else {
        xl6OutputPuts("    0\n", ctx->out);
    }
    xl6OutputPuts("};\n", ctx->out);

    if ( args != NULL && args->childCount > 0 ) {
        xl6OutputPuts("static xrt_once ", ctx->out);
        xl6CgenRuntimeTypeDescSymbol(ctx, ctx->out, typeNode);
        xl6OutputPuts("_once = XRT_ONCE_INIT;\n", ctx->out);
        xl6OutputPuts("static void ", ctx->out);
        xl6CgenRuntimeTypeDescSymbol(ctx, ctx->out, typeNode);
        xl6OutputPuts("_init(ptr pContext)\n{\n    (void)pContext;\n", ctx->out);
        for ( i = 0; i < args->childCount; ++i ) {
            xl6OutputPuts("    ", ctx->out);
            xl6CgenRuntimeTypeDescSymbol(ctx, ctx->out, typeNode);
            xl6OutputPrintf(ctx->out, "_args[%uu] = ", (unsigned)i);
            xl6CgenEmitXrtTypeDescRef(ctx, args->children[i]);
            xl6OutputPuts(";\n", ctx->out);
        }
        xl6OutputPuts("}\n", ctx->out);
    }

    xl6OutputPuts("static const xrt_type_desc* ", ctx->out);
    xl6CgenRuntimeTypeDescSymbol(ctx, ctx->out, typeNode);
    xl6OutputPuts("(void)\n{\n", ctx->out);
    if ( args != NULL && args->childCount > 0 ) {
        xl6OutputPuts("    xrtOnceRunGraph(&", ctx->out);
        xl6CgenRuntimeTypeDescSymbol(ctx, ctx->out, typeNode);
        xl6OutputPuts("_once, ", ctx->out);
        xl6CgenRuntimeTypeDescSymbol(ctx, ctx->out, typeNode);
        xl6OutputPuts("_init, 0);\n", ctx->out);
    }
    xl6OutputPuts("    return &", ctx->out);
    xl6CgenRuntimeTypeDescSymbol(ctx, ctx->out, typeNode);
    xl6OutputPuts("_value;\n}\n\n", ctx->out);
}

static void xl6CgenEmitRuntimeTypeDescs(XL6_CgenContext* ctx)
{
    size_t i;

    if ( ctx == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->runtimeTypeDescCount; ++i ) {
        xl6CgenEmitRuntimeTypeDescDependencyProto(ctx, ctx->runtimeTypeDescs[i].typeNode);
    }
    for ( i = 0; i < ctx->runtimeTypeDescCount; ++i ) {
        xl6CgenEmitRuntimeTypeDescProto(ctx, ctx->runtimeTypeDescs[i].typeNode);
    }
    if ( ctx->runtimeTypeDescCount > 0 ) {
        xl6OutputPutc('\n', ctx->out);
        xl6CgenEmitRuntimeXValueTypeOps(ctx);
    }
    for ( i = 0; i < ctx->runtimeTypeDescCount; ++i ) {
        xl6CgenEmitRuntimeTypeDesc(ctx, ctx->runtimeTypeDescs[i].typeNode);
    }
}

static int xl6CgenTypeValueUsesBaseType(const char* name)
{
    (void)name;
    return 0;
}

static XL6_AstNode* xl6CgenCloneExprTypeForTypeValue(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* rawType;
    const XL6_AstNode* concreteType;
    const XL6_AstNode* declaredType;
    XL6_AstNode* baseType;

    /*
        type(varName) 查询的是变量此刻保存的运行时值，不能使用控制流收窄结果做静态折叠。
        收窄仍用于成员调用和重载解析；这里只恢复 type 的动态观察语义。
    */
    if ( ctx != NULL && expr != NULL && expr->kind == XL6_AST_NAME ) {
        declaredType = xl6CgenFindVarType(ctx, expr->text);
        declaredType = xl6CgenResolveAliasType(ctx, declaredType);
        if ( declaredType != NULL && xl6CTextEquals(declaredType->text, "var") ) {
            return xl6CgenNewSimpleType("var", expr);
        }
    }

    rawType = xl6CgenCloneExprTypeForInference(ctx, expr);
    if ( rawType == NULL ) {
        return NULL;
    }

    concreteType = xl6CgenSubstituteGenericType(ctx, rawType);
    concreteType = xl6CgenResolveAliasType(ctx, concreteType);
    if ( concreteType != NULL &&
         concreteType->childCount > 0 &&
         xl6CgenTypeValueUsesBaseType(concreteType->text) ) {
        baseType = xl6CgenNewSimpleType(concreteType->text, concreteType);
        xl6AstFree(rawType);
        return baseType;
    }
    return rawType;
}

static void xl6CgenEmitTypeOfExpr(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* typeNode;
    XL6_AstNode* storageType;
    const XL6_AstNode* concreteType;

    typeNode = xl6CgenCloneExprTypeForTypeValue(ctx, expr);
    if ( typeNode == NULL ) {
        xl6CgenError(ctx, expr, "cannot infer type for type(expr)");
        xl6OutputPuts("0", ctx->out);
        return;
    }
    concreteType = xl6CgenSubstituteGenericType(ctx, typeNode);
    concreteType = xl6CgenResolveAliasType(ctx, concreteType);
    if ( concreteType != NULL && xl6CTextEquals(concreteType->text, "var") ) {
        storageType = xl6CgenCloneExprStorageType(ctx, expr);
        xl6OutputPuts("xl6_xrt_type_value_id(", ctx->out);
        if ( expr != NULL && expr->kind == XL6_AST_NAME &&
             xl6CgenIsXValueType(storageType) ) {
            /* 读取原始 xvalue，不能让控制流收窄路径先把它解箱。 */
            xl6CgenNamedValueReference(ctx, expr->text);
            xl6OutputPuts(", false)", ctx->out);
        } else {
            xl6CgenExpr(ctx, expr);
            xl6OutputPuts(
                xl6CgenExprIsXValueBorrow(ctx, expr) ? ", false)" : ", true)",
                ctx->out
            );
        }
        xl6AstFree(storageType);
        xl6AstFree(typeNode);
        return;
    }
    xl6CgenEmitTypeIdForTypeNode(ctx, typeNode);
    xl6AstFree(typeNode);
}
