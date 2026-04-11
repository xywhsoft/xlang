%top{
#include "src/lexer/xl_token.h"
}

%{
#include <stdlib.h>
#include <string.h>

#define YY_EXTRA_TYPE XL_LexerExtra*

typedef void* yyscan_t;
XL_LexerExtra* xlget_extra(yyscan_t yyscanner);
char* xlget_text(yyscan_t yyscanner);
int xlget_leng(yyscan_t yyscanner);
int xlget_lineno(yyscan_t yyscanner);

static int __xlUtf8ColCount(const char* sText, int iLen)
{
	int i;
	int iCols;

	iCols = 0;
	for ( i = 0; i < iLen; ++i ) {
		unsigned char ch = (unsigned char)sText[i];
		if ( (ch == '\r') || (ch == '\n') ) {
			continue;
		}
		if ( (ch & 0xC0) != 0x80 ) {
			iCols += 1;
		}
	}
	return iCols;
}

static void __xlSetTokenCommon(yyscan_t yyscanner, XL_TokenKind eKind)
{
	XL_LexerExtra* pExtra;
	XL_Token* pToken;

	pExtra = xlget_extra(yyscanner);
	pToken = &pExtra->currentToken;
	memset(pToken, 0, sizeof(*pToken));
	pToken->eKind = eKind;
	pToken->sText = xlget_text(yyscanner);
	pToken->iTextLen = (size_t)xlget_leng(yyscanner);
	pToken->iStartOffset = pExtra->iTokenStartOffset;
	pToken->iEndOffset = pExtra->iOffset;
	pToken->iStartLine = pExtra->iTokenStartLine;
	pToken->iStartColumn = pExtra->iTokenStartColumn;
	pToken->iEndLine = pExtra->iLine;
	pToken->iEndColumn = pExtra->iColumn;
}

static void __xlSetNewlineToken(yyscan_t yyscanner)
{
	XL_LexerExtra* pExtra;
	XL_Token* pToken;

	pExtra = xlget_extra(yyscanner);
	pToken = &pExtra->currentToken;
	memset(pToken, 0, sizeof(*pToken));
	pToken->eKind = XL_TK_NEWLINE;
	pToken->sText = xlget_text(yyscanner);
	pToken->iTextLen = (size_t)xlget_leng(yyscanner);
	pToken->iStartOffset = pExtra->iTokenStartOffset;
	pToken->iEndOffset = pExtra->iOffset;
	pToken->iStartLine = pExtra->iTokenStartLine;
	pToken->iStartColumn = pExtra->iTokenStartColumn;
	pToken->iEndLine = pExtra->iLine + 1;
	pToken->iEndColumn = 1;
}

static void __xlSetEofToken(yyscan_t yyscanner)
{
	XL_LexerExtra* pExtra;
	XL_Token* pToken;

	pExtra = xlget_extra(yyscanner);
	pToken = &pExtra->currentToken;
	memset(pToken, 0, sizeof(*pToken));
	pToken->eKind = XL_TK_EOF;
	pToken->sText = "";
	pToken->iTextLen = 0;
	pToken->iStartOffset = pExtra->iOffset;
	pToken->iEndOffset = pExtra->iOffset;
	pToken->iStartLine = pExtra->iLine;
	pToken->iStartColumn = pExtra->iColumn;
	pToken->iEndLine = pExtra->iLine;
	pToken->iEndColumn = pExtra->iColumn;
}

static void __xlSetErrorToken(yyscan_t yyscanner, const char* sMessage)
{
	XL_LexerExtra* pExtra;
	XL_Token* pToken;

	pExtra = xlget_extra(yyscanner);
	pExtra->bHadError = 1;
	snprintf(pExtra->sErrorText, sizeof(pExtra->sErrorText), "%s", sMessage);
	__xlSetTokenCommon(yyscanner, XL_TK_ERROR);
	pToken = &pExtra->currentToken;
	snprintf(pToken->sMessage, sizeof(pToken->sMessage), "%s", sMessage);
}

static int64_t __xlParseIntDec(const char* sText)
{
	int64_t iValue;

	iValue = 0;
	while ( *sText != '\0' ) {
		if ( *sText != '_' ) {
			iValue = (iValue * 10) + (int64_t)(*sText - '0');
		}
		++sText;
	}
	return iValue;
}

static int64_t __xlParseIntBase(const char* sText, int iBase, int iSkip)
{
	int64_t iValue;
	char ch;
	int iDigit;

	iValue = 0;
	sText += iSkip;
	while ( *sText != '\0' ) {
		ch = *sText;
		if ( ch == '_' ) {
			++sText;
			continue;
		}
		if ( (ch >= '0') && (ch <= '9') ) {
			iDigit = ch - '0';
		} else if ( (ch >= 'a') && (ch <= 'f') ) {
			iDigit = ch - 'a' + 10;
		} else if ( (ch >= 'A') && (ch <= 'F') ) {
			iDigit = ch - 'A' + 10;
		} else {
			break;
		}
		iValue = (iValue * iBase) + iDigit;
		++sText;
	}
	return iValue;
}

static XL_TokenKind __xlLookupKeyword(const char* sText, int iLen)
{
	typedef struct XL_KeywordEntry {
		const char* sName;
		XL_TokenKind eKind;
	} XL_KeywordEntry;

	static const XL_KeywordEntry arrKeywords[] = {
		{"null", XL_KW_NULL}, {"true", XL_KW_TRUE}, {"false", XL_KW_FALSE},
		{"func", XL_KW_FUNC}, {"const", XL_KW_CONST}, {"var", XL_KW_VAR}, {"as", XL_KW_AS},
		{"if", XL_KW_IF}, {"then", XL_KW_THEN}, {"else", XL_KW_ELSE}, {"elseif", XL_KW_ELSEIF}, {"end", XL_KW_END},
		{"for", XL_KW_FOR}, {"in", XL_KW_IN}, {"while", XL_KW_WHILE}, {"do", XL_KW_DO},
		{"break", XL_KW_BREAK}, {"continue", XL_KW_CONTINUE}, {"return", XL_KW_RETURN},
		{"async", XL_KW_ASYNC}, {"await", XL_KW_AWAIT}, {"yield", XL_KW_YIELD},
		{"try", XL_KW_TRY}, {"catch", XL_KW_CATCH}, {"finally", XL_KW_FINALLY},
		{"throw", XL_KW_THROW}, {"assert", XL_KW_ASSERT},
		{"import", XL_KW_IMPORT}, {"export", XL_KW_EXPORT}, {"include", XL_KW_INCLUDE},
		{"and", XL_KW_AND}, {"or", XL_KW_OR}, {"not", XL_KW_NOT}
	};
	size_t i;

	for ( i = 0; i < (sizeof(arrKeywords) / sizeof(arrKeywords[0])); ++i ) {
		if ( ((int)strlen(arrKeywords[i].sName) == iLen) &&
		     (memcmp(arrKeywords[i].sName, sText, (size_t)iLen) == 0) ) {
			return arrKeywords[i].eKind;
		}
	}
	return XL_TK_IDENT;
}

#define YY_USER_ACTION \
	yyextra->iTokenStartOffset = yyextra->iOffset; \
	yyextra->iTokenStartLine = yyextra->iLine; \
	yyextra->iTokenStartColumn = yyextra->iColumn; \
	yyextra->iOffset += (size_t)yyleng; \
	yyextra->iColumn += __xlUtf8ColCount(yytext, yyleng);
%}

