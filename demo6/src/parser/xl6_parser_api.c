#include "src/parser/xl6_parser_api.h"
#include "src/lexer/xl6_lexer_api.h"
#include "src/parser/xl6_parser.h"

void* xl6ParseAlloc(void* (*mallocProc)(size_t));
void xl6Parse(void* parser, int tokenKind, XL6_Token token, XL6_ParseContext* ctx);
void xl6ParseFree(void* parser, void (*freeProc)(void*));

void xl6ParseResultInit(XL6_ParseResult* result)
{
    memset(result, 0, sizeof(*result));
    xl6DiagInit(&result->diags);
}

void xl6ParseResultFree(XL6_ParseResult* result)
{
    if ( result == NULL ) {
        return;
    }
    xl6AstFree(result->root);
    xl6DiagFree(&result->diags);
    memset(result, 0, sizeof(*result));
}

static int xl6ParserTokenKind(const XL6_Token* token)
{
    XL6_TokenKind kind;

    if ( token == NULL ) {
        return XL6PG_TOK_ERROR;
    }

    kind = token->kind;
    if ( kind == XL6_TK_IDENT &&
         token->text != NULL &&
         token->len == 8 &&
         strncmp(token->text, "function", 8) == 0 ) {
        return XL6PG_TYPE_FUNCTION;
    }

    switch ( kind ) {
    case XL6_TK_IDENT: return XL6PG_IDENT;
    case XL6_TK_INT: return XL6PG_INT;
    case XL6_TK_FLOAT: return XL6PG_FLOAT;
    case XL6_TK_STRING: return XL6PG_STRING;
    case XL6_TK_TEMPLATE_STRING: return XL6PG_TEMPLATE_STRING;
    case XL6_TK_TIME: return XL6PG_TIME;
    case XL6_TK_DOC: return XL6PG_DOC;
    case XL6_TK_SEP: return XL6PG_SEP;
    case XL6_KW_TRY: return XL6PG_KW_TRY;
    case XL6_KW_CATCH: return XL6PG_KW_CATCH;
    case XL6_KW_FINALLY: return XL6PG_KW_FINALLY;
    case XL6_KW_THROW: return XL6PG_KW_THROW;
    case XL6_KW_ASSERT: return XL6PG_KW_ASSERT;
    case XL6_KW_NULL: return XL6PG_KW_NULL;
    case XL6_KW_TRUE: return XL6PG_KW_TRUE;
    case XL6_KW_FALSE: return XL6PG_KW_FALSE;
    case XL6_KW_FUNC: return XL6PG_KW_FUNC;
    case XL6_KW_CONST: return XL6PG_KW_CONST;
    case XL6_KW_VAR: return XL6PG_KW_VAR;
    case XL6_KW_AUTO: return XL6PG_KW_AUTO;
    case XL6_KW_TYPEDEF: return XL6PG_KW_TYPEDEF;
    case XL6_KW_IMPORT: return XL6PG_KW_IMPORT;
    case XL6_KW_EXTERN: return XL6PG_KW_EXTERN;
    case XL6_KW_EXPORT: return XL6PG_KW_EXPORT;
    case XL6_KW_CLASS: return XL6PG_KW_CLASS;
    case XL6_KW_CUSTOM: return XL6PG_KW_CUSTOM;
    case XL6_KW_EXTENDS: return XL6PG_KW_EXTENDS;
    case XL6_KW_UNION: return XL6PG_KW_UNION;
    case XL6_KW_RETURN: return XL6PG_KW_RETURN;
    case XL6_KW_END: return XL6PG_KW_END;
    case XL6_KW_IF: return XL6PG_KW_IF;
    case XL6_KW_THEN: return XL6PG_KW_THEN;
    case XL6_KW_ELSE: return XL6PG_KW_ELSE;
    case XL6_KW_ELSEIF: return XL6PG_KW_ELSEIF;
    case XL6_KW_WHILE: return XL6PG_KW_WHILE;
    case XL6_KW_DO: return XL6PG_KW_DO;
    case XL6_KW_FOR: return XL6PG_KW_FOR;
    case XL6_KW_BREAK: return XL6PG_KW_BREAK;
    case XL6_KW_CONTINUE: return XL6PG_KW_CONTINUE;
    case XL6_KW_ASYNC: return XL6PG_KW_ASYNC;
    case XL6_KW_AWAIT: return XL6PG_KW_AWAIT;
    case XL6_KW_YIELD: return XL6PG_KW_YIELD;
    case XL6_KW_AND: return XL6PG_KW_AND;
    case XL6_KW_OR: return XL6PG_KW_OR;
    case XL6_KW_NOT: return XL6PG_KW_NOT;
    case XL6_KW_IN: return XL6PG_KW_IN;
    case XL6_TK_LPAREN: return XL6PG_LPAREN;
    case XL6_TK_RPAREN: return XL6PG_RPAREN;
    case XL6_TK_LBRACKET: return XL6PG_LBRACKET;
    case XL6_TK_RBRACKET: return XL6PG_RBRACKET;
    case XL6_TK_LBRACE: return XL6PG_LBRACE;
    case XL6_TK_RBRACE: return XL6PG_RBRACE;
    case XL6_TK_COMMA: return XL6PG_COMMA;
    case XL6_TK_COLON: return XL6PG_COLON;
    case XL6_TK_DOT: return XL6PG_DOT;
    case XL6_TK_SCOPE: return XL6PG_SCOPE;
    case XL6_TK_LT: return XL6PG_LT;
    case XL6_TK_GT: return XL6PG_GT;
    case XL6_TK_ASSIGN: return XL6PG_ASSIGN;
    case XL6_TK_PLUS_ASSIGN: return XL6PG_PLUS_ASSIGN;
    case XL6_TK_MINUS_ASSIGN: return XL6PG_MINUS_ASSIGN;
    case XL6_TK_STAR_ASSIGN: return XL6PG_STAR_ASSIGN;
    case XL6_TK_SLASH_ASSIGN: return XL6PG_SLASH_ASSIGN;
    case XL6_TK_PERCENT_ASSIGN: return XL6PG_PERCENT_ASSIGN;
    case XL6_TK_POW_ASSIGN: return XL6PG_POW_ASSIGN;
    case XL6_TK_NULL_ASSIGN: return XL6PG_NULL_ASSIGN;
    case XL6_TK_ARROW: return XL6PG_ARROW;
    case XL6_TK_FAT_ARROW: return XL6PG_FAT_ARROW;
    case XL6_TK_ELLIPSIS: return XL6PG_ELLIPSIS;
    case XL6_TK_TRIPLE_STAR: return XL6PG_TRIPLE_STAR;
    case XL6_TK_PLUS: return XL6PG_PLUS;
    case XL6_TK_MINUS: return XL6PG_MINUS;
    case XL6_TK_STAR: return XL6PG_STAR;
    case XL6_TK_SLASH: return XL6PG_SLASH;
    case XL6_TK_PERCENT: return XL6PG_PERCENT;
    case XL6_TK_POW: return XL6PG_POW;
    case XL6_TK_EQ: return XL6PG_EQ;
    case XL6_TK_NE: return XL6PG_NE;
    case XL6_TK_LE: return XL6PG_LE;
    case XL6_TK_GE: return XL6PG_GE;
    case XL6_TK_APPROX: return XL6PG_APPROX;
    case XL6_TK_QUESTION: return XL6PG_QUESTION;
    case XL6_TK_NULL_COALESCE: return XL6PG_NULL_COALESCE;
    case XL6_TK_SAFE_DOT: return XL6PG_SAFE_DOT;
    case XL6_TK_SAFE_LBRACKET: return XL6PG_SAFE_LBRACKET;
    case XL6_TK_BANG: return XL6PG_BANG;
    case XL6_TK_BIT_NOT: return XL6PG_BIT_NOT;
    case XL6_TK_BIT_AND: return XL6PG_BIT_AND;
    case XL6_TK_BIT_OR: return XL6PG_BIT_OR;
    case XL6_TK_BIT_XOR: return XL6PG_BIT_XOR;
    case XL6_TK_SHL: return XL6PG_SHL;
    case XL6_TK_SHR: return XL6PG_SHR;
    default: return XL6PG_TOK_ERROR;
    }
}

