#include "../../lib/xrt/xrt.h"
#include "../parse/xl_tokens.h"
#include "../lexer/xl_lexer.h"
#include "../lexer/xl_lexer_api.h"
#include "xl_ast.h"
#include "xl_parser_api.h"

/* Lemon 生成的解析器实现 */
#include "xl_parser.c"

static XL_Token xlParserMakeSyntheticToken( XL_TokenType type, int iLine, int iCol, const char* pszText )
{
	XL_Token token;
	memset(&token, 0, sizeof(token));
	token.type = type;
	token.text = (char*)(pszText ? pszText : "");
	token.length = pszText ? strlen(pszText) : 0;
	token.line = iLine;
	token.column = iCol;
	return token;
}

static int xlParserMapToken( XL_TokenType type )
{
	switch ( type ) {
		case XL_TOK_EOF:               return 0;
		case XL_TOK_SEMICOLON:         return XLTP_TOK_SEMICOLON;
		case XL_OP_ASSIGN:             return XLTP_OP_ASSIGN;
		case XL_OP_PLUS_ASSIGN:        return XLTP_OP_PLUS_ASSIGN;
		case XL_OP_MINUS_ASSIGN:       return XLTP_OP_MINUS_ASSIGN;
		case XL_OP_STAR_ASSIGN:        return XLTP_OP_STAR_ASSIGN;
		case XL_OP_SLASH_ASSIGN:       return XLTP_OP_SLASH_ASSIGN;
		case XL_OP_PERCENT_ASSIGN:     return XLTP_OP_PERCENT_ASSIGN;
		case XL_OP_POWER_ASSIGN:       return XLTP_OP_POWER_ASSIGN;
		case XL_OP_NULL_ASSIGN:        return XLTP_OP_NULL_ASSIGN;
		case XL_OP_QUESTION:           return XLTP_OP_QUESTION;
		case XL_OP_COLON:              return XLTP_OP_COLON;
		case XL_OP_RANGE:              return XLTP_OP_RANGE;
		case XL_OP_NULL_COALESCE:      return XLTP_OP_NULL_COALESCE;
		case XL_KW_OR:                 return XLTP_KW_OR;
		case XL_KW_AND:                return XLTP_KW_AND;
		case XL_OP_PIPE:               return XLTP_OP_PIPE;
		case XL_OP_CARET:              return XLTP_OP_CARET;
		case XL_OP_AMP:                return XLTP_OP_AMP;
		case XL_OP_EQ:                 return XLTP_OP_EQ;
		case XL_OP_NE:                 return XLTP_OP_NE;
		case XL_OP_APPROX:             return XLTP_OP_APPROX;
		case XL_OP_LT:                 return XLTP_OP_LT;
		case XL_OP_LE:                 return XLTP_OP_LE;
		case XL_OP_GT:                 return XLTP_OP_GT;
		case XL_OP_GE:                 return XLTP_OP_GE;
		case XL_KW_IN:                 return XLTP_KW_IN;
		case XL_OP_SHL:                return XLTP_OP_SHL;
		case XL_OP_SHR:                return XLTP_OP_SHR;
		case XL_OP_PLUS:               return XLTP_OP_PLUS;
		case XL_OP_MINUS:              return XLTP_OP_MINUS;
		case XL_OP_STAR:               return XLTP_OP_STAR;
		case XL_OP_SLASH:              return XLTP_OP_SLASH;
		case XL_OP_PERCENT:            return XLTP_OP_PERCENT;
		case XL_OP_POWER:              return XLTP_OP_POWER;
		case XL_KW_NOT:                return XLTP_KW_NOT;
		case XL_OP_BANG:               return XLTP_OP_BANG;
		case XL_OP_DOT:                return XLTP_OP_DOT;
		case XL_OP_SAFE_DOT:           return XLTP_OP_SAFE_DOT;
		case XL_TOK_LBRACKET:          return XLTP_TOK_LBRACKET;
		case XL_OP_SAFE_BRACKET:       return XLTP_OP_SAFE_BRACKET;
		case XL_TOK_LPAREN:            return XLTP_TOK_LPAREN;
		case XL_OP_INC:                return XLTP_OP_INC;
		case XL_OP_DEC:                return XLTP_OP_DEC;
		case XL_KW_VAR:                return XLTP_KW_VAR;
		case XL_TOK_IDENT:             return XLTP_TOK_IDENT;
		case XL_KW_AUTO:               return XLTP_KW_AUTO;
		case XL_KW_CONST:              return XLTP_KW_CONST;
		case XL_KW_IF:                 return XLTP_KW_IF;
		case XL_KW_END:                return XLTP_KW_END;
		case XL_KW_THEN:               return XLTP_KW_THEN;
		case XL_KW_ELSEIF:             return XLTP_KW_ELSEIF;
		case XL_KW_ELSE:               return XLTP_KW_ELSE;
		case XL_KW_WHILE:              return XLTP_KW_WHILE;
		case XL_KW_DO:                 return XLTP_KW_DO;
		case XL_KW_FOR:                return XLTP_KW_FOR;
		case XL_TOK_COMMA:             return XLTP_TOK_COMMA;
		case XL_KW_TO:                 return XLTP_KW_TO;
		case XL_KW_STEP:               return XLTP_KW_STEP;
		case XL_KW_RETURN:             return XLTP_KW_RETURN;
		case XL_KW_BREAK:              return XLTP_KW_BREAK;
		case XL_TOK_INT:               return XLTP_TOK_INT;
		case XL_KW_CONTINUE:           return XLTP_KW_CONTINUE;
		case XL_KW_FUNC:               return XLTP_KW_FUNC;
		case XL_TOK_RPAREN:            return XLTP_TOK_RPAREN;
		case XL_OP_ELLIPSIS:           return XLTP_OP_ELLIPSIS;
		case XL_OP_ELLIPSIS_DICT:      return XLTP_OP_ELLIPSIS_DICT;
		case XL_KW_AS:                 return XLTP_KW_AS;
		case XL_KW_BOOL:               return XLTP_KW_BOOL;
		case XL_KW_INT:                return XLTP_KW_INT;
		case XL_KW_FLOAT:              return XLTP_KW_FLOAT;
		case XL_KW_STRING:             return XLTP_KW_STRING;
		case XL_KW_TIME:               return XLTP_KW_TIME;
		case XL_KW_POINT:              return XLTP_KW_POINT;
		case XL_KW_ARRAY:              return XLTP_KW_ARRAY;
		case XL_KW_LIST:               return XLTP_KW_LIST;
		case XL_KW_SET:                return XLTP_KW_SET;
		case XL_KW_DICT:               return XLTP_KW_DICT;
		case XL_KW_TYPE:               return XLTP_KW_TYPE;
		case XL_KW_RANGE:              return XLTP_KW_RANGE;
		case XL_TOK_RBRACKET:          return XLTP_TOK_RBRACKET;
		case XL_TOK_INT_HEX:           return XLTP_TOK_INT_HEX;
		case XL_TOK_INT_OCT:           return XLTP_TOK_INT_OCT;
		case XL_TOK_INT_BIN:           return XLTP_TOK_INT_BIN;
		case XL_TOK_FLOAT:             return XLTP_TOK_FLOAT;
		case XL_TOK_STRING:            return XLTP_TOK_STRING;
		case XL_TOK_STRING_RAW:        return XLTP_TOK_STRING_RAW;
		case XL_TOK_STRING_MULTI:      return XLTP_TOK_STRING_MULTI;
		case XL_TOK_STRING_MULTI_RAW:  return XLTP_TOK_STRING_MULTI_RAW;
		case XL_TOK_TIME:              return XLTP_TOK_TIME;
		case XL_KW_TRUE:               return XLTP_KW_TRUE;
		case XL_KW_FALSE:              return XLTP_KW_FALSE;
		case XL_KW_NULL:               return XLTP_KW_NULL;
		case XL_KW_ARG:                return XLTP_KW_ARG;
		case XL_KW_FUNCTION:           return XLTP_KW_FUNCTION;
		case XL_TOK_LBRACE:            return XLTP_TOK_LBRACE;
		case XL_TOK_RBRACE:            return XLTP_TOK_RBRACE;
		default:                       return -1;
	}
}

