/**
 * xlang v4 - 语法分析器模块
 * 
 * 手写递归下降语法分析器
 * 将 Token 流解析为 xvalue 格式的 AST
 * 仅支持 v4 一阶段语法
 */

#ifndef XL_PARSER_H
#define XL_PARSER_H



// ============================================================================
// 类型名检测
// ============================================================================

// v4 中类型名作为 IDENT，需要在 Parser 中判断
static const char* xl_type_names[] = {
	"bool", "int", "float", "string",
	"i8", "u8", "i16", "u16", "i32", "u32", "i64", "u64",
	"f32", "f64", "time", "point",
	"array", "list", "set", "dict",
	"type", "range",
	NULL
};

static inline int xlIsTypeName( const char* pszText, int iLen )
{
	for ( int i = 0; xl_type_names[i]; i++ ) {
		if ( (int)strlen(xl_type_names[i]) == iLen &&
		     memcmp(pszText, xl_type_names[i], iLen) == 0 ) {
			return 1;
		}
	}
	return 0;
}

static inline int xlIsHardTypeToken( XL_TokenType type )
{
	return (type >= XL_KW_BOOL) && (type <= XL_KW_RANGE);
}



// ============================================================================
// Parser 结构
// ============================================================================

typedef struct XL_Parser {
	XL_Lexer *pLexer;       // 词法分析器
	XL_Token token;          // 当前 Token
	XL_Token prevToken;      // 上一个 Token
	XL_Token peekToken;      // 预读 Token（1-token lookahead）
	int bHasPeek;            // peekToken 是否有效
	int bHadError;           // 是否发生错误
	int bPanicMode;          // 恐慌模式（错误恢复）
	int iErrorCount;         // 错误计数
} XL_Parser;



// ============================================================================
// 前向声明
// ============================================================================

static xvalue xlParseExpr( XL_Parser *pParser );
static xvalue xlParseStmt( XL_Parser *pParser );
static xvalue xlParseDecl( XL_Parser *pParser );
static xvalue xlParseBlockStmt( XL_Parser *pParser );



// ============================================================================
// 错误报告
// ============================================================================

static void xlParserError( XL_Parser *pParser, const char* pszFmt, ... )
{
	if ( pParser->bPanicMode ) return;
	pParser->bHadError = 1;
	pParser->bPanicMode = 1;
	pParser->iErrorCount++;

	va_list args;
	va_start(args, pszFmt);
	fprintf(stderr, "[%d:%d] 错误: ", pParser->token.line, pParser->token.column);
	vfprintf(stderr, pszFmt, args);
	fprintf(stderr, "\n");
	va_end(args);
}



// ============================================================================
// Parser 创建与销毁
// ============================================================================

static inline XL_Parser* xlParserCreate( XL_Lexer *pLexer )
{
	XL_Parser *pParser = (XL_Parser*)malloc(sizeof(XL_Parser));
	if ( !pParser ) return NULL;

	pParser->pLexer = pLexer;
	pParser->bHasPeek = 0;
	pParser->bHadError = 0;
	pParser->bPanicMode = 0;
	pParser->iErrorCount = 0;

	memset(&pParser->token, 0, sizeof(XL_Token));
	memset(&pParser->prevToken, 0, sizeof(XL_Token));
	memset(&pParser->peekToken, 0, sizeof(XL_Token));

	return pParser;
}

static inline void xlParserDestroy( XL_Parser *pParser )
{
	if ( pParser ) {
		free(pParser);
	}
}



// ============================================================================
// Token 操作
// ============================================================================

// 读取下一个非换行 Token（内部使用）
static inline void xlParserReadToken( XL_Parser *pParser, XL_Token *pToken )
{
	while ( 1 ) {
		xlLexerNextToken(pParser->pLexer, pToken);
		if ( pToken->type != XL_TOK_NEWLINE ) break;
	}
}

// 前进到下一个 Token
static inline void xlParserAdvance( XL_Parser *pParser )
{
	pParser->prevToken = pParser->token;

	if ( pParser->bHasPeek ) {
		pParser->token = pParser->peekToken;
		pParser->bHasPeek = 0;
	} else {
		xlParserReadToken(pParser, &pParser->token);
	}
}

// 预读下一个 Token（不消耗）
static inline XL_TokenType xlParserPeek( XL_Parser *pParser )
{
	if ( !pParser->bHasPeek ) {
		xlParserReadToken(pParser, &pParser->peekToken);
		pParser->bHasPeek = 1;
	}
	return pParser->peekToken.type;
}

// 检查当前 Token 类型
static inline int xlParserCheck( XL_Parser *pParser, XL_TokenType type )
{
	return pParser->token.type == type;
}

// 匹配并前进
static inline int xlParserMatch( XL_Parser *pParser, XL_TokenType type )
{
	if ( !xlParserCheck(pParser, type) ) return 0;
	xlParserAdvance(pParser);
	return 1;
}

// 期望特定 Token，否则报错
static inline int xlParserExpect( XL_Parser *pParser, XL_TokenType type, const char* pszWhat )
{
	if ( xlParserCheck(pParser, type) ) {
		xlParserAdvance(pParser);
		return 1;
	}
	xlParserError(pParser, "期望 %s，但得到 %s", pszWhat, xlTokenTypeName(pParser->token.type));
	return 0;
}

// 同步（错误恢复）
static inline void xlParserSync( XL_Parser *pParser )
{
	pParser->bPanicMode = 0;

	while ( pParser->token.type != XL_TOK_EOF ) {
		switch ( pParser->token.type ) {
			case XL_KW_FUNC:
			case XL_KW_VAR:
			case XL_KW_AUTO:
			case XL_KW_CONST:
			case XL_KW_FOR:
			case XL_KW_IF:
			case XL_KW_WHILE:
			case XL_KW_DO:
			case XL_KW_RETURN:
			case XL_KW_END:
				return;
			default:
				break;
		}
		xlParserAdvance(pParser);
	}
}

// 检查当前 IDENT 是否为类型名
static inline int xlParserCheckTypeName( XL_Parser *pParser )
{
	if ( xlIsHardTypeToken(pParser->token.type) ) {
		return 1;
	}
	if ( pParser->token.type != XL_TOK_IDENT ) return 0;
	return xlIsTypeName(pParser->token.text, (int)pParser->token.length);
}

static inline int xlParserIsSameLineContinuation( XL_Parser *pParser )
{
	return pParser->token.line == pParser->prevToken.line;
}



// ============================================================================
// 表达式解析 - 基础表达式
// ============================================================================

