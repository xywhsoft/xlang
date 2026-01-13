/**
 * xlang - 通用定义头文件
 * 
 * 定义 Token 枚举、Token 结构、通用类型和工具宏
 */

#ifndef XL_COMMON_H
#define XL_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "../lib/xrt/xrt.h"




// ============================================================================
// Token 类型枚举
// ============================================================================

typedef enum XL_TokenKind {
	// ==================== 特殊 Token ====================
	XL_TK_EOF = 0,             // 文件结束
	XL_TK_ERROR,               // 词法错误
	XL_TK_NEWLINE,             // 换行（用于语句分隔）

	// ==================== 字面量 ====================
	XL_TK_IDENT,               // 标识符（含中文）
	XL_TK_INT,                 // 整数字面量（十进制）
	XL_TK_INT_HEX,             // 整数字面量（十六进制 0x / &H）
	XL_TK_INT_OCT,             // 整数字面量（八进制 &O）
	XL_TK_INT_BIN,             // 整数字面量（二进制 0b / &B）
	XL_TK_FLOAT,               // 浮点数字面量
	XL_TK_STRING,              // 字符串 "..." （支持转义）
	XL_TK_STRING_RAW,          // 原始字符串 '...' （不支持转义）
	XL_TK_STRING_MULTI,        // 多行字符串 """..."""
	XL_TK_STRING_MULTI_RAW,    // 多行原始字符串 '''...'''
	XL_TK_TEMPLATE,            // 模板字符串 `...`
	XL_TK_TEMPLATE_MULTI,      // 多行模板字符串 ```...```
	XL_TK_TIME,                // 时间字面量 #2000-01-01T12:00:00

	// ==================== 数据类型关键字 ====================
	XL_TK_NULL,                // null
	XL_TK_BOOL,                // bool
	XL_TK_TRUE,                // true
	XL_TK_FALSE,               // false
	XL_TK_INT_T,               // int
	XL_TK_INT8,                // int8
	XL_TK_INT16,               // int16
	XL_TK_INT32,               // int32
	XL_TK_INT64,               // int64
	XL_TK_UINT8,               // uint8
	XL_TK_UINT16,              // uint16
	XL_TK_UINT32,              // uint32
	XL_TK_UINT64,              // uint64
	XL_TK_FLOAT_T,             // float
	XL_TK_FLOAT32,             // float32
	XL_TK_FLOAT64,             // float64
	XL_TK_STRING_T,            // string
	XL_TK_TIME_T,              // time
	XL_TK_POINT,               // point
	XL_TK_ARRAY,               // array
	XL_TK_LIST,                // list
	XL_TK_SET,                 // set
	XL_TK_DICT,                // dict
	XL_TK_FUNCTION,            // function
	XL_TK_CLASS,               // class
	XL_TK_CUSTOM,              // custom
	XL_TK_VAR,                 // var
	XL_TK_AUTO,                // auto
	XL_TK_VOID,                // void
	XL_TK_TYPE,                // type
	XL_TK_TYPEDEF,             // typedef
	XL_TK_OBJECT,              // object
	XL_TK_ENUM,                // enum
	XL_TK_UNION,               // union
	XL_TK_RANGE,               // range

	// ==================== 控制流关键字 ====================
	XL_TK_IF,                  // if
	XL_TK_THEN,                // then
	XL_TK_ELSE,                // else
	XL_TK_ELSEIF,              // elseif
	XL_TK_END,                 // end
	XL_TK_FOR,                 // for
	XL_TK_IN,                  // in
	XL_TK_TO,                  // to
	XL_TK_STEP,                // step
	XL_TK_WHILE,               // while
	XL_TK_DO,                  // do
	XL_TK_BREAK,               // break
	XL_TK_CONTINUE,            // continue
	XL_TK_RETURN,              // return
	XL_TK_WITH,                // with
	XL_TK_AS,                  // as

	// ==================== 类与函数关键字 ====================
	XL_TK_CREATE,              // create
	XL_TK_DESTROY,             // destroy
	XL_TK_GET,                 // get
	XL_TK_LET,                 // let
	XL_TK_META,                // meta
	XL_TK_SELF,                // self
	XL_TK_SUPER,               // super
	XL_TK_PUBLIC,              // public
	XL_TK_PRIVATE,             // private
	XL_TK_PROTECTED,           // protected
	XL_TK_STATIC,              // static
	XL_TK_CONST,               // const
	XL_TK_NEW,                 // new
	XL_TK_EXTENDS,             // extends

	// ==================== 调用约定关键字 ====================
	XL_TK_XCALL,               // xcall
	XL_TK_CDECL,               // cdecl
	XL_TK_STDCALL,             // stdcall
	XL_TK_FASTCALL,            // fastcall
	XL_TK_EXTERN,              // extern
	XL_TK_ALIAS,               // alias

	// ==================== 逻辑运算符关键字 ====================
	XL_TK_AND,                 // and
	XL_TK_OR,                  // or
	XL_TK_NOT,                 // not
	XL_TK_IS,                  // is

	// ==================== 异常处理关键字 ====================
	XL_TK_TRY,                 // try
	XL_TK_CATCH,               // catch
	XL_TK_FINALLY,             // finally
	XL_TK_THROW,               // throw
	XL_TK_ASSERT,              // assert

	// ==================== 模块关键字（预留） ====================
	XL_TK_ASYNC,               // async
	XL_TK_AWAIT,               // await
	XL_TK_YIELD,               // yield
	XL_TK_IMPORT,              // import
	XL_TK_EXPORT,              // export
	XL_TK_INCLUDE,             // include

	// ==================== 命名空间关键字 ====================
	XL_TK_NAMESPACE,           // namespace
	XL_TK_USING,               // using

	// ==================== 比较运算符 ====================
	XL_TK_GT,                  // >
	XL_TK_LT,                  // <
	XL_TK_GE,                  // >=
	XL_TK_LE,                  // <=
	XL_TK_NE,                  // !=
	XL_TK_EQ,                  // ==
	XL_TK_APPROX,              // ~=  约等于

	// ==================== 算术运算符 ====================
	XL_TK_PLUS,                // +
	XL_TK_MINUS,               // -
	XL_TK_STAR,                // *
	XL_TK_SLASH,               // /
	XL_TK_PERCENT,             // %
	XL_TK_POWER,               // **  乘方

	// ==================== 位运算符 ====================
	XL_TK_AMP,                 // &   位与
	XL_TK_PIPE,                // |   位或
	XL_TK_BANG,                // !   位非
	XL_TK_CARET,               // ^   位异或
	XL_TK_SHR,                 // >>  右移
	XL_TK_SHL,                 // <<  左移

	// ==================== 赋值运算符 ====================
	XL_TK_ASSIGN,              // =
	XL_TK_PLUS_ASSIGN,         // +=
	XL_TK_MINUS_ASSIGN,        // -=
	XL_TK_STAR_ASSIGN,         // *=
	XL_TK_SLASH_ASSIGN,        // /=
	XL_TK_PERCENT_ASSIGN,      // %=
	XL_TK_POWER_ASSIGN,        // **=
	XL_TK_INC,                 // ++  自增
	XL_TK_DEC,                 // --  自减

	// ==================== 特殊运算符 ====================
	XL_TK_QUESTION,            // ?
	XL_TK_COLON,               // :
	XL_TK_NULL_COALESCE,       // ??  空值合并
	XL_TK_NULL_ASSIGN,         // ?=  空值赋值
	XL_TK_DOT,                 // .   成员访问
	XL_TK_SAFE_DOT,            // ?.  安全成员访问
	XL_TK_DOTDOT,              // ..  （预留：范围）
	XL_TK_ELLIPSIS,            // ... 可变参数（数组）
	XL_TK_ELLIPSIS_DICT,       // *** 可变参数（字典）

	// ==================== 分隔符 ====================
	XL_TK_LPAREN,              // (
	XL_TK_RPAREN,              // )
	XL_TK_LBRACKET,            // [
	XL_TK_RBRACKET,            // ]
	XL_TK_LBRACE,              // {
	XL_TK_RBRACE,              // }
	XL_TK_COMMA,               // ,
	XL_TK_SEMICOLON,           // ;   （可选语句分隔符）

	// ==================== 枚举结束标记 ====================
	XL_TK_COUNT                // Token 类型总数
} XL_TokenKind;




