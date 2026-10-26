%name xl6Parse
%token_prefix XL6PG_
%token_type { XL6_Token }
%default_type { XL6_AstNode* }
%extra_argument { XL6_ParseContext* ctx }
%type assign_op { XL6_Token }
%type name_tok { XL6_Token }
%type import_name_part { XL6_Token }
%type member_name_tok { XL6_Token }
%type import_name { XL6_AstNode* }
%type import_target { XL6_AstNode* }
%type doc_block { XL6_Token }
%type class_member_mods { unsigned int }
%type class_member_modifier { unsigned int }
%type param_mode { unsigned int }
%type extern_func_contract { unsigned int }
%type extern_func_contract_item { unsigned int }
%type field_attr_arg { XL6_AstNode* }
%type field_attr_word { XL6_Token }

%include {
#include "src/parser/xl6_parser_api.h"
#include "src/lexer/xl6_literal_span.h"

static XL6_AstNode* xl6PList(void)
{
    return xl6AstNew(XL6_AST_LIST, NULL);
}

static XL6_AstNode* xl6PBlockWith(XL6_AstNode* stmt)
{
    XL6_AstNode* block = xl6AstNew(XL6_AST_BLOCK, NULL);
    xl6AstAddChild(block, stmt);
    return block;
}

static XL6_AstNode* xl6PReturnBlock(const XL6_Token* token, XL6_AstNode* expr)
{
    XL6_AstNode* stmt = xl6AstNew(XL6_AST_RETURN, token);
    xl6AstAddChild(stmt, expr);
    return xl6PBlockWith(stmt);
}

static void xl6PDiag(XL6_ParseContext* ctx, const XL6_Token* token, const char* msg)
{
    if ( ctx != NULL ) {
        ctx->hadError = 1;
        if ( ctx->diags != NULL ) {
            xl6DiagAdd(ctx->diags, token != NULL ? token->startLine : 1, token != NULL ? token->startColumn : 1, msg);
        }
    }
}

typedef struct XL6_PTextBuilder {
    char* data;
    size_t len;
    size_t cap;
} XL6_PTextBuilder;

static void xl6PTextBuilderInit(XL6_PTextBuilder* builder)
{
    memset(builder, 0, sizeof(*builder));
}

static void xl6PTextBuilderUnit(XL6_PTextBuilder* builder)
{
    if ( builder == NULL ) {
        return;
    }
    xrtFree(builder->data);
    memset(builder, 0, sizeof(*builder));
}

static int xl6PTextBuilderGrow(XL6_PTextBuilder* builder, size_t addLen)
{
    char* newData;
    size_t need;
    size_t newCap;

    if ( builder == NULL ) {
        return 0;
    }
    need = builder->len + addLen + 1;
    if ( need <= builder->cap ) {
        return 1;
    }
    newCap = builder->cap != 0 ? builder->cap : 64;
    while ( newCap < need ) {
        newCap *= 2;
    }
    newData = (char*)xrtRealloc(builder->data, newCap);
    if ( newData == NULL ) {
        return 0;
    }
    builder->data = newData;
    builder->cap = newCap;
    return 1;
}

static int xl6PTextBuilderAppendRange(XL6_PTextBuilder* builder, const char* text, size_t len)
{
    if ( len == 0 ) {
        return 1;
    }
    if ( text == NULL || !xl6PTextBuilderGrow(builder, len) ) {
        return 0;
    }
    memcpy(builder->data + builder->len, text, len);
    builder->len += len;
    builder->data[builder->len] = '\0';
    return 1;
}

static int xl6PTextBuilderAppendChar(XL6_PTextBuilder* builder, char ch)
{
    return xl6PTextBuilderAppendRange(builder, &ch, 1);
}

static int xl6PLiteralHexDigit(char ch)
{
    if ( ch >= '0' && ch <= '9' ) return ch - '0';
    if ( ch >= 'a' && ch <= 'f' ) return ch - 'a' + 10;
    if ( ch >= 'A' && ch <= 'F' ) return ch - 'A' + 10;
    return -1;
}

static int xl6PTextBuilderAppendScalar(XL6_PTextBuilder* builder, uint32 scalar)
{
    char encoded[4];
    size_t size = xrtUtf8Encode(scalar, encoded);

    return size != 0 && xl6PTextBuilderAppendRange(builder, encoded, size);
}

/* Triple strings strip one framing newline and the common indentation of
 * non-blank lines. The operation is byte-stable for UTF-8 because only ASCII
 * horizontal whitespace at line starts is removed. */
static void xl6PDedentTripleLiteral(XL6_PTextBuilder* builder)
{
    size_t start = 0;
    size_t minIndent = SIZE_MAX;
    size_t line;
    size_t read;
    size_t write;

    if ( builder == NULL || builder->len == 0 ) return;
    if ( builder->data[0] == '\n' ) start = 1;
    line = start;
    while ( line < builder->len ) {
        size_t at = line;
        size_t indent = 0;
        while ( at < builder->len &&
                (builder->data[at] == ' ' || builder->data[at] == '\t') ) {
            ++at;
            ++indent;
        }
        if ( at < builder->len && builder->data[at] != '\n' &&
             indent < minIndent ) {
            minIndent = indent;
        }
        while ( at < builder->len && builder->data[at] != '\n' ) ++at;
        line = at < builder->len ? at + 1 : builder->len;
    }
    if ( minIndent == SIZE_MAX ) minIndent = 0;
    read = start;
    write = 0;
    while ( read < builder->len ) {
        size_t removed = 0;
        while ( read < builder->len && removed < minIndent &&
                (builder->data[read] == ' ' || builder->data[read] == '\t') ) {
            ++read;
            ++removed;
        }
        while ( read < builder->len ) {
            char ch = builder->data[read++];
            builder->data[write++] = ch;
            if ( ch == '\n' ) break;
        }
    }
    /* A closing delimiter placed on its own indented line is framing, not
     * content. The preceding content newline remains stable. */
    if ( write != 0 ) {
        size_t tail = write;
        while ( tail != 0 && builder->data[tail - 1] != '\n' &&
                (builder->data[tail - 1] == ' ' ||
                 builder->data[tail - 1] == '\t') ) --tail;
        if ( tail == 0 || (tail != write && builder->data[tail - 1] == '\n') )
            write = tail;
    }
    builder->len = write;
    builder->data[write] = '\0';
}

static int xl6PDecodeLiteralBody(
    XL6_ParseContext* ctx,
    const XL6_Token* token,
    const char* text,
    size_t start,
    size_t end,
    int raw,
    XL6_PTextBuilder* builder)
{
    size_t i;

    for ( i = start; i < end; ++i ) {
        unsigned char ch = (unsigned char)text[i];
        if ( ch == '\r' ) {
            if ( i + 1 < end && text[i + 1] == '\n' ) ++i;
            if ( !xl6PTextBuilderAppendChar(builder, '\n') ) goto oom;
            continue;
        }
        if ( raw || ch != '\\' ) {
            if ( !xl6PTextBuilderAppendChar(builder, (char)ch) ) goto oom;
            continue;
        }
        if ( ++i >= end ) {
            xl6PDiag(ctx, token, "字面量末尾不能是单独的反斜杠");
            return 0;
        }
        ch = (unsigned char)text[i];
        switch ( ch ) {
        case '\\': case '\'': case '"': case '?':
            if ( !xl6PTextBuilderAppendChar(builder, (char)ch) ) goto oom;
            break;
        case '0':
            if ( !xl6PTextBuilderAppendChar(builder, '\0') ) goto oom;
            break;
        case 'a':
            if ( !xl6PTextBuilderAppendChar(builder, '\a') ) goto oom;
            break;
        case 'b':
            if ( !xl6PTextBuilderAppendChar(builder, '\b') ) goto oom;
            break;
        case 'f':
            if ( !xl6PTextBuilderAppendChar(builder, '\f') ) goto oom;
            break;
        case 'n':
            if ( !xl6PTextBuilderAppendChar(builder, '\n') ) goto oom;
            break;
        case 'r':
            if ( !xl6PTextBuilderAppendChar(builder, '\r') ) goto oom;
            break;
        case 't':
            if ( !xl6PTextBuilderAppendChar(builder, '\t') ) goto oom;
            break;
        case 'v':
            if ( !xl6PTextBuilderAppendChar(builder, '\v') ) goto oom;
            break;
        case 'x':
        {
            int high;
            int low;
            if ( i + 2 >= end ||
                 (high = xl6PLiteralHexDigit(text[i + 1])) < 0 ||
                 (low = xl6PLiteralHexDigit(text[i + 2])) < 0 ) {
                xl6PDiag(ctx, token, "\\x 转义必须恰好包含两个十六进制数字");
                return 0;
            }
            ch = (unsigned char)((high << 4) | low);
            i += 2;
            if ( !xl6PTextBuilderAppendChar(builder, (char)ch) ) goto oom;
            break;
        }
        case 'u':
        case 'U':
        {
            size_t digits = ch == 'u' ? 4 : 8;
            size_t digitIndex;
            uint32 scalar = 0;
            if ( i + digits >= end ) {
                xl6PDiag(ctx, token, "Unicode 转义长度不完整");
                return 0;
            }
            for ( digitIndex = 0; digitIndex < digits; ++digitIndex ) {
                int digit = xl6PLiteralHexDigit(text[i + 1 + digitIndex]);
                if ( digit < 0 || scalar > UINT32_MAX / 16u ) {
                    xl6PDiag(ctx, token, "Unicode 转义包含无效数字");
                    return 0;
                }
                scalar = scalar * 16u + (uint32)digit;
            }
            i += digits;
            if ( !xl6PTextBuilderAppendScalar(builder, scalar) ) {
                xl6PDiag(ctx, token, "Unicode 转义不是有效标量");
                return 0;
            }
            break;
        }
        default:
            xl6PDiag(ctx, token, "未知的字面量转义");
            return 0;
        }
    }
    return 1;

oom:
    xl6PDiag(ctx, token, "out of memory while decoding literal");
    return 0;
}

static XL6_AstNode* xl6PParseLiteral(
    XL6_ParseContext* ctx,
    const XL6_Token* token,
    XL6_AstKind kind)
{
    XL6_AstNode* node = xl6AstNew(kind, token);
    XL6_PTextBuilder builder;
    const char* text;
    size_t len;
    size_t pos = 0;
    size_t delimiter = 1;
    size_t start;
    size_t end;
    int raw = 0;
    int triple = 0;
    char quote;

    xl6PTextBuilderInit(&builder);
    if ( node == NULL || token == NULL || token->text == NULL ) {
        xl6PDiag(ctx, token, "invalid literal");
        return node;
    }
    text = token->text;
    len = token->len;
    if ( kind == XL6_AST_BYTES ) {
        if ( len < 3 || text[pos++] != 'b' ) goto invalid;
        if ( pos < len && text[pos] == 'r' ) {
            raw = 1;
            ++pos;
        }
    } else if ( kind == XL6_AST_STRING && pos < len && text[pos] == 'r' ) {
        raw = 1;
        ++pos;
    }
    if ( pos >= len ) goto invalid;
    quote = text[pos];
    if ( kind == XL6_AST_CHAR ) {
        if ( quote != '\'' ) goto invalid;
    } else if ( quote == (char)0x60 ) {
        raw = 1;
    } else if ( quote != '"' ) {
        goto invalid;
    }
    if ( quote == '"' && pos + 2 < len &&
         text[pos + 1] == quote && text[pos + 2] == quote ) {
        delimiter = 3;
        triple = 1;
    }
    if ( len < pos + delimiter * 2 ) goto invalid;
    start = pos + delimiter;
    end = len - delimiter;
    if ( end < start ) goto invalid;
    if ( delimiter == 3 ) {
        if ( text[len - 1] != quote || text[len - 2] != quote ||
             text[len - 3] != quote ) goto invalid;
    } else if ( text[len - 1] != quote ) {
        goto invalid;
    }
    if ( !xl6PDecodeLiteralBody(
            ctx, token, text, start, end, raw, &builder) ) goto done;
    if ( triple ) xl6PDedentTripleLiteral(&builder);
    if ( kind != XL6_AST_BYTES &&
         !xrtUtf8Valid((xstrview){ builder.data, builder.len }, NULL) ) {
        xl6PDiag(ctx, token, "字符串和字符字面量必须是有效 UTF-8");
        goto done;
    }
    if ( kind == XL6_AST_CHAR ) {
        size_t read = 0;
        uint32 scalar = 0;
        if ( xrtUtf8Decode(
                (xstrview){ builder.data, builder.len },
                &scalar,
                &read
            ) != XUTF_OK || read != builder.len ) {
            xl6PDiag(ctx, token, "字符字面量必须恰好包含一个 Unicode 标量");
            goto done;
        }
        node->literalScalar = scalar;
    }
    if ( !xl6AstSetLiteralData(node, builder.data, builder.len) ) {
        xl6PDiag(ctx, token, "out of memory while storing literal payload");
    }
    goto done;

invalid:
    xl6PDiag(ctx, token, "invalid literal spelling");
done:
    xl6PTextBuilderUnit(&builder);
    return node;
}

static XL6_AstNode* xl6PImportNameFromToken(const XL6_Token* token)
{
    return xl6AstNew(XL6_AST_NAME, token);
}

static XL6_AstNode* xl6PDottedNameAppendRange(
    XL6_ParseContext* ctx,
    XL6_AstNode* node,
    const char* suffix,
    size_t suffixLen,
    const XL6_Token* token)
{
    char* text;
    size_t leftLen;

    if ( node == NULL || suffix == NULL ) {
        xl6PDiag(ctx, token, "invalid dotted name");
        return node;
    }
    leftLen = node->text != NULL ? strlen(node->text) : 0;
    text = (char*)xrtMalloc(leftLen + suffixLen + 2);
    if ( text == NULL ) {
        xl6PDiag(ctx, token, "out of memory while parsing dotted name");
        return node;
    }
    if ( leftLen > 0 ) {
        memcpy(text, node->text, leftLen);
    }
    text[leftLen] = '.';
    if ( suffixLen > 0 ) {
        memcpy(text + leftLen + 1, suffix, suffixLen);
    }
    text[leftLen + 1 + suffixLen] = '\0';
    xrtFree(node->text);
    node->text = text;
    return node;
}

static XL6_AstNode* xl6PDottedNameAppend(XL6_ParseContext* ctx, XL6_AstNode* node, const XL6_Token* token)
{
    return xl6PDottedNameAppendRange(
        ctx,
        node,
        token != NULL ? token->text : NULL,
        token != NULL ? token->len : 0,
        token);
}

static XL6_AstNode* xl6PQualifiedType(
    XL6_ParseContext* ctx,
    const XL6_Token* first,
    XL6_AstNode* rest)
{
    XL6_AstNode* node = xl6AstNew(XL6_AST_TYPE, first);

    if ( rest != NULL && rest->text != NULL ) {
        node = xl6PDottedNameAppendRange(ctx, node, rest->text, strlen(rest->text), first);
    }
    xl6AstFree(rest);
    return node;
}

static XL6_AstNode* xl6PNullableType(
    XL6_ParseContext* ctx,
    XL6_AstNode* inner,
    const XL6_Token* token)
{
    XL6_AstNode* node;
    XL6_AstNode* args;

    node = xl6AstNewText(XL6_AST_TYPE, "nullable", 8, token);
    args = xl6PList();
    if ( node == NULL || args == NULL ) {
        xl6AstFree(node);
        xl6AstFree(args);
        xl6AstFree(inner);
        xl6PDiag(ctx, token, "out of memory while parsing nullable type");
        return NULL;
    }
    node->flags |= XL6_AST_FLAG_INTERNAL_NULLABLE;
    xl6AstAddChild(args, inner);
    xl6AstAddChild(node, args);
    return node;
}

static int xl6PFlushFStringText(
    XL6_ParseContext* ctx,
    XL6_AstNode* node,
    XL6_PTextBuilder* builder,
    const XL6_Token* token,
    int raw)
{
    XL6_PTextBuilder decoded;
    XL6_AstNode* part;
    size_t childCount;

    if ( builder == NULL || builder->len == 0 ) {
        return 1;
    }
    xl6PTextBuilderInit(&decoded);
    if ( !xl6PDecodeLiteralBody(
            ctx, token, builder->data, 0, builder->len, raw, &decoded) ) {
        xl6PTextBuilderUnit(&decoded);
        return 0;
    }
    if ( !xrtUtf8Valid((xstrview){ decoded.data, decoded.len }, NULL) ) {
        xl6PDiag(ctx, token, "f-string 文本必须是有效 UTF-8");
        xl6PTextBuilderUnit(&decoded);
        return 0;
    }
    part = xl6AstNewText(
        XL6_AST_INTERP_TEXT, builder->data, builder->len, token);
    if ( part == NULL ||
         !xl6AstSetLiteralData(part, decoded.data, decoded.len) ) {
        xl6AstFree(part);
        xl6PDiag(ctx, token, "out of memory while storing f-string text");
        xl6PTextBuilderUnit(&decoded);
        return 0;
    }
    childCount = node->childCount;
    xl6AstAddChild(node, part);
    if ( node->childCount == childCount ) {
        xl6AstFree(part);
        xl6PDiag(ctx, token, "out of memory while appending f-string text");
        xl6PTextBuilderUnit(&decoded);
        return 0;
    }
    xl6PTextBuilderUnit(&decoded);
    builder->len = 0;
    if ( builder->data != NULL ) {
        builder->data[0] = '\0';
    }
    return 1;
}

/* Triple interpolated strings use the same framing rule as ordinary triple
 * strings.  Literal children already contain decoded UTF-8; expression
 * children count as non-whitespace content but retain their original source
 * positions and runtime value unchanged. */
static void xl6PDedentInterpolatedString(XL6_AstNode* node)
{
    size_t childIndex;
    size_t minIndent = SIZE_MAX;
    size_t indent = 0;
    size_t removed = 0;
    int lineStart = 1;
    XL6_AstNode* child;

    if ( node == NULL || node->childCount == 0 ) return;
    child = node->children[0];
    if ( child != NULL && child->kind == XL6_AST_INTERP_TEXT &&
         child->literalDecoded && child->literalSize != 0 &&
         child->literalData[0] == '\n' ) {
        memmove(child->literalData, child->literalData + 1,
                child->literalSize);
        --child->literalSize;
    }
    for ( childIndex = 0; childIndex < node->childCount; ++childIndex ) {
        size_t offset;
        child = node->children[childIndex];
        if ( child == NULL || child->kind != XL6_AST_INTERP_TEXT ) {
            if ( lineStart ) {
                if ( indent < minIndent ) minIndent = indent;
                lineStart = 0;
            }
            continue;
        }
        for ( offset = 0; offset < child->literalSize; ++offset ) {
            unsigned char ch = child->literalData[offset];
            if ( lineStart ) {
                if ( ch == ' ' || ch == '\t' ) {
                    ++indent;
                    continue;
                }
                if ( ch == '\n' ) {
                    indent = 0;
                    continue;
                }
                if ( indent < minIndent ) minIndent = indent;
                lineStart = 0;
            } else if ( ch == '\n' ) {
                lineStart = 1;
                indent = 0;
            }
        }
    }
    if ( minIndent == SIZE_MAX ) minIndent = 0;
    lineStart = 1;
    removed = 0;
    for ( childIndex = 0; childIndex < node->childCount; ++childIndex ) {
        size_t read;
        size_t write = 0;
        child = node->children[childIndex];
        if ( child == NULL || child->kind != XL6_AST_INTERP_TEXT ) {
            lineStart = 0;
            continue;
        }
        for ( read = 0; read < child->literalSize; ++read ) {
            unsigned char ch = child->literalData[read];
            if ( lineStart && removed < minIndent &&
                 (ch == ' ' || ch == '\t') ) {
                ++removed;
                continue;
            }
            child->literalData[write++] = ch;
            if ( ch == '\n' ) {
                lineStart = 1;
                removed = 0;
            } else if ( lineStart ) {
                lineStart = 0;
            }
        }
        child->literalSize = write;
        child->literalData[write] = 0;
    }
    child = node->children[node->childCount - 1];
    if ( child != NULL && child->kind == XL6_AST_INTERP_TEXT &&
         child->literalDecoded ) {
        size_t tail = child->literalSize;
        while ( tail != 0 && child->literalData[tail - 1] != '\n' &&
                (child->literalData[tail - 1] == ' ' ||
                 child->literalData[tail - 1] == '\t') ) --tail;
        if ( tail == 0 ||
             (tail != child->literalSize &&
              child->literalData[tail - 1] == '\n') ) {
            child->literalSize = tail;
            child->literalData[tail] = 0;
        }
    }
}

static int xl6PIsFStringSpace(char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\f' || ch == '\v';
}

static int xl6PFStringExprIsEmpty(const char* text, size_t len)
{
    size_t i;

    if ( text == NULL ) {
        return 1;
    }
    for ( i = 0; i < len; ++i ) {
        if ( !xl6PIsFStringSpace(text[i]) ) {
            return 0;
        }
    }
    return 1;
}

static size_t xl6PFStringFindExprEnd(const char* text, size_t len, size_t pos)
{
    XL6_LiteralSpan span = xl6ScanLiteralSpan(text, len, pos, 1);
    return span.error == NULL ? span.end - 1 : len;
}

static XL6_AstNode* xl6PDetachParsedFStringExpr(XL6_ParseResult* result)
{
    XL6_AstNode* root;
    XL6_AstNode* stmt;
    XL6_AstNode* expr;

    if ( result == NULL || result->root == NULL || result->root->childCount != 1 ) {
        return NULL;
    }
    root = result->root;
    stmt = root->children[0];
    if ( stmt == NULL || stmt->kind != XL6_AST_VAR_DECL || stmt->childCount < 2 ) {
        return NULL;
    }
    expr = stmt->children[1];
    stmt->children[1] = NULL;
    return expr;
}

static void xl6PFStringAdvancePosition(const char* text, size_t len, int* line, int* column)
{
    size_t i;

    if ( text == NULL || line == NULL || column == NULL ) {
        return;
    }
    for ( i = 0; i < len; ++i ) {
        unsigned char ch = (unsigned char)text[i];

        if ( ch == '\r' ) {
            if ( i + 1 < len && text[i + 1] == '\n' ) {
                ++i;
            }
            *line += 1;
            *column = 1;
        } else if ( ch == '\n' ) {
            *line += 1;
            *column = 1;
        } else if ( (ch & 0xC0u) != 0x80u ) {
            *column += 1;
        }
    }
}

static void xl6PRebaseFStringRange(
    XL6_AstRange* range, const XL6_Token* token, const char* text,
    size_t exprOffset, size_t exprLength, size_t prefixLength, int clip)
{
    size_t start, end;
    if (!range->known) return;
    start = range->startOffset;
    end = range->endOffset;
    if (!token || !text || end < prefixLength || start > prefixLength + exprLength ||
        (!clip && (start < prefixLength || end > prefixLength + exprLength))) {
        memset(range, 0, sizeof(*range));
        return;
    }
    /* The wrapper's parentheses may widen the expression's grammar span.
     * Clip only that known synthetic syntax, never fabricate token spans. */
    if (start < prefixLength) start = prefixLength;
    if (end > prefixLength + exprLength) end = prefixLength + exprLength;
    start = exprOffset + start - prefixLength;
    end = exprOffset + end - prefixLength;
    range->startOffset = token->startOffset + start;
    range->endOffset = token->startOffset + end;
    range->startLine = range->endLine = token->startLine;
    range->startColumn = range->endColumn = token->startColumn;
    xl6PFStringAdvancePosition(text, start, &range->startLine, &range->startColumn);
    xl6PFStringAdvancePosition(text, end, &range->endLine, &range->endColumn);
}

static void xl6PRebaseFStringExprPosition(
    XL6_AstNode* node,
    int exprLine,
    int exprColumn,
    int syntheticPrefixColumns,
    const XL6_Token* token, const char* text, size_t exprOffset, size_t exprLength)
{
    size_t i;

    if ( node == NULL ) {
        return;
    }
    if ( node->line > 0 ) {
        if ( node->line == 1 ) {
            node->line = exprLine;
            node->column = exprColumn + node->column - syntheticPrefixColumns - 1;
        } else {
            node->line = exprLine + node->line - 1;
        }
    }
    xl6PRebaseFStringRange(&node->range, token, text, exprOffset, exprLength,
                          (size_t)syntheticPrefixColumns, 1);
    xl6PRebaseFStringRange(&node->tokenRange, token, text, exprOffset, exprLength,
                          (size_t)syntheticPrefixColumns, 0);
    for ( i = 0; i < node->childCount; ++i ) {
        xl6PRebaseFStringExprPosition(node->children[i], exprLine, exprColumn,
                                    syntheticPrefixColumns, token, text, exprOffset, exprLength);
    }
}

static XL6_AstNode* xl6PParseFStringExpr(
    XL6_ParseContext* ctx,
    const XL6_Token* token,
    const char* tokenText,
    size_t exprOffset,
    size_t len)
{
    static const char prefix[] = "auto __xl6_fstr_expr = (";
    static const char suffix[] = ")\n";
    XL6_ParseResult result;
    XL6_AstNode* expr;
    const char* text;
    char* source;
    size_t sourceLen;
    int exprLine;
    int exprColumn;
    int ok;

    text = tokenText != NULL ? tokenText + exprOffset : NULL;

    if ( xl6PFStringExprIsEmpty(text, len) ) {
        xl6PDiag(ctx, token, "f-string 插值表达式不能为空");
        return xl6AstNewText(XL6_AST_STRING, "", 0, token);
    }

    sourceLen = (sizeof(prefix) - 1) + len + (sizeof(suffix) - 1);
    source = (char*)xrtMalloc(sourceLen + 1);
    if ( source == NULL ) {
        xl6PDiag(ctx, token, "out of memory while parsing f-string interpolation expression");
        return xl6AstNewText(XL6_AST_STRING, "", 0, token);
    }
    memcpy(source, prefix, sizeof(prefix) - 1);
    memcpy(source + sizeof(prefix) - 1, text, len);
    memcpy(source + sizeof(prefix) - 1 + len, suffix, sizeof(suffix));

    xl6ParseResultInit(&result);
    ok = xl6ParseSource(source, "<f-string>", &result);
    xrtFree(source);
    if ( !ok ) {
        xl6ParseResultFree(&result);
        xl6PDiag(ctx, token, "invalid f-string interpolation expression");
        return xl6AstNewText(XL6_AST_STRING, "", 0, token);
    }

    expr = xl6PDetachParsedFStringExpr(&result);
    xl6ParseResultFree(&result);
    if ( expr == NULL ) {
        xl6PDiag(ctx, token, "invalid f-string interpolation expression");
        return xl6AstNewText(XL6_AST_STRING, "", 0, token);
    }
    exprLine = token != NULL ? token->startLine : 0;
    exprColumn = token != NULL ? token->startColumn : 0;
    xl6PFStringAdvancePosition(tokenText, exprOffset, &exprLine, &exprColumn);
    xl6PRebaseFStringExprPosition(expr, exprLine, exprColumn, (int)(sizeof(prefix) - 1),
                                token, tokenText, exprOffset, len);
    return expr;
}

static XL6_AstNode* xl6PParseFString(XL6_ParseContext* ctx, const XL6_Token* token)
{
    XL6_AstNode* node;
    XL6_PTextBuilder builder;
    const char* text;
    size_t len;
    size_t prefixLen;
    size_t quoteLen;
    size_t start;
    size_t end;
    size_t i;
    size_t exprEnd;
    char quote;
    int raw;
    int failed = 0;

    node = xl6AstNew(XL6_AST_INTERP_STRING, token);
    if ( token == NULL || token->text == NULL || token->len < 3 ) {
        xl6PDiag(ctx, token, "invalid f-string literal");
        return node;
    }

    text = token->text;
    len = token->len;
    prefixLen = (len >= 2 && text[0] == 'r' && text[1] == 'f') ? 2 : 1;
    if ( prefixLen >= len ||
         (text[prefixLen] != '"' && text[prefixLen] != (char)0x60) ) {
        xl6PDiag(ctx, token, "invalid f-string literal");
        return node;
    }
    quote = text[prefixLen];
    raw = prefixLen == 2 || quote == (char)0x60;
    quoteLen = (quote == '"' && prefixLen + 2 < len &&
                text[prefixLen + 1] == quote &&
                text[prefixLen + 2] == quote) ? 3 : 1;
    if ( len < prefixLen + quoteLen * 2 ) {
        xl6PDiag(ctx, token, "invalid f-string literal");
        return node;
    }

    start = prefixLen + quoteLen;
    end = len - quoteLen;
    xl6PTextBuilderInit(&builder);
    for ( i = start; i < end; ++i ) {
        if ( text[i] == '{' ) {
            if ( i + 1 < end && text[i + 1] == '{' ) {
                if ( !xl6PTextBuilderAppendChar(&builder, '{') ) {
                    xl6PDiag(ctx, token, "out of memory while parsing f-string text");
                    break;
                }
                ++i;
                continue;
            }
            if ( !xl6PFlushFStringText(ctx, node, &builder, token, raw) ) {
                failed = 1;
                break;
            }
            exprEnd = xl6PFStringFindExprEnd(text, end, i + 1);
            if ( exprEnd >= end ) {
                xl6PDiag(ctx, token, "未闭合的 f-string 插值表达式");
                break;
            }
            xl6AstAddChild(node, xl6PParseFStringExpr(ctx, token, text, i + 1, exprEnd - i - 1));
            i = exprEnd;
            continue;
        }
        if ( text[i] == '}' ) {
            if ( i + 1 < end && text[i + 1] == '}' ) {
                if ( !xl6PTextBuilderAppendChar(&builder, '}') ) {
                    xl6PDiag(ctx, token, "out of memory while parsing f-string text");
                    break;
                }
                ++i;
                continue;
            }
            xl6PDiag(ctx, token, "单独的 } 在 f-string 文本中无效，请写成 }}");
            break;
        }
        if ( !xl6PTextBuilderAppendChar(&builder, text[i]) ) {
            xl6PDiag(ctx, token, "out of memory while parsing f-string text");
            break;
        }
    }
    if ( !failed &&
         !xl6PFlushFStringText(ctx, node, &builder, token, raw) )
        failed = 1;
    if ( !failed && quoteLen == 3 ) xl6PDedentInterpolatedString(node);
    xl6PTextBuilderUnit(&builder);
    return node;
}

static int xl6PTokenTextEquals(const XL6_Token* token, const char* text)
{
    size_t len;

    if ( token == NULL || token->text == NULL || text == NULL ) {
        return 0;
    }
    len = strlen(text);
    return token->len == len && strncmp(token->text, text, len) == 0;
}

static int xl6PIsBracketLiteralPrefix(const XL6_Token* token)
{
    return xl6PTokenTextEquals(token, "array") || xl6PTokenTextEquals(token, "list");
}

static int xl6PIsBraceLiteralPrefix(const XL6_Token* token)
{
    return xl6PTokenTextEquals(token, "set") || xl6PTokenTextEquals(token, "dict");
}

static XL6_AstNode* xl6PTypedLiteral(const XL6_Token* token, XL6_AstNode* literal)
{
    XL6_AstNode* node = xl6AstNew(XL6_AST_TYPED_LITERAL, token);
    xl6AstAddChild(node, literal);
    return node;
}

static unsigned int xl6PExternContractFlag(
    XL6_ParseContext* ctx, const XL6_Token* token)
{
    if ( xl6PTokenTextEquals(token, "pure") ) {
        return XL6_AST_FLAG_FOREIGN_PURE;
    }
    if ( xl6PTokenTextEquals(token, "no_raise") ) {
        return XL6_AST_FLAG_FOREIGN_NO_RAISE;
    }
    if ( xl6PTokenTextEquals(token, "nonblocking") ) return XL6_AST_FLAG_FOREIGN_NONBLOCKING;
    if ( xl6PTokenTextEquals(token, "owned") )
        return XL6_AST_FLAG_FOREIGN_OWNED_STRING;
    xl6PDiag(ctx, token,
        "unknown extern effect contract; expected @pure, @no_raise, @nonblocking, or @owned");
    return 0;
}

static XL6_AstNode* xl6PTypedLiteralWithType(const XL6_Token* token, XL6_AstNode* typeNode, XL6_AstNode* literal)
{
    XL6_AstNode* node = xl6AstNew(XL6_AST_TYPED_LITERAL, token);
    xl6AstAddChild(node, literal);
    xl6AstAddChild(node, typeNode);
    return node;
}

static XL6_AstNode* xl6PIndexPayloadFromList(XL6_AstNode* list)
{
    XL6_AstNode* only;

    if ( list == NULL ) {
        return NULL;
    }
    if ( list->childCount != 1 ) {
        return list;
    }

    only = list->children[0];
    list->children[0] = NULL;
    xl6AstFree(list);
    return only;
}

static XL6_AstNode* xl6PIndexPayloadFromArrayLiteral(XL6_AstNode* literal)
{
    XL6_AstNode* list;

    if ( literal == NULL || literal->kind != XL6_AST_ARRAY_LITERAL || literal->childCount == 0 ) {
        return literal;
    }
    if ( literal->childCount == 1 ) {
        return xl6PIndexPayloadFromList(literal);
    }

    list = xl6PList();
    xl6AstMoveChildren(list, literal);
    xl6AstFree(literal);
    return list;
}

static XL6_AstNode* xl6PBracketAfterName(const XL6_Token* token, XL6_AstNode* literal)
{
    XL6_AstNode* node;
    XL6_AstNode* indexPayload;

    if ( xl6PIsBracketLiteralPrefix(token) ) {
        return xl6PTypedLiteral(token, literal);
    }

    if ( literal != NULL && literal->kind == XL6_AST_ARRAY_LITERAL && literal->childCount > 0 ) {
        indexPayload = xl6PIndexPayloadFromArrayLiteral(literal);
        node = xl6AstNew(XL6_AST_INDEX, token);
        xl6AstAddChild(node, xl6AstNew(XL6_AST_NAME, token));
        xl6AstAddChild(node, indexPayload);
        return node;
    }

    return xl6PTypedLiteral(token, literal);
}

static XL6_AstNode* xl6PBraceAfterName(const XL6_Token* token, XL6_AstNode* literal)
{
    if ( xl6PIsBraceLiteralPrefix(token) ) {
        return xl6PTypedLiteral(token, literal);
    }
    return xl6PTypedLiteral(token, literal);
}

static XL6_AstNode* xl6PWithDoc(XL6_AstNode* node, const XL6_Token* doc)
{
    xl6AstSetDoc(node, doc);
    return node;
}

static XL6_AstNode* xl6PWithFlags(XL6_AstNode* node, unsigned int flags)
{
    if ( node != NULL ) {
        node->flags |= flags;
    }
    return node;
}
}