%option reentrant
%option noyywrap
%option yylineno
%option prefix="xl"
%option extra-type="XL_LexerExtra*"
%option warn
%option noinput
%option nounput

%x COMMENT

DIGIT		[0-9]
HEXDIGIT	[0-9a-fA-F]
OCTDIGIT	[0-7]
BINDIGIT	[01]
ASCII_START	[a-zA-Z_]
ASCII_CONT	[a-zA-Z0-9_]
UTF8_CONT	[\x80-\xBF]
UTF8_2		[\xC2-\xDF]{UTF8_CONT}
UTF8_3		[\xE0-\xEF]{UTF8_CONT}{2}
UTF8_4		[\xF0-\xF4]{UTF8_CONT}{3}
UTF8_ANY	({UTF8_2}|{UTF8_3}|{UTF8_4})
IDENT_START	({ASCII_START}|{UTF8_ANY})
IDENT_CONT	({ASCII_CONT}|{UTF8_ANY})
IDENT		{IDENT_START}{IDENT_CONT}*

%%

[ \t\f\v]+				{ }
\r\n					{ __xlSetNewlineToken(yyscanner); yyextra->iLine += 1; yyextra->iColumn = 1; return XL_TK_NEWLINE; }
\n					{ __xlSetNewlineToken(yyscanner); yyextra->iLine += 1; yyextra->iColumn = 1; return XL_TK_NEWLINE; }
\r					{ __xlSetNewlineToken(yyscanner); yyextra->iLine += 1; yyextra->iColumn = 1; return XL_TK_NEWLINE; }

