/**
 * xlang - 词法分析器模块
 * 
 * 手写的词法分析器实现，支持：
 * - 所有 xlang Token 类型
 * - 中文标识符
 * - 多种字符串字面量
 * - 详细的错误报告
 */

#ifndef XL_LEXER_H
#define XL_LEXER_H

#include "xl_common.h"
#include "xl_error.h"




// ============================================================================
// Lexer 结构
// ============================================================================

typedef struct XL_Lexer {
	str pszSource;          // 源码缓冲区（不拥有）
	size_t iSize;           // 源码长度
	str pszFileName;        // 文件名（不拥有）

	str pszCurrent;         // 当前位置
	str pszEnd;             // 结束位置
	u32 iLine;              // 当前行号（从 1 开始）
	u32 iColumn;            // 当前列号（从 1 开始）
	u32 iLineStart;         // 当前行起始列号

	XL_ErrorList *pErrors;  // 错误列表
} XL_Lexer;




// ============================================================================
// Lexer 创建与销毁
// ============================================================================

// 从内存创建 Lexer
static inline XL_Lexer* xlLexerCreate( str pszSource, size_t iSize, str pszFileName )
{
	XL_Lexer *pLexer = (XL_Lexer*)malloc(sizeof(XL_Lexer));
	if ( !pLexer ) return NULL;

	pLexer->pszSource = pszSource;
	pLexer->iSize = iSize;
	pLexer->pszFileName = pszFileName;

	pLexer->pszCurrent = pszSource;
	pLexer->pszEnd = pszSource + iSize;
	pLexer->iLine = 1;
	pLexer->iColumn = 1;
	pLexer->iLineStart = 1;

	pLexer->pErrors = xlErrorListCreate(pszFileName);

	return pLexer;
}


// 从文件创建 Lexer（需要调用者管理文件内存）
static inline XL_Lexer* xlLexerCreateFromFile( str pszFilePath )
{
	FILE *fp = fopen((char*)pszFilePath, "rb");
	if ( !fp ) return NULL;

	fseek(fp, 0, SEEK_END);
	size_t iSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	str pszSource = (str)malloc(iSize + 1);
	if ( !pszSource ) {
		fclose(fp);
		return NULL;
	}

	fread(pszSource, 1, iSize, fp);
	pszSource[iSize] = '\0';
	fclose(fp);

	XL_Lexer *pLexer = xlLexerCreate(pszSource, iSize, pszFilePath);
	if ( !pLexer ) {
		free(pszSource);
		return NULL;
	}

	return pLexer;
}


// 释放 Lexer
static inline void xlLexerDestroy( XL_Lexer *pLexer )
{
	if ( !pLexer ) return;
	if ( pLexer->pErrors ) xlErrorListDestroy(pLexer->pErrors);
	free(pLexer);
}




// ============================================================================
// Lexer 辅助函数
// ============================================================================

// 是否到达末尾
static inline bool xlLexerAtEnd( XL_Lexer *pLexer )
{
	return pLexer->pszCurrent >= pLexer->pszEnd;
}


// 查看当前字符（不前进）
static inline u8 xlLexerPeek( XL_Lexer *pLexer )
{
	if ( xlLexerAtEnd(pLexer) ) return '\0';
	return *pLexer->pszCurrent;
}


// 查看下一个字符
static inline u8 xlLexerPeekNext( XL_Lexer *pLexer )
{
	if ( pLexer->pszCurrent + 1 >= pLexer->pszEnd ) return '\0';
	return pLexer->pszCurrent[1];
}


// 查看后续第 n 个字符
static inline u8 xlLexerPeekN( XL_Lexer *pLexer, int n )
{
	if ( pLexer->pszCurrent + n >= pLexer->pszEnd ) return '\0';
	return pLexer->pszCurrent[n];
}


// 前进一个字符
static inline u8 xlLexerAdvance( XL_Lexer *pLexer )
{
	if ( xlLexerAtEnd(pLexer) ) return '\0';
	u8 c = *pLexer->pszCurrent++;
	if ( c == '\n' ) {
		pLexer->iLine++;
		pLexer->iColumn = 1;
	} else {
		pLexer->iColumn++;
	}
	return c;
}


// 匹配当前字符，如果匹配则前进
static inline bool xlLexerMatch( XL_Lexer *pLexer, u8 expected )
{
	if ( xlLexerAtEnd(pLexer) ) return false;
	if ( *pLexer->pszCurrent != expected ) return false;
	xlLexerAdvance(pLexer);
	return true;
}


// 跳过空白字符
static inline void xlLexerSkipWhitespace( XL_Lexer *pLexer )
{
	while ( !xlLexerAtEnd(pLexer) ) {
		u8 c = xlLexerPeek(pLexer);
		if ( xlIsSpace(c) ) {
			xlLexerAdvance(pLexer);
		} else {
			break;
		}
	}
}


// 跳过单行注释
static inline void xlLexerSkipLineComment( XL_Lexer *pLexer )
{
	while ( !xlLexerAtEnd(pLexer) && (xlLexerPeek(pLexer) != '\n') ) {
		xlLexerAdvance(pLexer);
	}
}


