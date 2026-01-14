/**
 * xlang - 语法分析器模块
 * 
 * 手写的递归下降语法分析器
 * 支持 xlang 第三版语法
 */

#ifndef XL_PARSER_H
#define XL_PARSER_H

#include "xl_common.h"
#include "xl_error.h"
#include "xl_lexer.h"
#include "xl_ast.h"




// ============================================================================
// Parser 结构
// ============================================================================

typedef struct XL_Parser {
	XL_Lexer *pLexer;       // 词法分析器
	XL_Token token;         // 当前 Token
	XL_Token prevToken;     // 上一个 Token
	bool bHadError;         // 是否发生错误
	bool bPanicMode;        // 恐慌模式（错误恢复）
	XL_ErrorList *pErrors;  // 错误列表
} XL_Parser;




// ============================================================================
// Parser 辅助函数 - 前向声明
// ============================================================================

static xvalue xlParseExpr( XL_Parser *pParser );
static xvalue xlParseStmt( XL_Parser *pParser );
static xvalue xlParseDecl( XL_Parser *pParser );
static xvalue xlParseBlockStmt( XL_Parser *pParser );




// ============================================================================
// Parser 创建与销毁
// ============================================================================

static inline XL_Parser* xlParserCreate( XL_Lexer *pLexer )
{
	XL_Parser *pParser = (XL_Parser*)malloc(sizeof(XL_Parser));
	if ( !pParser ) return NULL;

	pParser->pLexer = pLexer;
	pParser->bHadError = false;
	pParser->bPanicMode = false;
	pParser->pErrors = pLexer->pErrors;

	memset(&pParser->token, 0, sizeof(XL_Token));
	memset(&pParser->prevToken, 0, sizeof(XL_Token));

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

// 前进到下一个 Token
static inline void xlParserAdvance( XL_Parser *pParser )
{
	pParser->prevToken = pParser->token;

	while ( true ) {
		int iResult = xlLexerNext(pParser->pLexer, &pParser->token);

		// 跳过换行符（简化处理）
		if ( pParser->token.kind == XL_TK_NEWLINE ) {
			continue;
		}

		if ( iResult >= 0 ) break;

		// 词法错误
		pParser->bHadError = true;
	}
}


// 检查当前 Token 类型
static inline bool xlParserCheck( XL_Parser *pParser, XL_TokenKind kind )
{
	return pParser->token.kind == kind;
}


// 匹配并前进
static inline bool xlParserMatch( XL_Parser *pParser, XL_TokenKind kind )
{
	if ( !xlParserCheck(pParser, kind) ) return false;
	xlParserAdvance(pParser);
	return true;
}


// 期望特定 Token，否则报错
static inline bool xlParserExpect( XL_Parser *pParser, XL_TokenKind kind, str pszMsg )
{
	if ( xlParserCheck(pParser, kind) ) {
		xlParserAdvance(pParser);
		return true;
	}

	xlErrorListAddFmt(pParser->pErrors, XL_ERR_SYN_EXPECTED_TOKEN,
	                  pParser->token.iLine, pParser->token.iColumn,
	                  (str)"期望 %s，但得到 %s",
	                  (char*)xlTokenKindName(kind),
	                  (char*)xlTokenKindName(pParser->token.kind));
	pParser->bHadError = true;
	pParser->bPanicMode = true;
	return false;
}


// 同步（错误恢复）
static inline void xlParserSync( XL_Parser *pParser )
{
	pParser->bPanicMode = false;

	while ( pParser->token.kind != XL_TK_EOF ) {
		// 在语句边界同步
		switch ( pParser->token.kind ) {
			case XL_TK_FUNCTION:
			case XL_TK_CLASS:
			case XL_TK_VAR:
			case XL_TK_FOR:
			case XL_TK_IF:
			case XL_TK_WHILE:
			case XL_TK_RETURN:
			case XL_TK_END:
				return;
			default:
				break;
		}
		xlParserAdvance(pParser);
	}
}




// ============================================================================
// 类型解析
// ============================================================================

// 解析类型引用
static inline xvalue xlParseTypeRef( XL_Parser *pParser )
{
	u32 iLine = pParser->token.iLine;
	u32 iCol = pParser->token.iColumn;

	// 基础类型关键字或标识符
	str pszName = pParser->token.pszStart;
	u32 iNameLen = pParser->token.iLength;

	// 检查是否为类型关键字
	bool bIsType = false;
	switch ( pParser->token.kind ) {
		case XL_TK_INT_T:
		case XL_TK_INT8:
		case XL_TK_INT16:
		case XL_TK_INT32:
		case XL_TK_INT64:
		case XL_TK_UINT8:
		case XL_TK_UINT16:
		case XL_TK_UINT32:
		case XL_TK_UINT64:
		case XL_TK_FLOAT_T:
		case XL_TK_FLOAT32:
		case XL_TK_FLOAT64:
		case XL_TK_STRING_T:
		case XL_TK_BOOL:
		case XL_TK_VAR:
		case XL_TK_AUTO:
		case XL_TK_VOID:
		case XL_TK_ARRAY:
		case XL_TK_LIST:
		case XL_TK_SET:
		case XL_TK_DICT:
		case XL_TK_IDENT:
			bIsType = true;
			break;
		default:
			break;
	}

	if ( !bIsType ) {
		return 0;
	}

	xlParserAdvance(pParser);

	// 检查数组类型 type[]
	bool bIsArray = false;
	if ( xlParserMatch(pParser, XL_TK_LBRACKET) ) {
		xlParserExpect(pParser, XL_TK_RBRACKET, (str)"]");
		bIsArray = true;
	}

	// 检查可空类型 type?
	bool bIsNullable = false;
	if ( xlParserMatch(pParser, XL_TK_QUESTION) ) {
		bIsNullable = true;
	}

	return xlAstCreateTypeRef(pszName, iNameLen, bIsArray, bIsNullable, iLine, iCol);
}




// ============================================================================
// 表达式解析
// ============================================================================

// 解析基础表达式（字面量、标识符、括号）
static xvalue xlParsePrimaryExpr( XL_Parser *pParser )
{
	u32 iLine = pParser->token.iLine;
	u32 iCol = pParser->token.iColumn;

	switch ( pParser->token.kind ) {
		// 字面量
		case XL_TK_INT:
		case XL_TK_INT_HEX:
		case XL_TK_INT_OCT:
		case XL_TK_INT_BIN: {
			i64 iValue = pParser->token.iValue;
			xlParserAdvance(pParser);
			return xlAstCreateIntLit(iValue, iLine, iCol);
		}

		case XL_TK_FLOAT: {
			f64 fValue = pParser->token.fValue;
			xlParserAdvance(pParser);
			return xlAstCreateFloatLit(fValue, iLine, iCol);
		}

		case XL_TK_STRING:
		case XL_TK_STRING_RAW:
		case XL_TK_STRING_MULTI:
		case XL_TK_STRING_MULTI_RAW: {
			str pszValue = pParser->token.sValue.pszText;
			u32 iLen = pParser->token.sValue.iLen;
			xlParserAdvance(pParser);
			xvalue xNode = xlAstCreateStringLit(pszValue, iLen, iLine, iCol);
			// 注意：字符串值已经在词法分析时分配，这里复制到 AST
			XL_SAFE_FREE(pszValue);
			return xNode;
		}

		// 模板字符串 `hello {$name}`
		case XL_TK_TEMPLATE:
		case XL_TK_TEMPLATE_MULTI: {
			str pszValue = pParser->token.sValue.pszText;
			u32 iLen = pParser->token.sValue.iLen;
			xlParserAdvance(pParser);
			xvalue xNode = xlAstCreateTemplateLit(pszValue, iLen, iLine, iCol);
			XL_SAFE_FREE(pszValue);
			return xNode;
		}

		case XL_TK_TRUE: {
			xlParserAdvance(pParser);
			return xlAstCreateBoolLit(true, iLine, iCol);
		}

		case XL_TK_FALSE: {
			xlParserAdvance(pParser);
			return xlAstCreateBoolLit(false, iLine, iCol);
		}

		case XL_TK_NULL: {
			xlParserAdvance(pParser);
			return xlAstCreateNullLit(iLine, iCol);
		}

		// 时间字面量
		case XL_TK_TIME: {
			str pszValue = pParser->token.sValue.pszText;
			u32 iLen = pParser->token.sValue.iLen;
			xlParserAdvance(pParser);
			xvalue xNode = xlAstCreateTimeLit(pszValue, iLen, iLine, iCol);
			XL_SAFE_FREE(pszValue);
			return xNode;
		}

		// 标识符
		case XL_TK_IDENT: {
			str pszName = pParser->token.pszStart;
			u32 iNameLen = pParser->token.iLength;
			xlParserAdvance(pParser);
			return xlAstCreateIdent(pszName, iNameLen, iLine, iCol);
		}

		// self / super
		case XL_TK_SELF: {
			xlParserAdvance(pParser);
			return xlAstCreateIdent((str)"self", 4, iLine, iCol);
		}

		case XL_TK_SUPER: {
			xlParserAdvance(pParser);
			return xlAstCreateIdent((str)"super", 5, iLine, iCol);
		}

		// range 关键字作为函数调用
		case XL_TK_RANGE: {
			xlParserAdvance(pParser);
			return xlAstCreateIdent((str)"range", 5, iLine, iCol);
		}

		// 括号表达式
		case XL_TK_LPAREN: {
			xlParserAdvance(pParser);
			xvalue xExpr = xlParseExpr(pParser);
			xlParserExpect(pParser, XL_TK_RPAREN, (str)")");
			return xExpr;
		}

		// 方括号字面量 [...] - 可能是数组或列表
		// [] = 空数组, [val, ...] = 数组, [key:val, ...] = 列表
		case XL_TK_LBRACKET: {
			xlParserAdvance(pParser);
			xvalue xElements = xlAstCreateArray();

			// 空方括号 = 空数组
			if ( xlParserCheck(pParser, XL_TK_RBRACKET) ) {
				xlParserAdvance(pParser);
				return xlAstCreateArrayLit(xElements, iLine, iCol);
			}

			// 解析第一个元素，判断是数组还是列表
			xvalue xFirst = xlParseExpr(pParser);

			if ( xlParserMatch(pParser, XL_TK_COLON) ) {
				// 列表字面量 [key:val, ...]
				xvalue xFirstVal = xlParseExpr(pParser);
				xvalue xEntry = xlAstCreate((str)"ListEntry", iLine, iCol);
				xvoTableSetValue(xEntry, (str)"key", 0, xFirst, FALSE);
				xvoTableSetValue(xEntry, (str)"value", 0, xFirstVal, FALSE);
				xlAstArrayAppend(xElements, xEntry);

				while ( xlParserMatch(pParser, XL_TK_COMMA) ) {
					xvalue xKey = xlParseExpr(pParser);
					xlParserExpect(pParser, XL_TK_COLON, (str)":");
					xvalue xVal = xlParseExpr(pParser);
					xEntry = xlAstCreate((str)"ListEntry", pParser->token.iLine, pParser->token.iColumn);
					xvoTableSetValue(xEntry, (str)"key", 0, xKey, FALSE);
					xvoTableSetValue(xEntry, (str)"value", 0, xVal, FALSE);
					xlAstArrayAppend(xElements, xEntry);
				}

				xlParserExpect(pParser, XL_TK_RBRACKET, (str)"]");
				return xlAstCreateListLit(xElements, iLine, iCol);
			} else {
				// 数组字面量 [val, ...]
				xlAstArrayAppend(xElements, xFirst);

				while ( xlParserMatch(pParser, XL_TK_COMMA) ) {
					xvalue xElem = xlParseExpr(pParser);
					xlAstArrayAppend(xElements, xElem);
				}

				xlParserExpect(pParser, XL_TK_RBRACKET, (str)"]");
				return xlAstCreateArrayLit(xElements, iLine, iCol);
			}
		}

		// 花括号字面量 {...} - 可能是字典或集合
		// {} = 空字典, {val, ...} = 集合, {key:val, ...} = 字典
		case XL_TK_LBRACE: {
			xlParserAdvance(pParser);
			xvalue xEntries = xlAstCreateArray();

			// 空花括号 = 空字典
			if ( xlParserCheck(pParser, XL_TK_RBRACE) ) {
				xlParserAdvance(pParser);
				return xlAstCreateDictLit(xEntries, iLine, iCol);
			}

			// 解析第一个元素，判断是集合还是字典
			// 字典键可以是标识符或字符串
			bool bIsDict = false;
			xvalue xFirstKey = NULL;

			if ( xlParserCheck(pParser, XL_TK_IDENT) ) {
				// 保存标识符信息
				str pszKey = pParser->token.pszStart;
				u32 iKeyLen = pParser->token.iLength;
				u32 iKeyLine = pParser->token.iLine;
				u32 iKeyCol = pParser->token.iColumn;
				xlParserAdvance(pParser);

				if ( xlParserMatch(pParser, XL_TK_COLON) || xlParserMatch(pParser, XL_TK_ASSIGN) ) {
					// 字典字面量 {key:val, ...}
					bIsDict = true;
					xFirstKey = xlAstCreateStringLit(pszKey, iKeyLen, iKeyLine, iKeyCol);
				} else {
					// 集合字面量，第一个元素是标识符表达式
					xFirstKey = xlAstCreateIdent(pszKey, iKeyLen, iKeyLine, iKeyCol);
				}
			} else if ( xlParserCheck(pParser, XL_TK_STRING) ) {
				// 字符串键
				str pszKey = pParser->token.sValue.pszText;
				u32 iKeyLen = pParser->token.sValue.iLen;
				u32 iKeyLine = pParser->token.iLine;
				u32 iKeyCol = pParser->token.iColumn;
				xlParserAdvance(pParser);

				if ( xlParserMatch(pParser, XL_TK_COLON) || xlParserMatch(pParser, XL_TK_ASSIGN) ) {
					// 字典字面量
					bIsDict = true;
					xFirstKey = xlAstCreateStringLit(pszKey, iKeyLen, iKeyLine, iKeyCol);
					XL_SAFE_FREE(pszKey);
				} else {
					// 集合中的字符串值
					xFirstKey = xlAstCreateStringLit(pszKey, iKeyLen, iKeyLine, iKeyCol);
					XL_SAFE_FREE(pszKey);
				}
			} else {
				// 其他表达式（集合元素）
				xFirstKey = xlParseExpr(pParser);
			}

			if ( bIsDict ) {
				// 字典字面量
				xvalue xFirstVal = xlParseExpr(pParser);
				xvalue xEntry = xlAstCreate((str)"DictEntry", iLine, iCol);
				xvoTableSetValue(xEntry, (str)"key", 0, xFirstKey, FALSE);
				xvoTableSetValue(xEntry, (str)"value", 0, xFirstVal, FALSE);
				xlAstArrayAppend(xEntries, xEntry);

				while ( xlParserMatch(pParser, XL_TK_COMMA) ) {
					xvalue xKey = NULL;
					if ( xlParserCheck(pParser, XL_TK_IDENT) ) {
						xKey = xlAstCreateStringLit(pParser->token.pszStart,
						                            pParser->token.iLength,
						                            pParser->token.iLine,
						                            pParser->token.iColumn);
						xlParserAdvance(pParser);
					} else if ( xlParserCheck(pParser, XL_TK_STRING) ) {
						xKey = xlAstCreateStringLit(pParser->token.sValue.pszText,
						                            pParser->token.sValue.iLen,
						                            pParser->token.iLine,
						                            pParser->token.iColumn);
						XL_SAFE_FREE(pParser->token.sValue.pszText);
						xlParserAdvance(pParser);
					} else {
						xlErrorListAddFmt(pParser->pErrors, XL_ERR_SYN_EXPECTED_TOKEN,
						                  pParser->token.iLine, pParser->token.iColumn,
						                  (str)"字典键必须是标识符或字符串");
						pParser->bHadError = true;
						break;
					}

					if ( !xlParserMatch(pParser, XL_TK_COLON) &&
					     !xlParserMatch(pParser, XL_TK_ASSIGN) ) {
						xlErrorListAddFmt(pParser->pErrors, XL_ERR_SYN_EXPECTED_TOKEN,
						                  pParser->token.iLine, pParser->token.iColumn,
						                  (str)"期望 ':' 或 '='");
						pParser->bHadError = true;
						break;
					}

					xvalue xVal = xlParseExpr(pParser);
					xEntry = xlAstCreate((str)"DictEntry", pParser->token.iLine, pParser->token.iColumn);
					xvoTableSetValue(xEntry, (str)"key", 0, xKey, FALSE);
					xvoTableSetValue(xEntry, (str)"value", 0, xVal, FALSE);
					xlAstArrayAppend(xEntries, xEntry);
				}

				xlParserExpect(pParser, XL_TK_RBRACE, (str)"}");
				return xlAstCreateDictLit(xEntries, iLine, iCol);
			} else {
				// 集合字面量
				xlAstArrayAppend(xEntries, xFirstKey);

				while ( xlParserMatch(pParser, XL_TK_COMMA) ) {
					xvalue xElem = xlParseExpr(pParser);
					xlAstArrayAppend(xEntries, xElem);
				}

				xlParserExpect(pParser, XL_TK_RBRACE, (str)"}");
				return xlAstCreateSetLit(xEntries, iLine, iCol);
			}
		}

		// new 表达式
		case XL_TK_NEW: {
			xlParserAdvance(pParser);
			xvalue xType = xlParseTypeRef(pParser);

			xvalue xArgs = xlAstCreateArray();
			if ( xlParserMatch(pParser, XL_TK_LPAREN) ) {
				if ( !xlParserCheck(pParser, XL_TK_RPAREN) ) {
					do {
						xvalue xArg = xlParseExpr(pParser);
						xlAstArrayAppend(xArgs, xArg);
					} while ( xlParserMatch(pParser, XL_TK_COMMA) );
				}
				xlParserExpect(pParser, XL_TK_RPAREN, (str)")");
			}

			return xlAstCreateNewExpr(xType, xArgs, iLine, iCol);
		}

		// Lambda 表达式 function(...) [as type] ... end
		case XL_TK_FUNCTION: {
			xlParserAdvance(pParser);
			
			// Lambda 表达式：function 后直接跟 (
			// 解析参数列表
			xvalue xParams = xlAstCreateArray();
			xlParserExpect(pParser, XL_TK_LPAREN, (str)"(");

			if ( !xlParserCheck(pParser, XL_TK_RPAREN) ) {
				do {
					u32 iParamLine = pParser->token.iLine;
					u32 iParamCol = pParser->token.iColumn;

					xvalue xType = NULL;
					str pszName = NULL;
					u32 iNameLen = 0;

					// 检查是否有类型注解
					bool bHasType = false;
					switch ( pParser->token.kind ) {
						case XL_TK_INT_T:
						case XL_TK_INT8:
						case XL_TK_INT16:
						case XL_TK_INT32:
						case XL_TK_INT64:
						case XL_TK_UINT8:
						case XL_TK_UINT16:
						case XL_TK_UINT32:
						case XL_TK_UINT64:
						case XL_TK_FLOAT_T:
						case XL_TK_FLOAT32:
						case XL_TK_FLOAT64:
						case XL_TK_STRING_T:
						case XL_TK_BOOL:
						case XL_TK_VAR:
						case XL_TK_AUTO:
						case XL_TK_VOID:
						case XL_TK_ARRAY:
						case XL_TK_LIST:
						case XL_TK_SET:
						case XL_TK_DICT:
							bHasType = true;
							break;
						default:
							break;
					}

					if ( bHasType ) {
						xType = xlParseTypeRef(pParser);
					}

					// 参数名
					if ( !xlParserCheck(pParser, XL_TK_IDENT) ) {
						xlErrorListAddFmt(pParser->pErrors, XL_ERR_SYN_EXPECTED_TOKEN,
						                  pParser->token.iLine, pParser->token.iColumn,
						                  (str)"期望参数名");
						pParser->bHadError = true;
						break;
					}

					pszName = pParser->token.pszStart;
					iNameLen = pParser->token.iLength;
					xlParserAdvance(pParser);

					// 默认值（可选）
					xvalue xDefault = NULL;
					if ( xlParserMatch(pParser, XL_TK_ASSIGN) ) {
						xDefault = xlParseExpr(pParser);
					}

					xvalue xParam = xlAstCreateParamDecl(pszName, iNameLen, xType, xDefault, false, iParamLine, iParamCol);
					xlAstArrayAppend(xParams, xParam);

				} while ( xlParserMatch(pParser, XL_TK_COMMA) );
			}

			xlParserExpect(pParser, XL_TK_RPAREN, (str)")");

			// 返回类型
			xvalue xReturns = xlAstCreateArray();
			if ( xlParserMatch(pParser, XL_TK_AS) ) {
				xvalue xType = xlParseTypeRef(pParser);
				if ( xType ) {
					xlAstArrayAppend(xReturns, xType);
				}
			}

			// 函数体
			xvalue xBody = xlParseBlockStmt(pParser);

			xlParserExpect(pParser, XL_TK_END, (str)"end");

			return xlAstCreateLambdaExpr(xParams, xReturns, xBody, iLine, iCol);
		}

		// 类型关键字作为值（类型值）或类型转换函数
		// 例如: var t = int  或  int("123")
		case XL_TK_TYPE:
		case XL_TK_INT_T:
		case XL_TK_INT8:
		case XL_TK_INT16:
		case XL_TK_INT32:
		case XL_TK_INT64:
		case XL_TK_UINT8:
		case XL_TK_UINT16:
		case XL_TK_UINT32:
		case XL_TK_UINT64:
		case XL_TK_FLOAT_T:
		case XL_TK_FLOAT32:
		case XL_TK_FLOAT64:
		case XL_TK_STRING_T:
		case XL_TK_BOOL:
		case XL_TK_ARRAY:
		case XL_TK_LIST:
		case XL_TK_DICT:
		case XL_TK_TIME_T:
		case XL_TK_SET: {
			str pszName = pParser->token.pszStart;
			u32 iNameLen = pParser->token.iLength;
			xlParserAdvance(pParser);
			// 如果后面跟着括号，返回标识符用于函数调用（类型转换）
			// 否则返回类型引用（类型值）
			if ( xlParserCheck(pParser, XL_TK_LPAREN) ) {
				return xlAstCreateIdent(pszName, iNameLen, iLine, iCol);
			} else {
				return xlAstCreateTypeRef(pszName, iNameLen, false, false, iLine, iCol);
			}
		}

		default:
			xlErrorListAddFmt(pParser->pErrors, XL_ERR_SYN_INVALID_EXPR,
			                  pParser->token.iLine, pParser->token.iColumn,
			                  (str)"意外的 Token: %s",
			                  (char*)xlTokenKindName(pParser->token.kind));
			pParser->bHadError = true;
			return 0;
	}
}


// 解析后缀表达式（调用、索引、成员访问）
static xvalue xlParsePostfixExpr( XL_Parser *pParser )
{
	xvalue xExpr = xlParsePrimaryExpr(pParser);
	if ( !xExpr ) return 0;

	while ( true ) {
		u32 iLine = pParser->token.iLine;
		u32 iCol = pParser->token.iColumn;

		// 函数调用 expr(...)
		if ( xlParserMatch(pParser, XL_TK_LPAREN) ) {
			xvalue xArgs = xlAstCreateArray();

			if ( !xlParserCheck(pParser, XL_TK_RPAREN) ) {
				do {
					xvalue xArg = xlParseExpr(pParser);
					xlAstArrayAppend(xArgs, xArg);
				} while ( xlParserMatch(pParser, XL_TK_COMMA) );
			}

			xlParserExpect(pParser, XL_TK_RPAREN, (str)")");
			xExpr = xlAstCreateCallExpr(xExpr, xArgs, iLine, iCol);
			continue;
		}

		// 索引 expr[...] 或切片 expr[a to b]
		if ( xlParserMatch(pParser, XL_TK_LBRACKET) ) {
			xvalue xStart = xlParseExpr(pParser);
			// 检查是否是切片语法
			if ( xlParserMatch(pParser, XL_TK_TO) ) {
				xvalue xEnd = xlParseExpr(pParser);
				xlParserExpect(pParser, XL_TK_RBRACKET, (str)"]");
				xExpr = xlAstCreateSliceExpr(xExpr, xStart, xEnd, iLine, iCol);
			} else {
				xlParserExpect(pParser, XL_TK_RBRACKET, (str)"]");
				xExpr = xlAstCreateIndexExpr(xExpr, xStart, iLine, iCol);
			}
			continue;
		}

		// 成员访问 expr.member 或 expr?.member
		if ( xlParserMatch(pParser, XL_TK_DOT) ) {
			if ( !xlParserCheck(pParser, XL_TK_IDENT) ) {
				xlErrorListAddFmt(pParser->pErrors, XL_ERR_SYN_EXPECTED_TOKEN,
				                  pParser->token.iLine, pParser->token.iColumn,
				                  (str)"期望成员名");
				pParser->bHadError = true;
				return xExpr;
			}
			str pszMember = pParser->token.pszStart;
			u32 iMemberLen = pParser->token.iLength;
			xlParserAdvance(pParser);
			xExpr = xlAstCreateMemberExpr(xExpr, pszMember, iMemberLen, false, iLine, iCol);
			continue;
		}

		if ( xlParserMatch(pParser, XL_TK_SAFE_DOT) ) {
			if ( !xlParserCheck(pParser, XL_TK_IDENT) ) {
				xlErrorListAddFmt(pParser->pErrors, XL_ERR_SYN_EXPECTED_TOKEN,
				                  pParser->token.iLine, pParser->token.iColumn,
				                  (str)"期望成员名");
				pParser->bHadError = true;
				return xExpr;
			}
			str pszMember = pParser->token.pszStart;
			u32 iMemberLen = pParser->token.iLength;
			xlParserAdvance(pParser);
			xExpr = xlAstCreateMemberExpr(xExpr, pszMember, iMemberLen, true, iLine, iCol);
			continue;
		}

		// 自增自减 expr++ / expr--
		if ( xlParserMatch(pParser, XL_TK_INC) ) {
			xExpr = xlAstCreateUnaryExpr((str)"++", xExpr, false, iLine, iCol);
			continue;
		}
		if ( xlParserMatch(pParser, XL_TK_DEC) ) {
			xExpr = xlAstCreateUnaryExpr((str)"--", xExpr, false, iLine, iCol);
			continue;
		}

		break;
	}

	return xExpr;
}


// 解析一元表达式
static xvalue xlParseUnaryExpr( XL_Parser *pParser )
{
	u32 iLine = pParser->token.iLine;
	u32 iCol = pParser->token.iColumn;

	// 前缀 - / not / ! / ++ / --
	if ( xlParserMatch(pParser, XL_TK_MINUS) ) {
		xvalue xOperand = xlParseUnaryExpr(pParser);
		return xlAstCreateUnaryExpr((str)"-", xOperand, true, iLine, iCol);
	}

	if ( xlParserMatch(pParser, XL_TK_NOT) ) {
		xvalue xOperand = xlParseUnaryExpr(pParser);
		return xlAstCreateUnaryExpr((str)"not", xOperand, true, iLine, iCol);
	}

	if ( xlParserMatch(pParser, XL_TK_BANG) ) {
		xvalue xOperand = xlParseUnaryExpr(pParser);
		return xlAstCreateUnaryExpr((str)"!", xOperand, true, iLine, iCol);
	}

	if ( xlParserMatch(pParser, XL_TK_INC) ) {
		xvalue xOperand = xlParseUnaryExpr(pParser);
		return xlAstCreateUnaryExpr((str)"++", xOperand, true, iLine, iCol);
	}

	if ( xlParserMatch(pParser, XL_TK_DEC) ) {
		xvalue xOperand = xlParseUnaryExpr(pParser);
		return xlAstCreateUnaryExpr((str)"--", xOperand, true, iLine, iCol);
	}

	return xlParsePostfixExpr(pParser);
}


// 解析乘方表达式（右结合）
static xvalue xlParsePowerExpr( XL_Parser *pParser )
{
	xvalue xLeft = xlParseUnaryExpr(pParser);
	if ( !xLeft ) return 0;

	if ( xlParserMatch(pParser, XL_TK_POWER) ) {
		u32 iLine = pParser->prevToken.iLine;
		u32 iCol = pParser->prevToken.iColumn;
		xvalue xRight = xlParsePowerExpr(pParser);  // 右结合
		return xlAstCreateBinaryExpr((str)"**", xLeft, xRight, iLine, iCol);
	}

	return xLeft;
}


// 解析乘除表达式
static xvalue xlParseMulExpr( XL_Parser *pParser )
{
	xvalue xLeft = xlParsePowerExpr(pParser);
	if ( !xLeft ) return 0;

	while ( true ) {
		u32 iLine = pParser->token.iLine;
		u32 iCol = pParser->token.iColumn;
		str pszOp = NULL;

		if ( xlParserMatch(pParser, XL_TK_STAR) ) {
			pszOp = (str)"*";
		} else if ( xlParserMatch(pParser, XL_TK_SLASH) ) {
			pszOp = (str)"/";
		} else if ( xlParserMatch(pParser, XL_TK_PERCENT) ) {
			pszOp = (str)"%";
		} else {
			break;
		}

		xvalue xRight = xlParsePowerExpr(pParser);
		xLeft = xlAstCreateBinaryExpr(pszOp, xLeft, xRight, iLine, iCol);
	}

	return xLeft;
}


// 解析加减表达式
static xvalue xlParseAddExpr( XL_Parser *pParser )
{
	xvalue xLeft = xlParseMulExpr(pParser);
	if ( !xLeft ) return 0;

	while ( true ) {
		u32 iLine = pParser->token.iLine;
		u32 iCol = pParser->token.iColumn;
		str pszOp = NULL;

		if ( xlParserMatch(pParser, XL_TK_PLUS) ) {
			pszOp = (str)"+";
		} else if ( xlParserMatch(pParser, XL_TK_MINUS) ) {
			pszOp = (str)"-";
		} else {
			break;
		}

		xvalue xRight = xlParseMulExpr(pParser);
		xLeft = xlAstCreateBinaryExpr(pszOp, xLeft, xRight, iLine, iCol);
	}

	return xLeft;
}


// 解析位移表达式 (优先级6: << >>)
static xvalue xlParseShiftExpr( XL_Parser *pParser )
{
	xvalue xLeft = xlParseAddExpr(pParser);
	if ( !xLeft ) return 0;

	while ( true ) {
		u32 iLine = pParser->token.iLine;
		u32 iCol = pParser->token.iColumn;
		str pszOp = NULL;

		if ( xlParserMatch(pParser, XL_TK_SHL) ) {
			pszOp = (str)"<<";
		} else if ( xlParserMatch(pParser, XL_TK_SHR) ) {
			pszOp = (str)">>";
		} else {
			break;
		}

		xvalue xRight = xlParseAddExpr(pParser);
		xLeft = xlAstCreateBinaryExpr(pszOp, xLeft, xRight, iLine, iCol);
	}

	return xLeft;
}


// 解析比较表达式 (优先级7: < <= > >= in is)
static xvalue xlParseCompareExpr( XL_Parser *pParser )
{
	xvalue xLeft = xlParseShiftExpr(pParser);
	if ( !xLeft ) return 0;

	while ( true ) {
		u32 iLine = pParser->token.iLine;
		u32 iCol = pParser->token.iColumn;
		str pszOp = NULL;

		if ( xlParserMatch(pParser, XL_TK_LT) ) {
			pszOp = (str)"<";
		} else if ( xlParserMatch(pParser, XL_TK_LE) ) {
			pszOp = (str)"<=";
		} else if ( xlParserMatch(pParser, XL_TK_GT) ) {
			pszOp = (str)">";
		} else if ( xlParserMatch(pParser, XL_TK_GE) ) {
			pszOp = (str)">=";
		} else if ( xlParserMatch(pParser, XL_TK_IN) ) {
			pszOp = (str)"in";
		} else if ( xlParserMatch(pParser, XL_TK_IS) ) {
			pszOp = (str)"is";
		} else {
			break;
		}

		xvalue xRight = xlParseShiftExpr(pParser);
		xLeft = xlAstCreateBinaryExpr(pszOp, xLeft, xRight, iLine, iCol);
	}

	return xLeft;
}


// 解析相等表达式 (优先级8: == != ~=)
static xvalue xlParseEqualityExpr( XL_Parser *pParser )
{
	xvalue xLeft = xlParseCompareExpr(pParser);
	if ( !xLeft ) return 0;

	while ( true ) {
		u32 iLine = pParser->token.iLine;
		u32 iCol = pParser->token.iColumn;
		str pszOp = NULL;

		if ( xlParserMatch(pParser, XL_TK_EQ) ) {
			pszOp = (str)"==";
		} else if ( xlParserMatch(pParser, XL_TK_NE) ) {
			pszOp = (str)"!=";
		} else if ( xlParserMatch(pParser, XL_TK_APPROX) ) {
			pszOp = (str)"~=";
		} else {
			break;
		}

		xvalue xRight = xlParseCompareExpr(pParser);
		xLeft = xlAstCreateBinaryExpr(pszOp, xLeft, xRight, iLine, iCol);
	}

	return xLeft;
}


// 解析位与表达式 (优先级9: &)
static xvalue xlParseBitAndExpr( XL_Parser *pParser )
{
	xvalue xLeft = xlParseEqualityExpr(pParser);
	if ( !xLeft ) return 0;

	while ( xlParserMatch(pParser, XL_TK_AMP) ) {
		u32 iLine = pParser->prevToken.iLine;
		u32 iCol = pParser->prevToken.iColumn;
		xvalue xRight = xlParseEqualityExpr(pParser);
		xLeft = xlAstCreateBinaryExpr((str)"&", xLeft, xRight, iLine, iCol);
	}

	return xLeft;
}


// 解析位异或表达式 (优先级10: ^)
static xvalue xlParseBitXorExpr( XL_Parser *pParser )
{
	xvalue xLeft = xlParseBitAndExpr(pParser);
	if ( !xLeft ) return 0;

	while ( xlParserMatch(pParser, XL_TK_CARET) ) {
		u32 iLine = pParser->prevToken.iLine;
		u32 iCol = pParser->prevToken.iColumn;
		xvalue xRight = xlParseBitAndExpr(pParser);
		xLeft = xlAstCreateBinaryExpr((str)"^", xLeft, xRight, iLine, iCol);
	}

	return xLeft;
}


// 解析位或表达式 (优先级11: |)
static xvalue xlParseBitOrExpr( XL_Parser *pParser )
{
	xvalue xLeft = xlParseBitXorExpr(pParser);
	if ( !xLeft ) return 0;

	while ( xlParserMatch(pParser, XL_TK_PIPE) ) {
		u32 iLine = pParser->prevToken.iLine;
		u32 iCol = pParser->prevToken.iColumn;
		xvalue xRight = xlParseBitXorExpr(pParser);
		xLeft = xlAstCreateBinaryExpr((str)"|", xLeft, xRight, iLine, iCol);
	}

	return xLeft;
}


// 解析逻辑与表达式 (优先级12: and)
static xvalue xlParseAndExpr( XL_Parser *pParser )
{
	xvalue xLeft = xlParseBitOrExpr(pParser);
	if ( !xLeft ) return 0;

	while ( xlParserMatch(pParser, XL_TK_AND) ) {
		u32 iLine = pParser->prevToken.iLine;
		u32 iCol = pParser->prevToken.iColumn;
		xvalue xRight = xlParseBitOrExpr(pParser);
		xLeft = xlAstCreateBinaryExpr((str)"and", xLeft, xRight, iLine, iCol);
	}

	return xLeft;
}


// 解析逻辑或表达式
static xvalue xlParseOrExpr( XL_Parser *pParser )
{
	xvalue xLeft = xlParseAndExpr(pParser);
	if ( !xLeft ) return 0;

	while ( xlParserMatch(pParser, XL_TK_OR) ) {
		u32 iLine = pParser->prevToken.iLine;
		u32 iCol = pParser->prevToken.iColumn;
		xvalue xRight = xlParseAndExpr(pParser);
		xLeft = xlAstCreateBinaryExpr((str)"or", xLeft, xRight, iLine, iCol);
	}

	return xLeft;
}


// 解析空值合并表达式
static xvalue xlParseCoalesceExpr( XL_Parser *pParser )
{
	xvalue xLeft = xlParseOrExpr(pParser);
	if ( !xLeft ) return 0;

	while ( xlParserMatch(pParser, XL_TK_NULL_COALESCE) ) {
		u32 iLine = pParser->prevToken.iLine;
		u32 iCol = pParser->prevToken.iColumn;
		xvalue xRight = xlParseOrExpr(pParser);
		xLeft = xlAstCreateBinaryExpr((str)"??", xLeft, xRight, iLine, iCol);
	}

	return xLeft;
}


// 解析三元表达式
static xvalue xlParseTernaryExpr( XL_Parser *pParser )
{
	xvalue xCond = xlParseCoalesceExpr(pParser);
	if ( !xCond ) return 0;

	if ( xlParserMatch(pParser, XL_TK_QUESTION) ) {
		u32 iLine = pParser->prevToken.iLine;
		u32 iCol = pParser->prevToken.iColumn;
		xvalue xThen = xlParseExpr(pParser);
		xlParserExpect(pParser, XL_TK_COLON, (str)":");
		xvalue xElse = xlParseTernaryExpr(pParser);
		return xlAstCreateTernaryExpr(xCond, xThen, xElse, iLine, iCol);
	}

	return xCond;
}


// 解析赋值表达式
static xvalue xlParseAssignExpr( XL_Parser *pParser )
{
	xvalue xLeft = xlParseTernaryExpr(pParser);
	if ( !xLeft ) return 0;

	u32 iLine = pParser->token.iLine;
	u32 iCol = pParser->token.iColumn;
	str pszOp = NULL;

	if ( xlParserMatch(pParser, XL_TK_ASSIGN) ) {
		pszOp = (str)"=";
	} else if ( xlParserMatch(pParser, XL_TK_PLUS_ASSIGN) ) {
		pszOp = (str)"+=";
	} else if ( xlParserMatch(pParser, XL_TK_MINUS_ASSIGN) ) {
		pszOp = (str)"-=";
	} else if ( xlParserMatch(pParser, XL_TK_STAR_ASSIGN) ) {
		pszOp = (str)"*=";
	} else if ( xlParserMatch(pParser, XL_TK_SLASH_ASSIGN) ) {
		pszOp = (str)"/=";
	} else if ( xlParserMatch(pParser, XL_TK_PERCENT_ASSIGN) ) {
		pszOp = (str)"%=";
	} else if ( xlParserMatch(pParser, XL_TK_POWER_ASSIGN) ) {
		pszOp = (str)"**=";
	} else if ( xlParserMatch(pParser, XL_TK_NULL_ASSIGN) ) {
		pszOp = (str)"?=";
	}

	if ( pszOp ) {
		xvalue xRight = xlParseAssignExpr(pParser);  // 右结合
		return xlAstCreateAssignExpr(pszOp, xLeft, xRight, iLine, iCol);
	}

	return xLeft;
}


// 解析表达式（入口）
static xvalue xlParseExpr( XL_Parser *pParser )
{
	return xlParseAssignExpr(pParser);
}




// ============================================================================
// 语句解析
// ============================================================================

// 解析变量声明
// var x = 1  或  int x = 1  或  type x
static xvalue xlParseVarDecl( XL_Parser *pParser, bool bIsConst )
{
	u32 iLine = pParser->token.iLine;
	u32 iCol = pParser->token.iColumn;

	xvalue xType = NULL;

	// const 后面直接跟变量名，不需要 var/auto
	if ( bIsConst ) {
		// const name = value 形式，不需要类型关键字
	} else if ( xlParserMatch(pParser, XL_TK_VAR) || xlParserMatch(pParser, XL_TK_AUTO) ) {
		// 动态类型
	} else {
		// 静态类型
		xType = xlParseTypeRef(pParser);
		if ( !xType ) {
			xlErrorListAddFmt(pParser->pErrors, XL_ERR_SYN_INVALID_TYPE,
			                  pParser->token.iLine, pParser->token.iColumn,
			                  (str)"期望类型名");
			pParser->bHadError = true;
			return 0;
		}
	}

	// 变量名
	if ( !xlParserCheck(pParser, XL_TK_IDENT) ) {
		xlErrorListAddFmt(pParser->pErrors, XL_ERR_SYN_EXPECTED_TOKEN,
		                  pParser->token.iLine, pParser->token.iColumn,
		                  (str)"期望变量名");
		pParser->bHadError = true;
		return 0;
	}

	str pszName = pParser->token.pszStart;
	u32 iNameLen = pParser->token.iLength;
	xlParserAdvance(pParser);

	// 初始值（可选）
	xvalue xInit = NULL;
	if ( xlParserMatch(pParser, XL_TK_ASSIGN) ) {
		xInit = xlParseExpr(pParser);
	}

	return xlAstCreateVarDecl(pszName, iNameLen, xType, xInit, bIsConst, iLine, iCol);
}


// 解析 if 语句
static xvalue xlParseIfStmt( XL_Parser *pParser )
{
	u32 iLine = pParser->token.iLine;
	u32 iCol = pParser->token.iColumn;

	xlParserExpect(pParser, XL_TK_IF, (str)"if");

	// 条件
	xvalue xCond = xlParseExpr(pParser);

	// then（可选关键字）- 检查是否是单行模式
	bool bSingleLine = false;
	u32 iThenLine = 0;
	if ( xlParserCheck(pParser, XL_TK_THEN) ) {
		iThenLine = pParser->token.iLine;
		xlParserAdvance(pParser);  // 消耗 then
		// 如果 then 后的 token 在同一行，说明是单行 if-then
		if ( pParser->token.iLine == iThenLine ) {
			bSingleLine = true;
		}
	}

	xvalue xThen;
	xvalue xElseIfs = xlAstCreateArray();
	xvalue xElse = NULL;

	if ( bSingleLine ) {
		// 单行模式：解析单条语句，不需要 end
		xThen = xlAstCreateBlockStmt(pParser->token.iLine, pParser->token.iColumn);
		xvalue xStmt = xlParseStmt(pParser);
		if ( xStmt ) {
			xlAstBlockAddStmt(xThen, xStmt);
		}
		// 单行模式不支持 elseif/else，直接返回
	} else {
		// 多行模式：解析 then 块
		xThen = xlParseBlockStmt(pParser);

		// elseif 列表
		while ( xlParserMatch(pParser, XL_TK_ELSEIF) ) {
			xvalue xElseIfCond = xlParseExpr(pParser);
			xlParserMatch(pParser, XL_TK_THEN);
			xvalue xElseIfBody = xlParseBlockStmt(pParser);

			xvalue xElseIf = xlAstCreate((str)"ElseIf", pParser->token.iLine, pParser->token.iColumn);
			xvoTableSetValue(xElseIf, (str)"cond", 0, xElseIfCond, FALSE);
			xvoTableSetValue(xElseIf, (str)"body", 0, xElseIfBody, FALSE);
			xlAstArrayAppend(xElseIfs, xElseIf);
		}

		// else 块（可选）
		if ( xlParserMatch(pParser, XL_TK_ELSE) ) {
			xElse = xlParseBlockStmt(pParser);
		}

		// end
		xlParserExpect(pParser, XL_TK_END, (str)"end");
	}

	return xlAstCreateIfStmt(xCond, xThen, xElseIfs, xElse, iLine, iCol);
}


// 解析 while 语句
static xvalue xlParseWhileStmt( XL_Parser *pParser )
{
	u32 iLine = pParser->token.iLine;
	u32 iCol = pParser->token.iColumn;

	xlParserExpect(pParser, XL_TK_WHILE, (str)"while");

	xvalue xCond = xlParseExpr(pParser);

	xvalue xBody = xlParseBlockStmt(pParser);

	xlParserExpect(pParser, XL_TK_END, (str)"end");

	return xlAstCreateWhileStmt(xCond, xBody, iLine, iCol);
}


// 解析 do-while 语句
static xvalue xlParseDoWhileStmt( XL_Parser *pParser )
{
	u32 iLine = pParser->token.iLine;
	u32 iCol = pParser->token.iColumn;

	xlParserExpect(pParser, XL_TK_DO, (str)"do");

	xvalue xBody = xlParseBlockStmt(pParser);

	xlParserExpect(pParser, XL_TK_WHILE, (str)"while");
	xvalue xCond = xlParseExpr(pParser);

	return xlAstCreateDoWhileStmt(xBody, xCond, iLine, iCol);
}


// 解析 for 语句
static xvalue xlParseForStmt( XL_Parser *pParser )
{
	u32 iLine = pParser->token.iLine;
	u32 iCol = pParser->token.iColumn;

	xlParserExpect(pParser, XL_TK_FOR, (str)"for");

	// 变量名
	if ( !xlParserCheck(pParser, XL_TK_IDENT) ) {
		xlErrorListAddFmt(pParser->pErrors, XL_ERR_SYN_EXPECTED_TOKEN,
		                  pParser->token.iLine, pParser->token.iColumn,
		                  (str)"期望变量名");
		pParser->bHadError = true;
		return 0;
	}

	str pszVar = pParser->token.pszStart;
	u32 iVarLen = pParser->token.iLength;
	xlParserAdvance(pParser);

	// for i = start to end 或 for v in iter
	if ( xlParserMatch(pParser, XL_TK_COMMA) ) {
		// for k, v in iter
		if ( !xlParserCheck(pParser, XL_TK_IDENT) ) {
			xlErrorListAddFmt(pParser->pErrors, XL_ERR_SYN_EXPECTED_TOKEN,
			                  pParser->token.iLine, pParser->token.iColumn,
			                  (str)"期望变量名");
			pParser->bHadError = true;
			return 0;
		}

		str pszValue = pParser->token.pszStart;
		u32 iValueLen = pParser->token.iLength;
		xlParserAdvance(pParser);

		xlParserExpect(pParser, XL_TK_IN, (str)"in");
		xvalue xIter = xlParseExpr(pParser);
		xvalue xBody = xlParseBlockStmt(pParser);
		xlParserExpect(pParser, XL_TK_END, (str)"end");

		return xlAstCreateForInStmt(pszVar, iVarLen, pszValue, iValueLen, xIter, xBody, iLine, iCol);
	} else if ( xlParserMatch(pParser, XL_TK_IN) ) {
		// for v in iter
		// range() 作为内置函数处理，不需要特殊解析
		xvalue xIter = xlParseExpr(pParser);
		xvalue xBody = xlParseBlockStmt(pParser);
		xlParserExpect(pParser, XL_TK_END, (str)"end");

		return xlAstCreateForInStmt(NULL, 0, pszVar, iVarLen, xIter, xBody, iLine, iCol);
	}

	xlErrorListAddFmt(pParser->pErrors, XL_ERR_SYN_INVALID_STMT,
	                  pParser->token.iLine, pParser->token.iColumn,
	                  (str)"无效的 for 语句");
	pParser->bHadError = true;
	return 0;
}


// 解析 return 语句
static xvalue xlParseReturnStmt( XL_Parser *pParser )
{
	u32 iLine = pParser->token.iLine;
	u32 iCol = pParser->token.iColumn;

	xlParserExpect(pParser, XL_TK_RETURN, (str)"return");

	xvalue xValues = xlAstCreateArray();

	// 检查是否有返回值
	if ( !xlParserCheck(pParser, XL_TK_END) &&
	     !xlParserCheck(pParser, XL_TK_ELSE) &&
	     !xlParserCheck(pParser, XL_TK_ELSEIF) &&
	     !xlParserCheck(pParser, XL_TK_EOF) ) {
		do {
			xvalue xVal = xlParseExpr(pParser);
			xlAstArrayAppend(xValues, xVal);
		} while ( xlParserMatch(pParser, XL_TK_COMMA) );
	}

	return xlAstCreateReturnStmt(xValues, iLine, iCol);
}


// 解析 try 语句
static xvalue xlParseTryStmt( XL_Parser *pParser )
{
	u32 iLine = pParser->token.iLine;
	u32 iCol = pParser->token.iColumn;

	xlParserExpect(pParser, XL_TK_TRY, (str)"try");

	xvalue xTry = xlParseBlockStmt(pParser);

	// catch 块
	xvalue xCatch = NULL;
	str pszCatchVar = NULL;
	u32 iCatchVarLen = 0;

	if ( xlParserMatch(pParser, XL_TK_CATCH) ) {
		if ( xlParserCheck(pParser, XL_TK_IDENT) ) {
			pszCatchVar = pParser->token.pszStart;
			iCatchVarLen = pParser->token.iLength;
			xlParserAdvance(pParser);
		}
		xCatch = xlParseBlockStmt(pParser);
	}

	// finally 块
	xvalue xFinally = NULL;
	if ( xlParserMatch(pParser, XL_TK_FINALLY) ) {
		xFinally = xlParseBlockStmt(pParser);
	}

	xlParserExpect(pParser, XL_TK_END, (str)"end");

	return xlAstCreateTryStmt(xTry, xCatch, pszCatchVar, iCatchVarLen, xFinally, iLine, iCol);
}


// 解析块语句（直到遇到 end/else/elseif/catch/finally）
static xvalue xlParseBlockStmt( XL_Parser *pParser )
{
	u32 iLine = pParser->token.iLine;
	u32 iCol = pParser->token.iColumn;

	xvalue xBlock = xlAstCreateBlockStmt(iLine, iCol);

	while ( !xlParserCheck(pParser, XL_TK_END) &&
	        !xlParserCheck(pParser, XL_TK_ELSE) &&
	        !xlParserCheck(pParser, XL_TK_ELSEIF) &&
	        !xlParserCheck(pParser, XL_TK_CATCH) &&
	        !xlParserCheck(pParser, XL_TK_FINALLY) &&
	        !xlParserCheck(pParser, XL_TK_EOF) ) {
		xvalue xStmt = xlParseStmt(pParser);
		if ( xStmt ) {
			xlAstBlockAddStmt(xBlock, xStmt);
		}

		if ( pParser->bPanicMode ) {
			xlParserSync(pParser);
		}
	}

	return xBlock;
}


// 解析语句
static xvalue xlParseStmt( XL_Parser *pParser )
{
	switch ( pParser->token.kind ) {
		case XL_TK_VAR:
		case XL_TK_AUTO:
			return xlParseVarDecl(pParser, false);

		case XL_TK_CONST:
			xlParserAdvance(pParser);
			return xlParseVarDecl(pParser, true);

		case XL_TK_IF:
			return xlParseIfStmt(pParser);

		case XL_TK_WHILE:
			return xlParseWhileStmt(pParser);

		case XL_TK_DO:
			return xlParseDoWhileStmt(pParser);

		case XL_TK_FOR:
			return xlParseForStmt(pParser);

		case XL_TK_RETURN:
			return xlParseReturnStmt(pParser);

		case XL_TK_BREAK: {
			u32 iLine = pParser->token.iLine;
			u32 iCol = pParser->token.iColumn;
			xlParserAdvance(pParser);
			// 解析可选的层级数 n
			i32 iLevel = 1;
			if ( pParser->token.kind == XL_TK_INT ) {
				iLevel = (i32)pParser->token.iValue;
				if ( iLevel < 1 ) {
					xlErrorListAddFmt(pParser->pErrors, XL_ERR_SYN_INVALID_EXPR,
					                  iLine, iCol, (str)"break level must be >= 1");
				}
				xlParserAdvance(pParser);
			}
			return xlAstCreateBreakStmt(iLevel, iLine, iCol);
		}

		case XL_TK_CONTINUE: {
			u32 iLine = pParser->token.iLine;
			u32 iCol = pParser->token.iColumn;
			xlParserAdvance(pParser);
			// 解析可选的层级数 n
			i32 iLevel = 1;
			if ( pParser->token.kind == XL_TK_INT ) {
				iLevel = (i32)pParser->token.iValue;
				if ( iLevel < 1 ) {
					xlErrorListAddFmt(pParser->pErrors, XL_ERR_SYN_INVALID_EXPR,
					                  iLine, iCol, (str)"continue level must be >= 1");
				}
				xlParserAdvance(pParser);
			}
			return xlAstCreateContinueStmt(iLevel, iLine, iCol);
		}

		case XL_TK_TRY:
			return xlParseTryStmt(pParser);

		case XL_TK_THROW: {
			u32 iLine = pParser->token.iLine;
			u32 iCol = pParser->token.iColumn;
			xlParserAdvance(pParser);
			xvalue xExpr = xlParseExpr(pParser);
			return xlAstCreateThrowStmt(xExpr, iLine, iCol);
		}

		// 类型名开头的变量声明
		case XL_TK_INT_T:
		case XL_TK_INT8:
		case XL_TK_INT16:
		case XL_TK_INT32:
		case XL_TK_INT64:
		case XL_TK_UINT8:
		case XL_TK_UINT16:
		case XL_TK_UINT32:
		case XL_TK_UINT64:
		case XL_TK_FLOAT_T:
		case XL_TK_FLOAT32:
		case XL_TK_FLOAT64:
		case XL_TK_STRING_T:
		case XL_TK_BOOL:
			return xlParseVarDecl(pParser, false);

		default:
			break;
	}

	// 表达式语句
	u32 iLine = pParser->token.iLine;
	u32 iCol = pParser->token.iColumn;
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

	xlParserExpect(pParser, XL_TK_LPAREN, (str)"(");

	if ( !xlParserCheck(pParser, XL_TK_RPAREN) ) {
		do {
			u32 iLine = pParser->token.iLine;
			u32 iCol = pParser->token.iColumn;

			xvalue xType = NULL;
			str pszName = NULL;
			u32 iNameLen = 0;

			// 检查是否有类型注解
			// 类型关键字后面跟参数名，或者两个连续的 IDENT（第一个是类型，第二个是名字）
			bool bHasType = false;

			// 检查是否为明确的类型关键字
			switch ( pParser->token.kind ) {
				case XL_TK_INT_T:
				case XL_TK_INT8:
				case XL_TK_INT16:
				case XL_TK_INT32:
				case XL_TK_INT64:
				case XL_TK_UINT8:
				case XL_TK_UINT16:
				case XL_TK_UINT32:
				case XL_TK_UINT64:
				case XL_TK_FLOAT_T:
				case XL_TK_FLOAT32:
				case XL_TK_FLOAT64:
				case XL_TK_STRING_T:
				case XL_TK_BOOL:
				case XL_TK_VAR:
				case XL_TK_AUTO:
				case XL_TK_VOID:
				case XL_TK_ARRAY:
				case XL_TK_LIST:
				case XL_TK_SET:
				case XL_TK_DICT:
					bHasType = true;
					break;
				default:
					break;
			}

			if ( bHasType ) {
				// 解析类型
				xType = xlParseTypeRef(pParser);
			}

			// 参数名
			if ( !xlParserCheck(pParser, XL_TK_IDENT) ) {
				xlErrorListAddFmt(pParser->pErrors, XL_ERR_SYN_EXPECTED_TOKEN,
				                  pParser->token.iLine, pParser->token.iColumn,
				                  (str)"期望参数名");
				pParser->bHadError = true;
				break;
			}

			pszName = pParser->token.pszStart;
			iNameLen = pParser->token.iLength;
			xlParserAdvance(pParser);

			// 默认值（可选）
			xvalue xDefault = NULL;
			if ( xlParserMatch(pParser, XL_TK_ASSIGN) ) {
				xDefault = xlParseExpr(pParser);
			}

			xvalue xParam = xlAstCreateParamDecl(pszName, iNameLen, xType, xDefault, false, iLine, iCol);
			xlAstArrayAppend(xParams, xParam);

		} while ( xlParserMatch(pParser, XL_TK_COMMA) );
	}

	xlParserExpect(pParser, XL_TK_RPAREN, (str)")");

	return xParams;
}


// 解析返回类型列表
static xvalue xlParseReturnTypes( XL_Parser *pParser )
{
	xvalue xReturns = xlAstCreateArray();

	if ( xlParserMatch(pParser, XL_TK_AS) ) {
		// 单返回类型
		xvalue xType = xlParseTypeRef(pParser);
		if ( xType ) {
			xlAstArrayAppend(xReturns, xType);
		}
	}

	return xReturns;
}


// 解析函数声明
static xvalue xlParseFuncDecl( XL_Parser *pParser )
{
	u32 iLine = pParser->token.iLine;
	u32 iCol = pParser->token.iColumn;

	xlParserExpect(pParser, XL_TK_FUNCTION, (str)"function");

	// 函数名
	if ( !xlParserCheck(pParser, XL_TK_IDENT) ) {
		xlErrorListAddFmt(pParser->pErrors, XL_ERR_SYN_EXPECTED_TOKEN,
		                  pParser->token.iLine, pParser->token.iColumn,
		                  (str)"期望函数名");
		pParser->bHadError = true;
		return 0;
	}

	str pszName = pParser->token.pszStart;
	u32 iNameLen = pParser->token.iLength;
	xlParserAdvance(pParser);

	// 参数列表
	xvalue xParams = xlParseParamList(pParser);

	// 返回类型
	xvalue xReturns = xlParseReturnTypes(pParser);

	// 函数体
	xvalue xBody = xlParseBlockStmt(pParser);

	xlParserExpect(pParser, XL_TK_END, (str)"end");

	return xlAstCreateFuncDecl(pszName, iNameLen, xParams, xReturns, xBody, false, iLine, iCol);
}


// 解析类声明
static xvalue xlParseClassDecl( XL_Parser *pParser )
{
	u32 iLine = pParser->token.iLine;
	u32 iCol = pParser->token.iColumn;

	xlParserExpect(pParser, XL_TK_CLASS, (str)"class");

	// 类名
	if ( !xlParserCheck(pParser, XL_TK_IDENT) ) {
		xlErrorListAddFmt(pParser->pErrors, XL_ERR_SYN_EXPECTED_TOKEN,
		                  pParser->token.iLine, pParser->token.iColumn,
		                  (str)"期望类名");
		pParser->bHadError = true;
		return 0;
	}

	str pszName = pParser->token.pszStart;
	u32 iNameLen = pParser->token.iLength;
	xlParserAdvance(pParser);

	// 父类（可选）
	xvalue xParent = NULL;
	if ( xlParserMatch(pParser, XL_TK_EXTENDS) || xlParserMatch(pParser, XL_TK_COLON) ) {
		if ( xlParserCheck(pParser, XL_TK_IDENT) ) {
			xParent = xlAstCreateIdent(pParser->token.pszStart,
			                           pParser->token.iLength,
			                           pParser->token.iLine,
			                           pParser->token.iColumn);
			xlParserAdvance(pParser);
		}
	}

	// 成员列表
	xvalue xMembers = xlAstCreateArray();

	while ( !xlParserCheck(pParser, XL_TK_END) && !xlParserCheck(pParser, XL_TK_EOF) ) {
		xvalue xMember = NULL;

		// 检查访问修饰符
		int iAccess = 0; // 0=public(默认), 1=private, 2=protected
		if ( xlParserMatch(pParser, XL_TK_PUBLIC) ) {
			iAccess = 0;
		} else if ( xlParserMatch(pParser, XL_TK_PRIVATE) ) {
			iAccess = 1;
		} else if ( xlParserMatch(pParser, XL_TK_PROTECTED) ) {
			iAccess = 2;
		}

		// 检查 static 修饰符
		bool bStatic = xlParserMatch(pParser, XL_TK_STATIC);

		// 构造函数 create(params)
		if ( xlParserCheck(pParser, XL_TK_CREATE) ) {
			u32 iCtorLine = pParser->token.iLine;
			u32 iCtorCol = pParser->token.iColumn;
			xlParserAdvance(pParser);

			// 参数列表
			xvalue xParams = xlParseParamList(pParser);

			// 函数体
			xvalue xBody = xlParseBlockStmt(pParser);

			xlParserExpect(pParser, XL_TK_END, (str)"end");

			xMember = xlAstCreateCtorDecl(xParams, xBody, iCtorLine, iCtorCol);
			xlAstSetInt(xMember, (str)"access", iAccess);
		}
		// 析构函数 destroy()
		else if ( xlParserCheck(pParser, XL_TK_DESTROY) ) {
			u32 iDtorLine = pParser->token.iLine;
			u32 iDtorCol = pParser->token.iColumn;
			xlParserAdvance(pParser);

			// 可选的空参数列表
			if ( xlParserCheck(pParser, XL_TK_LPAREN) ) {
				xlParserAdvance(pParser);
				xlParserExpect(pParser, XL_TK_RPAREN, (str)")");
			}

			// 函数体
			xvalue xBody = xlParseBlockStmt(pParser);

			xlParserExpect(pParser, XL_TK_END, (str)"end");

			xMember = xlAstCreateDtorDecl(xBody, iDtorLine, iDtorCol);
			xlAstSetInt(xMember, (str)"access", iAccess);
		}
		// 函数/方法
		else if ( xlParserCheck(pParser, XL_TK_FUNCTION) ) {
			xMember = xlParseFuncDecl(pParser);
			xlAstSetInt(xMember, (str)"access", iAccess);
			xlAstSetBool(xMember, (str)"isStatic", bStatic);
		}
		// 字段（类型 名称）
		else {
			xMember = xlParseVarDecl(pParser, false);
			if ( xMember ) {
				xlAstSetInt(xMember, (str)"access", iAccess);
				xlAstSetBool(xMember, (str)"isStatic", bStatic);
			}
		}

		if ( xMember ) {
			xlAstArrayAppend(xMembers, xMember);
		}

		if ( pParser->bPanicMode ) {
			xlParserSync(pParser);
		}
	}

	xlParserExpect(pParser, XL_TK_END, (str)"end");

	return xlAstCreateClassDecl(pszName, iNameLen, xParent, xMembers, iLine, iCol);
}


// 解析顶层声明
static xvalue xlParseDecl( XL_Parser *pParser )
{
	switch ( pParser->token.kind ) {
		case XL_TK_FUNCTION:
			return xlParseFuncDecl(pParser);

		case XL_TK_CLASS:
			return xlParseClassDecl(pParser);

		default:
			return xlParseStmt(pParser);
	}
}




// ============================================================================
// 解析入口
// ============================================================================

// 解析整个模块
static inline xvalue xlParserParse( XL_Parser *pParser, str pszFileName )
{
	xvalue xModule = xlAstCreateModule(pszFileName);

	// 读取第一个 Token
	xlParserAdvance(pParser);

	// 解析所有顶层声明
	while ( !xlParserCheck(pParser, XL_TK_EOF) ) {
		xvalue xDecl = xlParseDecl(pParser);
		if ( xDecl ) {
			xlAstModuleAddDecl(xModule, xDecl);
		}

		if ( pParser->bPanicMode ) {
			xlParserSync(pParser);
		}
	}

	return xModule;
}


#endif // XL_PARSER_H