// ============================================================================
// Token 结构
// ============================================================================

typedef struct XL_Token {
	XL_TokenKind kind;      // Token 类型
	str pszStart;           // 源码中的起始位置（指向原始缓冲区）
	u32 iLength;            // Token 长度
	u32 iLine;              // 行号（从 1 开始）
	u32 iColumn;            // 列号（从 1 开始）

	// 字面量值（解析后）
	union {
		i64 iValue;         // 整数值
		f64 fValue;         // 浮点值
		struct {
			str pszText;    // 字符串内容（已处理转义，需要 free）
			u32 iLen;       // 字符串长度
		} sValue;
		xtime tValue;       // 时间值
	};
} XL_Token;




// ============================================================================
// Token 类型名称获取
// ============================================================================

static inline str xlTokenKindName( XL_TokenKind kind )
{
	static const char* arrNames[] = {
		// 特殊 Token
		"EOF", "ERROR", "NEWLINE",
		// 字面量
		"IDENT", "INT", "INT_HEX", "INT_OCT", "INT_BIN", "FLOAT",
		"STRING", "STRING_RAW", "STRING_MULTI", "STRING_MULTI_RAW",
		"TEMPLATE", "TEMPLATE_MULTI", "TIME",
		// 数据类型关键字
		"null", "bool", "true", "false",
		"int", "int8", "int16", "int32", "int64",
		"uint8", "uint16", "uint32", "uint64",
		"float", "float32", "float64",
		"string", "time", "point",
		"array", "list", "set", "dict",
		"function", "class", "custom",
		"var", "auto", "void", "type", "typedef",
		"object", "enum", "union", "range",
		// 控制流关键字
		"if", "then", "else", "elseif", "end",
		"for", "in", "to", "step",
		"while", "do", "break", "continue", "return",
		"with", "as",
		// 类与函数关键字
		"create", "destroy", "get", "let", "meta",
		"self", "super",
		"public", "private", "protected", "static", "const",
		"new", "extends",
		// 调用约定关键字
		"xcall", "cdecl", "stdcall", "fastcall", "extern", "alias",
		// 逻辑运算符关键字
		"and", "or", "not", "is",
		// 异常处理关键字
		"try", "catch", "finally", "throw", "assert",
		// 模块关键字
		"async", "await", "yield", "import", "export", "include",
		// 命名空间关键字
		"namespace", "using",
		// 比较运算符
		">", "<", ">=", "<=", "!=", "==", "~=",
		// 算术运算符
		"+", "-", "*", "/", "%", "**",
		// 位运算符
		"&", "|", "!", "^", ">>", "<<",
		// 赋值运算符
		"=", "+=", "-=", "*=", "/=", "%=", "**=", "++", "--",
		// 特殊运算符
		"?", ":", "??", "?=", ".", "?.", "..", "...", "***",
		// 分隔符
		"(", ")", "[", "]", "{", "}", ",", ";",
		// 结束标记
		"COUNT"
	};

	if ( (kind >= 0) && (kind < XL_TK_COUNT) ) {
		return (str)arrNames[kind];
	}
	return (str)"UNKNOWN";
}