static xvalue xlParsePrimaryExpr( XL_Parser *pParser )
{
	int iLine = pParser->token.line;
	int iCol = pParser->token.column;

	switch ( pParser->token.type ) {
		/* 整数字面量 */
		case XL_TOK_INT:
		case XL_TOK_INT_HEX:
		case XL_TOK_INT_OCT:
		case XL_TOK_INT_BIN: {
			int64_t iVal = pParser->token.iVal;
			xlParserAdvance(pParser);
			return xlAstCreateIntLit(iVal, iLine, iCol);
		}

		/* 浮点字面量 */
		case XL_TOK_FLOAT: {
			double fVal = pParser->token.fVal;
			xlParserAdvance(pParser);
			return xlAstCreateFloatLit(fVal, iLine, iCol);
		}

		/* 字符串字面量（去掉引号） */
		case XL_TOK_STRING: {
			const char* pszVal = pParser->token.text + 1;
			int iLen = (int)pParser->token.length - 2;
			xlParserAdvance(pParser);
			return xlAstCreateStringLit(pszVal, iLen, iLine, iCol);
		}
		case XL_TOK_STRING_RAW: {
			const char* pszVal = pParser->token.text + 1;
			int iLen = (int)pParser->token.length - 2;
			xlParserAdvance(pParser);
			return xlAstCreateStringLit(pszVal, iLen, iLine, iCol);
		}
		case XL_TOK_STRING_MULTI: {
			const char* pszVal = pParser->token.text + 3;
			int iLen = (int)pParser->token.length - 6;
			xlParserAdvance(pParser);
			return xlAstCreateStringLit(pszVal, iLen, iLine, iCol);
		}
		case XL_TOK_STRING_MULTI_RAW: {
			const char* pszVal = pParser->token.text + 3;
			int iLen = (int)pParser->token.length - 6;
			xlParserAdvance(pParser);
			return xlAstCreateStringLit(pszVal, iLen, iLine, iCol);
		}

		/* 时间字面量（去掉 # 号） */
		case XL_TOK_TIME: {
			const char* pszVal = pParser->token.text + 1;
			int iLen = (int)pParser->token.length - 2;
			xlParserAdvance(pParser);
			return xlAstCreateTimeLit(pszVal, iLen, iLine, iCol);
		}

		/* 布尔字面量 */
		case XL_KW_TRUE: {
			xlParserAdvance(pParser);
			return xlAstCreateBoolLit(1, iLine, iCol);
		}
		case XL_KW_FALSE: {
			xlParserAdvance(pParser);
			return xlAstCreateBoolLit(0, iLine, iCol);
		}

		/* null 字面量 */
		case XL_KW_NULL: {
			xlParserAdvance(pParser);
			return xlAstCreateNullLit(iLine, iCol);
		}

		/* 标识符（可能是类型构造/转换） */
		case XL_TOK_IDENT: {
			const char* pszName = pParser->token.text;
			int iNameLen = (int)pParser->token.length;
			xlParserAdvance(pParser);
			return xlAstCreateIdent(pszName, iNameLen, iLine, iCol);
		}

		/* 硬关键字类型名在表达式中也可作为类型构造标识符 */
		case XL_KW_BOOL:
		case XL_KW_INT:
		case XL_KW_FLOAT:
		case XL_KW_STRING:
		case XL_KW_TIME:
		case XL_KW_POINT:
		case XL_KW_ARRAY:
		case XL_KW_LIST:
		case XL_KW_SET:
		case XL_KW_DICT:
		case XL_KW_TYPE:
		case XL_KW_RANGE: {
			const char* pszName = pParser->token.text;
			int iNameLen = (int)pParser->token.length;
			xlParserAdvance(pParser);
			return xlAstCreateIdent(pszName, iNameLen, iLine, iCol);
		}

		/* arg 关键字在表达式中作为标识符（可变参数变量） */
		case XL_KW_ARG: {
			xlParserAdvance(pParser);
			return xlAstCreateIdent("arg", 3, iLine, iCol);
		}

		/* function 关键字在表达式中作为标识符（类型值） */
		case XL_KW_FUNCTION: {
			xlParserAdvance(pParser);
			return xlAstCreateIdent("function", 8, iLine, iCol);
		}

		/* 括号表达式 */
		case XL_TOK_LPAREN: {
			xlParserAdvance(pParser);
			xvalue xExpr = xlParseExpr(pParser);
			xlParserExpect(pParser, XL_TOK_RPAREN, ")");
			return xExpr;
		}

		/* 方括号字面量 - 数组 或 列表 */
		case XL_TOK_LBRACKET: {
			xlParserAdvance(pParser);
			xvalue xElements = xlAstCreateArray();

			/* 空方括号 */
			if ( xlParserCheck(pParser, XL_TOK_RBRACKET) ) {
				xlParserAdvance(pParser);
				return xlAstCreateArrayLit(xElements, iLine, iCol);
			}

			/* 解析第一个元素 */
			xvalue xFirst = xlParseExpr(pParser);

			if ( xlParserMatch(pParser, XL_OP_COLON) ) {
				/* 列表字面量 [key:val, ...] → 暂用 DictLit 表示 */
				xvalue xEntries = xlAstCreateArray();
				xvalue xFirstVal = xlParseExpr(pParser);
				xvalue xEntry = xlAstCreate("DictEntry", iLine, iCol);
				xlAstSetNode(xEntry, "key", xFirst);
				xlAstSetNode(xEntry, "value", xFirstVal);
				xlAstArrayAppend(xEntries, xEntry);

				while ( xlParserMatch(pParser, XL_TOK_COMMA) ) {
					if ( xlParserCheck(pParser, XL_TOK_RBRACKET) ) break;
					xvalue xKey = xlParseExpr(pParser);
					xlParserExpect(pParser, XL_OP_COLON, ":");
					xvalue xVal = xlParseExpr(pParser);
					xEntry = xlAstCreate("DictEntry", pParser->prevToken.line, pParser->prevToken.column);
					xlAstSetNode(xEntry, "key", xKey);
					xlAstSetNode(xEntry, "value", xVal);
					xlAstArrayAppend(xEntries, xEntry);
				}
				xlParserExpect(pParser, XL_TOK_RBRACKET, "]");
				return xlAstCreateDictLit(xEntries, iLine, iCol);
			} else {
				/* 数组字面量 [val, ...] */
				xlAstArrayAppend(xElements, xFirst);

				while ( xlParserMatch(pParser, XL_TOK_COMMA) ) {
					if ( xlParserCheck(pParser, XL_TOK_RBRACKET) ) break;
					xlAstArrayAppend(xElements, xlParseExpr(pParser));
				}
				xlParserExpect(pParser, XL_TOK_RBRACKET, "]");
				return xlAstCreateArrayLit(xElements, iLine, iCol);
			}
		}

		/* 花括号字面量 - 字典 或 集合 */
		case XL_TOK_LBRACE: {
			xlParserAdvance(pParser);
			xvalue xEntries = xlAstCreateArray();

			/* 空花括号 = 空字典 */
			if ( xlParserCheck(pParser, XL_TOK_RBRACE) ) {
				xlParserAdvance(pParser);
				return xlAstCreateDictLit(xEntries, iLine, iCol);
			}

			/* 判断是字典还是集合 */
			int bIsDict = 0;
			xvalue xFirstKey = 0;

			if ( xlParserCheck(pParser, XL_TOK_IDENT) ) {
				const char* pszKey = pParser->token.text;
				int iKeyLen = (int)pParser->token.length;
				int iKeyLine = pParser->token.line;
				int iKeyCol = pParser->token.column;
				xlParserAdvance(pParser);

				if ( xlParserMatch(pParser, XL_OP_COLON) || xlParserMatch(pParser, XL_OP_ASSIGN) ) {
					bIsDict = 1;
					xFirstKey = xlAstCreateStringLit(pszKey, iKeyLen, iKeyLine, iKeyCol);
				} else {
					xFirstKey = xlAstCreateIdent(pszKey, iKeyLen, iKeyLine, iKeyCol);
				}
			} else if ( (xlParserCheck(pParser, XL_TOK_STRING)) || (xlParserCheck(pParser, XL_TOK_STRING_RAW)) ) {
				const char* pszKey = pParser->token.text + 1;
				int iKeyLen = (int)pParser->token.length - 2;
				int iKeyLine = pParser->token.line;
				int iKeyCol = pParser->token.column;
				xlParserAdvance(pParser);

				if ( xlParserMatch(pParser, XL_OP_COLON) || xlParserMatch(pParser, XL_OP_ASSIGN) ) {
					bIsDict = 1;
					xFirstKey = xlAstCreateStringLit(pszKey, iKeyLen, iKeyLine, iKeyCol);
				} else {
					xFirstKey = xlAstCreateStringLit(pszKey, iKeyLen, iKeyLine, iKeyCol);
				}
			} else {
				xFirstKey = xlParseExpr(pParser);
			}

			if ( bIsDict ) {
				/* 字典字面量 */
				xvalue xFirstVal = xlParseExpr(pParser);
				xvalue xEntry = xlAstCreate("DictEntry", iLine, iCol);
				xlAstSetNode(xEntry, "key", xFirstKey);
				xlAstSetNode(xEntry, "value", xFirstVal);
				xlAstArrayAppend(xEntries, xEntry);

				while ( xlParserMatch(pParser, XL_TOK_COMMA) ) {
					if ( xlParserCheck(pParser, XL_TOK_RBRACE) ) break;
					xvalue xKey = 0;
					if ( xlParserCheck(pParser, XL_TOK_IDENT) ) {
						xKey = xlAstCreateStringLit(pParser->token.text, (int)pParser->token.length,
						                            pParser->token.line, pParser->token.column);
						xlParserAdvance(pParser);
					} else if ( (xlParserCheck(pParser, XL_TOK_STRING)) || (xlParserCheck(pParser, XL_TOK_STRING_RAW)) ) {
						xKey = xlAstCreateStringLit(pParser->token.text + 1, (int)pParser->token.length - 2,
						                            pParser->token.line, pParser->token.column);
						xlParserAdvance(pParser);
					} else {
						xlParserError(pParser, "字典键必须是标识符或字符串");
						break;
					}
					if ( !xlParserMatch(pParser, XL_OP_COLON) && !xlParserMatch(pParser, XL_OP_ASSIGN) ) {
						xlParserError(pParser, "期望 ':' 或 '='");
						break;
					}
					xvalue xVal = xlParseExpr(pParser);
					xEntry = xlAstCreate("DictEntry", pParser->prevToken.line, pParser->prevToken.column);
					xlAstSetNode(xEntry, "key", xKey);
					xlAstSetNode(xEntry, "value", xVal);
					xlAstArrayAppend(xEntries, xEntry);
				}
				xlParserExpect(pParser, XL_TOK_RBRACE, "}");
				return xlAstCreateDictLit(xEntries, iLine, iCol);
			} else {
				/* 集合字面量 */
				xlAstArrayAppend(xEntries, xFirstKey);
				while ( xlParserMatch(pParser, XL_TOK_COMMA) ) {
					if ( xlParserCheck(pParser, XL_TOK_RBRACE) ) break;
					xlAstArrayAppend(xEntries, xlParseExpr(pParser));
				}
				xlParserExpect(pParser, XL_TOK_RBRACE, "}");
				/* 集合用 ArrayLit + kind="SetLit" 表示 */
				xvalue xNode = xlAstCreate("SetLit", iLine, iCol);
				xlAstSetNode(xNode, "elements", xEntries);
				return xNode;
			}
		}

		default:
			xlParserError(pParser, "意外的 Token: %s", xlTokenTypeName(pParser->token.type));
			return 0;
	}
}



