/**
 * xlang - 代码生成模块
 * 
 * 将 AST 转换为 C 代码
 */

#ifndef XL_CODEGEN_H
#define XL_CODEGEN_H

#include "xl_common.h"
#include "xl_error.h"
#include "xl_ast.h"




// ============================================================================
// 代码生成器
// ============================================================================

// 代码缓冲区
typedef struct XL_CodeBuf {
	char *pszData;          // 数据指针
	size_t iSize;           // 当前大小
	size_t iCapacity;       // 容量
} XL_CodeBuf;


// 代码生成器
typedef struct XL_Codegen {
	XL_CodeBuf *pHeader;    // 头部代码（#include、声明等）
	XL_CodeBuf *pGlobals;   // 全局变量声明
	XL_CodeBuf *pBody;      // 主体代码（函数实现等）
	XL_CodeBuf *pLambdas;   // Lambda 函数实现
	XL_ErrorList *pErrors;  // 错误列表
	int iIndent;            // 当前缩进级别
	bool bInClass;          // 是否在类内部
	bool bInGlobalInit;     // 是否在全局变量初始化阶段
	str pszClassName;       // 当前类名
	int iLambdaCount;       // Lambda 计数器
	int iScopeDepth;        // 当前作用域深度
	xvalue xLocalVars;      // 当前作用域的局部变量列表（用于闭包分析）
	xvalue xCapturedVars;   // 当前 Lambda 捕获的变量列表
	xvalue xOuterVars;      // 外层作用域变量列表（用于闭包分析）
	int iLoopDepth;         // 当前循环嵌套深度
	int arrLoopLabels[64];  // 循环标签栈（最多支持 64 层嵌套）
	int iLoopLabelCounter;  // 循环标签计数器（用于生成唯一标签）
} XL_Codegen;




// ============================================================================
// 代码缓冲区操作
// ============================================================================

// 创建代码缓冲区
static inline XL_CodeBuf* xlCodeBufCreate( void )
{
	XL_CodeBuf *pBuf = (XL_CodeBuf*)malloc(sizeof(XL_CodeBuf));
	if ( pBuf ) {
		pBuf->iCapacity = 4096;
		pBuf->pszData = (char*)malloc(pBuf->iCapacity);
		pBuf->iSize = 0;
		if ( pBuf->pszData ) {
			pBuf->pszData[0] = '\0';
		}
	}
	return pBuf;
}


// 销毁代码缓冲区
static inline void xlCodeBufDestroy( XL_CodeBuf *pBuf )
{
	if ( pBuf ) {
		XL_SAFE_FREE(pBuf->pszData);
		free(pBuf);
	}
}


// 确保容量
static inline bool xlCodeBufEnsure( XL_CodeBuf *pBuf, size_t iNeed )
{
	if ( pBuf->iSize + iNeed + 1 > pBuf->iCapacity ) {
		size_t iNewCap = pBuf->iCapacity * 2;
		while ( iNewCap < pBuf->iSize + iNeed + 1 ) {
			iNewCap *= 2;
		}
		char *pNew = (char*)realloc(pBuf->pszData, iNewCap);
		if ( !pNew ) return false;
		pBuf->pszData = pNew;
		pBuf->iCapacity = iNewCap;
	}
	return true;
}


// 追加字符串
static inline void xlCodeBufAppend( XL_CodeBuf *pBuf, const char *pszText )
{
	if ( !pBuf || !pszText ) return;
	size_t iLen = strlen(pszText);
	if ( xlCodeBufEnsure(pBuf, iLen) ) {
		memcpy(pBuf->pszData + pBuf->iSize, pszText, iLen);
		pBuf->iSize += iLen;
		pBuf->pszData[pBuf->iSize] = '\0';
	}
}


// 格式化追加
static inline void xlCodeBufAppendFmt( XL_CodeBuf *pBuf, const char *pszFmt, ... )
{
	if ( !pBuf || !pszFmt ) return;

	va_list args;
	va_start(args, pszFmt);

	// 先计算需要的长度
	va_list args2;
	va_copy(args2, args);
	int iLen = vsnprintf(NULL, 0, pszFmt, args2);
	va_end(args2);

	if ( (iLen > 0) && xlCodeBufEnsure(pBuf, iLen) ) {
		vsnprintf(pBuf->pszData + pBuf->iSize, iLen + 1, pszFmt, args);
		pBuf->iSize += iLen;
	}

	va_end(args);
}




// ============================================================================
// 代码生成器操作
// ============================================================================

// 创建代码生成器
static inline XL_Codegen* xlCodegenCreate( XL_ErrorList *pErrors )
{
	XL_Codegen *pGen = (XL_Codegen*)malloc(sizeof(XL_Codegen));
	if ( pGen ) {
		memset(pGen, 0, sizeof(XL_Codegen));
		pGen->pHeader = xlCodeBufCreate();
		pGen->pGlobals = xlCodeBufCreate();
		pGen->pBody = xlCodeBufCreate();
		pGen->pLambdas = xlCodeBufCreate();
		pGen->pErrors = pErrors;
		pGen->xLocalVars = xvoCreateArray();
		pGen->xCapturedVars = NULL;
		pGen->xOuterVars = xvoCreateArray();
	}
	return pGen;
}


// 销毁代码生成器
static inline void xlCodegenDestroy( XL_Codegen *pGen )
{
	if ( pGen ) {
		xlCodeBufDestroy(pGen->pHeader);
		xlCodeBufDestroy(pGen->pGlobals);
		xlCodeBufDestroy(pGen->pBody);
		xlCodeBufDestroy(pGen->pLambdas);
		if ( pGen->xLocalVars ) xvoUnref(pGen->xLocalVars);
		if ( pGen->xCapturedVars ) xvoUnref(pGen->xCapturedVars);
		if ( pGen->xOuterVars ) xvoUnref(pGen->xOuterVars);
		free(pGen);
	}
}


// 写入缩进
static inline void xlCodegenIndent( XL_Codegen *pGen, XL_CodeBuf *pBuf )
{
	for ( int i = 0; i < pGen->iIndent; i++ ) {
		xlCodeBufAppend(pBuf, "\t");
	}
}


// 获取生成的完整代码
static inline char* xlCodegenGetCode( XL_Codegen *pGen )
{
	if ( !pGen ) return NULL;

	size_t iTotal = pGen->pHeader->iSize + pGen->pGlobals->iSize + pGen->pLambdas->iSize + pGen->pBody->iSize + 1;
	char *pszCode = (char*)malloc(iTotal);
	if ( pszCode ) {
		size_t iOffset = 0;
		memcpy(pszCode + iOffset, pGen->pHeader->pszData, pGen->pHeader->iSize);
		iOffset += pGen->pHeader->iSize;
		memcpy(pszCode + iOffset, pGen->pGlobals->pszData, pGen->pGlobals->iSize);
		iOffset += pGen->pGlobals->iSize;
		memcpy(pszCode + iOffset, pGen->pLambdas->pszData, pGen->pLambdas->iSize);
		iOffset += pGen->pLambdas->iSize;
		memcpy(pszCode + iOffset, pGen->pBody->pszData, pGen->pBody->iSize);
		pszCode[iTotal - 1] = '\0';
	}
	return pszCode;
}




// ============================================================================
// 代码生成 - 前向声明
// ============================================================================

static void xlCodegenExpr( XL_Codegen *pGen, XL_CodeBuf *pBuf, xvalue xExpr );
static void xlCodegenStmt( XL_Codegen *pGen, xvalue xStmt );
static void xlCodegenDecl( XL_Codegen *pGen, xvalue xDecl );
static void xlCodegenCollectCaptures( XL_Codegen *pGen, xvalue xNode, xvalue xParams, xvalue xCaptured );




// ============================================================================
// 闭包变量捕获分析
// ============================================================================

// 检查变量是否在参数列表中
static bool xlCodegenIsParam( xvalue xParams, str pszName )
{
	int iLen = xlAstArrayLen(xParams);
	for ( int i = 0; i < iLen; i++ ) {
		xvalue xParam = xlAstArrayGet(xParams, i);
		str pszParamName = xlAstGetStr(xParam, (str)"name");
		if ( pszParamName && (strcmp((char*)pszParamName, (char*)pszName) == 0) ) {
			return true;
		}
	}
	return false;
}


// 检查变量是否已在捕获列表中
static bool xlCodegenIsCaptured( xvalue xCaptured, str pszName )
{
	int iLen = xvoArrayItemCount(xCaptured);
	for ( int i = 0; i < iLen; i++ ) {
		xvalue xVar = xvoArrayGetValue(xCaptured, i);
		str pszVarName = xvoGetText(xVar);
		if ( pszVarName && (strcmp((char*)pszVarName, (char*)pszName) == 0) ) {
			return true;
		}
	}
	return false;
}


// 检查是否为内置函数
static bool xlCodegenIsBuiltin( str pszName )
{
	static const char *arrBuiltins[] = {
		"print", "println", "len", "type",
		"int", "float", "string", "bool", "array", "list", "dict", "set",
		NULL
	};
	for ( int i = 0; arrBuiltins[i]; i++ ) {
		if ( strcmp((char*)pszName, arrBuiltins[i]) == 0 ) {
			return true;
		}
	}
	return false;
}