%syntax_error {
    xl6PDiag(ctx, &TOKEN, "语法错误");
}

%parse_failure {
    if ( ctx != NULL ) {
        ctx->hadError = 1;
    }
}

%nonassoc KW_THEN.
%nonassoc KW_ELSE.
/*
    这里不是旧语法兼容回退，而是上下文关键字机制。
    这些词在 class 成员修饰符、native-backed class 等特定位置具有语义；
    离开这些位置后，仍可作为普通标识符交给 IDENT 规则处理。
*/
/*
 * 这些词只在对应语法位置承担关键字含义；在普通标识符位置回退为 IDENT。
 * 这是上下文关键字机制，不是旧语法兼容路径。
 */
%fallback IDENT KW_FROM KW_PUBLIC KW_PRIVATE KW_PROTECTED KW_STATIC KW_OVERRIDE KW_FINAL KW_VIRTUAL KW_NATIVE KW_GET KW_LET KW_PROTOCOL.
%right FAT_ARROW.
%right QUESTION TYPE_QUESTION.
%left NULL_COALESCE.
%left KW_OR.
%left KW_AND.
%left BIT_OR.
%left BIT_XOR.
%left BIT_AND.
%left EQ NE APPROX.
%left LT LE GT GE KW_IN KW_IS KW_AS.
%left SHL SHR.
%left PLUS MINUS.
%left STAR SLASH PERCENT.
%right POW.
%right KW_NOT BANG BIT_NOT UMINUS.
%left LPAREN LBRACKET LBRACE SCOPE DOT SAFE_DOT SAFE_LBRACKET.