// ============================================================================
// 表达式解析 - 后缀表达式
// ============================================================================

static xvalue xlParsePostfixExpr( XL_Parser *pParser )
{
	xvalue xExpr = xlParsePrimaryExpr(pParser);
	if ( !xExpr ) return 0;

	while ( 1 ) {
		/* 后缀操作必须与前一个 token 在同一行，避免把下一行的语句起始误接到当前表达式后面 */
		if ( !xlParserIsSameLineContinuation(pParser) ) {
			break;
		}

		int iLine = pParser->token.line;
		int iCol = pParser->token.column;

		/* 函数调用 expr(...) */
		if ( xlParserMatch(pParser, XL_TOK_LPAREN) ) {
			xvalue xArgs = xlAstCreateArray();
			if ( !xlParserCheck(pParser, XL_TOK_RPAREN) ) {
				do {
					if ( xlParserCheck(pParser, XL_TOK_RPAREN) ) break;
					xlAstArrayAppend(xArgs, xlParseExpr(pParser));
				} while ( xlParserMatch(pParser, XL_TOK_COMMA) );
			}
			xlParserExpect(pParser, XL_TOK_RPAREN, ")");
			xExpr = xlAstCreateCallExpr(xExpr, xArgs, iLine, iCol);
			continue;
		}

		/* 索引 expr[...] */
		if ( xlParserMatch(pParser, XL_TOK_LBRACKET) ) {
			xvalue xIdx = xlParseExpr(pParser);
			xlParserExpect(pParser, XL_TOK_RBRACKET, "]");
			xExpr = xlAstCreateIndexExpr(xExpr, xIdx, iLine, iCol);
			continue;
		}

		/* 安全索引 expr?[...] */
		if ( xlParserMatch(pParser, XL_OP_SAFE_BRACKET) ) {
			xvalue xIdx = xlParseExpr(pParser);
			xlParserExpect(pParser, XL_TOK_RBRACKET, "]");
			xvalue xNode = xlAstCreateIndexExpr(xExpr, xIdx, iLine, iCol);
			xlAstSetBool(xNode, "isSafe", 1);
			xExpr = xNode;
			continue;
		}

		/* 成员访问 expr.member */
		if ( xlParserMatch(pParser, XL_OP_DOT) ) {
			if ( !xlParserCheck(pParser, XL_TOK_IDENT) ) {
				xlParserError(pParser, "期望成员名");
				return xExpr;
			}
			const char* pszMember = pParser->token.text;
			int iMemberLen = (int)pParser->token.length;
			xlParserAdvance(pParser);
			xExpr = xlAstCreateMemberExpr(xExpr, pszMember, iMemberLen, 0, iLine, iCol);
			continue;
		}

		/* 安全成员访问 expr?.member */
		if ( xlParserMatch(pParser, XL_OP_SAFE_DOT) ) {
			if ( !xlParserCheck(pParser, XL_TOK_IDENT) ) {
				xlParserError(pParser, "期望成员名");
				return xExpr;
			}
			const char* pszMember = pParser->token.text;
			int iMemberLen = (int)pParser->token.length;
			xlParserAdvance(pParser);
			xExpr = xlAstCreateMemberExpr(xExpr, pszMember, iMemberLen, 1, iLine, iCol);
			continue;
		}

			/* 后缀自增/自减（必须与操作数在同一行，避免跨行歧义） */
		if ( (pParser->token.type == XL_OP_INC) && (pParser->token.line == pParser->prevToken.line) ) {
			xlParserAdvance(pParser);
			xExpr = xlAstCreateUnaryExpr("++", xExpr, 0, iLine, iCol);
			continue;
		}
		if ( (pParser->token.type == XL_OP_DEC) && (pParser->token.line == pParser->prevToken.line) ) {
			xlParserAdvance(pParser);
			xExpr = xlAstCreateUnaryExpr("--", xExpr, 0, iLine, iCol);
			continue;
		}

		break;
	}

	return xExpr;
}