// 跳过块注释
static inline bool xlLexerSkipBlockComment( XL_Lexer *pLexer )
{
	u32 iStartLine = pLexer->iLine;
	u32 iStartCol = pLexer->iColumn;

	while ( !xlLexerAtEnd(pLexer) ) {
		if ( (xlLexerPeek(pLexer) == '*') && (xlLexerPeekNext(pLexer) == '/') ) {
			xlLexerAdvance(pLexer);  // *
			xlLexerAdvance(pLexer);  // /
			return true;
		}
		xlLexerAdvance(pLexer);
	}

	// 未闭合的注释
	xlErrorListAddFmt(pLexer->pErrors, XL_ERR_LEX_UNCLOSED_COMMENT,
	                  iStartLine, iStartCol, (str)"未闭合的块注释");
	return false;
}




// ============================================================================
// Token 创建辅助
// ============================================================================

static inline void xlLexerMakeToken( XL_Lexer *pLexer, XL_Token *pToken,
                                     XL_TokenKind kind, str pszStart, u32 iLen,
                                     u32 iLine, u32 iCol )
{
	pToken->kind = kind;
	pToken->pszStart = pszStart;
	pToken->iLength = iLen;
	pToken->iLine = iLine;
	pToken->iColumn = iCol;
	pToken->iValue = 0;
}




// ============================================================================
// 字面量解析
// ============================================================================

// 解析整数（十进制）
static inline void xlLexerParseInteger( XL_Lexer *pLexer, XL_Token *pToken,
                                        str pszStart, u32 iLine, u32 iCol )
{
	while ( xlIsDigit(xlLexerPeek(pLexer)) ) {
		xlLexerAdvance(pLexer);
	}

	// 检查是否为浮点数
	if ( (xlLexerPeek(pLexer) == '.') && xlIsDigit(xlLexerPeekNext(pLexer)) ) {
		xlLexerAdvance(pLexer);  // .
		while ( xlIsDigit(xlLexerPeek(pLexer)) ) {
			xlLexerAdvance(pLexer);
		}
		// 科学计数法
		if ( (xlLexerPeek(pLexer) == 'e') || (xlLexerPeek(pLexer) == 'E') ) {
			xlLexerAdvance(pLexer);
			if ( (xlLexerPeek(pLexer) == '+') || (xlLexerPeek(pLexer) == '-') ) {
				xlLexerAdvance(pLexer);
			}
			while ( xlIsDigit(xlLexerPeek(pLexer)) ) {
				xlLexerAdvance(pLexer);
			}
		}
		u32 iLen = (u32)(pLexer->pszCurrent - pszStart);
		xlLexerMakeToken(pLexer, pToken, XL_TK_FLOAT, pszStart, iLen, iLine, iCol);
		pToken->fValue = strtod((char*)pszStart, NULL);
		return;
	}

	// 科学计数法（整数形式）
	if ( (xlLexerPeek(pLexer) == 'e') || (xlLexerPeek(pLexer) == 'E') ) {
		xlLexerAdvance(pLexer);
		if ( (xlLexerPeek(pLexer) == '+') || (xlLexerPeek(pLexer) == '-') ) {
			xlLexerAdvance(pLexer);
		}
		while ( xlIsDigit(xlLexerPeek(pLexer)) ) {
			xlLexerAdvance(pLexer);
		}
		u32 iLen = (u32)(pLexer->pszCurrent - pszStart);
		xlLexerMakeToken(pLexer, pToken, XL_TK_FLOAT, pszStart, iLen, iLine, iCol);
		pToken->fValue = strtod((char*)pszStart, NULL);
		return;
	}

	u32 iLen = (u32)(pLexer->pszCurrent - pszStart);
	xlLexerMakeToken(pLexer, pToken, XL_TK_INT, pszStart, iLen, iLine, iCol);
	pToken->iValue = strtoll((char*)pszStart, NULL, 10);
}


// 解析十六进制整数 (0x...)
static inline void xlLexerParseHex( XL_Lexer *pLexer, XL_Token *pToken,
                                    str pszStart, u32 iLine, u32 iCol )
{
	xlLexerAdvance(pLexer);  // 跳过 x 或 X
	str pszNumStart = pLexer->pszCurrent;

	while ( xlIsHexDigit(xlLexerPeek(pLexer)) ) {
		xlLexerAdvance(pLexer);
	}

	if ( pLexer->pszCurrent == pszNumStart ) {
		xlErrorListAddFmt(pLexer->pErrors, XL_ERR_LEX_INVALID_HEX,
		                  iLine, iCol, (str)"无效的十六进制数");
		xlLexerMakeToken(pLexer, pToken, XL_TK_ERROR, pszStart, 
		                 (u32)(pLexer->pszCurrent - pszStart), iLine, iCol);
		return;
	}

	u32 iLen = (u32)(pLexer->pszCurrent - pszStart);
	xlLexerMakeToken(pLexer, pToken, XL_TK_INT_HEX, pszStart, iLen, iLine, iCol);
	pToken->iValue = strtoll((char*)pszNumStart, NULL, 16);
}


