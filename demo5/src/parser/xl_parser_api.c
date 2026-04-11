#include "xl_parser_api.h"
#include "xl_parser.h"

typedef struct XL_ParseContext {
	XL_AstNode* pRoot;
	XL_DiagContext* pDiag;
	const char* sSourceFile;
	int bHadError;
	int iErrorCount;
} XL_ParseContext;

void* xlParseAlloc(void* (*mallocProc)(size_t));
void xlParse(void* pParser, int iTokenId, XL_Token token, XL_ParseContext* pCtx);
void xlParseFree(void* pParser, void (*freeProc)(void*));
void xlParseTrace(FILE* pTrace, char* sPrefix);

static int __xlParserMapToken(XL_TokenKind eKind)
{
	switch ( eKind ) {
		case XL_TK_NEWLINE:
		case XL_TK_SEMICOLON:
			return XLPG_STMT_END;
		case XL_TK_IDENT:
			return XLPG_TOK_IDENT;
		case XL_TK_INT:
			return XLPG_INT;
		case XL_TK_INT_HEX:
			return XLPG_INT_HEX;
		case XL_TK_INT_OCT:
			return XLPG_INT_OCT;
		case XL_TK_INT_BIN:
			return XLPG_INT_BIN;
		case XL_TK_FLOAT:
			return XLPG_FLOAT;
		case XL_TK_STRING:
			return XLPG_STRING;
		case XL_TK_STRING_RAW:
			return XLPG_STRING_RAW;
		case XL_TK_TEMPLATE:
			return XLPG_TEMPLATE;
		case XL_TK_TIME:
			return XLPG_TIME;
		case XL_KW_NULL:
			return XLPG_KW_NULL;
		case XL_KW_TRUE:
			return XLPG_KW_TRUE;
		case XL_KW_FALSE:
			return XLPG_KW_FALSE;
		case XL_KW_FUNC:
			return XLPG_KW_FUNC;
		case XL_KW_CONST:
			return XLPG_KW_CONST;
		case XL_KW_VAR:
			return XLPG_KW_VAR;
		case XL_KW_AS:
			return XLPG_KW_AS;
		case XL_KW_IF:
			return XLPG_KW_IF;
		case XL_KW_THEN:
			return XLPG_KW_THEN;
		case XL_KW_ELSE:
			return XLPG_KW_ELSE;
		case XL_KW_ELSEIF:
			return XLPG_KW_ELSEIF;
		case XL_KW_END:
			return XLPG_KW_END;
		case XL_KW_FOR:
			return XLPG_KW_FOR;
		case XL_KW_IN:
			return XLPG_KW_IN;
		case XL_KW_WHILE:
			return XLPG_KW_WHILE;
		case XL_KW_DO:
			return XLPG_KW_DO;
		case XL_KW_BREAK:
			return XLPG_KW_BREAK;
		case XL_KW_CONTINUE:
			return XLPG_KW_CONTINUE;
		case XL_KW_RETURN:
			return XLPG_KW_RETURN;
		case XL_KW_ASYNC:
			return XLPG_KW_ASYNC;
		case XL_KW_AWAIT:
			return XLPG_KW_AWAIT;
		case XL_KW_YIELD:
			return XLPG_KW_YIELD;
		case XL_KW_TRY:
			return XLPG_KW_TRY;
		case XL_KW_CATCH:
			return XLPG_KW_CATCH;
		case XL_KW_FINALLY:
			return XLPG_KW_FINALLY;
		case XL_KW_THROW:
			return XLPG_KW_THROW;
		case XL_KW_ASSERT:
			return XLPG_KW_ASSERT;
		case XL_KW_IMPORT:
			return XLPG_KW_IMPORT;
		case XL_KW_EXPORT:
			return XLPG_KW_EXPORT;
		case XL_KW_AND:
			return XLPG_KW_AND;
		case XL_KW_OR:
			return XLPG_KW_OR;
		case XL_KW_NOT:
			return XLPG_KW_NOT;
		case XL_OP_GT:
			return XLPG_OP_GT;
		case XL_OP_LT:
			return XLPG_OP_LT;
		case XL_OP_GE:
			return XLPG_OP_GE;
		case XL_OP_LE:
			return XLPG_OP_LE;
		case XL_OP_NE:
			return XLPG_OP_NE;
		case XL_OP_EQ:
			return XLPG_OP_EQ;
		case XL_OP_APPROX:
			return XLPG_OP_APPROX;
		case XL_OP_PLUS:
			return XLPG_OP_PLUS;
		case XL_OP_MINUS:
			return XLPG_OP_MINUS;
		case XL_OP_STAR:
			return XLPG_OP_STAR;
		case XL_OP_SLASH:
			return XLPG_OP_SLASH;
		case XL_OP_PERCENT:
			return XLPG_OP_PERCENT;
		case XL_OP_POWER:
			return XLPG_OP_POWER;
		case XL_OP_AMP:
			return XLPG_OP_AMP;
		case XL_OP_PIPE:
			return XLPG_OP_PIPE;
		case XL_OP_BANG:
			return XLPG_OP_BANG;
		case XL_OP_CARET:
			return XLPG_OP_CARET;
		case XL_OP_SHR:
			return XLPG_OP_SHR;
		case XL_OP_SHL:
			return XLPG_OP_SHL;
		case XL_OP_ASSIGN:
			return XLPG_OP_ASSIGN;
		case XL_OP_PLUS_ASSIGN:
			return XLPG_OP_PLUS_ASSIGN;
		case XL_OP_MINUS_ASSIGN:
			return XLPG_OP_MINUS_ASSIGN;
		case XL_OP_STAR_ASSIGN:
			return XLPG_OP_STAR_ASSIGN;
		case XL_OP_SLASH_ASSIGN:
			return XLPG_OP_SLASH_ASSIGN;
		case XL_OP_PERCENT_ASSIGN:
			return XLPG_OP_PERCENT_ASSIGN;
		case XL_OP_POWER_ASSIGN:
			return XLPG_OP_POWER_ASSIGN;
		case XL_OP_NULL_COALESCE:
			return XLPG_OP_NULL_COALESCE;
		case XL_OP_NULL_ASSIGN:
			return XLPG_OP_NULL_ASSIGN;
		case XL_OP_QUESTION:
			return XLPG_OP_QUESTION;
		case XL_OP_COLON:
			return XLPG_OP_COLON;
		case XL_OP_DOT:
			return XLPG_OP_DOT;
		case XL_OP_SAFE_DOT:
			return XLPG_OP_SAFE_DOT;
		case XL_OP_SAFE_LBRACKET:
			return XLPG_OP_SAFE_LBRACKET;
		case XL_TK_LPAREN:
			return XLPG_TK_LPAREN;
		case XL_TK_RPAREN:
			return XLPG_TK_RPAREN;
		case XL_TK_LBRACKET:
			return XLPG_TK_LBRACKET;
		case XL_TK_RBRACKET:
			return XLPG_TK_RBRACKET;
		case XL_TK_LBRACE:
			return XLPG_TK_LBRACE;
		case XL_TK_RBRACE:
			return XLPG_TK_RBRACE;
		case XL_TK_COMMA:
			return XLPG_TK_COMMA;
		default:
			return -1;
	}
}

