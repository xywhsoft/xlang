/**
 * xlang v4 - 共享 Token 定义
 * 
 * 从 Lexer 中提取的 Token 类型枚举和结构体定义
 * 供 Lexer、Parser（Lemon）和 API 层共享使用
 */

#ifndef XL_TOKENS_H
#define XL_TOKENS_H

#include <stdint.h>
#include <string.h>



/* Token 类型枚举 - 统一使用 XL_ 前缀避免关键字冲突 */
typedef enum XL_TokenType {
	/* === 特殊 Token === */
	XL_TOK_EOF = 0,
	XL_TOK_ERROR,
	XL_TOK_NEWLINE,

	/* === 字面量 === */
	XL_TOK_IDENT,           /* 标识符 */
	XL_TOK_INT,
	XL_TOK_INT_HEX,
	XL_TOK_INT_OCT,
	XL_TOK_INT_BIN,
	XL_TOK_FLOAT,
	XL_TOK_STRING,
	XL_TOK_STRING_RAW,
	XL_TOK_STRING_MULTI,
	XL_TOK_STRING_MULTI_RAW,
	XL_TOK_TIME,

	/* === 内置类型关键字 === */
	XL_KW_NULL,             /* null */
	XL_KW_FUNCTION,         /* function */

	/* === 布尔字面量 === */
	XL_KW_TRUE,
	XL_KW_FALSE,

	/* === 声明关键字 === */
	XL_KW_VAR,              /* var - 变体类型声明 */
	XL_KW_AUTO,             /* auto - 自动类型推导 */

	/* === 控制流关键字 === */
	XL_KW_IF,
	XL_KW_THEN,
	XL_KW_ELSE,
	XL_KW_ELSEIF,
	XL_KW_END,
	XL_KW_FOR,
	XL_KW_IN,
	XL_KW_TO,
	XL_KW_STEP,
	XL_KW_WHILE,
	XL_KW_DO,
	XL_KW_BREAK,
	XL_KW_CONTINUE,
	XL_KW_RETURN,

	/* === 函数关键字 === */
	XL_KW_FUNC,
	XL_KW_ARG,
	XL_KW_AS,
	XL_KW_CONST,

	/* === 逻辑运算符关键字 === */
	XL_KW_AND,
	XL_KW_OR,
	XL_KW_NOT,

	/* === 类型名硬关键字（12 个） === */
	XL_KW_BOOL,
	XL_KW_INT,
	XL_KW_FLOAT,
	XL_KW_STRING,
	XL_KW_TIME,
	XL_KW_POINT,
	XL_KW_ARRAY,
	XL_KW_LIST,
	XL_KW_SET,
	XL_KW_DICT,
	XL_KW_TYPE,
	XL_KW_RANGE,

	/* === 比较运算符 === */
	XL_OP_GT,
	XL_OP_LT,
	XL_OP_GE,
	XL_OP_LE,
	XL_OP_NE,
	XL_OP_EQ,
	XL_OP_APPROX,

	/* === 算术运算符 === */
	XL_OP_PLUS,
	XL_OP_MINUS,
	XL_OP_STAR,
	XL_OP_SLASH,
	XL_OP_PERCENT,
	XL_OP_POWER,

	/* === 位运算符 === */
	XL_OP_AMP,
	XL_OP_PIPE,
	XL_OP_BANG,
	XL_OP_CARET,
	XL_OP_SHR,
	XL_OP_SHL,

	/* === 赋值运算符 === */
	XL_OP_ASSIGN,
	XL_OP_PLUS_ASSIGN,
	XL_OP_MINUS_ASSIGN,
	XL_OP_STAR_ASSIGN,
	XL_OP_SLASH_ASSIGN,
	XL_OP_PERCENT_ASSIGN,
	XL_OP_POWER_ASSIGN,
	XL_OP_INC,
	XL_OP_DEC,

	/* === 特殊运算符 === */
	XL_OP_QUESTION,
	XL_OP_COLON,
	XL_OP_NULL_COALESCE,
	XL_OP_NULL_ASSIGN,
	XL_OP_DOT,
	XL_OP_SAFE_DOT,
	XL_OP_SAFE_BRACKET,
	XL_OP_ELLIPSIS,
	XL_OP_ELLIPSIS_DICT,
	XL_OP_RANGE,

	/* === 分隔符 === */
	XL_TOK_LPAREN,
	XL_TOK_RPAREN,
	XL_TOK_LBRACKET,
	XL_TOK_RBRACKET,
	XL_TOK_LBRACE,
	XL_TOK_RBRACE,
	XL_TOK_COMMA,
	XL_TOK_SEMICOLON,

	XL_TOK_COUNT
} XL_TokenType;