// ============================================================================
// 表达式解析 - 一元、幂、乘除、加减、位移、比较、相等、位运算、逻辑
// ============================================================================

/* 一元表达式 */
static xvalue xlParseUnaryExpr( XL_Parser *pParser )
{
	int iLine = pParser->token.line;
	int iCol = pParser->token.column;

	if ( xlParserMatch(pParser, XL_OP_MINUS) ) {
		return xlAstCreateUnaryExpr("-", xlParseUnaryExpr(pParser), 1, iLine, iCol);
	}
	if ( xlParserMatch(pParser, XL_KW_NOT) ) {
		return xlAstCreateUnaryExpr("not", xlParseUnaryExpr(pParser), 1, iLine, iCol);
	}
	if ( xlParserMatch(pParser, XL_OP_BANG) ) {
		return xlAstCreateUnaryExpr("!", xlParseUnaryExpr(pParser), 1, iLine, iCol);
	}
	return xlParsePostfixExpr(pParser);
}

/* 幂表达式（右结合） */
static xvalue xlParsePowerExpr( XL_Parser *pParser )
{
	xvalue xLeft = xlParseUnaryExpr(pParser);
	if ( !xLeft ) return 0;
	if ( xlParserIsSameLineContinuation(pParser) && xlParserMatch(pParser, XL_OP_POWER) ) {
		int iLine = pParser->prevToken.line;
		int iCol = pParser->prevToken.column;
		return xlAstCreateBinaryExpr("**", xLeft, xlParsePowerExpr(pParser), iLine, iCol);
	}
	return xLeft;
}

/* 乘除取模 */
static xvalue xlParseMulExpr( XL_Parser *pParser )
{
	xvalue xLeft = xlParsePowerExpr(pParser);
	if ( !xLeft ) return 0;
	while ( 1 ) {
		if ( !xlParserIsSameLineContinuation(pParser) ) break;
		int iLine = pParser->token.line;
		int iCol = pParser->token.column;
		const char* pszOp = NULL;
		if ( xlParserMatch(pParser, XL_OP_STAR) ) pszOp = "*";
		else if ( xlParserMatch(pParser, XL_OP_SLASH) ) pszOp = "/";
		else if ( xlParserMatch(pParser, XL_OP_PERCENT) ) pszOp = "%";
		else break;
		xLeft = xlAstCreateBinaryExpr(pszOp, xLeft, xlParsePowerExpr(pParser), iLine, iCol);
	}
	return xLeft;
}

/* 加减 */
static xvalue xlParseAddExpr( XL_Parser *pParser )
{
	xvalue xLeft = xlParseMulExpr(pParser);
	if ( !xLeft ) return 0;
	while ( 1 ) {
		if ( !xlParserIsSameLineContinuation(pParser) ) break;
		int iLine = pParser->token.line;
		int iCol = pParser->token.column;
		const char* pszOp = NULL;
		if ( xlParserMatch(pParser, XL_OP_PLUS) ) pszOp = "+";
		else if ( xlParserMatch(pParser, XL_OP_MINUS) ) pszOp = "-";
		else break;
		xLeft = xlAstCreateBinaryExpr(pszOp, xLeft, xlParseMulExpr(pParser), iLine, iCol);
	}
	return xLeft;
}

/* 位移 */
static xvalue xlParseShiftExpr( XL_Parser *pParser )
{
	xvalue xLeft = xlParseAddExpr(pParser);
	if ( !xLeft ) return 0;
	while ( 1 ) {
		if ( !xlParserIsSameLineContinuation(pParser) ) break;
		int iLine = pParser->token.line;
		int iCol = pParser->token.column;
		const char* pszOp = NULL;
		if ( xlParserMatch(pParser, XL_OP_SHL) ) pszOp = "<<";
		else if ( xlParserMatch(pParser, XL_OP_SHR) ) pszOp = ">>";
		else break;
		xLeft = xlAstCreateBinaryExpr(pszOp, xLeft, xlParseAddExpr(pParser), iLine, iCol);
	}
	return xLeft;
}

/* 比较 */
static xvalue xlParseCompareExpr( XL_Parser *pParser )
{
	xvalue xLeft = xlParseShiftExpr(pParser);
	if ( !xLeft ) return 0;
	while ( 1 ) {
		if ( !xlParserIsSameLineContinuation(pParser) ) break;
		int iLine = pParser->token.line;
		int iCol = pParser->token.column;
		const char* pszOp = NULL;
		if ( xlParserMatch(pParser, XL_OP_LT) ) pszOp = "<";
		else if ( xlParserMatch(pParser, XL_OP_LE) ) pszOp = "<=";
		else if ( xlParserMatch(pParser, XL_OP_GT) ) pszOp = ">";
		else if ( xlParserMatch(pParser, XL_OP_GE) ) pszOp = ">=";
		else if ( xlParserMatch(pParser, XL_KW_IN) ) pszOp = "in";
		else break;
		xLeft = xlAstCreateBinaryExpr(pszOp, xLeft, xlParseShiftExpr(pParser), iLine, iCol);
	}
	return xLeft;
}

