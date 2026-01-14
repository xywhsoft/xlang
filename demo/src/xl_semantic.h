/**
 * xlang - 语义分析模块
 * 
 * 负责符号解析、类型检查和闭包分析
 */

#ifndef XL_SEMANTIC_H
#define XL_SEMANTIC_H

#include "xl_common.h"
#include "xl_error.h"
#include "xl_ast.h"




// ============================================================================
// 类型系统
// ============================================================================

// 基础类型 ID
typedef enum XL_TypeKind {
	XL_TYPE_UNKNOWN = 0,    // 未知类型
	XL_TYPE_VOID,           // void
	XL_TYPE_NULL,           // null
	XL_TYPE_BOOL,           // bool
	XL_TYPE_INT,            // int (默认 64 位)
	XL_TYPE_INT8,           // int8
	XL_TYPE_INT16,          // int16
	XL_TYPE_INT32,          // int32
	XL_TYPE_INT64,          // int64
	XL_TYPE_UINT8,          // uint8
	XL_TYPE_UINT16,         // uint16
	XL_TYPE_UINT32,         // uint32
	XL_TYPE_UINT64,         // uint64
	XL_TYPE_FLOAT,          // float (默认 64 位)
	XL_TYPE_FLOAT32,        // float32
	XL_TYPE_FLOAT64,        // float64
	XL_TYPE_STRING,         // string
	XL_TYPE_ARRAY,          // array
	XL_TYPE_TABLE,          // table
	XL_TYPE_FUNCTION,       // function
	XL_TYPE_CLASS,          // class
	XL_TYPE_ANY             // var/auto (动态类型)
} XL_TypeKind;


// 类型信息
typedef struct XL_Type {
	XL_TypeKind kind;           // 类型种类
	str pszName;                // 类型名（用户定义类型）
	struct XL_Type *pElemType;  // 元素类型（数组）
	struct XL_Type *pRetType;   // 返回类型（函数）
	bool bNullable;             // 是否可空
} XL_Type;


// 创建基础类型
static inline XL_Type* xlTypeCreate( XL_TypeKind kind )
{
	XL_Type *pType = (XL_Type*)malloc(sizeof(XL_Type));
	if ( pType ) {
		memset(pType, 0, sizeof(XL_Type));
		pType->kind = kind;
	}
	return pType;
}


// 销毁类型
static inline void xlTypeDestroy( XL_Type *pType )
{
	if ( pType ) {
		if ( pType->pElemType ) xlTypeDestroy(pType->pElemType);
		if ( pType->pRetType ) xlTypeDestroy(pType->pRetType);
		XL_SAFE_FREE(pType->pszName);
		free(pType);
	}
}


// 类型名称查询
static inline str xlTypeKindName( XL_TypeKind kind )
{
	switch ( kind ) {
		case XL_TYPE_VOID:      return (str)"void";
		case XL_TYPE_NULL:      return (str)"null";
		case XL_TYPE_BOOL:      return (str)"bool";
		case XL_TYPE_INT:       return (str)"int";
		case XL_TYPE_INT8:      return (str)"int8";
		case XL_TYPE_INT16:     return (str)"int16";
		case XL_TYPE_INT32:     return (str)"int32";
		case XL_TYPE_INT64:     return (str)"int64";
		case XL_TYPE_UINT8:     return (str)"uint8";
		case XL_TYPE_UINT16:    return (str)"uint16";
		case XL_TYPE_UINT32:    return (str)"uint32";
		case XL_TYPE_UINT64:    return (str)"uint64";
		case XL_TYPE_FLOAT:     return (str)"float";
		case XL_TYPE_FLOAT32:   return (str)"float32";
		case XL_TYPE_FLOAT64:   return (str)"float64";
		case XL_TYPE_STRING:    return (str)"string";
		case XL_TYPE_ARRAY:     return (str)"array";
		case XL_TYPE_TABLE:     return (str)"table";
		case XL_TYPE_FUNCTION:  return (str)"function";
		case XL_TYPE_CLASS:     return (str)"class";
		case XL_TYPE_ANY:       return (str)"any";
		default:                return (str)"unknown";
	}
}


// 从类型名解析类型种类
static inline XL_TypeKind xlTypeKindFromName( str pszName )
{
	if ( !pszName ) return XL_TYPE_UNKNOWN;

	if ( strcmp((char*)pszName, "void") == 0 )    return XL_TYPE_VOID;
	if ( strcmp((char*)pszName, "bool") == 0 )    return XL_TYPE_BOOL;
	if ( strcmp((char*)pszName, "int") == 0 )     return XL_TYPE_INT;
	if ( strcmp((char*)pszName, "int8") == 0 )    return XL_TYPE_INT8;
	if ( strcmp((char*)pszName, "int16") == 0 )   return XL_TYPE_INT16;
	if ( strcmp((char*)pszName, "int32") == 0 )   return XL_TYPE_INT32;
	if ( strcmp((char*)pszName, "int64") == 0 )   return XL_TYPE_INT64;
	if ( strcmp((char*)pszName, "uint8") == 0 )   return XL_TYPE_UINT8;
	if ( strcmp((char*)pszName, "uint16") == 0 )  return XL_TYPE_UINT16;
	if ( strcmp((char*)pszName, "uint32") == 0 )  return XL_TYPE_UINT32;
	if ( strcmp((char*)pszName, "uint64") == 0 )  return XL_TYPE_UINT64;
	if ( strcmp((char*)pszName, "float") == 0 )   return XL_TYPE_FLOAT;
	if ( strcmp((char*)pszName, "float32") == 0 ) return XL_TYPE_FLOAT32;
	if ( strcmp((char*)pszName, "float64") == 0 ) return XL_TYPE_FLOAT64;
	if ( strcmp((char*)pszName, "string") == 0 )  return XL_TYPE_STRING;
	if ( strcmp((char*)pszName, "array") == 0 )   return XL_TYPE_ARRAY;
	if ( strcmp((char*)pszName, "table") == 0 )   return XL_TYPE_TABLE;
	if ( strcmp((char*)pszName, "var") == 0 )     return XL_TYPE_ANY;
	if ( strcmp((char*)pszName, "auto") == 0 )    return XL_TYPE_ANY;

	return XL_TYPE_UNKNOWN;
}