"//"[^\r\n]*				{ }
"/*"					{ BEGIN(COMMENT); }
<COMMENT>"*/"				{ BEGIN(INITIAL); }
<COMMENT>\r\n				{ yyextra->iLine += 1; yyextra->iColumn = 1; }
<COMMENT>\n				{ yyextra->iLine += 1; yyextra->iColumn = 1; }
<COMMENT>\r				{ yyextra->iLine += 1; yyextra->iColumn = 1; }
<COMMENT>.				{ }
<COMMENT><<EOF>>			{ __xlSetErrorToken(yyscanner, "unterminated block comment"); BEGIN(INITIAL); return XL_TK_ERROR; }

"**="					{ __xlSetTokenCommon(yyscanner, XL_OP_POWER_ASSIGN); return XL_OP_POWER_ASSIGN; }
"**"					{ __xlSetTokenCommon(yyscanner, XL_OP_POWER); return XL_OP_POWER; }
"..."					{ __xlSetTokenCommon(yyscanner, XL_OP_ELLIPSIS); return XL_OP_ELLIPSIS; }
".."					{ __xlSetTokenCommon(yyscanner, XL_OP_RANGE); return XL_OP_RANGE; }
"?["					{ __xlSetTokenCommon(yyscanner, XL_OP_SAFE_LBRACKET); return XL_OP_SAFE_LBRACKET; }
"?."					{ __xlSetTokenCommon(yyscanner, XL_OP_SAFE_DOT); return XL_OP_SAFE_DOT; }
"??"					{ __xlSetTokenCommon(yyscanner, XL_OP_NULL_COALESCE); return XL_OP_NULL_COALESCE; }
"?="					{ __xlSetTokenCommon(yyscanner, XL_OP_NULL_ASSIGN); return XL_OP_NULL_ASSIGN; }
"+="					{ __xlSetTokenCommon(yyscanner, XL_OP_PLUS_ASSIGN); return XL_OP_PLUS_ASSIGN; }
"-="					{ __xlSetTokenCommon(yyscanner, XL_OP_MINUS_ASSIGN); return XL_OP_MINUS_ASSIGN; }
"*="					{ __xlSetTokenCommon(yyscanner, XL_OP_STAR_ASSIGN); return XL_OP_STAR_ASSIGN; }
"/="					{ __xlSetTokenCommon(yyscanner, XL_OP_SLASH_ASSIGN); return XL_OP_SLASH_ASSIGN; }
"%="					{ __xlSetTokenCommon(yyscanner, XL_OP_PERCENT_ASSIGN); return XL_OP_PERCENT_ASSIGN; }
">="					{ __xlSetTokenCommon(yyscanner, XL_OP_GE); return XL_OP_GE; }
"<="					{ __xlSetTokenCommon(yyscanner, XL_OP_LE); return XL_OP_LE; }
"!="					{ __xlSetTokenCommon(yyscanner, XL_OP_NE); return XL_OP_NE; }
"=="					{ __xlSetTokenCommon(yyscanner, XL_OP_EQ); return XL_OP_EQ; }
"~="					{ __xlSetTokenCommon(yyscanner, XL_OP_APPROX); return XL_OP_APPROX; }
">>"					{ __xlSetTokenCommon(yyscanner, XL_OP_SHR); return XL_OP_SHR; }
"<<"					{ __xlSetTokenCommon(yyscanner, XL_OP_SHL); return XL_OP_SHL; }