void xlParseResultInit(XL_ParseResult* pResult)
{
	if ( pResult != NULL ) {
		memset(pResult, 0, sizeof(*pResult));
	}
}

void xlParseResultFree(XL_ParseResult* pResult)
{
	if ( pResult != NULL ) {
		xlAstFree(pResult->pRoot);
		pResult->pRoot = NULL;
		pResult->bHadError = 0;
		pResult->iErrorCount = 0;
	}
}

int xlParserParseLexer(XL_Lexer* pLexer, XL_DiagContext* pDiag, int bTraceParser, XL_ParseResult* pResult)
{
	void* pEngine;
	XL_TokenList list;
	XL_ParseContext ctx;
	size_t i;
	int iRc;

	if ( (pLexer == NULL) || (pResult == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	xlParseResultInit(pResult);
	memset(&ctx, 0, sizeof(ctx));
	ctx.pDiag = pDiag;
	ctx.sSourceFile = xlLexerFileName(pLexer);

	xlTokenListInit(&list);
	iRc = xlLexerTokenizeAll(pLexer, &list);
	if ( (iRc != XL_RC_OK) && !xlLexerHadError(pLexer) ) {
		xlTokenListFree(&list);
		return iRc;
	}

	pEngine = xlParseAlloc(malloc);
	if ( pEngine == NULL ) {
		xlTokenListFree(&list);
		return XL_RC_IO_ERROR;
	}

	if ( bTraceParser ) {
		xlParseTrace(stderr, "[lemon] ");
	}

	for ( i = 0; i < list.iCount; ++i ) {
		XL_Token token;
		int iParseType;

		token = list.pItems[i];
		if ( token.eKind == XL_TK_ERROR ) {
			ctx.bHadError = 1;
			ctx.iErrorCount += 1;
			if ( pDiag != NULL ) {
				xlDiagEmitSource(
					pDiag,
					XL_DIAG_LEVEL_ERROR,
					"lexer",
					__FILE__,
					__LINE__,
					xlLexerFileName(pLexer),
					token.iStartLine,
					token.iStartColumn,
					"lexer error near token \"%s\": %s",
					token.sText,
					token.sMessage
				);
			}
			continue;
		}
		if ( token.eKind == XL_TK_EOF ) {
			xlParse(pEngine, 0, token, &ctx);
			break;
		}

		iParseType = __xlParserMapToken(token.eKind);
		if ( iParseType < 0 ) {
			ctx.bHadError = 1;
			ctx.iErrorCount += 1;
			if ( pDiag != NULL ) {
				xlDiagEmitSource(
					pDiag,
					XL_DIAG_LEVEL_ERROR,
					"parser",
					__FILE__,
					__LINE__,
					xlLexerFileName(pLexer),
					token.iStartLine,
					token.iStartColumn,
					"unsupported token for parser: %s",
					xlTokenKindName(token.eKind)
				);
			}
			continue;
		}

		xlParse(pEngine, iParseType, token, &ctx);
	}

	xlParseFree(pEngine, free);
	xlTokenListFree(&list);

	pResult->pRoot = ctx.pRoot;
	pResult->bHadError = ctx.bHadError;
	pResult->iErrorCount = ctx.iErrorCount;
	return ctx.bHadError ? XL_RC_FAIL : XL_RC_OK;
}
