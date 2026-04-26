/**
 * xlang v4 词法分析器
 * 
 * 使用 Flex 生成可重入的词法分析器
 * 支持 xlang v4 一阶段所有 Token 类型
 * 
 * 设计原则：
 * - Lexer 识别所有关键字（控制流、函数声明、类型名等）
 * - 12 个类型名硬关键字（bool, int, float, string 等）
 * - 软关键字（i8, u8 等）作为普通标识符，Parser 查表判断
 * - Token 类型定义在 xl_tokens.h 中共享
 */

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Token 类型和结构体定义在 xl_tokens.h 中（由 main.c 先 include） */
/* 此处仅使用这些类型，不再重复定义 */

#define YY_EXTRA_TYPE XL_LexerExtra*

/* Flex 函数前向声明（可重入模式） */
typedef void* yyscan_t;
XL_LexerExtra* xlget_extra(yyscan_t scanner);
char* xlget_text(yyscan_t scanner);
int xlget_leng(yyscan_t scanner);
int xlget_lineno(yyscan_t scanner);

/* 更新列号 */
#define YY_USER_ACTION \
	yyextra->prevColumn = yyextra->column; \
	yyextra->column += yyleng;

/* 创建 Token */
static XL_Token* xlMakeToken(yyscan_t scanner, XL_TokenType type)
{
	XL_LexerExtra* extra = yyget_extra(scanner);
	extra->currentToken.type = type;
	extra->currentToken.text = yyget_text(scanner);
	extra->currentToken.length = yyget_leng(scanner);
	extra->currentToken.line = yyget_lineno(scanner);
	extra->currentToken.column = extra->prevColumn;
	return &extra->currentToken;
}

/* 处理换行 */
static void xlNewline(yyscan_t scanner)
{
	XL_LexerExtra* extra = yyget_extra(scanner);
	extra->column = 1;
}

/* 解析整数（十进制） */
static int64_t xlParseIntDec(const char* text)
{
	int64_t result = 0;
	while ( *text ) {
		if ( *text != '_' ) {
			result = result * 10 + (*text - '0');
		}
		text++;
	}
	return result;
}

/* 解析整数（十六进制） */
static int64_t xlParseIntHex(const char* text)
{
	int64_t result = 0;
	/* 跳过 0x 或 &H */
	if ( (text[0] == '0') && ((text[1] == 'x') || (text[1] == 'X')) ) {
		text += 2;
	} else if ( (text[0] == '&') && ((text[1] == 'h') || (text[1] == 'H')) ) {
		text += 2;
	}
	while ( *text ) {
		if ( *text != '_' ) {
			char c = *text;
			int digit;
			if ( (c >= '0') && (c <= '9') ) {
				digit = c - '0';
			} else if ( (c >= 'a') && (c <= 'f') ) {
				digit = c - 'a' + 10;
			} else if ( (c >= 'A') && (c <= 'F') ) {
				digit = c - 'A' + 10;
			} else {
				break;
			}
			result = result * 16 + digit;
		}
		text++;
	}
	return result;
}

/* 解析整数（八进制） */
static int64_t xlParseIntOct(const char* text)
{
	int64_t result = 0;
	/* 跳过 &O */
	text += 2;
	while ( *text ) {
		if ( *text != '_' ) {
			result = result * 8 + (*text - '0');
		}
		text++;
	}
	return result;
}

/* 解析整数（二进制） */
static int64_t xlParseIntBin(const char* text)
{
	int64_t result = 0;
	/* 跳过 0b 或 &B */
	if ( (text[0] == '0') && ((text[1] == 'b') || (text[1] == 'B')) ) {
		text += 2;
	} else if ( (text[0] == '&') && ((text[1] == 'b') || (text[1] == 'B')) ) {
		text += 2;
	}
	while ( *text ) {
		if ( *text != '_' ) {
			result = result * 2 + (*text - '0');
		}
		text++;
	}
	return result;
}

%}

/* Flex 选项 */
%option reentrant
%option noyywrap
%option yylineno
%option extra-type="XL_LexerExtra*"
%option prefix="xl"
%option outfile="xl_lexer_impl.h"
%option header-file="xl_lexer.h"

/* 状态定义 */
%x COMMENT_MULTI
%x STRING_DOUBLE
%x STRING_SINGLE
%x STRING_MULTI_DOUBLE
%x STRING_MULTI_SINGLE
%x TIME_LITERAL