static int xlParserReadRawToken( XL_Parser* pParser, XL_Token* pToken )
{
	int iType;

	if ( pParser->bHasBufferedToken ) {
		*pToken = pParser->bufferedToken;
		pParser->bHasBufferedToken = 0;
		return pToken->type;
	}

	iType = xlLexerNextToken(pParser->pLexer, pToken);
	if ( iType == XL_TOK_NEWLINE ) {
		pToken->type = XL_TOK_SEMICOLON;
		pToken->text = ";";
		pToken->length = 1;
		return XL_TOK_SEMICOLON;
	}
	return iType;
}

static void xlParserBufferToken( XL_Parser* pParser, XL_Token* pToken )
{
	pParser->bufferedToken = *pToken;
	pParser->bHasBufferedToken = 1;
}

static int xlParserNextToken( XL_Parser* pParser, XL_Token* pToken )
{
	int iType;

	if ( pParser->iPendingSyntheticEnd > 0 ) {
		*pToken = xlParserMakeSyntheticToken(XL_KW_END, pParser->iLastLine, pParser->iLastColumn, "end");
		pParser->iPendingSyntheticEnd--;
		return pToken->type;
	}

	iType = xlParserReadRawToken(pParser, pToken);
	if ( ((iType == XL_TOK_SEMICOLON) || (iType == XL_TOK_EOF)) && (pParser->iSingleLineIfDepth > 0) ) {
		xlParserBufferToken(pParser, pToken);
		pParser->iPendingSyntheticEnd = pParser->iSingleLineIfDepth - 1;
		pParser->iSingleLineIfDepth = 0;
		*pToken = xlParserMakeSyntheticToken(XL_KW_END, pParser->iLastLine, pParser->iLastColumn, "end");
		return pToken->type;
	}

	if ( iType == XL_KW_THEN ) {
		XL_Token lookahead;
		int iLookaheadType = xlParserReadRawToken(pParser, &lookahead);
		xlParserBufferToken(pParser, &lookahead);
		if ( (iLookaheadType != XL_TOK_SEMICOLON) && (iLookaheadType != XL_TOK_EOF) && (lookahead.line == pToken->line) ) {
			pParser->iSingleLineIfDepth++;
		}
	}

	pParser->iLastLine = pToken->line;
	pParser->iLastColumn = pToken->column;
	return iType;
}

