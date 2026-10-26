#include "src/parser/xl6_parser_api.h"
#include "src/lexer/xl6_lexer_api.h"
#include "src/parser/xl6_parser.h"
#include "src/sem/xl6_completion.h"

#include <stdlib.h>

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

    switch ( kind ) {
    case XL6_TK_IDENT: return XL6PG_IDENT;
    case XL6_TK_INT: return XL6PG_INT;
    case XL6_TK_FLOAT: return XL6PG_FLOAT;
    case XL6_TK_STRING: return XL6PG_STRING;
    case XL6_TK_CHAR: return XL6PG_CHAR;
    case XL6_TK_BYTES: return XL6PG_BYTES;
    case XL6_TK_FSTRING: return XL6PG_FSTRING;
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
    case XL6_KW_AS: return XL6PG_KW_AS;
    case XL6_KW_IMPORT: return XL6PG_KW_IMPORT;
    case XL6_KW_FROM: return XL6PG_KW_FROM;
    case XL6_KW_EXTERN: return XL6PG_KW_EXTERN;
    case XL6_KW_EXPORT: return XL6PG_KW_EXPORT;
    case XL6_KW_INCLUDE: return XL6PG_KW_INCLUDE;
    case XL6_KW_CLASS: return XL6PG_KW_CLASS;
    case XL6_KW_PROTOCOL: return XL6PG_KW_PROTOCOL;
    case XL6_KW_ENUM: return XL6PG_KW_ENUM;
    case XL6_KW_EXTENDS: return XL6PG_KW_EXTENDS;
    case XL6_KW_UNION: return XL6PG_KW_UNION;
    case XL6_KW_DERIVE: return XL6PG_KW_DERIVE;
    case XL6_KW_GET: return XL6PG_KW_GET;
    case XL6_KW_LET: return XL6PG_KW_LET;
    case XL6_KW_PUBLIC: return XL6PG_KW_PUBLIC;
    case XL6_KW_PRIVATE: return XL6PG_KW_PRIVATE;
    case XL6_KW_PROTECTED: return XL6PG_KW_PROTECTED;
    case XL6_KW_STATIC: return XL6PG_KW_STATIC;
    case XL6_KW_OVERRIDE: return XL6PG_KW_OVERRIDE;
    case XL6_KW_FINAL: return XL6PG_KW_FINAL;
    case XL6_KW_VIRTUAL: return XL6PG_KW_VIRTUAL;
    case XL6_KW_NATIVE: return XL6PG_KW_NATIVE;
    case XL6_KW_C_VARARG: return XL6PG_KW_C_VARARG;
    case XL6_KW_C_CALLBACK: return XL6PG_KW_C_CALLBACK;
    case XL6_KW_MATCH: return XL6PG_KW_MATCH;
    case XL6_KW_CASE: return XL6PG_KW_CASE;
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
    case XL6_KW_IS: return XL6PG_KW_IS;
    case XL6_TK_LPAREN: return XL6PG_LPAREN;
    case XL6_TK_RPAREN: return XL6PG_RPAREN;
    case XL6_TK_LBRACKET: return XL6PG_LBRACKET;
    case XL6_TK_RBRACKET: return XL6PG_RBRACKET;
    case XL6_TK_LBRACE: return XL6PG_LBRACE;
    case XL6_TK_RBRACE: return XL6PG_RBRACE;
    case XL6_TK_COMMA: return XL6PG_COMMA;
    case XL6_TK_COLON: return XL6PG_COLON;
    case XL6_TK_DOT: return XL6PG_DOT;
    case XL6_TK_AT: return XL6PG_AT;
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
    case XL6_TK_BIT_AND_ASSIGN: return XL6PG_BIT_AND_ASSIGN;
    case XL6_TK_BIT_OR_ASSIGN: return XL6PG_BIT_OR_ASSIGN;
    case XL6_TK_BIT_XOR_ASSIGN: return XL6PG_BIT_XOR_ASSIGN;
    case XL6_TK_NULL_ASSIGN: return XL6PG_NULL_ASSIGN;
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
    return kind == XL6PG_IDENT;
}

static int xl6ParserIsTypeNameToken(const XL6_Token* token)
{
    return xl6ParserIsNameToken(token) ||
           (token != NULL &&
            (token->kind == XL6_KW_FUNC ||
             token->kind == XL6_KW_VAR ||
             token->kind == XL6_KW_NULL));
}

static size_t xl6ParserStmtStartIndex(const XL6_ParseTokenStream* stream, size_t index)
{
    size_t start;

    if ( stream == NULL || index >= stream->count ) {
        return index;
    }

    start = index;
    while ( start > 0 && stream->tokens[start - 1].kind != XL6_TK_SEP ) {
        start -= 1;
    }
    return start;
}

static int xl6ParserIsStmtStart(const XL6_ParseTokenStream* stream, size_t index)
{
    if ( index == 0 ) {
        return 1;
    }
    return stream->tokens[index - 1].kind == XL6_TK_SEP;
}

/*
 * 后置循环以 `while 条件 end` 结束。它和普通 while 使用同一个词法 Token，
 * 但前者的 end 必须位于同一个逻辑行。这里仅标记 Lemon 看到的 Token 类型，
 * 不修改词法结果，也不把上下文状态引入 lexer。
 */
static void xl6ParserMarkPostWhileTokens(XL6_ParseTokenStream* stream)
{
    size_t i;
    size_t lineEnd;

    if ( stream == NULL ) {
        return;
    }

    for ( i = 0; i < stream->count; ++i ) {
        if ( stream->tokens[i].kind != XL6_KW_WHILE ||
             !xl6ParserIsStmtStart(stream, i) ) {
            continue;
        }

        lineEnd = i + 1;
        while ( lineEnd < stream->count &&
                stream->tokens[lineEnd].kind != XL6_TK_SEP &&
                stream->tokens[lineEnd].kind != XL6_TK_EOF ) {
            lineEnd += 1;
        }
        if ( lineEnd > i + 1 && stream->tokens[lineEnd - 1].kind == XL6_KW_END ) {
            stream->parserKinds[i] = XL6PG_POST_WHILE;
        }
    }
}

/*
 * match 语句和 match 表达式共享源码关键字，但语法体分别使用 block
 * 和 `then expr`。在语句起点提前分类，避免把二义性扩散到整个表达式文法。
 */