/* 相等 */
static xvalue xlParseEqualityExpr( XL_Parser *pParser )
{
	xvalue xLeft = xlParseCompareExpr(pParser);
	if ( !xLeft ) return 0;
	while ( 1 ) {
		if ( !xlParserIsSameLineContinuation(pParser) ) break;
		int iLine = pParser->token.line;
		int iCol = pParser->token.column;
		const char* pszOp = NULL;
		if ( xlParserMatch(pParser, XL_OP_EQ) ) pszOp = "==";
		else if ( xlParserMatch(pParser, XL_OP_NE) ) pszOp = "!=";
		else if ( xlParserMatch(pParser, XL_OP_APPROX) ) pszOp = "~=";
		else break;
		xLeft = xlAstCreateBinaryExpr(pszOp, xLeft, xlParseCompareExpr(pParser), iLine, iCol);
	}
	return xLeft;
}

/* 位与 */
static xvalue xlParseBitAndExpr( XL_Parser *pParser )
{
	xvalue xLeft = xlParseEqualityExpr(pParser);
	if ( !xLeft ) return 0;
	while ( xlParserIsSameLineContinuation(pParser) && xlParserMatch(pParser, XL_OP_AMP) ) {
		int iLine = pParser->prevToken.line;
		int iCol = pParser->prevToken.column;
		xLeft = xlAstCreateBinaryExpr("&", xLeft, xlParseEqualityExpr(pParser), iLine, iCol);
	}
	return xLeft;
}

/* 位异或 */
static xvalue xlParseBitXorExpr( XL_Parser *pParser )
{
	xvalue xLeft = xlParseBitAndExpr(pParser);
	if ( !xLeft ) return 0;
	while ( xlParserIsSameLineContinuation(pParser) && xlParserMatch(pParser, XL_OP_CARET) ) {
		int iLine = pParser->prevToken.line;
		int iCol = pParser->prevToken.column;
		xLeft = xlAstCreateBinaryExpr("^", xLeft, xlParseBitAndExpr(pParser), iLine, iCol);
	}
	return xLeft;
}

/* 位或 */
static xvalue xlParseBitOrExpr( XL_Parser *pParser )
{
	xvalue xLeft = xlParseBitXorExpr(pParser);
	if ( !xLeft ) return 0;
	while ( xlParserIsSameLineContinuation(pParser) && xlParserMatch(pParser, XL_OP_PIPE) ) {
		int iLine = pParser->prevToken.line;
		int iCol = pParser->prevToken.column;
		xLeft = xlAstCreateBinaryExpr("|", xLeft, xlParseBitXorExpr(pParser), iLine, iCol);
	}
	return xLeft;
}

/* 逻辑与 */
static xvalue xlParseAndExpr( XL_Parser *pParser )
{
	xvalue xLeft = xlParseBitOrExpr(pParser);
	if ( !xLeft ) return 0;
	while ( xlParserIsSameLineContinuation(pParser) && xlParserMatch(pParser, XL_KW_AND) ) {
		int iLine = pParser->prevToken.line;
		int iCol = pParser->prevToken.column;
		xLeft = xlAstCreateBinaryExpr("and", xLeft, xlParseBitOrExpr(pParser), iLine, iCol);
	}
	return xLeft;
}

/* 逻辑或 */
static xvalue xlParseOrExpr( XL_Parser *pParser )
{
	xvalue xLeft = xlParseAndExpr(pParser);
	if ( !xLeft ) return 0;
	while ( xlParserIsSameLineContinuation(pParser) && xlParserMatch(pParser, XL_KW_OR) ) {
		int iLine = pParser->prevToken.line;
		int iCol = pParser->prevToken.column;
		xLeft = xlAstCreateBinaryExpr("or", xLeft, xlParseAndExpr(pParser), iLine, iCol);
	}
	return xLeft;
}

/* 空值合并 ?? */
static xvalue xlParseCoalesceExpr( XL_Parser *pParser )
{
	xvalue xLeft = xlParseOrExpr(pParser);
	if ( !xLeft ) return 0;
	while ( xlParserIsSameLineContinuation(pParser) && xlParserMatch(pParser, XL_OP_NULL_COALESCE) ) {
		int iLine = pParser->prevToken.line;
		int iCol = pParser->prevToken.column;
		xLeft = xlAstCreateBinaryExpr("??", xLeft, xlParseOrExpr(pParser), iLine, iCol);
	}
	return xLeft;
}

/* 范围 a..b */
static xvalue xlParseRangeExpr( XL_Parser *pParser )
{
	xvalue xLeft = xlParseCoalesceExpr(pParser);
	if ( !xLeft ) return 0;
	if ( xlParserIsSameLineContinuation(pParser) && xlParserMatch(pParser, XL_OP_RANGE) ) {
		int iLine = pParser->prevToken.line;
		int iCol = pParser->prevToken.column;
		xLeft = xlAstCreateBinaryExpr("..", xLeft, xlParseCoalesceExpr(pParser), iLine, iCol);
	}
	return xLeft;
}

/* 三元表达式 */
static xvalue xlParseTernaryExpr( XL_Parser *pParser )
{
	xvalue xCond = xlParseRangeExpr(pParser);
	if ( !xCond ) return 0;
	if ( xlParserIsSameLineContinuation(pParser) && xlParserMatch(pParser, XL_OP_QUESTION) ) {
		int iLine = pParser->prevToken.line;
		int iCol = pParser->prevToken.column;
		xvalue xThen = xlParseExpr(pParser);
		xlParserExpect(pParser, XL_OP_COLON, ":");
		xvalue xElse = xlParseTernaryExpr(pParser);
		return xlAstCreateTernaryExpr(xCond, xThen, xElse, iLine, iCol);
	}
	return xCond;
}