program ::= opt_seps items(A). {
    XL6_AstNode* root = xl6AstNew(XL6_AST_MODULE, NULL);
    xl6AstMoveChildren(root, A);
    xl6AstFree(A);
    ctx->root = root;
}

items(A) ::= . {
    A = xl6PList();
}
items(A) ::= items(B) item(C) opt_seps. {
    A = B;
    xl6AstAddChild(A, C);
}

item(A) ::= item_base(B). { A = B; }
item(A) ::= doc_block(D) item_base(B). { A = xl6PWithDoc(B, &D); }

item_base(A) ::= func_decl(B). { A = B; }
item_base(A) ::= import_decl(B). { A = B; }
item_base(A) ::= extern_func_decl(B). { A = B; }
item_base(A) ::= export_extern_func_decl(B). { A = B; }
item_base(A) ::= export_func_decl(B). { A = B; }
item_base(A) ::= extern_var_decl(B). { A = B; }
item_base(A) ::= export_extern_var_decl(B). { A = B; }
item_base(A) ::= export_var_decl(B). { A = B; }
item_base(A) ::= export_const_decl(B). { A = B; }
item_base(A) ::= type_alias_decl(B). { A = B; }
item_base(A) ::= extern_type_alias_decl(B). { A = B; }
item_base(A) ::= export_type_alias_decl(B). { A = B; }
item_base(A) ::= export_extern_type_alias_decl(B). { A = B; }
item_base(A) ::= class_decl(B). { A = B; }
item_base(A) ::= export_class_decl(B). { A = B; }
item_base(A) ::= protocol_decl(B). { A = B; }
item_base(A) ::= export_protocol_decl(B). { A = B; }
item_base(A) ::= enum_decl(B). { A = B; }
item_base(A) ::= export_enum_decl(B). { A = B; }
item_base(A) ::= union_decl(B). { A = B; }
item_base(A) ::= export_union_decl(B). { A = B; }
item_base(A) ::= stmt(B). { A = B; }
item(A) ::= TOK_ERROR(E). {
    xl6PDiag(ctx, &E, "当前语法位置不支持该 token");
    A = xl6AstNew(XL6_AST_LIST, NULL);
}

doc_block(A) ::= DOC(D). { A = D; }

name_tok(A) ::= IDENT(B). { A = B; }

import_name_part(A) ::= name_tok(N). { A = N; }
import_name_part(A) ::= KW_AUTO(N). { A = N; }

member_name_tok(A) ::= name_tok(N). { A = N; }
member_name_tok(A) ::= KW_AUTO(N). { A = N; }
member_name_tok(A) ::= KW_GET(N). { A = N; }
member_name_tok(A) ::= KW_LET(N). { A = N; }
member_name_tok(A) ::= KW_STATIC(N). { A = N; }
member_name_tok(A) ::= KW_END(N). { A = N; }
member_name_tok(A) ::= KW_THEN(N). { A = N; }
member_name_tok(A) ::= KW_CATCH(N). { A = N; }
member_name_tok(A) ::= KW_FINALLY(N). { A = N; }

import_name(A) ::= import_name_part(N). {
    A = xl6PImportNameFromToken(&N);
}
import_name(A) ::= import_name(P) DOT import_name_part(N). {
    A = xl6PDottedNameAppend(ctx, P, &N);
}

import_target(A) ::= STRING(S). {
    A = xl6AstNew(XL6_AST_NAME, &S);
}
import_target(A) ::= import_name(N). {
    A = N;
}

import_alias_opt(A) ::= . {
    A = xl6PList();
}
import_alias_opt(A) ::= KW_AS name_tok(N). {
    A = xl6AstNew(XL6_AST_NAME, &N);
}

import_select_item(A) ::= name_tok(N). {
    A = xl6AstNew(XL6_AST_NAME, &N);
}
import_select_item(A) ::= name_tok(N) KW_AS name_tok(R). {
    A = xl6AstNew(XL6_AST_NAME, &N);
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &R));
}