// 递归收集闭包捕获变量
static void xlCodegenCollectCaptures( XL_Codegen *pGen, xvalue xNode, xvalue xParams, xvalue xCaptured )
{
	if ( !xNode ) return;

	str pszKind = xlAstGetKind(xNode);
	if ( !pszKind ) return;

	// 标识符 - 检查是否需要捕获
	if ( strcmp((char*)pszKind, XL_AST_IDENT) == 0 ) {
		str pszName = xlAstGetStr(xNode, (str)"name");
		if ( pszName &&
		     !xlCodegenIsParam(xParams, pszName) &&
		     !xlCodegenIsCaptured(xCaptured, pszName) &&
		     !xlCodegenIsBuiltin(pszName) ) {
			// 添加到捕获列表
			xvalue xVarName = xvoCreateText(pszName, 0, FALSE);
			xvoArrayAppendValue(xCaptured, xVarName, FALSE);
		}
		return;
	}

	// 变量声明 - 局部变量不需要捕获
	if ( strcmp((char*)pszKind, XL_AST_VAR_DECL) == 0 ) {
		xvalue xInit = xlAstGetNode(xNode, (str)"init");
		if ( xInit ) {
			xlCodegenCollectCaptures(pGen, xInit, xParams, xCaptured);
		}
		return;
	}

	// 二元表达式
	if ( strcmp((char*)pszKind, XL_AST_BINARY_EXPR) == 0 ) {
		xlCodegenCollectCaptures(pGen, xlAstGetNode(xNode, (str)"left"), xParams, xCaptured);
		xlCodegenCollectCaptures(pGen, xlAstGetNode(xNode, (str)"right"), xParams, xCaptured);
		return;
	}

	// 一元表达式
	if ( strcmp((char*)pszKind, XL_AST_UNARY_EXPR) == 0 ) {
		xlCodegenCollectCaptures(pGen, xlAstGetNode(xNode, (str)"operand"), xParams, xCaptured);
		return;
	}

	// 赋值表达式
	if ( strcmp((char*)pszKind, XL_AST_ASSIGN_EXPR) == 0 ) {
		xlCodegenCollectCaptures(pGen, xlAstGetNode(xNode, (str)"target"), xParams, xCaptured);
		xlCodegenCollectCaptures(pGen, xlAstGetNode(xNode, (str)"value"), xParams, xCaptured);
		return;
	}

	// 函数调用
	if ( strcmp((char*)pszKind, XL_AST_CALL_EXPR) == 0 ) {
		xlCodegenCollectCaptures(pGen, xlAstGetNode(xNode, (str)"callee"), xParams, xCaptured);
		xvalue xArgs = xlAstGetNode(xNode, (str)"args");
		int iLen = xlAstArrayLen(xArgs);
		for ( int i = 0; i < iLen; i++ ) {
			xlCodegenCollectCaptures(pGen, xlAstArrayGet(xArgs, i), xParams, xCaptured);
		}
		return;
	}

	// 索引表达式
	if ( strcmp((char*)pszKind, XL_AST_INDEX_EXPR) == 0 ) {
		xlCodegenCollectCaptures(pGen, xlAstGetNode(xNode, (str)"object"), xParams, xCaptured);
		xlCodegenCollectCaptures(pGen, xlAstGetNode(xNode, (str)"index"), xParams, xCaptured);
		return;
	}

	// 成员访问
	if ( strcmp((char*)pszKind, XL_AST_MEMBER_EXPR) == 0 ) {
		xlCodegenCollectCaptures(pGen, xlAstGetNode(xNode, (str)"object"), xParams, xCaptured);
		return;
	}

	// 三元表达式
	if ( strcmp((char*)pszKind, XL_AST_TERNARY_EXPR) == 0 ) {
		xlCodegenCollectCaptures(pGen, xlAstGetNode(xNode, (str)"cond"), xParams, xCaptured);
		xlCodegenCollectCaptures(pGen, xlAstGetNode(xNode, (str)"then"), xParams, xCaptured);
		xlCodegenCollectCaptures(pGen, xlAstGetNode(xNode, (str)"else"), xParams, xCaptured);
		return;
	}

	// 块语句
	if ( strcmp((char*)pszKind, XL_AST_BLOCK_STMT) == 0 ) {
		xvalue xStmts = xlAstGetNode(xNode, (str)"stmts");
		int iLen = xlAstArrayLen(xStmts);
		for ( int i = 0; i < iLen; i++ ) {
			xlCodegenCollectCaptures(pGen, xlAstArrayGet(xStmts, i), xParams, xCaptured);
		}
		return;
	}

	// 表达式语句
	if ( strcmp((char*)pszKind, XL_AST_EXPR_STMT) == 0 ) {
		xlCodegenCollectCaptures(pGen, xlAstGetNode(xNode, (str)"expr"), xParams, xCaptured);
		return;
	}

	// if 语句
	if ( strcmp((char*)pszKind, XL_AST_IF_STMT) == 0 ) {
		xlCodegenCollectCaptures(pGen, xlAstGetNode(xNode, (str)"cond"), xParams, xCaptured);
		xlCodegenCollectCaptures(pGen, xlAstGetNode(xNode, (str)"then"), xParams, xCaptured);
		xvalue xElseIfs = xlAstGetNode(xNode, (str)"elseifs");
		int iLen = xlAstArrayLen(xElseIfs);
		for ( int i = 0; i < iLen; i++ ) {
			xvalue xElseIf = xlAstArrayGet(xElseIfs, i);
			xlCodegenCollectCaptures(pGen, xlAstGetNode(xElseIf, (str)"cond"), xParams, xCaptured);
			xlCodegenCollectCaptures(pGen, xlAstGetNode(xElseIf, (str)"body"), xParams, xCaptured);
		}
		xlCodegenCollectCaptures(pGen, xlAstGetNode(xNode, (str)"else"), xParams, xCaptured);
		return;
	}

	// while 语句
	if ( strcmp((char*)pszKind, XL_AST_WHILE_STMT) == 0 ) {
		xlCodegenCollectCaptures(pGen, xlAstGetNode(xNode, (str)"cond"), xParams, xCaptured);
		xlCodegenCollectCaptures(pGen, xlAstGetNode(xNode, (str)"body"), xParams, xCaptured);
		return;
	}

	// for 语句
	if ( strcmp((char*)pszKind, XL_AST_FOR_STMT) == 0 ) {
		xlCodegenCollectCaptures(pGen, xlAstGetNode(xNode, (str)"start"), xParams, xCaptured);
		xlCodegenCollectCaptures(pGen, xlAstGetNode(xNode, (str)"end"), xParams, xCaptured);
		xlCodegenCollectCaptures(pGen, xlAstGetNode(xNode, (str)"step"), xParams, xCaptured);
		xlCodegenCollectCaptures(pGen, xlAstGetNode(xNode, (str)"body"), xParams, xCaptured);
		return;
	}

	// for-in 语句
	if ( strcmp((char*)pszKind, XL_AST_FOR_IN_STMT) == 0 ) {
		xlCodegenCollectCaptures(pGen, xlAstGetNode(xNode, (str)"iter"), xParams, xCaptured);
		xlCodegenCollectCaptures(pGen, xlAstGetNode(xNode, (str)"body"), xParams, xCaptured);
		return;
	}

	// return 语句
	if ( strcmp((char*)pszKind, XL_AST_RETURN_STMT) == 0 ) {
		xvalue xValues = xlAstGetNode(xNode, (str)"values");
		int iLen = xlAstArrayLen(xValues);
		for ( int i = 0; i < iLen; i++ ) {
			xlCodegenCollectCaptures(pGen, xlAstArrayGet(xValues, i), xParams, xCaptured);
		}
		return;
	}

	// 数组字面量
	if ( strcmp((char*)pszKind, XL_AST_ARRAY_LIT) == 0 ) {
		xvalue xElements = xlAstGetNode(xNode, (str)"elements");
		int iLen = xlAstArrayLen(xElements);
		for ( int i = 0; i < iLen; i++ ) {
			xlCodegenCollectCaptures(pGen, xlAstArrayGet(xElements, i), xParams, xCaptured);
		}
		return;
	}

	// 表字面量
	if ( strcmp((char*)pszKind, XL_AST_TABLE_LIT) == 0 ) {
		xvalue xEntries = xlAstGetNode(xNode, (str)"entries");
		int iLen = xlAstArrayLen(xEntries);
		for ( int i = 0; i < iLen; i++ ) {
			xvalue xEntry = xlAstArrayGet(xEntries, i);
			xlCodegenCollectCaptures(pGen, xlAstGetNode(xEntry, (str)"value"), xParams, xCaptured);
		}
		return;
	}
}




// ============================================================================
// 代码生成 - 表达式
// ============================================================================