// ============================================================================
// 符号表
// ============================================================================

// 符号种类
typedef enum XL_SymbolKind {
	XL_SYM_VAR = 0,         // 变量
	XL_SYM_CONST,           // 常量
	XL_SYM_PARAM,           // 函数参数
	XL_SYM_FUNCTION,        // 函数
	XL_SYM_CLASS,           // 类
	XL_SYM_FIELD,           // 类字段
	XL_SYM_METHOD           // 类方法
} XL_SymbolKind;


// 符号定义
typedef struct XL_Symbol {
	str pszName;                // 符号名
	XL_SymbolKind kind;         // 符号种类
	XL_Type *pType;             // 类型信息
	xvalue xDeclNode;           // 声明节点（AST）
	u32 iLine;                  // 声明行号
	u32 iCol;                   // 声明列号
	bool bCaptured;             // 是否被闭包捕获
	struct XL_Symbol *pNext;    // 链表下一个
} XL_Symbol;


// 作用域
typedef struct XL_Scope {
	XL_Symbol *pSymbols;        // 符号链表
	struct XL_Scope *pParent;   // 父作用域
	int iLevel;                 // 嵌套层级
	bool bIsFunction;           // 是否为函数作用域
	bool bIsLoop;               // 是否为循环作用域
} XL_Scope;


// 语义分析器
typedef struct XL_Semantic {
	XL_Scope *pCurrentScope;    // 当前作用域
	XL_Scope *pGlobalScope;     // 全局作用域
	XL_ErrorList *pErrors;      // 错误列表
	bool bHadError;             // 是否有错误
	xvalue xCurrentFunc;        // 当前函数节点
	xvalue xCurrentClass;       // 当前类节点
} XL_Semantic;




// ============================================================================
// 符号表操作
// ============================================================================

// 创建符号
static inline XL_Symbol* xlSymbolCreate( str pszName, XL_SymbolKind kind, u32 iLine, u32 iCol )
{
	XL_Symbol *pSym = (XL_Symbol*)malloc(sizeof(XL_Symbol));
	if ( pSym ) {
		memset(pSym, 0, sizeof(XL_Symbol));
		pSym->pszName = xlStrDup(pszName, 0);
		pSym->kind = kind;
		pSym->iLine = iLine;
		pSym->iCol = iCol;
	}
	return pSym;
}


// 销毁符号
static inline void xlSymbolDestroy( XL_Symbol *pSym )
{
	if ( pSym ) {
		XL_SAFE_FREE(pSym->pszName);
		if ( pSym->pType ) xlTypeDestroy(pSym->pType);
		free(pSym);
	}
}


// 创建作用域
static inline XL_Scope* xlScopeCreate( XL_Scope *pParent )
{
	XL_Scope *pScope = (XL_Scope*)malloc(sizeof(XL_Scope));
	if ( pScope ) {
		memset(pScope, 0, sizeof(XL_Scope));
		pScope->pParent = pParent;
		pScope->iLevel = pParent ? pParent->iLevel + 1 : 0;
	}
	return pScope;
}


// 销毁作用域
static inline void xlScopeDestroy( XL_Scope *pScope )
{
	if ( pScope ) {
		// 销毁所有符号
		XL_Symbol *pSym = pScope->pSymbols;
		while ( pSym ) {
			XL_Symbol *pNext = pSym->pNext;
			xlSymbolDestroy(pSym);
			pSym = pNext;
		}
		free(pScope);
	}
}


// 在当前作用域定义符号
static inline bool xlScopeDefine( XL_Scope *pScope, XL_Symbol *pSym )
{
	if ( !pScope || !pSym ) return false;

	// 检查重复定义
	XL_Symbol *pExist = pScope->pSymbols;
	while ( pExist ) {
		if ( strcmp((char*)pExist->pszName, (char*)pSym->pszName) == 0 ) {
			return false;  // 重复定义
		}
		pExist = pExist->pNext;
	}

	// 添加到链表头部
	pSym->pNext = pScope->pSymbols;
	pScope->pSymbols = pSym;
	return true;
}


// 在当前作用域查找符号（不递归）
static inline XL_Symbol* xlScopeLookupLocal( XL_Scope *pScope, str pszName )
{
	if ( !pScope || !pszName ) return NULL;

	XL_Symbol *pSym = pScope->pSymbols;
	while ( pSym ) {
		if ( strcmp((char*)pSym->pszName, (char*)pszName) == 0 ) {
			return pSym;
		}
		pSym = pSym->pNext;
	}
	return NULL;
}


// 递归查找符号
static inline XL_Symbol* xlScopeLookup( XL_Scope *pScope, str pszName )
{
	while ( pScope ) {
		XL_Symbol *pSym = xlScopeLookupLocal(pScope, pszName);
		if ( pSym ) return pSym;
		pScope = pScope->pParent;
	}
	return NULL;
}




// ============================================================================
// 语义分析器
// ============================================================================