/* 字符类定义 */
DIGIT       [0-9]
HEXDIGIT    [0-9a-fA-F]
OCTDIGIT    [0-7]
BINDIGIT    [01]
LETTER      [a-zA-Z_]
IDENT_CHAR  [a-zA-Z0-9_]
/* 支持中文标识符 (UTF-8) */
UTF8_CHAR   [\x80-\xBF]
UTF8_2      [\xC2-\xDF]{UTF8_CHAR}
UTF8_3      [\xE0-\xEF]{UTF8_CHAR}{2}
UTF8_4      [\xF0-\xF4]{UTF8_CHAR}{3}
UTF8_ANY    {UTF8_2}|{UTF8_3}|{UTF8_4}

%%

	/* === 空白字符 === */
[ \t\r]+                { /* 忽略空白 */ }
\n                      { xlNewline(yyscanner); return XL_TOK_NEWLINE; }

	/* === 注释 === */
"//"[^\n]*              { /* 单行注释 */ }
"/*"                    { BEGIN(COMMENT_MULTI); }
<COMMENT_MULTI>"*/"     { BEGIN(INITIAL); }
<COMMENT_MULTI>\n       { xlNewline(yyscanner); }
<COMMENT_MULTI>.        { /* 忽略注释内容 */ }

	/* === 多运算符（必须在单字符之前匹配） === */
"**="                   { return XL_OP_POWER_ASSIGN; }
"**"                    { return XL_OP_POWER; }
"***"                   { return XL_OP_ELLIPSIS_DICT; }
"..."                   { return XL_OP_ELLIPSIS; }
".."                    { return XL_OP_RANGE; }
"?."                    { return XL_OP_SAFE_DOT; }
"?["                    { return XL_OP_SAFE_BRACKET; }
"??"                    { return XL_OP_NULL_COALESCE; }
"?="                    { return XL_OP_NULL_ASSIGN; }
"+="                    { return XL_OP_PLUS_ASSIGN; }
"-="                    { return XL_OP_MINUS_ASSIGN; }
"*="                    { return XL_OP_STAR_ASSIGN; }
"/="                    { return XL_OP_SLASH_ASSIGN; }
"%="                    { return XL_OP_PERCENT_ASSIGN; }
"++"                    { return XL_OP_INC; }
"--"                    { return XL_OP_DEC; }
">="                    { return XL_OP_GE; }
"<="                    { return XL_OP_LE; }
"!="                    { return XL_OP_NE; }
"=="                    { return XL_OP_EQ; }
"~="                    { return XL_OP_APPROX; }
">>"                    { return XL_OP_SHR; }
"<<"                    { return XL_OP_SHL; }

	/* === 单字符运算符 === */
">"                     { return XL_OP_GT; }
"<"                     { return XL_OP_LT; }
"+"                     { return XL_OP_PLUS; }
"-"                     { return XL_OP_MINUS; }
"*"                     { return XL_OP_STAR; }
"/"                     { return XL_OP_SLASH; }
"%"                     { return XL_OP_PERCENT; }
"&"                     { return XL_OP_AMP; }
"|"                     { return XL_OP_PIPE; }
"!"                     { return XL_OP_BANG; }
"^"                     { return XL_OP_CARET; }
"="                     { return XL_OP_ASSIGN; }
"?"                     { return XL_OP_QUESTION; }
":"                     { return XL_OP_COLON; }
"."                     { return XL_OP_DOT; }

	/* === 分隔符 === */
"("                     { return XL_TOK_LPAREN; }
")"                     { return XL_TOK_RPAREN; }
"["                     { return XL_TOK_LBRACKET; }
"]"                     { return XL_TOK_RBRACKET; }
"{"                     { return XL_TOK_LBRACE; }
"}"                     { return XL_TOK_RBRACE; }
","                     { return XL_TOK_COMMA; }
";"                     { return XL_TOK_SEMICOLON; }

	/* === 数字字面量 === */
{DIGIT}({DIGIT}|_)*\.({DIGIT}|_)+([eE][+-]?{DIGIT}+)?   { 
	xlMakeToken(yyscanner, XL_TOK_FLOAT)->fVal = atof(yytext);
	return XL_TOK_FLOAT; 
}
{DIGIT}({DIGIT}|_)*[eE][+-]?{DIGIT}+   { 
	xlMakeToken(yyscanner, XL_TOK_FLOAT)->fVal = atof(yytext);
	return XL_TOK_FLOAT; 
}
0[xX]({HEXDIGIT}|_)+    { 
	xlMakeToken(yyscanner, XL_TOK_INT_HEX)->iVal = xlParseIntHex(yytext);
	return XL_TOK_INT_HEX; 
}
"&"[hH]({HEXDIGIT}|_)+  { 
	xlMakeToken(yyscanner, XL_TOK_INT_HEX)->iVal = xlParseIntHex(yytext);
	return XL_TOK_INT_HEX; 
}
"&"[oO]({OCTDIGIT}|_)+  { 
	xlMakeToken(yyscanner, XL_TOK_INT_OCT)->iVal = xlParseIntOct(yytext);
	return XL_TOK_INT_OCT; 
}
0[bB]({BINDIGIT}|_)+    { 
	xlMakeToken(yyscanner, XL_TOK_INT_BIN)->iVal = xlParseIntBin(yytext);
	return XL_TOK_INT_BIN; 
}
"&"[bB]({BINDIGIT}|_)+  { 
	xlMakeToken(yyscanner, XL_TOK_INT_BIN)->iVal = xlParseIntBin(yytext);
	return XL_TOK_INT_BIN; 
}
{DIGIT}({DIGIT}|_)*     { 
	xlMakeToken(yyscanner, XL_TOK_INT)->iVal = xlParseIntDec(yytext);
	return XL_TOK_INT; 
}

	/* === 时间字面量 === */