typedef struct XL6_ParseTokenStream {
    XL6_Token* tokens;
    int* parserKinds;
    size_t count;
    size_t capacity;
} XL6_ParseTokenStream;

static void xl6ParseTokenStreamInit(XL6_ParseTokenStream* stream)
{
    memset(stream, 0, sizeof(*stream));
}

static void xl6ParseTokenStreamFree(XL6_ParseTokenStream* stream)
{
    if ( stream == NULL ) {
        return;
    }
    xrtFree(stream->tokens);
    xrtFree(stream->parserKinds);
    memset(stream, 0, sizeof(*stream));
}

static int xl6ParseTokenStreamAppend(XL6_ParseTokenStream* stream, const XL6_Token* token)
{
    XL6_Token* newTokens;
    int* newParserKinds;
    size_t newCapacity;

    if ( stream == NULL || token == NULL ) {
        return 0;
    }

    if ( stream->count == stream->capacity ) {
        newCapacity = (stream->capacity == 0) ? 128 : stream->capacity * 2;
        newTokens = (XL6_Token*)xrtRealloc(stream->tokens, newCapacity * sizeof(XL6_Token));
        if ( newTokens == NULL ) {
            return 0;
        }
        stream->tokens = newTokens;

        newParserKinds = (int*)xrtRealloc(stream->parserKinds, newCapacity * sizeof(int));
        if ( newParserKinds == NULL ) {
            return 0;
        }
        stream->parserKinds = newParserKinds;
        stream->capacity = newCapacity;
    }

    stream->tokens[stream->count] = *token;
    stream->parserKinds[stream->count] = xl6ParserTokenKind(token);
    stream->count += 1;
    return 1;
}