// 创建语义分析器
static inline XL_Semantic* xlSemanticCreate( XL_ErrorList *pErrors )
{
	XL_Semantic *pSem = (XL_Semantic*)malloc(sizeof(XL_Semantic));
	if ( pSem ) {
		memset(pSem, 0, sizeof(XL_Semantic));
		pSem->pErrors = pErrors;
		pSem->pGlobalScope = xlScopeCreate(NULL);
		pSem->pCurrentScope = pSem->pGlobalScope;

		// 添加内置函数
		XL_Symbol *pPrint = xlSymbolCreate((str)"print", XL_SYM_FUNCTION, 0, 0);
		pPrint->pType = xlTypeCreate(XL_TYPE_FUNCTION);
		xlScopeDefine(pSem->pGlobalScope, pPrint);

		XL_Symbol *pPrintln = xlSymbolCreate((str)"println", XL_SYM_FUNCTION, 0, 0);
		pPrintln->pType = xlTypeCreate(XL_TYPE_FUNCTION);
		xlScopeDefine(pSem->pGlobalScope, pPrintln);

		XL_Symbol *pLen = xlSymbolCreate((str)"len", XL_SYM_FUNCTION, 0, 0);
		pLen->pType = xlTypeCreate(XL_TYPE_FUNCTION);
		xlScopeDefine(pSem->pGlobalScope, pLen);

		XL_Symbol *pType = xlSymbolCreate((str)"type", XL_SYM_FUNCTION, 0, 0);
		pType->pType = xlTypeCreate(XL_TYPE_FUNCTION);
		xlScopeDefine(pSem->pGlobalScope, pType);

		// typeof 内置函数 - 返回类型整数值
		XL_Symbol *pTypeof = xlSymbolCreate((str)"typeof", XL_SYM_FUNCTION, 0, 0);
		pTypeof->pType = xlTypeCreate(XL_TYPE_FUNCTION);
		xlScopeDefine(pSem->pGlobalScope, pTypeof);

		// range 内置函数
		XL_Symbol *pRange = xlSymbolCreate((str)"range", XL_SYM_FUNCTION, 0, 0);
		pRange->pType = xlTypeCreate(XL_TYPE_FUNCTION);
		xlScopeDefine(pSem->pGlobalScope, pRange);

		// input 内置函数
		XL_Symbol *pInput = xlSymbolCreate((str)"input", XL_SYM_FUNCTION, 0, 0);
		pInput->pType = xlTypeCreate(XL_TYPE_FUNCTION);
		xlScopeDefine(pSem->pGlobalScope, pInput);

		// abs 内置函数
		XL_Symbol *pAbs = xlSymbolCreate((str)"abs", XL_SYM_FUNCTION, 0, 0);
		pAbs->pType = xlTypeCreate(XL_TYPE_FUNCTION);
		xlScopeDefine(pSem->pGlobalScope, pAbs);

		// min 内置函数
		XL_Symbol *pMin = xlSymbolCreate((str)"min", XL_SYM_FUNCTION, 0, 0);
		pMin->pType = xlTypeCreate(XL_TYPE_FUNCTION);
		xlScopeDefine(pSem->pGlobalScope, pMin);

		// max 内置函数
		XL_Symbol *pMax = xlSymbolCreate((str)"max", XL_SYM_FUNCTION, 0, 0);
		pMax->pType = xlTypeCreate(XL_TYPE_FUNCTION);
		xlScopeDefine(pSem->pGlobalScope, pMax);

		// int 类型转换函数
		XL_Symbol *pInt = xlSymbolCreate((str)"int", XL_SYM_FUNCTION, 0, 0);
		pInt->pType = xlTypeCreate(XL_TYPE_FUNCTION);
		xlScopeDefine(pSem->pGlobalScope, pInt);

		// float 类型转换函数
		XL_Symbol *pFloat = xlSymbolCreate((str)"float", XL_SYM_FUNCTION, 0, 0);
		pFloat->pType = xlTypeCreate(XL_TYPE_FUNCTION);
		xlScopeDefine(pSem->pGlobalScope, pFloat);

		// string 类型转换函数
		XL_Symbol *pString = xlSymbolCreate((str)"string", XL_SYM_FUNCTION, 0, 0);
		pString->pType = xlTypeCreate(XL_TYPE_FUNCTION);
		xlScopeDefine(pSem->pGlobalScope, pString);

		// bool 类型转换函数
		XL_Symbol *pBool = xlSymbolCreate((str)"bool", XL_SYM_FUNCTION, 0, 0);
		pBool->pType = xlTypeCreate(XL_TYPE_FUNCTION);
		xlScopeDefine(pSem->pGlobalScope, pBool);

		// list 创建函数
		XL_Symbol *pList = xlSymbolCreate((str)"list", XL_SYM_FUNCTION, 0, 0);
		pList->pType = xlTypeCreate(XL_TYPE_FUNCTION);
		xlScopeDefine(pSem->pGlobalScope, pList);

		// set 创建函数
		XL_Symbol *pSet = xlSymbolCreate((str)"set", XL_SYM_FUNCTION, 0, 0);
		pSet->pType = xlTypeCreate(XL_TYPE_FUNCTION);
		xlScopeDefine(pSem->pGlobalScope, pSet);

		// datetime 创建函数
		XL_Symbol *pDateTime = xlSymbolCreate((str)"datetime", XL_SYM_FUNCTION, 0, 0);
		pDateTime->pType = xlTypeCreate(XL_TYPE_FUNCTION);
		xlScopeDefine(pSem->pGlobalScope, pDateTime);

		// now 获取当前时间
		XL_Symbol *pNow = xlSymbolCreate((str)"now", XL_SYM_FUNCTION, 0, 0);
		pNow->pType = xlTypeCreate(XL_TYPE_FUNCTION);
		xlScopeDefine(pSem->pGlobalScope, pNow);
	}
	return pSem;
}


// 销毁语义分析器
static inline void xlSemanticDestroy( XL_Semantic *pSem )
{
	if ( pSem ) {
		// 销毁所有作用域
		XL_Scope *pScope = pSem->pCurrentScope;
		while ( pScope ) {
			XL_Scope *pParent = pScope->pParent;
			xlScopeDestroy(pScope);
			pScope = pParent;
		}
		free(pSem);
	}
}


// 进入新作用域
static inline void xlSemanticEnterScope( XL_Semantic *pSem, bool bIsFunction, bool bIsLoop )
{
	XL_Scope *pScope = xlScopeCreate(pSem->pCurrentScope);
	if ( pScope ) {
		pScope->bIsFunction = bIsFunction;
		pScope->bIsLoop = bIsLoop;
		pSem->pCurrentScope = pScope;
	}
}


// 离开当前作用域
static inline void xlSemanticLeaveScope( XL_Semantic *pSem )
{
	if ( pSem->pCurrentScope && pSem->pCurrentScope->pParent ) {
		XL_Scope *pOld = pSem->pCurrentScope;
		pSem->pCurrentScope = pOld->pParent;
		xlScopeDestroy(pOld);
	}
}


