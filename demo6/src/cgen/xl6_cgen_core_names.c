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
    return 1;
}

static void xl6CgenFputsEncodedCodepoint(XL6_Output* out, unsigned int cp)
{
    if ( out == NULL ) {
        return;
    }
    if ( (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || (cp >= '0' && cp <= '9') ) {
        xl6OutputPutc((int)cp, out);
    } else if ( cp == '_' ) {
        xl6OutputPuts("__", out);
    } else if ( cp <= 0xFFFFU ) {
        xl6OutputPrintf(out, "_u%04X_", cp);
    } else {
        xl6OutputPrintf(out, "_u%06X_", cp);
    }
}

static void xl6CgenFputsEncodedText(XL6_Output* out, const char* text)
{
    const unsigned char* p;

    if ( text == NULL || text[0] == '\0' ) {
        xl6OutputPutc('_', out);
        return;
    }
    p = (const unsigned char*)text;
    while ( *p != '\0' ) {
        xl6CgenFputsEncodedCodepoint(out, xl6CgenNextUtf8Codepoint(&p));
    }
}

static void xl6CgenAppendText(char* buffer, size_t capacity, const char* text);

static void xl6CgenFputsCIdentCodepoint(XL6_Output* out, unsigned int cp, int isFirst)
{
    if ( out == NULL ) {
        return;
    }
    if ( (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || cp == '_' ||
         (!isFirst && cp >= '0' && cp <= '9') ) {
        xl6OutputPutc((int)cp, out);
    } else if ( cp <= 0xFFFFU ) {
        xl6OutputPrintf(out, "_u%04X_", cp);
    } else {
        xl6OutputPrintf(out, "_u%06X_", cp);
    }
}

static void xl6CgenFputsCIdent(XL6_Output* out, const char* text)
{
    const unsigned char* p;
    int isFirst;

    if ( text == NULL || text[0] == '\0' ) {
        xl6OutputPutc('_', out);
        return;
    }
    p = (const unsigned char*)text;
    isFirst = 1;
    while ( *p != '\0' ) {
        xl6CgenFputsCIdentCodepoint(out, xl6CgenNextUtf8Codepoint(&p), isFirst);
        isFirst = 0;
    }
}

static void xl6CgenAppendCIdentText(char* buffer, size_t capacity, const char* text)
{
    const unsigned char* p;
    char tmp[32];
    int isFirst;

    if ( text == NULL || text[0] == '\0' ) {
        xl6CgenAppendText(buffer, capacity, "_");
        return;
    }
    p = (const unsigned char*)text;
    isFirst = 1;
    while ( *p != '\0' ) {
        unsigned int cp = xl6CgenNextUtf8Codepoint(&p);
        if ( (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || cp == '_' ||
             (!isFirst && cp >= '0' && cp <= '9') ) {
            tmp[0] = (char)cp;
            tmp[1] = '\0';
        } else if ( cp <= 0xFFFFU ) {
            snprintf(tmp, sizeof(tmp), "_u%04X_", cp);
        } else {
            snprintf(tmp, sizeof(tmp), "_u%06X_", cp);
        }
        xl6CgenAppendText(buffer, capacity, tmp);
        isFirst = 0;
    }
}

static void xl6CgenCIdentText(char* buffer, size_t capacity, const char* text)
{
    if ( buffer == NULL || capacity == 0 ) {
        return;
    }
    buffer[0] = '\0';
    xl6CgenAppendCIdentText(buffer, capacity, text);
}

static void xl6CgenCustomTypeName(XL6_Output* out, const char* name)
{
    xl6OutputPuts("XL_Custom_", out);
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
    const unsigned char* p;
    char tmp[32];

    if ( text == NULL || text[0] == '\0' ) {
        xl6CgenAppendText(buffer, capacity, "_");
        return;
    }
    p = (const unsigned char*)text;
    while ( *p != '\0' ) {
        unsigned int cp = xl6CgenNextUtf8Codepoint(&p);
        if ( (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || (cp >= '0' && cp <= '9') ) {
            tmp[0] = (char)cp;
            tmp[1] = '\0';
        } else if ( cp == '_' ) {
            tmp[0] = '_';
            tmp[1] = '_';
            tmp[2] = '\0';
        } else if ( cp <= 0xFFFFU ) {
            snprintf(tmp, sizeof(tmp), "_u%04X_", cp);
        } else {
            snprintf(tmp, sizeof(tmp), "_u%06X_", cp);
        }
        xl6CgenAppendText(buffer, capacity, tmp);
    }
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

static void xl6CgenCustomTypeRefName(char* buffer, size_t capacity, const char* name)
{
    if ( buffer == NULL || capacity == 0 ) {
        return;
    }
    buffer[0] = '\0';
    xl6CgenAppendText(buffer, capacity, "XL_Custom_");
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
