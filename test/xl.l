/*
 * xlang - 词法分析器定义文件
 * 使用 flex 生成词法分析器
 */

%option reentrant
%option bison-bridge
%option noyywrap
%option nounput
%option noinput
%option yylineno
%option outfile="xl_lex.c"
%option header-file="xl_lex.h"
%option prefix="xl_"

%{
// 定义 YYSTYPE 为 XL_Token*
#define YYSTYPE XL_Token*

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xl_token.h"
#include "xl_ast.h"

// 先取消可能冲突的宏
#undef TRUE
#undef FALSE
#undef NIL

// 包含 lemon 生成的 token 定义
#include "xl_parser.h"

// 将 TK_ 前缀映射到 lemon 的 token 名称
#define TK_VAR       VAR
#define TK_NIL       NIL
#define TK_TRUE      TRUE
#define TK_FALSE     FALSE
#define TK_IF        IF
#define TK_THEN      THEN
#define TK_ELSEIF    ELSEIF
#define TK_ELSE      ELSE
#define TK_END       END
#define TK_WHILE     WHILE
#define TK_DO        DO
#define TK_FOR       FOR
#define TK_TO        TO
#define TK_STEP      STEP
// TK_IN 未使用，定义为错误值
#define TK_IN        (-100)
#define TK_BREAK     BREAK
#define TK_CONTINUE  CONTINUE
#define TK_FUNCTION  FUNCTION
#define TK_RETURN    RETURN
#define TK_AND       AND
#define TK_OR        OR
#define TK_NOT       NOT
#define TK_LPAREN    LPAREN
#define TK_RPAREN    RPAREN
#define TK_LBRACKET  LBRACKET
#define TK_RBRACKET  RBRACKET
#define TK_LBRACE    LBRACE
#define TK_RBRACE    RBRACE
#define TK_PLUS      PLUS
#define TK_MINUS     MINUS
#define TK_MUL       MUL
#define TK_DIV       DIV
#define TK_MOD       MOD
#define TK_POW       POW
#define TK_EQ        EQ
#define TK_NE        NE
#define TK_LT        LT
#define TK_LE        LE
#define TK_GT        GT
#define TK_GE        GE
#define TK_ASSIGN    ASSIGN
#define TK_PLUSEQ    PLUSEQ
#define TK_MINUSEQ   MINUSEQ
#define TK_MULEQ     MULEQ
#define TK_DIVEQ     DIVEQ
#define TK_INC       INC
#define TK_DEC       DEC
#define TK_COMMA     COMMA
#define TK_SEMI      SEMI
#define TK_DOT       DOT
// TK_COLON, TK_QUEST 未使用，定义为错误值
#define TK_COLON     (-101)
#define TK_QUEST     (-102)
#define TK_INTEGER   INTEGER
#define TK_NUMBER    NUMBER
#define TK_STRING    STRING
#define TK_IDENT     IDENT

// 当前行号和列号
static int xl_col = 1;

// 更新列号
#define YY_USER_ACTION xl_col += yyleng;

// 简化返回 token 的宏
#define RETURN_TOKEN(t) do { \
    *yylval = make_token((t), yytext, yyleng, yylineno, xl_col - yyleng); \
    return (t); \
} while(0)

// 创建 token
static XL_Token* make_token(int type, const char* text, int len, int line, int col) {
    XL_Token* tok = (XL_Token*)malloc(sizeof(XL_Token));
    tok->type = type;
    tok->text = strdup(text);
    tok->len = len;
    tok->line = line;
    tok->col = col;
    tok->ival = 0;
    return tok;
}

// 创建整数 token
static XL_Token* make_int_token(const char* text, int len, int line, int col) {
    XL_Token* tok = make_token(TK_INTEGER, text, len, line, col);
    tok->ival = strtoll(text, 0, 10);
    return tok;
}

