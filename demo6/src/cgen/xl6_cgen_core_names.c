static void xl6CgenError(XL6_CgenContext* ctx, const XL6_AstNode* node, const char* message)
{
    ctx->ok = 0;
    if ( ctx->diags != NULL ) {
        xl6DiagAdd(ctx->diags, node != NULL ? node->line : 1, node != NULL ? node->column : 1, message);
    }
}

static const char* xl6CgenOwnText(XL6_CgenContext* ctx, const char* text)
{
    XL6_CgenTextBlock* block;
    size_t length;
    size_t capacity;
    char* result;

    if ( ctx == NULL || text == NULL ) {
        return NULL;
    }
    length = strlen(text) + 1;
    block = ctx->textBlocks;
    if ( block == NULL || length > block->capacity - block->used ) {
        capacity = length > 4096 ? length : 4096;
        if ( capacity > SIZE_MAX - sizeof(XL6_CgenTextBlock) + 1 ) {
            xl6CgenError(ctx, NULL, "C generator text arena size overflow");
            return NULL;
        }
        block = (XL6_CgenTextBlock*)xrtMalloc(sizeof(XL6_CgenTextBlock) + capacity - 1);
        if ( block == NULL ) {
            xl6CgenError(ctx, NULL, "C generator text arena out of memory");
            return NULL;
        }
        block->next = ctx->textBlocks;
        block->used = 0;
        block->capacity = capacity;
        ctx->textBlocks = block;
    }
    result = block->data + block->used;
    memcpy(result, text, length);
    block->used += length;
    return result;
}

static void xl6CgenFreeTextBlocks(XL6_CgenContext* ctx)
{
    XL6_CgenTextBlock* block;

    if ( ctx == NULL ) {
        return;
    }
    block = ctx->textBlocks;
    while ( block != NULL ) {
        XL6_CgenTextBlock* next = block->next;
        xrtFree(block);
        block = next;
    }
    ctx->textBlocks = NULL;
}

static void xl6CgenIndent(XL6_CgenContext* ctx)
{
    int i;

    for ( i = 0; i < ctx->indent; ++i ) {
        xl6OutputPuts("    ", ctx->out);
    }
}

static int xl6CgenIsSimpleCIdent(const char* text)
{
    return xl6OutputIsSimpleCIdentifier(text);
}

#define XL6_CGEN_C_IDENT_MAX XL6_C_SYMBOL_COMPONENT_MAX

void xl6CgenEncodeSymbolComponent(char* out, size_t capacity, const char* text)
{
    if ( !xl6OutputEncodeSymbolComponent(out, capacity, text) &&
         out != NULL && capacity != 0 ) out[0] = '\0';
}

static void xl6CgenFputsEncodedText(XL6_Output* out, const char* text)
{
    char encoded[XL6_CGEN_C_IDENT_MAX + 1];

    if ( out == NULL ) {
        return;
    }
    xl6CgenEncodeSymbolComponent(encoded, sizeof(encoded), text);
    xl6OutputPuts(encoded, out);
}

/* ------------------------------------------------------------------------- */
/* 自动导出符号：C 源、头文件和 ABI 元数据共用这一套唯一规则。 */

static int xl6CgenAutoExportOverloadCount(
    const XL6_AstNode* root,
    const char* name)
{
    size_t i;
    int count;

    if ( root == NULL || name == NULL ) {
        return 0;
    }
    count = 0;
    for ( i = 0; i < root->childCount; ++i ) {
        const XL6_AstNode* item = root->children[i];
        const XL6_AstNode* itemName;

        if ( item == NULL ||
             item->kind != XL6_AST_EXPORT_FUNC ||
             item->childCount == 0 ) {
            continue;
        }
        itemName = item->children[0];
        if ( itemName != NULL &&
             itemName->text != NULL &&
             strcmp(itemName->text, name) == 0 ) {
            count += 1;
        }
    }
    return count;
}

static void xl6CgenEmitAutoExportType(
    XL6_Output* out,
    const XL6_AstNode* typeNode)
{
    const XL6_AstNode* args;
    size_t i;

    if ( typeNode == NULL || typeNode->text == NULL ) {
        xl6OutputPutc('_', out);
        return;
    }
    xl6CgenFputsEncodedText(out, typeNode->text);
    args = typeNode->childCount > 0 &&
           typeNode->children[0] != NULL &&
           typeNode->children[0]->kind == XL6_AST_LIST ?
        typeNode->children[0] :
        NULL;
    if ( args == NULL || args->childCount == 0 ) {
        return;
    }
    xl6OutputPuts("_of", out);
    for ( i = 0; i < args->childCount; ++i ) {
        xl6OutputPutc('_', out);
        xl6CgenEmitAutoExportType(out, args->children[i]);
    }
}