// 生成表达式
static void xlCodegenExpr( XL_Codegen *pGen, XL_CodeBuf *pBuf, xvalue xExpr )
{
	if ( !xExpr ) {
		xlCodeBufAppend(pBuf, "xvoCreateNull()");
		return;
	}

	str pszKind = xlAstGetKind(xExpr);
	if ( !pszKind ) return;

	// 整数字面量
	if ( strcmp((char*)pszKind, XL_AST_INT_LIT) == 0 ) {
		i64 iVal = xlAstGetInt(xExpr, (str)"value");
		xlCodeBufAppendFmt(pBuf, "xvoCreateInt(%lld)", iVal);
		return;
	}

	// 浮点数字面量
	if ( strcmp((char*)pszKind, XL_AST_FLOAT_LIT) == 0 ) {
		xvalue xVal = xvoTableGetValue(xExpr, (str)"value", 0);
		f64 fVal = xvoGetFloat(xVal);
		xlCodeBufAppendFmt(pBuf, "xvoCreateFloat(%g)", fVal);
		return;
	}

	// 字符串字面量
	if ( strcmp((char*)pszKind, XL_AST_STRING_LIT) == 0 ) {
		str pszVal = xlAstGetStr(xExpr, (str)"value");
		xlCodeBufAppend(pBuf, "xvoCreateText((str)\"");
		// 转义字符串
		if ( pszVal ) {
			for ( const char *p = (char*)pszVal; *p; p++ ) {
				switch ( *p ) {
					case '\\': xlCodeBufAppend(pBuf, "\\\\"); break;
					case '"':  xlCodeBufAppend(pBuf, "\\\""); break;
					case '\n': xlCodeBufAppend(pBuf, "\\n"); break;
					case '\r': xlCodeBufAppend(pBuf, "\\r"); break;
					case '\t': xlCodeBufAppend(pBuf, "\\t"); break;
					default:   xlCodeBufAppendFmt(pBuf, "%c", *p); break;
				}
			}
		}
		xlCodeBufAppend(pBuf, "\", 0, FALSE)");
		return;
	}

	// 布尔字面量
	if ( strcmp((char*)pszKind, XL_AST_BOOL_LIT) == 0 ) {
		xvalue xVal = xvoTableGetValue(xExpr, (str)"value", 0);
		xlCodeBufAppendFmt(pBuf, "xvoCreateBool(%s)", xvoGetBool(xVal) ? "TRUE" : "FALSE");
		return;
	}

	// null 字面量
	if ( strcmp((char*)pszKind, XL_AST_NULL_LIT) == 0 ) {
		xlCodeBufAppend(pBuf, "xvoCreateNull()");
		return;
	}

	// 标识符
	if ( strcmp((char*)pszKind, XL_AST_IDENT) == 0 ) {
		str pszName = xlAstGetStr(xExpr, (str)"name");

		// 检查是否是捕获变量（在 lambda 内部）
		if ( pGen->xCapturedVars ) {
			int iCaptureCount = xvoArrayItemCount(pGen->xCapturedVars);
			for ( int i = 0; i < iCaptureCount; i++ ) {
				xvalue xVar = xvoArrayGetValue(pGen->xCapturedVars, i);
				str pszVarName = xvoGetText(xVar);
				if ( pszVarName && (strcmp((char*)pszVarName, (char*)pszName) == 0) ) {
					// 直接访问闭包环境中的变量
					xlCodeBufAppendFmt(pBuf, "pEnv->pVars[%d]", i);
					return;
				}
			}
		}

		xlCodeBufAppendFmt(pBuf, "xlv_%s", (char*)pszName);
		return;
	}

	// 二元表达式
	if ( strcmp((char*)pszKind, XL_AST_BINARY_EXPR) == 0 ) {
		str pszOp = xlAstGetStr(xExpr, (str)"op");
		xvalue xLeft = xlAstGetNode(xExpr, (str)"left");
		xvalue xRight = xlAstGetNode(xExpr, (str)"right");

		// 使用运行时函数进行操作
		if ( strcmp((char*)pszOp, "+") == 0 ) {
			xlCodeBufAppend(pBuf, "xl_add(");
		} else if ( strcmp((char*)pszOp, "-") == 0 ) {
			xlCodeBufAppend(pBuf, "xl_sub(");
		} else if ( strcmp((char*)pszOp, "*") == 0 ) {
			xlCodeBufAppend(pBuf, "xl_mul(");
		} else if ( strcmp((char*)pszOp, "/") == 0 ) {
			xlCodeBufAppend(pBuf, "xl_div(");
		} else if ( strcmp((char*)pszOp, "%") == 0 ) {
			xlCodeBufAppend(pBuf, "xl_mod(");
		} else if ( strcmp((char*)pszOp, "**") == 0 ) {
			xlCodeBufAppend(pBuf, "xl_pow(");
		} else if ( strcmp((char*)pszOp, "==") == 0 ) {
			xlCodeBufAppend(pBuf, "xl_eq(");
		} else if ( strcmp((char*)pszOp, "!=") == 0 ) {
			xlCodeBufAppend(pBuf, "xl_ne(");
		} else if ( strcmp((char*)pszOp, "<") == 0 ) {
			xlCodeBufAppend(pBuf, "xl_lt(");
		} else if ( strcmp((char*)pszOp, "<=") == 0 ) {
			xlCodeBufAppend(pBuf, "xl_le(");
		} else if ( strcmp((char*)pszOp, ">") == 0 ) {
			xlCodeBufAppend(pBuf, "xl_gt(");
		} else if ( strcmp((char*)pszOp, ">=") == 0 ) {
			xlCodeBufAppend(pBuf, "xl_ge(");
		} else if ( (strcmp((char*)pszOp, "and") == 0) || (strcmp((char*)pszOp, "&&") == 0) ) {
			xlCodeBufAppend(pBuf, "xl_and(");
		} else if ( (strcmp((char*)pszOp, "or") == 0) || (strcmp((char*)pszOp, "||") == 0) ) {
			xlCodeBufAppend(pBuf, "xl_or(");
		} else if ( strcmp((char*)pszOp, "??") == 0 ) {
			xlCodeBufAppend(pBuf, "xl_coalesce(");
		} else {
			xlCodeBufAppend(pBuf, "xl_binop(");
		}

		xlCodegenExpr(pGen, pBuf, xLeft);
		xlCodeBufAppend(pBuf, ", ");
		xlCodegenExpr(pGen, pBuf, xRight);
		xlCodeBufAppend(pBuf, ")");
		return;
	}

	// 一元表达式
	if ( strcmp((char*)pszKind, XL_AST_UNARY_EXPR) == 0 ) {
		str pszOp = xlAstGetStr(xExpr, (str)"op");
		xvalue xOperand = xlAstGetNode(xExpr, (str)"operand");

		if ( strcmp((char*)pszOp, "-") == 0 ) {
			xlCodeBufAppend(pBuf, "xl_neg(");
		} else if ( (strcmp((char*)pszOp, "not") == 0) || (strcmp((char*)pszOp, "!") == 0) ) {
			xlCodeBufAppend(pBuf, "xl_not(");
		} else if ( strcmp((char*)pszOp, "++") == 0 ) {
			xlCodeBufAppend(pBuf, "xl_inc(");
		} else if ( strcmp((char*)pszOp, "--") == 0 ) {
			xlCodeBufAppend(pBuf, "xl_dec(");
		} else {
			xlCodeBufAppend(pBuf, "xl_unop(");
		}

		xlCodegenExpr(pGen, pBuf, xOperand);
		xlCodeBufAppend(pBuf, ")");
		return;
	}

	// 赋值表达式
	if ( strcmp((char*)pszKind, XL_AST_ASSIGN_EXPR) == 0 ) {
		str pszOp = xlAstGetStr(xExpr, (str)"op");
		xvalue xTarget = xlAstGetNode(xExpr, (str)"target");
		xvalue xValue = xlAstGetNode(xExpr, (str)"value");

		// 获取目标变量名
		str pszTargetKind = xlAstGetKind(xTarget);
		if ( strcmp((char*)pszTargetKind, XL_AST_IDENT) == 0 ) {
			str pszName = xlAstGetStr(xTarget, (str)"name");

			// 检查是否是捕获变量
			int iCaptureIdx = -1;
			if ( pGen->xCapturedVars ) {
				int iCaptureCount = xvoArrayItemCount(pGen->xCapturedVars);
				for ( int i = 0; i < iCaptureCount; i++ ) {
					xvalue xVar = xvoArrayGetValue(pGen->xCapturedVars, i);
					str pszVarName = xvoGetText(xVar);
					if ( pszVarName && (strcmp((char*)pszVarName, (char*)pszName) == 0) ) {
						iCaptureIdx = i;
						break;
					}
				}
			}

			if ( iCaptureIdx >= 0 ) {
				// 捕获变量赋值 - 直接访问闭包环境
				if ( strcmp((char*)pszOp, "=") == 0 ) {
					xlCodeBufAppendFmt(pBuf, "(pEnv->pVars[%d] = ", iCaptureIdx);
					xlCodegenExpr(pGen, pBuf, xValue);
					xlCodeBufAppend(pBuf, ")");
				} else if ( strcmp((char*)pszOp, "+=") == 0 ) {
					xlCodeBufAppendFmt(pBuf, "(pEnv->pVars[%d] = xl_add(pEnv->pVars[%d], ", iCaptureIdx, iCaptureIdx);
					xlCodegenExpr(pGen, pBuf, xValue);
					xlCodeBufAppend(pBuf, "))");
				} else if ( strcmp((char*)pszOp, "-=") == 0 ) {
					xlCodeBufAppendFmt(pBuf, "(pEnv->pVars[%d] = xl_sub(pEnv->pVars[%d], ", iCaptureIdx, iCaptureIdx);
					xlCodegenExpr(pGen, pBuf, xValue);
					xlCodeBufAppend(pBuf, "))");
				} else if ( strcmp((char*)pszOp, "*=") == 0 ) {
					xlCodeBufAppendFmt(pBuf, "(pEnv->pVars[%d] = xl_mul(pEnv->pVars[%d], ", iCaptureIdx, iCaptureIdx);
					xlCodegenExpr(pGen, pBuf, xValue);
					xlCodeBufAppend(pBuf, "))");
				} else if ( strcmp((char*)pszOp, "/=") == 0 ) {
					xlCodeBufAppendFmt(pBuf, "(pEnv->pVars[%d] = xl_div(pEnv->pVars[%d], ", iCaptureIdx, iCaptureIdx);
					xlCodegenExpr(pGen, pBuf, xValue);
					xlCodeBufAppend(pBuf, "))");
				} else {
					xlCodeBufAppendFmt(pBuf, "(pEnv->pVars[%d] = ", iCaptureIdx);
					xlCodegenExpr(pGen, pBuf, xValue);
					xlCodeBufAppend(pBuf, ")");
				}
			} else {
				// 普通变量赋值
				if ( strcmp((char*)pszOp, "=") == 0 ) {
					xlCodeBufAppendFmt(pBuf, "(xlv_%s = ", (char*)pszName);
					xlCodegenExpr(pGen, pBuf, xValue);
					xlCodeBufAppend(pBuf, ")");
				} else if ( strcmp((char*)pszOp, "+=") == 0 ) {
					xlCodeBufAppendFmt(pBuf, "(xlv_%s = xl_add(xlv_%s, ", (char*)pszName, (char*)pszName);
					xlCodegenExpr(pGen, pBuf, xValue);
					xlCodeBufAppend(pBuf, "))");
				} else if ( strcmp((char*)pszOp, "-=") == 0 ) {
					xlCodeBufAppendFmt(pBuf, "(xlv_%s = xl_sub(xlv_%s, ", (char*)pszName, (char*)pszName);
					xlCodegenExpr(pGen, pBuf, xValue);
					xlCodeBufAppend(pBuf, "))");
				} else if ( strcmp((char*)pszOp, "*=") == 0 ) {
					xlCodeBufAppendFmt(pBuf, "(xlv_%s = xl_mul(xlv_%s, ", (char*)pszName, (char*)pszName);
					xlCodegenExpr(pGen, pBuf, xValue);
					xlCodeBufAppend(pBuf, "))");
				} else if ( strcmp((char*)pszOp, "/=") == 0 ) {
					xlCodeBufAppendFmt(pBuf, "(xlv_%s = xl_div(xlv_%s, ", (char*)pszName, (char*)pszName);
					xlCodegenExpr(pGen, pBuf, xValue);
					xlCodeBufAppend(pBuf, "))");
				} else {
					xlCodeBufAppendFmt(pBuf, "(xlv_%s = ", (char*)pszName);
					xlCodegenExpr(pGen, pBuf, xValue);
					xlCodeBufAppend(pBuf, ")");
				}
			}
		} else {
			// 复杂赋值目标（索引、成员）
			xlCodeBufAppend(pBuf, "xl_assign(");
			xlCodegenExpr(pGen, pBuf, xTarget);
			xlCodeBufAppend(pBuf, ", ");
			xlCodegenExpr(pGen, pBuf, xValue);
			xlCodeBufAppend(pBuf, ")");
		}
		return;
	}

	// 函数调用
	if ( strcmp((char*)pszKind, XL_AST_CALL_EXPR) == 0 ) {
		xvalue xCallee = xlAstGetNode(xExpr, (str)"callee");
		xvalue xArgs = xlAstGetNode(xExpr, (str)"args");

		// 获取被调用函数名
		str pszCalleeKind = xlAstGetKind(xCallee);
		if ( strcmp((char*)pszCalleeKind, XL_AST_IDENT) == 0 ) {
			str pszName = xlAstGetStr(xCallee, (str)"name");

			// 检查是否为内置函数
			bool bBuiltin = false;
			if ( (strcmp((char*)pszName, "print") == 0) ||
			     (strcmp((char*)pszName, "println") == 0) ||
			     (strcmp((char*)pszName, "len") == 0) ||
			     (strcmp((char*)pszName, "type") == 0) ) {
				bBuiltin = true;
			}

			if ( bBuiltin ) {
				xlCodeBufAppendFmt(pBuf, "xl_%s(", (char*)pszName);
				// 参数
				int iLen = xlAstArrayLen(xArgs);
				for ( int i = 0; i < iLen; i++ ) {
					if ( i > 0 ) xlCodeBufAppend(pBuf, ", ");
					xlCodegenExpr(pGen, pBuf, xlAstArrayGet(xArgs, i));
				}
				xlCodeBufAppend(pBuf, ")");
			} else {
				// 用户定义函数或闭包变量
				// 使用 xl_call_var 统一处理
				xlCodeBufAppend(pBuf, "xl_call_var(xlv_");
				xlCodeBufAppend(pBuf, (char*)pszName);
				xlCodeBufAppend(pBuf, ", xl_array(");
				int iLen = xlAstArrayLen(xArgs);
				xlCodeBufAppendFmt(pBuf, "%d", iLen);
				for ( int i = 0; i < iLen; i++ ) {
					xlCodeBufAppend(pBuf, ", ");
					xlCodegenExpr(pGen, pBuf, xlAstArrayGet(xArgs, i));
				}
				xlCodeBufAppend(pBuf, "))");
			}
		} else {
			// 表达式调用（可能是闭包）
			xlCodeBufAppend(pBuf, "xl_closure_call(");
			xlCodegenExpr(pGen, pBuf, xCallee);
			xlCodeBufAppend(pBuf, ", xl_array(");
			int iLen = xlAstArrayLen(xArgs);
			xlCodeBufAppendFmt(pBuf, "%d", iLen);
			for ( int i = 0; i < iLen; i++ ) {
				xlCodeBufAppend(pBuf, ", ");
				xlCodegenExpr(pGen, pBuf, xlAstArrayGet(xArgs, i));
			}
			xlCodeBufAppend(pBuf, "))");
		}
		return;
	}

	// 索引表达式
	if ( strcmp((char*)pszKind, XL_AST_INDEX_EXPR) == 0 ) {
		xvalue xObject = xlAstGetNode(xExpr, (str)"object");
		xvalue xIndex = xlAstGetNode(xExpr, (str)"index");

		xlCodeBufAppend(pBuf, "xl_index(");
		xlCodegenExpr(pGen, pBuf, xObject);
		xlCodeBufAppend(pBuf, ", ");
		xlCodegenExpr(pGen, pBuf, xIndex);
		xlCodeBufAppend(pBuf, ")");
		return;
	}

	// 成员访问
	if ( strcmp((char*)pszKind, XL_AST_MEMBER_EXPR) == 0 ) {
		xvalue xObject = xlAstGetNode(xExpr, (str)"object");
		str pszMember = xlAstGetStr(xExpr, (str)"member");

		xlCodeBufAppend(pBuf, "xl_member(");
		xlCodegenExpr(pGen, pBuf, xObject);
		xlCodeBufAppendFmt(pBuf, ", \"%s\")", (char*)pszMember);
		return;
	}

	// 三元表达式
	if ( strcmp((char*)pszKind, XL_AST_TERNARY_EXPR) == 0 ) {
		xvalue xCond = xlAstGetNode(xExpr, (str)"cond");
		xvalue xThen = xlAstGetNode(xExpr, (str)"then");
		xvalue xElse = xlAstGetNode(xExpr, (str)"else");

		xlCodeBufAppend(pBuf, "(xvoIsTrue(");
		xlCodegenExpr(pGen, pBuf, xCond);
		xlCodeBufAppend(pBuf, ") ? ");
		xlCodegenExpr(pGen, pBuf, xThen);
		xlCodeBufAppend(pBuf, " : ");
		xlCodegenExpr(pGen, pBuf, xElse);
		xlCodeBufAppend(pBuf, ")");
		return;
	}

	// 数组字面量
	if ( strcmp((char*)pszKind, XL_AST_ARRAY_LIT) == 0 ) {
		xvalue xElements = xlAstGetNode(xExpr, (str)"elements");
		int iLen = xlAstArrayLen(xElements);

		xlCodeBufAppend(pBuf, "xl_array(");
		xlCodeBufAppendFmt(pBuf, "%d", iLen);
		for ( int i = 0; i < iLen; i++ ) {
			xlCodeBufAppend(pBuf, ", ");
			xlCodegenExpr(pGen, pBuf, xlAstArrayGet(xElements, i));
		}
		xlCodeBufAppend(pBuf, ")");
		return;
	}

	// 表字面量
	if ( strcmp((char*)pszKind, XL_AST_TABLE_LIT) == 0 ) {
		xvalue xEntries = xlAstGetNode(xExpr, (str)"entries");
		int iLen = xlAstArrayLen(xEntries);

		xlCodeBufAppend(pBuf, "xl_table(");
		xlCodeBufAppendFmt(pBuf, "%d", iLen);
		for ( int i = 0; i < iLen; i++ ) {
			xvalue xEntry = xlAstArrayGet(xEntries, i);
			xvalue xKey = xlAstGetNode(xEntry, (str)"key");
			xvalue xVal = xlAstGetNode(xEntry, (str)"value");

			xlCodeBufAppend(pBuf, ", \"");
			str pszKeyName = xlAstGetStr(xKey, (str)"name");
			if ( pszKeyName ) {
				xlCodeBufAppend(pBuf, (char*)pszKeyName);
			}
			xlCodeBufAppend(pBuf, "\", ");
			xlCodegenExpr(pGen, pBuf, xVal);
		}
		xlCodeBufAppend(pBuf, ")");
		return;
	}

	// Lambda 表达式（闭包）
	if ( strcmp((char*)pszKind, XL_AST_LAMBDA_EXPR) == 0 ) {
		xvalue xParams = xlAstGetNode(xExpr, (str)"params");
		xvalue xBody = xlAstGetNode(xExpr, (str)"body");

		// 分配 Lambda ID
		int iLambdaId = pGen->iLambdaCount++;
		char szLambdaName[64];
		snprintf(szLambdaName, sizeof(szLambdaName), "xl_lambda_%d", iLambdaId);

		// 收集闭包捕获的变量
		// 这里采用简化方案：将所有外部变量都作为捕获变量
		// 在实际调用时通过闭包环境传递
		xvalue xCaptured = xvoCreateArray();

		// 分析 Lambda 体中引用的变量
		xlCodegenCollectCaptures(pGen, xBody, xParams, xCaptured);

		int iCaptureCount = xvoArrayItemCount(xCaptured);

		// 生成 Lambda 函数声明
		xlCodeBufAppendFmt(pGen->pGlobals, "static xvalue %s(XL_ClosureEnv *pEnv, xvalue xl__args);\n", szLambdaName);

		// 生成 Lambda 函数实现
		xlCodeBufAppendFmt(pGen->pLambdas, "\nstatic xvalue %s(XL_ClosureEnv *pEnv, xvalue xl__args)\n{\n", szLambdaName);

		// 注意：不再为捕获变量创建本地副本，而是直接通过 pEnv->pVars[i] 访问
		// 这样可以实现真正的引用捕获语义

		// 展开参数（参数仍然需要本地副本）
		int iParamCount = xlAstArrayLen(xParams);
		for ( int i = 0; i < iParamCount; i++ ) {
			xvalue xParam = xlAstArrayGet(xParams, i);
			str pszParamName = xlAstGetStr(xParam, (str)"name");
			xlCodeBufAppendFmt(pGen->pLambdas, "\txvalue xlv_%s = xvoArrayGetValue(xl__args, %d);\n", (char*)pszParamName, i);
		}

		if ( iParamCount > 0 ) {
			xlCodeBufAppend(pGen->pLambdas, "\n");
		}

		// 保存当前状态
		XL_CodeBuf *pOldBody = pGen->pBody;
		int iOldIndent = pGen->iIndent;
		xvalue xOldCaptured = pGen->xCapturedVars;

		// 切换到 Lambda 缓冲区
		pGen->pBody = pGen->pLambdas;
		pGen->iIndent = 1;
		pGen->xCapturedVars = xCaptured;

		// 生成 Lambda 函数体
		xlCodegenStmt(pGen, xBody);

		// 恢复状态
		pGen->pBody = pOldBody;
		pGen->iIndent = iOldIndent;
		pGen->xCapturedVars = xOldCaptured;

		// 默认返回 null
		xlCodeBufAppend(pGen->pLambdas, "\treturn xvoCreateNull();\n");
		xlCodeBufAppend(pGen->pLambdas, "}\n");

		// 生成闭包创建代码
		if ( iCaptureCount > 0 ) {
			// 创建闭包环境
			xlCodeBufAppend(pBuf, "({\n");
			xlCodeBufAppendFmt(pBuf, "\t\tXL_ClosureEnv *pEnv = xl_env_create(%d);\n", iCaptureCount);

			// 填充捕获变量
			for ( int i = 0; i < iCaptureCount; i++ ) {
				xvalue xVar = xvoArrayGetValue(xCaptured, i);
				str pszVarName = xvoGetText(xVar);
				xlCodeBufAppendFmt(pBuf, "\t\tpEnv->pVars[%d] = xlv_%s;\n", i, (char*)pszVarName);
			}

			xlCodeBufAppendFmt(pBuf, "\t\txl_closure_create(%s, pEnv);\n", szLambdaName);
			xlCodeBufAppend(pBuf, "\t})");
		} else {
			// 无捕获变量，直接创建闭包
			xlCodeBufAppendFmt(pBuf, "xl_closure_create(%s, NULL)", szLambdaName);
		}

		xvoUnref(xCaptured);
		return;
	}

	// 默认
	xlCodeBufAppend(pBuf, "xvoCreateNull()");
}