// 创建十六进制整数 token
static XL_Token* make_hex_token(const char* text, int len, int line, int col) {
    XL_Token* tok = make_token(TK_INTEGER, text, len, line, col);
    tok->ival = strtoll(text, 0, 16);
    return tok;
}

// 创建浮点数 token
static XL_Token* make_float_token(const char* text, int len, int line, int col) {
    XL_Token* tok = make_token(TK_NUMBER, text, len, line, col);
    tok->fval = strtod(text, 0);
    return tok;
}

// 创建字符串 token（去除引号）
static XL_Token* make_string_token(const char* text, int len, int line, int col) {
    XL_Token* tok = make_token(TK_STRING, text, len, line, col);
    // 去掉首尾引号，处理转义
    if (len >= 2) {
        char* str = (char*)malloc(len);
        int j = 0;
        for (int i = 1; i < len - 1; i++) {
            if (text[i] == '\\' && i + 1 < len - 1) {
                i++;
                switch (text[i]) {
                    case 'n': str[j++] = '\n'; break;
                    case 'r': str[j++] = '\r'; break;
                    case 't': str[j++] = '\t'; break;
                    case '\\': str[j++] = '\\'; break;
                    case '"': str[j++] = '"'; break;
                    case '\'': str[j++] = '\''; break;
                    default: str[j++] = text[i]; break;
                }
            } else {
                str[j++] = text[i];
            }
        }
        str[j] = '\0';
        tok->sval = str;
    } else {
        tok->sval = strdup("");
    }
    return tok;
}

// 创建单引号字符串 token（不处理转义，除了两个单引号转一个）
static XL_Token* make_raw_string_token(const char* text, int len, int line, int col) {
    XL_Token* tok = make_token(TK_STRING, text, len, line, col);
    if (len >= 2) {
        char* str = (char*)malloc(len);
        int j = 0;
        for (int i = 1; i < len - 1; i++) {
            if (text[i] == '\'' && i + 1 < len - 1 && text[i+1] == '\'') {
                str[j++] = '\'';
                i++;
            } else {
                str[j++] = text[i];
            }
        }
        str[j] = '\0';
        tok->sval = str;
    } else {
        tok->sval = strdup("");
    }
    return tok;
}

%}

/* 空白字符 */
WS          [ \t\r]+
NL          \n

/* 注释 */
LINE_COMMENT    "//"[^\n]*
BLOCK_COMMENT   "/*"([^*]|\*+[^*/])*\*+"/"

/* 数字 */
INTEGER     [0-9]+
HEX         0[xX][0-9a-fA-F]+
NUMBER      [0-9]+\.[0-9]+([eE][+-]?[0-9]+)?

/* 字符串 */
DSTRING     \"([^\"\n\\]|\\.)*\"
SSTRING     \'([^\'\n]|\'\')*\'

/* 标识符 */
IDENT       [a-zA-Z_][a-zA-Z0-9_]*

%%

 /* 空白和换行 */
{WS}                { /* 忽略空白 */ }
{NL}                { xl_col = 1; }

 /* 注释 */
{LINE_COMMENT}      { /* 忽略行注释 */ }
{BLOCK_COMMENT}     { /* 忽略块注释 */ }

 /* 关键字 */
"var"               { RETURN_TOKEN(TK_VAR); }
"null"              { RETURN_TOKEN(TK_NIL); }
"true"              { RETURN_TOKEN(TK_TRUE); }
"false"             { RETURN_TOKEN(TK_FALSE); }
"if"                { RETURN_TOKEN(TK_IF); }
"then"              { RETURN_TOKEN(TK_THEN); }
"elseif"            { RETURN_TOKEN(TK_ELSEIF); }
"else"              { RETURN_TOKEN(TK_ELSE); }
"end"               { RETURN_TOKEN(TK_END); }
"while"             { RETURN_TOKEN(TK_WHILE); }
"do"                { RETURN_TOKEN(TK_DO); }
"for"               { RETURN_TOKEN(TK_FOR); }
"to"                { RETURN_TOKEN(TK_TO); }
"step"              { RETURN_TOKEN(TK_STEP); }
"in"                { RETURN_TOKEN(TK_IN); }
"break"             { RETURN_TOKEN(TK_BREAK); }
"continue"          { RETURN_TOKEN(TK_CONTINUE); }
"function"          { RETURN_TOKEN(TK_FUNCTION); }
"return"            { RETURN_TOKEN(TK_RETURN); }
"and"               { RETURN_TOKEN(TK_AND); }
"or"                { RETURN_TOKEN(TK_OR); }
"not"               { RETURN_TOKEN(TK_NOT); }

 /* 双字符运算符 */