/* 赋值表达式（右结合） */
static xvalue xlParseAssignExpr( XL_Parser *pParser )
{
	xvalue xLeft = xlParseTernaryExpr(pParser);
	if ( !xLeft ) return 0;

	int iLine = pParser->token.line;
	int iCol = pParser->token.column;
	const char* pszOp = NULL;

	if ( xlParserIsSameLineContinuation(pParser) ) {
		if ( xlParserMatch(pParser, XL_OP_ASSIGN) ) pszOp = "=";
		else if ( xlParserMatch(pParser, XL_OP_PLUS_ASSIGN) ) pszOp = "+=";
		else if ( xlParserMatch(pParser, XL_OP_MINUS_ASSIGN) ) pszOp = "-=";
		else if ( xlParserMatch(pParser, XL_OP_STAR_ASSIGN) ) pszOp = "*=";
		else if ( xlParserMatch(pParser, XL_OP_SLASH_ASSIGN) ) pszOp = "/=";
		else if ( xlParserMatch(pParser, XL_OP_PERCENT_ASSIGN) ) pszOp = "%=";
		else if ( xlParserMatch(pParser, XL_OP_POWER_ASSIGN) ) pszOp = "**=";
		else if ( xlParserMatch(pParser, XL_OP_NULL_ASSIGN) ) pszOp = "?=";
	}

	if ( pszOp ) {
		return xlAstCreateAssignExpr(pszOp, xLeft, xlParseAssignExpr(pParser), iLine, iCol);
	}
	return xLeft;
}

/* 表达式入口 */
static xvalue xlParseExpr( XL_Parser *pParser )
{
	return xlParseAssignExpr(pParser);
}



// ============================================================================
// 语句解析
// ============================================================================

// 解析变量声明
// 形式: Type name [= expr]  或  var/auto name [= expr]  或  const [Type] name = expr
static xvalue xlParseVarDecl( XL_Parser *pParser, int bIsConst )
{
	int iLine = pParser->token.line;
	int iCol = pParser->token.column;

	const char* pszType = NULL;
	int iTypeLen = 0;

	if ( bIsConst ) {
		/* const 后面可选类型名 */
		if ( (xlParserCheck(pParser, XL_KW_VAR)) || (xlParserCheck(pParser, XL_KW_AUTO)) ) {
			xlParserAdvance(pParser);
		} else if ( xlParserCheckTypeName(pParser) ) {
			/* 如果 IDENT 是类型名，且下一个也是 IDENT → 有类型注解 */
			if ( xlParserPeek(pParser) == XL_TOK_IDENT ) {
				pszType = pParser->token.text;
				iTypeLen = (int)pParser->token.length;
				xlParserAdvance(pParser);
			}
		}
	} else if ( (xlParserMatch(pParser, XL_KW_VAR)) || (xlParserMatch(pParser, XL_KW_AUTO)) ) {
		/* var / auto 声明 */
	} else {
		/* 类型名开头: Type name */
		pszType = pParser->token.text;
		iTypeLen = (int)pParser->token.length;
		xlParserAdvance(pParser);
	}

	/* 变量名 */
	if ( !xlParserCheck(pParser, XL_TOK_IDENT) ) {
		xlParserError(pParser, "期望变量名");
		return 0;
	}

	const char* pszName = pParser->token.text;
	int iNameLen = (int)pParser->token.length;
	xlParserAdvance(pParser);

	/* 初始值（可选） */
	xvalue xInit = 0;
	if ( xlParserMatch(pParser, XL_OP_ASSIGN) ) {
		xInit = xlParseExpr(pParser);
	}

	return xlAstCreateVarDecl(pszName, iNameLen, pszType, iTypeLen, xInit, bIsConst, iLine, iCol);
}


// 解析 if 语句
static xvalue xlParseIfStmt( XL_Parser *pParser )
{
	int iLine = pParser->token.line;
	int iCol = pParser->token.column;
	xlParserExpect(pParser, XL_KW_IF, "if");

	xvalue xCond = xlParseExpr(pParser);

	/* then（可选） - 判断是否是单行模式 */
	int bSingleLine = 0;
	if ( xlParserCheck(pParser, XL_KW_THEN) ) {
		int iThenLine = pParser->token.line;
		xlParserAdvance(pParser);
		/* 如果 then 后的 token 在同一行 → 单行 if */
		if ( pParser->token.line == iThenLine ) {
			bSingleLine = 1;
		}
	}

	xvalue xThen;
	xvalue xElseIfs = xlAstCreateArray();
	xvalue xElse = 0;

	if ( bSingleLine ) {
		xThen = xlAstCreateBlockStmt(pParser->token.line, pParser->token.column);
		xvalue xStmt = xlParseStmt(pParser);
		if ( xStmt ) xlAstBlockAddStmt(xThen, xStmt);
	} else {
		xThen = xlParseBlockStmt(pParser);

		/* elseif 列表 */
		while ( xlParserMatch(pParser, XL_KW_ELSEIF) ) {
			xvalue xEICond = xlParseExpr(pParser);
			xlParserMatch(pParser, XL_KW_THEN);
			xvalue xEIBody = xlParseBlockStmt(pParser);

			xvalue xElseIf = xlAstCreate("ElseIf", pParser->token.line, pParser->token.column);
			xlAstSetNode(xElseIf, "cond", xEICond);
			xlAstSetNode(xElseIf, "body", xEIBody);
			xlAstArrayAppend(xElseIfs, xElseIf);
		}

		/* else 块（可选） */
		if ( xlParserMatch(pParser, XL_KW_ELSE) ) {
			xElse = xlParseBlockStmt(pParser);
		}

		xlParserExpect(pParser, XL_KW_END, "end");
	}

	return xlAstCreateIfStmt(xCond, xThen, xElseIfs, xElse, iLine, iCol);
}


// 解析 while 语句: while expr do ... end
static xvalue xlParseWhileStmt( XL_Parser *pParser )
{
	int iLine = pParser->token.line;
	int iCol = pParser->token.column;
	xlParserExpect(pParser, XL_KW_WHILE, "while");

	xvalue xCond = xlParseExpr(pParser);
	xlParserExpect(pParser, XL_KW_DO, "do");
	xvalue xBody = xlParseBlockStmt(pParser);
	xlParserExpect(pParser, XL_KW_END, "end");

	return xlAstCreateWhileStmt(xCond, xBody, iLine, iCol);
}


// 解析 do-while 语句: do ... while expr end
static xvalue xlParseDoWhileStmt( XL_Parser *pParser )
{
	int iLine = pParser->token.line;
	int iCol = pParser->token.column;
	xlParserExpect(pParser, XL_KW_DO, "do");

	/* 解析 body 直到 while */
	int iBlockLine = pParser->token.line;
	int iBlockCol = pParser->token.column;
	xvalue xBody = xlAstCreateBlockStmt(iBlockLine, iBlockCol);

	while ( !xlParserCheck(pParser, XL_KW_WHILE) && !xlParserCheck(pParser, XL_TOK_EOF) ) {
		char* pszPrev = pParser->token.text;
		xvalue xStmt = xlParseStmt(pParser);
		if ( xStmt ) xlAstBlockAddStmt(xBody, xStmt);
		if ( pParser->bPanicMode ) {
			xlParserSync(pParser);
		} else if ( (!xStmt) && (pParser->token.text == pszPrev) ) {
			xlParserAdvance(pParser);
		}
	}

	xlParserExpect(pParser, XL_KW_WHILE, "while");
	xvalue xCond = xlParseExpr(pParser);
	xlParserExpect(pParser, XL_KW_END, "end");

	return xlAstCreateDoWhileStmt(xBody, xCond, iLine, iCol);
}