// ============================================================================
// 关键字查找结构
// ============================================================================

typedef struct XL_Keyword {
	str pszName;            // 关键字名称
	XL_TokenKind kind;      // Token 类型
} XL_Keyword;

// 关键字表（按字母顺序，便于二分查找或 hash）
static const XL_Keyword g_arrKeywords[] = {
	{ (str)"alias",     XL_TK_ALIAS },
	{ (str)"and",       XL_TK_AND },
	{ (str)"array",     XL_TK_ARRAY },
	{ (str)"as",        XL_TK_AS },
	{ (str)"assert",    XL_TK_ASSERT },
	{ (str)"async",     XL_TK_ASYNC },
	{ (str)"auto",      XL_TK_AUTO },
	{ (str)"await",     XL_TK_AWAIT },
	{ (str)"bool",      XL_TK_BOOL },
	{ (str)"break",     XL_TK_BREAK },
	{ (str)"catch",     XL_TK_CATCH },
	{ (str)"cdecl",     XL_TK_CDECL },
	{ (str)"class",     XL_TK_CLASS },
	{ (str)"const",     XL_TK_CONST },
	{ (str)"continue",  XL_TK_CONTINUE },
	{ (str)"create",    XL_TK_CREATE },
	{ (str)"custom",    XL_TK_CUSTOM },
	{ (str)"destroy",   XL_TK_DESTROY },
	{ (str)"dict",      XL_TK_DICT },
	{ (str)"do",        XL_TK_DO },
	{ (str)"else",      XL_TK_ELSE },
	{ (str)"elseif",    XL_TK_ELSEIF },
	{ (str)"end",       XL_TK_END },
	{ (str)"enum",      XL_TK_ENUM },
	{ (str)"export",    XL_TK_EXPORT },
	{ (str)"extends",   XL_TK_EXTENDS },
	{ (str)"extern",    XL_TK_EXTERN },
	{ (str)"false",     XL_TK_FALSE },
	{ (str)"fastcall",  XL_TK_FASTCALL },
	{ (str)"finally",   XL_TK_FINALLY },
	{ (str)"float",     XL_TK_FLOAT_T },
	{ (str)"float32",   XL_TK_FLOAT32 },
	{ (str)"float64",   XL_TK_FLOAT64 },
	{ (str)"for",       XL_TK_FOR },
	{ (str)"function",  XL_TK_FUNCTION },
	{ (str)"get",       XL_TK_GET },
	{ (str)"if",        XL_TK_IF },
	{ (str)"import",    XL_TK_IMPORT },
	{ (str)"in",        XL_TK_IN },
	{ (str)"include",   XL_TK_INCLUDE },
	{ (str)"int",       XL_TK_INT_T },
	{ (str)"int16",     XL_TK_INT16 },
	{ (str)"int32",     XL_TK_INT32 },
	{ (str)"int64",     XL_TK_INT64 },
	{ (str)"int8",      XL_TK_INT8 },
	{ (str)"is",        XL_TK_IS },
	{ (str)"let",       XL_TK_LET },
	{ (str)"list",      XL_TK_LIST },
	{ (str)"meta",      XL_TK_META },
	{ (str)"namespace", XL_TK_NAMESPACE },
	{ (str)"new",       XL_TK_NEW },
	{ (str)"not",       XL_TK_NOT },
	{ (str)"null",      XL_TK_NULL },
	{ (str)"object",    XL_TK_OBJECT },
	{ (str)"or",        XL_TK_OR },
	{ (str)"point",     XL_TK_POINT },
	{ (str)"private",   XL_TK_PRIVATE },
	{ (str)"protected", XL_TK_PROTECTED },
	{ (str)"public",    XL_TK_PUBLIC },
	{ (str)"range",     XL_TK_RANGE },
	{ (str)"return",    XL_TK_RETURN },
	{ (str)"self",      XL_TK_SELF },
	{ (str)"set",       XL_TK_SET },
	{ (str)"static",    XL_TK_STATIC },
	{ (str)"stdcall",   XL_TK_STDCALL },
	{ (str)"step",      XL_TK_STEP },
	{ (str)"string",    XL_TK_STRING_T },
	{ (str)"super",     XL_TK_SUPER },
	{ (str)"then",      XL_TK_THEN },
	{ (str)"throw",     XL_TK_THROW },
	{ (str)"time",      XL_TK_TIME_T },
	{ (str)"to",        XL_TK_TO },
	{ (str)"true",      XL_TK_TRUE },
	{ (str)"try",       XL_TK_TRY },
	{ (str)"type",      XL_TK_TYPE },
	{ (str)"typedef",   XL_TK_TYPEDEF },
	{ (str)"uint16",    XL_TK_UINT16 },
	{ (str)"uint32",    XL_TK_UINT32 },
	{ (str)"uint64",    XL_TK_UINT64 },
	{ (str)"uint8",     XL_TK_UINT8 },
	{ (str)"union",     XL_TK_UNION },
	{ (str)"using",     XL_TK_USING },
	{ (str)"var",       XL_TK_VAR },
	{ (str)"void",      XL_TK_VOID },
	{ (str)"while",     XL_TK_WHILE },
	{ (str)"with",      XL_TK_WITH },
	{ (str)"xcall",     XL_TK_XCALL },
	{ (str)"yield",     XL_TK_YIELD },
	{ NULL, XL_TK_EOF }  // 结束标记
};