void xl6CgenEmitAutoExportSymbol(
    XL6_Output* out,
    const XL6_AstNode* root,
    const XL6_AstNode* node)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* params;
    size_t i;

    if ( out == NULL ) {
        return;
    }
    nameNode = node != NULL && node->childCount > 0 ?
        node->children[0] :
        NULL;
    xl6OutputPuts("xl6_export_", out);
    xl6CgenFputsEncodedText(
        out,
        nameNode != NULL && nameNode->text != NULL ?
            nameNode->text :
            "_");
    if ( nameNode == NULL ||
         xl6CgenAutoExportOverloadCount(root, nameNode->text) <= 1 ) {
        return;
    }

    params = node->childCount > 1 ? node->children[1] : NULL;
    if ( params == NULL || params->childCount == 0 ) {
        xl6OutputPuts("__void", out);
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];

        xl6OutputPuts("__", out);
        if ( param != NULL && (param->flags & XL6_AST_FLAG_BYREF) != 0 ) {
            xl6OutputPuts("byref_", out);
        } else if ( param != NULL && (param->flags & XL6_AST_FLAG_OUT) != 0 ) {
            xl6OutputPuts("out_", out);
        }
        xl6CgenEmitAutoExportType(
            out,
            param != NULL && param->childCount > 0 ?
                param->children[0] :
                NULL);
    }
}

static void xl6CgenAppendText(char* buffer, size_t capacity, const char* text);

void xl6CgenEncodeCIdentifier(char* out, size_t capacity, const char* text)
{
    if ( !xl6OutputEncodeCIdentifier(out, capacity, text) &&
         out != NULL && capacity != 0 ) out[0] = '\0';
}

static void xl6CgenFputsCIdent(XL6_Output* out, const char* text)
{
    char encoded[XL6_CGEN_C_IDENT_MAX + 1];

    if ( out == NULL ) {
        return;
    }
    xl6CgenEncodeCIdentifier(encoded, sizeof(encoded), text);
    xl6OutputPuts(encoded, out);
}

static void xl6CgenAppendCIdentText(char* buffer, size_t capacity, const char* text)
{
    char encoded[XL6_CGEN_C_IDENT_MAX + 1];

    xl6CgenEncodeCIdentifier(encoded, sizeof(encoded), text);
    xl6CgenAppendText(buffer, capacity, encoded);
}

static void xl6CgenCIdentText(char* buffer, size_t capacity, const char* text)
{
    if ( buffer == NULL || capacity == 0 ) {
        return;
    }
    buffer[0] = '\0';
    xl6CgenAppendCIdentText(buffer, capacity, text);
}

static int xl6CgenIsPrintIntrinsicName(const char* name)
{
    return xl6CTextEquals(name, "__xl_std_print") ||
           xl6CTextEquals(name, "__xl_std_println");
}

static int xl6CgenPrintIntrinsicNewline(const char* name)
{
    return xl6CTextEquals(name, "__xl_std_println") ? 1 : 0;
}

static int xl6CgenIsPrintIntrinsicFunc(const XL6_AstNode* node)
{
    const XL6_AstNode* nameNode;

    if ( node == NULL || node->kind != XL6_AST_FUNC || node->childCount == 0 ) {
        return 0;
    }
    nameNode = node->children[0];
    return nameNode != NULL &&
           nameNode->kind == XL6_AST_NAME &&
           xl6CgenIsPrintIntrinsicName(nameNode->text);
}

static int xl6CgenExternTextEqualsSymbol(const char* text, const char* symbol)
{
    size_t len;
    size_t textLen;

    if ( xl6CTextEquals(text, symbol) ) {
        return 1;
    }
    if ( text == NULL || symbol == NULL ) {
        return 0;
    }
    len = strlen(symbol);
    textLen = strlen(text);
    if ( textLen != len + 2 ) {
        return 0;
    }
    return ((text[0] == '"' && text[len + 1] == '"' && text[len + 2] == '\0') ||
            (text[0] == '\'' && text[len + 1] == '\'' && text[len + 2] == '\0')) &&
           strncmp(text + 1, symbol, len) == 0;
}



