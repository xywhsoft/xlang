%top{
#include "src/lexer/xl6_token.h"
}

%{
#include "src/lexer/xl6_lexer_api.h"

#define YY_EXTRA_TYPE XL6_LexerExtra*

typedef void* yyscan_t;
XL6_LexerExtra* xl6get_extra(yyscan_t yyscanner);
char* xl6get_text(yyscan_t yyscanner);
int xl6get_leng(yyscan_t yyscanner);

static int xl6Utf8Columns(const char* text, int len)
{
    int i;
    int cols;

    cols = 0;
    for ( i = 0; i < len; ++i ) {
        unsigned char ch = (unsigned char)text[i];
        if ( (ch & 0xC0) != 0x80 ) {
            cols += 1;
        }
    }
    return cols;
}

static void xl6AdvancePosition(XL6_LexerExtra* extra, const char* text, int len)
{
    int i;

    for ( i = 0; i < len; ++i ) {
        char ch = text[i];
        extra->offset += 1;
        if ( ch == '\r' ) {
            if ( (i + 1) < len && text[i + 1] == '\n' ) {
                extra->offset += 1;
                i += 1;
            }
            extra->line += 1;
            extra->column = 1;
        } else if ( ch == '\n' ) {
            extra->line += 1;
            extra->column = 1;
        } else {
            if ( ((unsigned char)ch & 0xC0) != 0x80 ) {
                extra->column += 1;
            }
        }
    }
}

static void xl6SetToken(yyscan_t yyscanner, XL6_TokenKind kind)
{
    XL6_LexerExtra* extra;
    XL6_Token* token;

    extra = xl6get_extra(yyscanner);
    token = &extra->current;
    memset(token, 0, sizeof(*token));
    token->kind = kind;
    token->text = xl6get_text(yyscanner);
    token->len = (size_t)xl6get_leng(yyscanner);
    token->startOffset = extra->tokenStartOffset;
    token->endOffset = extra->offset;
    token->startLine = extra->tokenStartLine;
    token->startColumn = extra->tokenStartColumn;
    token->endLine = extra->line;
    token->endColumn = extra->column;
}

static int xl6SetSepToken(yyscan_t yyscanner)
{
    XL6_LexerExtra* extra;

    extra = xl6get_extra(yyscanner);
    if ( (extra->parenDepth > 0) || (extra->bracketDepth > 0) || (extra->braceDepth > 0) ) {
        return 0;
    }
    xl6SetToken(yyscanner, XL6_TK_SEP);
    return 1;
}

static int xl6SetErrorToken(yyscan_t yyscanner, const char* message)
{
    XL6_LexerExtra* extra;

    extra = xl6get_extra(yyscanner);
    extra->hadError = 1;
    snprintf(extra->errorText, sizeof(extra->errorText), "%s", message);
    xl6SetToken(yyscanner, XL6_TK_ERROR);
    snprintf(extra->current.message, sizeof(extra->current.message), "%s", message);
    return XL6_TK_ERROR;
}

static int xl6SetDocToken(yyscan_t yyscanner)
{
    XL6_LexerExtra* extra;
    XL6_Token* token;

    extra = xl6get_extra(yyscanner);
    token = &extra->current;
    memset(token, 0, sizeof(*token));
    token->kind = XL6_TK_DOC;
    token->text = extra->sourceText != NULL ? extra->sourceText + extra->docStartOffset : xl6get_text(yyscanner);
    token->len = extra->offset >= extra->docStartOffset ? extra->offset - extra->docStartOffset : 0;
    token->startOffset = extra->docStartOffset;
    token->endOffset = extra->offset;
    token->startLine = extra->docStartLine;
    token->startColumn = extra->docStartColumn;
    token->endLine = extra->line;
    token->endColumn = extra->column;
    return XL6_TK_DOC;
}

static XL6_TokenKind xl6LookupKeyword(const char* text, int len)
{
    typedef struct XL6_Keyword {
        const char* name;
        XL6_TokenKind kind;
    } XL6_Keyword;

    static const XL6_Keyword keywords[] = {
        {"null", XL6_KW_NULL}, {"true", XL6_KW_TRUE}, {"false", XL6_KW_FALSE},
        {"func", XL6_KW_FUNC}, {"const", XL6_KW_CONST}, {"var", XL6_KW_VAR}, {"auto", XL6_KW_AUTO},
        {"typedef", XL6_KW_TYPEDEF}, {"as", XL6_KW_AS},
        {"if", XL6_KW_IF}, {"then", XL6_KW_THEN}, {"else", XL6_KW_ELSE}, {"elseif", XL6_KW_ELSEIF}, {"end", XL6_KW_END},
        {"for", XL6_KW_FOR}, {"in", XL6_KW_IN}, {"is", XL6_KW_IS}, {"while", XL6_KW_WHILE}, {"do", XL6_KW_DO},
        {"break", XL6_KW_BREAK}, {"continue", XL6_KW_CONTINUE}, {"return", XL6_KW_RETURN},
        {"async", XL6_KW_ASYNC}, {"await", XL6_KW_AWAIT}, {"yield", XL6_KW_YIELD},
        {"try", XL6_KW_TRY}, {"catch", XL6_KW_CATCH}, {"finally", XL6_KW_FINALLY},
        {"throw", XL6_KW_THROW}, {"assert", XL6_KW_ASSERT},
        {"import", XL6_KW_IMPORT}, {"from", XL6_KW_FROM}, {"export", XL6_KW_EXPORT}, {"extern", XL6_KW_EXTERN}, {"include", XL6_KW_INCLUDE}, {"define", XL6_KW_DEFINE},
        {"class", XL6_KW_CLASS}, {"protocol", XL6_KW_PROTOCOL}, {"enum", XL6_KW_ENUM},
        {"extends", XL6_KW_EXTENDS}, {"union", XL6_KW_UNION}, {"derive", XL6_KW_DERIVE},
        {"get", XL6_KW_GET}, {"let", XL6_KW_LET},
        {"public", XL6_KW_PUBLIC}, {"private", XL6_KW_PRIVATE}, {"protected", XL6_KW_PROTECTED}, {"static", XL6_KW_STATIC},
        {"override", XL6_KW_OVERRIDE}, {"final", XL6_KW_FINAL}, {"virtual", XL6_KW_VIRTUAL}, {"native", XL6_KW_NATIVE},
        {"c_vararg", XL6_KW_C_VARARG}, {"c_callback", XL6_KW_C_CALLBACK},
        {"match", XL6_KW_MATCH}, {"case", XL6_KW_CASE},
        {"and", XL6_KW_AND}, {"or", XL6_KW_OR}, {"not", XL6_KW_NOT}
    };
    size_t i;

    for ( i = 0; i < XL6_ARRAY_COUNT(keywords); ++i ) {
        if ( (strlen(keywords[i].name) == (size_t)len) && (memcmp(keywords[i].name, text, (size_t)len) == 0) ) {
            return keywords[i].kind;
        }
    }
    return XL6_TK_IDENT;
}

#define YY_USER_ACTION \
    yyextra->tokenStartOffset = yyextra->offset; \
    yyextra->tokenStartLine = yyextra->line; \
    yyextra->tokenStartColumn = yyextra->column; \
    xl6AdvancePosition(yyextra, yytext, yyleng);
%}