static void xl6ParserMarkStatementMatchTokens(XL6_ParseTokenStream* stream)
{
    size_t i;

    if ( stream == NULL ) {
        return;
    }
    for ( i = 0; i < stream->count; ++i ) {
        if ( stream->tokens[i].kind == XL6_KW_MATCH &&
             xl6ParserIsStmtStart(stream, i) ) {
            stream->parserKinds[i] = XL6PG_STMT_MATCH;
        }
    }
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
    if ( next < stream->count && stream->tokens[next].kind == XL6_TK_QUESTION ) {
        next += 1;
    }

    while ( next < stream->count && stream->tokens[next].kind == XL6_TK_COMMA ) {
        if ( !xl6ParserMatchTypeExpr(stream, next + 1, &next) ) {
            return 0;
        }
        if ( next < stream->count && stream->tokens[next].kind == XL6_TK_QUESTION ) {
            next += 1;
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
         stream->tokens[index].kind == XL6_KW_C_VARARG ||
         stream->tokens[index].kind == XL6_TK_TRIPLE_STAR ) {
        *outNext = index + 1;
        return 1;
    }
    size_t next;
    if(!xl6ParserMatchTypeExpr(stream,index,&next))return 0;
    /* Parameters are type-only contexts. Consume their nullable suffix just
     * as return types/type lists do, before matching the enclosing ')'. */
    if(next<stream->count && stream->tokens[next].kind==XL6_TK_QUESTION)++next;
    *outNext=next;return 1;
}

static int xl6ParserMatchFunctionReturnSig(const XL6_ParseTokenStream* stream, size_t index, size_t* outNext)
{
    size_t next;

    if ( stream == NULL || index >= stream->count || stream->tokens[index].kind != XL6_KW_AS ) {
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

    if ( !xl6ParserMatchTypeExpr(stream, next, &next) ) {
        return 0;
    }
    if ( next < stream->count && stream->tokens[next].kind == XL6_TK_QUESTION ) {
        next += 1;
    }
    *outNext = next;
    return 1;
}

static int xl6ParserMatchFunctionTypeRef(const XL6_ParseTokenStream* stream, size_t index, size_t* outNext)
{
    size_t next;

    if ( stream == NULL || index + 1 >= stream->count ||
         stream->tokens[index].kind != XL6_KW_FUNC ||
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

    if ( index >= stream->count || !xl6ParserIsTypeNameToken(&stream->tokens[index]) ) {
        return 0;
    }

    next = index + 1;
    while ( next + 1 < stream->count &&
            stream->tokens[next].kind == XL6_TK_DOT &&
            xl6ParserIsNameToken(&stream->tokens[next + 1]) ) {
        next += 2;
    }
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
    size_t next;

    if ( stream != NULL &&
         index + 1 < stream->count &&
         stream->tokens[index].kind == XL6_TK_LPAREN &&
         xl6ParserMatchFunctionTypeRef(stream, index + 1, &next) &&
         next < stream->count &&
         stream->tokens[next].kind == XL6_TK_RPAREN ) {
        *outNext = next + 1;
        return 1;
    }
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

static int xl6ParserVIsTypeNameToken(const XL6_ParseTokenStream* stream, XL6_VTokenPos pos)
{
    if ( stream == NULL || pos.index >= stream->count || pos.secondGt ) {
        return 0;
    }
    return xl6ParserIsTypeNameToken(&stream->tokens[pos.index]);
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
    if ( xl6ParserVKindAt(stream, next) == XL6_TK_QUESTION ) {
        next = xl6ParserVNext(stream, next);
    }
    while ( xl6ParserVKindAt(stream, next) == XL6_TK_COMMA ) {
        next = xl6ParserVNext(stream, next);
        if ( !xl6ParserVMatchTypeExpr(stream, next, &next) ) {
            return 0;
        }
        if ( xl6ParserVKindAt(stream, next) == XL6_TK_QUESTION ) {
            next = xl6ParserVNext(stream, next);
        }
    }

    *outNext = next;
    return 1;
}

static int xl6ParserVMatchFuncTypeParam(const XL6_ParseTokenStream* stream, XL6_VTokenPos pos, XL6_VTokenPos* outNext)
{
    int kind;

    kind = xl6ParserVKindAt(stream, pos);
    if ( kind == XL6_TK_ELLIPSIS || kind == XL6_TK_TRIPLE_STAR || kind == XL6_KW_C_VARARG ) {
        *outNext = xl6ParserVNext(stream, pos);
        return 1;
    }
    XL6_VTokenPos next;
    if(!xl6ParserVMatchTypeExpr(stream,pos,&next))return 0;
    if(xl6ParserVKindAt(stream,next)==XL6_TK_QUESTION)next=xl6ParserVNext(stream,next);
    *outNext=next;return 1;
}

static int xl6ParserVMatchFunctionReturnSig(const XL6_ParseTokenStream* stream, XL6_VTokenPos pos, XL6_VTokenPos* outNext)
{
    XL6_VTokenPos next;

    if ( xl6ParserVKindAt(stream, pos) != XL6_KW_AS ) {
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

    if ( !xl6ParserVMatchTypeExpr(stream, next, &next) ) {
        return 0;
    }
    if ( xl6ParserVKindAt(stream, next) == XL6_TK_QUESTION ) {
        next = xl6ParserVNext(stream, next);
    }
    *outNext = next;
    return 1;
}

static int xl6ParserVMatchFunctionTypeRef(const XL6_ParseTokenStream* stream, XL6_VTokenPos pos, XL6_VTokenPos* outNext)
{
    XL6_VTokenPos next;

    if ( stream == NULL || pos.index >= stream->count || pos.secondGt ||
         stream->tokens[pos.index].kind != XL6_KW_FUNC ) {
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

    if ( !xl6ParserVIsTypeNameToken(stream, pos) ) {
        return 0;
    }

    next = xl6ParserVNext(stream, pos);
    while ( xl6ParserVKindAt(stream, next) == XL6_TK_DOT ) {
        XL6_VTokenPos part = xl6ParserVNext(stream, next);

        if ( !xl6ParserVIsNameToken(stream, part) ) {
            return 0;
        }
        next = xl6ParserVNext(stream, part);
    }
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
    XL6_VTokenPos inner;
    XL6_VTokenPos next;

    /*
     * 函数签名自身已经使用括号表示参数和多返回值。
     * 外层括号只用于让后缀 ? 作用于完整函数值类型：
     *     (func(int) as int)?
     */
    if ( xl6ParserVKindAt(stream, pos) == XL6_TK_LPAREN ) {
        inner = xl6ParserVNext(stream, pos);
        if ( xl6ParserVMatchFunctionTypeRef(stream, inner, &next) &&
             xl6ParserVKindAt(stream, next) == XL6_TK_RPAREN ) {
            *outNext = xl6ParserVNext(stream, next);
            return 1;
        }
    }
    if ( xl6ParserVMatchFunctionTypeRef(stream, pos, outNext) ) {
        return 1;
    }
    return xl6ParserVMatchTypeRef(stream, pos, outNext);
}

static int xl6ParserVMatchGenericDeclConstraintList(const XL6_ParseTokenStream* stream, XL6_VTokenPos pos, XL6_VTokenPos* outNext)
{
    XL6_VTokenPos next;

    if ( !xl6ParserVMatchTypeRef(stream, pos, &next) ) {
        return 0;
    }
    while ( xl6ParserVKindAt(stream, next) == XL6_KW_AND ) {
        next = xl6ParserVNext(stream, next);
        if ( !xl6ParserVMatchTypeRef(stream, next, &next) ) {
            return 0;
        }
    }

    *outNext = next;
    return 1;
}

static int xl6ParserVMatchGenericDeclParam(const XL6_ParseTokenStream* stream, XL6_VTokenPos pos, XL6_VTokenPos* outNext)
{
    XL6_VTokenPos next;

    if ( !xl6ParserVIsNameToken(stream, pos) ) {
        return 0;
    }

    next = xl6ParserVNext(stream, pos);
    if ( xl6ParserVKindAt(stream, next) == XL6_KW_IS ) {
        next = xl6ParserVNext(stream, next);
        if ( !xl6ParserVMatchGenericDeclConstraintList(stream, next, &next) ) {
            return 0;
        }
    }

    *outNext = next;
    return 1;
}

static int xl6ParserVMatchGenericDeclParamList(const XL6_ParseTokenStream* stream, XL6_VTokenPos pos, XL6_VTokenPos* outNext)
{
    XL6_VTokenPos next;

    if ( !xl6ParserVMatchGenericDeclParam(stream, pos, &next) ) {
        return 0;
    }
    while ( xl6ParserVKindAt(stream, next) == XL6_TK_COMMA ) {
        next = xl6ParserVNext(stream, next);
        if ( !xl6ParserVMatchGenericDeclParam(stream, next, &next) ) {
            return 0;
        }
    }

    *outNext = next;
    return 1;
}

static int xl6ParserTypeRefFollowerKind(int kind)
{
    switch ( kind ) {
    case XL6_TK_IDENT:
    case XL6_TK_QUESTION:
    case XL6_TK_COMMA:
    case XL6_TK_RPAREN:
    case XL6_KW_AS:
    case XL6_TK_ASSIGN:
    case XL6_TK_SEP:
    case XL6_TK_EOF:
    case XL6_TK_GT:
        return 1;
    default:
        return 0;
    }
}

static int xl6ParserFunctionTypeFollowerKind(int kind)
{
    switch ( kind ) {
    case XL6_TK_COMMA:
    case XL6_TK_RPAREN:
    case XL6_TK_RBRACKET:
    case XL6_TK_GT:
    case XL6_TK_SEP:
    case XL6_TK_EOF:
        return 1;
    default:
        return 0;
    }
}

static int xl6ParserFunctionTypePrevKind(
	const XL6_ParseTokenStream* stream,
	size_t index,
	XL6_TokenKind kind)
{
    if ( stream == NULL || index == 0 || index > stream->count ) {
        return 0;
    }
    return stream->tokens[index - 1].kind == kind;
}

static int xl6ParserFunctionTypePrevParserKind(const XL6_ParseTokenStream* stream, size_t index, int kind)
{
    if ( stream == NULL || index == 0 || index > stream->count ) {
        return 0;
    }
    return stream->parserKinds[index - 1] == kind;
}

static int xl6ParserFunctionTypeInAlias(const XL6_ParseTokenStream* stream, size_t index)
{
    size_t start;

    if ( stream == NULL || index >= stream->count ) {
        return 0;
    }

    start = xl6ParserStmtStartIndex(stream, index);
    return start < stream->count &&
           (stream->tokens[start].kind == XL6_KW_TYPEDEF ||
            stream->tokens[start].kind == XL6_KW_EXTERN);
}

static int xl6ParserIsFuncDeclHead(const XL6_ParseTokenStream* stream, size_t index)
{
    XL6_VTokenPos pos;
    XL6_VTokenPos next;

    if ( stream == NULL || index + 2 >= stream->count ) {
        return 0;
    }
    if ( stream->tokens[index].kind != XL6_KW_FUNC ||
         !xl6ParserIsNameToken(&stream->tokens[index + 1]) ) {
        return 0;
    }
    if ( stream->tokens[index + 2].kind == XL6_TK_LPAREN ) {
        return 1;
    }
    if ( stream->tokens[index + 2].kind != XL6_TK_LT ) {
        return 0;
    }

    pos.index = index + 3;
    pos.secondGt = 0;
    if ( !xl6ParserVMatchGenericDeclParamList(stream, pos, &next) ) {
        return 0;
    }
    if ( xl6ParserVKindAt(stream, next) != XL6_TK_GT ) {
        return 0;
    }
    next = xl6ParserVNext(stream, next);
    return xl6ParserVKindAt(stream, next) == XL6_TK_LPAREN;
}

static int xl6ParserMatchAnonSignatureEnd(const XL6_ParseTokenStream*, size_t, size_t*);
static int xl6ParserWhileLineEndsBlock(const XL6_ParseTokenStream*, size_t);

/* Inside delimiters the lexer suppresses newlines, so a signature followed
 * by `name = ...` can be either a parameter declaration or a block lambda's
 * first assignment. Only a matching body END before the containing delimiter
 * resolves that ambiguity. Nested blocks/functions own their own END tokens;
 * in particular, a lambda default in a function-typed parameter is not the
 * body of that parameter's type. No symbol spelling participates in this test. */
static int xl6ParserSignatureOwnsFollowingBlock(
    const XL6_ParseTokenStream* stream, size_t next)
{
    size_t i;
    int paren = 0, bracket = 0, brace = 0, blocks = 1;
    for (i = next; i < stream->count; ++i) {
        XL6_TokenKind kind = stream->tokens[i].kind;
        if (kind == XL6_TK_EOF) return 0;
        if (kind == XL6_TK_LPAREN) ++paren;
        else if (kind == XL6_TK_RPAREN) { if (paren == 0) return 0; --paren; }
        else if (kind == XL6_TK_LBRACKET || kind == XL6_TK_SAFE_LBRACKET) ++bracket;
        else if (kind == XL6_TK_RBRACKET) { if (bracket == 0) return 0; --bracket; }
        else if (kind == XL6_TK_LBRACE) ++brace;
        else if (kind == XL6_TK_RBRACE) { if (brace == 0) return 0; --brace; }
        else if (kind == XL6_KW_END) { if (--blocks == 0) return 1; }
        else if (kind == XL6_KW_IF || kind == XL6_KW_FOR || kind == XL6_KW_TRY ||
                 kind == XL6_KW_DO || kind == XL6_KW_MATCH ||
                 (kind == XL6_KW_WHILE && !xl6ParserWhileLineEndsBlock(stream, i))) ++blocks;
        else if (kind == XL6_KW_FUNC) {
            size_t body;
            if (xl6ParserIsFuncDeclHead(stream, i)) ++blocks;
            else if (!xl6ParserFunctionTypePrevKind(stream, i, XL6_KW_AS) &&
                     xl6ParserMatchAnonSignatureEnd(stream, i, &body) &&
                     body > i && body < stream->count &&
                     stream->tokens[body].kind != XL6_TK_FAT_ARROW &&
                     (stream->tokens[body].kind == XL6_TK_SEP ||
                      stream->tokens[body - 1].endLine < stream->tokens[body].startLine)) {
                ++blocks;
                /* Nested return signatures can themselves contain func types. */
                i = body - 1;
            }
        }
    }
    return 0;
}

static int xl6ParserCanUseFunctionType(const XL6_ParseTokenStream* stream, size_t index, size_t next)
{
    int nextKind;

    if ( stream == NULL || index >= stream->count || next > stream->count ) {
        return 0;
    }

    nextKind = next < stream->count ? stream->tokens[next].kind : XL6_TK_EOF;
    if ( nextKind == XL6_TK_FAT_ARROW ) {
        return 0;
    }

    /* A declaration name must actually terminate a declaration/parameter
     * prefix. With suppressed newlines, the first statement of a block
     * lambda may start with a call or a generic type; that identifier alone
     * is not evidence that the preceding function signature was a type. */
    if ( next + 1 < stream->count && xl6ParserIsNameToken(&stream->tokens[next]) &&
         (xl6ParserFunctionTypeFollowerKind(stream->tokens[next + 1].kind) ||
          stream->tokens[next + 1].kind == XL6_TK_ASSIGN) ) {
        if (next > index + 1 && stream->tokens[next - 1].endLine < stream->tokens[next].startLine &&
            !xl6ParserFunctionTypePrevKind(stream, index, XL6_KW_AS) &&
            !xl6ParserFunctionTypePrevKind(stream, index, XL6_KW_IS) &&
            xl6ParserSignatureOwnsFollowingBlock(stream, next)) return 0;
        return 1;
    }

    /* 返回类型上下文，例如：func outer() as func(int) as int */
    if ( xl6ParserFunctionTypePrevKind(stream, index, XL6_KW_AS) ||
         xl6ParserFunctionTypePrevKind(stream, index, XL6_KW_IS) ) {
        return 1;
    }

    /* 类型字面量比较，例如：type(value) == func */
    if ( xl6ParserFunctionTypePrevKind(stream, index, XL6_TK_EQ) ||
         xl6ParserFunctionTypePrevKind(stream, index, XL6_TK_NE) ) {
        return xl6ParserFunctionTypeFollowerKind(nextKind);
    }

    /* 裸 func 类型字面量初始化，例如：type t = func */
    if ( next == index + 1 &&
         xl6ParserFunctionTypePrevKind(stream, index, XL6_TK_ASSIGN) ) {
        return xl6ParserFunctionTypeFollowerKind(nextKind);
    }

    /* The raw C callback qualifier prefixes a function type. */
    if (index > 0 && stream->tokens[index - 1].kind == XL6_KW_C_CALLBACK) {
        return xl6ParserFunctionTypeFollowerKind(nextKind);
    }

    /* 泛型实参或多返回类型列表中的函数类型。 */
    if ( xl6ParserFunctionTypePrevKind(stream, index, XL6_TK_LT) ||
         xl6ParserFunctionTypePrevParserKind(stream, index, XL6PG_TYPE_LT) ||
         xl6ParserFunctionTypePrevKind(stream, index, XL6_TK_COMMA) ||
         xl6ParserFunctionTypePrevKind(stream, index, XL6_TK_LPAREN) ) {
        return xl6ParserFunctionTypeFollowerKind(nextKind);
    }

    /* typedef/extern typedef 的右侧是类型，不是匿名函数表达式。 */
    if ( xl6ParserFunctionTypePrevKind(stream, index, XL6_TK_ASSIGN) &&
         xl6ParserFunctionTypeInAlias(stream, index) ) {
        return xl6ParserFunctionTypeFollowerKind(nextKind);
    }

    return 0;
}

static void xl6ParserMarkFunctionTypeTokens(XL6_ParseTokenStream* stream)
{
    size_t i;
    size_t next;

    if ( stream == NULL ) {
        return;
    }

    for ( i = 0; i < stream->count; ++i ) {
        if ( stream->tokens[i].kind != XL6_KW_FUNC ) {
            continue;
        }
        if ( xl6ParserIsFuncDeclHead(stream, i) ) {
            continue;
        }

        if ( i + 1 < stream->count &&
             stream->tokens[i + 1].kind == XL6_TK_LPAREN &&
             xl6ParserMatchFunctionTypeRef(stream, i, &next) &&
             xl6ParserCanUseFunctionType(stream, i, next) ) {
            stream->parserKinds[i] = XL6PG_TYPE_FUNCTION;
            continue;
        }

        if ( xl6ParserCanUseFunctionType(stream, i, i + 1) ) {
            stream->parserKinds[i] = XL6PG_TYPE_FUNCTION;
        }
    }
}

static int xl6ParserNullableNamedDeclContext(
    const XL6_ParseTokenStream* stream,
    size_t typeStart,
    size_t question)
{
    size_t stmtStart;
    size_t i;
    int follower;

    if ( stream == NULL || question + 1 >= stream->count ||
         !xl6ParserIsNameToken(&stream->tokens[question + 1]) ) {
        return 0;
    }

    follower = question + 2 < stream->count
        ? stream->tokens[question + 2].kind
        : XL6_TK_EOF;
    if ( follower != XL6_TK_ASSIGN &&
         follower != XL6_TK_COMMA &&
         follower != XL6_TK_RPAREN &&
         follower != XL6_TK_SEP &&
         follower != XL6_TK_EOF &&
         follower != XL6_TK_AT ) {
        return 0;
    }

    if ( xl6ParserIsStmtStart(stream, typeStart) ) {
        return 1;
    }
    /*
     * 参数类型只会出现在函数、属性或 class 特殊方法的声明头中。
     * 这里检查当前语句头，避免把普通调用里的三元表达式改写成类型后缀。
     */
    stmtStart = xl6ParserStmtStartIndex(stream, typeStart);
    /* A field's type can follow a complete modifier prefix. Do not classify
     * by the preceding token alone: contextual modifier names also occur in
     * ordinary expressions, whose question mark must remain ternary syntax. */
    for (i = stmtStart; i < typeStart; ++i) {
        int kind = stream->tokens[i].kind;
        if (kind != XL6_KW_PUBLIC && kind != XL6_KW_PRIVATE &&
            kind != XL6_KW_PROTECTED && kind != XL6_KW_STATIC &&
            kind != XL6_KW_OVERRIDE && kind != XL6_KW_FINAL &&
            kind != XL6_KW_VIRTUAL && kind != XL6_KW_NATIVE) break;
    }
    if (i == typeStart && i > stmtStart) return 1;
    for ( i = stmtStart; i < typeStart; ++i ) {
        if ( stream->tokens[i].kind == XL6_KW_FUNC ||
             stream->tokens[i].kind == XL6_KW_GET ||
             stream->tokens[i].kind == XL6_KW_LET ) {
            return 1;
        }
    }
    return stmtStart + 1 < typeStart &&
           xl6ParserIsNameToken(&stream->tokens[stmtStart]) &&
           stream->tokens[stmtStart + 1].kind == XL6_TK_LPAREN;
}

static int xl6ParserNullableAliasContext(
    const XL6_ParseTokenStream* stream,
    size_t typeStart)
{
    size_t stmtStart;
    size_t i;

    if ( stream == NULL || typeStart == 0 ||
         stream->tokens[typeStart - 1].kind != XL6_TK_ASSIGN ) {
        return 0;
    }
    stmtStart = xl6ParserStmtStartIndex(stream, typeStart);
    for ( i = stmtStart; i < typeStart; ++i ) {
        if ( stream->tokens[i].kind == XL6_KW_TYPEDEF ) {
            return 1;
        }
    }
    return 0;
}

/*
 * is/as 后面的类型名与三元表达式共用问号字符。
 * `value is int ? 1 : 0` 的问号后紧跟一段表达式，不能把它改写成 int?；
 * `value is int?`、`value as int? ?? 0` 则由后续运算符结束类型后缀。
 */
static int xl6ParserTokenStartsExpression(int kind)
{
    switch ( kind ) {
    case XL6_TK_IDENT:
    case XL6_TK_INT:
    case XL6_TK_FLOAT:
    case XL6_TK_STRING:
    case XL6_TK_FSTRING:
    case XL6_TK_TIME:
    case XL6_KW_NULL:
    case XL6_KW_TRUE:
    case XL6_KW_FALSE:
    case XL6_KW_FUNC:
    case XL6_KW_AWAIT:
    case XL6_TK_LPAREN:
    case XL6_TK_LBRACKET:
    case XL6_TK_LBRACE:
    case XL6_TK_PLUS:
    case XL6_TK_MINUS:
    case XL6_KW_NOT:
    case XL6_TK_BANG:
    case XL6_TK_BIT_NOT:
        return 1;
    default:
        return 0;
    }
}

static int xl6ParserNullableTypeContext(
    const XL6_ParseTokenStream* stream,
    size_t typeStart,
    size_t question)
{
    int follower;
    int previous;

    if ( stream == NULL || question >= stream->count ||
         stream->tokens[question].kind != XL6_TK_QUESTION ) {
        return 0;
    }

    follower = question + 1 < stream->count
        ? stream->tokens[question + 1].kind
        : XL6_TK_EOF;
    previous = typeStart > 0
        ? stream->tokens[typeStart - 1].kind
        : XL6_TK_EOF;

    if ( xl6ParserNullableNamedDeclContext(stream, typeStart, question) ) {
        return 1;
    }
    if ( previous == XL6_KW_AS || previous == XL6_KW_IS ) {
        return !xl6ParserTokenStartsExpression(follower);
    }
    if ( xl6ParserNullableAliasContext(stream, typeStart) ) {
        return 1;
    }

    /*
     * 无声明名的类型只出现在泛型实参、函数类型参数和多返回值列表中。
     * 这些位置的后继符号不可能构成合法三元表达式，因此可以安全分类。
     */
    return follower == XL6_TK_COMMA ||
           follower == XL6_TK_RPAREN ||
           follower == XL6_TK_GT ||
           follower == XL6_TK_SHR;
}

static void xl6ParserMarkNullableTypeQuestions(XL6_ParseTokenStream* stream)
{
    size_t i;
    size_t next;

    if ( stream == NULL ) {
        return;
    }

    for ( i = 0; i < stream->count; ++i ) {
        if ( !xl6ParserMatchTypeExpr(stream, i, &next) ||
             next >= stream->count ||
             stream->tokens[next].kind != XL6_TK_QUESTION ) {
            continue;
        }
        if ( xl6ParserNullableTypeContext(stream, i, next) ) {
            stream->parserKinds[next] = XL6PG_TYPE_QUESTION;
            if ( i + 1 < next &&
                 stream->tokens[i].kind == XL6_TK_LPAREN &&
                 stream->tokens[i + 1].kind == XL6_KW_FUNC &&
                 stream->tokens[next - 1].kind == XL6_TK_RPAREN ) {
                stream->parserKinds[next - 1] = XL6PG_TYPE_RPAREN;
            }
        }
    }
}

static void xl6ParserDelimiterStep(const XL6_Token* token, int* paren, int* bracket, int* brace)
{
    if ( token == NULL ) {
        return;
    }
    switch ( token->kind ) {
    case XL6_TK_LPAREN: *paren += 1; break;
    case XL6_TK_RPAREN: if ( *paren > 0 ) { *paren -= 1; } break;
    case XL6_TK_LBRACKET:
    case XL6_TK_SAFE_LBRACKET: *bracket += 1; break;
    case XL6_TK_RBRACKET: if ( *bracket > 0 ) { *bracket -= 1; } break;
    case XL6_TK_LBRACE: *brace += 1; break;
    case XL6_TK_RBRACE: if ( *brace > 0 ) { *brace -= 1; } break;
    default: break;
    }
}

static int xl6ParserWhileLineEndsBlock(const XL6_ParseTokenStream* stream, size_t index)
{
    size_t end;
    int line;

    if ( stream == NULL || index >= stream->count || stream->tokens[index].kind != XL6_KW_WHILE ) {
        return 0;
    }
    line = stream->tokens[index].startLine;
    end = index + 1;
    while ( end < stream->count &&
            stream->tokens[end].kind != XL6_TK_EOF &&
            stream->tokens[end].startLine == line ) {
        end += 1;
    }
    return end > index + 1 && stream->tokens[end - 1].kind == XL6_KW_END;
}

static int xl6ParserMatchAnonSignatureEnd(
    const XL6_ParseTokenStream* stream,
    size_t index,
    size_t* outNext)
{
    size_t next;
    int depth;

    if ( stream == NULL || outNext == NULL || index + 1 >= stream->count ||
         stream->tokens[index].kind != XL6_KW_FUNC ||
         stream->tokens[index + 1].kind != XL6_TK_LPAREN ) {
        return 0;
    }

    /*
     * 匿名函数参数带名称和默认值，不能复用只接受类型列表的 func 类型匹配器。
     * 这里只确定签名边界，参数是否合法仍由 Lemon 语法规则统一判断。
     */
    depth = 0;
    for ( next = index + 1; next < stream->count; ++next ) {
        if ( stream->tokens[next].kind == XL6_TK_LPAREN ) {
            depth += 1;
        } else if ( stream->tokens[next].kind == XL6_TK_RPAREN ) {
            depth -= 1;
            if ( depth == 0 ) {
                return xl6ParserMatchFunctionReturnSig(stream, next + 1, outNext);
            }
        } else if ( stream->tokens[next].kind == XL6_TK_EOF ) {
            break;
        }
    }
    return 0;
}

static int xl6ParserMultilineAnonBodyStart(
    const XL6_ParseTokenStream* stream,
    size_t index,
    size_t* bodyStart)
{
    size_t next;

    if ( stream == NULL || index >= stream->count ||
         stream->tokens[index].kind != XL6_KW_FUNC ||
         stream->parserKinds[index] == XL6PG_TYPE_FUNCTION ||
         xl6ParserIsFuncDeclHead(stream, index) ||
         !xl6ParserMatchAnonSignatureEnd(stream, index, &next) ||
         next >= stream->count ||
         stream->tokens[next].kind == XL6_TK_FAT_ARROW ) {
        return 0;
    }
    if ( next > index &&
         stream->tokens[next].kind != XL6_TK_SEP &&
         stream->tokens[next - 1].endLine >= stream->tokens[next].startLine ) {
        return 0;
    }
    *bodyStart = next;
    return 1;
}

/*
 * 普通括号内部换行不产生 SEP；匿名函数字面量的函数体是唯一例外。
 * func/type 的上下文分类完成后，才能无歧义识别这个例外。这里只在函数体
 * 自身的分隔符深度补 SEP，嵌套调用、索引和字面量内部仍保持连续表达式语义。
 */
static int xl6ParserInsertAnonBlockSeparators(XL6_ParseTokenStream* stream)
{
    XL6_ParseTokenStream rewritten;
    unsigned char* insertBefore;
    size_t i;
    int paren;
    int bracket;
    int brace;

    if ( stream == NULL || stream->count == 0 ) {
        return 1;
    }
    insertBefore = (unsigned char*)xrtCalloc(stream->count, sizeof(unsigned char));
    if ( insertBefore == NULL ) {
        return 0;
    }

    paren = 0;
    bracket = 0;
    brace = 0;
    for ( i = 0; i < stream->count; ++i ) {
        size_t bodyStart;

        if ( xl6ParserMultilineAnonBodyStart(stream, i, &bodyStart) ) {
            size_t j;
            int baseParen = paren;
            int baseBracket = bracket;
            int baseBrace = brace;
            int scanParen = paren;
            int scanBracket = bracket;
            int scanBrace = brace;
            int blockDepth = 1;

            for ( j = i; j < bodyStart; ++j ) {
                xl6ParserDelimiterStep(&stream->tokens[j], &scanParen, &scanBracket, &scanBrace);
            }
            baseParen = scanParen;
            baseBracket = scanBracket;
            baseBrace = scanBrace;
            for ( j = bodyStart; j < stream->count && blockDepth > 0; ++j ) {
                XL6_TokenKind kind = stream->tokens[j].kind;

                if ( j > 0 &&
                     stream->tokens[j - 1].kind != XL6_TK_SEP &&
                     kind != XL6_TK_SEP && kind != XL6_TK_EOF &&
                     stream->tokens[j - 1].endLine < stream->tokens[j].startLine &&
                     scanParen == baseParen && scanBracket == baseBracket && scanBrace == baseBrace ) {
                    insertBefore[j] = 1;
                }

                if ( kind == XL6_KW_END ) {
                    blockDepth -= 1;
                } else if ( kind == XL6_KW_IF || kind == XL6_KW_FOR ||
                            kind == XL6_KW_TRY || kind == XL6_KW_DO ||
                            kind == XL6_KW_MATCH ||
                            (kind == XL6_KW_WHILE && !xl6ParserWhileLineEndsBlock(stream, j)) ) {
                    blockDepth += 1;
                } else if ( kind == XL6_KW_FUNC ) {
                    size_t nestedBody;
                    if ( xl6ParserMultilineAnonBodyStart(stream, j, &nestedBody) ) {
                        blockDepth += 1;
                    }
                }
                xl6ParserDelimiterStep(&stream->tokens[j], &scanParen, &scanBracket, &scanBrace);
            }
        }
        xl6ParserDelimiterStep(&stream->tokens[i], &paren, &bracket, &brace);
    }

    xl6ParseTokenStreamInit(&rewritten);
    for ( i = 0; i < stream->count; ++i ) {
        if ( insertBefore[i] ) {
            XL6_Token separator = stream->tokens[i];
            separator.kind = XL6_TK_SEP;
            separator.len = 0;
            separator.endOffset = separator.startOffset;
            separator.endLine = separator.startLine;
            separator.endColumn = separator.startColumn;
            separator.message[0] = '\0';
            if ( !xl6ParseTokenStreamAppend(&rewritten, &separator) ) {
                xl6ParseTokenStreamFree(&rewritten);
                xrtFree(insertBefore);
                return 0;
            }
        }
        if ( !xl6ParseTokenStreamAppend(&rewritten, &stream->tokens[i]) ) {
            xl6ParseTokenStreamFree(&rewritten);
            xrtFree(insertBefore);
            return 0;
        }
        /* Classification precedes synthetic body separators. Preserve it:
         * reclassifying `func() as T` after insertion mistakes the new SEP
         * for a function-type follower and turns a block lambda into a type. */
        rewritten.parserKinds[rewritten.count - 1] = stream->parserKinds[i];
    }
    xrtFree(insertBefore);
    xl6ParseTokenStreamFree(stream);
    *stream = rewritten;
    return 1;
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

/*
 * Lemon 在语句开头无法只靠一个 token 的前瞻区分 `module.Type value`
 * 和 `object.field = value`。词法结果保持原样，这里仅把“后面明确跟着声明名”
 * 的点分类型合并成 parser 虚拟 token，避免把普通成员访问引入类型语法。
 */
static int xl6ParserQualifiedDeclTypeEnd(
    const XL6_ParseTokenStream* stream,
    size_t start,
    size_t* outEnd)
{
    size_t end;

    if ( stream == NULL || outEnd == NULL || start + 2 >= stream->count ||
         !xl6ParserIsNameToken(&stream->tokens[start]) ||
         stream->tokens[start + 1].kind != XL6_TK_DOT ||
         !xl6ParserIsNameToken(&stream->tokens[start + 2]) ) {
        return 0;
    }

    end = start + 3;
    while ( end + 1 < stream->count &&
            stream->tokens[end].kind == XL6_TK_DOT &&
            xl6ParserIsNameToken(&stream->tokens[end + 1]) ) {
        end += 2;
    }
    if ( end < stream->count && xl6ParserIsNameToken(&stream->tokens[end]) ) {
        *outEnd = end;
        return 1;
    }
    if ( end + 1 < stream->count &&
         stream->tokens[end].kind == XL6_TK_QUESTION &&
         xl6ParserIsNameToken(&stream->tokens[end + 1]) ) {
        *outEnd = end;
        return 1;
    }
    if ( end < stream->count && stream->tokens[end].kind == XL6_TK_LT ) {
        size_t cursor = end;
        int depth = 0;

        while ( cursor < stream->count ) {
            if ( stream->tokens[cursor].kind == XL6_TK_LT ) {
                depth += 1;
            } else if ( stream->tokens[cursor].kind == XL6_TK_GT ) {
                depth -= 1;
                if ( depth == 0 ) {
                    cursor += 1;
                    break;
                }
            } else if ( stream->tokens[cursor].kind == XL6_TK_SEP ||
                        stream->tokens[cursor].kind == XL6_TK_EOF ) {
                return 0;
            }
            cursor += 1;
        }
        if ( depth == 0 && cursor < stream->count &&
             (xl6ParserIsNameToken(&stream->tokens[cursor]) ||
              (cursor + 1 < stream->count &&
               stream->tokens[cursor].kind == XL6_TK_QUESTION &&
               xl6ParserIsNameToken(&stream->tokens[cursor + 1]))) ) {
            *outEnd = end;
            return 1;
        }
    }
    return 0;
}

static int xl6ParserRewriteQualifiedDeclTypes(XL6_ParseTokenStream* stream)
{
    XL6_ParseTokenStream rewritten;
    size_t i;

    if ( stream == NULL ) {
        return 0;
    }
    xl6ParseTokenStreamInit(&rewritten);
    for ( i = 0; i < stream->count; ++i ) {
        size_t end;

        if ( xl6ParserQualifiedDeclTypeEnd(stream, i, &end) ) {
            XL6_Token token = stream->tokens[i];
            const XL6_Token* last = &stream->tokens[end - 1];

            token.len = last->endOffset - token.startOffset;
            token.endOffset = last->endOffset;
            token.endLine = last->endLine;
            token.endColumn = last->endColumn;
            if ( !xl6ParseTokenStreamAppend(&rewritten, &token) ) {
                xl6ParseTokenStreamFree(&rewritten);
                return 0;
            }
            rewritten.parserKinds[rewritten.count - 1] = XL6PG_QUALIFIED_TYPE;
            i = end - 1;
            continue;
        }
        if ( !xl6ParseTokenStreamAppend(&rewritten, &stream->tokens[i]) ) {
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
             (xl6ParserTypeRefFollowerKind(stream->tokens[next.index].kind) ||
              (i>0 && (stream->tokens[i-1].kind==XL6_KW_AS ||
                       stream->tokens[i-1].kind==XL6_KW_IS))) ) {
            /* Explicit type contexts include lambda results whose following
             * body keyword has no synthetic SEP yet. Only split the matched
             * type range; a real shift after the type stays an expression. */
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

static int xl6ParserTokenTextIs(const XL6_Token* token, const char* text)
{
    size_t len;

    if ( token == NULL || token->text == NULL || text == NULL ) {
        return 0;
    }
    len = strlen(text);
    return token->len == len && strncmp(token->text, text, len) == 0;
}

static int xl6ParserIsContainerLiteralTypeName(const XL6_Token* token)
{
    return xl6ParserTokenTextIs(token, "array") ||
           xl6ParserTokenTextIs(token, "list") ||
           xl6ParserTokenTextIs(token, "set") ||
           xl6ParserTokenTextIs(token, "dict");
}

static void xl6ParserMarkContainerLiteralTypeLt(XL6_ParseTokenStream* stream)
{
    size_t i;
    size_t afterType;
    XL6_TokenKind nextKind;

    if ( stream == NULL ) {
        return;
    }

    for ( i = 0; i + 4 < stream->count; ++i ) {
        if ( !xl6ParserIsContainerLiteralTypeName(&stream->tokens[i]) ) {
            continue;
        }
        if ( stream->tokens[i + 1].kind != XL6_TK_LT ) {
            continue;
        }
        if ( !xl6ParserMatchTypeRef(stream, i, &afterType) || afterType >= stream->count ) {
            continue;
        }

        nextKind = stream->tokens[afterType].kind;
        if ( nextKind == XL6_TK_LBRACKET || nextKind == XL6_TK_LBRACE ) {
            stream->parserKinds[i + 1] = XL6PG_TYPE_LT;
        }
    }
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
        if ( afterType < stream->count &&
             stream->tokens[afterType].kind == XL6_TK_QUESTION ) {
            stream->parserKinds[afterType] = XL6PG_TYPE_QUESTION;
            afterType += 1;
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
    XL6_Token eofToken;
    XL6_Token separator;

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

    /*
     * EOF 和换行一样可以结束最后一条语句。词法器只报告真实源码中的
     * 换行，因此这里在 EOF 前补一个逻辑分隔符，避免要求源文件必须
     * 以换行结尾。这个分隔符只存在于 parser token 流，不修改源码。
     */
    if ( stream->count >= 2 && stream->tokens[stream->count - 2].kind != XL6_TK_SEP ) {
        eofToken = stream->tokens[stream->count - 1];
        stream->count -= 1;
        separator = eofToken;
        separator.kind = XL6_TK_SEP;
        separator.message[0] = '\0';
        if ( !xl6ParseTokenStreamAppend(stream, &separator) ||
             !xl6ParseTokenStreamAppend(stream, &eofToken) ) {
            xl6DiagAdd(diags, eofToken.startLine, eofToken.startColumn,
                       "内存不足，无法补齐文件末尾语句分隔符");
            return 0;
        }
    }

    if ( !xl6ParserSplitNestedGenericShr(stream) ) {
        xl6DiagAdd(diags, token.startLine, token.startColumn, "内存不足，无法拆分嵌套泛型 token");
        return 0;
    }
    if ( !xl6ParserRewriteQualifiedDeclTypes(stream) ) {
        xl6DiagAdd(diags, token.startLine, token.startColumn, "内存不足，无法标记限定类型");
        return 0;
    }
    xl6ParserMarkFunctionTypeTokens(stream);
    if ( !xl6ParserInsertAnonBlockSeparators(stream) ) {
        xl6DiagAdd(diags, token.startLine, token.startColumn, "内存不足，无法补齐匿名函数块分隔符");
        return 0;
    }
    xl6ParserMarkNullableTypeQuestions(stream);
    xl6ParserMarkPostWhileTokens(stream);
    xl6ParserMarkStatementMatchTokens(stream);
    xl6ParserMarkContainerLiteralTypeLt(stream);
    xl6ParserMarkGenericDeclTypeLt(stream);
    return 1;
}

/* Replace exactly one identifier or insert one zero-width member token.
 * The original bytes and all source ranges remain unchanged. */
static int xl6ParserCompletionToken(XL6_ParseTokenStream* stream,const char* raw,XL6_CompletionQuery* q)
{
 const char* text=raw;XL6_Token token;size_t i,j,size,chosen=(size_t)-1;int insert=0;
 if (strlen(text)>=3&&(unsigned char)text[0]==0xef&&(unsigned char)text[1]==0xbb&&(unsigned char)text[2]==0xbf) text+=3;
 size=strlen(text);
 if (q->offset>size||(q->offset<size&&((unsigned char)text[q->offset]&0xc0)==0x80)) {q->error="cursor offset is outside a UTF-8 scalar boundary";return 0;}
 for (i=0;i<stream->count;++i) {
  token=stream->tokens[i];
  if (token.kind==XL6_TK_IDENT&&token.startOffset<=q->offset&&q->offset<=token.endOffset) {chosen=i;break;}
 }
 if (chosen==(size_t)-1) {
  for (i=0;i<stream->count;++i) {
   token=stream->tokens[i];
   if ((token.kind==XL6_TK_DOT||token.kind==XL6_TK_SAFE_DOT)&&token.endOffset==q->offset) {
    chosen=i+1;insert=1;token.startOffset=token.endOffset;token.startLine=token.endLine;token.startColumn=token.endColumn;break;
   }
  }
 } else token=stream->tokens[chosen];
 if (chosen==(size_t)-1) {q->error="cursor is not in an identifier or immediately after a member dot";return 0;}
 q->replacement=xl6AstRangeFromToken(&token);
 if (!q->replacement.known||token.startOffset>q->offset||token.endOffset>size) {q->error="query token has no original source range";return 0;}
 q->prefix=xrtStrDupN(text+token.startOffset,q->offset-token.startOffset);
 if (!q->prefix) {q->error="cannot copy completion prefix";return 0;}
 token.kind=XL6_TK_IDENT;token.text=XL6_COMPLETION_MARKER;token.len=strlen(token.text);token.message[0]=0;
 if (insert) {
  if (!xl6ParseTokenStreamAppend(stream,&token)) {q->error="cannot insert query token";return 0;}
  for (j=stream->count-1;j>chosen;--j) {stream->tokens[j]=stream->tokens[j-1];stream->parserKinds[j]=stream->parserKinds[j-1];}
 }
 stream->tokens[chosen]=token;stream->parserKinds[chosen]=XL6PG_IDENT;return 1;
}

static int xl6ParseSourceInternal(const char* source,const char* fileName,XL6_ParseResult* result,XL6_CompletionQuery* query)
{
    XL6_Lexer lexer;
    XL6_ParseTokenStream stream;
    XL6_ParseContext ctx;
    XL6_Token eofToken;
    void* parser;
    size_t i;
    int ok;

    if ( result == NULL ) {
        return 0;
    }
    result->diags.defaultFile = fileName;
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
    if (query) {
        query->parsed++;
        if (query->parsed != 1) { query->error = "query source is parsed more than once"; ok = 0; }
        else if (ok) ok = xl6ParserCompletionToken(&stream, source, query);
    }
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
    if (!xl6AstSetSourceTree(result->root, fileName)) {
        xl6DiagAdd(&result->diags, 1, 1, "cannot retain source origin");
        ok = 0;
    }
    if (ok && query && result->root && result->root->source) {
        result->root->source->completionQuery = 1;
        result->root->source->completionStart = query->replacement.startOffset;
        result->root->source->completionEnd = query->replacement.endOffset;
    }
    xl6ParseFree(parser, free);
    xl6ParseTokenStreamFree(&stream);
    xl6LexerClose(&lexer);
    return ok && result->diags.count == 0;
}

int xl6ParseSource(const char* source,const char* fileName,XL6_ParseResult* result)
{
 return xl6ParseSourceInternal(source,fileName,result,NULL);
}
int xl6ParseSourceWithCompletion(const char* source,const char* fileName,XL6_ParseResult* result,XL6_CompletionQuery* q)
{
 return xl6ParseSourceInternal(source,fileName,result,xl6CompletionTarget(q,fileName)?q:NULL);
}

int xl6ParseFile(const char* path, XL6_ParseResult* result)
{
    char* source;
    size_t len;
    int ok;

    if (!result || !path) return 0;
    result->diags.defaultFile = path;
    source = xrtFileReadText(path, XENCODING_UTF8, XUTF_STRICT, &len);
    (void)len;
    if ( source == NULL ) {
        xrtFree(source);
        xl6DiagAdd(&result->diags, 1, 1, "无法读取源文件");
        return 0;
    }

    ok = xl6ParseSource(source, path, result);
    xrtFree(source);
    return ok;
}