// 报告语义错误
static inline void xlSemanticError( XL_Semantic *pSem, u32 iLine, u32 iCol, str pszFmt, ... )
{
	pSem->bHadError = true;
	if ( pSem->pErrors ) {
		va_list args;
		va_start(args, pszFmt);
		// 简化错误添加
		char szMsg[512];
		vsnprintf(szMsg, sizeof(szMsg), (char*)pszFmt, args);
		va_end(args);
		xlErrorListAdd(pSem->pErrors, XL_ERR_SEM_UNDEFINED, iLine, iCol, (str)szMsg);
	}
}




// ============================================================================
// 类型检查辅助函数
// ============================================================================

// 检查类型是否兼容
static inline bool xlTypeCompatible( XL_Type *pLeft, XL_Type *pRight )
{
	if ( !pLeft || !pRight ) return true;  // unknown 类型总是兼容

	// any 类型与任何类型兼容
	if ( (pLeft->kind == XL_TYPE_ANY) || (pRight->kind == XL_TYPE_ANY) ) {
		return true;
	}

	// 相同类型
	if ( pLeft->kind == pRight->kind ) {
		return true;
	}

	// 数值类型之间可以隐式转换
	bool bLeftNum = (pLeft->kind >= XL_TYPE_INT) && (pLeft->kind <= XL_TYPE_FLOAT64);
	bool bRightNum = (pRight->kind >= XL_TYPE_INT) && (pRight->kind <= XL_TYPE_FLOAT64);
	if ( bLeftNum && bRightNum ) {
		return true;
	}

	// null 可以赋值给可空类型
	if ( (pRight->kind == XL_TYPE_NULL) && pLeft->bNullable ) {
		return true;
	}

	return false;
}


// 从 AST TypeRef 节点解析类型
static inline XL_Type* xlTypeFromAst( xvalue xTypeRef )
{
	if ( !xTypeRef ) return xlTypeCreate(XL_TYPE_ANY);

	str pszName = xlAstGetStr(xTypeRef, (str)"name");
	XL_TypeKind kind = xlTypeKindFromName(pszName);

	XL_Type *pType = xlTypeCreate(kind);
	if ( pType ) {
		if ( kind == XL_TYPE_UNKNOWN ) {
			// 用户定义类型
			pType->pszName = xlStrDup(pszName, 0);
			pType->kind = XL_TYPE_CLASS;
		}

		// 检查数组和可空
		xvalue xIsArray = xvoTableGetValue(xTypeRef, (str)"isArray", 0);
		xvalue xIsNullable = xvoTableGetValue(xTypeRef, (str)"isNullable", 0);

		if ( xvoGetBool(xIsArray) ) {
			XL_Type *pElem = pType;
			pType = xlTypeCreate(XL_TYPE_ARRAY);
			pType->pElemType = pElem;
		}

		pType->bNullable = xvoGetBool(xIsNullable);
	}

	return pType;
}




// ============================================================================
// AST 遍历和语义分析
// ============================================================================

// 前向声明
static void xlSemanticAnalyzeExpr( XL_Semantic *pSem, xvalue xExpr );
static void xlSemanticAnalyzeStmt( XL_Semantic *pSem, xvalue xStmt );
static void xlSemanticAnalyzeDecl( XL_Semantic *pSem, xvalue xDecl );