// ============================================================================
// 代码生成 - 语句
// ============================================================================

// 生成语句
static void xlCodegenStmt( XL_Codegen *pGen, xvalue xStmt )
{
	if ( !xStmt ) return;

	str pszKind = xlAstGetKind(xStmt);
	if ( !pszKind ) return;

	XL_CodeBuf *pBuf = pGen->pBody;

	// 变量声明
	if ( strcmp((char*)pszKind, XL_AST_VAR_DECL) == 0 ) {
		str pszName = xlAstGetStr(xStmt, (str)"name");
		xvalue xInit = xlAstGetNode(xStmt, (str)"init");

		xlCodegenIndent(pGen, pBuf);

		// 顶层全局变量初始化模式：只赋值，不声明（iIndent==1 表示 main 函数顶层）
		if ( pGen->bInGlobalInit && (pGen->iIndent == 1) ) {
			xlCodeBufAppendFmt(pBuf, "xlv_%s = ", (char*)pszName);
		} else {
			xlCodeBufAppendFmt(pBuf, "xvalue xlv_%s = ", (char*)pszName);
		}

		if ( xInit ) {
			xlCodegenExpr(pGen, pBuf, xInit);
		} else {
			xlCodeBufAppend(pBuf, "xvoCreateNull()");
		}
		xlCodeBufAppend(pBuf, ";\n");
		return;
	}

	// 表达式语句
	if ( strcmp((char*)pszKind, XL_AST_EXPR_STMT) == 0 ) {
		xvalue xExpr = xlAstGetNode(xStmt, (str)"expr");
		xlCodegenIndent(pGen, pBuf);
		xlCodegenExpr(pGen, pBuf, xExpr);
		xlCodeBufAppend(pBuf, ";\n");
		return;
	}

	// 块语句
	if ( strcmp((char*)pszKind, XL_AST_BLOCK_STMT) == 0 ) {
		xvalue xStmts = xlAstGetNode(xStmt, (str)"stmts");
		int iLen = xlAstArrayLen(xStmts);
		for ( int i = 0; i < iLen; i++ ) {
			xlCodegenStmt(pGen, xlAstArrayGet(xStmts, i));
		}
		return;
	}

	// if 语句
	if ( strcmp((char*)pszKind, XL_AST_IF_STMT) == 0 ) {
		xvalue xCond = xlAstGetNode(xStmt, (str)"cond");
		xvalue xThen = xlAstGetNode(xStmt, (str)"then");
		xvalue xElseIfs = xlAstGetNode(xStmt, (str)"elseifs");
		xvalue xElse = xlAstGetNode(xStmt, (str)"else");

		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "if ( xvoIsTrue(");
		xlCodegenExpr(pGen, pBuf, xCond);
		xlCodeBufAppend(pBuf, ") ) {\n");

		pGen->iIndent++;
		xlCodegenStmt(pGen, xThen);
		pGen->iIndent--;

		// elseif
		int iElseIfLen = xlAstArrayLen(xElseIfs);
		for ( int i = 0; i < iElseIfLen; i++ ) {
			xvalue xElseIf = xlAstArrayGet(xElseIfs, i);
			xvalue xElseIfCond = xlAstGetNode(xElseIf, (str)"cond");
			xvalue xElseIfBody = xlAstGetNode(xElseIf, (str)"body");

			xlCodegenIndent(pGen, pBuf);
			xlCodeBufAppend(pBuf, "} else if ( xvoIsTrue(");
			xlCodegenExpr(pGen, pBuf, xElseIfCond);
			xlCodeBufAppend(pBuf, ") ) {\n");

			pGen->iIndent++;
			xlCodegenStmt(pGen, xElseIfBody);
			pGen->iIndent--;
		}

		// else
		if ( xElse ) {
			xlCodegenIndent(pGen, pBuf);
			xlCodeBufAppend(pBuf, "} else {\n");
			pGen->iIndent++;
			xlCodegenStmt(pGen, xElse);
			pGen->iIndent--;
		}

		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "}\n");
		return;
	}

	// while 语句
	if ( strcmp((char*)pszKind, XL_AST_WHILE_STMT) == 0 ) {
		xvalue xCond = xlAstGetNode(xStmt, (str)"cond");
		xvalue xBody = xlAstGetNode(xStmt, (str)"body");

		// 生成循环标签
		int iLoopLabel = pGen->iLoopLabelCounter++;
		pGen->arrLoopLabels[pGen->iLoopDepth++] = iLoopLabel;

		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "while ( xvoIsTrue(");
		xlCodegenExpr(pGen, pBuf, xCond);
		xlCodeBufAppend(pBuf, ") ) {\n");

		pGen->iIndent++;
		xlCodegenStmt(pGen, xBody);
		// continue 标签在循环体末尾
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppendFmt(pBuf, "xl__continue_%d:;\n", iLoopLabel);
		pGen->iIndent--;

		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "}\n");
		// break 标签在循环后
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppendFmt(pBuf, "xl__break_%d:;\n", iLoopLabel);

		pGen->iLoopDepth--;
		return;
	}

	// do-while 语句
	if ( strcmp((char*)pszKind, XL_AST_DO_WHILE_STMT) == 0 ) {
		xvalue xBody = xlAstGetNode(xStmt, (str)"body");
		xvalue xCond = xlAstGetNode(xStmt, (str)"cond");

		// 生成循环标签
		int iLoopLabel = pGen->iLoopLabelCounter++;
		pGen->arrLoopLabels[pGen->iLoopDepth++] = iLoopLabel;

		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "do {\n");

		pGen->iIndent++;
		xlCodegenStmt(pGen, xBody);
		// continue 标签在循环体末尾
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppendFmt(pBuf, "xl__continue_%d:;\n", iLoopLabel);
		pGen->iIndent--;

		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "} while ( xvoIsTrue(");
		xlCodegenExpr(pGen, pBuf, xCond);
		xlCodeBufAppend(pBuf, ") );\n");
		// break 标签在循环后
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppendFmt(pBuf, "xl__break_%d:;\n", iLoopLabel);

		pGen->iLoopDepth--;
		return;
	}

	// for 语句
	if ( strcmp((char*)pszKind, XL_AST_FOR_STMT) == 0 ) {
		str pszVar = xlAstGetStr(xStmt, (str)"var");
		xvalue xStart = xlAstGetNode(xStmt, (str)"start");
		xvalue xEnd = xlAstGetNode(xStmt, (str)"end");
		xvalue xStep = xlAstGetNode(xStmt, (str)"step");
		xvalue xBody = xlAstGetNode(xStmt, (str)"body");

		// 生成循环标签
		int iLoopLabel = pGen->iLoopLabelCounter++;
		pGen->arrLoopLabels[pGen->iLoopDepth++] = iLoopLabel;

		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "{\n");
		pGen->iIndent++;

		// 起始值、结束值、步长
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "xvalue xl__start = ");
		xlCodegenExpr(pGen, pBuf, xStart);
		xlCodeBufAppend(pBuf, ";\n");

		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "xvalue xl__end = ");
		xlCodegenExpr(pGen, pBuf, xEnd);
		xlCodeBufAppend(pBuf, ";\n");

		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "xvalue xl__step = ");
		if ( xStep && (xvoType(xStep) == XVO_DT_TABLE) ) {
			xlCodegenExpr(pGen, pBuf, xStep);
		} else {
			xlCodeBufAppend(pBuf, "xvoCreateInt(1)");
		}
		xlCodeBufAppend(pBuf, ";\n");

		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppendFmt(pBuf, "for ( xvalue xlv_%s = xl__start; xvoGetInt(xlv_%s) <= xvoGetInt(xl__end); xlv_%s = xl_add(xlv_%s, xl__step) ) {\n",
		                   (char*)pszVar, (char*)pszVar, (char*)pszVar, (char*)pszVar);

		pGen->iIndent++;
		xlCodegenStmt(pGen, xBody);
		// continue 标签在循环体末尾
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppendFmt(pBuf, "xl__continue_%d:;\n", iLoopLabel);
		pGen->iIndent--;

		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "}\n");

		pGen->iIndent--;
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "}\n");
		// break 标签在循环后
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppendFmt(pBuf, "xl__break_%d:;\n", iLoopLabel);

		pGen->iLoopDepth--;
		return;
	}

	// for-in 语句
	if ( strcmp((char*)pszKind, XL_AST_FOR_IN_STMT) == 0 ) {
		str pszKey = xlAstGetStr(xStmt, (str)"key");
		str pszValue = xlAstGetStr(xStmt, (str)"value");
		xvalue xIter = xlAstGetNode(xStmt, (str)"iter");
		xvalue xBody = xlAstGetNode(xStmt, (str)"body");

		// 生成循环标签
		int iLoopLabel = pGen->iLoopLabelCounter++;
		pGen->arrLoopLabels[pGen->iLoopDepth++] = iLoopLabel;

		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "{\n");
		pGen->iIndent++;

		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "xvalue xl__iter = ");
		xlCodegenExpr(pGen, pBuf, xIter);
		xlCodeBufAppend(pBuf, ";\n");

		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "int xl__len = xvoArrayItemCount(xl__iter);\n");

		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "for ( int xl__i = 0; xl__i < xl__len; xl__i++ ) {\n");

		pGen->iIndent++;

		if ( pszKey ) {
			xlCodegenIndent(pGen, pBuf);
			xlCodeBufAppendFmt(pBuf, "xvalue xlv_%s = xvoCreateInt(xl__i);\n", (char*)pszKey);
		}

		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppendFmt(pBuf, "xvalue xlv_%s = xvoArrayGetValue(xl__iter, xl__i);\n", (char*)pszValue);

		xlCodegenStmt(pGen, xBody);

		// continue 标签在循环体末尾
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppendFmt(pBuf, "xl__continue_%d:;\n", iLoopLabel);

		pGen->iIndent--;
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "}\n");

		pGen->iIndent--;
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "}\n");
		// break 标签在循环后
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppendFmt(pBuf, "xl__break_%d:;\n", iLoopLabel);

		pGen->iLoopDepth--;
		return;
	}

	// return 语句
	if ( strcmp((char*)pszKind, XL_AST_RETURN_STMT) == 0 ) {
		xvalue xValues = xlAstGetNode(xStmt, (str)"values");
		int iLen = xlAstArrayLen(xValues);

		xlCodegenIndent(pGen, pBuf);
		if ( iLen == 0 ) {
			xlCodeBufAppend(pBuf, "return xvoCreateNull();\n");
		} else if ( iLen == 1 ) {
			xlCodeBufAppend(pBuf, "return ");
			xlCodegenExpr(pGen, pBuf, xlAstArrayGet(xValues, 0));
			xlCodeBufAppend(pBuf, ";\n");
		} else {
			// 多返回值 - 返回数组
			xlCodeBufAppend(pBuf, "return xl_array(");
			xlCodeBufAppendFmt(pBuf, "%d", iLen);
			for ( int i = 0; i < iLen; i++ ) {
				xlCodeBufAppend(pBuf, ", ");
				xlCodegenExpr(pGen, pBuf, xlAstArrayGet(xValues, i));
			}
			xlCodeBufAppend(pBuf, ");\n");
		}
		return;
	}

	// break
	if ( strcmp((char*)pszKind, XL_AST_BREAK_STMT) == 0 ) {
		xvalue xLevel = xlAstGetNode(xStmt, (str)"level");
		int iLevel = xLevel ? (int)xvoGetInt(xLevel) : 1;
		xlCodegenIndent(pGen, pBuf);
		if ( (iLevel == 1) && (pGen->iLoopDepth > 0) ) {
			// 单层 break，使用普通 break
			xlCodeBufAppend(pBuf, "break;\n");
		} else if ( pGen->iLoopDepth >= iLevel ) {
			// 多层 break，使用 goto
			int iTargetLabel = pGen->arrLoopLabels[pGen->iLoopDepth - iLevel];
			xlCodeBufAppendFmt(pBuf, "goto xl__break_%d;\n", iTargetLabel);
		} else {
			xlCodeBufAppend(pBuf, "break; /* warning: level exceeds loop depth */\n");
		}
		return;
	}

	// continue
	if ( strcmp((char*)pszKind, XL_AST_CONTINUE_STMT) == 0 ) {
		xvalue xLevel = xlAstGetNode(xStmt, (str)"level");
		int iLevel = xLevel ? (int)xvoGetInt(xLevel) : 1;
		xlCodegenIndent(pGen, pBuf);
		if ( (iLevel == 1) && (pGen->iLoopDepth > 0) ) {
			// 单层 continue，使用普通 continue
			xlCodeBufAppend(pBuf, "continue;\n");
		} else if ( pGen->iLoopDepth >= iLevel ) {
			// 多层 continue，使用 goto
			int iTargetLabel = pGen->arrLoopLabels[pGen->iLoopDepth - iLevel];
			xlCodeBufAppendFmt(pBuf, "goto xl__continue_%d;\n", iTargetLabel);
		} else {
			xlCodeBufAppend(pBuf, "continue; /* warning: level exceeds loop depth */\n");
		}
		return;
	}

	// try 语句 - 简化处理
	if ( strcmp((char*)pszKind, XL_AST_TRY_STMT) == 0 ) {
		xvalue xTry = xlAstGetNode(xStmt, (str)"try");
		xvalue xCatch = xlAstGetNode(xStmt, (str)"catch");
		xvalue xFinally = xlAstGetNode(xStmt, (str)"finally");

		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "/* try */ {\n");
		pGen->iIndent++;
		xlCodegenStmt(pGen, xTry);
		pGen->iIndent--;
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "}\n");

		if ( xFinally ) {
			xlCodegenIndent(pGen, pBuf);
			xlCodeBufAppend(pBuf, "/* finally */ {\n");
			pGen->iIndent++;
			xlCodegenStmt(pGen, xFinally);
			pGen->iIndent--;
			xlCodegenIndent(pGen, pBuf);
			xlCodeBufAppend(pBuf, "}\n");
		}
		return;
	}
}