/* 判断最终展开 AST 是否声明了指定的 native extern。 */
static int xl6CgenNodeHasExternSymbol(
    const XL6_AstNode* node,
    const char* symbol
)
{
    size_t i;

    if ( node == NULL || symbol == NULL ) {
        return 0;
    }
    if ( (node->kind == XL6_AST_EXTERN_FUNC ||
          node->kind == XL6_AST_NATIVE_EXTERN_FUNC) &&
         xl6CgenExternTextEqualsSymbol(node->text, symbol) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; i += 1 ) {
        if ( xl6CgenNodeHasExternSymbol(node->children[i], symbol) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenCompareExternSymbol(const char* text, const char* symbol)
{
    size_t textLen;
    size_t symbolLen;
    size_t compareLen;
    int result;

    if ( text == NULL ) { return symbol == NULL ? 0 : -1; }
    if ( symbol == NULL ) { return 1; }

    textLen = strlen(text);
    if ( (textLen >= 2u) &&
         (((text[0] == '"') && (text[textLen - 1u] == '"')) ||
          ((text[0] == '\'') && (text[textLen - 1u] == '\''))) ) {
        ++text;
        textLen -= 2u;
    }

    symbolLen = strlen(symbol);
    compareLen = textLen < symbolLen ? textLen : symbolLen;
    result = memcmp(text, symbol, compareLen);
    if ( result != 0 ) { return result; }
    if ( textLen < symbolLen ) { return -1; }
    return textLen > symbolLen ? 1 : 0;
}

static int xl6CgenIsXrtHeaderSymbol(const char* text)
{
    static const char* const symbols[] = {
#define XL_XRT_SYMBOL(name) #name,
#include "../api/xl_xrt_symbols.inc"
#undef XL_XRT_SYMBOL
    };
    size_t left = 0u;
    size_t right = sizeof(symbols) / sizeof(symbols[0]);

    while ( left < right ) {
        size_t middle = left + ((right - left) / 2u);
        int compare = xl6CgenCompareExternSymbol(text, symbols[middle]);

        if ( compare == 0 ) { return 1; }
        if ( compare < 0 ) {
            right = middle;
        } else {
            left = middle + 1u;
        }
    }
    return 0;
}



/* 判断 extern 文本在去除可选引号后是否使用指定运行时头前缀。 */
static int xl6CgenExternTextHasPrefix(const char* text, const char* prefix)
{
    size_t textSize;
    size_t prefixSize;

    if ( text == NULL || prefix == NULL ) {
        return 0;
    }
    textSize = strlen(text);
    prefixSize = strlen(prefix);
    if ( textSize >= 2u &&
         ((text[0] == '"' && text[textSize - 1u] == '"') ||
          (text[0] == '\'' && text[textSize - 1u] == '\'')) ) {
        text += 1;
        textSize -= 2u;
    }
    return textSize >= prefixSize &&
           strncmp(text, prefix, prefixSize) == 0;
}



static int xl6CgenIsRuntimeHeaderExtern(const XL6_AstNode* node)
{
    return node != NULL &&
           node->kind == XL6_AST_EXTERN_FUNC &&
            (xl6CgenExternTextEqualsSymbol(node->text, "xl6_xrt_print") ||
             xl6CgenExternTextEqualsSymbol(node->text, "xrtHash64") ||
             xl6CgenExternTextEqualsSymbol(node->text, "xl6_xrt_last_error_text") ||
             xl6CgenExternTextHasPrefix(node->text, "xl6_xrt_dynamic_fields_") ||
             xl6CgenIsXrtHeaderSymbol(node->text));
}

static int xl6CgenIsInternalStdRuntimeExtern(const XL6_AstNode* node)
{
    const XL6_AstNode* nameNode;
    static const char prefix[] = "__xl_std_runtime_";

    if ( node == NULL ||
         (node->kind != XL6_AST_EXTERN_FUNC &&
          node->kind != XL6_AST_NATIVE_EXTERN_FUNC) ||
         node->childCount == 0 ) {
        return 0;
    }
    nameNode = node->children[0];
    return nameNode != NULL && nameNode->text != NULL &&
           strncmp(nameNode->text, prefix, sizeof(prefix) - 1u) == 0;
}

static void xl6CgenNativeBackedTypeName(XL6_Output* out, const char* name)
{
    xl6OutputPuts("XL_Class_", out);
    xl6CgenFputsEncodedText(out, name);
}

static void xl6CgenClassTypeName(XL6_Output* out, const char* name)
{
    xl6OutputPuts("XL_Class_", out);
    xl6CgenFputsEncodedText(out, name);
}

static void xl6CgenTypeDescSymbol(XL6_Output* out, const char* cTypeName)
{
    xl6OutputPuts("xl6_type_desc_", out);
    xl6OutputPuts(cTypeName != NULL ? cTypeName : "_", out);
}

static void xl6CgenBoxSymbol(XL6_Output* out, const char* cTypeName)
{
    xl6OutputPuts("xl6_box_", out);
    xl6OutputPuts(cTypeName != NULL ? cTypeName : "_", out);
}

static void xl6CgenDefaultClassSymbol(XL6_Output* out, const char* cTypeName)
{
    xl6OutputPuts("xl6_default_", out);
    xl6OutputPuts(cTypeName != NULL ? cTypeName : "_", out);
}

static void xl6CgenAppendText(char* buffer, size_t capacity, const char* text)
{
    size_t used;
    size_t left;
    size_t copyLen;

    if ( buffer == NULL || capacity == 0 || text == NULL ) {
        return;
    }
    used = strlen(buffer);
    if ( used + 1 >= capacity ) {
        return;
    }
    left = capacity - used - 1;
    copyLen = strlen(text);
    if ( copyLen > left ) {
        copyLen = left;
    }
    if ( copyLen > 0 ) {
        memcpy(buffer + used, text, copyLen);
    }
    buffer[used + copyLen] = '\0';
}

static void xl6CgenAppendMangledText(char* buffer, size_t capacity, const char* text)
{
    char encoded[XL6_CGEN_C_IDENT_MAX + 1];

    xl6CgenEncodeSymbolComponent(encoded, sizeof(encoded), text);
    xl6CgenAppendText(buffer, capacity, encoded);
}

static void xl6CgenAppendTypeId(char* buffer, size_t capacity, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* args;
    size_t i;

    if ( typeNode == NULL ) {
        xl6CgenAppendText(buffer, capacity, "_");
        return;
    }
    xl6CgenAppendMangledText(buffer, capacity, typeNode->text);
    args = typeNode->childCount > 0 && typeNode->children[0] != NULL &&
           typeNode->children[0]->kind == XL6_AST_LIST ? typeNode->children[0] : NULL;
    if ( args == NULL || args->childCount == 0 ) {
        return;
    }
    xl6CgenAppendText(buffer, capacity, "_of");
    for ( i = 0; i < args->childCount; ++i ) {
        xl6CgenAppendText(buffer, capacity, "_");
        xl6CgenAppendTypeId(buffer, capacity, args->children[i]);
    }
}

static void xl6CgenNativeBackedTypeRefName(char* buffer, size_t capacity, const char* name)
{
    if ( buffer == NULL || capacity == 0 ) {
        return;
    }
    buffer[0] = '\0';
    xl6CgenAppendText(buffer, capacity, "XL_Class_");
    xl6CgenAppendMangledText(buffer, capacity, name);
}

static void xl6CgenClassTypeTextName(char* buffer, size_t capacity, const char* name)
{
    if ( buffer == NULL || capacity == 0 ) {
        return;
    }
    buffer[0] = '\0';
    xl6CgenAppendText(buffer, capacity, "XL_Class_");
    xl6CgenAppendMangledText(buffer, capacity, name);
}

static void xl6CgenUnionTypeTextName(char* buffer, size_t capacity, const char* name)
{
    if ( buffer == NULL || capacity == 0 ) {
        return;
    }
    buffer[0] = '\0';
    xl6CgenAppendText(buffer, capacity, "XL_Union_");
    xl6CgenAppendMangledText(buffer, capacity, name);
}

static void xl6CgenEnumTypeRefName(
    char* buffer,
    size_t capacity,
    const XL6_AstNode* typeNode)
{
    const XL6_AstNode* args;
    size_t i;

    if ( buffer == NULL || capacity == 0 ) {
        return;
    }
    buffer[0] = '\0';
    xl6CgenAppendText(buffer, capacity, "XL_Enum_");
    xl6CgenAppendMangledText(
        buffer,
        capacity,
        typeNode != NULL ? typeNode->text : "_");
    args = typeNode != NULL && typeNode->childCount > 0 &&
           typeNode->children[0] != NULL &&
           typeNode->children[0]->kind == XL6_AST_LIST ?
        typeNode->children[0] :
        NULL;
    for ( i = 0; args != NULL && i < args->childCount; ++i ) {
        xl6CgenAppendText(buffer, capacity, "__");
        xl6CgenAppendTypeId(buffer, capacity, args->children[i]);
    }
}

static void xl6CgenClassTypeRefName(char* buffer, size_t capacity, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* args;
    size_t i;

    if ( buffer == NULL || capacity == 0 ) {
        return;
    }
    xl6CgenClassTypeTextName(buffer, capacity, typeNode != NULL ? typeNode->text : "_");
    args = typeNode != NULL && typeNode->childCount > 0 && typeNode->children[0] != NULL &&
           typeNode->children[0]->kind == XL6_AST_LIST ? typeNode->children[0] : NULL;
    if ( args == NULL || args->childCount == 0 ) {
        return;
    }
    for ( i = 0; i < args->childCount; ++i ) {
        xl6CgenAppendText(buffer, capacity, "__");
        xl6CgenAppendTypeId(buffer, capacity, args->children[i]);
    }
}

static void xl6CgenClassMemberSymbol(XL6_Output* out, const char* className, const char* memberName)
{
    xl6OutputPuts("xl_class_", out);
    xl6CgenFputsEncodedText(out, className);
    xl6OutputPuts("__", out);
    xl6CgenFputsEncodedText(out, memberName);
}
