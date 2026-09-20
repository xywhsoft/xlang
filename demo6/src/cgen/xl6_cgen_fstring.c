static int xl6CgenFStringIsRaw(const XL6_AstNode* node)
{
    return node != NULL && node->text != NULL && node->text[0] == 'r' && node->text[1] == 'f';
}

static void xl6CgenFStringTextLiteral(XL6_CgenContext* ctx, const char* text, int raw)
{
    const unsigned char* p;

    xl6OutputPutc('"', ctx->out);
    if ( text != NULL ) {
        for ( p = (const unsigned char*)text; *p != '\0'; ++p ) {
            if ( !raw && *p == '\\' && p[1] != '\0' ) {
                xl6OutputPutc('\\', ctx->out);
                xl6OutputPutc((int)*(++p), ctx->out);
                continue;
            }
            if ( *p == '\r' ) {
                if ( p[1] == '\n' ) {
                    ++p;
                }
                xl6CgenWriteCStringChar(ctx->out, '\n');
                continue;
            }
            xl6CgenWriteCStringChar(ctx->out, *p);
        }
    }
    xl6OutputPutc('"', ctx->out);
}

static int xl6CgenFStringTypeNeedsRuntime(const char* typeName)
{
    return xl6CgenIsIntegerTypeName(typeName) ||
           xl6CgenIsFloatTypeName(typeName) ||
           xl6CTextEquals(typeName, "time") ||
           xl6CTextEquals(typeName, "point") ||
           xl6CTextEquals(typeName, "var");
}

static void xl6CgenFStringValueExpr(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* typeNode;
    const char* typeName;

    typeNode = xl6CgenCloneExprTypeForInference(ctx, expr);
    typeName = typeNode != NULL ? typeNode->text : NULL;

    if ( xl6CTextEquals(typeName, "string") ) {
        xl6CgenExpr(ctx, expr);
    } else if ( xl6CTextEquals(typeName, "null") ) {
        xl6OutputPuts("\"\"", ctx->out);
    } else if ( xl6CgenIsIntegerTypeName(typeName) ) {
        xl6OutputPuts(xl6CgenIsUnsignedIntegerTypeName(typeName) ?
                      "xl6_xrt_u64_to_string((unsigned long long)(" :
                      "xl6_xrt_i64_to_string((long long)(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts("))", ctx->out);
    } else if ( xl6CgenIsFloatTypeName(typeName) ) {
        xl6OutputPuts("xl6_xrt_f64_to_string((double)(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts("))", ctx->out);
    } else if ( xl6CgenIsBoolTypeName(typeName) ) {
        xl6OutputPutc('(', ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts(" ? \"true\" : \"false\")", ctx->out);
    } else if ( xl6CTextEquals(typeName, "time") ) {
        xl6OutputPuts("xl6_xrt_take_cstr((char*)xl6_xrt_time_string_owned((long long)(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts("), 0))", ctx->out);
    } else if ( xl6CTextEquals(typeName, "point") ) {
        xl6OutputPuts("xl6_xrt_take_cstr((char*)xrtFormat(\"[point:%p]\", ", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts("))", ctx->out);
    } else if ( xl6CTextEquals(typeName, "var") ) {
        if ( xl6CgenExprIsXValueBorrow(ctx, expr) ) {
            xl6OutputPuts("xl6_xrt_value_to_string(", ctx->out);
        } else {
            xl6OutputPuts("xl6_xrt_value_to_string_release(", ctx->out);
        }
        xl6CgenExpr(ctx, expr);
        xl6OutputPutc(')', ctx->out);
    } else {
        xl6CgenError(ctx, expr, "f-string 插值表达式不能转换为 string");
        xl6OutputPuts("\"\"", ctx->out);
    }

    xl6AstFree(typeNode);
}

static void xl6CgenFStringExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;
    int raw;
    size_t count;

    raw = xl6CgenFStringIsRaw(node);
    count = node != NULL ? node->childCount : 0;

    xl6OutputPrintf(ctx->out, "xl6_xrt_fstr_join(%u", (unsigned int)count);
    for ( i = 0; node != NULL && i < node->childCount; ++i ) {
        const XL6_AstNode* child = node->children[i];
        if ( child == NULL ) {
            continue;
        }
        xl6OutputPuts(", ", ctx->out);
        if ( child->kind == XL6_AST_INTERP_TEXT ) {
            xl6CgenFStringTextLiteral(ctx, child->text, raw);
        } else {
            xl6CgenFStringValueExpr(ctx, child);
        }
    }
    xl6OutputPutc(')', ctx->out);
}

static int xl6CgenFStringNeedsTypeConvertRuntime(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL || node->kind != XL6_AST_INTERP_STRING ) {
        return 0;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        const XL6_AstNode* child = node->children[i];
        XL6_AstNode* typeNode;
        const char* typeName;
        int result;

        if ( child == NULL || child->kind == XL6_AST_INTERP_TEXT ) {
            continue;
        }
        typeNode = xl6CgenCloneExprTypeForInference(ctx, child);
        typeName = typeNode != NULL ? typeNode->text : NULL;
        result = xl6CgenFStringTypeNeedsRuntime(typeName);
        xl6AstFree(typeNode);
        if ( result ) {
            return 1;
        }
    }
    return 0;
}