static int xl6ParserIsNameToken(const XL6_Token* token)
{
    int kind;

    kind = xl6ParserTokenKind(token);
    return kind == XL6PG_IDENT || kind == XL6PG_TYPE_FUNCTION;
}

static int xl6ParserIsStmtStart(const XL6_ParseTokenStream* stream, size_t index)
{
    if ( index == 0 ) {
        return 1;
    }
    return stream->tokens[index - 1].kind == XL6_TK_SEP;
}

static int xl6ParserMatchTypeExpr(const XL6_ParseTokenStream* stream, size_t index, size_t* outNext);
static int xl6ParserMatchTypeRef(const XL6_ParseTokenStream* stream, size_t index, size_t* outNext);
static int xl6ParserMatchFunctionTypeRef(const XL6_ParseTokenStream* stream, size_t index, size_t* outNext);

static int xl6ParserMatchTypeList(const XL6_ParseTokenStream* stream, size_t index, size_t* outNext)
{
    size_t next;

    if ( !xl6ParserMatchTypeExpr(stream, index, &next) ) {
        return 0;
    }

    while ( next < stream->count && stream->tokens[next].kind == XL6_TK_COMMA ) {
        if ( !xl6ParserMatchTypeExpr(stream, next + 1, &next) ) {
            return 0;
        }
    }

    *outNext = next;
    return 1;
}

static int xl6ParserMatchFuncTypeParam(const XL6_ParseTokenStream* stream, size_t index, size_t* outNext)
{
    if ( stream == NULL || index >= stream->count ) {
        return 0;
    }
    if ( stream->tokens[index].kind == XL6_TK_ELLIPSIS ||
         stream->tokens[index].kind == XL6_TK_TRIPLE_STAR ) {
        *outNext = index + 1;
        return 1;
    }
    return xl6ParserMatchTypeExpr(stream, index, outNext);
}

static int xl6ParserMatchFunctionReturnSig(const XL6_ParseTokenStream* stream, size_t index, size_t* outNext)
{
    size_t next;

    if ( stream == NULL || index >= stream->count || stream->tokens[index].kind != XL6_TK_ARROW ) {
        *outNext = index;
        return 1;
    }

    next = index + 1;
    if ( next < stream->count && stream->tokens[next].kind == XL6_TK_LPAREN ) {
        if ( !xl6ParserMatchTypeList(stream, next + 1, &next) ) {
            return 0;
        }
        if ( next >= stream->count || stream->tokens[next].kind != XL6_TK_RPAREN ) {
            return 0;
        }
        *outNext = next + 1;
        return 1;
    }

    return xl6ParserMatchTypeExpr(stream, next, outNext);
}