// ============================================================================
// 代码生成 - 声明
// ============================================================================

// 生成声明
static void xlCodegenDecl( XL_Codegen *pGen, xvalue xDecl )
{
	if ( !xDecl ) return;

	str pszKind = xlAstGetKind(xDecl);
	if ( !pszKind ) return;

	// 函数声明
	if ( strcmp((char*)pszKind, XL_AST_FUNC_DECL) == 0 ) {
		str pszName = xlAstGetStr(xDecl, (str)"name");
		xvalue xParams = xlAstGetNode(xDecl, (str)"params");
		xvalue xBody = xlAstGetNode(xDecl, (str)"body");
		int iParamCount = xlAstArrayLen(xParams);

		// 函数声明 - 使用 xlf_ 前缀避免与内置函数冲突
		xlCodeBufAppendFmt(pGen->pHeader, "xvalue xlf_%s(", (char*)pszName);

		for ( int i = 0; i < iParamCount; i++ ) {
			if ( i > 0 ) xlCodeBufAppend(pGen->pHeader, ", ");
			xvalue xParam = xlAstArrayGet(xParams, i);
			str pszParamName = xlAstGetStr(xParam, (str)"name");
			xlCodeBufAppendFmt(pGen->pHeader, "xvalue xlv_%s", (char*)pszParamName);
		}

		if ( iParamCount == 0 ) {
			xlCodeBufAppend(pGen->pHeader, "void");
		}

		xlCodeBufAppend(pGen->pHeader, ");\n");

		// 函数包装器声明（用于闭包调用）
		xlCodeBufAppendFmt(pGen->pHeader, "static xvalue xlfw_%s(XL_ClosureEnv *pEnv, xvalue xl__args);\n", (char*)pszName);

		// 函数定义
		xlCodeBufAppendFmt(pGen->pBody, "\nxvalue xlf_%s(", (char*)pszName);

		for ( int i = 0; i < iParamCount; i++ ) {
			if ( i > 0 ) xlCodeBufAppend(pGen->pBody, ", ");
			xvalue xParam = xlAstArrayGet(xParams, i);
			str pszParamName = xlAstGetStr(xParam, (str)"name");
			xlCodeBufAppendFmt(pGen->pBody, "xvalue xlv_%s", (char*)pszParamName);
		}

		if ( iParamCount == 0 ) {
			xlCodeBufAppend(pGen->pBody, "void");
		}

		xlCodeBufAppend(pGen->pBody, ")\n{\n");

		pGen->iIndent = 1;
		xlCodegenStmt(pGen, xBody);
		pGen->iIndent = 0;

		// 默认返回 null
		xlCodeBufAppend(pGen->pBody, "\treturn xvoCreateNull();\n");
		xlCodeBufAppend(pGen->pBody, "}\n");

		// 函数包装器定义（用于闭包调用）
		xlCodeBufAppendFmt(pGen->pBody, "\nstatic xvalue xlfw_%s(XL_ClosureEnv *pEnv, xvalue xl__args)\n{\n", (char*)pszName);
		xlCodeBufAppendFmt(pGen->pBody, "\treturn xlf_%s(", (char*)pszName);
		for ( int i = 0; i < iParamCount; i++ ) {
			if ( i > 0 ) xlCodeBufAppend(pGen->pBody, ", ");
			xlCodeBufAppendFmt(pGen->pBody, "xvoArrayGetValue(xl__args, %d)", i);
		}
		xlCodeBufAppend(pGen->pBody, ");\n");
		xlCodeBufAppend(pGen->pBody, "}\n");
		return;
	}

	// 类声明 - 简化为结构体和函数
	if ( strcmp((char*)pszKind, XL_AST_CLASS_DECL) == 0 ) {
		str pszName = xlAstGetStr(xDecl, (str)"name");
		xvalue xMembers = xlAstGetNode(xDecl, (str)"members");

		// 类作为 table 实现
		xlCodeBufAppendFmt(pGen->pHeader, "// class %s\n", (char*)pszName);
		xlCodeBufAppendFmt(pGen->pHeader, "xvalue xlc_%s_new(void);\n", (char*)pszName);

		// 构造函数
		xlCodeBufAppendFmt(pGen->pBody, "\nxvalue xlc_%s_new(void)\n{\n", (char*)pszName);
		xlCodeBufAppend(pGen->pBody, "\txvalue xl_self = xvoCreateTable();\n");

		// 处理成员
		int iMemberCount = xlAstArrayLen(xMembers);
		for ( int i = 0; i < iMemberCount; i++ ) {
			xvalue xMember = xlAstArrayGet(xMembers, i);
			str pszMemberKind = xlAstGetKind(xMember);

			if ( strcmp((char*)pszMemberKind, XL_AST_VAR_DECL) == 0 ) {
				str pszFieldName = xlAstGetStr(xMember, (str)"name");
				xvalue xInit = xlAstGetNode(xMember, (str)"init");

				xlCodeBufAppendFmt(pGen->pBody, "\txvoTableSetValue(xl_self, (str)\"%s\", 0, ", (char*)pszFieldName);
				if ( xInit ) {
					xlCodegenExpr(pGen, pGen->pBody, xInit);
				} else {
					xlCodeBufAppend(pGen->pBody, "xvoCreateNull()");
				}
				xlCodeBufAppend(pGen->pBody, ", FALSE);\n");
			}
		}

		xlCodeBufAppend(pGen->pBody, "\treturn xl_self;\n");
		xlCodeBufAppend(pGen->pBody, "}\n");

		// 方法
		for ( int i = 0; i < iMemberCount; i++ ) {
			xvalue xMember = xlAstArrayGet(xMembers, i);
			str pszMemberKind = xlAstGetKind(xMember);

			if ( strcmp((char*)pszMemberKind, XL_AST_FUNC_DECL) == 0 ) {
				str pszMethodName = xlAstGetStr(xMember, (str)"name");
				xvalue xMethodParams = xlAstGetNode(xMember, (str)"params");
				xvalue xMethodBody = xlAstGetNode(xMember, (str)"body");

				// 方法声明
				xlCodeBufAppendFmt(pGen->pHeader, "xvalue xlc_%s_%s(xvalue xlv_self", (char*)pszName, (char*)pszMethodName);

				int iMethodParamCount = xlAstArrayLen(xMethodParams);
				for ( int j = 0; j < iMethodParamCount; j++ ) {
					xvalue xParam = xlAstArrayGet(xMethodParams, j);
					str pszParamName = xlAstGetStr(xParam, (str)"name");
					xlCodeBufAppendFmt(pGen->pHeader, ", xvalue xlv_%s", (char*)pszParamName);
				}
				xlCodeBufAppend(pGen->pHeader, ");\n");

				// 方法定义
				xlCodeBufAppendFmt(pGen->pBody, "\nxvalue xlc_%s_%s(xvalue xlv_self", (char*)pszName, (char*)pszMethodName);

				for ( int j = 0; j < iMethodParamCount; j++ ) {
					xvalue xParam = xlAstArrayGet(xMethodParams, j);
					str pszParamName = xlAstGetStr(xParam, (str)"name");
					xlCodeBufAppendFmt(pGen->pBody, ", xvalue xlv_%s", (char*)pszParamName);
				}
				xlCodeBufAppend(pGen->pBody, ")\n{\n");

				pGen->iIndent = 1;
				xlCodegenStmt(pGen, xMethodBody);
				pGen->iIndent = 0;

				xlCodeBufAppend(pGen->pBody, "\treturn xvoCreateNull();\n");
				xlCodeBufAppend(pGen->pBody, "}\n");
			}
		}
		return;
	}

	// 顶层语句不在这里处理，在 main 中处理
}