// 解析二进制整数 (0b...)
static inline void xlLexerParseBin( XL_Lexer *pLexer, XL_Token *pToken,
                                    str pszStart, u32 iLine, u32 iCol )
{
	xlLexerAdvance(pLexer);  // 跳过 b 或 B
	str pszNumStart = pLexer->pszCurrent;

	while ( (xlLexerPeek(pLexer) == '0') || (xlLexerPeek(pLexer) == '1') ) {
		xlLexerAdvance(pLexer);
	}

	if ( pLexer->pszCurrent == pszNumStart ) {
		xlErrorListAddFmt(pLexer->pErrors, XL_ERR_LEX_INVALID_BIN,
		                  iLine, iCol, (str)"无效的二进制数");
		xlLexerMakeToken(pLexer, pToken, XL_TK_ERROR, pszStart,
		                 (u32)(pLexer->pszCurrent - pszStart), iLine, iCol);
		return;
	}

	u32 iLen = (u32)(pLexer->pszCurrent - pszStart);
	xlLexerMakeToken(pLexer, pToken, XL_TK_INT_BIN, pszStart, iLen, iLine, iCol);
	pToken->iValue = strtoll((char*)pszNumStart, NULL, 2);
}


// 解析八进制整数 (&O 或 &o)
static inline void xlLexerParseOct( XL_Lexer *pLexer, XL_Token *pToken,
                                    str pszStart, u32 iLine, u32 iCol )
{
	xlLexerAdvance(pLexer);  // 跳过 O 或 o
	str pszNumStart = pLexer->pszCurrent;

	while ( xlIsOctDigit(xlLexerPeek(pLexer)) ) {
		xlLexerAdvance(pLexer);
	}

	if ( pLexer->pszCurrent == pszNumStart ) {
		xlErrorListAddFmt(pLexer->pErrors, XL_ERR_LEX_INVALID_OCT,
		                  iLine, iCol, (str)"无效的八进制数");
		xlLexerMakeToken(pLexer, pToken, XL_TK_ERROR, pszStart,
		                 (u32)(pLexer->pszCurrent - pszStart), iLine, iCol);
		return;
	}

	u32 iLen = (u32)(pLexer->pszCurrent - pszStart);
	xlLexerMakeToken(pLexer, pToken, XL_TK_INT_OCT, pszStart, iLen, iLine, iCol);
	pToken->iValue = strtoll((char*)pszNumStart, NULL, 8);
}


// 解析字符串 "..."
static inline void xlLexerParseString( XL_Lexer *pLexer, XL_Token *pToken,
                                       str pszStart, u32 iLine, u32 iCol )
{
	// 检查是否为多行字符串 """..."""
	if ( (xlLexerPeek(pLexer) == '"') && (xlLexerPeekNext(pLexer) == '"') ) {
		xlLexerAdvance(pLexer);  // "
		xlLexerAdvance(pLexer);  // "
		// 多行字符串
		str pszContentStart = pLexer->pszCurrent;
		while ( !xlLexerAtEnd(pLexer) ) {
			if ( (xlLexerPeek(pLexer) == '"') &&
			     (xlLexerPeekNext(pLexer) == '"') &&
			     (xlLexerPeekN(pLexer, 2) == '"') ) {
				u32 iContentLen = (u32)(pLexer->pszCurrent - pszContentStart);
				xlLexerAdvance(pLexer);  // "
				xlLexerAdvance(pLexer);  // "
				xlLexerAdvance(pLexer);  // "
				u32 iLen = (u32)(pLexer->pszCurrent - pszStart);
				xlLexerMakeToken(pLexer, pToken, XL_TK_STRING_MULTI, pszStart, iLen, iLine, iCol);
				// TODO: 处理转义
				pToken->sValue.pszText = xlStrDup(pszContentStart, iContentLen);
				pToken->sValue.iLen = iContentLen;
				return;
			}
			xlLexerAdvance(pLexer);
		}
		xlErrorListAddFmt(pLexer->pErrors, XL_ERR_LEX_UNCLOSED_STRING,
		                  iLine, iCol, (str)"未闭合的多行字符串");
		xlLexerMakeToken(pLexer, pToken, XL_TK_ERROR, pszStart,
		                 (u32)(pLexer->pszCurrent - pszStart), iLine, iCol);
		return;
	}

	// 单行字符串
	str pszContentStart = pLexer->pszCurrent;
	size_t iContentLen = 0;
	char *pszContent = (char*)malloc(pLexer->iSize);  // 最大长度
	if ( !pszContent ) {
		xlLexerMakeToken(pLexer, pToken, XL_TK_ERROR, pszStart, 1, iLine, iCol);
		return;
	}

	while ( !xlLexerAtEnd(pLexer) ) {
		u8 c = xlLexerPeek(pLexer);
		if ( c == '"' ) {
			xlLexerAdvance(pLexer);
			break;
		}
		if ( c == '\n' ) {
			xlErrorListAddFmt(pLexer->pErrors, XL_ERR_LEX_UNCLOSED_STRING,
			                  iLine, iCol, (str)"字符串不能跨行");
			xlLexerMakeToken(pLexer, pToken, XL_TK_ERROR, pszStart,
			                 (u32)(pLexer->pszCurrent - pszStart), iLine, iCol);
			free(pszContent);
			return;
		}
		if ( c == '\\' ) {
			xlLexerAdvance(pLexer);
			if ( xlLexerAtEnd(pLexer) ) break;
			u8 esc = xlLexerAdvance(pLexer);
			switch ( esc ) {
				case 'n': pszContent[iContentLen++] = '\n'; break;
				case 'r': pszContent[iContentLen++] = '\r'; break;
				case 't': pszContent[iContentLen++] = '\t'; break;
				case '\\': pszContent[iContentLen++] = '\\'; break;
				case '"': pszContent[iContentLen++] = '"'; break;
				case '0': pszContent[iContentLen++] = '\0'; break;
				default:
					pszContent[iContentLen++] = esc;
					break;
			}
		} else {
			pszContent[iContentLen++] = xlLexerAdvance(pLexer);
		}
	}

	pszContent[iContentLen] = '\0';
	u32 iLen = (u32)(pLexer->pszCurrent - pszStart);
	xlLexerMakeToken(pLexer, pToken, XL_TK_STRING, pszStart, iLen, iLine, iCol);
	pToken->sValue.pszText = (str)pszContent;
	pToken->sValue.iLen = (u32)iContentLen;
}