#define XL_KEYWORD_COUNT (sizeof(g_arrKeywords) / sizeof(g_arrKeywords[0]) - 1)


// 二分查找关键字
static inline XL_TokenKind xlLookupKeyword( str pszName, u32 iLen )
{
	int iLow = 0;
	int iHigh = XL_KEYWORD_COUNT - 1;

	while ( iLow <= iHigh ) {
		int iMid = (iLow + iHigh) / 2;
		str pszKeyword = g_arrKeywords[iMid].pszName;
		
		// 先比较完整字符串
		int iCmp = strncmp((char*)pszName, (char*)pszKeyword, iLen);
		
		if ( iCmp == 0 ) {
			// 前 iLen 个字符相等，检查关键字长度是否也是 iLen
			if ( pszKeyword[iLen] == '\0' ) {
				// 完全匹配
				return g_arrKeywords[iMid].kind;
			} else {
				// 关键字更长，输入是关键字的前缀
				// 例如输入 "int"(3)，关键字 "int8"(4)
				// 需要往前找更短的关键字
				iHigh = iMid - 1;
			}
		} else if ( iCmp < 0 ) {
			iHigh = iMid - 1;
		} else {
			iLow = iMid + 1;
		}
	}

	return XL_TK_IDENT;  // 不是关键字，返回标识符
}




