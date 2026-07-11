static void xl6CgenError(XL6_CgenContext* ctx, const XL6_AstNode* node, const char* message)
{
    ctx->ok = 0;
    if ( ctx->diags != NULL ) {
        xl6DiagAdd(ctx->diags, node != NULL ? node->line : 1, node != NULL ? node->column : 1, message);
    }
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

    reserved = xl6CgenIsCReservedIdent(text);
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

static int xl6CgenIsRuntimeHeaderExtern(const XL6_AstNode* node)
{
    return node != NULL &&
           node->kind == XL6_AST_EXTERN_FUNC &&
           (xl6CgenExternTextEqualsSymbol(node->text, "xl6_xrt_print") ||
            xl6CgenExternTextEqualsSymbol(node->text, "xrtHash64"));
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