import_select_list(A) ::= import_select_item(I). {
    A = xl6PList();
    xl6AstAddChild(A, I);
}
import_select_list(A) ::= import_select_list(L) COMMA import_select_item(I). {
    A = L;
    xl6AstAddChild(A, I);
}

import_decl(A) ::= KW_IMPORT(I) import_target(T) import_alias_opt(R) need_sep. {
    A = xl6AstNewText(
        XL6_AST_IMPORT,
        T != NULL && T->text != NULL ? T->text : "",
        T != NULL && T->text != NULL ? strlen(T->text) : 0,
        &I);
    if ( R != NULL && R->kind == XL6_AST_NAME ) {
        xl6AstAddChild(A, R);
    } else {
        xl6AstFree(R);
    }
    xl6AstFree(T);
}
import_decl(A) ::= KW_FROM(F) import_target(T) KW_IMPORT import_select_list(L) need_sep. {
    A = xl6AstNewText(
        XL6_AST_IMPORT,
        T != NULL && T->text != NULL ? T->text : "",
        T != NULL && T->text != NULL ? strlen(T->text) : 0,
        &F);
    A->flags |= XL6_AST_FLAG_IMPORT_SELECTIVE;
    xl6AstMoveChildren(A, L);
    xl6AstFree(L);
    xl6AstFree(T);
}

opt_seps ::= .
opt_seps ::= opt_seps SEP.
need_sep ::= SEP.

func_decl(A) ::= KW_FUNC(N) name_tok(F) generic_params_opt(G) name_params(P) return_sig(R) need_sep block(B) KW_END. {
    A = xl6AstNew(XL6_AST_FUNC, &N);
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &F));
    xl6AstAddChild(A, P);
    xl6AstAddChild(A, R);
    xl6AstAddChild(A, B);
    xl6AstAddChild(A, G);
}
func_decl(A) ::= KW_FUNC(N) name_tok(F) LT generic_param_list(L) GT name_params(P) return_sig(R) need_sep block(B) KW_END. {
    XL6_AstNode* G = xl6AstNew(XL6_AST_TYPE_PARAMS, NULL);
    xl6AstMoveChildren(G, L);
    xl6AstFree(L);
    A = xl6AstNew(XL6_AST_FUNC, &N);
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &F));
    xl6AstAddChild(A, P);
    xl6AstAddChild(A, R);
    xl6AstAddChild(A, B);
    xl6AstAddChild(A, G);
}
func_decl(A) ::= KW_ASYNC(AW) KW_FUNC name_tok(F) generic_params_opt(G) name_params(P) return_sig(R) need_sep block(B) KW_END. {
    A = xl6AstNew(XL6_AST_ASYNC_FUNC, &AW);
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &F));
    xl6AstAddChild(A, P);
    xl6AstAddChild(A, R);
    xl6AstAddChild(A, B);
    xl6AstAddChild(A, G);
}
func_decl(A) ::= KW_ASYNC(AW) KW_FUNC name_tok(F) LT generic_param_list(L) GT name_params(P) return_sig(R) need_sep block(B) KW_END. {
    XL6_AstNode* G = xl6AstNew(XL6_AST_TYPE_PARAMS, NULL);
    xl6AstMoveChildren(G, L);
    xl6AstFree(L);
    A = xl6AstNew(XL6_AST_ASYNC_FUNC, &AW);
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &F));
    xl6AstAddChild(A, P);
    xl6AstAddChild(A, R);
    xl6AstAddChild(A, B);
    xl6AstAddChild(A, G);
}

extern_func_decl(A) ::= KW_EXTERN(E) STRING(S) extern_func_contract(C) KW_FUNC name_tok(F) name_params(P) return_sig(R) need_sep. {
    A = xl6AstNew(XL6_AST_EXTERN_FUNC, &S);
    A->flags |= C;
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &F));
    xl6AstAddChild(A, P);
    xl6AstAddChild(A, R);
    (void)E;
}

export_extern_func_decl(A) ::= KW_EXPORT(E) extern_func_decl(F). {
    A = xl6PWithFlags(F, XL6_AST_FLAG_PUBLIC);
    (void)E;
}

export_func_decl(A) ::= KW_EXPORT(E) STRING(S) KW_FUNC name_tok(F) name_params(P) return_sig(R) need_sep block(B) KW_END. {
    A = xl6AstNew(XL6_AST_EXPORT_FUNC, &S);
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &F));
    xl6AstAddChild(A, P);
    xl6AstAddChild(A, R);
    xl6AstAddChild(A, B);
    (void)E;
}
export_func_decl(A) ::= KW_EXPORT(E) KW_FUNC name_tok(F) name_params(P) return_sig(R) need_sep block(B) KW_END. {
    A = xl6AstNew(XL6_AST_EXPORT_FUNC, NULL);
    A->line = E.startLine;
    A->column = E.startColumn;
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &F));
    xl6AstAddChild(A, P);
    xl6AstAddChild(A, R);
    xl6AstAddChild(A, B);
}

extern_var_decl(A) ::= KW_EXTERN(E) STRING(S) var_type_ref(T) name_tok(N) need_sep. {
    A = xl6AstNew(XL6_AST_EXTERN_VAR, &S);
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &N));
    xl6AstAddChild(A, T);
    (void)E;
}

export_extern_var_decl(A) ::= KW_EXPORT(E) KW_EXTERN STRING(S) var_type_ref(T) name_tok(N) need_sep. {
    A = xl6AstNew(XL6_AST_EXTERN_VAR, &S);
    A->flags |= XL6_AST_FLAG_PUBLIC;
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &N));
    xl6AstAddChild(A, T);
    (void)E;
}

export_var_decl(A) ::= KW_EXPORT(E) var_type_ref(T) name_tok(N) ASSIGN expr(V) need_sep. {
    A = xl6AstNew(XL6_AST_EXPORT_VAR, NULL);
    A->line = E.startLine;
    A->column = E.startColumn;
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &N));
    xl6AstAddChild(A, T);
    xl6AstAddChild(A, V);
}
export_var_decl(A) ::= KW_EXPORT(E) STRING(S) var_type_ref(T) name_tok(N) ASSIGN expr(V) need_sep. {
    A = xl6AstNew(XL6_AST_EXPORT_VAR, &S);
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &N));
    xl6AstAddChild(A, T);
    xl6AstAddChild(A, V);
    (void)E;
}

export_const_decl(A) ::= KW_EXPORT(E) KW_CONST type_ref(T) name_tok(N) ASSIGN expr(V) need_sep. {
    A = xl6AstNew(XL6_AST_EXPORT_CONST, NULL);
    A->line = E.startLine;
    A->column = E.startColumn;
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &N));
    xl6AstAddChild(A, T);
    xl6AstAddChild(A, V);
}
export_const_decl(A) ::= KW_EXPORT(E) STRING(S) KW_CONST type_ref(T) name_tok(N) ASSIGN expr(V) need_sep. {
    A = xl6AstNew(XL6_AST_EXPORT_CONST, &S);
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &N));
    xl6AstAddChild(A, T);
    xl6AstAddChild(A, V);
    (void)E;
}

type_alias_decl(A) ::= KW_TYPEDEF(D) name_tok(N) ASSIGN type_expr(T) need_sep. {
    A = xl6AstNew(XL6_AST_TYPE_ALIAS, &N);
    xl6AstAddChild(A, T);
    (void)D;
}

extern_type_alias_decl(A) ::= KW_EXTERN(E) name_tok(K) name_tok(N) ASSIGN type_expr(T) need_sep. {
    if ( !xl6PTokenTextEquals(&K, "type") ) {
        xl6PDiag(ctx, &K, "extern type alias must start with extern type");
    }
    A = xl6AstNew(XL6_AST_EXTERN_TYPE_ALIAS, &N);
    xl6AstAddChild(A, T);
    (void)E;
}

export_type_alias_decl(A) ::= KW_EXPORT(E) type_alias_decl(T). {
    A = xl6PWithFlags(T, XL6_AST_FLAG_PUBLIC);
    (void)E;
}

export_extern_type_alias_decl(A) ::= KW_EXPORT(E) extern_type_alias_decl(T). {
    A = xl6PWithFlags(T, XL6_AST_FLAG_PUBLIC);
    (void)E;
}

class_decl(A) ::= KW_CLASS(C) name_tok(N) generic_params_opt(G) class_extends(E) need_sep class_body(B) KW_END. {
    A = xl6AstNew(XL6_AST_CLASS, &N);
    xl6AstAddChild(A, E);
    xl6AstAddChild(A, B);
    xl6AstAddChild(A, G);
    (void)C;
}

export_class_decl(A) ::= KW_EXPORT(E) class_decl(C). {
    A = xl6PWithFlags(C, XL6_AST_FLAG_PUBLIC);
    (void)E;
}

protocol_decl(A) ::= KW_PROTOCOL(P) name_tok(N) generic_params_opt(G) need_sep protocol_body(B) KW_END. {
    A = xl6AstNew(XL6_AST_PROTOCOL, &N);
    xl6AstAddChild(A, B);
    xl6AstAddChild(A, G);
    (void)P;
}

export_protocol_decl(A) ::= KW_EXPORT(E) protocol_decl(P). {
    A = xl6PWithFlags(P, XL6_AST_FLAG_PUBLIC);
    (void)E;
}

enum_decl(A) ::= KW_ENUM(E) name_tok(N) generic_params_opt(G) KW_AS type_expr(T) need_sep numeric_enum_body(B) KW_END. {
    A = xl6AstNew(XL6_AST_ENUM, &N);
    A->flags |= XL6_AST_FLAG_NUMERIC_ENUM;
    xl6AstAddChild(A, T);
    xl6AstAddChild(A, B);
    xl6AstAddChild(A, G);
    (void)E;
}
enum_decl(A) ::= KW_ENUM(E) name_tok(N) generic_params_opt(G) need_sep tagged_enum_body(B) KW_END. {
    A = xl6AstNew(XL6_AST_ENUM, &N);
    xl6AstAddChild(A, xl6PList());
    xl6AstAddChild(A, B);
    xl6AstAddChild(A, G);
    (void)E;
}

export_enum_decl(A) ::= KW_EXPORT(E) enum_decl(N). {
    A = xl6PWithFlags(N, XL6_AST_FLAG_PUBLIC);
    (void)E;
}

numeric_enum_body(A) ::= . {
    A = xl6PList();
}
numeric_enum_body(A) ::= numeric_enum_body(B) SEP. {
    A = B;
}
numeric_enum_body(A) ::= numeric_enum_body(B) numeric_enum_variant(V). {
    A = B;
    xl6AstAddChild(A, V);
}

numeric_enum_variant(A) ::= name_tok(N) need_sep. {
    A = xl6AstNew(XL6_AST_ENUM_VARIANT, &N);
}
numeric_enum_variant(A) ::= name_tok(N) ASSIGN expr(V) need_sep. {
    A = xl6AstNew(XL6_AST_ENUM_VARIANT, &N);
    xl6AstAddChild(A, V);
}

tagged_enum_body(A) ::= . {
    A = xl6PList();
}
tagged_enum_body(A) ::= tagged_enum_body(B) SEP. {
    A = B;
}
tagged_enum_body(A) ::= tagged_enum_body(B) tagged_enum_variant(V). {
    A = B;
    xl6AstAddChild(A, V);
}

tagged_enum_variant(A) ::= name_tok(N) need_sep. {
    A = xl6AstNew(XL6_AST_ENUM_VARIANT, &N);
    xl6AstAddChild(A, xl6PList());
}
tagged_enum_variant(A) ::= name_tok(N) name_params(P) need_sep. {
    A = xl6AstNew(XL6_AST_ENUM_VARIANT, &N);
    xl6AstAddChild(A, P);
}

protocol_body(A) ::= . {
    A = xl6PList();
}
protocol_body(A) ::= protocol_body(B) SEP. {
    A = B;
}
protocol_body(A) ::= protocol_body(B) protocol_member(M). {
    A = B;
    xl6AstAddChild(A, M);
}

protocol_member(A) ::= KW_FUNC(F) name_tok(N) generic_params_opt(G) name_params(P) return_sig(R) need_sep. {
    A = xl6AstNew(XL6_AST_FUNC, &F);
    A->flags |= XL6_AST_FLAG_PROTOCOL_REQUIREMENT;
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &N));
    xl6AstAddChild(A, P);
    xl6AstAddChild(A, R);
    xl6AstAddChild(A, xl6AstNew(XL6_AST_BLOCK, NULL));
    xl6AstAddChild(A, G);
}
protocol_member(A) ::= KW_ASYNC(AW) KW_FUNC name_tok(N) generic_params_opt(G) name_params(P) return_sig(R) need_sep. {
    A = xl6AstNew(XL6_AST_ASYNC_FUNC, &AW);
    A->flags |= XL6_AST_FLAG_PROTOCOL_REQUIREMENT | XL6_AST_FLAG_ASYNC;
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &N));
    xl6AstAddChild(A, P);
    xl6AstAddChild(A, R);
    xl6AstAddChild(A, xl6AstNew(XL6_AST_BLOCK, NULL));
    xl6AstAddChild(A, G);
}
protocol_member(A) ::= KW_GET(G) member_name_tok(N) name_params(P) return_sig(R) need_sep. {
    A = xl6AstNew(XL6_AST_GET, &G);
    A->flags |= XL6_AST_FLAG_PROTOCOL_REQUIREMENT;
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &N));
    xl6AstAddChild(A, P);
    xl6AstAddChild(A, R);
    xl6AstAddChild(A, xl6AstNew(XL6_AST_BLOCK, NULL));
}
protocol_member(A) ::= KW_LET(L) member_name_tok(N) name_params(P) need_sep. {
    A = xl6AstNew(XL6_AST_LET, &L);
    A->flags |= XL6_AST_FLAG_PROTOCOL_REQUIREMENT;
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &N));
    xl6AstAddChild(A, P);
    xl6AstAddChild(A, xl6AstNew(XL6_AST_RETURN_TYPES, NULL));
    xl6AstAddChild(A, xl6AstNew(XL6_AST_BLOCK, NULL));
}