%option reentrant
%option noyywrap
%option prefix="xl6"
%option extra-type="XL6_LexerExtra*"
%option warn
%option noinput
%option nounput

%x COMMENT DOC

DIGIT          [0-9]
HEXDIGIT       [0-9a-fA-F]
OCTDIGIT       [0-7]
BINDIGIT       [01]
ASCII_START    [a-zA-Z_]
ASCII_CONT     [a-zA-Z0-9_]
UTF8_CONT      [\x80-\xBF]
UTF8_2         [\xC2-\xDF]{UTF8_CONT}
UTF8_3         [\xE0-\xEF]{UTF8_CONT}{2}
UTF8_4         [\xF0-\xF4]{UTF8_CONT}{3}
UTF8_ANY       ({UTF8_2}|{UTF8_3}|{UTF8_4})
IDENT_START    ({ASCII_START}|{UTF8_ANY})
IDENT_CONT     ({ASCII_CONT}|{UTF8_ANY})
IDENT          {IDENT_START}{IDENT_CONT}*
DEC_INT        {DIGIT}({DIGIT}|_)*
HEX_INT        0x{HEXDIGIT}({HEXDIGIT}|_)*
OCT_INT        0o{OCTDIGIT}({OCTDIGIT}|_)*
BIN_INT        0b{BINDIGIT}({BINDIGIT}|_)*
FLOAT_A        {DIGIT}({DIGIT}|_)*\.{DIGIT}({DIGIT}|_)*([eE][+-]?{DIGIT}({DIGIT}|_)*)?
FLOAT_B        {DIGIT}({DIGIT}|_)*[eE][+-]?{DIGIT}({DIGIT}|_)*