// ============================================================================
// 代码生成 - 入口
// ============================================================================

// 生成模块代码
static inline bool xlCodegenGenerate( XL_Codegen *pGen, xvalue xModule )
{
	if ( !pGen || !xModule ) return false;

	// 生成头部
	xlCodeBufAppend(pGen->pHeader, "// xlang 生成的 C 代码\n");
	xlCodeBufAppend(pGen->pHeader, "#include <stdio.h>\n");
	xlCodeBufAppend(pGen->pHeader, "#include <stdlib.h>\n");
	xlCodeBufAppend(pGen->pHeader, "#include <string.h>\n");
	xlCodeBufAppend(pGen->pHeader, "#include <stdarg.h>\n");
	xlCodeBufAppend(pGen->pHeader, "#include \"lib/xrt/xrt.h\"\n");
	xlCodeBufAppend(pGen->pHeader, "\n");

	// 运行时辅助函数
	xlCodeBufAppend(pGen->pHeader, "// xlang 运行时\n");
	xlCodeBufAppend(pGen->pHeader, "#define xvoIsTrue(x) (xvoGetBool(x) || xvoGetInt(x) || xvoGetFloat(x))\n");
	xlCodeBufAppend(pGen->pHeader, "\n");

	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_add(xvalue a, xvalue b) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( (xvoType(a) == XVO_DT_INT) && (xvoType(b) == XVO_DT_INT) )\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn xvoCreateInt(xvoGetInt(a) + xvoGetInt(b));\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateFloat(xvoGetFloat(a) + xvoGetFloat(b));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_sub(xvalue a, xvalue b) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( (xvoType(a) == XVO_DT_INT) && (xvoType(b) == XVO_DT_INT) )\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn xvoCreateInt(xvoGetInt(a) - xvoGetInt(b));\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateFloat(xvoGetFloat(a) - xvoGetFloat(b));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_mul(xvalue a, xvalue b) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( (xvoType(a) == XVO_DT_INT) && (xvoType(b) == XVO_DT_INT) )\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn xvoCreateInt(xvoGetInt(a) * xvoGetInt(b));\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateFloat(xvoGetFloat(a) * xvoGetFloat(b));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_div(xvalue a, xvalue b) {\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateFloat(xvoGetFloat(a) / xvoGetFloat(b));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_mod(xvalue a, xvalue b) {\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateInt(xvoGetInt(a) % xvoGetInt(b));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_neg(xvalue a) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(a) == XVO_DT_INT ) return xvoCreateInt(-xvoGetInt(a));\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateFloat(-xvoGetFloat(a));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_not(xvalue a) { return xvoCreateBool(!xvoIsTrue(a)); }\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_eq(xvalue a, xvalue b) { return xvoCreateBool(xvoGetInt(a) == xvoGetInt(b)); }\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_ne(xvalue a, xvalue b) { return xvoCreateBool(xvoGetInt(a) != xvoGetInt(b)); }\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_lt(xvalue a, xvalue b) { return xvoCreateBool(xvoGetInt(a) < xvoGetInt(b)); }\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_le(xvalue a, xvalue b) { return xvoCreateBool(xvoGetInt(a) <= xvoGetInt(b)); }\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_gt(xvalue a, xvalue b) { return xvoCreateBool(xvoGetInt(a) > xvoGetInt(b)); }\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_ge(xvalue a, xvalue b) { return xvoCreateBool(xvoGetInt(a) >= xvoGetInt(b)); }\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_and(xvalue a, xvalue b) { return xvoCreateBool(xvoIsTrue(a) && xvoIsTrue(b)); }\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_or(xvalue a, xvalue b) { return xvoCreateBool(xvoIsTrue(a) || xvoIsTrue(b)); }\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_coalesce(xvalue a, xvalue b) { return xvoType(a) == XVO_DT_NULL ? b : a; }\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_index(xvalue obj, xvalue idx) { return xvoArrayGetValue(obj, xvoGetInt(idx)); }\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_member(xvalue obj, const char* name) { return xvoTableGetValue(obj, (str)name, 0); }\n");
	xlCodeBufAppend(pGen->pHeader, "\n");

	// 数组创建辅助函数
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_array(int count, ...) {\n");
	xlCodeBufAppend(pGen->pHeader, "\txvalue arr = xvoCreateArray();\n");
	xlCodeBufAppend(pGen->pHeader, "\tva_list args;\n");
	xlCodeBufAppend(pGen->pHeader, "\tva_start(args, count);\n");
	xlCodeBufAppend(pGen->pHeader, "\tfor ( int i = 0; i < count; i++ ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\txvoArrayAppendValue(arr, va_arg(args, xvalue), FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\tva_end(args);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn arr;\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// 表创建辅助函数
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_table(int count, ...) {\n");
	xlCodeBufAppend(pGen->pHeader, "\txvalue tbl = xvoCreateTable();\n");
	xlCodeBufAppend(pGen->pHeader, "\tva_list args;\n");
	xlCodeBufAppend(pGen->pHeader, "\tva_start(args, count);\n");
	xlCodeBufAppend(pGen->pHeader, "\tfor ( int i = 0; i < count; i++ ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tconst char* key = va_arg(args, const char*);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\txvalue val = va_arg(args, xvalue);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\txvoTableSetValue(tbl, (str)key, 0, val, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\tva_end(args);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn tbl;\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// print 内置函数
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_print(xvalue v) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tswitch ( xvoType(v) ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_NULL:  printf(\"null\"); break;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_BOOL:  printf(\"%s\", xvoGetBool(v) ? \"true\" : \"false\"); break;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_INT:   printf(\"%lld\", xvoGetInt(v)); break;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_FLOAT: printf(\"%g\", xvoGetFloat(v)); break;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_TEXT:  printf(\"%s\", (char*)xvoGetText(v)); break;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tdefault: printf(\"[object]\"); break;\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_println(xvalue v) { xl_print(v); printf(\"\\n\"); return xvoCreateNull(); }\n\n");

	// len 内置函数
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_len(xvalue v) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tswitch ( xvoType(v) ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_TEXT:  return xvoCreateInt(strlen((char*)xvoGetText(v)));\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_ARRAY: return xvoCreateInt(xvoArrayItemCount(v));\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_LIST:  return xvoCreateInt(xvoListItemCount(v));\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_TABLE: return xvoCreateInt(xvoTableItemCount(v));\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tdefault: return xvoCreateInt(0);\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// type 内置函数
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_type(xvalue v) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tswitch ( xvoType(v) ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_NULL:  return xvoCreateText((str)\"null\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_BOOL:  return xvoCreateText((str)\"bool\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_INT:   return xvoCreateText((str)\"int\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_FLOAT: return xvoCreateText((str)\"float\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_TEXT:  return xvoCreateText((str)\"string\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_ARRAY: return xvoCreateText((str)\"array\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_LIST:  return xvoCreateText((str)\"list\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_TABLE: return xvoCreateText((str)\"table\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tdefault: return xvoCreateText((str)\"unknown\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// 闭包运行时支持
	xlCodeBufAppend(pGen->pHeader, "// 闭包运行时\n");
	xlCodeBufAppend(pGen->pHeader, "typedef struct XL_ClosureEnv {\n");
	xlCodeBufAppend(pGen->pHeader, "\tint iRefCount;\n");
	xlCodeBufAppend(pGen->pHeader, "\tint iVarCount;\n");
	xlCodeBufAppend(pGen->pHeader, "\txvalue *pVars;\n");
	xlCodeBufAppend(pGen->pHeader, "} XL_ClosureEnv;\n\n");

	xlCodeBufAppend(pGen->pHeader, "typedef struct XL_Closure {\n");
	xlCodeBufAppend(pGen->pHeader, "\txvalue (*pFunc)(XL_ClosureEnv*, xvalue);\n");
	xlCodeBufAppend(pGen->pHeader, "\tXL_ClosureEnv *pEnv;\n");
	xlCodeBufAppend(pGen->pHeader, "} XL_Closure;\n\n");

	xlCodeBufAppend(pGen->pHeader, "static XL_ClosureEnv* xl_env_create(int count) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tXL_ClosureEnv *pEnv = (XL_ClosureEnv*)malloc(sizeof(XL_ClosureEnv));\n");
	xlCodeBufAppend(pGen->pHeader, "\tpEnv->iRefCount = 1;\n");
	xlCodeBufAppend(pGen->pHeader, "\tpEnv->iVarCount = count;\n");
	xlCodeBufAppend(pGen->pHeader, "\tpEnv->pVars = (xvalue*)malloc(sizeof(xvalue) * count);\n");
	xlCodeBufAppend(pGen->pHeader, "\tfor ( int i = 0; i < count; i++ ) pEnv->pVars[i] = NULL;\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn pEnv;\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	xlCodeBufAppend(pGen->pHeader, "static void xl_env_ref(XL_ClosureEnv *pEnv) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( pEnv ) pEnv->iRefCount++;\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	xlCodeBufAppend(pGen->pHeader, "static void xl_env_unref(XL_ClosureEnv *pEnv) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( pEnv && (--pEnv->iRefCount == 0) ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tfor ( int i = 0; i < pEnv->iVarCount; i++ ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t\tif ( pEnv->pVars[i] ) xvoUnref(pEnv->pVars[i]);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tfree(pEnv->pVars);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tfree(pEnv);\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_closure_create(xvalue (*pFunc)(XL_ClosureEnv*, xvalue), XL_ClosureEnv *pEnv) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tXL_Closure *pClosure = (XL_Closure*)malloc(sizeof(XL_Closure));\n");
	xlCodeBufAppend(pGen->pHeader, "\tpClosure->pFunc = pFunc;\n");
	xlCodeBufAppend(pGen->pHeader, "\tpClosure->pEnv = pEnv;\n");
	xlCodeBufAppend(pGen->pHeader, "\txl_env_ref(pEnv);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreatePoint(pClosure);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_closure_call(xvalue closure, xvalue args) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tXL_Closure *pClosure = (XL_Closure*)xvoGetPoint(closure);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( !pClosure || !pClosure->pFunc ) return xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn pClosure->pFunc(pClosure->pEnv, args);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	xlCodeBufAppend(pGen->pHeader, "static void xl_closure_free(xvalue closure) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tXL_Closure *pClosure = (XL_Closure*)xvoGetPoint(closure);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( pClosure ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\txl_env_unref(pClosure->pEnv);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tfree(pClosure);\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// 通用变量调用函数（支持闭包和函数指针）
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_call_var(xvalue func, xvalue args) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(func) == XVO_DT_POINT ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t// 闭包调用\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn xl_closure_call(func, args);\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// 生成所有声明
	xvalue xBody = xlAstGetNode(xModule, (str)"body");
	int iLen = xlAstArrayLen(xBody);

	// 第一遍：收集全局变量声明和函数名
	xlCodeBufAppend(pGen->pGlobals, "// 全局变量\n");
	for ( int i = 0; i < iLen; i++ ) {
		xvalue xDecl = xlAstArrayGet(xBody, i);
		str pszKind = xlAstGetKind(xDecl);

		if ( strcmp((char*)pszKind, XL_AST_VAR_DECL) == 0 ) {
			str pszName = xlAstGetStr(xDecl, (str)"name");
			xlCodeBufAppendFmt(pGen->pGlobals, "static xvalue xlv_%s = NULL;\n", (char*)pszName);
		}
		// 为函数也创建变量（用于闭包调用）
		if ( strcmp((char*)pszKind, XL_AST_FUNC_DECL) == 0 ) {
			str pszName = xlAstGetStr(xDecl, (str)"name");
			xlCodeBufAppendFmt(pGen->pGlobals, "static xvalue xlv_%s = NULL;\n", (char*)pszName);
		}
	}
	xlCodeBufAppend(pGen->pGlobals, "\n");

	// 第二遍：生成函数前向声明
	xlCodeBufAppend(pGen->pGlobals, "// 函数声明\n");

	for ( int i = 0; i < iLen; i++ ) {
		xlCodegenDecl(pGen, xlAstArrayGet(xBody, i));
	}

	// 生成 main 函数
	xlCodeBufAppend(pGen->pBody, "\n// 程序入口\n");
	xlCodeBufAppend(pGen->pBody, "int main(int argc, char* argv[])\n{\n");
	xlCodeBufAppend(pGen->pBody, "\txrtInit();\n\n");

	// 初始化函数变量（用于闭包调用）
	xlCodeBufAppend(pGen->pBody, "\t// 初始化函数变量\n");
	for ( int i = 0; i < iLen; i++ ) {
		xvalue xDecl = xlAstArrayGet(xBody, i);
		str pszKind = xlAstGetKind(xDecl);

		if ( strcmp((char*)pszKind, XL_AST_FUNC_DECL) == 0 ) {
			str pszName = xlAstGetStr(xDecl, (str)"name");
			xlCodeBufAppendFmt(pGen->pBody, "\txlv_%s = xl_closure_create(xlfw_%s, NULL);\n", (char*)pszName, (char*)pszName);
		}
	}
	xlCodeBufAppend(pGen->pBody, "\n");

	// 执行顶层语句（全局变量初始化模式）
	pGen->bInGlobalInit = true;
	for ( int i = 0; i < iLen; i++ ) {
		xvalue xDecl = xlAstArrayGet(xBody, i);
		str pszKind = xlAstGetKind(xDecl);

		// 跳过函数和类声明
		if ( (strcmp((char*)pszKind, XL_AST_FUNC_DECL) == 0) ||
		     (strcmp((char*)pszKind, XL_AST_CLASS_DECL) == 0) ) {
			continue;
		}

		pGen->iIndent = 1;
		xlCodegenStmt(pGen, xDecl);
	}
	pGen->bInGlobalInit = false;

	xlCodeBufAppend(pGen->pBody, "\n\txrtUnit();\n");
	xlCodeBufAppend(pGen->pBody, "\treturn 0;\n");
	xlCodeBufAppend(pGen->pBody, "}\n");

	return true;
}


#endif // XL_CODEGEN_H