class_extends(A) ::= . {
    A = xl6PList();
}
class_extends(A) ::= KW_EXTENDS name_tok(N). {
    A = xl6AstNew(XL6_AST_NAME, &N);
}
class_extends(A) ::= KW_EXTENDS name_tok(P) DOT import_name(N). {
    A = xl6PQualifiedType(ctx, &P, N);
    if ( A != NULL ) {
        A->kind = XL6_AST_NAME;
    }
}

class_body(A) ::= . {
    A = xl6PList();
}
class_body(A) ::= class_body(B) SEP. {
    A = B;
}
class_body(A) ::= class_body(B) class_member(M). {
    A = B;
    xl6AstAddChild(A, M);
}

class_member(A) ::= class_member_base(B). { A = B; }
class_member(A) ::= class_member_mods(M) class_member_base(B). { A = xl6PWithFlags(B, M); }
class_member(A) ::= doc_block(D) class_member_base(B). { A = xl6PWithDoc(B, &D); }
class_member(A) ::= doc_block(D) class_member_mods(M) class_member_base(B). { A = xl6PWithDoc(xl6PWithFlags(B, M), &D); }

class_member_mods(A) ::= class_member_modifier(M). { A = M; }
class_member_mods(A) ::= class_member_mods(B) class_member_modifier(M). { A = B | M; }

class_member_modifier(A) ::= KW_PUBLIC. { A = XL6_AST_FLAG_PUBLIC; }
class_member_modifier(A) ::= KW_PRIVATE. { A = XL6_AST_FLAG_PRIVATE; }
class_member_modifier(A) ::= KW_PROTECTED. { A = XL6_AST_FLAG_PROTECTED; }
class_member_modifier(A) ::= KW_STATIC. { A = XL6_AST_FLAG_STATIC; }
class_member_modifier(A) ::= KW_OVERRIDE. { A = XL6_AST_FLAG_OVERRIDE; }
class_member_modifier(A) ::= KW_FINAL. { A = XL6_AST_FLAG_FINAL; }
class_member_modifier(A) ::= KW_VIRTUAL. { A = XL6_AST_FLAG_VIRTUAL; }

class_member_base(A) ::= field_decl(B). { A = B; }
class_member_base(A) ::= native_field_decl(B). { A = B; }
class_member_base(A) ::= derive_decl(B). { A = B; }
class_member_base(A) ::= func_decl(B). { A = B; }
class_member_base(A) ::= class_extern_special(B). { A = B; }
class_member_base(A) ::= class_extern_func(B). { A = B; }
class_member_base(A) ::= class_extern_get(B). { A = B; }
class_member_base(A) ::= class_extern_let(B). { A = B; }
class_member_base(A) ::= export_func_decl(B). { A = B; }
class_member_base(A) ::= property_get_decl(B). { A = B; }
class_member_base(A) ::= property_let_decl(B). { A = B; }
class_member_base(A) ::= class_special_decl(B). { A = B; }
class_member_base(A) ::= union_decl(B). { A = B; }

derive_decl(A) ::= KW_DERIVE(D) name_tok(N) need_sep. {
    A = xl6AstNew(XL6_AST_DERIVE, &N);
    (void)D;
}

field_decl(A) ::= var_type_ref(T) name_tok(N) field_attrs_opt(R) field_default(D) need_sep. {
    A = xl6AstNew(XL6_AST_FIELD, &N);
    xl6AstAddChild(A, T);
    if ( D != NULL ) {
        xl6AstAddChild(A, D);
    }
    if ( R != NULL ) {
        xl6AstMoveChildren(A, R);
        xl6AstFree(R);
    }
}

native_field_decl(A) ::= KW_NATIVE(K) native_type_ref(T) name_tok(N) field_attrs_opt(R) field_default(D) need_sep. {
    A = xl6AstNew(XL6_AST_FIELD, &N);
    A->flags |= XL6_AST_FLAG_NATIVE;
    xl6AstAddChild(A, T);
    if ( D != NULL ) {
        xl6AstAddChild(A, D);
    }
    if ( R != NULL ) {
        xl6AstMoveChildren(A, R);
        xl6AstFree(R);
    }
    (void)K;
}

field_default(A) ::= . { A = NULL; }
field_default(A) ::= ASSIGN expr(E). { A = E; }

field_attrs_opt(A) ::= . { A = xl6PList(); }
field_attrs_opt(A) ::= field_attrs(B). { A = B; }

field_attrs(A) ::= field_attr(B). {
    A = xl6PList();
    xl6AstAddChild(A, B);
}
field_attrs(A) ::= field_attrs(B) field_attr(C). {
    A = B;
    xl6AstAddChild(A, C);
}

field_attr(A) ::= AT name_tok(N) LPAREN field_attr_args_opt(P) RPAREN. {
    A = xl6AstNew(XL6_AST_FIELD_ATTR, &N);
    xl6AstMoveChildren(A, P);
    xl6AstFree(P);
}

field_attr_args_opt(A) ::= . { A = xl6PList(); }
field_attr_args_opt(A) ::= field_attr_args(B). { A = B; }

field_attr_args(A) ::= field_attr_arg(B). {
    A = xl6PList();
    xl6AstAddChild(A, B);
}
field_attr_args(A) ::= field_attr_args(B) COMMA field_attr_arg(C). {
    A = B;
    xl6AstAddChild(A, C);
}

field_attr_word(A) ::= name_tok(N). { A = N; }
field_attr_word(A) ::= KW_INCLUDE(N). { A = N; }
field_attr_word(A) ::= KW_NULL(N). { A = N; }

field_attr_arg(A) ::= field_attr_word(N). {
    A = xl6AstNew(XL6_AST_NAME, &N);
}
field_attr_arg(A) ::= STRING(S). {
    A = xl6AstNew(XL6_AST_STRING, &S);
}
field_attr_arg(A) ::= field_attr_word(N) ASSIGN field_attr_word(V). {
    A = xl6AstNew(XL6_AST_KEY_VALUE, NULL);
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &N));
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &V));
}
field_attr_arg(A) ::= field_attr_word(N) ASSIGN STRING(V). {
    A = xl6AstNew(XL6_AST_KEY_VALUE, NULL);
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &N));
    xl6AstAddChild(A, xl6AstNew(XL6_AST_STRING, &V));
}

/* Keep these productions after field_attr so AT retains its established
 * terminal number in the generated parser/lexer interface. */
extern_func_contract(A) ::= . { A = 0; }
extern_func_contract(A) ::= extern_func_contract(B) extern_func_contract_item(C). { A = B | C; }
extern_func_contract_item(A) ::= AT name_tok(N). {
    A = xl6PExternContractFlag(ctx, &N);
}

native_type_ref(A) ::= var_type_ref(T). { A = T; }
native_type_ref(A) ::= name_tok(N) STAR. {
    if ( !xl6PTokenTextEquals(&N, "void") ) {
        xl6PDiag(ctx, &N, "native pointer type only supports void* in this stage");
    }
    A = xl6AstNewText(XL6_AST_TYPE, "point", 5, &N);
}

class_extern_func(A) ::= KW_EXTERN(E) STRING(S) extern_func_contract(C) KW_FUNC name_tok(N) name_params(P) return_sig(R) need_sep. {
    A = xl6AstNew(XL6_AST_NATIVE_EXTERN_FUNC, &S);
    A->flags |= C;
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &N));
    xl6AstAddChild(A, P);
    xl6AstAddChild(A, R);
    (void)E;
}

class_extern_special(A) ::= KW_EXTERN(E) STRING(S) extern_func_contract(C) name_tok(N) name_params(P) need_sep. {
    A = xl6AstNew(XL6_AST_NATIVE_EXTERN_SPECIAL, &S);
    A->flags |= C;
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &N));
    xl6AstAddChild(A, P);
    (void)E;
}

class_extern_get(A) ::= KW_EXTERN(E) STRING(S) extern_func_contract(C) KW_GET member_name_tok(N) name_params(P) return_sig(R) need_sep. {
    A = xl6AstNew(XL6_AST_GET, &S);
    A->flags |= XL6_AST_FLAG_EXTERN | C;
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &N));
    xl6AstAddChild(A, P);
    xl6AstAddChild(A, R);
    xl6AstAddChild(A, xl6AstNew(XL6_AST_BLOCK, NULL));
    (void)E;
}

class_extern_let(A) ::= KW_EXTERN(E) STRING(S) extern_func_contract(C) KW_LET member_name_tok(N) name_params(P) need_sep. {
    A = xl6AstNew(XL6_AST_LET, &S);
    A->flags |= XL6_AST_FLAG_EXTERN | C;
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &N));
    xl6AstAddChild(A, P);
    xl6AstAddChild(A, xl6AstNew(XL6_AST_RETURN_TYPES, NULL));
    xl6AstAddChild(A, xl6AstNew(XL6_AST_BLOCK, NULL));
    (void)E;
}

property_get_decl(A) ::= KW_GET(G) member_name_tok(N) name_params(P) return_sig(R) need_sep block(B) KW_END. {
    A = xl6AstNew(XL6_AST_GET, &G);
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &N));
    xl6AstAddChild(A, P);
    xl6AstAddChild(A, R);
    xl6AstAddChild(A, B);
}

property_let_decl(A) ::= KW_LET(L) member_name_tok(N) name_params(P) need_sep block(B) KW_END. {
    A = xl6AstNew(XL6_AST_LET, &L);
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &N));
    xl6AstAddChild(A, P);
    xl6AstAddChild(A, xl6AstNew(XL6_AST_RETURN_TYPES, NULL));
    xl6AstAddChild(A, B);
}

union_decl(A) ::= KW_UNION(U) need_sep union_body(B) KW_END. {
    A = xl6AstNew(XL6_AST_UNION, &U);
    xrtFree(A->text);
    A->text = NULL;
    xl6AstAddChild(A, B);
}
union_decl(A) ::= KW_UNION(U) name_tok(N) need_sep union_body(B) KW_END. {
    A = xl6AstNew(XL6_AST_UNION, &N);
    xl6AstAddChild(A, B);
    (void)U;
}

export_union_decl(A) ::= KW_EXPORT(E) union_decl(U). {
    A = xl6PWithFlags(U, XL6_AST_FLAG_PUBLIC);
    if ( A != NULL && A->text == NULL ) {
        xl6PDiag(ctx, &E, "export union requires a name");
    }
}
union_body(A) ::= . {
    A = xl6PList();
}
union_body(A) ::= union_body(B) SEP. {
    A = B;
}
union_body(A) ::= union_body(B) union_member(M). {
    A = B;
    xl6AstAddChild(A, M);
}

union_member(A) ::= field_decl(B). { A = B; }

class_special_decl(A) ::= name_tok(N) name_params(P) need_sep block(B) KW_END. {
    A = xl6AstNew(XL6_AST_CREATE, &N);
    xl6AstAddChild(A, P);
    xl6AstAddChild(A, B);
}

name_params(A) ::= LPAREN param_list_opt(B) RPAREN. { A = B; }

generic_params_opt(A) ::= . {
    A = xl6AstNew(XL6_AST_TYPE_PARAMS, NULL);
}
generic_params_opt(A) ::= LT generic_param_list(L) GT. {
    A = xl6AstNew(XL6_AST_TYPE_PARAMS, NULL);
    xl6AstMoveChildren(A, L);
    xl6AstFree(L);
}

generic_param_list(A) ::= generic_param(P). {
    A = xl6PList();
    xl6AstAddChild(A, P);
}
generic_param_list(A) ::= generic_param_list(B) COMMA generic_param(P). {
    A = B;
    xl6AstAddChild(A, P);
}

generic_param(A) ::= name_tok(N) generic_constraint_opt(C). {
    A = xl6AstNew(XL6_AST_NAME, &N);
    if ( C != NULL ) {
        xl6AstAddChild(A, C);
    }
}

generic_constraint_opt(A) ::= . { A = NULL; }
generic_constraint_opt(A) ::= KW_IS generic_constraint_list(L). {
    A = xl6AstNew(XL6_AST_GENERIC_CONSTRAINT, NULL);
    xl6AstMoveChildren(A, L);
    xl6AstFree(L);
}

generic_constraint_list(A) ::= type_ref(T). {
    A = xl6PList();
    xl6AstAddChild(A, T);
}
generic_constraint_list(A) ::= qualified_type_ref(T). {
    A = xl6PList();
    xl6AstAddChild(A, T);
}
generic_constraint_list(A) ::= generic_constraint_list(B) KW_AND type_ref(T). {
    A = B;
    xl6AstAddChild(A, T);
}
generic_constraint_list(A) ::= generic_constraint_list(B) KW_AND qualified_type_ref(T). {
    A = B;
    xl6AstAddChild(A, T);
}

param_list_opt(A) ::= . { A = xl6PList(); }
param_list_opt(A) ::= param_list(B). { A = B; }

param_list(A) ::= param(B). {
    A = xl6PList();
    xl6AstAddChild(A, B);
}
param_list(A) ::= param_list(B) COMMA param(C). {
    A = B;
    xl6AstAddChild(A, C);
}