// 解析原始字符串 '...'
static inline void xlLexerParseRawString( XL_Lexer *pLexer, XL_Token *pToken,
                                          str pszStart, u32 iLine, u32 iCol )
{
	// 检查是否为多行原始字符串 '''...'''
	if ( (xlLexerPeek(pLexer) == '\'') && (xlLexerPeekNext(pLexer) == '\'') ) {
		xlLexerAdvance(pLexer);  // '
		xlLexerAdvance(pLexer);  // '
		str pszContentStart = pLexer->pszCurrent;
		while ( !xlLexerAtEnd(pLexer) ) {
			if ( (xlLexerPeek(pLexer) == '\'') &&
			     (xlLexerPeekNext(pLexer) == '\'') &&
			     (xlLexerPeekN(pLexer, 2) == '\'') ) {
				u32 iContentLen = (u32)(pLexer->pszCurrent - pszContentStart);
				xlLexerAdvance(pLexer);
				xlLexerAdvance(pLexer);
				xlLexerAdvance(pLexer);
				u32 iLen = (u32)(pLexer->pszCurrent - pszStart);
				xlLexerMakeToken(pLexer, pToken, XL_TK_STRING_MULTI_RAW, pszStart, iLen, iLine, iCol);
				pToken->sValue.pszText = xlStrDup(pszContentStart, iContentLen);
				pToken->sValue.iLen = iContentLen;
				return;
			}
			xlLexerAdvance(pLexer);
		}
		xlErrorListAddFmt(pLexer->pErrors, XL_ERR_LEX_UNCLOSED_STRING,
		                  iLine, iCol, (str)"未闭合的多行原始字符串");
		xlLexerMakeToken(pLexer, pToken, XL_TK_ERROR, pszStart,
		                 (u32)(pLexer->pszCurrent - pszStart), iLine, iCol);
		return;
	}

	// 单行原始字符串（两个单引号表示一个单引号）
	str pszContentStart = pLexer->pszCurrent;
	size_t iContentLen = 0;
	char *pszContent = (char*)malloc(pLexer->iSize);
	if ( !pszContent ) {
		xlLexerMakeToken(pLexer, pToken, XL_TK_ERROR, pszStart, 1, iLine, iCol);
		return;
	}

	while ( !xlLexerAtEnd(pLexer) ) {
		u8 c = xlLexerPeek(pLexer);
		if ( c == '\'' ) {
			if ( xlLexerPeekNext(pLexer) == '\'' ) {
				// 两个单引号表示一个单引号
				xlLexerAdvance(pLexer);
				xlLexerAdvance(pLexer);
				pszContent[iContentLen++] = '\'';
			} else {
				xlLexerAdvance(pLexer);
				break;
			}
		} else if ( c == '\n' ) {
			xlErrorListAddFmt(pLexer->pErrors, XL_ERR_LEX_UNCLOSED_STRING,
			                  iLine, iCol, (str)"原始字符串不能跨行");
			xlLexerMakeToken(pLexer, pToken, XL_TK_ERROR, pszStart,
			                 (u32)(pLexer->pszCurrent - pszStart), iLine, iCol);
			free(pszContent);
			return;
		} else {
			pszContent[iContentLen++] = xlLexerAdvance(pLexer);
		}
	}

	pszContent[iContentLen] = '\0';
	u32 iLen = (u32)(pLexer->pszCurrent - pszStart);
	xlLexerMakeToken(pLexer, pToken, XL_TK_STRING_RAW, pszStart, iLen, iLine, iCol);
	pToken->sValue.pszText = (str)pszContent;
	pToken->sValue.iLen = (u32)iContentLen;
}


