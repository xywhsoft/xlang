/* 只从 typed HIR 查询源码表达式类型，语句节点和后端合成节点不参与。 */
static int xl6CgenNodeHasErrorType(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node)
{
    XL6_AstNode* typeNode;
    int result;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_TYPE && xl6CTextEquals(node->text, "error") ) {
        return 1;
    }
    if ( xl6CgenHirNode(ctx, node) == NULL ||
         !xl6CgenExprHasStaticValueType(ctx, node) ) {
        return 0;
    }

    typeNode = xl6CgenCloneExprTypeForInference(ctx, node);
    result = typeNode != NULL && xl6CTextEquals(typeNode->text, "error");
    xl6AstFree(typeNode);
    return result;
}

static int xl6CgenNodeNeedsErrorRuntime(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNodeHasErrorType(ctx, node) ) {
        return 1;
    }
    if ( node->kind == XL6_AST_CALL &&
         node->childCount > 0 &&
         node->children[0] != NULL &&
         node->children[0]->kind == XL6_AST_NAME &&
         xl6CTextEquals(node->children[0]->text, "__xl_std_xrt_file_read_text_strict") ) {
        return 1;
    }
    if ( node->kind == XL6_AST_INDEX ||
         node->kind == XL6_AST_SAFE_INDEX ) {
        /*
         * 索引读写需要运行时边界诊断：
         * 赋值失败、错误 key 类型等场景应进入异常系统，而不是静默 fatal。
         */
        return 1;
    }
    if ( node->kind == XL6_AST_BINARY &&
         (xl6CTextEquals(node->text, "/") ||
          xl6CTextEquals(node->text, "%") ||
          xl6CTextEquals(node->text, "<<") ||
          xl6CTextEquals(node->text, ">>")) ) {
        return 1;
    }
    if ( node->kind == XL6_AST_FOR &&
         node->childCount > 1 &&
         xl6CgenIsRangeCall(node->children[1]) &&
         node->children[1] != NULL &&
         node->children[1]->childCount > 1 &&
         node->children[1]->children[1] != NULL &&
         node->children[1]->children[1]->childCount >= 3 ) {
        return 1;
    }
    if ( node->kind == XL6_AST_TRY ||
         node->kind == XL6_AST_THROW ||
         node->kind == XL6_AST_ASSERT ||
         node->kind == XL6_AST_AWAIT ||
         node->kind == XL6_AST_ASYNC_FUNC ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsErrorRuntime(ctx, node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6CgenEmitErrorRuntime(XL6_Output* out, int hasXValueRuntime)
{
	xl6OutputPuts("#include <stddef.h>\n", out);
	xl6OutputPuts("#include <stdio.h>\n", out);
	xl6OutputPuts("#include <string.h>\n", out);
	xl6OutputPuts("#ifndef XL6_ERROR_TYPEDEF\n", out);
    xl6OutputPuts("#define XL6_ERROR_TYPEDEF\n", out);
    xl6OutputPuts("typedef struct XL6_Error XL6_Error;\n", out);
    xl6OutputPuts("struct XL6_Error {\n", out);
    xl6OutputPuts("    volatile int32 refCount;\n", out);
    xl6OutputPuts("    char* kind;\n", out);
    xl6OutputPuts("    char* message;\n", out);
    xl6OutputPuts("    char* file;\n", out);
    xl6OutputPuts("    int line;\n", out);
    xl6OutputPuts("    int column;\n", out);
    xl6OutputPuts("    xerrkind xrtKind;\n", out);
    xl6OutputPuts("    int code;\n", out);
    xl6OutputPuts("    int systemCode;\n", out);
    xl6OutputPuts("    char* domain;\n", out);
    xl6OutputPuts("    char* operation;\n", out);
    xl6OutputPuts("    char* data;\n", out);
    xl6OutputPuts("    XL6_Error* cause;\n", out);
    xl6OutputPuts("};\n", out);
    xl6OutputPuts("#endif\n\n", out);
    xl6OutputPuts("static void xl6_error_free(XL6_Error* error);\n", out);
    xl6OutputPuts("static xonce xl6__active_error_once = XRT_ONCE_INIT;\n", out);
    xl6OutputPuts("static xthreadkey* xl6__active_error_key = 0;\n\n", out);
    xl6OutputPuts("static void xl6_error_key_destroy(void* value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xl6_error_free((XL6_Error*)value);\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static bool xl6_error_key_init(void* unused)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    (void)unused;\n", out);
    xl6OutputPuts("    xl6__active_error_key = xrtThreadKeyCreate(xl6_error_key_destroy);\n", out);
    xl6OutputPuts("    return xl6__active_error_key != 0;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static int xl6_error_tls_ensure(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtOnce(&xl6__active_error_once, xl6_error_key_init, 0);\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static XL6_Error* xl6_error_active_get(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_error_tls_ensure() ? (XL6_Error*)xrtThreadKeyGet(xl6__active_error_key) : 0;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static XL6_Error* xl6_error_active_take(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_error_tls_ensure() ? (XL6_Error*)xrtThreadKeyTake(xl6__active_error_key) : 0;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static void xl6_error_active_set(XL6_Error* error)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( !xl6_error_tls_ensure() || !xrtThreadKeySet(xl6__active_error_key, error) ) {\n", out);
    xl6OutputPuts("        xl6_error_free(error);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static void xl6_error_tls_unit(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( xl6__active_error_key != 0 ) {\n", out);
    xl6OutputPuts("        (void)xrtThreadKeyDestroy(xl6__active_error_key);\n", out);
    xl6OutputPuts("        xl6__active_error_key = 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    memset(&xl6__active_error_once, 0, sizeof(xl6__active_error_once));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static char* xl6_error_dup(const char* text)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t len = text != 0 ? strlen(text) : 0;\n", out);
    xl6OutputPuts("    char* copy = (char*)xrtMalloc(len + 1);\n", out);
    xl6OutputPuts("    if ( copy == 0 ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( len > 0 ) {\n", out);
    xl6OutputPuts("        memcpy(copy, text, len);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    copy[len] = 0;\n", out);
    xl6OutputPuts("    return copy;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static XL6_Error* xl6_error_retain(XL6_Error* error)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return error != 0 && xrtRefRetain(&error->refCount) > 0 ? error : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void xl6_error_free(XL6_Error* error)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( error == 0 ) {\n", out);
    xl6OutputPuts("        return;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( xrtRefRelease(&error->refCount) != 0 ) {\n", out);
    xl6OutputPuts("        return;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    xl6_error_free(error->cause);\n", out);
    xl6OutputPuts("    xrtFree(error->kind);\n", out);
    xl6OutputPuts("    xrtFree(error->message);\n", out);
    xl6OutputPuts("    xrtFree(error->file);\n", out);
    xl6OutputPuts("    xrtFree(error->domain);\n", out);
    xl6OutputPuts("    xrtFree(error->operation);\n", out);
    xl6OutputPuts("    xrtFree(error->data);\n", out);
    xl6OutputPuts("    xrtFree(error);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static XL6_Error* xl6_error_new(const char* kind, const char* message, const char* file, int line, int column, XL6_Error* cause)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Error* error = (XL6_Error*)xrtMalloc(sizeof(XL6_Error));\n", out);
    xl6OutputPuts("    if ( error == 0 ) {\n", out);
    xl6OutputPuts("        xl6_error_free(cause);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    error->refCount = 1;\n", out);
    xl6OutputPuts("    error->kind = xl6_error_dup(kind != 0 ? kind : \"Error\");\n", out);
    xl6OutputPuts("    error->message = xl6_error_dup(message != 0 ? message : \"\");\n", out);
    xl6OutputPuts("    error->file = xl6_error_dup(file != 0 ? file : \"\");\n", out);
    xl6OutputPuts("    error->line = line;\n", out);
    xl6OutputPuts("    error->column = column;\n", out);
    xl6OutputPuts("    error->xrtKind = XERR_STATE;\n", out);
    xl6OutputPuts("    error->code = 1;\n", out);
    xl6OutputPuts("    error->systemCode = 0;\n", out);
    xl6OutputPuts("    error->domain = 0;\n", out);
    xl6OutputPuts("    error->operation = 0;\n", out);
    xl6OutputPuts("    error->data = 0;\n", out);
    xl6OutputPuts("    error->cause = cause;\n", out);
    xl6OutputPuts("    if ( error->kind == 0 || error->message == 0 || error->file == 0 ) {\n", out);
    xl6OutputPuts("        xl6_error_free(error);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return error;\n", out);
    xl6OutputPuts("}\n\n", out);

    /* error 是一等托管句柄；静态 ABI 与动态 var ABI 共用同一 TypeDesc。 */
    xl6OutputPuts("static void xl6_error_type_init(void* pObj)\n", out);
    xl6OutputPuts("{\n    if ( pObj != 0 ) { *(XL6_Error**)pObj = 0; }\n}\n\n", out);
    xl6OutputPuts("static void xl6_error_type_copy(void* pDst, const void* pSrc)\n", out);
    xl6OutputPuts("{\n    XL6_Error* error = pSrc != 0 ? *(XL6_Error* const*)pSrc : 0;\n    if ( pDst != 0 ) { *(XL6_Error**)pDst = xl6_error_retain(error); }\n}\n\n", out);
    xl6OutputPuts("static void xl6_error_type_move(void* pDst, void* pSrc)\n", out);
    xl6OutputPuts("{\n    if ( pDst != 0 ) { *(XL6_Error**)pDst = pSrc != 0 ? *(XL6_Error**)pSrc : 0; }\n    if ( pSrc != 0 ) { *(XL6_Error**)pSrc = 0; }\n}\n\n", out);
    xl6OutputPuts("static void xl6_error_type_drop(void* pObj)\n", out);
    xl6OutputPuts("{\n    if ( pObj != 0 ) { xl6_error_free(*(XL6_Error**)pObj); *(XL6_Error**)pObj = 0; }\n}\n\n", out);
    xl6OutputPuts("static int xl6_error_type_compare(const void* pA, const void* pB)\n", out);
    xl6OutputPuts("{\n    XL6_Error* left = pA != 0 ? *(XL6_Error* const*)pA : 0;\n    XL6_Error* right = pB != 0 ? *(XL6_Error* const*)pB : 0;\n    return left == right ? 0 : ((uintptr_t)left < (uintptr_t)right ? -1 : 1);\n}\n\n", out);
    xl6OutputPuts("static uint64 xl6_error_type_hash(const void* pObj)\n", out);
    xl6OutputPuts("{\n    return (uint64)(uintptr_t)(pObj != 0 ? *(XL6_Error* const*)pObj : 0);\n}\n\n", out);
    if ( hasXValueRuntime ) {
        xl6OutputPuts("static const xl6_type_desc* xl6TypeError(void);\n\n", out);
        xl6OutputPuts("static xvalue* xl6_error_box_take(XL6_Error* error)\n", out);
        xl6OutputPuts("{\n    xvalue* value;\n    if ( error == 0 ) { return xrtValueNull(); }\n    value = xl6ValueHandle(xl6TypeError(), error, XL6_XRT_HANDLE_FLAG_OWNED | XL6_XRT_HANDLE_FLAG_NULLABLE);\n    if ( value == 0 ) { xl6_error_free(error); }\n    return value;\n}\n\n", out);
        xl6OutputPuts("static xvalue* xl6_error_box_ref(XL6_Error* error)\n", out);
        xl6OutputPuts("{\n    return xl6_error_box_take(xl6_error_retain(error));\n}\n\n", out);
        xl6OutputPuts("static xvalue* xl6_error_type_box(const void* pObj, const xl6_type_desc* pType)\n", out);
        xl6OutputPuts("{\n    (void)pType;\n    return xl6_error_box_ref(pObj != 0 ? *(XL6_Error* const*)pObj : 0);\n}\n\n", out);
        xl6OutputPuts("static bool xl6_error_type_unbox(xvalue* value, void* pOut, const xl6_type_desc* pType)\n", out);
        xl6OutputPuts("{\n    XL6_Error* error;\n    if ( pOut == 0 ) { return false; }\n    if ( value == 0 || xl6_xrt_value_is_null(value) ) { *(XL6_Error**)pOut = 0; return true; }\n    if ( !xl6TypeSame(xl6ValueType(value), pType) ) { return false; }\n    error = (XL6_Error*)xl6ValueHandleData(value);\n    *(XL6_Error**)pOut = xl6_error_retain(error);\n    return error == 0 || *(XL6_Error**)pOut != 0;\n}\n\n", out);
    }
    xl6OutputPuts("static const xl6_type_ops xl6_error_type_ops = {\n", out);
    xl6OutputPuts("    xl6_error_type_init, xl6_error_type_copy, xl6_error_type_move, xl6_error_type_drop,\n", out);
    xl6OutputPuts("    xl6_error_type_compare, xl6_error_type_hash, 0, ", out);
    xl6OutputPuts(hasXValueRuntime ? "xl6_error_type_box, xl6_error_type_unbox, " : "0, 0, ", out);
    xl6OutputPuts("xl6_error_type_copy\n};\n\n", out);
    xl6OutputPuts("static const xl6_type_desc* xl6TypeError(void)\n", out);
    xl6OutputPuts("{\n    static const xl6_type_desc type = {\n        0x00010031u, XL6_XRT_TYPE_KIND_HANDLE, \"error\", 5u, \"XL6_Error*\", 10u,\n        sizeof(XL6_Error*), sizeof(void*), &xl6_error_type_ops, 0, 0, 0\n    };\n    return &type;\n}\n\n", out);

    xl6OutputPuts("static int xl6_error_has(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_error_active_get() != 0;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static XL6_Error* xl6_error_take(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Error* error = xl6_error_active_take();\n", out);
    xl6OutputPuts("    return error;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static void xl6_error_clear(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Error* error = xl6_error_take();\n", out);
    xl6OutputPuts("    xl6_error_free(error);\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static void xl6_error_set_active(XL6_Error* error)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xl6_error_clear();\n", out);
    xl6OutputPuts("    xl6_error_active_set(error);\n", out);
    xl6OutputPuts("}\n\n", out);

    /* 把语言异常映射到 XRT 通用错误类别，供 Native 回调保留原始失败原因。 */
	xl6OutputPuts("static xerrkind xl6_error_xrt_kind(const char* kind)\n", out);
	xl6OutputPuts("{\n", out);
	xl6OutputPuts("    if ( kind == 0 ) { return XERR_STATE; }\n", out);
	xl6OutputPuts("    if ( strcmp(kind, \"TypeError\") == 0 ) { return XERR_TYPE; }\n", out);
	xl6OutputPuts("    if ( strcmp(kind, \"RangeError\") == 0 || strcmp(kind, \"IndexError\") == 0 || strcmp(kind, \"OverflowError\") == 0 ) { return XERR_RANGE; }\n", out);
	xl6OutputPuts("    if ( strcmp(kind, \"MemoryError\") == 0 ) { return XERR_MEMORY; }\n", out);
	xl6OutputPuts("    if ( strcmp(kind, \"FileError\") == 0 || strcmp(kind, \"IOError\") == 0 || strcmp(kind, \"NetworkError\") == 0 || strcmp(kind, \"EncodingError\") == 0 || strcmp(kind, \"CompressionError\") == 0 || strcmp(kind, \"ArchiveError\") == 0 ) { return XERR_IO; }\n", out);
	xl6OutputPuts("    if ( strcmp(kind, \"TimeoutError\") == 0 ) { return XERR_TIMEOUT; }\n", out);
	xl6OutputPuts("    if ( strcmp(kind, \"CancelledError\") == 0 ) { return XERR_CANCELLED; }\n", out);
	xl6OutputPuts("    if ( strcmp(kind, \"NotImplementedError\") == 0 ) { return XERR_UNSUPPORTED; }\n", out);
	xl6OutputPuts("    if ( strcmp(kind, \"ValueError\") == 0 || strcmp(kind, \"LookupError\") == 0 || strcmp(kind, \"KeyError\") == 0 || strcmp(kind, \"AttributeError\") == 0 || strcmp(kind, \"NameError\") == 0 || strcmp(kind, \"ArithmeticError\") == 0 || strcmp(kind, \"ZeroDivisionError\") == 0 ) { return XERR_VALUE; }\n", out);
	xl6OutputPuts("    return XERR_STATE;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static const char* xl6_error_kind_from_xrt(const xerror* nativeError)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const char* domain = nativeError != 0 ? xrtErrorDomain(nativeError) : 0;\n", out);
    xl6OutputPuts("    const char* data = nativeError != 0 ? xrtErrorData(nativeError) : 0;\n", out);
    xl6OutputPuts("    xerrkind kind = nativeError != 0 ? xrtErrorKind(nativeError) : XERR_INTERNAL;\n", out);
    xl6OutputPuts("    if ( domain != 0 && strcmp(domain, \"xlang\") == 0 && data != 0 && data[0] != 0 ) { return data; }\n", out);
    xl6OutputPuts("    if ( kind == XERR_ARGUMENT || kind == XERR_VALUE || kind == XERR_EXISTS ) { return \"ValueError\"; }\n", out);
    xl6OutputPuts("    if ( kind == XERR_TYPE ) { return \"TypeError\"; }\n", out);
    xl6OutputPuts("    if ( kind == XERR_RANGE ) { return \"RangeError\"; }\n", out);
    xl6OutputPuts("    if ( kind == XERR_MEMORY ) { return \"MemoryError\"; }\n", out);
    xl6OutputPuts("    if ( kind == XERR_TIMEOUT ) { return \"TimeoutError\"; }\n", out);
    xl6OutputPuts("    if ( kind == XERR_CANCELLED ) { return \"CancelledError\"; }\n", out);
    xl6OutputPuts("    if ( kind == XERR_UNSUPPORTED ) { return \"NotImplementedError\"; }\n", out);
    xl6OutputPuts("    if ( kind == XERR_NOT_FOUND && domain != 0 && strstr(domain, \"file\") != 0 ) { return \"FileError\"; }\n", out);
    xl6OutputPuts("    if ( kind == XERR_NOT_FOUND ) { return \"LookupError\"; }\n", out);
    xl6OutputPuts("    if ( kind == XERR_IO || kind == XERR_PERMISSION || kind == XERR_PROTOCOL ) {\n", out);
    xl6OutputPuts("        if ( domain != 0 && strstr(domain, \"file\") != 0 ) { return \"FileError\"; }\n", out);
    xl6OutputPuts("        if ( domain != 0 && (strstr(domain, \"net\") != 0 || strstr(domain, \"tls\") != 0) ) { return \"NetworkError\"; }\n", out);
    xl6OutputPuts("        return \"IOError\";\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return \"RuntimeError\";\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static XL6_Error* xl6_error_from_xrt_depth(const xerror* nativeError, const char* file, int line, int column, int depth)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const xerror* nativeCause;\n", out);
    xl6OutputPuts("    const char* nativeFile;\n", out);
    xl6OutputPuts("    int nativeLine;\n", out);
    xl6OutputPuts("    int nativeColumn;\n", out);
    xl6OutputPuts("    int hasNativeLocation;\n", out);
    xl6OutputPuts("    XL6_Error* cause;\n", out);
    xl6OutputPuts("    XL6_Error* error;\n", out);
    xl6OutputPuts("    if ( nativeError == 0 || depth >= 64 ) { return 0; }\n", out);
    xl6OutputPuts("    nativeFile = xrtErrorFile(nativeError);\n", out);
    xl6OutputPuts("    nativeLine = (int)xrtErrorLine(nativeError);\n", out);
    xl6OutputPuts("    nativeColumn = (int)xrtErrorColumn(nativeError);\n", out);
    xl6OutputPuts("    hasNativeLocation = (nativeFile != 0 && nativeFile[0] != '\\0') || nativeLine > 0 || nativeColumn > 0;\n", out);
    xl6OutputPuts("    nativeCause = xrtErrorCause(nativeError);\n", out);
    xl6OutputPuts("    cause = xl6_error_from_xrt_depth(nativeCause, \"\", 0, 0, depth + 1);\n", out);
    xl6OutputPuts("    if ( nativeCause != 0 && cause == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    error = xl6_error_new(xl6_error_kind_from_xrt(nativeError), xrtErrorMessage(nativeError),\n", out);
    xl6OutputPuts("        hasNativeLocation ? (nativeFile != 0 ? nativeFile : \"\") : file,\n", out);
    xl6OutputPuts("        hasNativeLocation ? nativeLine : line,\n", out);
    xl6OutputPuts("        hasNativeLocation ? nativeColumn : column, cause);\n", out);
    xl6OutputPuts("    if ( error == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    error->xrtKind = xrtErrorKind(nativeError);\n", out);
    xl6OutputPuts("    error->code = xrtErrorCode(nativeError);\n", out);
    xl6OutputPuts("    error->systemCode = xrtErrorSystemCode(nativeError);\n", out);
    xl6OutputPuts("    error->domain = xl6_error_dup(xrtErrorDomain(nativeError));\n", out);
    xl6OutputPuts("    error->operation = xl6_error_dup(xrtErrorOperation(nativeError));\n", out);
    xl6OutputPuts("    error->data = xl6_error_dup(xrtErrorData(nativeError));\n", out);
    xl6OutputPuts("    if ( error->domain == 0 || error->operation == 0 || error->data == 0 ) { xl6_error_free(error); return 0; }\n", out);
    xl6OutputPuts("    return error;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static XL6_Error* xl6_error_from_xrt(const xerror* nativeError, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_error_from_xrt_depth(nativeError, file, line, column, 0);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xerror* xl6_error_to_xrt_depth(const XL6_Error* error, const char* defaultOperation, int depth)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xerrordesc desc;\n", out);
    xl6OutputPuts("    xerrorlocation location;\n", out);
    xl6OutputPuts("    xerror* cause;\n", out);
    xl6OutputPuts("    xerror* nativeError;\n", out);
    xl6OutputPuts("    if ( error == 0 || depth >= 64 ) { return 0; }\n", out);
    xl6OutputPuts("    cause = xl6_error_to_xrt_depth(error->cause, defaultOperation, depth + 1);\n", out);
    xl6OutputPuts("    if ( error->cause != 0 && cause == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    memset(&desc, 0, sizeof(desc));\n", out);
    xl6OutputPuts("    desc.Kind = error->xrtKind != XERR_NONE ? error->xrtKind : xl6_error_xrt_kind(error->kind);\n", out);
    xl6OutputPuts("    desc.Code = error->code;\n", out);
    xl6OutputPuts("    desc.SystemCode = error->systemCode;\n", out);
    xl6OutputPuts("    desc.Domain = error->domain != 0 ? error->domain : \"xlang\";\n", out);
    xl6OutputPuts("    desc.Operation = error->operation != 0 ? error->operation : defaultOperation;\n", out);
    xl6OutputPuts("    desc.Message = error->message != 0 ? error->message : error->kind;\n", out);
    xl6OutputPuts("    desc.Data = error->data != 0 ? error->data : error->kind;\n", out);
    xl6OutputPuts("    desc.Cause = cause;\n", out);
    xl6OutputPuts("    location.File = error->file != 0 ? error->file : \"\";\n", out);
    xl6OutputPuts("    location.Line = (int32_t)error->line;\n", out);
    xl6OutputPuts("    location.Column = (int32_t)error->column;\n", out);
    xl6OutputPuts("    nativeError = xrtErrorBuildAt(&desc, &location);\n", out);
    xl6OutputPuts("    xrtErrorFree(cause);\n", out);
    xl6OutputPuts("    return nativeError;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xerror* xl6_error_to_xrt(const XL6_Error* error, const char* defaultOperation)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_error_to_xrt_depth(error, defaultOperation != 0 ? defaultOperation : \"throw\", 0);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void xl6_error_publish_xrt(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Error* error = xl6_error_active_get();\n", out);
    xl6OutputPuts("    if ( error != 0 ) {\n", out);
    xl6OutputPuts("        xerror* nativeError = xl6_error_to_xrt(error, \"callback\");\n", out);
    xl6OutputPuts("        if ( nativeError != 0 ) { xrtSetError(nativeError); xrtErrorFree(nativeError); }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("}\n\n", out);

	/* 扩展库只看到稳定 ABI，不直接依赖错误运行时的私有线程状态。 */
	xl6OutputPuts("int xl6ExtlibErrorPending(void)\n", out);
	xl6OutputPuts("{\n", out);
	xl6OutputPuts("    return xl6_error_has();\n", out);
	xl6OutputPuts("}\n\n", out);

	xl6OutputPuts("void xl6ExtlibErrorPublishXrt(void)\n", out);
	xl6OutputPuts("{\n", out);
	xl6OutputPuts("    xl6_error_publish_xrt();\n", out);
	xl6OutputPuts("}\n\n", out);

	xl6OutputPuts("void xl6ExtlibErrorClear(void)\n", out);
	xl6OutputPuts("{\n", out);
	xl6OutputPuts("    XL6_Error* error = xl6_error_take();\n", out);
	xl6OutputPuts("    xl6_error_free(error);\n", out);
	xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void xl6_error_reraise(XL6_Error* error)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xl6_error_retain(error);\n", out);
    xl6OutputPuts("    xl6_error_set_active(error);\n", out);
    xl6OutputPuts("}\n\n", out);

    /* 整数除法先检查 C 未定义行为，并把错误送入 xlang 异常链。 */
    xl6OutputPuts("static void xl6_error_raise(const char* kind, const char* message, const char* file, int line, int column);\n\n", out);
    xl6OutputPuts("static long long xl6_i64_div_checked(long long left, long long right, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( right == 0 ) { xl6_error_raise(\"ZeroDivisionError\", \"integer division by zero\", \"\", line, column); return 0; }\n", out);
    xl6OutputPuts("    if ( left == (-9223372036854775807LL - 1LL) && right == -1 ) { xl6_error_raise(\"OverflowError\", \"integer division overflow\", \"\", line, column); return 0; }\n", out);
    xl6OutputPuts("    return left / right;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static long long xl6_i64_mod_checked(long long left, long long right, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( right == 0 ) { xl6_error_raise(\"ZeroDivisionError\", \"integer modulo by zero\", \"\", line, column); return 0; }\n", out);
    xl6OutputPuts("    if ( left == (-9223372036854775807LL - 1LL) && right == -1 ) { xl6_error_raise(\"OverflowError\", \"integer modulo overflow\", \"\", line, column); return 0; }\n", out);
    xl6OutputPuts("    return left % right;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static unsigned long long xl6_u64_div_checked(unsigned long long left, unsigned long long right, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( right == 0 ) { xl6_error_raise(\"ZeroDivisionError\", \"integer division by zero\", \"\", line, column); return 0; }\n", out);
    xl6OutputPuts("    return left / right;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static unsigned long long xl6_u64_mod_checked(unsigned long long left, unsigned long long right, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( right == 0 ) { xl6_error_raise(\"ZeroDivisionError\", \"integer modulo by zero\", \"\", line, column); return 0; }\n", out);
    xl6OutputPuts("    return left % right;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static unsigned long long xl6_u64_shl_checked(unsigned long long left, long long right, unsigned int width, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( width == 0u || width > 64u || right < 0 || (unsigned long long)right >= (unsigned long long)width ) { xl6_error_raise(\"RangeError\", \"shift count is outside the left operand width\", \"\", line, column); return 0; }\n", out);
    xl6OutputPuts("    return left << (unsigned int)right;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static unsigned long long xl6_u64_shr_checked(unsigned long long left, long long right, unsigned int width, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( width == 0u || width > 64u || right < 0 || (unsigned long long)right >= (unsigned long long)width ) { xl6_error_raise(\"RangeError\", \"shift count is outside the left operand width\", \"\", line, column); return 0; }\n", out);
    xl6OutputPuts("    return left >> (unsigned int)right;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("#ifdef XL_RUNTIME_NEED_XVALUE_BINARY\n", out);
    xl6OutputPuts("static xvalue* xl6_xrt_basic_binary_checked(xvalue* left, xvalue* right, int op, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    int leftType = xl6_xrt_value_kind(left);\n", out);
    xl6OutputPuts("    int rightType = xl6_xrt_value_kind(right);\n", out);
    xl6OutputPuts("    if ( (op == 3 || op == 4 || op == 8 || op == 9) && leftType == XL6_XVO_DT_INT && rightType == XL6_XVO_DT_INT ) {\n", out);
    xl6OutputPuts("        long long l = xl6_xrt_value_int(left);\n", out);
    xl6OutputPuts("        long long r = xl6_xrt_value_int(right);\n", out);
    xl6OutputPuts("        if ( op == 3 ) { long long value = xl6_i64_div_checked(l, r, line, column); return xl6_error_has() ? xrtValueNull() : xrtValueInt(value); }\n", out);
    xl6OutputPuts("        if ( op == 4 ) { long long value = xl6_i64_mod_checked(l, r, line, column); return xl6_error_has() ? xrtValueNull() : xrtValueInt(value); }\n", out);
    xl6OutputPuts("        if ( op == 8 ) { unsigned long long value = xl6_u64_shl_checked((unsigned long long)l, r, 64u, line, column); return xl6_error_has() ? xrtValueNull() : xrtValueInt((long long)value); }\n", out);
    xl6OutputPuts("        { unsigned long long value = xl6_u64_shr_checked((unsigned long long)l, r, 64u, line, column); return xl6_error_has() ? xrtValueNull() : xrtValueInt((long long)value); }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return xl6_xrt_basic_binary(left, right, op);\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static xvalue* xl6_xrt_basic_binary_release_checked(xvalue* left, xvalue* right, int op, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue* result = (left != 0 && right != 0) ? xl6_xrt_basic_binary_checked(left, right, op, line, column) : xrtValueNull();\n", out);
    xl6OutputPuts("    if ( left != 0 ) { xrtValueRelease(left); }\n", out);
    xl6OutputPuts("    if ( right != 0 ) { xrtValueRelease(right); }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("#endif\n\n", out);
    xl6OutputPuts("static void xl6_error_raise(const char* kind, const char* message, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Error* cause = xl6_error_take();\n", out);
    xl6OutputPuts("    XL6_Error* error = xl6_error_new(kind, message, file, line, column, cause);\n", out);
    xl6OutputPuts("    if ( error == 0 ) {\n", out);
    xl6OutputPuts("        error = xl6_error_new(\"Error\", \"out of memory while creating error\", file, line, column, 0);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( error != 0 ) { error->xrtKind = xl6_error_xrt_kind(kind); }\n", out);
    xl6OutputPuts("    xl6_error_set_active(error);\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static void* xl6_nullable_require_pointer(void* value, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( value == 0 ) {\n", out);
    xl6OutputPuts("        xl6_error_raise(\"TypeError\", \"cannot cast null nullable value to a non-null type\", file, line, column);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return value;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static const char* xl6_error_text_copy(const char* text)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t len = text != 0 ? strlen(text) : 0;\n", out);
    xl6OutputPuts("    char* copy;\n\n", out);
    xl6OutputPuts("    if ( len == 0 ) {\n", out);
    xl6OutputPuts("        return \"\";\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    copy = (char*)xrtTemp(len + 1);\n", out);
    xl6OutputPuts("    if ( copy == 0 ) {\n", out);
    xl6OutputPuts("        return \"\";\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    memcpy(copy, text, len + 1);\n", out);
    xl6OutputPuts("    return copy;\n", out);
    xl6OutputPuts("}\n\n", out);
    /* 成员访问可接收借用值或临时值；先复制结果，再按需释放接收者。 */
    xl6OutputPuts("static const char* xl6_error_text_member(XL6_Error* error, const char* text, int release) { const char* result = xl6_error_text_copy(text); if ( release ) { xl6_error_free(error); } return result; }\n", out);
    xl6OutputPuts("static int xl6_error_int_member(XL6_Error* error, int value, int release) { if ( release ) { xl6_error_free(error); } return value; }\n", out);
    xl6OutputPuts("static const char* xl6_error_kind(XL6_Error* error, int release) { return xl6_error_text_member(error, error != 0 ? error->kind : 0, release); }\n", out);
    xl6OutputPuts("static const char* xl6_error_message(XL6_Error* error, int release) { return xl6_error_text_member(error, error != 0 ? error->message : 0, release); }\n", out);
    xl6OutputPuts("static const char* xl6_error_file(XL6_Error* error, int release) { return xl6_error_text_member(error, error != 0 ? error->file : 0, release); }\n", out);
    xl6OutputPuts("static const char* xl6_error_domain(XL6_Error* error, int release) { return xl6_error_text_member(error, error != 0 && error->domain != 0 ? error->domain : \"xlang\", release); }\n", out);
    xl6OutputPuts("static const char* xl6_error_operation(XL6_Error* error, int release) { return xl6_error_text_member(error, error != 0 && error->operation != 0 ? error->operation : \"throw\", release); }\n", out);
    xl6OutputPuts("static const char* xl6_error_data(XL6_Error* error, int release) { return xl6_error_text_member(error, error != 0 && error->data != 0 ? error->data : (error != 0 ? error->kind : 0), release); }\n", out);
    xl6OutputPuts("static int xl6_error_line(XL6_Error* error, int release) { return xl6_error_int_member(error, error != 0 ? error->line : 0, release); }\n", out);
    xl6OutputPuts("static int xl6_error_column(XL6_Error* error, int release) { return xl6_error_int_member(error, error != 0 ? error->column : 0, release); }\n", out);
    xl6OutputPuts("static int xl6_error_code(XL6_Error* error, int release) { return xl6_error_int_member(error, error != 0 ? error->code : 0, release); }\n", out);
    xl6OutputPuts("static int xl6_error_system_code(XL6_Error* error, int release) { return xl6_error_int_member(error, error != 0 ? error->systemCode : 0, release); }\n", out);
    xl6OutputPuts("static XL6_Error* xl6_error_cause(XL6_Error* error, int release) { XL6_Error* cause = xl6_error_retain(error != 0 ? error->cause : 0); if ( release ) { xl6_error_free(error); } return cause; }\n\n", out);

    xl6OutputPuts("static const char* xl6_error_parent_kind(const char* kind)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( kind == 0 || kind[0] == 0 || strcmp(kind, \"Error\") == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( strcmp(kind, \"AssertionError\") == 0 || strcmp(kind, \"NullReferenceError\") == 0 || strcmp(kind, \"NotImplementedError\") == 0 || strcmp(kind, \"NativeError\") == 0 || strcmp(kind, \"ArithmeticError\") == 0 ) { return \"RuntimeError\"; }\n", out);
    xl6OutputPuts("    if ( strcmp(kind, \"ZeroDivisionError\") == 0 || strcmp(kind, \"OverflowError\") == 0 ) { return \"ArithmeticError\"; }\n", out);
    xl6OutputPuts("    if ( strcmp(kind, \"RangeError\") == 0 ) { return \"ValueError\"; }\n", out);
    xl6OutputPuts("    if ( strcmp(kind, \"IndexError\") == 0 || strcmp(kind, \"KeyError\") == 0 || strcmp(kind, \"AttributeError\") == 0 || strcmp(kind, \"NameError\") == 0 ) { return \"LookupError\"; }\n", out);
    xl6OutputPuts("    if ( strcmp(kind, \"FileError\") == 0 || strcmp(kind, \"EncodingError\") == 0 || strcmp(kind, \"NetworkError\") == 0 || strcmp(kind, \"TimeoutError\") == 0 ) { return \"IOError\"; }\n", out);
    xl6OutputPuts("    if ( strcmp(kind, \"CompressionError\") == 0 || strcmp(kind, \"ArchiveError\") == 0 ) { return \"IOError\"; }\n", out);
    xl6OutputPuts("    if ( strcmp(kind, \"ModuleNotFoundError\") == 0 ) { return \"ImportError\"; }\n", out);
    xl6OutputPuts("    if ( strcmp(kind, \"FutureError\") == 0 || strcmp(kind, \"TaskError\") == 0 || strcmp(kind, \"CancelledError\") == 0 || strcmp(kind, \"GeneratorError\") == 0 ) { return \"AsyncError\"; }\n", out);
    xl6OutputPuts("    if ( strcmp(kind, \"SyntaxError\") == 0 ) { return \"ParseError\"; }\n", out);
    xl6OutputPuts("    if ( strcmp(kind, \"CsvError\") == 0 || strcmp(kind, \"ConfigError\") == 0 || strcmp(kind, \"TomlError\") == 0 || strcmp(kind, \"PlistError\") == 0 || strcmp(kind, \"XmlError\") == 0 ) { return \"ParseError\"; }\n", out);
    xl6OutputPuts("    return \"Error\";\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_error_is(XL6_Error* error, const char* target)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const char* kind = error != 0 ? error->kind : 0;\n", out);
    xl6OutputPuts("    if ( target == 0 || target[0] == 0 ) { return 1; }\n", out);
    xl6OutputPuts("    while ( kind != 0 && kind[0] != 0 ) {\n", out);
    xl6OutputPuts("        if ( strcmp(kind, target) == 0 ) { return 1; }\n", out);
    xl6OutputPuts("        kind = xl6_error_parent_kind(kind);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void xl6_error_print_uncaught(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Error* error = xl6_error_active_get();\n", out);
    xl6OutputPuts("    const char* kind;\n", out);
    xl6OutputPuts("    const char* message;\n", out);
    xl6OutputPuts("    const char* file;\n", out);
    xl6OutputPuts("    int line;\n", out);
    xl6OutputPuts("    int column;\n\n", out);
    xl6OutputPuts("    if ( error == 0 ) {\n", out);
    xl6OutputPuts("        return;\n", out);
    xl6OutputPuts("    }\n", out);
    /* 未捕获错误会在打印完成后才释放，直接读取拥有字段即可，避免额外临时内存。 */
    xl6OutputPuts("    kind = error->kind;\n", out);
    xl6OutputPuts("    message = error->message;\n", out);
    xl6OutputPuts("    file = error->file;\n", out);
    xl6OutputPuts("    line = error->line;\n", out);
    xl6OutputPuts("    column = error->column;\n", out);
    xl6OutputPuts("    if ( kind == 0 || kind[0] == 0 ) {\n", out);
    xl6OutputPuts("        kind = \"Error\";\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( message == 0 ) {\n", out);
    xl6OutputPuts("        message = \"\";\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( file != 0 && file[0] != 0 && line > 0 && column > 0 ) {\n", out);
    xl6OutputPuts("        f", out);
    xl6OutputPuts("pri", out);
    xl6OutputPuts("ntf(st", out);
    xl6OutputPuts("derr, \"%s:%d:%d: uncaught %s: %s\\n\", file, line, column, kind, message);\n", out);
    xl6OutputPuts("    } else if ( line > 0 && column > 0 ) {\n", out);
    xl6OutputPuts("        f", out);
    xl6OutputPuts("pri", out);
    xl6OutputPuts("ntf(st", out);
    xl6OutputPuts("derr, \"%d:%d: uncaught %s: %s\\n\", line, column, kind, message);\n", out);
    xl6OutputPuts("    } else {\n", out);
    xl6OutputPuts("        f", out);
    xl6OutputPuts("pri", out);
    xl6OutputPuts("ntf(st", out);
    xl6OutputPuts("derr, \"uncaught %s: %s\\n\", kind, message);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("}\n\n", out);
}

#include "xl6_cgen_stmt_error_control.c"