">"					{ __xlSetTokenCommon(yyscanner, XL_OP_GT); return XL_OP_GT; }
"<"					{ __xlSetTokenCommon(yyscanner, XL_OP_LT); return XL_OP_LT; }
"+"					{ __xlSetTokenCommon(yyscanner, XL_OP_PLUS); return XL_OP_PLUS; }
"-"					{ __xlSetTokenCommon(yyscanner, XL_OP_MINUS); return XL_OP_MINUS; }
"*"					{ __xlSetTokenCommon(yyscanner, XL_OP_STAR); return XL_OP_STAR; }
"/"					{ __xlSetTokenCommon(yyscanner, XL_OP_SLASH); return XL_OP_SLASH; }
"%"					{ __xlSetTokenCommon(yyscanner, XL_OP_PERCENT); return XL_OP_PERCENT; }
"&"					{ __xlSetTokenCommon(yyscanner, XL_OP_AMP); return XL_OP_AMP; }
"|"					{ __xlSetTokenCommon(yyscanner, XL_OP_PIPE); return XL_OP_PIPE; }
"!"					{ __xlSetTokenCommon(yyscanner, XL_OP_BANG); return XL_OP_BANG; }
"^"					{ __xlSetTokenCommon(yyscanner, XL_OP_CARET); return XL_OP_CARET; }
"="					{ __xlSetTokenCommon(yyscanner, XL_OP_ASSIGN); return XL_OP_ASSIGN; }
"?"					{ __xlSetTokenCommon(yyscanner, XL_OP_QUESTION); return XL_OP_QUESTION; }
":"					{ __xlSetTokenCommon(yyscanner, XL_OP_COLON); return XL_OP_COLON; }
"."					{ __xlSetTokenCommon(yyscanner, XL_OP_DOT); return XL_OP_DOT; }

"("					{ __xlSetTokenCommon(yyscanner, XL_TK_LPAREN); return XL_TK_LPAREN; }
")"					{ __xlSetTokenCommon(yyscanner, XL_TK_RPAREN); return XL_TK_RPAREN; }
"["					{ __xlSetTokenCommon(yyscanner, XL_TK_LBRACKET); return XL_TK_LBRACKET; }
"]"					{ __xlSetTokenCommon(yyscanner, XL_TK_RBRACKET); return XL_TK_RBRACKET; }
"{"					{ __xlSetTokenCommon(yyscanner, XL_TK_LBRACE); return XL_TK_LBRACE; }
"}"					{ __xlSetTokenCommon(yyscanner, XL_TK_RBRACE); return XL_TK_RBRACE; }
","					{ __xlSetTokenCommon(yyscanner, XL_TK_COMMA); return XL_TK_COMMA; }
";"					{ __xlSetTokenCommon(yyscanner, XL_TK_SEMICOLON); return XL_TK_SEMICOLON; }

{DIGIT}({DIGIT}|_)*\.({DIGIT}|_)+([eE][+-]?{DIGIT}+)?	{
						__xlSetTokenCommon(yyscanner, XL_TK_FLOAT);
						yyextra->currentToken.eValueKind = XL_TV_FLOAT;
						yyextra->currentToken.fFloatValue = strtod(yytext, NULL);
						return XL_TK_FLOAT;
					}
{DIGIT}({DIGIT}|_)*[eE][+-]?{DIGIT}+	{
						__xlSetTokenCommon(yyscanner, XL_TK_FLOAT);
						yyextra->currentToken.eValueKind = XL_TV_FLOAT;
						yyextra->currentToken.fFloatValue = strtod(yytext, NULL);
						return XL_TK_FLOAT;
					}
