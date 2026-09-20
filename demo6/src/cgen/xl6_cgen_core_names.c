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

static unsigned int xl6CgenNextUtf8Codepoint(const unsigned char** p)
{
    const unsigned char* s;
    unsigned int cp;

    if ( p == NULL || *p == NULL || **p == '\0' ) {
        return 0;
    }

    s = *p;
    if ( s[0] < 0x80 ) {
        *p = s + 1;
        return s[0];
    }
    if ( (s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80 ) {
        cp = ((unsigned int)(s[0] & 0x1F) << 6) | (unsigned int)(s[1] & 0x3F);
        *p = s + 2;
        return cp;
    }
    if ( (s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 ) {
        cp = ((unsigned int)(s[0] & 0x0F) << 12) |
             ((unsigned int)(s[1] & 0x3F) << 6) |
             (unsigned int)(s[2] & 0x3F);
        *p = s + 3;
        return cp;
    }
    if ( (s[0] & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 &&
         (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80 ) {
        cp = ((unsigned int)(s[0] & 0x07) << 18) |
             ((unsigned int)(s[1] & 0x3F) << 12) |
             ((unsigned int)(s[2] & 0x3F) << 6) |
             (unsigned int)(s[3] & 0x3F);
        *p = s + 4;
        return cp;
    }

    *p = s + 1;
    return s[0];
}

static int xl6CgenIsCReservedIdent(const char* text)
{
    static const char* const names[] = {
        "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic", "_Imaginary",
        "_Noreturn", "_Static_assert", "_Thread_local",
        "auto", "break", "case", "char", "const", "continue", "default", "do", "double",
        "else", "enum", "extern", "float", "for", "goto", "if", "inline", "int", "long",
        "register", "restrict", "return", "short", "signed", "sizeof", "static", "struct",
        "switch", "typedef", "union", "unsigned", "void", "volatile", "while",
        /*
            TCC/GNU/Windows 扩展关键字和历史调用约定宏。
            xlang 允许它们作为普通标识符，生成 C 时必须统一转义。
        */
        "asm", "__asm", "__asm__",
        "__const", "__const__", "__volatile", "__volatile__", "__signed", "__signed__",
        "__inline", "__inline__", "__restrict", "__restrict__", "__extension__",
        "__thread", "__attribute", "__attribute__", "__alignof", "__alignof__",
        "typeof", "__typeof", "__typeof__", "__label__",
        "cdecl", "__cdecl", "__cdecl__", "stdcall", "__stdcall", "__stdcall__",
        "fastcall", "__fastcall", "__fastcall__", "thiscall", "__thiscall", "__thiscall__",
        "regparm", "__regparm__", "pascal", "near", "far", "huge", "interrupt",
        "based", "segment", "declspec", "__declspec",
        /* TCC/CRT 常以宏暴露这些名字，不能让宏改写生成的局部标识符。 */
        "errno", "stdin", "stdout", "stderr"
    };
    size_t i;

    if ( text == NULL ) {
        return 0;
    }
    for ( i = 0; i < sizeof(names) / sizeof(names[0]); ++i ) {
        if ( strcmp(text, names[i]) == 0 ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenLooksLikeCMacroIdent(const char* text)
{
    const unsigned char* p;
    int hasUpper;

    if ( text == NULL || text[0] == '\0' ) {
        return 0;
    }

    /*
        C/CRT 的对象宏通常使用全大写名字。这里统一编码这类用户标识符，
        避免 NULL、EOF、SIZE_MAX 等宏在预处理阶段改写局部变量、参数和字段。
    */
    hasUpper = 0;
    for ( p = (const unsigned char*)text; *p != '\0'; ++p ) {
        if ( *p >= 'A' && *p <= 'Z' ) {
            hasUpper = 1;
            continue;
        }
        if ( (*p >= 'a' && *p <= 'z') ||
             ((*p < '0' || *p > '9') && *p != '_') ) {
            return 0;
        }
    }
    return hasUpper;
}

static int xl6CgenIsSimpleCIdent(const char* text)
{
    const unsigned char* p;

    if ( text == NULL || text[0] == '\0' ) {
        return 0;
    }

    p = (const unsigned char*)text;
    if ( !((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_') ) {
        return 0;
    }
    for ( ++p; *p != '\0'; ++p ) {
        if ( !((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
               (*p >= '0' && *p <= '9') || *p == '_') ) {
            return 0;
        }
    }
    return !xl6CgenIsCReservedIdent(text);
}

#define XL6_CGEN_C_IDENT_MAX 48u
#define XL6_CGEN_C_IDENT_PREFIX 28u

static unsigned long long xl6CgenIdentHash(const char* text)
{
    const unsigned char* p = (const unsigned char*)(text != NULL ? text : "");
    unsigned long long hash = 1469598103934665603ULL;

    while ( *p != '\0' ) {
        hash ^= (unsigned long long)*p++;
        hash *= 1099511628211ULL;
    }
    return hash;
}

static void xl6CgenEncodedCodepointText(char text[32], unsigned int cp)
{
    if ( text == NULL ) {
        return;
    }
    if ( (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || (cp >= '0' && cp <= '9') ) {
        text[0] = (char)cp;
        text[1] = '\0';
    } else if ( cp == '_' ) {
        snprintf(text, 32, "__");
    } else if ( cp <= 0xFFFFU ) {
        snprintf(text, 32, "_u%04X_", cp);
    } else {
        snprintf(text, 32, "_u%06X_", cp);
    }
}

void xl6CgenEncodeSymbolComponent(char* out, size_t capacity, const char* text)
{
    const unsigned char* p;
    size_t encodedSize;
    size_t prefixSize;
    char token[32];
    char prefix[XL6_CGEN_C_IDENT_PREFIX + 1];

    if ( out == NULL || capacity == 0 ) {
        return;
    }
    out[0] = '\0';
    if ( text == NULL || text[0] == '\0' ) {
        snprintf(out, capacity, "_");
        return;
    }
    encodedSize = 0;
    prefixSize = 0;
    prefix[0] = '\0';
    p = (const unsigned char*)text;
    while ( *p != '\0' ) {
        size_t tokenSize;

        xl6CgenEncodedCodepointText(token, xl6CgenNextUtf8Codepoint(&p));
        tokenSize = strlen(token);
        encodedSize += tokenSize;
        if ( prefixSize + tokenSize <= XL6_CGEN_C_IDENT_PREFIX ) {
            memcpy(prefix + prefixSize, token, tokenSize + 1);
            prefixSize += tokenSize;
        }
    }
    if ( encodedSize <= XL6_CGEN_C_IDENT_MAX ) {
        p = (const unsigned char*)text;
        while ( *p != '\0' ) {
            size_t used;
            size_t tokenSize;

            xl6CgenEncodedCodepointText(token, xl6CgenNextUtf8Codepoint(&p));
            used = strlen(out);
            tokenSize = strlen(token);
            if ( used + tokenSize + 1 > capacity ) {
                break;
            }
            memcpy(out + used, token, tokenSize + 1);
        }
        return;
    }
    snprintf(out,
             capacity,
             "%s_h%016llX",
             prefix,
             (unsigned long long)xl6CgenIdentHash(text));
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

static void xl6CgenCIdentCodepointText(char text[32], unsigned int cp, int isFirst)
{
    if ( text == NULL ) {
        return;
    }
    if ( (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || cp == '_' ||
         (!isFirst && cp >= '0' && cp <= '9') ) {
        text[0] = (char)cp;
        text[1] = '\0';
    } else if ( cp <= 0xFFFFU ) {
        snprintf(text, 32, "_u%04X_", cp);
    } else {
        snprintf(text, 32, "_u%06X_", cp);
    }
}

void xl6CgenEncodeCIdentifier(char* out, size_t capacity, const char* text)
{
    const unsigned char* p;
    size_t encodedSize;
    size_t prefixSize;
    int isFirst;
    int reserved;
    char token[32];
    char prefix[XL6_CGEN_C_IDENT_PREFIX + 1];

    if ( out == NULL || capacity == 0 ) {
        return;
    }
    out[0] = '\0';
    if ( text == NULL || text[0] == '\0' ) {
        snprintf(out, capacity, "_");
        return;
    }

    reserved = xl6CgenIsCReservedIdent(text) || xl6CgenLooksLikeCMacroIdent(text);
    encodedSize = reserved ? strlen("xl_kw_") : 0;
    prefix[0] = '\0';
    prefixSize = 0;
    if ( reserved ) {
        snprintf(prefix, sizeof(prefix), "xl_kw_");
        prefixSize = strlen(prefix);
    }
    p = (const unsigned char*)text;
    isFirst = 1;
    while ( *p != '\0' ) {
        size_t tokenSize;

        xl6CgenCIdentCodepointText(token, xl6CgenNextUtf8Codepoint(&p), isFirst);
        tokenSize = strlen(token);
        encodedSize += tokenSize;
        if ( prefixSize + tokenSize <= XL6_CGEN_C_IDENT_PREFIX ) {
            memcpy(prefix + prefixSize, token, tokenSize + 1);
            prefixSize += tokenSize;
        }
        isFirst = 0;
    }

    if ( encodedSize <= XL6_CGEN_C_IDENT_MAX ) {
        if ( reserved ) {
            xl6CgenAppendText(out, capacity, "xl_kw_");
        }
        p = (const unsigned char*)text;
        isFirst = 1;
        while ( *p != '\0' ) {
            xl6CgenCIdentCodepointText(token, xl6CgenNextUtf8Codepoint(&p), isFirst);
            xl6CgenAppendText(out, capacity, token);
            isFirst = 0;
        }
        return;
    }
    snprintf(out,
             capacity,
             "%s_h%016llX",
             prefix,
             (unsigned long long)xl6CgenIdentHash(text));
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

static void xl6CgenUnionTypeName(XL6_Output* out, const char* name)
{
    xl6OutputPuts("XL_Union_", out);
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