// 分析表达式
static void xlSemanticAnalyzeExpr( XL_Semantic *pSem, xvalue xExpr )
{
	if ( !xExpr ) return;

	str pszKind = xlAstGetKind(xExpr);
	if ( !pszKind ) return;

	i64 iLine = xlAstGetInt(xExpr, (str)"line");
	i64 iCol = xlAstGetInt(xExpr, (str)"col");

	// 标识符引用 - 检查是否已定义
	if ( strcmp((char*)pszKind, XL_AST_IDENT) == 0 ) {
		str pszName = xlAstGetStr(xExpr, (str)"name");
		XL_Symbol *pSym = xlScopeLookup(pSem->pCurrentScope, pszName);
		if ( !pSym ) {
			xlSemanticError(pSem, (u32)iLine, (u32)iCol, (str)"未定义的标识符: %s", (char*)pszName);
		} else {
			// 标记符号引用（用于闭包分析）
			xlAstSetStr(xExpr, (str)"_sym", pSym->pszName);
		}
		return;
	}

	// 二元表达式
	if ( strcmp((char*)pszKind, XL_AST_BINARY_EXPR) == 0 ) {
		xlSemanticAnalyzeExpr(pSem, xlAstGetNode(xExpr, (str)"left"));
		xlSemanticAnalyzeExpr(pSem, xlAstGetNode(xExpr, (str)"right"));
		return;
	}

	// 一元表达式
	if ( strcmp((char*)pszKind, XL_AST_UNARY_EXPR) == 0 ) {
		xlSemanticAnalyzeExpr(pSem, xlAstGetNode(xExpr, (str)"operand"));
		return;
	}

	// 赋值表达式
	if ( strcmp((char*)pszKind, XL_AST_ASSIGN_EXPR) == 0 ) {
		xvalue xTarget = xlAstGetNode(xExpr, (str)"target");
		xlSemanticAnalyzeExpr(pSem, xTarget);
		xlSemanticAnalyzeExpr(pSem, xlAstGetNode(xExpr, (str)"value"));

		// 检查赋值目标是否为常量
		str pszTargetKind = xlAstGetKind(xTarget);
		if ( (pszTargetKind != NULL) && (strcmp((char*)pszTargetKind, XL_AST_IDENT) == 0) ) {
			str pszName = xlAstGetStr(xTarget, (str)"name");
			XL_Symbol *pSym = xlScopeLookup(pSem->pCurrentScope, pszName);
			if ( (pSym != NULL) && (pSym->kind == XL_SYM_CONST) ) {
				xlSemanticError(pSem, (u32)iLine, (u32)iCol, (str)"不能对常量 '%s' 重新赋值", (char*)pszName);
			}
		}
		return;
	}

	// 函数调用
	if ( strcmp((char*)pszKind, XL_AST_CALL_EXPR) == 0 ) {
		xlSemanticAnalyzeExpr(pSem, xlAstGetNode(xExpr, (str)"callee"));
		xvalue xArgs = xlAstGetNode(xExpr, (str)"args");
		int iLen = xlAstArrayLen(xArgs);
		for ( int i = 0; i < iLen; i++ ) {
			xlSemanticAnalyzeExpr(pSem, xlAstArrayGet(xArgs, i));
		}
		return;
	}

	// 索引表达式
	if ( strcmp((char*)pszKind, XL_AST_INDEX_EXPR) == 0 ) {
		xlSemanticAnalyzeExpr(pSem, xlAstGetNode(xExpr, (str)"object"));
		xlSemanticAnalyzeExpr(pSem, xlAstGetNode(xExpr, (str)"index"));
		return;
	}

	// 切片表达式
	if ( strcmp((char*)pszKind, XL_AST_SLICE_EXPR) == 0 ) {
		xlSemanticAnalyzeExpr(pSem, xlAstGetNode(xExpr, (str)"object"));
		xlSemanticAnalyzeExpr(pSem, xlAstGetNode(xExpr, (str)"start"));
		xlSemanticAnalyzeExpr(pSem, xlAstGetNode(xExpr, (str)"end"));
		return;
	}

	// 成员访问
	if ( strcmp((char*)pszKind, XL_AST_MEMBER_EXPR) == 0 ) {
		xlSemanticAnalyzeExpr(pSem, xlAstGetNode(xExpr, (str)"object"));
		return;
	}

	// 三元表达式
	if ( strcmp((char*)pszKind, XL_AST_TERNARY_EXPR) == 0 ) {
		xlSemanticAnalyzeExpr(pSem, xlAstGetNode(xExpr, (str)"cond"));
		xlSemanticAnalyzeExpr(pSem, xlAstGetNode(xExpr, (str)"then"));
		xlSemanticAnalyzeExpr(pSem, xlAstGetNode(xExpr, (str)"else"));
		return;
	}

	// 数组字面量
	if ( strcmp((char*)pszKind, XL_AST_ARRAY_LIT) == 0 ) {
		xvalue xElements = xlAstGetNode(xExpr, (str)"elements");
		int iLen = xlAstArrayLen(xElements);
		for ( int i = 0; i < iLen; i++ ) {
			xlSemanticAnalyzeExpr(pSem, xlAstArrayGet(xElements, i));
		}
		return;
	}

	// 表字面量
	if ( strcmp((char*)pszKind, XL_AST_TABLE_LIT) == 0 ) {
		xvalue xEntries = xlAstGetNode(xExpr, (str)"entries");
		int iLen = xlAstArrayLen(xEntries);
		for ( int i = 0; i < iLen; i++ ) {
			xvalue xEntry = xlAstArrayGet(xEntries, i);
			xlSemanticAnalyzeExpr(pSem, xlAstGetNode(xEntry, (str)"value"));
		}
		return;
	}

	// new 表达式
	if ( strcmp((char*)pszKind, XL_AST_NEW_EXPR) == 0 ) {
		// 检查类型是否存在
		xvalue xType = xlAstGetNode(xExpr, (str)"type");
		if ( xType ) {
			str pszTypeName = xlAstGetStr(xType, (str)"name");
			XL_TypeKind kind = xlTypeKindFromName(pszTypeName);
			if ( kind == XL_TYPE_UNKNOWN ) {
				// 用户定义类型 - 检查是否已定义
				XL_Symbol *pSym = xlScopeLookup(pSem->pCurrentScope, pszTypeName);
				if ( !pSym || (pSym->kind != XL_SYM_CLASS) ) {
					xlSemanticError(pSem, (u32)iLine, (u32)iCol, (str)"未定义的类型: %s", (char*)pszTypeName);
				}
			}
		}
		// 分析参数
		xvalue xArgs = xlAstGetNode(xExpr, (str)"args");
		int iLen = xlAstArrayLen(xArgs);
		for ( int i = 0; i < iLen; i++ ) {
			xlSemanticAnalyzeExpr(pSem, xlAstArrayGet(xArgs, i));
		}
		return;
	}

	// 字面量不需要进一步分析
}