// 解析模板字符串 `...`
static inline void xlLexerParseTemplate( XL_Lexer *pLexer, XL_Token *pToken,
                                         str pszStart, u32 iLine, u32 iCol )
{
	// 检查是否为多行模板字符串 ```...```
	if ( (xlLexerPeek(pLexer) == '`') && (xlLexerPeekNext(pLexer) == '`') ) {
		xlLexerAdvance(pLexer);  // `
		xlLexerAdvance(pLexer);  // `
		str pszContentStart = pLexer->pszCurrent;
		while ( !xlLexerAtEnd(pLexer) ) {
			if ( (xlLexerPeek(pLexer) == '`') &&
			     (xlLexerPeekNext(pLexer) == '`') &&
			     (xlLexerPeekN(pLexer, 2) == '`') ) {
				u32 iContentLen = (u32)(pLexer->pszCurrent - pszContentStart);
				xlLexerAdvance(pLexer);
				xlLexerAdvance(pLexer);
				xlLexerAdvance(pLexer);
				u32 iLen = (u32)(pLexer->pszCurrent - pszStart);
				xlLexerMakeToken(pLexer, pToken, XL_TK_TEMPLATE_MULTI, pszStart, iLen, iLine, iCol);
				pToken->sValue.pszText = xlStrDup(pszContentStart, iContentLen);
				pToken->sValue.iLen = iContentLen;
				return;
			}
			xlLexerAdvance(pLexer);
		}
		xlErrorListAddFmt(pLexer->pErrors, XL_ERR_LEX_UNCLOSED_STRING,
		                  iLine, iCol, (str)"未闭合的多行模板字符串");
		xlLexerMakeToken(pLexer, pToken, XL_TK_ERROR, pszStart,
		                 (u32)(pLexer->pszCurrent - pszStart), iLine, iCol);
		return;
	}

	// 单行模板字符串
	str pszContentStart = pLexer->pszCurrent;
	while ( !xlLexerAtEnd(pLexer) ) {
		u8 c = xlLexerPeek(pLexer);
		if ( c == '`' ) {
			u32 iContentLen = (u32)(pLexer->pszCurrent - pszContentStart);
			xlLexerAdvance(pLexer);
			u32 iLen = (u32)(pLexer->pszCurrent - pszStart);
			xlLexerMakeToken(pLexer, pToken, XL_TK_TEMPLATE, pszStart, iLen, iLine, iCol);
			pToken->sValue.pszText = xlStrDup(pszContentStart, iContentLen);
			pToken->sValue.iLen = iContentLen;
			return;
		}
		if ( c == '\n' ) {
			xlErrorListAddFmt(pLexer->pErrors, XL_ERR_LEX_UNCLOSED_STRING,
			                  iLine, iCol, (str)"模板字符串不能跨行");
			xlLexerMakeToken(pLexer, pToken, XL_TK_ERROR, pszStart,
			                 (u32)(pLexer->pszCurrent - pszStart), iLine, iCol);
			return;
		}
		xlLexerAdvance(pLexer);
	}

	xlErrorListAddFmt(pLexer->pErrors, XL_ERR_LEX_UNCLOSED_STRING,
	                  iLine, iCol, (str)"未闭合的模板字符串");
	xlLexerMakeToken(pLexer, pToken, XL_TK_ERROR, pszStart,
	                 (u32)(pLexer->pszCurrent - pszStart), iLine, iCol);
}


// 解析标识符或关键字
static inline void xlLexerParseIdent( XL_Lexer *pLexer, XL_Token *pToken,
                                      str pszStart, u32 iLine, u32 iCol )
{
	while ( xlIsIdentChar(xlLexerPeek(pLexer)) ) {
		xlLexerAdvance(pLexer);
	}

	u32 iLen = (u32)(pLexer->pszCurrent - pszStart);
	XL_TokenKind kind = xlLookupKeyword(pszStart, iLen);
	xlLexerMakeToken(pLexer, pToken, kind, pszStart, iLen, iLine, iCol);
}




// ============================================================================
// 主要扫描函数
// ============================================================================

