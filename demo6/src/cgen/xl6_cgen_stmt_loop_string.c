static void xl6CgenForStringIterable(XL6_CgenContext* ctx, const XL6_AstNode* node, const XL6_AstNode* iterableType)
{
    const XL6_AstNode* targets;
    const XL6_AstNode* iterable;
    XL6_CgenTaskSpawnArgTempScope taskScope;
    size_t varMark;
    size_t loopMark;
    size_t iterCleanupMark;
    int hasTaskScope;
    int id;
    char iterName[64];
    char sourceName[64];
    char lenName[64];
    char posName[64];
    char charSizeName[64];
    char indexName[64];

    (void)iterableType;
    if ( node->childCount < 3 ) {
        xl6CgenError(ctx, node, "malformed string for-in statement");
        return;
    }

    targets = node->children[0];
    iterable = node->children[1];
    if ( targets == NULL || targets->childCount == 0 || targets->childCount > 2 ) {
        xl6CgenError(ctx, targets, "for string 只支持一个或两个迭代变量");
        return;
    }

    id = ctx->tempId++;
    snprintf(iterName, sizeof(iterName), "__xl6_string_iter_%d", id);
    snprintf(sourceName, sizeof(sourceName), "__xl6_string_source_%d", id);
    snprintf(lenName, sizeof(lenName), "__xl6_string_len_%d", id);
    snprintf(posName, sizeof(posName), "__xl6_string_pos_%d", id);
    snprintf(charSizeName, sizeof(charSizeName), "__xl6_string_char_size_%d", id);
    snprintf(indexName, sizeof(indexName), "__xl6_string_index_%d", id);

    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("extern void xrtFree(void* p);\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("extern char* xrtCopyStr(const char* sText, size_t iSize);\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("extern size_t xrtStrUtf8CharSize(const char* sText, size_t iSize, size_t iPos);\n", ctx->out);

    iterCleanupMark = ctx->cleanupCount;
    hasTaskScope = iterable != NULL && xl6CgenTaskSpawnArgTempScopeBegin(ctx, iterable, &taskScope);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "const char* %s = ", sourceName);
    xl6CgenExpr(ctx, iterable);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out,
                    "char* %s = xrtCopyStr(%s != 0 ? %s : \"\", 0u);\n",
                    iterName,
                    sourceName,
                    sourceName);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (%s == 0) { xl6_runtime_fatal(3); }\n", iterName);
    xl6CgenAddXrtPtrCleanup(ctx, iterName, iterable);
    if ( hasTaskScope ) {
        xl6CgenTaskSpawnArgTempScopeEnd(ctx, &taskScope);
    }
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "size_t %s = strlen(%s);\n", lenName, iterName);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "size_t %s = 0;\n", posName);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "long long %s = 0;\n", indexName);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "while (%s < %s) {\n", posName, lenName);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "size_t %s = xrtStrUtf8CharSize(%s, %s, %s);\n",
            charSizeName, iterName, lenName, posName);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (%s == 0) { break; }\n", charSizeName);

    varMark = ctx->varCount;
    loopMark = ctx->cleanupCount;
    if ( targets->childCount == 1 ) {
        char valueName[512];

        xl6CgenForTargetCName(valueName, sizeof(valueName), targets, 0);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "char* %s = xrtCopyStr(%s + %s, %s);\n",
                valueName, iterName, posName, charSizeName);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s == 0 && %s != 0) { xl6_runtime_fatal(3); }\n", valueName, charSizeName);
        xl6CgenRegisterLoopTargetType(ctx, targets->children[0], "string");
    } else {
        char userIndexName[512];
        char valueName[512];

        xl6CgenForTargetCName(userIndexName, sizeof(userIndexName), targets, 0);
        xl6CgenForTargetCName(valueName, sizeof(valueName), targets, 1);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "long long %s = %s;\n", userIndexName, indexName);
        xl6CgenRegisterLoopTargetType(ctx, targets->children[0], "int");
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "char* %s = xrtCopyStr(%s + %s, %s);\n",
                valueName, iterName, posName, charSizeName);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s == 0 && %s != 0) { xl6_runtime_fatal(3); }\n", valueName, charSizeName);
        xl6CgenRegisterLoopTargetType(ctx, targets->children[1], "string");
    }

    xl6CgenForLoopBody(ctx, node, loopMark, loopMark, varMark, id, id);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s += %s;\n", posName, charSizeName);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "++%s;\n", indexName);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "__xl6_break_%d:\n", id);
    xl6CgenIndent(ctx);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenEmitCleanupsSince(ctx, iterCleanupMark);
    xl6CgenFreeCleanupsSince(ctx, iterCleanupMark);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}