// 分析语句
static void xlSemanticAnalyzeStmt( XL_Semantic *pSem, xvalue xStmt )
{
	if ( !xStmt ) return;

	str pszKind = xlAstGetKind(xStmt);
	if ( !pszKind ) return;

	i64 iLine = xlAstGetInt(xStmt, (str)"line");
	i64 iCol = xlAstGetInt(xStmt, (str)"col");

	// 变量声明
	if ( strcmp((char*)pszKind, XL_AST_VAR_DECL) == 0 ) {
		str pszName = xlAstGetStr(xStmt, (str)"name");

		// 检查重复定义
		if ( xlScopeLookupLocal(pSem->pCurrentScope, pszName) ) {
			xlSemanticError(pSem, (u32)iLine, (u32)iCol, (str)"变量重复定义: %s", (char*)pszName);
			return;
		}

		// 分析初始值
		xvalue xInit = xlAstGetNode(xStmt, (str)"init");
		if ( xInit ) {
			xlSemanticAnalyzeExpr(pSem, xInit);
		}

		// 添加符号
		xvalue xIsConst = xvoTableGetValue(xStmt, (str)"isConst", 0);
		XL_SymbolKind symKind = xvoGetBool(xIsConst) ? XL_SYM_CONST : XL_SYM_VAR;
		XL_Symbol *pSym = xlSymbolCreate(pszName, symKind, (u32)iLine, (u32)iCol);
		pSym->xDeclNode = xStmt;
		pSym->pType = xlTypeFromAst(xlAstGetNode(xStmt, (str)"type"));
		xlScopeDefine(pSem->pCurrentScope, pSym);
		return;
	}

	// 表达式语句
	if ( strcmp((char*)pszKind, XL_AST_EXPR_STMT) == 0 ) {
		xlSemanticAnalyzeExpr(pSem, xlAstGetNode(xStmt, (str)"expr"));
		return;
	}

	// 块语句
	if ( strcmp((char*)pszKind, XL_AST_BLOCK_STMT) == 0 ) {
		xvalue xStmts = xlAstGetNode(xStmt, (str)"stmts");
		int iLen = xlAstArrayLen(xStmts);
		for ( int i = 0; i < iLen; i++ ) {
			xlSemanticAnalyzeStmt(pSem, xlAstArrayGet(xStmts, i));
		}
		return;
	}

	// if 语句
	if ( strcmp((char*)pszKind, XL_AST_IF_STMT) == 0 ) {
		xlSemanticAnalyzeExpr(pSem, xlAstGetNode(xStmt, (str)"cond"));

		xlSemanticEnterScope(pSem, false, false);
		xlSemanticAnalyzeStmt(pSem, xlAstGetNode(xStmt, (str)"then"));
		xlSemanticLeaveScope(pSem);

		// elseif
		xvalue xElseIfs = xlAstGetNode(xStmt, (str)"elseifs");
		int iLen = xlAstArrayLen(xElseIfs);
		for ( int i = 0; i < iLen; i++ ) {
			xvalue xElseIf = xlAstArrayGet(xElseIfs, i);
			xlSemanticAnalyzeExpr(pSem, xlAstGetNode(xElseIf, (str)"cond"));
			xlSemanticEnterScope(pSem, false, false);
			xlSemanticAnalyzeStmt(pSem, xlAstGetNode(xElseIf, (str)"body"));
			xlSemanticLeaveScope(pSem);
		}

		// else
		xvalue xElse = xlAstGetNode(xStmt, (str)"else");
		if ( xElse ) {
			xlSemanticEnterScope(pSem, false, false);
			xlSemanticAnalyzeStmt(pSem, xElse);
			xlSemanticLeaveScope(pSem);
		}
		return;
	}

	// while 语句
	if ( strcmp((char*)pszKind, XL_AST_WHILE_STMT) == 0 ) {
		xlSemanticAnalyzeExpr(pSem, xlAstGetNode(xStmt, (str)"cond"));
		xlSemanticEnterScope(pSem, false, true);
		xlSemanticAnalyzeStmt(pSem, xlAstGetNode(xStmt, (str)"body"));
		xlSemanticLeaveScope(pSem);
		return;
	}

	// for 语句
	if ( strcmp((char*)pszKind, XL_AST_FOR_STMT) == 0 ) {
		xlSemanticEnterScope(pSem, false, true);

		// 定义循环变量
		str pszVar = xlAstGetStr(xStmt, (str)"var");
		if ( pszVar ) {
			XL_Symbol *pSym = xlSymbolCreate(pszVar, XL_SYM_VAR, (u32)iLine, (u32)iCol);
			pSym->pType = xlTypeCreate(XL_TYPE_INT);
			xlScopeDefine(pSem->pCurrentScope, pSym);
		}

		xlSemanticAnalyzeExpr(pSem, xlAstGetNode(xStmt, (str)"start"));
		xlSemanticAnalyzeExpr(pSem, xlAstGetNode(xStmt, (str)"end"));
		xlSemanticAnalyzeExpr(pSem, xlAstGetNode(xStmt, (str)"step"));
		xlSemanticAnalyzeStmt(pSem, xlAstGetNode(xStmt, (str)"body"));

		xlSemanticLeaveScope(pSem);
		return;
	}

	// for-in 语句
	if ( strcmp((char*)pszKind, XL_AST_FOR_IN_STMT) == 0 ) {
		// 先分析迭代器表达式（在外层作用域）
		xlSemanticAnalyzeExpr(pSem, xlAstGetNode(xStmt, (str)"iter"));

		xlSemanticEnterScope(pSem, false, true);

		// 定义循环变量
		str pszKey = xlAstGetStr(xStmt, (str)"key");
		str pszValue = xlAstGetStr(xStmt, (str)"value");

		if ( pszKey ) {
			XL_Symbol *pSym = xlSymbolCreate(pszKey, XL_SYM_VAR, (u32)iLine, (u32)iCol);
			pSym->pType = xlTypeCreate(XL_TYPE_ANY);
			xlScopeDefine(pSem->pCurrentScope, pSym);
		}

		if ( pszValue ) {
			XL_Symbol *pSym = xlSymbolCreate(pszValue, XL_SYM_VAR, (u32)iLine, (u32)iCol);
			pSym->pType = xlTypeCreate(XL_TYPE_ANY);
			xlScopeDefine(pSem->pCurrentScope, pSym);
		}

		xlSemanticAnalyzeStmt(pSem, xlAstGetNode(xStmt, (str)"body"));
		xlSemanticLeaveScope(pSem);
		return;
	}

	// return 语句
	if ( strcmp((char*)pszKind, XL_AST_RETURN_STMT) == 0 ) {
		xvalue xValues = xlAstGetNode(xStmt, (str)"values");
		int iLen = xlAstArrayLen(xValues);
		for ( int i = 0; i < iLen; i++ ) {
			xlSemanticAnalyzeExpr(pSem, xlAstArrayGet(xValues, i));
		}
		return;
	}

	// break/continue
	if ( (strcmp((char*)pszKind, XL_AST_BREAK_STMT) == 0) ||
	     (strcmp((char*)pszKind, XL_AST_CONTINUE_STMT) == 0) ) {
		// 检查是否在循环中
		XL_Scope *pScope = pSem->pCurrentScope;
		bool bInLoop = false;
		while ( pScope ) {
			if ( pScope->bIsLoop ) {
				bInLoop = true;
				break;
			}
			pScope = pScope->pParent;
		}
		if ( !bInLoop ) {
			xlSemanticError(pSem, (u32)iLine, (u32)iCol,
			                (str)"%s 语句必须在循环内使用",
			                strcmp((char*)pszKind, XL_AST_BREAK_STMT) == 0 ? "break" : "continue");
		}
		return;
	}

	// try 语句
	if ( strcmp((char*)pszKind, XL_AST_TRY_STMT) == 0 ) {
		xlSemanticEnterScope(pSem, false, false);
		xlSemanticAnalyzeStmt(pSem, xlAstGetNode(xStmt, (str)"try"));
		xlSemanticLeaveScope(pSem);

		xvalue xCatch = xlAstGetNode(xStmt, (str)"catch");
		if ( xCatch ) {
			xlSemanticEnterScope(pSem, false, false);
			// 定义 catch 变量
			str pszCatchVar = xlAstGetStr(xStmt, (str)"catchVar");
			if ( pszCatchVar ) {
				XL_Symbol *pSym = xlSymbolCreate(pszCatchVar, XL_SYM_VAR, (u32)iLine, (u32)iCol);
				pSym->pType = xlTypeCreate(XL_TYPE_ANY);
				xlScopeDefine(pSem->pCurrentScope, pSym);
			}
			xlSemanticAnalyzeStmt(pSem, xCatch);
			xlSemanticLeaveScope(pSem);
		}

		xvalue xFinally = xlAstGetNode(xStmt, (str)"finally");
		if ( xFinally ) {
			xlSemanticEnterScope(pSem, false, false);
			xlSemanticAnalyzeStmt(pSem, xFinally);
			xlSemanticLeaveScope(pSem);
		}
		return;
	}

	// throw 语句
	if ( strcmp((char*)pszKind, XL_AST_THROW_STMT) == 0 ) {
		xlSemanticAnalyzeExpr(pSem, xlAstGetNode(xStmt, (str)"expr"));
		return;
	}
}