XL_Parser* xlParserCreate( XL_Lexer* pLexer )
{
	XL_Parser* pParser = (XL_Parser*)calloc(1, sizeof(XL_Parser));
	if ( !pParser ) {
		return NULL;
	}
	pParser->pLexer = pLexer;
	return pParser;
}

void xlParserDestroy( XL_Parser* pParser )
{
	if ( pParser ) {
		free(pParser);
	}
}

xvalue xlParserParse( XL_Parser* pParser, const char* pszFilename )
{
	void* pEngine;
	XL_Token token;
	int iType = XL_TOK_EOF;

	if ( !pParser || !pParser->pLexer ) {
		return 0;
	}

	memset(&pParser->ctx, 0, sizeof(pParser->ctx));
	pParser->ctx.pszFilename = pszFilename ? pszFilename : "<input>";
	pParser->bHasBufferedToken = 0;
	pParser->iPendingSyntheticEnd = 0;
	pParser->iSingleLineIfDepth = 0;
	pParser->iLastLine = 1;
	pParser->iLastColumn = 1;

	pEngine = xlParseAlloc(malloc);
	if ( !pEngine ) {
		fprintf(stderr, "错误: 无法创建 Lemon Parser\n");
		pParser->ctx.bHadError = 1;
		pParser->ctx.iErrorCount = 1;
		pParser->bHadError = 1;
		pParser->iErrorCount = 1;
		return 0;
	}

	while ( 1 ) {
		int iParseType;
		memset(&token, 0, sizeof(token));
		iType = xlParserNextToken(pParser, &token);

		if ( iType == XL_TOK_ERROR ) {
			pParser->ctx.bHadError = 1;
			pParser->ctx.iErrorCount++;
			fprintf(stderr, "[%d:%d] 词法错误: 无法识别的符号 \"%.*s\"\n",
				token.line, token.column, (int)token.length, token.text ? token.text : "");
			break;
		}

		iParseType = xlParserMapToken((XL_TokenType)iType);
		if ( iParseType < 0 ) {
			pParser->ctx.bHadError = 1;
			pParser->ctx.iErrorCount++;
			fprintf(stderr, "[%d:%d] 语法前处理错误: 不支持的 Token %s\n",
				token.line, token.column, xlTokenTypeName((XL_TokenType)iType));
			break;
		}

		xlParse(pEngine, iParseType, token, &pParser->ctx);
		if ( iType == XL_TOK_EOF ) {
			break;
		}
	}

	if ( iType != XL_TOK_EOF ) {
		memset(&token, 0, sizeof(token));
		token.type = XL_TOK_EOF;
		xlParse(pEngine, 0, token, &pParser->ctx);
	}

	xlParseFree(pEngine, free);

	pParser->bHadError = pParser->ctx.bHadError;
	pParser->iErrorCount = pParser->ctx.iErrorCount;
	return pParser->ctx.xAst;
}