// ============================================================================
// 工具宏
// ============================================================================

// 安全释放宏
#define XL_SAFE_FREE(p) do { if (p) { free(p); (p) = NULL; } } while(0)

// 字符串复制（带长度）
static inline str xlStrDup( str pszSrc, u32 iLen )
{
	if ( !pszSrc ) return NULL;
	if ( iLen == 0 ) iLen = (u32)strlen((char*)pszSrc);
	str pszDst = (str)malloc(iLen + 1);
	if ( pszDst ) {
		memcpy(pszDst, pszSrc, iLen);
		pszDst[iLen] = '\0';
	}
	return pszDst;
}

// 判断是否为字母或下划线（标识符起始字符）
static inline bool xlIsIdentStart( u8 c )
{
	return ( (c >= 'a') && (c <= 'z') ) ||
	       ( (c >= 'A') && (c <= 'Z') ) ||
	       ( c == '_' ) ||
	       ( c >= 0x80 );  // UTF-8 多字节字符（中文等）
}

// 判断是否为标识符字符
static inline bool xlIsIdentChar( u8 c )
{
	return xlIsIdentStart(c) ||
	       ( (c >= '0') && (c <= '9') );
}

// 判断是否为数字
static inline bool xlIsDigit( u8 c )
{
	return ( c >= '0' ) && ( c <= '9' );
}

// 判断是否为十六进制数字
static inline bool xlIsHexDigit( u8 c )
{
	return xlIsDigit(c) ||
	       ( (c >= 'a') && (c <= 'f') ) ||
	       ( (c >= 'A') && (c <= 'F') );
}

// 判断是否为空白字符
static inline bool xlIsSpace( u8 c )
{
	return ( c == ' ' ) || ( c == '\t' ) || ( c == '\r' );
}


#endif // XL_COMMON_H
