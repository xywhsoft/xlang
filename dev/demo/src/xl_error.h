/**
 * xlang - 错误处理模块
 * 
 * 定义错误结构、错误码和错误报告函数
 */

#ifndef XL_ERROR_H
#define XL_ERROR_H

#include "xl_common.h"




// ============================================================================
// 错误码定义
// ============================================================================

typedef enum XL_ErrorCode {
	// ==================== 无错误 ====================
	XL_ERR_NONE = 0,

	// ==================== 词法错误 (1000-1999) ====================
	XL_ERR_LEX_UNKNOWN_CHAR = 1000,     // 未知字符
	XL_ERR_LEX_INVALID_NUMBER,          // 无效数字格式
	XL_ERR_LEX_UNCLOSED_STRING,         // 未闭合的字符串
	XL_ERR_LEX_UNCLOSED_COMMENT,        // 未闭合的注释
	XL_ERR_LEX_INVALID_ESCAPE,          // 无效的转义序列
	XL_ERR_LEX_INVALID_HEX,             // 无效的十六进制数
	XL_ERR_LEX_INVALID_OCT,             // 无效的八进制数
	XL_ERR_LEX_INVALID_BIN,             // 无效的二进制数
	XL_ERR_LEX_INVALID_TIME,            // 无效的时间字面量
	XL_ERR_LEX_NUMBER_OVERFLOW,         // 数字溢出

	// ==================== 语法错误 (2000-2999) ====================
	XL_ERR_SYN_UNEXPECTED_TOKEN = 2000, // 意外的 Token
	XL_ERR_SYN_EXPECTED_TOKEN,          // 期望某个 Token
	XL_ERR_SYN_MISSING_END,             // 缺少 end
	XL_ERR_SYN_MISSING_PAREN,           // 缺少括号
	XL_ERR_SYN_MISSING_BRACKET,         // 缺少方括号
	XL_ERR_SYN_MISSING_BRACE,           // 缺少花括号
	XL_ERR_SYN_INVALID_EXPR,            // 无效的表达式
	XL_ERR_SYN_INVALID_STMT,            // 无效的语句
	XL_ERR_SYN_INVALID_DECL,            // 无效的声明
	XL_ERR_SYN_INVALID_TYPE,            // 无效的类型

	// ==================== 语义错误 - 名称 (3000-3999) ====================
	XL_ERR_SEM_UNDEFINED = 3000,        // 未定义的标识符
	XL_ERR_SEM_REDEFINED,               // 重复定义
	XL_ERR_SEM_INVALID_SELF,            // 无效的 self 使用
	XL_ERR_SEM_INVALID_SUPER,           // 无效的 super 使用
	XL_ERR_SEM_INVALID_BREAK,           // break 不在循环中
	XL_ERR_SEM_INVALID_CONTINUE,        // continue 不在循环中
	XL_ERR_SEM_INVALID_RETURN,          // return 不在函数中
	XL_ERR_SEM_NOT_CALLABLE,            // 不可调用
	XL_ERR_SEM_NOT_INDEXABLE,           // 不可索引
	XL_ERR_SEM_NOT_MEMBER,              // 无此成员

	// ==================== 语义错误 - 类型 (4000-4999) ====================
	XL_ERR_SEM_TYPE_MISMATCH = 4000,    // 类型不匹配
	XL_ERR_SEM_INVALID_CAST,            // 无效的类型转换
	XL_ERR_SEM_INVALID_OP,              // 无效的运算符
	XL_ERR_SEM_PARAM_COUNT,             // 参数数量不匹配
	XL_ERR_SEM_PARAM_TYPE,              // 参数类型不匹配
	XL_ERR_SEM_RETURN_TYPE,             // 返回类型不匹配
	XL_ERR_SEM_ASSIGN_CONST,            // 不能给常量赋值
	XL_ERR_SEM_ASSIGN_READONLY,         // 不能给只读属性赋值

	// ==================== 语义错误 - 控制流 (5000-5999) ====================
	XL_ERR_SEM_UNREACHABLE = 5000,      // 不可达代码
	XL_ERR_SEM_MISSING_RETURN,          // 缺少返回语句
	XL_ERR_SEM_INVALID_THROW,           // 无效的 throw
	XL_ERR_SEM_BREAK_LEVEL,             // break 层数超出

	// ==================== 代码生成错误 (6000-6999) ====================
	XL_ERR_GEN_INTERNAL = 6000,         // 内部错误
	XL_ERR_GEN_UNSUPPORTED,             // 不支持的特性
	XL_ERR_GEN_IO_ERROR,                // IO 错误

	// ==================== 运行时错误 (7000-7999) ====================
	XL_ERR_RT_VALUE = 7000,             // 值错误
	XL_ERR_RT_TYPE,                     // 类型错误
	XL_ERR_RT_INDEX,                    // 索引越界
	XL_ERR_RT_KEY,                      // 键不存在
	XL_ERR_RT_DIVZERO,                  // 除零错误
	XL_ERR_RT_OVERFLOW,                 // 溢出错误
	XL_ERR_RT_MEMORY,                   // 内存错误
	XL_ERR_RT_IO,                       // IO 错误
	XL_ERR_RT_ASSERTION,                // 断言失败

	XL_ERR_COUNT
} XL_ErrorCode;




// ============================================================================
// 错误结构
// ============================================================================