"**"                { RETURN_TOKEN(TK_POW); }
"=="                { RETURN_TOKEN(TK_EQ); }
"!="                { RETURN_TOKEN(TK_NE); }
"<="                { RETURN_TOKEN(TK_LE); }
">="                { RETURN_TOKEN(TK_GE); }
"+="                { RETURN_TOKEN(TK_PLUSEQ); }
"-="                { RETURN_TOKEN(TK_MINUSEQ); }
"*="                { RETURN_TOKEN(TK_MULEQ); }
"/="                { RETURN_TOKEN(TK_DIVEQ); }
"++"                { RETURN_TOKEN(TK_INC); }
"--"                { RETURN_TOKEN(TK_DEC); }

 /* 单字符运算符和分隔符 */
"+"                 { RETURN_TOKEN(TK_PLUS); }
"-"                 { RETURN_TOKEN(TK_MINUS); }
"*"                 { RETURN_TOKEN(TK_MUL); }
"/"                 { RETURN_TOKEN(TK_DIV); }
"%"                 { RETURN_TOKEN(TK_MOD); }
"<"                 { RETURN_TOKEN(TK_LT); }
">"                 { RETURN_TOKEN(TK_GT); }
"="                 { RETURN_TOKEN(TK_ASSIGN); }
"("                 { RETURN_TOKEN(TK_LPAREN); }
")"                 { RETURN_TOKEN(TK_RPAREN); }
"["                 { RETURN_TOKEN(TK_LBRACKET); }
"]"                 { RETURN_TOKEN(TK_RBRACKET); }
"{"                 { RETURN_TOKEN(TK_LBRACE); }
"}"                 { RETURN_TOKEN(TK_RBRACE); }
","                 { RETURN_TOKEN(TK_COMMA); }
";"                 { RETURN_TOKEN(TK_SEMI); }
"."                 { RETURN_TOKEN(TK_DOT); }
":"                 { RETURN_TOKEN(TK_COLON); }
"?"                 { RETURN_TOKEN(TK_QUEST); }

 /* 数字字面量 */
{HEX}               { 
                        *yylval = make_hex_token(yytext, yyleng, yylineno, xl_col - yyleng);
                        return TK_INTEGER;
                    }
{INTEGER}           { 
                        *yylval = make_int_token(yytext, yyleng, yylineno, xl_col - yyleng);
                        return TK_INTEGER;
                    }
{NUMBER}            { 
                        *yylval = make_float_token(yytext, yyleng, yylineno, xl_col - yyleng);
                        return TK_NUMBER;
                    }

 /* 字符串字面量 */
{DSTRING}           { 
                        *yylval = make_string_token(yytext, yyleng, yylineno, xl_col - yyleng);
                        return TK_STRING;
                    }
{SSTRING}           { 
                        *yylval = make_raw_string_token(yytext, yyleng, yylineno, xl_col - yyleng);
                        return TK_STRING;
                    }

 /* 标识符 */
{IDENT}             { 
                        *yylval = make_token(TK_IDENT, yytext, yyleng, yylineno, xl_col - yyleng);
                        return TK_IDENT;
                    }

 /* 未知字符 */
.                   { 
                        fprintf(stderr, "Unknown character '%c' at line %d\n", yytext[0], yylineno);
                        return -1;
                    }

%%

/* 用户自定义代码 */