/* Token 结构 */
typedef struct XL_Token {
	XL_TokenType type;
	char* text;
	size_t length;
	int line;
	int column;
	union {
		int64_t iVal;
		double fVal;
		char* sVal;
	};
} XL_Token;



/* Lexer 额外数据 */
typedef struct XL_LexerExtra {
	const char* filename;
	int column;
	int prevColumn;
	char* errorMsg;
	XL_Token currentToken;
} XL_LexerExtra;



/* 获取 Token 类型名称 */
static const char* xlTokenTypeName( XL_TokenType type )
{
	static const char* names[] = {
		"EOF", "ERROR", "NEWLINE",
		"IDENT", "INT", "INT_HEX", "INT_OCT", "INT_BIN",
		"FLOAT", "STRING", "STRING_RAW", "STRING_MULTI", "STRING_MULTI_RAW", "TIME",
		"null", "function",
		"true", "false",
		"var", "auto",
		"if", "then", "else", "elseif", "end",
		"for", "in", "to", "step", "while", "do",
		"break", "continue", "return",
		"func", "arg", "as", "const",
		"and", "or", "not",
		"bool", "int", "float", "string", "time", "point",
		"array", "list", "set", "dict", "type", "range",
		">", "<", ">=", "<=", "!=", "==", "~=",
		"+", "-", "*", "/", "%", "**",
		"&", "|", "!", "^", ">>", "<<",
		"=", "+=", "-=", "*=", "/=", "%=", "**=", "++", "--",
		"?", ":", "??", "?=", ".", "?.", "?[", "...", "***", "..",
		"(", ")", "[", "]", "{", "}", ",", ";"
	};
	if ( (type >= 0) && (type < XL_TOK_COUNT) ) {
		return names[type];
	}
	return "UNKNOWN";
}



/* 软关键字（类型别名）检测 - Parser 用 IDENT 匹配后查表 */
static const char* xl_soft_type_names[] = {
	"i8", "u8", "i16", "u16", "i32", "u32", "i64", "u64",
	"f32", "f64",
	NULL
};

static inline int xlIsSoftTypeName( const char* pszText, int iLen )
{
	for ( int i = 0; xl_soft_type_names[i]; i++ ) {
		if ( ((int)strlen(xl_soft_type_names[i]) == iLen) &&
		     (memcmp(pszText, xl_soft_type_names[i], iLen) == 0) ) {
			return 1;
		}
	}
	return 0;
}

/* 检查 Token 是否为类型名（硬关键字 Token 或软关键字 IDENT） */
static inline int xlIsTypeToken( XL_Token *pToken )
{
	if ( (pToken->type >= XL_KW_BOOL) && (pToken->type <= XL_KW_RANGE) ) {
		return 1;
	}
	if ( pToken->type == XL_KW_VAR ) return 1;
	if ( pToken->type == XL_KW_AUTO ) return 1;
	if ( pToken->type == XL_TOK_IDENT ) {
		return xlIsSoftTypeName(pToken->text, (int)pToken->length);
	}
	return 0;
}


#endif /* XL_TOKENS_H */