#[^#\n]+#               { return XL_TOK_TIME; }

	/* === 字符串字面量 === */
\"\"\"                  { BEGIN(STRING_MULTI_DOUBLE); }
\'\'\'                  { BEGIN(STRING_MULTI_SINGLE); }
\"([^"\\]|\\.)*\"       { return XL_TOK_STRING; }
\'[^']*\'               { return XL_TOK_STRING_RAW; }

<STRING_MULTI_DOUBLE>\"\"\"   { BEGIN(INITIAL); return XL_TOK_STRING_MULTI; }
<STRING_MULTI_DOUBLE>\n       { xlNewline(yyscanner); }
<STRING_MULTI_DOUBLE>.        { /* 累积字符 */ }

<STRING_MULTI_SINGLE>\'\'\'   { BEGIN(INITIAL); return XL_TOK_STRING_MULTI_RAW; }
<STRING_MULTI_SINGLE>\n       { xlNewline(yyscanner); }
<STRING_MULTI_SINGLE>.        { /* 累积字符 */ }

	/* === 关键字（必须在标识符之前匹配） === */

	/* 内置类型关键字 */
"null"                  { return XL_KW_NULL; }
"function"              { return XL_KW_FUNCTION; }

	/* 布尔字面量 */
"true"                  { return XL_KW_TRUE; }
"false"                 { return XL_KW_FALSE; }

	/* 声明关键字 */
"var"                   { return XL_KW_VAR; }
"auto"                  { return XL_KW_AUTO; }

	/* 控制流关键字 */
"if"                    { return XL_KW_IF; }
"then"                  { return XL_KW_THEN; }
"else"                  { return XL_KW_ELSE; }
"elseif"                { return XL_KW_ELSEIF; }
"end"                   { return XL_KW_END; }
"for"                   { return XL_KW_FOR; }
"in"                    { return XL_KW_IN; }
"to"                    { return XL_KW_TO; }
"step"                  { return XL_KW_STEP; }
"while"                 { return XL_KW_WHILE; }
"do"                    { return XL_KW_DO; }
"break"                 { return XL_KW_BREAK; }
"continue"              { return XL_KW_CONTINUE; }
"return"                { return XL_KW_RETURN; }

	/* 函数关键字 */
"func"                  { return XL_KW_FUNC; }
"arg"                   { return XL_KW_ARG; }
"as"                    { return XL_KW_AS; }
"const"                 { return XL_KW_CONST; }

	/* 逻辑运算符关键字 */
"and"                   { return XL_KW_AND; }
"or"                    { return XL_KW_OR; }
"not"                   { return XL_KW_NOT; }

	/* 类型名硬关键字（12 个） */
"bool"                  { return XL_KW_BOOL; }
"int"                   { return XL_KW_INT; }
"float"                 { return XL_KW_FLOAT; }
"string"                { return XL_KW_STRING; }
"time"                  { return XL_KW_TIME; }
"point"                 { return XL_KW_POINT; }
"array"                 { return XL_KW_ARRAY; }
"list"                  { return XL_KW_LIST; }
"set"                   { return XL_KW_SET; }
"dict"                  { return XL_KW_DICT; }
"type"                  { return XL_KW_TYPE; }
"range"                 { return XL_KW_RANGE; }

	/* === 标识符（支持中文） === */
	/* 软关键字（i8, u8 等）会被识别为标识符 */
	/* 由 Parser 查表判断是否为软类型名 */
({LETTER}|{UTF8_ANY})({IDENT_CHAR}|{UTF8_ANY})*   { return XL_TOK_IDENT; }

	/* === 未识别字符 === */
.                       { return XL_TOK_ERROR; }

%%