// 分析声明
static void xlSemanticAnalyzeDecl( XL_Semantic *pSem, xvalue xDecl )
{
	if ( !xDecl ) return;

	str pszKind = xlAstGetKind(xDecl);
	if ( !pszKind ) return;

	i64 iLine = xlAstGetInt(xDecl, (str)"line");
	i64 iCol = xlAstGetInt(xDecl, (str)"col");

	// 函数声明
	if ( strcmp((char*)pszKind, XL_AST_FUNC_DECL) == 0 ) {
		str pszName = xlAstGetStr(xDecl, (str)"name");

		// 查找已预注册的符号（第一遍已注册）
		XL_Symbol *pSym = xlScopeLookupLocal(pSem->pCurrentScope, pszName);
		if ( pSym ) {
			pSym->xDeclNode = xDecl;
			pSym->pType = xlTypeCreate(XL_TYPE_FUNCTION);
		}

		// 进入函数作用域
		xvalue xOldFunc = pSem->xCurrentFunc;
		pSem->xCurrentFunc = xDecl;
		xlSemanticEnterScope(pSem, true, false);

		// 定义参数
		xvalue xParams = xlAstGetNode(xDecl, (str)"params");
		int iParamCount = xlAstArrayLen(xParams);
		for ( int i = 0; i < iParamCount; i++ ) {
			xvalue xParam = xlAstArrayGet(xParams, i);
			str pszParamName = xlAstGetStr(xParam, (str)"name");
			i64 iPLine = xlAstGetInt(xParam, (str)"line");
			i64 iPCol = xlAstGetInt(xParam, (str)"col");

			XL_Symbol *pParamSym = xlSymbolCreate(pszParamName, XL_SYM_PARAM, (u32)iPLine, (u32)iPCol);
			pParamSym->pType = xlTypeFromAst(xlAstGetNode(xParam, (str)"type"));
			pParamSym->xDeclNode = xParam;

			if ( !xlScopeDefine(pSem->pCurrentScope, pParamSym) ) {
				xlSemanticError(pSem, (u32)iPLine, (u32)iPCol, (str)"参数重复定义: %s", (char*)pszParamName);
				xlSymbolDestroy(pParamSym);
			}

			// 分析默认值
			xlSemanticAnalyzeExpr(pSem, xlAstGetNode(xParam, (str)"default"));
		}

		// 分析函数体
		xlSemanticAnalyzeStmt(pSem, xlAstGetNode(xDecl, (str)"body"));

		xlSemanticLeaveScope(pSem);
		pSem->xCurrentFunc = xOldFunc;
		return;
	}

	// 类声明
	if ( strcmp((char*)pszKind, XL_AST_CLASS_DECL) == 0 ) {
		str pszName = xlAstGetStr(xDecl, (str)"name");

		// 查找已预注册的符号
		XL_Symbol *pSym = xlScopeLookupLocal(pSem->pCurrentScope, pszName);
		if ( pSym ) {
			pSym->xDeclNode = xDecl;
			pSym->pType = xlTypeCreate(XL_TYPE_CLASS);
			pSym->pType->pszName = xlStrDup(pszName, 0);
		}

		// 检查父类
		xvalue xParent = xlAstGetNode(xDecl, (str)"parent");
		if ( xParent ) {
			str pszParentName = xlAstGetStr(xParent, (str)"name");
			if ( pszParentName ) {
				XL_Symbol *pParentSym = xlScopeLookup(pSem->pCurrentScope, pszParentName);
				if ( !pParentSym || (pParentSym->kind != XL_SYM_CLASS) ) {
					xlSemanticError(pSem, (u32)iLine, (u32)iCol, (str)"未定义的父类: %s", (char*)pszParentName);
				}
			}
		}

		// 进入类作用域
		xvalue xOldClass = pSem->xCurrentClass;
		pSem->xCurrentClass = xDecl;
		xlSemanticEnterScope(pSem, false, false);

		// 添加 self 符号
		XL_Symbol *pSelfSym = xlSymbolCreate((str)"self", XL_SYM_VAR, (u32)iLine, (u32)iCol);
		pSelfSym->pType = xlTypeCreate(XL_TYPE_CLASS);
		pSelfSym->pType->pszName = xlStrDup(pszName, 0);
		xlScopeDefine(pSem->pCurrentScope, pSelfSym);

		// 添加 super 符号（用于继承）
		XL_Symbol *pSuperSym = xlSymbolCreate((str)"super", XL_SYM_VAR, (u32)iLine, (u32)iCol);
		pSuperSym->pType = xlTypeCreate(XL_TYPE_CLASS);
		xlScopeDefine(pSem->pCurrentScope, pSuperSym);

		// 分析成员
		xvalue xMembers = xlAstGetNode(xDecl, (str)"members");
		int iMemberCount = xlAstArrayLen(xMembers);
		for ( int i = 0; i < iMemberCount; i++ ) {
			xvalue xMember = xlAstArrayGet(xMembers, i);
			str pszMemberKind = xlAstGetKind(xMember);

			if ( strcmp((char*)pszMemberKind, XL_AST_FUNC_DECL) == 0 ) {
				// 类方法需要单独处理（不预注册）
				str pszMethodName = xlAstGetStr(xMember, (str)"name");
				i64 iMLine = xlAstGetInt(xMember, (str)"line");
				i64 iMCol = xlAstGetInt(xMember, (str)"col");

				// 检查重复定义
				if ( xlScopeLookupLocal(pSem->pCurrentScope, pszMethodName) ) {
					xlSemanticError(pSem, (u32)iMLine, (u32)iMCol, (str)"方法重复定义: %s", (char*)pszMethodName);
					continue;
				}

				// 添加方法符号
				XL_Symbol *pMethodSym = xlSymbolCreate(pszMethodName, XL_SYM_METHOD, (u32)iMLine, (u32)iMCol);
				pMethodSym->xDeclNode = xMember;
				pMethodSym->pType = xlTypeCreate(XL_TYPE_FUNCTION);
				xlScopeDefine(pSem->pCurrentScope, pMethodSym);

				// 分析方法体
				xvalue xOldFunc = pSem->xCurrentFunc;
				pSem->xCurrentFunc = xMember;
				xlSemanticEnterScope(pSem, true, false);

				// 定义方法参数
				xvalue xMethodParams = xlAstGetNode(xMember, (str)"params");
				int iMethodParamCount = xlAstArrayLen(xMethodParams);
				for ( int j = 0; j < iMethodParamCount; j++ ) {
					xvalue xParam = xlAstArrayGet(xMethodParams, j);
					str pszParamName = xlAstGetStr(xParam, (str)"name");
					i64 iPLine = xlAstGetInt(xParam, (str)"line");
					i64 iPCol = xlAstGetInt(xParam, (str)"col");

					XL_Symbol *pParamSym = xlSymbolCreate(pszParamName, XL_SYM_PARAM, (u32)iPLine, (u32)iPCol);
					pParamSym->pType = xlTypeFromAst(xlAstGetNode(xParam, (str)"type"));
					pParamSym->xDeclNode = xParam;

					if ( !xlScopeDefine(pSem->pCurrentScope, pParamSym) ) {
						xlSemanticError(pSem, (u32)iPLine, (u32)iPCol, (str)"参数重复定义: %s", (char*)pszParamName);
						xlSymbolDestroy(pParamSym);
					}
				}

				xlSemanticAnalyzeStmt(pSem, xlAstGetNode(xMember, (str)"body"));
				xlSemanticLeaveScope(pSem);
				pSem->xCurrentFunc = xOldFunc;

			} else if ( strcmp((char*)pszMemberKind, XL_AST_VAR_DECL) == 0 ) {
				// 类字段
				str pszFieldName = xlAstGetStr(xMember, (str)"name");
				i64 iFLine = xlAstGetInt(xMember, (str)"line");
				i64 iFCol = xlAstGetInt(xMember, (str)"col");

				XL_Symbol *pFieldSym = xlSymbolCreate(pszFieldName, XL_SYM_FIELD, (u32)iFLine, (u32)iFCol);
				pFieldSym->pType = xlTypeFromAst(xlAstGetNode(xMember, (str)"type"));
				pFieldSym->xDeclNode = xMember;

				if ( !xlScopeDefine(pSem->pCurrentScope, pFieldSym) ) {
					xlSemanticError(pSem, (u32)iFLine, (u32)iFCol, (str)"字段重复定义: %s", (char*)pszFieldName);
					xlSymbolDestroy(pFieldSym);
				}
			}
		}

		xlSemanticLeaveScope(pSem);
		pSem->xCurrentClass = xOldClass;
		return;
	}

	// 其他声明作为语句处理
	xlSemanticAnalyzeStmt(pSem, xDecl);
}