type_expr(A) ::= type_ref(T). { A = T; }
type_expr(A) ::= type_ref(T) TYPE_QUESTION(Q). {
    A = xl6PNullableType(ctx, T, &Q);
}
type_expr(A) ::= function_type_ref(T). { A = T; }
/*
 * func(int) as int? 中的 ? 属于返回类型。
 * 可空函数值必须写成 (func(int) as int)?，让后缀明确作用于完整签名。
 */
type_expr(A) ::= LPAREN function_type_ref(T) TYPE_RPAREN TYPE_QUESTION(Q). {
    A = xl6PNullableType(ctx, T, &Q);
}
type_expr(A) ::= KW_ASYNC type_ref(T). {
    T->flags |= XL6_AST_FLAG_ASYNC;
    A = T;
}

param_type_ref(A) ::= type_expr(T). { A = T; }

param_mode(A) ::= . { A = 0; }
param_mode(A) ::= KW_C_CALLBACK. { A = XL6_AST_FLAG_C_CALLBACK; }

param(A) ::= param_mode(M) param_type_ref(T) name_tok(N). {
    A = xl6AstNew(XL6_AST_PARAM, &N);
    A->flags |= M;
    xl6AstAddChild(A, T);
}
param(A) ::= param_mode(M) param_type_ref(T) name_tok(N) ASSIGN expr(E). {
    A = xl6AstNew(XL6_AST_PARAM, &N);
    A->flags |= M;
    xl6AstAddChild(A, T);
    xl6AstAddChild(A, E);
}
param(A) ::= param_mode(M) qualified_type_ref(T) name_tok(N). {
    A = xl6AstNew(XL6_AST_PARAM, &N);
    A->flags |= M;
    xl6AstAddChild(A, T);
}
param(A) ::= param_mode(M) qualified_type_ref(T) TYPE_QUESTION(Q) name_tok(N). {
    A = xl6AstNew(XL6_AST_PARAM, &N);
    A->flags |= M;
    T = xl6PNullableType(ctx, T, &Q);
    xl6AstAddChild(A, T);
}
param(A) ::= param_mode(M) qualified_type_ref(T) name_tok(N) ASSIGN expr(E). {
    A = xl6AstNew(XL6_AST_PARAM, &N);
    A->flags |= M;
    xl6AstAddChild(A, T);
    xl6AstAddChild(A, E);
}
param(A) ::= param_mode(M) qualified_type_ref(T) TYPE_QUESTION(Q) name_tok(N) ASSIGN expr(E). {
    A = xl6AstNew(XL6_AST_PARAM, &N);
    A->flags |= M;
    T = xl6PNullableType(ctx, T, &Q);
    xl6AstAddChild(A, T);
    xl6AstAddChild(A, E);
}
param(A) ::= KW_C_VARARG(T). {
    A = xl6AstNewText(XL6_AST_PARAM, "arg", 3, &T);
    xl6AstAddChild(A, xl6AstNewText(XL6_AST_TYPE, "c_vararg", 8, &T));
}
param(A) ::= ELLIPSIS(T). {
    A = xl6AstNewText(XL6_AST_PARAM, "arg", 3, &T);
    xl6AstAddChild(A, xl6AstNewText(XL6_AST_TYPE, "array<var>", 10, &T));
}
param(A) ::= TRIPLE_STAR(T). {
    A = xl6AstNewText(XL6_AST_PARAM, "arg", 3, &T);
    xl6AstAddChild(A, xl6AstNewText(XL6_AST_TYPE, "dict<var>", 9, &T));
}

return_sig(A) ::= . [KW_THEN] {
    A = xl6AstNew(XL6_AST_RETURN_TYPES, NULL);
}
return_sig(A) ::= KW_AS return_value_type_ref(T). {
    A = xl6AstNew(XL6_AST_RETURN_TYPES, NULL);
    xl6AstAddChild(A, T);
}
return_sig(A) ::= KW_AS qualified_type_ref(T). {
    A = xl6AstNew(XL6_AST_RETURN_TYPES, NULL);
    xl6AstAddChild(A, T);
}
return_sig(A) ::= KW_AS qualified_type_ref(T) TYPE_QUESTION(Q). {
    A = xl6AstNew(XL6_AST_RETURN_TYPES, NULL);
    xl6AstAddChild(A, xl6PNullableType(ctx, T, &Q));
}
return_sig(A) ::= KW_AS LPAREN type_list(L) RPAREN. {
    A = xl6AstNew(XL6_AST_RETURN_TYPES, NULL);
    xl6AstMoveChildren(A, L);
    xl6AstFree(L);
}

type_list(A) ::= type_expr(T). {
    A = xl6PList();
    xl6AstAddChild(A, T);
}
type_list(A) ::= qualified_type_ref(T). {
    A = xl6PList();
    xl6AstAddChild(A, T);
}
type_list(A) ::= qualified_type_ref(T) TYPE_QUESTION(Q). {
    A = xl6PList();
    xl6AstAddChild(A, xl6PNullableType(ctx, T, &Q));
}
type_list(A) ::= type_list(B) COMMA type_expr(T). {
    A = B;
    xl6AstAddChild(A, T);
}
type_list(A) ::= type_list(B) COMMA qualified_type_ref(T). {
    A = B;
    xl6AstAddChild(A, T);
}
type_list(A) ::= type_list(B) COMMA qualified_type_ref(T) TYPE_QUESTION(Q). {
    A = B;
    xl6AstAddChild(A, xl6PNullableType(ctx, T, &Q));
}

type_ref(A) ::= name_tok(N). [KW_THEN] {
    A = xl6AstNew(XL6_AST_TYPE, &N);
}
type_ref(A) ::= QUALIFIED_TYPE(N). [KW_THEN] {
    A = xl6AstNew(XL6_AST_TYPE, &N);
}
type_ref(A) ::= KW_VAR(N). {
    A = xl6AstNewText(XL6_AST_TYPE, "var", 3, &N);
}
type_ref(A) ::= KW_NULL(N). {
    A = xl6AstNewText(XL6_AST_TYPE, "null", 4, &N);
}
type_ref(A) ::= TYPE_FUNCTION(N). [KW_THEN] {
    A = xl6AstNewText(XL6_AST_TYPE, "function", 8, &N);
}
type_ref(A) ::= name_tok(N) LT type_list(L) GT. {
    A = xl6AstNewGenericType(&N, L);
}
type_ref(A) ::= name_tok(N) TYPE_LT type_list(L) GT. {
    A = xl6AstNewGenericType(&N, L);
}
type_ref(A) ::= QUALIFIED_TYPE(N) LT type_list(L) GT. {
    A = xl6AstNewGenericType(&N, L);
}
type_ref(A) ::= QUALIFIED_TYPE(N) TYPE_LT type_list(L) GT. {
    A = xl6AstNewGenericType(&N, L);
}
qualified_type_ref(A) ::= name_tok(P) DOT import_name(N). [KW_THEN] {
    A = xl6PQualifiedType(ctx, &P, N);
}
qualified_type_ref(A) ::= name_tok(P) DOT import_name(N) LT type_list(L) GT. {
    A = xl6PQualifiedType(ctx, &P, N);
    xl6AstAddChild(A, L);
}
qualified_type_ref(A) ::= name_tok(P) DOT import_name(N) TYPE_LT type_list(L) GT. {
    A = xl6PQualifiedType(ctx, &P, N);
    xl6AstAddChild(A, L);
}

return_value_type_ref(A) ::= type_expr(T). { A = T; }

function_type_ref(A) ::= TYPE_FUNCTION(N) LPAREN func_type_params_opt(P) RPAREN return_sig(R). {
    A = xl6AstNewText(XL6_AST_TYPE, "function", 8, &N);
    xl6AstAddChild(A, P);
    xl6AstAddChild(A, R);
}

func_type_params_opt(A) ::= . { A = xl6PList(); }
func_type_params_opt(A) ::= func_type_params(B). { A = B; }

func_type_params(A) ::= func_type_param(T). {
    A = xl6PList();
    xl6AstAddChild(A, T);
}
func_type_params(A) ::= func_type_params(B) COMMA func_type_param(T). {
    A = B;
    xl6AstAddChild(A, T);
}

func_type_param(A) ::= param_mode(M) type_expr(T). {
    T->flags |= M;
    A = T;
}
func_type_param(A) ::= param_mode(M) qualified_type_ref(T). {
    T->flags |= M;
    A = T;
}
func_type_param(A) ::= param_mode(M) qualified_type_ref(T) TYPE_QUESTION(Q). {
    T = xl6PNullableType(ctx, T, &Q);
    if ( T != NULL ) {
        T->flags |= M;
    }
    A = T;
}
func_type_param(A) ::= ELLIPSIS(T). {
    A = xl6AstNewText(XL6_AST_TYPE, "array<var>", 10, &T);
}
func_type_param(A) ::= TRIPLE_STAR(T). {
    A = xl6AstNewText(XL6_AST_TYPE, "dict<var>", 9, &T);
}

block(A) ::= . {
    A = xl6AstNew(XL6_AST_BLOCK, NULL);
}
block(A) ::= block(B) SEP. {
    A = B;
}
block(A) ::= block(B) stmt(S). {
    A = B;
    xl6AstAddChild(A, S);
}

stmt(A) ::= simple_stmt(B) need_sep. { A = B; }
stmt(A) ::= compound_stmt(B). { A = B; }

simple_stmt(A) ::= var_decl(B). { A = B; }
simple_stmt(A) ::= assign_stmt(B). { A = B; }
simple_stmt(A) ::= return_stmt(B). { A = B; }
simple_stmt(A) ::= yield_stmt(B). { A = B; }
simple_stmt(A) ::= throw_stmt(B). { A = B; }
simple_stmt(A) ::= assert_stmt(B). { A = B; }
simple_stmt(A) ::= break_stmt(B). { A = B; }
simple_stmt(A) ::= continue_stmt(B). { A = B; }
simple_stmt(A) ::= expr(E). {
    A = xl6AstNew(XL6_AST_EXPR_STMT, NULL);
    xl6AstAddChild(A, E);
}

assign_stmt(A) ::= postfix_expr(L) assign_op(OP) expr(R). {
    A = xl6AstNew(XL6_AST_ASSIGN, &OP);
    xl6AstAddChild(A, L);
    xl6AstAddChild(A, R);
}
assign_stmt(A) ::= multi_assign_targets(L) ASSIGN(OP) expr(R). {
    A = xl6AstNew(XL6_AST_MULTI_ASSIGN, &OP);
    xl6AstAddChild(A, L);
    xl6AstAddChild(A, R);
}
assign_op(A) ::= ASSIGN(B). { A = B; }
assign_op(A) ::= PLUS_ASSIGN(B). { A = B; }
assign_op(A) ::= MINUS_ASSIGN(B). { A = B; }
assign_op(A) ::= STAR_ASSIGN(B). { A = B; }
assign_op(A) ::= SLASH_ASSIGN(B). { A = B; }
assign_op(A) ::= PERCENT_ASSIGN(B). { A = B; }
assign_op(A) ::= POW_ASSIGN(B). { A = B; }
assign_op(A) ::= BIT_AND_ASSIGN(B). { A = B; }
assign_op(A) ::= BIT_OR_ASSIGN(B). { A = B; }
assign_op(A) ::= BIT_XOR_ASSIGN(B). { A = B; }
assign_op(A) ::= NULL_ASSIGN(B). { A = B; }

compound_stmt(A) ::= if_stmt(B). { A = B; }
compound_stmt(A) ::= while_stmt(B). { A = B; }
compound_stmt(A) ::= do_while_stmt(B). { A = B; }
compound_stmt(A) ::= for_stmt(B). { A = B; }
compound_stmt(A) ::= match_stmt(B). { A = B; }
compound_stmt(A) ::= try_stmt(B). { A = B; }

var_type_ref(A) ::= type_expr(T). { A = T; }

var_decl(A) ::= var_type_ref(T) name_tok(N). {
    A = xl6AstNew(XL6_AST_VAR_DECL, &N);
    xl6AstAddChild(A, T);
}
var_decl(A) ::= var_type_ref(T) name_tok(N) ASSIGN expr(E). {
    A = xl6AstNew(XL6_AST_VAR_DECL, &N);
    xl6AstAddChild(A, T);
    xl6AstAddChild(A, E);
}
var_decl(A) ::= KW_AUTO(N) name_tok(V) ASSIGN expr(E). {
    A = xl6AstNew(XL6_AST_VAR_DECL, &V);
    xl6AstAddChild(A, xl6AstNewText(XL6_AST_TYPE, "auto", 4, &N));
    xl6AstAddChild(A, E);
}
var_decl(A) ::= KW_AUTO(N) multi_name_list(L) ASSIGN expr(E). {
    A = xl6AstNew(XL6_AST_MULTI_VAR_DECL, &N);
    xl6AstAddChild(A, L);
    xl6AstAddChild(A, E);
}
var_decl(A) ::= KW_CONST var_type_ref(T) name_tok(V) ASSIGN expr(E). {
    A = xl6AstNew(XL6_AST_CONST_DECL, &V);
    xl6AstAddChild(A, T);
    xl6AstAddChild(A, E);
}

multi_name_list(A) ::= name_tok(F) COMMA name_tok(S). {
    A = xl6PList();
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &F));
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &S));
}
multi_name_list(A) ::= multi_name_list(B) COMMA name_tok(N). {
    A = B;
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &N));
}