// 解析 for 语句
static xvalue xlParseForStmt( XL_Parser *pParser )
{
	int iLine = pParser->token.line;
	int iCol = pParser->token.column;
	xlParserExpect(pParser, XL_KW_FOR, "for");

	if ( !xlParserCheck(pParser, XL_TOK_IDENT) ) {
		xlParserError(pParser, "期望变量名");
		return 0;
	}

	const char* pszVar = pParser->token.text;
	int iVarLen = (int)pParser->token.length;
	xlParserAdvance(pParser);

	/* for k, v in expr */
	if ( xlParserMatch(pParser, XL_TOK_COMMA) ) {
		if ( !xlParserCheck(pParser, XL_TOK_IDENT) ) {
			xlParserError(pParser, "期望变量名");
			return 0;
		}
		const char* pszVal = pParser->token.text;
		int iValLen = (int)pParser->token.length;
		xlParserAdvance(pParser);

		xlParserExpect(pParser, XL_KW_IN, "in");
		xvalue xIter = xlParseExpr(pParser);
		xvalue xBody = xlParseBlockStmt(pParser);
		xlParserExpect(pParser, XL_KW_END, "end");

		return xlAstCreateForInStmt(pszVar, iVarLen, pszVal, iValLen, xIter, xBody, iLine, iCol);
	}

	/* for v in expr */
	if ( xlParserMatch(pParser, XL_KW_IN) ) {
		xvalue xIter = xlParseExpr(pParser);
		xvalue xBody = xlParseBlockStmt(pParser);
		xlParserExpect(pParser, XL_KW_END, "end");

		return xlAstCreateForInStmt(NULL, 0, pszVar, iVarLen, xIter, xBody, iLine, iCol);
	}

	/* for i = start to end [step n] */
	if ( xlParserMatch(pParser, XL_OP_ASSIGN) ) {
		xvalue xStart = xlParseExpr(pParser);
		xlParserExpect(pParser, XL_KW_TO, "to");
		xvalue xEnd = xlParseExpr(pParser);

		xvalue xStep = 0;
		if ( xlParserMatch(pParser, XL_KW_STEP) ) {
			xStep = xlParseExpr(pParser);
		}

		xvalue xBody = xlParseBlockStmt(pParser);
		xlParserExpect(pParser, XL_KW_END, "end");

		return xlAstCreateForStmt(pszVar, iVarLen, xStart, xEnd, xStep, xBody, iLine, iCol);
	}

	xlParserError(pParser, "无效的 for 语句，期望 'in'、',' 或 '='");
	return 0;
}


// 解析 return 语句
static xvalue xlParseReturnStmt( XL_Parser *pParser )
{
	int iLine = pParser->token.line;
	int iCol = pParser->token.column;
	xlParserExpect(pParser, XL_KW_RETURN, "return");

	xvalue xValue = 0;
	/* 检查后面是否有返回值（不是 end/else/elseif/EOF） */
	if ( !xlParserCheck(pParser, XL_KW_END) &&
	     !xlParserCheck(pParser, XL_KW_ELSE) &&
	     !xlParserCheck(pParser, XL_KW_ELSEIF) &&
	     !xlParserCheck(pParser, XL_TOK_EOF) ) {
		xValue = xlParseExpr(pParser);
	}

	return xlAstCreateReturnStmt(xValue, iLine, iCol);
}


// 解析块语句（直到 end/else/elseif/EOF）
static xvalue xlParseBlockStmt( XL_Parser *pParser )
{
	int iLine = pParser->token.line;
	int iCol = pParser->token.column;
	xvalue xBlock = xlAstCreateBlockStmt(iLine, iCol);

	while ( !xlParserCheck(pParser, XL_KW_END) &&
	        !xlParserCheck(pParser, XL_KW_ELSE) &&
	        !xlParserCheck(pParser, XL_KW_ELSEIF) &&
	        !xlParserCheck(pParser, XL_KW_WHILE) &&
	        !xlParserCheck(pParser, XL_TOK_EOF) ) {
		char* pszPrev = pParser->token.text;
		xvalue xStmt = xlParseStmt(pParser);
		if ( xStmt ) {
			xlAstBlockAddStmt(xBlock, xStmt);
		}
		if ( pParser->bPanicMode ) {
			xlParserSync(pParser);
		} else if ( (!xStmt) && (pParser->token.text == pszPrev) ) {
			xlParserAdvance(pParser);
		}
	}

	return xBlock;
}


// 解析语句
static xvalue xlParseStmt( XL_Parser *pParser )
{
	switch ( pParser->token.type ) {
		case XL_KW_VAR:
		case XL_KW_AUTO:
			return xlParseVarDecl(pParser, 0);

		case XL_KW_BOOL:
		case XL_KW_INT:
		case XL_KW_FLOAT:
		case XL_KW_STRING:
		case XL_KW_TIME:
		case XL_KW_POINT:
		case XL_KW_ARRAY:
		case XL_KW_LIST:
		case XL_KW_SET:
		case XL_KW_DICT:
		case XL_KW_TYPE:
		case XL_KW_RANGE:
			return xlParseVarDecl(pParser, 0);

		case XL_KW_CONST:
			xlParserAdvance(pParser);
			return xlParseVarDecl(pParser, 1);

		case XL_KW_IF:
			return xlParseIfStmt(pParser);

		case XL_KW_WHILE:
			return xlParseWhileStmt(pParser);

		case XL_KW_DO:
			return xlParseDoWhileStmt(pParser);

		case XL_KW_FOR:
			return xlParseForStmt(pParser);

		case XL_KW_RETURN:
			return xlParseReturnStmt(pParser);

		case XL_KW_BREAK: {
			int iLine = pParser->token.line;
			int iCol = pParser->token.column;
			xlParserAdvance(pParser);
			int iLevel = 1;
			if ( (pParser->token.type == XL_TOK_INT) && (pParser->token.line == pParser->prevToken.line) ) {
				iLevel = (int)pParser->token.iVal;
				xlParserAdvance(pParser);
			}
			return xlAstCreateBreakStmt(iLevel, iLine, iCol);
		}

		case XL_KW_CONTINUE: {
			int iLine = pParser->token.line;
			int iCol = pParser->token.column;
			xlParserAdvance(pParser);
			int iLevel = 1;
			if ( (pParser->token.type == XL_TOK_INT) && (pParser->token.line == pParser->prevToken.line) ) {
				iLevel = (int)pParser->token.iVal;
				xlParserAdvance(pParser);
			}
			return xlAstCreateContinueStmt(iLevel, iLine, iCol);
		}

		case XL_TOK_IDENT: {
			/* 类型名开头的变量声明: Type name [= expr] */
			if ( xlParserCheckTypeName(pParser) ) {
				/* 前瞻: 如果下一个是 IDENT → 变量声明 */
				if ( xlParserPeek(pParser) == XL_TOK_IDENT ) {
					return xlParseVarDecl(pParser, 0);
				}
			}
			/* 否则是表达式语句 */
			break;
		}

		default:
			break;
	}

	/* 表达式语句 */
	int iLine = pParser->token.line;
	int iCol = pParser->token.column;
	xvalue xExpr = xlParseExpr(pParser);
	if ( xExpr ) {
		return xlAstCreateExprStmt(xExpr, iLine, iCol);
	}
	return 0;
}