// 获取下一个 Token
// 返回: 0 = 成功, 1 = EOF, -1 = 错误
static inline int xlLexerNext( XL_Lexer *pLexer, XL_Token *pToken )
{
	if ( !pLexer || !pToken ) return -1;

	// 跳过空白和注释
	while ( !xlLexerAtEnd(pLexer) ) {
		xlLexerSkipWhitespace(pLexer);
		if ( xlLexerAtEnd(pLexer) ) break;

		u8 c = xlLexerPeek(pLexer);

		// 换行符
		if ( c == '\n' ) {
			u32 iLine = pLexer->iLine;
			u32 iCol = pLexer->iColumn;
			str pszStart = pLexer->pszCurrent;
			xlLexerAdvance(pLexer);
			xlLexerMakeToken(pLexer, pToken, XL_TK_NEWLINE, pszStart, 1, iLine, iCol);
			return 0;
		}

		// 单行注释
		if ( (c == '/') && (xlLexerPeekNext(pLexer) == '/') ) {
			xlLexerSkipLineComment(pLexer);
			continue;
		}

		// 块注释
		if ( (c == '/') && (xlLexerPeekNext(pLexer) == '*') ) {
			xlLexerAdvance(pLexer);  // /
			xlLexerAdvance(pLexer);  // *
			if ( !xlLexerSkipBlockComment(pLexer) ) {
				xlLexerMakeToken(pLexer, pToken, XL_TK_ERROR, pLexer->pszCurrent, 0,
				                 pLexer->iLine, pLexer->iColumn);
				return -1;
			}
			continue;
		}

		break;
	}

	// EOF
	if ( xlLexerAtEnd(pLexer) ) {
		xlLexerMakeToken(pLexer, pToken, XL_TK_EOF, pLexer->pszCurrent, 0,
		                 pLexer->iLine, pLexer->iColumn);
		return 1;
	}

	u32 iLine = pLexer->iLine;
	u32 iCol = pLexer->iColumn;
	str pszStart = pLexer->pszCurrent;
	u8 c = xlLexerAdvance(pLexer);

	// 标识符或关键字
	if ( xlIsIdentStart(c) ) {
		xlLexerParseIdent(pLexer, pToken, pszStart, iLine, iCol);
		return 0;
	}

	// 数字
	if ( xlIsDigit(c) ) {
		if ( c == '0' ) {
			u8 next = xlLexerPeek(pLexer);
			if ( (next == 'x') || (next == 'X') ) {
				xlLexerParseHex(pLexer, pToken, pszStart, iLine, iCol);
				return 0;
			}
			if ( (next == 'b') || (next == 'B') ) {
				xlLexerParseBin(pLexer, pToken, pszStart, iLine, iCol);
				return 0;
			}
		}
		xlLexerParseInteger(pLexer, pToken, pszStart, iLine, iCol);
		return 0;
	}

	// 字符串
	if ( c == '"' ) {
		xlLexerParseString(pLexer, pToken, pszStart, iLine, iCol);
		return 0;
	}

	// 原始字符串
	if ( c == '\'' ) {
		xlLexerParseRawString(pLexer, pToken, pszStart, iLine, iCol);
		return 0;
	}

	// 模板字符串
	if ( c == '`' ) {
		xlLexerParseTemplate(pLexer, pToken, pszStart, iLine, iCol);
		return 0;
	}

	// 时间字面量 #2000-01-01 或 #12:00:00 或 #2000-01-01T12:00:00
	if ( c == '#' ) {
		// 解析时间字面量
		str pszTimeStart = pLexer->pszCurrent;
		
		// 跳过所有合法的时间字符: 数字、-、:、T、+、.
		while ( !xlLexerAtEnd(pLexer) ) {
			u8 ch = xlLexerPeek(pLexer);
			if ( xlIsDigit(ch) || (ch == '-') || (ch == ':') ||
			     (ch == 'T') || (ch == '+') || (ch == '.') || (ch == 'Z') ) {
				xlLexerAdvance(pLexer);
			} else {
				break;
			}
		}
		
		u32 iTimeLen = (u32)(pLexer->pszCurrent - pszTimeStart);
		if ( iTimeLen == 0 ) {
			xlErrorListAddFmt(pLexer->pErrors, XL_ERR_LEX_INVALID_TIME,
			                  iLine, iCol, (str)"无效的时间字面量");
			xlLexerMakeToken(pLexer, pToken, XL_TK_ERROR, pszStart, 1, iLine, iCol);
			return -1;
		}
		
		u32 iLen = (u32)(pLexer->pszCurrent - pszStart);
		xlLexerMakeToken(pLexer, pToken, XL_TK_TIME, pszStart, iLen, iLine, iCol);
		pToken->sValue.pszText = xlStrDup(pszTimeStart, iTimeLen);
		pToken->sValue.iLen = iTimeLen;
		return 0;
	}

	// 运算符和分隔符
	switch ( c ) {
		// 比较运算符
		case '>':
			if ( xlLexerMatch(pLexer, '=') ) {
				xlLexerMakeToken(pLexer, pToken, XL_TK_GE, pszStart, 2, iLine, iCol);
			} else if ( xlLexerMatch(pLexer, '>') ) {
				xlLexerMakeToken(pLexer, pToken, XL_TK_SHR, pszStart, 2, iLine, iCol);
			} else {
				xlLexerMakeToken(pLexer, pToken, XL_TK_GT, pszStart, 1, iLine, iCol);
			}
			return 0;

		case '<':
			if ( xlLexerMatch(pLexer, '=') ) {
				xlLexerMakeToken(pLexer, pToken, XL_TK_LE, pszStart, 2, iLine, iCol);
			} else if ( xlLexerMatch(pLexer, '<') ) {
				xlLexerMakeToken(pLexer, pToken, XL_TK_SHL, pszStart, 2, iLine, iCol);
			} else {
				xlLexerMakeToken(pLexer, pToken, XL_TK_LT, pszStart, 1, iLine, iCol);
			}
			return 0;

		case '=':
			if ( xlLexerMatch(pLexer, '=') ) {
				xlLexerMakeToken(pLexer, pToken, XL_TK_EQ, pszStart, 2, iLine, iCol);
			} else {
				xlLexerMakeToken(pLexer, pToken, XL_TK_ASSIGN, pszStart, 1, iLine, iCol);
			}
			return 0;

		case '!':
			if ( xlLexerMatch(pLexer, '=') ) {
				xlLexerMakeToken(pLexer, pToken, XL_TK_NE, pszStart, 2, iLine, iCol);
			} else {
				xlLexerMakeToken(pLexer, pToken, XL_TK_BANG, pszStart, 1, iLine, iCol);
			}
			return 0;

		case '~':
			if ( xlLexerMatch(pLexer, '=') ) {
				xlLexerMakeToken(pLexer, pToken, XL_TK_APPROX, pszStart, 2, iLine, iCol);
			} else {
				xlErrorListAddFmt(pLexer->pErrors, XL_ERR_LEX_UNKNOWN_CHAR,
				                  iLine, iCol, (str)"未知字符 '~'");
				xlLexerMakeToken(pLexer, pToken, XL_TK_ERROR, pszStart, 1, iLine, iCol);
			}
			return 0;

		// 算术运算符
		case '+':
			if ( xlLexerMatch(pLexer, '=') ) {
				xlLexerMakeToken(pLexer, pToken, XL_TK_PLUS_ASSIGN, pszStart, 2, iLine, iCol);
			} else if ( xlLexerMatch(pLexer, '+') ) {
				xlLexerMakeToken(pLexer, pToken, XL_TK_INC, pszStart, 2, iLine, iCol);
			} else {
				xlLexerMakeToken(pLexer, pToken, XL_TK_PLUS, pszStart, 1, iLine, iCol);
			}
			return 0;

		case '-':
			if ( xlLexerMatch(pLexer, '=') ) {
				xlLexerMakeToken(pLexer, pToken, XL_TK_MINUS_ASSIGN, pszStart, 2, iLine, iCol);
			} else if ( xlLexerMatch(pLexer, '-') ) {
				xlLexerMakeToken(pLexer, pToken, XL_TK_DEC, pszStart, 2, iLine, iCol);
			} else {
				xlLexerMakeToken(pLexer, pToken, XL_TK_MINUS, pszStart, 1, iLine, iCol);
			}
			return 0;

		case '*':
			if ( xlLexerMatch(pLexer, '*') ) {
				if ( xlLexerMatch(pLexer, '=') ) {
					xlLexerMakeToken(pLexer, pToken, XL_TK_POWER_ASSIGN, pszStart, 3, iLine, iCol);
				} else if ( xlLexerMatch(pLexer, '*') ) {
					xlLexerMakeToken(pLexer, pToken, XL_TK_ELLIPSIS_DICT, pszStart, 3, iLine, iCol);
				} else {
					xlLexerMakeToken(pLexer, pToken, XL_TK_POWER, pszStart, 2, iLine, iCol);
				}
			} else if ( xlLexerMatch(pLexer, '=') ) {
				xlLexerMakeToken(pLexer, pToken, XL_TK_STAR_ASSIGN, pszStart, 2, iLine, iCol);
			} else {
				xlLexerMakeToken(pLexer, pToken, XL_TK_STAR, pszStart, 1, iLine, iCol);
			}
			return 0;

		case '/':
			if ( xlLexerMatch(pLexer, '=') ) {
				xlLexerMakeToken(pLexer, pToken, XL_TK_SLASH_ASSIGN, pszStart, 2, iLine, iCol);
			} else {
				xlLexerMakeToken(pLexer, pToken, XL_TK_SLASH, pszStart, 1, iLine, iCol);
			}
			return 0;

		case '%':
			if ( xlLexerMatch(pLexer, '=') ) {
				xlLexerMakeToken(pLexer, pToken, XL_TK_PERCENT_ASSIGN, pszStart, 2, iLine, iCol);
			} else {
				xlLexerMakeToken(pLexer, pToken, XL_TK_PERCENT, pszStart, 1, iLine, iCol);
			}
			return 0;

		// 位运算符 & 或 VB 风格进制前缀 (&O, &H, &B)
		case '&': {
			u8 next = xlLexerPeek(pLexer);
			if ( (next == 'O') || (next == 'o') ) {
				// 八进制: &O 或 &o
				xlLexerParseOct(pLexer, pToken, pszStart, iLine, iCol);
				return 0;
			}
			if ( (next == 'H') || (next == 'h') ) {
				// 十六进制: &H 或 &h
				xlLexerParseHex(pLexer, pToken, pszStart, iLine, iCol);
				return 0;
			}
			if ( (next == 'B') || (next == 'b') ) {
				// 二进制: &B 或 &b
				xlLexerParseBin(pLexer, pToken, pszStart, iLine, iCol);
				return 0;
			}
			xlLexerMakeToken(pLexer, pToken, XL_TK_AMP, pszStart, 1, iLine, iCol);
			return 0;
		}

		case '|':
			xlLexerMakeToken(pLexer, pToken, XL_TK_PIPE, pszStart, 1, iLine, iCol);
			return 0;

		case '^':
			xlLexerMakeToken(pLexer, pToken, XL_TK_CARET, pszStart, 1, iLine, iCol);
			return 0;

		// 特殊运算符
		case '?':
			if ( xlLexerMatch(pLexer, '?') ) {
				xlLexerMakeToken(pLexer, pToken, XL_TK_NULL_COALESCE, pszStart, 2, iLine, iCol);
			} else if ( xlLexerMatch(pLexer, '=') ) {
				xlLexerMakeToken(pLexer, pToken, XL_TK_NULL_ASSIGN, pszStart, 2, iLine, iCol);
			} else if ( xlLexerMatch(pLexer, '.') ) {
				xlLexerMakeToken(pLexer, pToken, XL_TK_SAFE_DOT, pszStart, 2, iLine, iCol);
			} else {
				xlLexerMakeToken(pLexer, pToken, XL_TK_QUESTION, pszStart, 1, iLine, iCol);
			}
			return 0;

		case ':':
			xlLexerMakeToken(pLexer, pToken, XL_TK_COLON, pszStart, 1, iLine, iCol);
			return 0;

		case '.':
			if ( xlLexerMatch(pLexer, '.') ) {
				if ( xlLexerMatch(pLexer, '.') ) {
					xlLexerMakeToken(pLexer, pToken, XL_TK_ELLIPSIS, pszStart, 3, iLine, iCol);
				} else {
					xlLexerMakeToken(pLexer, pToken, XL_TK_DOTDOT, pszStart, 2, iLine, iCol);
				}
			} else {
				xlLexerMakeToken(pLexer, pToken, XL_TK_DOT, pszStart, 1, iLine, iCol);
			}
			return 0;

		// 分隔符
		case '(':
			xlLexerMakeToken(pLexer, pToken, XL_TK_LPAREN, pszStart, 1, iLine, iCol);
			return 0;

		case ')':
			xlLexerMakeToken(pLexer, pToken, XL_TK_RPAREN, pszStart, 1, iLine, iCol);
			return 0;

		case '[':
			xlLexerMakeToken(pLexer, pToken, XL_TK_LBRACKET, pszStart, 1, iLine, iCol);
			return 0;

		case ']':
			xlLexerMakeToken(pLexer, pToken, XL_TK_RBRACKET, pszStart, 1, iLine, iCol);
			return 0;

		case '{':
			xlLexerMakeToken(pLexer, pToken, XL_TK_LBRACE, pszStart, 1, iLine, iCol);
			return 0;

		case '}':
			xlLexerMakeToken(pLexer, pToken, XL_TK_RBRACE, pszStart, 1, iLine, iCol);
			return 0;

		case ',':
			xlLexerMakeToken(pLexer, pToken, XL_TK_COMMA, pszStart, 1, iLine, iCol);
			return 0;

		case ';':
			xlLexerMakeToken(pLexer, pToken, XL_TK_SEMICOLON, pszStart, 1, iLine, iCol);
			return 0;

		default:
			xlErrorListAddFmt(pLexer->pErrors, XL_ERR_LEX_UNKNOWN_CHAR,
			                  iLine, iCol, (str)"未知字符 '%c' (0x%02X)", c, c);
			xlLexerMakeToken(pLexer, pToken, XL_TK_ERROR, pszStart, 1, iLine, iCol);
			return -1;
	}
}