multi_assign_targets(A) ::= postfix_expr(F) COMMA postfix_expr(S). {
    A = xl6PList();
    xl6AstAddChild(A, F);
    xl6AstAddChild(A, S);
}
multi_assign_targets(A) ::= multi_assign_targets(B) COMMA postfix_expr(T). {
    A = B;
    xl6AstAddChild(A, T);
}

return_stmt(A) ::= KW_RETURN(R). {
    A = xl6AstNew(XL6_AST_RETURN, &R);
}
return_stmt(A) ::= KW_RETURN(R) expr_list(L). {
    A = xl6AstNew(XL6_AST_RETURN, &R);
    xl6AstMoveChildren(A, L);
    xl6AstFree(L);
}

yield_stmt(A) ::= KW_YIELD(Y) expr(E). {
    A = xl6AstNew(XL6_AST_YIELD, &Y);
    xl6AstAddChild(A, E);
}

break_stmt(A) ::= KW_BREAK(B). {
    A = xl6AstNew(XL6_AST_BREAK, &B);
}
break_stmt(A) ::= KW_BREAK(B) INT(N). {
    A = xl6AstNew(XL6_AST_BREAK, &B);
    xl6AstAddChild(A, xl6AstNew(XL6_AST_INT, &N));
}

continue_stmt(A) ::= KW_CONTINUE(C). {
    A = xl6AstNew(XL6_AST_CONTINUE, &C);
}
continue_stmt(A) ::= KW_CONTINUE(C) INT(N). {
    A = xl6AstNew(XL6_AST_CONTINUE, &C);
    xl6AstAddChild(A, xl6AstNew(XL6_AST_INT, &N));
}

throw_stmt(A) ::= KW_THROW(T). {
    A = xl6AstNew(XL6_AST_THROW, &T);
}
throw_stmt(A) ::= KW_THROW(T) expr(E). {
    A = xl6AstNew(XL6_AST_THROW, &T);
    xl6AstAddChild(A, E);
}
throw_stmt(A) ::= KW_THROW(T) name_tok(K) expr(E). {
    A = xl6AstNew(XL6_AST_THROW, &T);
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &K));
    xl6AstAddChild(A, E);
}

assert_stmt(A) ::= KW_ASSERT(T) expr(C). {
    A = xl6AstNew(XL6_AST_ASSERT, &T);
    xl6AstAddChild(A, C);
}
assert_stmt(A) ::= KW_ASSERT(T) expr(C) COMMA expr(M). {
    A = xl6AstNew(XL6_AST_ASSERT, &T);
    xl6AstAddChild(A, C);
    xl6AstAddChild(A, M);
}

if_stmt(A) ::= KW_IF(I) expr(C) if_sep block(B) elseif_list(EL) else_opt(EO) KW_END. {
    A = xl6AstNew(XL6_AST_IF, &I);
    xl6AstAddChild(A, C);
    xl6AstAddChild(A, B);
    xl6AstAddChild(A, EL);
    xl6AstAddChild(A, EO);
}

if_stmt(A) ::= KW_IF(I) expr(C) KW_THEN inline_stmt(S) KW_END. [KW_THEN] {
    A = xl6AstNew(XL6_AST_IF, &I);
    xl6AstAddChild(A, C);
    xl6AstAddChild(A, xl6PBlockWith(S));
    xl6AstAddChild(A, xl6PList());
    xl6AstAddChild(A, xl6AstNew(XL6_AST_BLOCK, NULL));
}

if_stmt(A) ::= KW_IF(I) expr(C) KW_THEN inline_stmt(T) KW_ELSE inline_stmt(E) KW_END. {
    A = xl6AstNew(XL6_AST_IF, &I);
    xl6AstAddChild(A, C);
    xl6AstAddChild(A, xl6PBlockWith(T));
    xl6AstAddChild(A, xl6PList());
    xl6AstAddChild(A, xl6PBlockWith(E));
}

inline_stmt(A) ::= assign_stmt(B). { A = B; }
inline_stmt(A) ::= return_stmt(B). { A = B; }
inline_stmt(A) ::= yield_stmt(B). { A = B; }
inline_stmt(A) ::= throw_stmt(B). { A = B; }
inline_stmt(A) ::= assert_stmt(B). { A = B; }
inline_stmt(A) ::= break_stmt(B). { A = B; }
inline_stmt(A) ::= continue_stmt(B). { A = B; }
inline_stmt(A) ::= expr(E). {
    A = xl6AstNew(XL6_AST_EXPR_STMT, NULL);
    xl6AstAddChild(A, E);
}

if_sep ::= need_sep.
if_sep ::= KW_THEN need_sep.

elseif_list(A) ::= . {
    A = xl6PList();
}
elseif_list(A) ::= elseif_list(B) elseif_item(C). {
    A = B;
    xl6AstAddChild(A, C);
}

elseif_item(A) ::= KW_ELSEIF(E) expr(C) if_sep block(B). {
    A = xl6AstNew(XL6_AST_ELSEIF, &E);
    xl6AstAddChild(A, C);
    xl6AstAddChild(A, B);
}

else_opt(A) ::= . {
    A = xl6AstNew(XL6_AST_BLOCK, NULL);
}
else_opt(A) ::= KW_ELSE need_sep block(B). {
    A = B;
}

while_stmt(A) ::= KW_WHILE(W) expr(C) need_sep block(B) KW_END. {
    A = xl6AstNew(XL6_AST_WHILE, &W);
    xl6AstAddChild(A, C);
    xl6AstAddChild(A, B);
}

do_while_stmt(A) ::= KW_DO(D) need_sep block(B) POST_WHILE expr(C) KW_END need_sep. {
    A = xl6AstNew(XL6_AST_DO_WHILE, &D);
    xl6AstAddChild(A, B);
    xl6AstAddChild(A, C);
}

for_stmt(A) ::= KW_FOR(F) for_targets(T) KW_IN expr(E) need_sep block(B) KW_END. {
    A = xl6AstNew(XL6_AST_FOR, &F);
    xl6AstAddChild(A, T);
    xl6AstAddChild(A, E);
    xl6AstAddChild(A, B);
}
for_stmt(A) ::= KW_FOR(F) KW_AWAIT for_targets(T) KW_IN expr(E) need_sep block(B) KW_END. {
    A = xl6AstNew(XL6_AST_FOR, &F);
    A->flags |= XL6_AST_FLAG_ASYNC;
    xl6AstAddChild(A, T);
    xl6AstAddChild(A, E);
    xl6AstAddChild(A, B);
}

for_targets(A) ::= name_tok(N). {
    A = xl6AstNew(XL6_AST_FOR_TARGETS, &N);
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &N));
}
for_targets(A) ::= name_tok(K) COMMA name_tok(V). {
    A = xl6AstNew(XL6_AST_FOR_TARGETS, &K);
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &K));
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &V));
}

match_stmt(A) ::= STMT_MATCH(M) expr(E) need_sep match_stmt_cases(C) KW_END. {
    A = xl6AstNew(XL6_AST_MATCH, &M);
    xl6AstAddChild(A, E);
    xl6AstAddChild(A, C);
}

match_stmt_cases(A) ::= match_stmt_case(C). {
    A = xl6PList();
    xl6AstAddChild(A, C);
}
match_stmt_cases(A) ::= match_stmt_cases(L) match_stmt_case(C). {
    A = L;
    xl6AstAddChild(A, C);
}

match_stmt_case(A) ::= KW_CASE(C) match_pattern(P) need_sep block(B). {
    A = xl6AstNew(XL6_AST_MATCH_CASE, &C);
    xl6AstAddChild(A, P);
    xl6AstAddChild(A, B);
}
match_stmt_case(A) ::= KW_ELSE(E) need_sep block(B). {
    A = xl6AstNew(XL6_AST_MATCH_CASE, &E);
    xl6AstAddChild(A, xl6AstNew(XL6_AST_PATTERN_ELSE, &E));
    xl6AstAddChild(A, B);
}

match_pattern(A) ::= name_tok(N). {
    A = xl6AstNew(XL6_AST_PATTERN_VARIANT, &N);
}
match_pattern(A) ::= name_tok(N) LPAREN match_bindings_opt(B) RPAREN. {
    A = xl6AstNew(XL6_AST_PATTERN_VARIANT, &N);
    xl6AstMoveChildren(A, B);
    xl6AstFree(B);
}
match_pattern(A) ::= INT(N). {
    A = xl6AstNew(XL6_AST_PATTERN_LITERAL, &N);
    xl6AstAddChild(A, xl6AstNew(XL6_AST_INT, &N));
}
match_pattern(A) ::= FLOAT(N). {
    A = xl6AstNew(XL6_AST_PATTERN_LITERAL, &N);
    xl6AstAddChild(A, xl6AstNew(XL6_AST_FLOAT, &N));
}
match_pattern(A) ::= STRING(N). {
    A = xl6AstNew(XL6_AST_PATTERN_LITERAL, &N);
    xl6AstAddChild(A, xl6PParseLiteral(ctx, &N, XL6_AST_STRING));
}
match_pattern(A) ::= KW_TRUE(N). {
    A = xl6AstNew(XL6_AST_PATTERN_LITERAL, &N);
    xl6AstAddChild(A, xl6AstNewText(XL6_AST_BOOL, "true", 4, &N));
}
match_pattern(A) ::= KW_FALSE(N). {
    A = xl6AstNew(XL6_AST_PATTERN_LITERAL, &N);
    xl6AstAddChild(A, xl6AstNewText(XL6_AST_BOOL, "false", 5, &N));
}
match_pattern(A) ::= KW_NULL(N). {
    A = xl6AstNew(XL6_AST_PATTERN_LITERAL, &N);
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NULL, &N));
}

match_bindings_opt(A) ::= . {
    A = xl6PList();
}
match_bindings_opt(A) ::= match_bindings(B). {
    A = B;
}
match_bindings(A) ::= name_tok(N). {
    A = xl6PList();
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &N));
}
match_bindings(A) ::= match_bindings(B) COMMA name_tok(N). {
    A = B;
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &N));
}

try_stmt(A) ::= KW_TRY(T) need_sep block(B) catch_opt(C) finally_opt(F) KW_END. {
    if ( C == NULL && F == NULL ) {
        xl6PDiag(ctx, &T, "try 必须包含 catch 或 finally");
    }
    A = xl6AstNew(XL6_AST_TRY, &T);
    xl6AstAddChild(A, B);
    xl6AstAddChild(A, C != NULL ? C : xl6PList());
    xl6AstAddChild(A, F != NULL ? F : xl6AstNew(XL6_AST_BLOCK, NULL));
}

catch_opt(A) ::= . { A = NULL; }
catch_opt(A) ::= catch_list(L). { A = L; }

catch_list(A) ::= catch_clause(C). {
    A = xl6PList();
    xl6AstAddChild(A, C);
}
catch_list(A) ::= catch_list(L) catch_clause(C). {
    A = L;
    xl6AstAddChild(A, C);
}

catch_clause(A) ::= KW_CATCH(C) name_tok(N) need_sep block(B). {
    A = xl6AstNew(XL6_AST_CATCH, &C);
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &N));
    xl6AstAddChild(A, B);
}
catch_clause(A) ::= KW_CATCH(C) name_tok(K) name_tok(N) need_sep block(B). {
    A = xl6AstNew(XL6_AST_CATCH, &C);
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &K));
    xl6AstAddChild(A, xl6AstNew(XL6_AST_NAME, &N));
    xl6AstAddChild(A, B);
}

finally_opt(A) ::= . { A = NULL; }
finally_opt(A) ::= KW_FINALLY(F) need_sep block(B). {
    A = B;
    (void)F;
}

expr_list(A) ::= expr(E). {
    A = xl6PList();
    xl6AstAddChild(A, E);
}
expr_list(A) ::= expr_list(B) COMMA expr(E). {
    A = B;
    xl6AstAddChild(A, E);
}

expr(A) ::= conditional_expr(B). { A = B; }

conditional_expr(A) ::= null_expr(B). [QUESTION] { A = B; }
conditional_expr(A) ::= null_expr(C) QUESTION(Q) expr(T) COLON conditional_expr(F). {
    A = xl6AstNew(XL6_AST_CONDITIONAL, &Q);
    xl6AstAddChild(A, C);
    xl6AstAddChild(A, T);
    xl6AstAddChild(A, F);
}

null_expr(A) ::= or_expr(B). [NULL_COALESCE] { A = B; }
null_expr(A) ::= null_expr(B) NULL_COALESCE(OP) or_expr(C). { A = xl6AstNewBinary(&OP, B, C); }

or_expr(A) ::= and_expr(B). [KW_OR] { A = B; }
or_expr(A) ::= or_expr(B) KW_OR(OP) and_expr(C). { A = xl6AstNewBinary(&OP, B, C); }

and_expr(A) ::= bit_or_expr(B). [KW_AND] { A = B; }
and_expr(A) ::= and_expr(B) KW_AND(OP) bit_or_expr(C). { A = xl6AstNewBinary(&OP, B, C); }

bit_or_expr(A) ::= bit_xor_expr(B). [BIT_OR] { A = B; }
bit_or_expr(A) ::= bit_or_expr(B) BIT_OR(OP) bit_xor_expr(C). { A = xl6AstNewBinary(&OP, B, C); }

bit_xor_expr(A) ::= bit_and_expr(B). [BIT_XOR] { A = B; }
bit_xor_expr(A) ::= bit_xor_expr(B) BIT_XOR(OP) bit_and_expr(C). { A = xl6AstNewBinary(&OP, B, C); }