static int xl6ParserMatchFunctionTypeRef(const XL6_ParseTokenStream* stream, size_t index, size_t* outNext)
{
    size_t next;

    if ( stream == NULL || index + 1 >= stream->count ||
         stream->parserKinds[index] != XL6PG_TYPE_FUNCTION ||
         stream->tokens[index + 1].kind != XL6_TK_LPAREN ) {
        return 0;
    }

    next = index + 2;
    if ( next < stream->count && stream->tokens[next].kind != XL6_TK_RPAREN ) {
        if ( !xl6ParserMatchFuncTypeParam(stream, next, &next) ) {
            return 0;
        }
        while ( next < stream->count && stream->tokens[next].kind == XL6_TK_COMMA ) {
            if ( !xl6ParserMatchFuncTypeParam(stream, next + 1, &next) ) {
                return 0;
            }
        }
    }
    if ( next >= stream->count || stream->tokens[next].kind != XL6_TK_RPAREN ) {
        return 0;
    }

    return xl6ParserMatchFunctionReturnSig(stream, next + 1, outNext);
}

static int xl6ParserMatchTypeRef(const XL6_ParseTokenStream* stream, size_t index, size_t* outNext)
{
    size_t next;

    if ( index >= stream->count || !xl6ParserIsNameToken(&stream->tokens[index]) ) {
        return 0;
    }

    next = index + 1;
    if ( next < stream->count && stream->tokens[next].kind == XL6_TK_LT ) {
        if ( !xl6ParserMatchTypeList(stream, next + 1, &next) ) {
            return 0;
        }
        if ( next >= stream->count || stream->tokens[next].kind != XL6_TK_GT ) {
            return 0;
        }
        next += 1;
    }

    *outNext = next;
    return 1;
}

static int xl6ParserMatchTypeExpr(const XL6_ParseTokenStream* stream, size_t index, size_t* outNext)
{
    if ( xl6ParserMatchFunctionTypeRef(stream, index, outNext) ) {
        return 1;
    }
    return xl6ParserMatchTypeRef(stream, index, outNext);
}

typedef struct XL6_VTokenPos {
    size_t index;
    int secondGt;
} XL6_VTokenPos;

static int xl6ParserVKindAt(const XL6_ParseTokenStream* stream, XL6_VTokenPos pos)
{
    if ( stream == NULL || pos.index >= stream->count ) {
        return XL6_TK_EOF;
    }
    if ( stream->tokens[pos.index].kind == XL6_TK_SHR ) {
        return XL6_TK_GT;
    }
    if ( pos.secondGt ) {
        return XL6_TK_ERROR;
    }
    return stream->tokens[pos.index].kind;
}

static XL6_VTokenPos xl6ParserVNext(const XL6_ParseTokenStream* stream, XL6_VTokenPos pos)
{
    if ( stream != NULL && pos.index < stream->count &&
         stream->tokens[pos.index].kind == XL6_TK_SHR && !pos.secondGt ) {
        pos.secondGt = 1;
        return pos;
    }
    pos.index += 1;
    pos.secondGt = 0;
    return pos;
}

static int xl6ParserVIsNameToken(const XL6_ParseTokenStream* stream, XL6_VTokenPos pos)
{
    if ( stream == NULL || pos.index >= stream->count || pos.secondGt ) {
        return 0;
    }
    return xl6ParserIsNameToken(&stream->tokens[pos.index]);
}

static int xl6ParserVMatchTypeExpr(const XL6_ParseTokenStream* stream, XL6_VTokenPos pos, XL6_VTokenPos* outNext);
static int xl6ParserVMatchTypeRef(const XL6_ParseTokenStream* stream, XL6_VTokenPos pos, XL6_VTokenPos* outNext);
static int xl6ParserVMatchFunctionTypeRef(const XL6_ParseTokenStream* stream, XL6_VTokenPos pos, XL6_VTokenPos* outNext);

static int xl6ParserVMatchTypeList(const XL6_ParseTokenStream* stream, XL6_VTokenPos pos, XL6_VTokenPos* outNext)
{
    XL6_VTokenPos next;

    if ( !xl6ParserVMatchTypeExpr(stream, pos, &next) ) {
        return 0;
    }
    while ( xl6ParserVKindAt(stream, next) == XL6_TK_COMMA ) {
        next = xl6ParserVNext(stream, next);
        if ( !xl6ParserVMatchTypeExpr(stream, next, &next) ) {
            return 0;
        }
    }

    *outNext = next;
    return 1;
}