// ============================================================================
// 调试输出
// ============================================================================

// 打印所有 Token
static inline void xlLexerDumpTokens( XL_Lexer *pLexer )
{
	if ( !pLexer ) return;

	// 重置位置
	pLexer->pszCurrent = pLexer->pszSource;
	pLexer->iLine = 1;
	pLexer->iColumn = 1;

	XL_Token token;
	int iResult;

	printf("=== Token 列表 ===\n");
	printf("%-6s %-4s %-4s %-20s %s\n", "行", "列", "长度", "类型", "文本");
	printf("--------------------------------------------------------------\n");

	while ( (iResult = xlLexerNext(pLexer, &token)) == 0 ) {
		// 跳过换行符（可选）
		// if ( token.kind == XL_TK_NEWLINE ) continue;

		char szText[64];
		if ( token.iLength < sizeof(szText) - 1 ) {
			memcpy(szText, token.pszStart, token.iLength);
			szText[token.iLength] = '\0';
		} else {
			memcpy(szText, token.pszStart, sizeof(szText) - 4);
			strcpy(szText + sizeof(szText) - 4, "...");
		}

		// 转义换行符用于显示
		if ( token.kind == XL_TK_NEWLINE ) {
			strcpy(szText, "\\n");
		}

		printf("%-6u %-4u %-4u %-20s %s\n",
		       token.iLine, token.iColumn, token.iLength,
		       (char*)xlTokenKindName(token.kind), szText);

		// 释放字符串值
		if ( (token.kind == XL_TK_STRING) || (token.kind == XL_TK_STRING_RAW) ||
		     (token.kind == XL_TK_STRING_MULTI) || (token.kind == XL_TK_STRING_MULTI_RAW) ||
		     (token.kind == XL_TK_TEMPLATE) || (token.kind == XL_TK_TEMPLATE_MULTI) ) {
			XL_SAFE_FREE(token.sValue.pszText);
		}
	}

	if ( iResult == 1 ) {
		printf("%-6u %-4u %-4u %-20s\n",
		       token.iLine, token.iColumn, 0, "EOF");
	}

	printf("--------------------------------------------------------------\n");

	// 打印错误
	if ( xlErrorListHasError(pLexer->pErrors) ) {
		printf("\n");
		xlErrorListPrint(pLexer->pErrors);
	}
}


#endif // XL_LEXER_H