%%

[ \t\f\v]+                         { }
\r\n                               { if ( xl6SetSepToken(yyscanner) ) return XL6_TK_SEP; }
\n                                 { if ( xl6SetSepToken(yyscanner) ) return XL6_TK_SEP; }
\r                                 { if ( xl6SetSepToken(yyscanner) ) return XL6_TK_SEP; }

^[ \t]*"#doc"[ \t]*(\r\n|\n|\r)    { yyextra->docStartOffset = yyextra->tokenStartOffset; yyextra->docStartLine = yyextra->tokenStartLine; yyextra->docStartColumn = yyextra->tokenStartColumn; BEGIN(DOC); }
<DOC>^[ \t]*"#end"[ \t]*(\r\n|\n|\r)? { BEGIN(INITIAL); return xl6SetDocToken(yyscanner); }
<DOC><<EOF>>                       { BEGIN(INITIAL); return xl6SetErrorToken(yyscanner, "未闭合的 #doc 文档块"); }
<DOC>.|\r\n|\n|\r                  { }

"//"[^\r\n]*                       { }
"/*"                               { yyextra->commentDepth = 1; BEGIN(COMMENT); }
<COMMENT>"/*"                      { yyextra->commentDepth += 1; }
<COMMENT>"*/"                      { yyextra->commentDepth -= 1; if ( yyextra->commentDepth <= 0 ) BEGIN(INITIAL); }
<COMMENT><<EOF>>                   { BEGIN(INITIAL); return xl6SetErrorToken(yyscanner, "未闭合的多行注释"); }
<COMMENT>.|\r\n|\n|\r              { }

";"                                { xl6SetToken(yyscanner, XL6_TK_SEP); return XL6_TK_SEP; }