static int xl6ParserVMatchFuncTypeParam(const XL6_ParseTokenStream* stream, XL6_VTokenPos pos, XL6_VTokenPos* outNext)
{
    int kind;

    kind = xl6ParserVKindAt(stream, pos);
    if ( kind == XL6_TK_ELLIPSIS || kind == XL6_TK_TRIPLE_STAR ) {
        *outNext = xl6ParserVNext(stream, pos);
        return 1;
    }
    return xl6ParserVMatchTypeExpr(stream, pos, outNext);
}

static int xl6ParserVMatchFunctionReturnSig(const XL6_ParseTokenStream* stream, XL6_VTokenPos pos, XL6_VTokenPos* outNext)
{
    XL6_VTokenPos next;

    if ( xl6ParserVKindAt(stream, pos) != XL6_TK_ARROW ) {
        *outNext = pos;
        return 1;
    }

    next = xl6ParserVNext(stream, pos);
    if ( xl6ParserVKindAt(stream, next) == XL6_TK_LPAREN ) {
        next = xl6ParserVNext(stream, next);
        if ( !xl6ParserVMatchTypeList(stream, next, &next) ) {
            return 0;
        }
        if ( xl6ParserVKindAt(stream, next) != XL6_TK_RPAREN ) {
            return 0;
        }
        *outNext = xl6ParserVNext(stream, next);
        return 1;
    }

    return xl6ParserVMatchTypeExpr(stream, next, outNext);
}

static int xl6ParserVMatchFunctionTypeRef(const XL6_ParseTokenStream* stream, XL6_VTokenPos pos, XL6_VTokenPos* outNext)
{
    XL6_VTokenPos next;

    if ( stream == NULL || pos.index >= stream->count || pos.secondGt ||
         stream->parserKinds[pos.index] != XL6PG_TYPE_FUNCTION ) {
        return 0;
    }

    next = xl6ParserVNext(stream, pos);
    if ( xl6ParserVKindAt(stream, next) != XL6_TK_LPAREN ) {
        return 0;
    }

    next = xl6ParserVNext(stream, next);
    if ( xl6ParserVKindAt(stream, next) != XL6_TK_RPAREN ) {
        if ( !xl6ParserVMatchFuncTypeParam(stream, next, &next) ) {
            return 0;
        }
        while ( xl6ParserVKindAt(stream, next) == XL6_TK_COMMA ) {
            next = xl6ParserVNext(stream, next);
            if ( !xl6ParserVMatchFuncTypeParam(stream, next, &next) ) {
                return 0;
            }
        }
    }
    if ( xl6ParserVKindAt(stream, next) != XL6_TK_RPAREN ) {
        return 0;
    }

    return xl6ParserVMatchFunctionReturnSig(stream, xl6ParserVNext(stream, next), outNext);
}

static int xl6ParserVMatchTypeRef(const XL6_ParseTokenStream* stream, XL6_VTokenPos pos, XL6_VTokenPos* outNext)
{
    XL6_VTokenPos next;
    int kind;

    if ( !xl6ParserVIsNameToken(stream, pos) ) {
        return 0;
    }

    next = xl6ParserVNext(stream, pos);
    kind = xl6ParserVKindAt(stream, next);
    if ( kind == XL6_TK_LT ) {
        next = xl6ParserVNext(stream, next);
        if ( !xl6ParserVMatchTypeList(stream, next, &next) ) {
            return 0;
        }
        if ( xl6ParserVKindAt(stream, next) != XL6_TK_GT ) {
            return 0;
        }
        next = xl6ParserVNext(stream, next);
    }

    *outNext = next;
    return 1;
}

static int xl6ParserVMatchTypeExpr(const XL6_ParseTokenStream* stream, XL6_VTokenPos pos, XL6_VTokenPos* outNext)
{
    if ( xl6ParserVMatchFunctionTypeRef(stream, pos, outNext) ) {
        return 1;
    }
    return xl6ParserVMatchTypeRef(stream, pos, outNext);
}

static int xl6ParserTypeRefFollowerKind(int kind)
{
    switch ( kind ) {
    case XL6_TK_IDENT:
    case XL6_TK_COMMA:
    case XL6_TK_RPAREN:
    case XL6_TK_ARROW:
    case XL6_TK_ASSIGN:
    case XL6_TK_SEP:
    case XL6_TK_EOF:
    case XL6_TK_GT:
        return 1;
    default:
        return 0;
    }
}