0[xX]({HEXDIGIT}|_)+			{
						__xlSetTokenCommon(yyscanner, XL_TK_INT_HEX);
						yyextra->currentToken.eValueKind = XL_TV_INT;
						yyextra->currentToken.iIntValue = __xlParseIntBase(yytext, 16, 2);
						return XL_TK_INT_HEX;
					}
"&"[hH]({HEXDIGIT}|_)+			{
						__xlSetTokenCommon(yyscanner, XL_TK_INT_HEX);
						yyextra->currentToken.eValueKind = XL_TV_INT;
						yyextra->currentToken.iIntValue = __xlParseIntBase(yytext, 16, 2);
						return XL_TK_INT_HEX;
					}
"&"[oO]({OCTDIGIT}|_)+			{
						__xlSetTokenCommon(yyscanner, XL_TK_INT_OCT);
						yyextra->currentToken.eValueKind = XL_TV_INT;
						yyextra->currentToken.iIntValue = __xlParseIntBase(yytext, 8, 2);
						return XL_TK_INT_OCT;
					}
0[bB]({BINDIGIT}|_)+			{
						__xlSetTokenCommon(yyscanner, XL_TK_INT_BIN);
						yyextra->currentToken.eValueKind = XL_TV_INT;
						yyextra->currentToken.iIntValue = __xlParseIntBase(yytext, 2, 2);
						return XL_TK_INT_BIN;
					}
"&"[bB]({BINDIGIT}|_)+			{
						__xlSetTokenCommon(yyscanner, XL_TK_INT_BIN);
						yyextra->currentToken.eValueKind = XL_TV_INT;
						yyextra->currentToken.iIntValue = __xlParseIntBase(yytext, 2, 2);
						return XL_TK_INT_BIN;
					}
{DIGIT}({DIGIT}|_)*			{
						__xlSetTokenCommon(yyscanner, XL_TK_INT);
						yyextra->currentToken.eValueKind = XL_TV_INT;
						yyextra->currentToken.iIntValue = __xlParseIntDec(yytext);
						return XL_TK_INT;
					}

#[^#\r\n]+#				{ __xlSetTokenCommon(yyscanner, XL_TK_TIME); return XL_TK_TIME; }

\"([^\"\\\r\n]|\\.)*\"			{ __xlSetTokenCommon(yyscanner, XL_TK_STRING); return XL_TK_STRING; }
\"([^\"\\\r\n]|\\.)*			{ __xlSetErrorToken(yyscanner, "unterminated string literal"); return XL_TK_ERROR; }
\'[^\'\r\n]*\'				{ __xlSetTokenCommon(yyscanner, XL_TK_STRING_RAW); return XL_TK_STRING_RAW; }
\'[^\'\r\n]*				{ __xlSetErrorToken(yyscanner, "unterminated raw string literal"); return XL_TK_ERROR; }
\`([^\`\\\r\n]|\\.)*\`			{ __xlSetTokenCommon(yyscanner, XL_TK_TEMPLATE); return XL_TK_TEMPLATE; }
\`([^\`\\\r\n]|\\.)*			{ __xlSetErrorToken(yyscanner, "unterminated template string"); return XL_TK_ERROR; }

{IDENT}					{
						XL_TokenKind eKind;
						eKind = __xlLookupKeyword(yytext, yyleng);
						__xlSetTokenCommon(yyscanner, eKind);
						return eKind;
					}

<<EOF>>					{
						__xlSetEofToken(yyscanner);
						return XL_TK_EOF;
					}

.						{
						__xlSetErrorToken(yyscanner, "unexpected character");
						return XL_TK_ERROR;
					}

%%