"rf\"\"\""([^"]|\"[^"]|\"\"[^"])*"\"\"\"" { xl6SetToken(yyscanner, XL6_TK_FSTRING); return XL6_TK_FSTRING; }
"f\"\"\""([^"]|\"[^"]|\"\"[^"])*"\"\"\"" { xl6SetToken(yyscanner, XL6_TK_FSTRING); return XL6_TK_FSTRING; }
"rf\""[^"\r\n]*"\""               { xl6SetToken(yyscanner, XL6_TK_FSTRING); return XL6_TK_FSTRING; }
"f\""([^"\\\r\n]|\\.)*"\""        { xl6SetToken(yyscanner, XL6_TK_FSTRING); return XL6_TK_FSTRING; }
"rf`"[^`]*"`"                      { xl6SetToken(yyscanner, XL6_TK_FSTRING); return XL6_TK_FSTRING; }
"f`"[^`]*"`"                       { xl6SetToken(yyscanner, XL6_TK_FSTRING); return XL6_TK_FSTRING; }

"br\"\"\""([^"]|\"[^"]|\"\"[^"])*"\"\"\"" { xl6SetToken(yyscanner, XL6_TK_BYTES); return XL6_TK_BYTES; }
"b\"\"\""([^"]|\"[^"]|\"\"[^"])*"\"\"\"" { xl6SetToken(yyscanner, XL6_TK_BYTES); return XL6_TK_BYTES; }
"br\""[^"\r\n]*"\""               { xl6SetToken(yyscanner, XL6_TK_BYTES); return XL6_TK_BYTES; }
"b\""([^"\\\r\n]|\\.)*"\""        { xl6SetToken(yyscanner, XL6_TK_BYTES); return XL6_TK_BYTES; }

"r\"\"\""([^"]|\"[^"]|\"\"[^"])*"\"\"\"" { xl6SetToken(yyscanner, XL6_TK_STRING); return XL6_TK_STRING; }
"\"\"\""([^"]|\"[^"]|\"\"[^"])*"\"\"\"" { xl6SetToken(yyscanner, XL6_TK_STRING); return XL6_TK_STRING; }
"r\""[^"\r\n]*"\""                { xl6SetToken(yyscanner, XL6_TK_STRING); return XL6_TK_STRING; }
"\""([^"\\\r\n]|\\.)*"\""         { xl6SetToken(yyscanner, XL6_TK_STRING); return XL6_TK_STRING; }
"r`"[^`]*"`"                       { xl6SetToken(yyscanner, XL6_TK_STRING); return XL6_TK_STRING; }
"`"[^`]*"`"                        { xl6SetToken(yyscanner, XL6_TK_STRING); return XL6_TK_STRING; }
"'"([^'\\\r\n]|\\.)*"'"           { xl6SetToken(yyscanner, XL6_TK_CHAR); return XL6_TK_CHAR; }
"\""([^"\\\r\n]|\\.)*             { return xl6SetErrorToken(yyscanner, "未闭合的字符串"); }
"'"([^'\\\r\n]|\\.)*               { return xl6SetErrorToken(yyscanner, "未闭合的字符字面量"); }

"#"[^#\r\n]*"#"                   { xl6SetToken(yyscanner, XL6_TK_TIME); return XL6_TK_TIME; }
"#"[^#\r\n]*                       { return xl6SetErrorToken(yyscanner, "未闭合的时间字面量"); }

{FLOAT_A}|{FLOAT_B}                { xl6SetToken(yyscanner, XL6_TK_FLOAT); return XL6_TK_FLOAT; }
{HEX_INT}|{OCT_INT}|{BIN_INT}|{DEC_INT} { xl6SetToken(yyscanner, XL6_TK_INT); return XL6_TK_INT; }

"::"                               { xl6SetToken(yyscanner, XL6_TK_SCOPE); return XL6_TK_SCOPE; }
"=>"                               { xl6SetToken(yyscanner, XL6_TK_FAT_ARROW); return XL6_TK_FAT_ARROW; }
"..."                              { xl6SetToken(yyscanner, XL6_TK_ELLIPSIS); return XL6_TK_ELLIPSIS; }
".."                               { return xl6SetErrorToken(yyscanner, "当前不支持 .. 范围运算符，请使用 range(...)"); }
"***"                              { xl6SetToken(yyscanner, XL6_TK_TRIPLE_STAR); return XL6_TK_TRIPLE_STAR; }
"**="                              { xl6SetToken(yyscanner, XL6_TK_POW_ASSIGN); return XL6_TK_POW_ASSIGN; }
"**"                               { xl6SetToken(yyscanner, XL6_TK_POW); return XL6_TK_POW; }
"+="                               { xl6SetToken(yyscanner, XL6_TK_PLUS_ASSIGN); return XL6_TK_PLUS_ASSIGN; }
"-="                               { xl6SetToken(yyscanner, XL6_TK_MINUS_ASSIGN); return XL6_TK_MINUS_ASSIGN; }
"*="                               { xl6SetToken(yyscanner, XL6_TK_STAR_ASSIGN); return XL6_TK_STAR_ASSIGN; }
"/="                               { xl6SetToken(yyscanner, XL6_TK_SLASH_ASSIGN); return XL6_TK_SLASH_ASSIGN; }
"%="                               { xl6SetToken(yyscanner, XL6_TK_PERCENT_ASSIGN); return XL6_TK_PERCENT_ASSIGN; }
"&="                               { xl6SetToken(yyscanner, XL6_TK_BIT_AND_ASSIGN); return XL6_TK_BIT_AND_ASSIGN; }
"|="                               { xl6SetToken(yyscanner, XL6_TK_BIT_OR_ASSIGN); return XL6_TK_BIT_OR_ASSIGN; }
"^="                               { xl6SetToken(yyscanner, XL6_TK_BIT_XOR_ASSIGN); return XL6_TK_BIT_XOR_ASSIGN; }
"?="                               { xl6SetToken(yyscanner, XL6_TK_NULL_ASSIGN); return XL6_TK_NULL_ASSIGN; }
"??"                               { xl6SetToken(yyscanner, XL6_TK_NULL_COALESCE); return XL6_TK_NULL_COALESCE; }
"?."                               { xl6SetToken(yyscanner, XL6_TK_SAFE_DOT); return XL6_TK_SAFE_DOT; }
"?["                               { yyextra->bracketDepth += 1; xl6SetToken(yyscanner, XL6_TK_SAFE_LBRACKET); return XL6_TK_SAFE_LBRACKET; }
"=="                               { xl6SetToken(yyscanner, XL6_TK_EQ); return XL6_TK_EQ; }
"!="                               { xl6SetToken(yyscanner, XL6_TK_NE); return XL6_TK_NE; }
"<="                               { xl6SetToken(yyscanner, XL6_TK_LE); return XL6_TK_LE; }
">="                               { xl6SetToken(yyscanner, XL6_TK_GE); return XL6_TK_GE; }
"~="                               { xl6SetToken(yyscanner, XL6_TK_APPROX); return XL6_TK_APPROX; }
"<<"                               { xl6SetToken(yyscanner, XL6_TK_SHL); return XL6_TK_SHL; }
">>"                               { xl6SetToken(yyscanner, XL6_TK_SHR); return XL6_TK_SHR; }

"("                                { yyextra->parenDepth += 1; xl6SetToken(yyscanner, XL6_TK_LPAREN); return XL6_TK_LPAREN; }
")"                                { if ( yyextra->parenDepth > 0 ) yyextra->parenDepth -= 1; xl6SetToken(yyscanner, XL6_TK_RPAREN); return XL6_TK_RPAREN; }
"["                                { yyextra->bracketDepth += 1; xl6SetToken(yyscanner, XL6_TK_LBRACKET); return XL6_TK_LBRACKET; }
"]"                                { if ( yyextra->bracketDepth > 0 ) yyextra->bracketDepth -= 1; xl6SetToken(yyscanner, XL6_TK_RBRACKET); return XL6_TK_RBRACKET; }
"{"                                { yyextra->braceDepth += 1; xl6SetToken(yyscanner, XL6_TK_LBRACE); return XL6_TK_LBRACE; }
"}"                                { if ( yyextra->braceDepth > 0 ) yyextra->braceDepth -= 1; xl6SetToken(yyscanner, XL6_TK_RBRACE); return XL6_TK_RBRACE; }
","                                { xl6SetToken(yyscanner, XL6_TK_COMMA); return XL6_TK_COMMA; }
":"                                { xl6SetToken(yyscanner, XL6_TK_COLON); return XL6_TK_COLON; }
"."                                { xl6SetToken(yyscanner, XL6_TK_DOT); return XL6_TK_DOT; }
"@"                                { xl6SetToken(yyscanner, XL6_TK_AT); return XL6_TK_AT; }
"="                                { xl6SetToken(yyscanner, XL6_TK_ASSIGN); return XL6_TK_ASSIGN; }
"+"                                { xl6SetToken(yyscanner, XL6_TK_PLUS); return XL6_TK_PLUS; }
"-"                                { xl6SetToken(yyscanner, XL6_TK_MINUS); return XL6_TK_MINUS; }
"*"                                { xl6SetToken(yyscanner, XL6_TK_STAR); return XL6_TK_STAR; }
"/"                                { xl6SetToken(yyscanner, XL6_TK_SLASH); return XL6_TK_SLASH; }
"%"                                { xl6SetToken(yyscanner, XL6_TK_PERCENT); return XL6_TK_PERCENT; }
"<"                                { xl6SetToken(yyscanner, XL6_TK_LT); return XL6_TK_LT; }
">"                                { xl6SetToken(yyscanner, XL6_TK_GT); return XL6_TK_GT; }
"?"                                { xl6SetToken(yyscanner, XL6_TK_QUESTION); return XL6_TK_QUESTION; }
"!"                                { xl6SetToken(yyscanner, XL6_TK_BANG); return XL6_TK_BANG; }
"~"                                { xl6SetToken(yyscanner, XL6_TK_BIT_NOT); return XL6_TK_BIT_NOT; }
"&"                                { xl6SetToken(yyscanner, XL6_TK_BIT_AND); return XL6_TK_BIT_AND; }
"|"                                { xl6SetToken(yyscanner, XL6_TK_BIT_OR); return XL6_TK_BIT_OR; }
"^"                                { xl6SetToken(yyscanner, XL6_TK_BIT_XOR); return XL6_TK_BIT_XOR; }

{IDENT}                             { XL6_TokenKind kind = xl6LookupKeyword(yytext, yyleng); xl6SetToken(yyscanner, kind); return kind; }

<<EOF>>                             { xl6SetToken(yyscanner, XL6_TK_EOF); return XL6_TK_EOF; }
.                                   { return xl6SetErrorToken(yyscanner, "无法识别的字符"); }

%%