static void xl6ParserMarkShrRange(const XL6_ParseTokenStream* stream, size_t begin, size_t end, int* splitShr)
{
    size_t i;

    if ( stream == NULL || splitShr == NULL ) {
        return;
    }
    if ( end > stream->count ) {
        end = stream->count;
    }
    for ( i = begin; i < end; ++i ) {
        if ( stream->tokens[i].kind == XL6_TK_SHR ) {
            splitShr[i] = 1;
        }
    }
}

static int xl6ParserRewriteSplitShr(XL6_ParseTokenStream* stream, const int* splitShr)
{
    XL6_ParseTokenStream rewritten;
    size_t i;

    if ( stream == NULL || splitShr == NULL ) {
        return 1;
    }

    xl6ParseTokenStreamInit(&rewritten);
    for ( i = 0; i < stream->count; ++i ) {
        if ( stream->tokens[i].kind == XL6_TK_SHR && splitShr[i] ) {
            XL6_Token first = stream->tokens[i];
            XL6_Token second = stream->tokens[i];

            first.kind = XL6_TK_GT;
            first.len = 1;
            first.endOffset = first.startOffset + 1;
            first.endColumn = first.startColumn + 1;

            second.kind = XL6_TK_GT;
            if ( second.text != NULL ) {
                second.text += 1;
            }
            second.len = 1;
            second.startOffset += 1;
            second.startColumn += 1;

            if ( !xl6ParseTokenStreamAppend(&rewritten, &first) ||
                 !xl6ParseTokenStreamAppend(&rewritten, &second) ) {
                xl6ParseTokenStreamFree(&rewritten);
                return 0;
            }
        } else if ( !xl6ParseTokenStreamAppend(&rewritten, &stream->tokens[i]) ) {
            xl6ParseTokenStreamFree(&rewritten);
            return 0;
        }
    }

    xl6ParseTokenStreamFree(stream);
    *stream = rewritten;
    return 1;
}

static int xl6ParserSplitNestedGenericShr(XL6_ParseTokenStream* stream)
{
    int* splitShr;
    size_t i;
    int ok;

    if ( stream == NULL || stream->count == 0 ) {
        return 1;
    }

    splitShr = (int*)xrtCalloc(stream->count, sizeof(int));
    if ( splitShr == NULL ) {
        return 0;
    }

    for ( i = 0; i < stream->count; ++i ) {
        XL6_VTokenPos start;
        XL6_VTokenPos next;

        start.index = i;
        start.secondGt = 0;
        if ( xl6ParserVMatchTypeExpr(stream, start, &next) &&
             !next.secondGt &&
             next.index < stream->count &&
             xl6ParserTypeRefFollowerKind(stream->tokens[next.index].kind) ) {
            xl6ParserMarkShrRange(stream, i, next.index, splitShr);
        }

        if ( i + 2 < stream->count &&
             stream->tokens[i].kind == XL6_TK_SCOPE &&
             stream->tokens[i + 1].kind == XL6_TK_LT ) {
            XL6_VTokenPos argsStart;
            XL6_VTokenPos argsEnd;
            XL6_VTokenPos closeEnd;

            argsStart.index = i + 2;
            argsStart.secondGt = 0;
            if ( xl6ParserVMatchTypeList(stream, argsStart, &argsEnd) &&
                 xl6ParserVKindAt(stream, argsEnd) == XL6_TK_GT ) {
                closeEnd = xl6ParserVNext(stream, argsEnd);
                if ( !closeEnd.secondGt ) {
                    xl6ParserMarkShrRange(stream, i + 2, closeEnd.index, splitShr);
                }
            }
        }
    }

    ok = xl6ParserRewriteSplitShr(stream, splitShr);
    xrtFree(splitShr);
    return ok;
}