// ============================================================================
// 声明解析
// ============================================================================

// 解析参数列表
static xvalue xlParseParamList( XL_Parser *pParser )
{
	xvalue xParams = xlAstCreateArray();

	xlParserExpect(pParser, XL_TOK_LPAREN, "(");

	/* 可变参数 ... */
	if ( xlParserMatch(pParser, XL_OP_ELLIPSIS) ) {
		xlParserExpect(pParser, XL_TOK_RPAREN, ")");
		/* 标记可变参数 - 空数组 + 设置一个标志 */
		xvalue xParam = xlAstCreate(XL_AST_PARAM_DECL, pParser->prevToken.line, pParser->prevToken.column);
		xlAstSetStr(xParam, "name", "...", 3);
		xlAstSetBool(xParam, "isVariadic", 1);
		xlAstArrayAppend(xParams, xParam);
		return xParams;
	}
	/* 字典可变参数 *** */
	if ( xlParserMatch(pParser, XL_OP_ELLIPSIS_DICT) ) {
		xlParserExpect(pParser, XL_TOK_RPAREN, ")");
		xvalue xParam = xlAstCreate(XL_AST_PARAM_DECL, pParser->prevToken.line, pParser->prevToken.column);
		xlAstSetStr(xParam, "name", "***", 3);
		xlAstSetBool(xParam, "isVariadicDict", 1);
		xlAstArrayAppend(xParams, xParam);
		return xParams;
	}

	if ( !xlParserCheck(pParser, XL_TOK_RPAREN) ) {
		do {
			int iParamLine = pParser->token.line;
			int iParamCol = pParser->token.column;

			const char* pszType = NULL;
			int iTypeLen = 0;
			const char* pszName = NULL;
			int iNameLen = 0;

			/* 检查是否有类型注解 */
			if ( (xlParserCheck(pParser, XL_KW_VAR)) || (xlParserCheck(pParser, XL_KW_AUTO)) ) {
				xlParserAdvance(pParser);
			} else if ( xlParserCheckTypeName(pParser) && (xlParserPeek(pParser) == XL_TOK_IDENT) ) {
				pszType = pParser->token.text;
				iTypeLen = (int)pParser->token.length;
				xlParserAdvance(pParser);
			}

			/* 参数名 */
			if ( !xlParserCheck(pParser, XL_TOK_IDENT) ) {
				xlParserError(pParser, "期望参数名");
				break;
			}
			pszName = pParser->token.text;
			iNameLen = (int)pParser->token.length;
			xlParserAdvance(pParser);

			/* 默认值（可选） */
			xvalue xDefault = 0;
			if ( xlParserMatch(pParser, XL_OP_ASSIGN) ) {
				xDefault = xlParseExpr(pParser);
			}

			xvalue xParam = xlAstCreateParamDecl(pszName, iNameLen, pszType, iTypeLen, xDefault, iParamLine, iParamCol);
			xlAstArrayAppend(xParams, xParam);

		} while ( xlParserMatch(pParser, XL_TOK_COMMA) );
	}

	xlParserExpect(pParser, XL_TOK_RPAREN, ")");
	return xParams;
}


// 解析函数声明: func name(params) [as RetType[, RetType2]] ... end
static xvalue xlParseFuncDecl( XL_Parser *pParser )
{
	int iLine = pParser->token.line;
	int iCol = pParser->token.column;
	xlParserExpect(pParser, XL_KW_FUNC, "func");

	/* 函数名 */
	if ( !xlParserCheck(pParser, XL_TOK_IDENT) ) {
		xlParserError(pParser, "期望函数名");
		return 0;
	}
	const char* pszName = pParser->token.text;
	int iNameLen = (int)pParser->token.length;
	xlParserAdvance(pParser);

	/* 参数列表 */
	xvalue xParams = xlParseParamList(pParser);

	/* 返回类型（可选: as Type） */
	const char* pszRetType = NULL;
	int iRetTypeLen = 0;
	if ( xlParserMatch(pParser, XL_KW_AS) ) {
		if ( xlParserCheckTypeName(pParser) || xlParserCheck(pParser, XL_TOK_IDENT) ) {
			pszRetType = pParser->token.text;
			iRetTypeLen = (int)pParser->token.length;
			xlParserAdvance(pParser);
		} else {
			xlParserError(pParser, "期望返回类型");
		}
	}

	/* 函数体 */
	xvalue xBody = xlParseBlockStmt(pParser);

	xlParserExpect(pParser, XL_KW_END, "end");

	return xlAstCreateFuncDecl(pszName, iNameLen, xParams, pszRetType, iRetTypeLen, xBody, iLine, iCol);
}


// 解析顶层声明
static xvalue xlParseDecl( XL_Parser *pParser )
{
	switch ( pParser->token.type ) {
		case XL_KW_FUNC:
			return xlParseFuncDecl(pParser);
		default:
			return xlParseStmt(pParser);
	}
}



// ============================================================================
// 解析入口
// ============================================================================

static inline xvalue xlParserParse( XL_Parser *pParser, const char* pszFileName )
{
	xvalue xModule = xlAstCreateModule(pszFileName);

	/* 读取第一个 Token */
	xlParserAdvance(pParser);

	/* 解析所有顶层声明 */
	while ( !xlParserCheck(pParser, XL_TOK_EOF) ) {
		char* pszPrev = pParser->token.text;

		xvalue xDecl = xlParseDecl(pParser);
		if ( xDecl ) {
			xlAstModuleAddStmt(xModule, xDecl);
		}

		if ( pParser->bPanicMode ) {
			xlParserSync(pParser);
		} else if ( (!xDecl) && (pParser->token.text == pszPrev) ) {
			xlParserAdvance(pParser);
		}
	}

	return xModule;
}


#endif /* XL_PARSER_H */