typedef struct XL_Error {
	XL_ErrorCode iCode;     // 错误码
	u32 iLine;              // 行号
	u32 iColumn;            // 列号
	str pszFile;            // 文件名（不拥有，不需要释放）
	str pszMessage;         // 错误消息（需要释放）
	str pszContext;         // 上下文（源码片段，需要释放）
} XL_Error;


typedef struct XL_ErrorList {
	u32 iCount;             // 错误数量
	u32 iCapacity;          // 容量
	XL_Error *arrErrors;    // 错误数组
	str pszFile;            // 当前文件名
} XL_ErrorList;




// ============================================================================
// 错误分类名称
// ============================================================================

static inline str xlErrorCategoryName( XL_ErrorCode iCode )
{
	if ( iCode == XL_ERR_NONE ) return (str)"无错误";
	if ( (iCode >= 1000) && (iCode < 2000) ) return (str)"词法错误";
	if ( (iCode >= 2000) && (iCode < 3000) ) return (str)"语法错误";
	if ( (iCode >= 3000) && (iCode < 4000) ) return (str)"语义错误(名称)";
	if ( (iCode >= 4000) && (iCode < 5000) ) return (str)"语义错误(类型)";
	if ( (iCode >= 5000) && (iCode < 6000) ) return (str)"语义错误(控制流)";
	if ( (iCode >= 6000) && (iCode < 7000) ) return (str)"代码生成错误";
	if ( (iCode >= 7000) && (iCode < 8000) ) return (str)"运行时错误";
	return (str)"未知错误";
}




// ============================================================================
// 错误列表操作
// ============================================================================

// 创建错误列表
static inline XL_ErrorList* xlErrorListCreate( str pszFile )
{
	XL_ErrorList *pList = (XL_ErrorList*)malloc(sizeof(XL_ErrorList));
	if ( !pList ) return NULL;

	pList->iCount = 0;
	pList->iCapacity = 16;
	pList->arrErrors = (XL_Error*)malloc(sizeof(XL_Error) * pList->iCapacity);
	pList->pszFile = pszFile;

	if ( !pList->arrErrors ) {
		free(pList);
		return NULL;
	}

	return pList;
}


// 释放错误列表
static inline void xlErrorListDestroy( XL_ErrorList *pList )
{
	if ( !pList ) return;

	for ( u32 i = 0; i < pList->iCount; i++ ) {
		XL_SAFE_FREE(pList->arrErrors[i].pszMessage);
		XL_SAFE_FREE(pList->arrErrors[i].pszContext);
	}

	XL_SAFE_FREE(pList->arrErrors);
	free(pList);
}


// 添加错误
static inline void xlErrorListAdd( XL_ErrorList *pList, XL_ErrorCode iCode, 
                                   u32 iLine, u32 iColumn, str pszMessage )
{
	if ( !pList ) return;

	// 扩容
	if ( pList->iCount >= pList->iCapacity ) {
		pList->iCapacity *= 2;
		XL_Error *arrNew = (XL_Error*)realloc(pList->arrErrors, 
		                                       sizeof(XL_Error) * pList->iCapacity);
		if ( !arrNew ) return;
		pList->arrErrors = arrNew;
	}

	XL_Error *pErr = &pList->arrErrors[pList->iCount++];
	pErr->iCode = iCode;
	pErr->iLine = iLine;
	pErr->iColumn = iColumn;
	pErr->pszFile = pList->pszFile;
	pErr->pszMessage = pszMessage ? xlStrDup(pszMessage, 0) : NULL;
	pErr->pszContext = NULL;
}


// 添加错误（带格式化消息）
static inline void xlErrorListAddFmt( XL_ErrorList *pList, XL_ErrorCode iCode,
                                      u32 iLine, u32 iColumn, str pszFmt, ... )
{
	if ( !pList || !pszFmt ) return;

	char szBuf[1024];
	va_list args;
	va_start(args, pszFmt);
	vsnprintf(szBuf, sizeof(szBuf), (char*)pszFmt, args);
	va_end(args);

	xlErrorListAdd(pList, iCode, iLine, iColumn, (str)szBuf);
}


// 检查是否有错误
static inline bool xlErrorListHasError( XL_ErrorList *pList )
{
	return pList && (pList->iCount > 0);
}


// 获取错误数量
static inline u32 xlErrorListCount( XL_ErrorList *pList )
{
	return pList ? pList->iCount : 0;
}




// ============================================================================
// 错误打印
// ============================================================================

// 打印单个错误
static inline void xlErrorPrint( XL_Error *pErr )
{
	if ( !pErr ) return;

	fprintf(stderr, "[%s] %s:%u:%u: %s\n",
	        (char*)xlErrorCategoryName(pErr->iCode),
	        pErr->pszFile ? (char*)pErr->pszFile : "<unknown>",
	        pErr->iLine,
	        pErr->iColumn,
	        pErr->pszMessage ? (char*)pErr->pszMessage : "");

	if ( pErr->pszContext ) {
		fprintf(stderr, "    %s\n", (char*)pErr->pszContext);
	}
}


// 打印所有错误
static inline void xlErrorListPrint( XL_ErrorList *pList )
{
	if ( !pList ) return;

	for ( u32 i = 0; i < pList->iCount; i++ ) {
		xlErrorPrint(&pList->arrErrors[i]);
	}

	if ( pList->iCount > 0 ) {
		fprintf(stderr, "\n共 %u 个错误\n", pList->iCount);
	}
}


#endif // XL_ERROR_H