static void xl6ParserMarkGenericDeclTypeLt(XL6_ParseTokenStream* stream)
{
    size_t i;
    size_t afterType;
    size_t afterName;

    if ( stream == NULL ) {
        return;
    }

    for ( i = 0; i + 4 < stream->count; ++i ) {
        if ( !xl6ParserIsStmtStart(stream, i) ) {
            continue;
        }
        if ( !xl6ParserIsNameToken(&stream->tokens[i]) ) {
            continue;
        }
        if ( stream->tokens[i + 1].kind != XL6_TK_LT ) {
            continue;
        }
        if ( !xl6ParserMatchTypeRef(stream, i, &afterType) ) {
            continue;
        }
        if ( afterType >= stream->count || !xl6ParserIsNameToken(&stream->tokens[afterType]) ) {
            continue;
        }

        afterName = afterType + 1;
        if ( afterName >= stream->count ||
             stream->tokens[afterName].kind == XL6_TK_ASSIGN ||
             stream->tokens[afterName].kind == XL6_TK_SEP ||
             stream->tokens[afterName].kind == XL6_TK_EOF ) {
            /* 语句起点的 Type<...> name 声明优先按类型解析，避免和比较表达式抢 `<`。 */
            stream->parserKinds[i + 1] = XL6PG_TYPE_LT;
        }
    }
}

static int xl6ParserReadTokens(XL6_Lexer* lexer, XL6_ParseTokenStream* stream, XL6_DiagList* diags)
{
    XL6_Token token;

    for (;;) {
        token = xl6LexerNext(lexer);
        if ( !xl6ParseTokenStreamAppend(stream, &token) ) {
            xl6DiagAdd(diags, token.startLine, token.startColumn, "内存不足，无法缓存 token");
            return 0;
        }
        if ( token.kind == XL6_TK_ERROR ) {
            xl6DiagAdd(diags, token.startLine, token.startColumn,
                       token.message[0] != '\0' ? token.message : "词法错误");
            return 0;
        }
        if ( token.kind == XL6_TK_EOF ) {
            break;
        }
    }

    if ( !xl6ParserSplitNestedGenericShr(stream) ) {
        xl6DiagAdd(diags, token.startLine, token.startColumn, "内存不足，无法拆分嵌套泛型 token");
        return 0;
    }
    xl6ParserMarkGenericDeclTypeLt(stream);
    return 1;
}

int xl6ParseSource(const char* source, const char* fileName, XL6_ParseResult* result)
{
    XL6_Lexer lexer;
    XL6_ParseTokenStream stream;
    XL6_ParseContext ctx;
    XL6_Token eofToken;
    void* parser;
    size_t i;
    int ok;

    (void)fileName;
    if ( result == NULL ) {
        return 0;
    }
    xl6ParseTokenStreamInit(&stream);
    if ( !xl6LexerOpen(&lexer, source) ) {
        xl6DiagAdd(&result->diags, 1, 1, "无法初始化 lexer");
        return 0;
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.diags = &result->diags;
    parser = xl6ParseAlloc(malloc);
    if ( parser == NULL ) {
        xl6LexerClose(&lexer);
        xl6ParseTokenStreamFree(&stream);
        xl6DiagAdd(&result->diags, 1, 1, "无法初始化 parser");
        return 0;
    }

    ok = xl6ParserReadTokens(&lexer, &stream, &result->diags);
    if ( ok ) {
        for ( i = 0; i < stream.count; ++i ) {
            if ( stream.tokens[i].kind == XL6_TK_EOF ) {
                break;
            }
            xl6Parse(parser, stream.parserKinds[i], stream.tokens[i], &ctx);
            if ( ctx.hadError ) {
                ok = 0;
                break;
            }
        }
    }

    if ( ok ) {
        if ( stream.count > 0 ) {
            eofToken = stream.tokens[stream.count - 1];
        } else {
            memset(&eofToken, 0, sizeof(eofToken));
            eofToken.kind = XL6_TK_EOF;
        }
        xl6Parse(parser, 0, eofToken, &ctx);
        if ( ctx.hadError ) {
            ok = 0;
        }
    }

    result->root = ctx.root;
    xl6ParseFree(parser, free);
    xl6ParseTokenStreamFree(&stream);
    xl6LexerClose(&lexer);
    return ok && result->diags.count == 0;
}

int xl6ParseFile(const char* path, XL6_ParseResult* result)
{
    char* source;
    size_t len;
    int ok;

    xrtClearError();
    source = xrtFileReadAll((char*)path, XRT_CP_UTF8, &len);
    (void)len;
    if ( xrtGetError()[0] != '\0' || source == NULL ) {
        xrtFree(source);
        xl6DiagAdd(&result->diags, 1, 1, "无法读取源文件");
        return 0;
    }

    ok = xl6ParseSource(source, path, result);
    xrtFree(source);
    return ok;
}