// 分析模块（入口）
static inline bool xlSemanticAnalyze( XL_Semantic *pSem, xvalue xModule )
{
	if ( !pSem || !xModule ) return false;

	xvalue xBody = xlAstGetNode(xModule, (str)"body");
	int iLen = xlAstArrayLen(xBody);

	// 第一遍：收集所有顶层声明（函数和类）
	for ( int i = 0; i < iLen; i++ ) {
		xvalue xDecl = xlAstArrayGet(xBody, i);
		str pszKind = xlAstGetKind(xDecl);

		if ( !pszKind ) continue;

		// 只预注册函数和类
		if ( (strcmp((char*)pszKind, XL_AST_FUNC_DECL) == 0) ||
		     (strcmp((char*)pszKind, XL_AST_CLASS_DECL) == 0) ) {
			str pszName = xlAstGetStr(xDecl, (str)"name");
			i64 iLine = xlAstGetInt(xDecl, (str)"line");
			i64 iCol = xlAstGetInt(xDecl, (str)"col");

			XL_SymbolKind symKind = (strcmp((char*)pszKind, XL_AST_FUNC_DECL) == 0)
			                        ? XL_SYM_FUNCTION : XL_SYM_CLASS;

			if ( !xlScopeLookupLocal(pSem->pGlobalScope, pszName) ) {
				XL_Symbol *pSym = xlSymbolCreate(pszName, symKind, (u32)iLine, (u32)iCol);
				pSym->xDeclNode = xDecl;
				xlScopeDefine(pSem->pGlobalScope, pSym);
			}
		}
	}

	// 第二遍：完整分析
	for ( int i = 0; i < iLen; i++ ) {
		xlSemanticAnalyzeDecl(pSem, xlAstArrayGet(xBody, i));
	}

	return !pSem->bHadError;
}


#endif // XL_SEMANTIC_H