bit_and_expr(A) ::= equality_expr(B). [BIT_AND] { A = B; }
bit_and_expr(A) ::= bit_and_expr(B) BIT_AND(OP) equality_expr(C). { A = xl6AstNewBinary(&OP, B, C); }

equality_expr(A) ::= compare_expr(B). [EQ] { A = B; }
equality_expr(A) ::= equality_expr(B) EQ(OP) compare_expr(C). { A = xl6AstNewBinary(&OP, B, C); }
equality_expr(A) ::= equality_expr(B) NE(OP) compare_expr(C). { A = xl6AstNewBinary(&OP, B, C); }
equality_expr(A) ::= equality_expr(B) APPROX(OP) compare_expr(C). { A = xl6AstNewBinary(&OP, B, C); }

compare_expr(A) ::= shift_expr(B). [LT] { A = B; }
compare_expr(A) ::= compare_expr(B) LT(OP) shift_expr(C). { A = xl6AstNewBinary(&OP, B, C); }
compare_expr(A) ::= compare_expr(B) LE(OP) shift_expr(C). { A = xl6AstNewBinary(&OP, B, C); }
compare_expr(A) ::= compare_expr(B) GT(OP) shift_expr(C). { A = xl6AstNewBinary(&OP, B, C); }
compare_expr(A) ::= compare_expr(B) GE(OP) shift_expr(C). { A = xl6AstNewBinary(&OP, B, C); }
compare_expr(A) ::= compare_expr(B) KW_IN(OP) shift_expr(C). { A = xl6AstNewBinary(&OP, B, C); }
compare_expr(A) ::= compare_expr(B) KW_IS(OP) type_expr(T). {
    A = xl6AstNew(XL6_AST_TYPE_IS, &OP);
    xl6AstAddChild(A, B);
    xl6AstAddChild(A, T);
}
compare_expr(A) ::= compare_expr(B) KW_AS(OP) type_expr(T). {
    A = xl6AstNew(XL6_AST_TYPE_AS, &OP);
    xl6AstAddChild(A, B);
    xl6AstAddChild(A, T);
}

shift_expr(A) ::= add_expr(B). [SHL] { A = B; }
shift_expr(A) ::= shift_expr(B) SHL(OP) add_expr(C). { A = xl6AstNewBinary(&OP, B, C); }
shift_expr(A) ::= shift_expr(B) SHR(OP) add_expr(C). { A = xl6AstNewBinary(&OP, B, C); }

add_expr(A) ::= mul_expr(B). [PLUS] { A = B; }
add_expr(A) ::= add_expr(B) PLUS(OP) mul_expr(C). { A = xl6AstNewBinary(&OP, B, C); }
add_expr(A) ::= add_expr(B) MINUS(OP) mul_expr(C). { A = xl6AstNewBinary(&OP, B, C); }

mul_expr(A) ::= pow_expr(B). [STAR] { A = B; }
mul_expr(A) ::= mul_expr(B) STAR(OP) pow_expr(C). { A = xl6AstNewBinary(&OP, B, C); }
mul_expr(A) ::= mul_expr(B) SLASH(OP) pow_expr(C). { A = xl6AstNewBinary(&OP, B, C); }
mul_expr(A) ::= mul_expr(B) PERCENT(OP) pow_expr(C). { A = xl6AstNewBinary(&OP, B, C); }

pow_expr(A) ::= unary_expr(B). [POW] { A = B; }
pow_expr(A) ::= unary_expr(B) POW(OP) pow_expr(C). { A = xl6AstNewBinary(&OP, B, C); }

unary_expr(A) ::= postfix_expr(B). [KW_NOT] { A = B; }
unary_expr(A) ::= PLUS(OP) unary_expr(B). { A = xl6AstNewUnary(&OP, B); }
unary_expr(A) ::= MINUS(OP) unary_expr(B). { A = xl6AstNewUnary(&OP, B); }
unary_expr(A) ::= KW_NOT(OP) unary_expr(B). { A = xl6AstNewUnary(&OP, B); }
unary_expr(A) ::= BANG(OP) unary_expr(B). { A = xl6AstNewUnary(&OP, B); }
unary_expr(A) ::= BIT_NOT(OP) unary_expr(B). { A = xl6AstNewUnary(&OP, B); }
unary_expr(A) ::= KW_AWAIT(OP) unary_expr(B). {
    A = xl6AstNew(XL6_AST_AWAIT, &OP);
    xl6AstAddChild(A, B);
}

postfix_expr(A) ::= atom(B). { A = B; }
postfix_expr(A) ::= postfix_expr(C) LPAREN(P) call_args_opt(L) RPAREN. {
    A = xl6AstNewCall(C, L, &P);
}
postfix_expr(A) ::= postfix_expr(C) SCOPE(S) LT type_list(T) GT. {
    A = xl6AstNew(XL6_AST_GENERIC_INSTANT, &S);
    xl6AstAddChild(A, C);
    xl6AstAddChild(A, T);
}
postfix_expr(A) ::= postfix_expr(C) DOT member_name_tok(M). {
    A = xl6AstNew(XL6_AST_MEMBER, &M);
    xl6AstAddChild(A, C);
}
postfix_expr(A) ::= postfix_expr(C) SAFE_DOT member_name_tok(M). {
    A = xl6AstNew(XL6_AST_SAFE_MEMBER, &M);
    xl6AstAddChild(A, C);
}
postfix_expr(A) ::= postfix_expr(C) LBRACKET(B) expr_list(I) RBRACKET. {
    A = xl6AstNew(XL6_AST_INDEX, &B);
    xl6AstAddChild(A, C);
    xl6AstAddChild(A, xl6PIndexPayloadFromList(I));
}
postfix_expr(A) ::= postfix_expr(C) SAFE_LBRACKET(B) expr_list(I) RBRACKET. {
    A = xl6AstNew(XL6_AST_SAFE_INDEX, &B);
    xl6AstAddChild(A, C);
    xl6AstAddChild(A, xl6PIndexPayloadFromList(I));
}

atom(A) ::= name_tok(N). [KW_NOT] { A = xl6AstNew(XL6_AST_NAME, &N); }
atom(A) ::= TYPE_FUNCTION(N). [KW_NOT] { A = xl6AstNewText(XL6_AST_NAME, "function", 8, &N); }
atom(A) ::= INT(N). { A = xl6AstNew(XL6_AST_INT, &N); }
atom(A) ::= FLOAT(N). { A = xl6AstNew(XL6_AST_FLOAT, &N); }
atom(A) ::= STRING(N). { A = xl6PParseLiteral(ctx, &N, XL6_AST_STRING); }
atom(A) ::= CHAR(N). { A = xl6PParseLiteral(ctx, &N, XL6_AST_CHAR); }
atom(A) ::= BYTES(N). { A = xl6PParseLiteral(ctx, &N, XL6_AST_BYTES); }
atom(A) ::= FSTRING(N). { A = xl6PParseFString(ctx, &N); }
atom(A) ::= TIME(N). { A = xl6AstNew(XL6_AST_TIME, &N); }
atom(A) ::= KW_TRUE(N). { A = xl6AstNewText(XL6_AST_BOOL, "true", 4, &N); }
atom(A) ::= KW_FALSE(N). { A = xl6AstNewText(XL6_AST_BOOL, "false", 5, &N); }
atom(A) ::= KW_NULL(N). { A = xl6AstNew(XL6_AST_NULL, &N); }
atom(A) ::= match_expr(B). { A = B; }
atom(A) ::= anon_func(B). { A = B; }
atom(A) ::= LPAREN expr(E) RPAREN. { A = E; }
atom(A) ::= bracket_literal(B). { A = B; }
atom(A) ::= brace_literal(B). { A = B; }
atom(A) ::= name_tok(T) bracket_literal(B). {
    A = xl6PBracketAfterName(&T, B);
}
atom(A) ::= name_tok(T) brace_literal(B). {
    A = xl6PBraceAfterName(&T, B);
}
atom(A) ::= name_tok(T) LT type_list(L) GT bracket_literal(B). {
    A = xl6PTypedLiteralWithType(&T, xl6AstNewGenericType(&T, L), B);
}
atom(A) ::= name_tok(T) TYPE_LT type_list(L) GT bracket_literal(B). {
    A = xl6PTypedLiteralWithType(&T, xl6AstNewGenericType(&T, L), B);
}
atom(A) ::= name_tok(T) LT type_list(L) GT brace_literal(B). {
    A = xl6PTypedLiteralWithType(&T, xl6AstNewGenericType(&T, L), B);
}
atom(A) ::= name_tok(T) TYPE_LT type_list(L) GT brace_literal(B). {
    A = xl6PTypedLiteralWithType(&T, xl6AstNewGenericType(&T, L), B);
}

match_expr(A) ::= KW_MATCH(M) expr(E) need_sep match_expr_cases(C) KW_END. {
    A = xl6AstNew(XL6_AST_MATCH, &M);
    A->flags |= XL6_AST_FLAG_MATCH_EXPR;
    xl6AstAddChild(A, E);
    xl6AstAddChild(A, C);
}

match_expr_cases(A) ::= match_expr_case(C). {
    A = xl6PList();
    xl6AstAddChild(A, C);
}
match_expr_cases(A) ::= match_expr_cases(L) match_expr_case(C). {
    A = L;
    xl6AstAddChild(A, C);
}

match_expr_case(A) ::= KW_CASE(C) match_pattern(P) KW_THEN expr(E) need_sep. {
    A = xl6AstNew(XL6_AST_MATCH_CASE, &C);
    xl6AstAddChild(A, P);
    xl6AstAddChild(A, E);
}
match_expr_case(A) ::= KW_ELSE(E) KW_THEN expr(V) need_sep. {
    A = xl6AstNew(XL6_AST_MATCH_CASE, &E);
    xl6AstAddChild(A, xl6AstNew(XL6_AST_PATTERN_ELSE, &E));
    xl6AstAddChild(A, V);
}

anon_func(A) ::= KW_FUNC(F) name_params(P) return_sig(R) need_sep block(B) KW_END. {
    A = xl6AstNew(XL6_AST_ANON_FUNC, &F);
    xl6AstAddChild(A, P);
    xl6AstAddChild(A, R);
    xl6AstAddChild(A, B);
}
anon_func(A) ::= KW_FUNC(F) name_params(P) return_sig(R) FAT_ARROW expr(E). {
    A = xl6AstNew(XL6_AST_ANON_FUNC, &F);
    xl6AstAddChild(A, P);
    xl6AstAddChild(A, R);
    xl6AstAddChild(A, xl6PReturnBlock(&F, E));
}

bracket_literal(A) ::= LBRACKET(L) RBRACKET. {
    A = xl6AstNew(XL6_AST_ARRAY_LITERAL, &L);
}
bracket_literal(A) ::= LBRACKET(L) value_list(V) RBRACKET. {
    A = xl6AstNew(XL6_AST_ARRAY_LITERAL, &L);
    xl6AstMoveChildren(A, V);
    xl6AstFree(V);
}
bracket_literal(A) ::= LBRACKET(L) pair_list(P) RBRACKET. {
    A = xl6AstNew(XL6_AST_LIST_LITERAL, &L);
    xl6AstMoveChildren(A, P);
    xl6AstFree(P);
}

brace_literal(A) ::= LBRACE(L) RBRACE. {
    A = xl6AstNew(XL6_AST_DICT_LITERAL, &L);
}
brace_literal(A) ::= LBRACE(L) value_list(V) RBRACE. {
    A = xl6AstNew(XL6_AST_SET_LITERAL, &L);
    xl6AstMoveChildren(A, V);
    xl6AstFree(V);
}
brace_literal(A) ::= LBRACE(L) pair_list(P) RBRACE. {
    A = xl6AstNew(XL6_AST_DICT_LITERAL, &L);
    xl6AstMoveChildren(A, P);
    xl6AstFree(P);
}

value_list(A) ::= expr(E). {
    A = xl6PList();
    xl6AstAddChild(A, E);
}
value_list(A) ::= value_list(B) COMMA expr(E). {
    A = B;
    xl6AstAddChild(A, E);
}
value_list(A) ::= value_list(B) COMMA. {
    A = B;
}

pair_list(A) ::= pair(P). {
    A = xl6PList();
    xl6AstAddChild(A, P);
}
pair_list(A) ::= pair_list(B) COMMA pair(P). {
    A = B;
    xl6AstAddChild(A, P);
}
pair_list(A) ::= pair_list(B) COMMA. {
    A = B;
}

pair(A) ::= expr(K) COLON expr(V). {
    A = xl6AstNew(XL6_AST_KEY_VALUE, NULL);
    xl6AstAddChild(A, K);
    xl6AstAddChild(A, V);
}

call_args_opt(A) ::= . { A = xl6PList(); }
call_args_opt(A) ::= call_args(B). { A = B; }

call_args(A) ::= call_arg(B). {
    A = xl6PList();
    xl6AstAddChild(A, B);
}
call_args(A) ::= call_args(B) COMMA call_arg(C). {
    A = B;
    xl6AstAddChild(A, C);
}

call_arg(A) ::= expr(E). {
    A = xl6AstNew(XL6_AST_ARG, NULL);
    xl6AstAddChild(A, E);
}
call_arg(A) ::= name_tok(N) ASSIGN expr(E). {
    A = xl6AstNew(XL6_AST_NAMED_ARG, &N);
    xl6AstAddChild(A, E);
}

func_type_param(A) ::= KW_C_VARARG(T). {
    A = xl6AstNewText(XL6_AST_TYPE, "c_vararg", 8, &T);
}
