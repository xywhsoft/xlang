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
	xvalue xScopeVars;      // 当前作用域内声明的变量列表（用于清理）
	bool bTrackScopeVars;   // 是否追踪作用域变量（用于循环体清理）
	char arrClassNames[64][64]; // 已定义的类名列表
	int iClassCount;        // 类数量
	char arrClassMethods[64][32][64]; // 每个类的方法名列表
	int arrMethodCounts[64]; // 每个类的方法数量
	char arrClassParents[64][64]; // 每个类的父类名
	char szCurrentClass[64]; // 当前正在生成的类名
	bool arrClassHasStatic[64]; // 每个类是否有静态成员
	bool arrClassHasMetaLet[64];  // 每个类是否有 meta let
	bool arrClassHasMetaGet[64];  // 每个类是否有 meta get
	bool arrClassHasMetaCall[64]; // 每个类是否有 meta call
	// 运算符重载元方法
	bool arrClassHasMetaAdd[64];  // meta add (+)
	bool arrClassHasMetaSub[64];  // meta sub (-)
	bool arrClassHasMetaMul[64];  // meta mul (*)
	bool arrClassHasMetaDiv[64];  // meta div (/)
	bool arrClassHasMetaMod[64];  // meta mod (%)
	bool arrClassHasMetaPow[64];  // meta pow (**)
	bool arrClassHasMetaNeg[64];  // meta neg (一元负号)
	bool arrClassHasMetaEq[64];   // meta eq (==)
	bool arrClassHasMetaLt[64];   // meta lt (<)
	bool arrClassHasMetaLe[64];   // meta le (<=)
	// 索引元方法
	bool arrClassHasMetaIndex[64];    // meta index ([])
	bool arrClassHasMetaNewindex[64]; // meta newindex ([]=)
	bool arrClassHasMetaLen[64];      // meta len
	// 迭代器元方法
	bool arrClassHasMetaIter[64]; // meta iter
	bool arrClassHasMetaNext[64]; // meta next
	// 类型转换元方法
	bool arrClassHasMetaTo[64];   // meta to
	bool arrClassHasMetaFrom[64]; // meta from
	// with 语句元方法
	bool arrClassHasMetaEnter[64]; // meta enter
	bool arrClassHasMetaExit[64];  // meta exit
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
		pGen->xScopeVars = NULL;
		pGen->bTrackScopeVars = FALSE;
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
		if ( pGen->xScopeVars ) xvoUnref(pGen->xScopeVars);
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
	if ( !pszKind ) {
		xlCodeBufAppend(pBuf, "xvoCreateNull()");
		return;
	}

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

	// 模板字符串字面量
	// 格式: `hello {$name}` 转换为字符串拼接
	if ( strcmp((char*)pszKind, XL_AST_TEMPLATE_LIT) == 0 ) {
		str pszVal = xlAstGetStr(xExpr, (str)"value");
		if ( !pszVal || !*pszVal ) {
			xlCodeBufAppend(pBuf, "xvoCreateText((str)\"\", 0, FALSE)");
			return;
		}
		
		// 第一遍：统计部分数量
		int iPartCount = 0;
		const char *p = (char*)pszVal;
		const char *pszSegStart = p;
		while ( *p ) {
			if ( (p[0] == '{') && (p[1] == '$') ) {
				if ( p > pszSegStart ) iPartCount++;  // 文本段
				p += 2;
				while ( *p && (*p != '}') ) p++;
				if ( *p == '}' ) { iPartCount++; p++; }  // 变量段
				pszSegStart = p;
			} else {
				p++;
			}
		}
		if ( p > pszSegStart ) iPartCount++;  // 最后的文本段
		
		if ( iPartCount == 0 ) {
			xlCodeBufAppend(pBuf, "xvoCreateText((str)\"\", 0, FALSE)");
			return;
		}
		
		// 第二遍：生成代码
		xlCodeBufAppendFmt(pBuf, "xl_template_concat(%d", iPartCount);
		p = (char*)pszVal;
		pszSegStart = p;
		
		while ( *p ) {
			if ( (p[0] == '{') && (p[1] == '$') ) {
				// 输出之前的文本段
				if ( p > pszSegStart ) {
					xlCodeBufAppend(pBuf, ", xvoCreateText((str)\"");
					for ( const char *s = pszSegStart; s < p; s++ ) {
						switch ( *s ) {
							case '\\': xlCodeBufAppend(pBuf, "\\\\"); break;
							case '"':  xlCodeBufAppend(pBuf, "\\\""); break;
							case '\n': xlCodeBufAppend(pBuf, "\\n"); break;
							case '\r': xlCodeBufAppend(pBuf, "\\r"); break;
							case '\t': xlCodeBufAppend(pBuf, "\\t"); break;
							default:   xlCodeBufAppendFmt(pBuf, "%c", *s); break;
						}
					}
					xlCodeBufAppend(pBuf, "\", 0, FALSE)");
				}
				
				// 解析变量名
				p += 2;
				const char *pszVarStart = p;
				while ( *p && (*p != '}') ) p++;
				
				if ( *p == '}' ) {
					xlCodeBufAppend(pBuf, ", xl_tostring(xlv_");
					for ( const char *v = pszVarStart; v < p; v++ ) {
						xlCodeBufAppendFmt(pBuf, "%c", *v);
					}
					xlCodeBufAppend(pBuf, ")");
					p++;
					pszSegStart = p;
				}
			} else {
				p++;
			}
		}
		
		// 输出最后的文本段
		if ( p > pszSegStart ) {
			xlCodeBufAppend(pBuf, ", xvoCreateText((str)\"");
			for ( const char *s = pszSegStart; s < p; s++ ) {
				switch ( *s ) {
					case '\\': xlCodeBufAppend(pBuf, "\\\\"); break;
					case '"':  xlCodeBufAppend(pBuf, "\\\""); break;
					case '\n': xlCodeBufAppend(pBuf, "\\n"); break;
					case '\r': xlCodeBufAppend(pBuf, "\\r"); break;
					case '\t': xlCodeBufAppend(pBuf, "\\t"); break;
					default:   xlCodeBufAppendFmt(pBuf, "%c", *s); break;
				}
			}
			xlCodeBufAppend(pBuf, "\", 0, FALSE)");
		}
		
		xlCodeBufAppend(pBuf, ")");
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

		// 检查左右操作数是否为二元表达式（会产生临时对象）
		str pszLeftKind = xlAstGetKind(xLeft);
		str pszRightKind = xlAstGetKind(xRight);
		bool bLeftIsBinary = (pszLeftKind && (strcmp((char*)pszLeftKind, XL_AST_BINARY_EXPR) == 0));
		bool bRightIsBinary = (pszRightKind && (strcmp((char*)pszRightKind, XL_AST_BINARY_EXPR) == 0));

		// 如果有任一操作数是二元表达式，需要使用临时变量管理引用计数
		if ( bLeftIsBinary || bRightIsBinary ) {
			xlCodeBufAppend(pBuf, "({ xvalue xl__left = ");
			xlCodegenExpr(pGen, pBuf, xLeft);
			xlCodeBufAppend(pBuf, "; xvalue xl__right = ");
			xlCodegenExpr(pGen, pBuf, xRight);
			xlCodeBufAppend(pBuf, "; xvalue xl__result = ");

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
			} else if ( strcmp((char*)pszOp, "~=") == 0 ) {
				xlCodeBufAppend(pBuf, "xl_approx(");
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
			} else if ( strcmp((char*)pszOp, "in") == 0 ) {
				xlCodeBufAppend(pBuf, "xl_in(");
			} else if ( strcmp((char*)pszOp, "is") == 0 ) {
				xlCodeBufAppend(pBuf, "xl_is(");
			} else if ( strcmp((char*)pszOp, "&") == 0 ) {
				xlCodeBufAppend(pBuf, "xl_bitand(");
			} else if ( strcmp((char*)pszOp, "|") == 0 ) {
				xlCodeBufAppend(pBuf, "xl_bitor(");
			} else if ( strcmp((char*)pszOp, "^") == 0 ) {
				xlCodeBufAppend(pBuf, "xl_bitxor(");
			} else if ( strcmp((char*)pszOp, "<<") == 0 ) {
				xlCodeBufAppend(pBuf, "xl_shl(");
			} else if ( strcmp((char*)pszOp, ">>") == 0 ) {
				xlCodeBufAppend(pBuf, "xl_shr(");
			} else {
				xlCodeBufAppend(pBuf, "xl_binop(");
			}

			xlCodeBufAppend(pBuf, "xl__left, xl__right); ");
			
			// 释放临时对象
			if ( bLeftIsBinary ) {
				xlCodeBufAppend(pBuf, "xvoUnref(xl__left); ");
			}
			if ( bRightIsBinary ) {
				xlCodeBufAppend(pBuf, "xvoUnref(xl__right); ");
			}
			
			xlCodeBufAppend(pBuf, "xl__result; })");
		} else {
			// 两个操作数都不是二元表达式，直接生成函数调用
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
			} else if ( strcmp((char*)pszOp, "~=") == 0 ) {
				xlCodeBufAppend(pBuf, "xl_approx(");
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
			} else if ( strcmp((char*)pszOp, "in") == 0 ) {
				xlCodeBufAppend(pBuf, "xl_in(");
			} else if ( strcmp((char*)pszOp, "is") == 0 ) {
				xlCodeBufAppend(pBuf, "xl_is(");
			} else if ( strcmp((char*)pszOp, "&") == 0 ) {
				xlCodeBufAppend(pBuf, "xl_bitand(");
			} else if ( strcmp((char*)pszOp, "|") == 0 ) {
				xlCodeBufAppend(pBuf, "xl_bitor(");
			} else if ( strcmp((char*)pszOp, "^") == 0 ) {
				xlCodeBufAppend(pBuf, "xl_bitxor(");
			} else if ( strcmp((char*)pszOp, "<<") == 0 ) {
				xlCodeBufAppend(pBuf, "xl_shl(");
			} else if ( strcmp((char*)pszOp, ">>") == 0 ) {
				xlCodeBufAppend(pBuf, "xl_shr(");
			} else {
				xlCodeBufAppend(pBuf, "xl_binop(");
			}

			xlCodegenExpr(pGen, pBuf, xLeft);
			xlCodeBufAppend(pBuf, ", ");
			xlCodegenExpr(pGen, pBuf, xRight);
			xlCodeBufAppend(pBuf, ")");
		}
		return;
	}

	// 一元表达式
	if ( strcmp((char*)pszKind, XL_AST_UNARY_EXPR) == 0 ) {
		str pszOp = xlAstGetStr(xExpr, (str)"op");
		xvalue xOperand = xlAstGetNode(xExpr, (str)"operand");
		bool bIsPrefix = xlAstGetBool(xExpr, (str)"isPrefix");

		if ( strcmp((char*)pszOp, "-") == 0 ) {
			xlCodeBufAppend(pBuf, "xl_neg(");
			xlCodegenExpr(pGen, pBuf, xOperand);
			xlCodeBufAppend(pBuf, ")");
		} else if ( (strcmp((char*)pszOp, "not") == 0) || (strcmp((char*)pszOp, "!") == 0) ) {
			xlCodeBufAppend(pBuf, "xl_not(");
			xlCodegenExpr(pGen, pBuf, xOperand);
			xlCodeBufAppend(pBuf, ")");
		} else if ( (strcmp((char*)pszOp, "++") == 0) || (strcmp((char*)pszOp, "--") == 0) ) {
			// 自增/自减运算符需要传递变量指针
			bool bIsInc = (strcmp((char*)pszOp, "++") == 0);
			str pszOperandKind = xlAstGetKind(xOperand);

			if ( strcmp((char*)pszOperandKind, XL_AST_IDENT) == 0 ) {
				str pszName = xlAstGetStr(xOperand, (str)"name");

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
					// 捕获变量的自增自减
					if ( bIsPrefix ) {
						xlCodeBufAppendFmt(pBuf, "%s(&pEnv->pVars[%d])",
							bIsInc ? "xl_preinc" : "xl_predec", iCaptureIdx);
					} else {
						xlCodeBufAppendFmt(pBuf, "%s(&pEnv->pVars[%d])",
							bIsInc ? "xl_postinc" : "xl_postdec", iCaptureIdx);
					}
				} else {
					// 普通变量的自增自减
					if ( bIsPrefix ) {
						xlCodeBufAppendFmt(pBuf, "%s(&xlv_%s)",
							bIsInc ? "xl_preinc" : "xl_predec", (char*)pszName);
					} else {
						xlCodeBufAppendFmt(pBuf, "%s(&xlv_%s)",
							bIsInc ? "xl_postinc" : "xl_postdec", (char*)pszName);
					}
				}
			} else {
				// 对于非变量操作数，仅返回增减后的值（不修改原值）
				xlCodeBufAppendFmt(pBuf, "xl_add(");
				xlCodegenExpr(pGen, pBuf, xOperand);
				xlCodeBufAppendFmt(pBuf, ", xvoCreateInt(%s))", bIsInc ? "1" : "-1");
			}
		} else {
			xlCodeBufAppend(pBuf, "xl_unop(");
			xlCodegenExpr(pGen, pBuf, xOperand);
			xlCodeBufAppend(pBuf, ")");
		}
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
				} else if ( strcmp((char*)pszOp, "%=") == 0 ) {
					xlCodeBufAppendFmt(pBuf, "(pEnv->pVars[%d] = xl_mod(pEnv->pVars[%d], ", iCaptureIdx, iCaptureIdx);
					xlCodegenExpr(pGen, pBuf, xValue);
					xlCodeBufAppend(pBuf, "))");
				} else if ( strcmp((char*)pszOp, "**=") == 0 ) {
					xlCodeBufAppendFmt(pBuf, "(pEnv->pVars[%d] = xl_pow(pEnv->pVars[%d], ", iCaptureIdx, iCaptureIdx);
					xlCodegenExpr(pGen, pBuf, xValue);
					xlCodeBufAppend(pBuf, "))");
				} else if ( strcmp((char*)pszOp, "?=") == 0 ) {
					xlCodeBufAppendFmt(pBuf, "(pEnv->pVars[%d] = xl_coalesce(pEnv->pVars[%d], ", iCaptureIdx, iCaptureIdx);
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
					xlCodeBufAppendFmt(pBuf, "({ xvalue xl__tmp = ", (char*)pszName);
					xlCodegenExpr(pGen, pBuf, xValue);
					xlCodeBufAppendFmt(pBuf, "; xvoUnref(xlv_%s); xlv_%s = xl__tmp; xl__tmp; })", (char*)pszName, (char*)pszName);
				} else if ( strcmp((char*)pszOp, "+=") == 0 ) {
					xlCodeBufAppendFmt(pBuf, "({ xvalue xl__old = xlv_%s; xlv_%s = xl_add(xlv_%s, ", (char*)pszName, (char*)pszName, (char*)pszName);
					xlCodegenExpr(pGen, pBuf, xValue);
					xlCodeBufAppendFmt(pBuf, "); xvoUnref(xl__old); xlv_%s; })", (char*)pszName);
				} else if ( strcmp((char*)pszOp, "-=") == 0 ) {
					xlCodeBufAppendFmt(pBuf, "({ xvalue xl__old = xlv_%s; xlv_%s = xl_sub(xlv_%s, ", (char*)pszName, (char*)pszName, (char*)pszName);
					xlCodegenExpr(pGen, pBuf, xValue);
					xlCodeBufAppendFmt(pBuf, "); xvoUnref(xl__old); xlv_%s; })", (char*)pszName);
				} else if ( strcmp((char*)pszOp, "*=") == 0 ) {
					xlCodeBufAppendFmt(pBuf, "({ xvalue xl__old = xlv_%s; xlv_%s = xl_mul(xlv_%s, ", (char*)pszName, (char*)pszName, (char*)pszName);
					xlCodegenExpr(pGen, pBuf, xValue);
					xlCodeBufAppendFmt(pBuf, "); xvoUnref(xl__old); xlv_%s; })", (char*)pszName);
				} else if ( strcmp((char*)pszOp, "/=") == 0 ) {
					xlCodeBufAppendFmt(pBuf, "({ xvalue xl__old = xlv_%s; xlv_%s = xl_div(xlv_%s, ", (char*)pszName, (char*)pszName, (char*)pszName);
					xlCodegenExpr(pGen, pBuf, xValue);
					xlCodeBufAppendFmt(pBuf, "); xvoUnref(xl__old); xlv_%s; })", (char*)pszName);
				} else if ( strcmp((char*)pszOp, "%=") == 0 ) {
					xlCodeBufAppendFmt(pBuf, "({ xvalue xl__old = xlv_%s; xlv_%s = xl_mod(xlv_%s, ", (char*)pszName, (char*)pszName, (char*)pszName);
					xlCodegenExpr(pGen, pBuf, xValue);
					xlCodeBufAppendFmt(pBuf, "); xvoUnref(xl__old); xlv_%s; })", (char*)pszName);
				} else if ( strcmp((char*)pszOp, "**=") == 0 ) {
					xlCodeBufAppendFmt(pBuf, "({ xvalue xl__old = xlv_%s; xlv_%s = xl_pow(xlv_%s, ", (char*)pszName, (char*)pszName, (char*)pszName);
					xlCodegenExpr(pGen, pBuf, xValue);
					xlCodeBufAppendFmt(pBuf, "); xvoUnref(xl__old); xlv_%s; })", (char*)pszName);
				} else if ( strcmp((char*)pszOp, "?=") == 0 ) {
					xlCodeBufAppendFmt(pBuf, "({ xvalue xl__old = xlv_%s; xlv_%s = xl_coalesce(xlv_%s, ", (char*)pszName, (char*)pszName, (char*)pszName);
					xlCodegenExpr(pGen, pBuf, xValue);
					xlCodeBufAppendFmt(pBuf, "); xvoUnref(xl__old); xlv_%s; })", (char*)pszName);
				} else {
					xlCodeBufAppendFmt(pBuf, "({ xvalue xl__tmp = ", (char*)pszName);
					xlCodegenExpr(pGen, pBuf, xValue);
					xlCodeBufAppendFmt(pBuf, "; xvoUnref(xlv_%s); xlv_%s = xl__tmp; xl__tmp; })", (char*)pszName, (char*)pszName);
				}
			}
		} else {
			// 复杂赋值目标（索引、成员）
			str pszTargetKind = xlAstGetKind(xTarget);
			if ( strcmp((char*)pszTargetKind, XL_AST_MEMBER_EXPR) == 0 ) {
				// 成员赋值: obj.member = value
				xvalue xObj = xlAstGetNode(xTarget, (str)"object");
				str pszMemberName = xlAstGetStr(xTarget, (str)"member");

				// 检查是否是静态成员赋值
				str pszObjKind = xlAstGetKind(xObj);
				bool bIsStaticAssign = false;
				str pszClassName = NULL;
				if ( strcmp((char*)pszObjKind, XL_AST_IDENT) == 0 ) {
					str pszObjName = xlAstGetStr(xObj, (str)"name");
					for ( int c = 0; c < pGen->iClassCount; c++ ) {
						if ( strcmp((char*)pszObjName, pGen->arrClassNames[c]) == 0 ) {
							bIsStaticAssign = true;
							pszClassName = pszObjName;
							break;
						}
					}
				}

				if ( bIsStaticAssign ) {
					// 静态成员赋值: ClassName.member = value -> xlc_ClassName_member = value
					xlCodeBufAppendFmt(pBuf, "(xlc_%s_%s = ", (char*)pszClassName, (char*)pszMemberName);
					xlCodegenExpr(pGen, pBuf, xValue);
					xlCodeBufAppend(pBuf, ")");
				} else {
					// 实例成员赋值（支持 meta let）
					xlCodeBufAppend(pBuf, "xl_member_set(");
					xlCodegenExpr(pGen, pBuf, xObj);
					xlCodeBufAppendFmt(pBuf, ", \"%s\", ", (char*)pszMemberName);
					xlCodegenExpr(pGen, pBuf, xValue);
					xlCodeBufAppend(pBuf, ")");
				}
			} else if ( strcmp((char*)pszTargetKind, XL_AST_INDEX_EXPR) == 0 ) {
				// 索引赋值: obj[index] = value
				xvalue xObj = xlAstGetNode(xTarget, (str)"object");
				xvalue xIndex = xlAstGetNode(xTarget, (str)"index");
				xlCodeBufAppend(pBuf, "xl_index_set(");
				xlCodegenExpr(pGen, pBuf, xObj);
				xlCodeBufAppend(pBuf, ", ");
				xlCodegenExpr(pGen, pBuf, xIndex);
				xlCodeBufAppend(pBuf, ", ");
				xlCodegenExpr(pGen, pBuf, xValue);
				xlCodeBufAppend(pBuf, ")");
			} else {
				// 其他复杂赋值
				xlCodeBufAppend(pBuf, "xl_assign(");
				xlCodegenExpr(pGen, pBuf, xTarget);
				xlCodeBufAppend(pBuf, ", ");
				xlCodegenExpr(pGen, pBuf, xValue);
				xlCodeBufAppend(pBuf, ")");
			}
		}
		return;
	}

	// new 表达式 - 创建类实例
	if ( strcmp((char*)pszKind, XL_AST_NEW_EXPR) == 0 ) {
		xvalue xType = xlAstGetNode(xExpr, (str)"type");
		xvalue xArgs = xlAstGetNode(xExpr, (str)"args");
		str pszTypeName = xlAstGetStr(xType, (str)"name");
		
		if ( pszTypeName ) {
			// 检查是否是内置类型
			if ( (strcmp((char*)pszTypeName, "array") == 0) ||
			     (strcmp((char*)pszTypeName, "Array") == 0) ) {
				xlCodeBufAppend(pBuf, "xvoCreateArray()");
			} else if ( (strcmp((char*)pszTypeName, "list") == 0) ||
			            (strcmp((char*)pszTypeName, "List") == 0) ) {
				xlCodeBufAppend(pBuf, "xvoCreateList()");
			} else if ( (strcmp((char*)pszTypeName, "dict") == 0) ||
			            (strcmp((char*)pszTypeName, "Dict") == 0) ) {
				xlCodeBufAppend(pBuf, "xvoCreateTable()");
			} else if ( (strcmp((char*)pszTypeName, "set") == 0) ||
			            (strcmp((char*)pszTypeName, "Set") == 0) ) {
				xlCodeBufAppend(pBuf, "xl_set(0)");
			} else {
				// 自定义类：调用构造函数 xlc_ClassName_new(args)
				xlCodeBufAppendFmt(pBuf, "xlc_%s_new(", (char*)pszTypeName);
				int iLen = xlAstArrayLen(xArgs);
				for ( int i = 0; i < iLen; i++ ) {
					if ( i > 0 ) xlCodeBufAppend(pBuf, ", ");
					xlCodegenExpr(pGen, pBuf, xlAstArrayGet(xArgs, i));
				}
				xlCodeBufAppend(pBuf, ")");
			}
		} else {
			xlCodeBufAppend(pBuf, "xvoCreateNull()");
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
			     (strcmp((char*)pszName, "type") == 0) ||
			     (strcmp((char*)pszName, "typeof") == 0) ||
			     (strcmp((char*)pszName, "range") == 0) ||
			     (strcmp((char*)pszName, "input") == 0) ||
			     (strcmp((char*)pszName, "abs") == 0) ||
			     (strcmp((char*)pszName, "min") == 0) ||
			     (strcmp((char*)pszName, "max") == 0) ||
			     (strcmp((char*)pszName, "round") == 0) ||
			     (strcmp((char*)pszName, "floor") == 0) ||
			     (strcmp((char*)pszName, "ceil") == 0) ||
			     (strcmp((char*)pszName, "chr") == 0) ||
			     (strcmp((char*)pszName, "ord") == 0) ||
			     (strcmp((char*)pszName, "hash") == 0) ||
			     (strcmp((char*)pszName, "id") == 0) ||
			     (strcmp((char*)pszName, "int") == 0) ||
			     (strcmp((char*)pszName, "float") == 0) ||
			     (strcmp((char*)pszName, "string") == 0) ||
			     (strcmp((char*)pszName, "bool") == 0) ||
			     (strcmp((char*)pszName, "list") == 0) ||
			     (strcmp((char*)pszName, "set") == 0) ||
			     (strcmp((char*)pszName, "datetime") == 0) ||
			     (strcmp((char*)pszName, "now") == 0) ||
			     // 字符串函数
			     (strcmp((char*)pszName, "trim") == 0) ||
			     (strcmp((char*)pszName, "ltrim") == 0) ||
			     (strcmp((char*)pszName, "rtrim") == 0) ||
			     (strcmp((char*)pszName, "upper") == 0) ||
			     (strcmp((char*)pszName, "lower") == 0) ||
			     (strcmp((char*)pszName, "replace") == 0) ||
			     (strcmp((char*)pszName, "split") == 0) ||
			     (strcmp((char*)pszName, "join") == 0) ||
			     (strcmp((char*)pszName, "find") == 0) ||
			     (strcmp((char*)pszName, "contains") == 0) ||
			     (strcmp((char*)pszName, "substr") == 0) ||
			     (strcmp((char*)pszName, "startswith") == 0) ||
			     (strcmp((char*)pszName, "endswith") == 0) ||
			     (strcmp((char*)pszName, "repeat") == 0) ||
			     (strcmp((char*)pszName, "reverse") == 0) ||
			     // 数学函数
			     (strcmp((char*)pszName, "rand") == 0) ||
			     (strcmp((char*)pszName, "random") == 0) ||
			     (strcmp((char*)pszName, "randrange") == 0) ||
			     (strcmp((char*)pszName, "pow") == 0) ||
			     (strcmp((char*)pszName, "sqrt") == 0) ||
			     (strcmp((char*)pszName, "log") == 0) ||
			     (strcmp((char*)pszName, "sin") == 0) ||
			     (strcmp((char*)pszName, "cos") == 0) ||
			     (strcmp((char*)pszName, "tan") == 0) ||
			     // 时间函数
			     (strcmp((char*)pszName, "year") == 0) ||
			     (strcmp((char*)pszName, "month") == 0) ||
			     (strcmp((char*)pszName, "day") == 0) ||
			     (strcmp((char*)pszName, "hour") == 0) ||
			     (strcmp((char*)pszName, "minute") == 0) ||
			     (strcmp((char*)pszName, "second") == 0) ||
			     (strcmp((char*)pszName, "weekday") == 0) ||
			     (strcmp((char*)pszName, "strftime") == 0) ||
			     (strcmp((char*)pszName, "timestamp") == 0) ||
			     (strcmp((char*)pszName, "fromtimestamp") == 0) ||
			     (strcmp((char*)pszName, "sleep") == 0) ||
			     (strcmp((char*)pszName, "timer") == 0) ||
			     // 文件函数
			     (strcmp((char*)pszName, "readfile") == 0) ||
			     (strcmp((char*)pszName, "writefile") == 0) ||
			     (strcmp((char*)pszName, "appendfile") == 0) ||
			     (strcmp((char*)pszName, "exists") == 0) ||
			     (strcmp((char*)pszName, "isfile") == 0) ||
			     (strcmp((char*)pszName, "isdir") == 0) ||
			     (strcmp((char*)pszName, "filesize") == 0) ||
			     (strcmp((char*)pszName, "mkdir") == 0) ||
			     (strcmp((char*)pszName, "remove") == 0) ||
			     // JSON函数
			     (strcmp((char*)pszName, "json_parse") == 0) ||
			     (strcmp((char*)pszName, "json_stringify") == 0) ||
			     // 编码函数
			     (strcmp((char*)pszName, "base64_encode") == 0) ||
			     (strcmp((char*)pszName, "base64_decode") == 0) ||
			     (strcmp((char*)pszName, "hex_encode") == 0) ||
			     (strcmp((char*)pszName, "hex_decode") == 0) ) {
				bBuiltin = true;
			}

			if ( bBuiltin ) {
				// list() 创建列表
				if ( strcmp((char*)pszName, "list") == 0 ) {
					int iLen = xlAstArrayLen(xArgs);
					xlCodeBufAppend(pBuf, "xl_list(");
					xlCodeBufAppendFmt(pBuf, "%d", iLen);
					for ( int i = 0; i < iLen; i++ ) {
						xlCodeBufAppend(pBuf, ", ");
						xlCodegenExpr(pGen, pBuf, xlAstArrayGet(xArgs, i));
					}
					xlCodeBufAppend(pBuf, ")");
				}
				// set() 创建集合（暂时用 table 模拟）
				else if ( strcmp((char*)pszName, "set") == 0 ) {
					int iLen = xlAstArrayLen(xArgs);
					xlCodeBufAppend(pBuf, "xl_set(");
					xlCodeBufAppendFmt(pBuf, "%d", iLen);
					for ( int i = 0; i < iLen; i++ ) {
						xlCodeBufAppend(pBuf, ", ");
						xlCodegenExpr(pGen, pBuf, xlAstArrayGet(xArgs, i));
					}
					xlCodeBufAppend(pBuf, ")");
				}
				// datetime() 创建时间
				else if ( strcmp((char*)pszName, "datetime") == 0 ) {
					int iLen = xlAstArrayLen(xArgs);
					if ( iLen == 0 ) {
						// time() -> 当前时间
						xlCodeBufAppend(pBuf, "xvoCreateTime(xrtNow())");
					} else if ( iLen == 6 ) {
						// time(year, month, day, hour, minute, second)
						xlCodeBufAppend(pBuf, "xvoCreateTimeSerial(xvoGetInt(");
						xlCodegenExpr(pGen, pBuf, xlAstArrayGet(xArgs, 0));
						xlCodeBufAppend(pBuf, "), xvoGetInt(");
						xlCodegenExpr(pGen, pBuf, xlAstArrayGet(xArgs, 1));
						xlCodeBufAppend(pBuf, "), xvoGetInt(");
						xlCodegenExpr(pGen, pBuf, xlAstArrayGet(xArgs, 2));
						xlCodeBufAppend(pBuf, "), xvoGetInt(");
						xlCodegenExpr(pGen, pBuf, xlAstArrayGet(xArgs, 3));
						xlCodeBufAppend(pBuf, "), xvoGetInt(");
						xlCodegenExpr(pGen, pBuf, xlAstArrayGet(xArgs, 4));
						xlCodeBufAppend(pBuf, "), xvoGetInt(");
						xlCodegenExpr(pGen, pBuf, xlAstArrayGet(xArgs, 5));
						xlCodeBufAppend(pBuf, "))");
					} else if ( iLen == 3 ) {
						// time(year, month, day) - 只有日期
						xlCodeBufAppend(pBuf, "xvoCreateTimeSerial(xvoGetInt(");
						xlCodegenExpr(pGen, pBuf, xlAstArrayGet(xArgs, 0));
						xlCodeBufAppend(pBuf, "), xvoGetInt(");
						xlCodegenExpr(pGen, pBuf, xlAstArrayGet(xArgs, 1));
						xlCodeBufAppend(pBuf, "), xvoGetInt(");
						xlCodegenExpr(pGen, pBuf, xlAstArrayGet(xArgs, 2));
						xlCodeBufAppend(pBuf, "), 0, 0, 0)");
					} else {
						xlCodeBufAppend(pBuf, "xvoCreateTime(xrtNow())");
					}
				}
				// now() 获取当前时间
				else if ( strcmp((char*)pszName, "now") == 0 ) {
					xlCodeBufAppend(pBuf, "xvoCreateTime(xrtNow())");
				}
				// range 需要特殊处理参数
				else if ( strcmp((char*)pszName, "range") == 0 ) {
					int iLen = xlAstArrayLen(xArgs);
					xlCodeBufAppend(pBuf, "xl_range(");
					if ( iLen == 1 ) {
						// range(end) -> range(0, end, null)
						xlCodeBufAppend(pBuf, "xvoCreateInt(0), ");
						xlCodegenExpr(pGen, pBuf, xlAstArrayGet(xArgs, 0));
						xlCodeBufAppend(pBuf, ", xvoCreateNull()");
					} else if ( iLen == 2 ) {
						// range(start, end) -> range(start, end, null)
						xlCodegenExpr(pGen, pBuf, xlAstArrayGet(xArgs, 0));
						xlCodeBufAppend(pBuf, ", ");
						xlCodegenExpr(pGen, pBuf, xlAstArrayGet(xArgs, 1));
						xlCodeBufAppend(pBuf, ", xvoCreateNull()");
					} else if ( iLen >= 3 ) {
						// range(start, end, step)
						xlCodegenExpr(pGen, pBuf, xlAstArrayGet(xArgs, 0));
						xlCodeBufAppend(pBuf, ", ");
						xlCodegenExpr(pGen, pBuf, xlAstArrayGet(xArgs, 1));
						xlCodeBufAppend(pBuf, ", ");
						xlCodegenExpr(pGen, pBuf, xlAstArrayGet(xArgs, 2));
					} else {
						// 无参数，返回空数组
						xlCodeBufAppend(pBuf, "xvoCreateInt(0), xvoCreateInt(0), xvoCreateNull()");
					}
					xlCodeBufAppend(pBuf, ")");
				} else if ( strcmp((char*)pszName, "input") == 0 ) {
					// input 可以有 0 或 1 个参数
					int iLen = xlAstArrayLen(xArgs);
					xlCodeBufAppend(pBuf, "xl_input(");
					if ( iLen > 0 ) {
						xlCodegenExpr(pGen, pBuf, xlAstArrayGet(xArgs, 0));
					} else {
						xlCodeBufAppend(pBuf, "xvoCreateNull()");
					}
					xlCodeBufAppend(pBuf, ")");
				} else {
					xlCodeBufAppendFmt(pBuf, "xl_%s(", (char*)pszName);
					// 参数
					int iLen = xlAstArrayLen(xArgs);
					for ( int i = 0; i < iLen; i++ ) {
						if ( i > 0 ) xlCodeBufAppend(pBuf, ", ");
						xlCodegenExpr(pGen, pBuf, xlAstArrayGet(xArgs, i));
					}
					xlCodeBufAppend(pBuf, ")");
				}
			} else {
				// 检查是否是类构造函数
				bool bIsClass = false;
				for ( int i = 0; i < pGen->iClassCount; i++ ) {
					if ( strcmp((char*)pszName, pGen->arrClassNames[i]) == 0 ) {
						bIsClass = true;
						break;
					}
				}

				if ( bIsClass ) {
					// 类构造函数调用（传递参数给 create）
					xlCodeBufAppendFmt(pBuf, "xlc_%s_new(", (char*)pszName);
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
			}
		} else {
			// 表达式调用（可能是方法调用或闭包）
			// 检查是否是方法调用 (obj.method(args))
			if ( strcmp((char*)pszCalleeKind, XL_AST_MEMBER_EXPR) == 0 ) {
				xvalue xObj = xlAstGetNode(xCallee, (str)"object");
				str pszMember = xlAstGetStr(xCallee, (str)"member");

				// 检查是否是 super.method() 调用
				str pszObjKind = xlAstGetKind(xObj);
				bool bIsSuperCall = false;
				bool bIsStaticCall = false;
				str pszClassName = NULL;

				if ( strcmp((char*)pszObjKind, XL_AST_IDENT) == 0 ) {
					str pszObjName = xlAstGetStr(xObj, (str)"name");
					if ( strcmp((char*)pszObjName, "super") == 0 ) {
						bIsSuperCall = true;
					} else {
						// 检查是否是类名（静态方法调用）
						for ( int c = 0; c < pGen->iClassCount; c++ ) {
							if ( strcmp((char*)pszObjName, pGen->arrClassNames[c]) == 0 ) {
								bIsStaticCall = true;
								pszClassName = pszObjName;
								break;
							}
						}
					}
				}

				if ( bIsSuperCall ) {
					// super.create(...) 或 super.method(...)
					// 需要查找父类并调用父类的方法
					xlCodeBufAppendFmt(pBuf, "xl_super_call(xlv_self, \"%s\", xl_array(", (char*)pszMember);
					int iLen = xlAstArrayLen(xArgs);
					xlCodeBufAppendFmt(pBuf, "%d", iLen);
					for ( int i = 0; i < iLen; i++ ) {
						xlCodeBufAppend(pBuf, ", ");
						xlCodegenExpr(pGen, pBuf, xlAstArrayGet(xArgs, i));
					}
					xlCodeBufAppend(pBuf, "))");
				} else if ( bIsStaticCall ) {
					// 静态方法调用: ClassName.method(args) -> xlc_ClassName_method(args)
					xlCodeBufAppendFmt(pBuf, "xlc_%s_%s(", (char*)pszClassName, (char*)pszMember);
					int iLen = xlAstArrayLen(xArgs);
					for ( int i = 0; i < iLen; i++ ) {
						if ( i > 0 ) xlCodeBufAppend(pBuf, ", ");
						xlCodegenExpr(pGen, pBuf, xlAstArrayGet(xArgs, i));
					}
					xlCodeBufAppend(pBuf, ")");
				} else {
					// 普通方法调用
					xlCodeBufAppendFmt(pBuf, "xl_method_call(");
					xlCodegenExpr(pGen, pBuf, xObj);
					xlCodeBufAppendFmt(pBuf, ", \"%s\", xl_array(", (char*)pszMember);
					int iLen = xlAstArrayLen(xArgs);
					xlCodeBufAppendFmt(pBuf, "%d", iLen);
					for ( int i = 0; i < iLen; i++ ) {
						xlCodeBufAppend(pBuf, ", ");
						xlCodegenExpr(pGen, pBuf, xlAstArrayGet(xArgs, i));
					}
					xlCodeBufAppend(pBuf, "))");
				}
			} else {
				// 普通闭包调用
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

	// 切片表达式
	if ( strcmp((char*)pszKind, XL_AST_SLICE_EXPR) == 0 ) {
		xvalue xObject = xlAstGetNode(xExpr, (str)"object");
		xvalue xStart = xlAstGetNode(xExpr, (str)"start");
		xvalue xEnd = xlAstGetNode(xExpr, (str)"end");

		xlCodeBufAppend(pBuf, "xl_slice(");
		xlCodegenExpr(pGen, pBuf, xObject);
		xlCodeBufAppend(pBuf, ", ");
		xlCodegenExpr(pGen, pBuf, xStart);
		xlCodeBufAppend(pBuf, ", ");
		xlCodegenExpr(pGen, pBuf, xEnd);
		xlCodeBufAppend(pBuf, ")");
		return;
	}

	// 成员访问
	if ( strcmp((char*)pszKind, XL_AST_MEMBER_EXPR) == 0 ) {
		xvalue xObject = xlAstGetNode(xExpr, (str)"object");
		str pszMember = xlAstGetStr(xExpr, (str)"member");
		bool bIsSafe = xlAstGetBool(xExpr, (str)"isSafe");

		// 检查是否是 super.member
		str pszObjKind = xlAstGetKind(xObject);
		if ( strcmp((char*)pszObjKind, XL_AST_IDENT) == 0 ) {
			str pszObjName = xlAstGetStr(xObject, (str)"name");
			if ( strcmp((char*)pszObjName, "super") == 0 ) {
				// super.property 等同于 self.property
				xlCodeBufAppendFmt(pBuf, "xl_member(xlv_self, \"%s\")", (char*)pszMember);
				return;
			}

			// 检查是否是类名（静态成员访问）
			for ( int c = 0; c < pGen->iClassCount; c++ ) {
				if ( strcmp((char*)pszObjName, pGen->arrClassNames[c]) == 0 ) {
					// 静态成员访问: ClassName.member -> xlc_ClassName_member
					xlCodeBufAppendFmt(pBuf, "xlc_%s_%s", (char*)pszObjName, (char*)pszMember);
					return;
				}
			}
		}

		if ( bIsSafe ) {
			// 安全访问: obj?.member -> xl_safe_member(obj, "member")
			xlCodeBufAppend(pBuf, "xl_safe_member(");
			xlCodegenExpr(pGen, pBuf, xObject);
			xlCodeBufAppendFmt(pBuf, ", \"%s\")", (char*)pszMember);
		} else {
			xlCodeBufAppend(pBuf, "xl_member(");
			xlCodegenExpr(pGen, pBuf, xObject);
			xlCodeBufAppendFmt(pBuf, ", \"%s\")", (char*)pszMember);
		}
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

	// 列表字面量 [key:val, ...] - 生成为 TABLE
	if ( strcmp((char*)pszKind, XL_AST_LIST_LIT) == 0 ) {
		xvalue xEntries = xlAstGetNode(xExpr, (str)"entries");
		int iLen = xlAstArrayLen(xEntries);

		xlCodeBufAppend(pBuf, "xl_table(");
		xlCodeBufAppendFmt(pBuf, "%d", iLen);
		for ( int i = 0; i < iLen; i++ ) {
			xvalue xEntry = xlAstArrayGet(xEntries, i);
			xvalue xKey = xlAstGetNode(xEntry, (str)"key");
			xvalue xVal = xlAstGetNode(xEntry, (str)"value");

			// key 是 StringLit，提取其值
			str pszKeyKind = xlAstGetKind(xKey);
			xlCodeBufAppend(pBuf, ", \"");
			if ( (pszKeyKind != NULL) && (strcmp((char*)pszKeyKind, XL_AST_STRING_LIT) == 0) ) {
				str pszKeyVal = xlAstGetStr(xKey, (str)"value");
				if ( pszKeyVal ) {
					xlCodeBufAppend(pBuf, (char*)pszKeyVal);
				}
			}
			xlCodeBufAppend(pBuf, "\", ");
			xlCodegenExpr(pGen, pBuf, xVal);
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

	// 字典字面量
	if ( strcmp((char*)pszKind, XL_AST_DICT_LIT) == 0 ) {
		xvalue xEntries = xlAstGetNode(xExpr, (str)"entries");
		int iLen = xlAstArrayLen(xEntries);

		xlCodeBufAppend(pBuf, "xl_table(");
		xlCodeBufAppendFmt(pBuf, "%d", iLen);
		for ( int i = 0; i < iLen; i++ ) {
			xvalue xEntry = xlAstArrayGet(xEntries, i);
			xvalue xKey = xlAstGetNode(xEntry, (str)"key");
			xvalue xVal = xlAstGetNode(xEntry, (str)"value");

			// 字典的 key 是 StringLit
			str pszKind = xlAstGetKind(xKey);
			xlCodeBufAppend(pBuf, ", \"");
			if ( (pszKind != NULL) && (strcmp((char*)pszKind, XL_AST_STRING_LIT) == 0) ) {
				str pszKeyVal = xlAstGetStr(xKey, (str)"value");
				if ( pszKeyVal ) {
					xlCodeBufAppend(pBuf, (char*)pszKeyVal);
				}
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

	// 类型引用 (int, string, bool, etc.)
	if ( strcmp((char*)pszKind, XL_AST_TYPE_REF) == 0 ) {
		str pszTypeName = xlAstGetStr(xExpr, (str)"name");
		if ( pszTypeName ) {
			// 将类型名称映射到 XVO_DT_* 常量
			if ( strcmp((char*)pszTypeName, "null") == 0 ) {
				xlCodeBufAppend(pBuf, "xvoCreateInt(XVO_DT_NULL)");
			} else if ( strcmp((char*)pszTypeName, "bool") == 0 ) {
				xlCodeBufAppend(pBuf, "xvoCreateInt(XVO_DT_BOOL)");
			} else if ( strcmp((char*)pszTypeName, "int") == 0 ) {
				xlCodeBufAppend(pBuf, "xvoCreateInt(XVO_DT_INT)");
			} else if ( strcmp((char*)pszTypeName, "float") == 0 ) {
				xlCodeBufAppend(pBuf, "xvoCreateInt(XVO_DT_FLOAT)");
			} else if ( strcmp((char*)pszTypeName, "string") == 0 ) {
				xlCodeBufAppend(pBuf, "xvoCreateInt(XVO_DT_TEXT)");
			} else if ( strcmp((char*)pszTypeName, "time") == 0 ) {
				xlCodeBufAppend(pBuf, "xvoCreateInt(XVO_DT_TIME)");
			} else if ( strcmp((char*)pszTypeName, "array") == 0 ) {
				xlCodeBufAppend(pBuf, "xvoCreateInt(XVO_DT_ARRAY)");
			} else if ( strcmp((char*)pszTypeName, "list") == 0 ) {
				xlCodeBufAppend(pBuf, "xvoCreateInt(XVO_DT_LIST)");
			} else if ( strcmp((char*)pszTypeName, "set") == 0 ) {
				xlCodeBufAppend(pBuf, "xvoCreateInt(XVO_DT_COLL)");
			} else if ( strcmp((char*)pszTypeName, "table") == 0 ) {
				xlCodeBufAppend(pBuf, "xvoCreateInt(XVO_DT_TABLE)");
			} else if ( strcmp((char*)pszTypeName, "dict") == 0 ) {
				xlCodeBufAppend(pBuf, "xvoCreateInt(XVO_DT_TABLE)");
			} else if ( strcmp((char*)pszTypeName, "object") == 0 ) {
				xlCodeBufAppend(pBuf, "xvoCreateInt(XVO_DT_TABLE)");
			} else if ( strcmp((char*)pszTypeName, "function") == 0 ) {
				xlCodeBufAppend(pBuf, "xvoCreateInt(XVO_DT_FUNC)");
			} else {
				// 自定义类型（类名），返回类型名称字符串以便运行时检查
				xlCodeBufAppendFmt(pBuf, "xvoCreateText((str)\"%s\", 0, FALSE)", (char*)pszTypeName);
			}
		} else {
			xlCodeBufAppend(pBuf, "xvoCreateNull()");
		}
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

		// 如果开启了作用域变量追踪，记录这个变量
		if ( pGen->bTrackScopeVars && pGen->xScopeVars ) {
			xvalue xVarName = xvoCreateText(pszName, 0, FALSE);
			xvoArrayAppendValue(pGen->xScopeVars, xVarName, FALSE);
		}

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
		xlCodeBufAppendFmt(pBuf, "for ( xvalue xlv_%s = xl__start; (xvoGetInt(xl__step) > 0) ? (xvoGetInt(xlv_%s) <= xvoGetInt(xl__end)) : (xvoGetInt(xlv_%s) >= xvoGetInt(xl__end)); ) {\n",
		                   (char*)pszVar, (char*)pszVar, (char*)pszVar);
		
		pGen->iIndent++;
		// 为循环体创建作用域块，确保每次迭代的变量能被正确清理
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "{\n");
		pGen->iIndent++;
		
		// 开启变量追踪，创建变量列表
		pGen->xScopeVars = xvoCreateArray();
		pGen->bTrackScopeVars = TRUE;
		
		xlCodegenStmt(pGen, xBody);
		
		// 生成变量清理代码
		if ( pGen->xScopeVars ) {
			u32 iVarCount = xvoArrayItemCount(pGen->xScopeVars);
			for ( u32 i = 0; i < iVarCount; i++ ) {
				xvalue xVarName = xvoArrayGetValue(pGen->xScopeVars, i);
				str pszVarName = xvoGetText(xVarName);
				xlCodegenIndent(pGen, pBuf);
				xlCodeBufAppendFmt(pBuf, "xvoUnref(xlv_%s);\n", (char*)pszVarName);
			}
			xvoUnref(pGen->xScopeVars);
			pGen->xScopeVars = NULL;
		}
		pGen->bTrackScopeVars = FALSE;
		
		pGen->iIndent--;
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "}\n");
		// 在循环体末尾更新循环变量，先释放旧值
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppendFmt(pBuf, "{ xvalue xl__old = xlv_%s; xlv_%s = xl_add(xlv_%s, xl__step); xvoUnref(xl__old); }\n", 
		                   (char*)pszVar, (char*)pszVar, (char*)pszVar);
		// continue 标签在更新语句之前
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
		xlCodeBufAppend(pBuf, "xvalue xl__iter_obj = ");
		xlCodegenExpr(pGen, pBuf, xIter);
		xlCodeBufAppend(pBuf, ";\n");

		// 检查是否是自定义迭代器
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "if ( xl_is_custom_obj(xl__iter_obj) && xl_has_meta_op(xl__iter_obj, \"iter\") ) {\n");
		pGen->iIndent++;
		
		// 自定义迭代器逻辑
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "xvalue xl__iterator = xl_op_iter(xl__iter_obj);\n");
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "int xl__idx = 0;\n");
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "while ( 1 ) {\n");
		pGen->iIndent++;
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "xvalue xl__next_val = xl_op_next(xl__iterator);\n");
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "if ( (xl__next_val == NULL) || (xvoType(xl__next_val) == XVO_DT_NULL) ) break;\n");
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "{\n");
		pGen->iIndent++;
		
		if ( pszKey ) {
			xlCodegenIndent(pGen, pBuf);
			xlCodeBufAppendFmt(pBuf, "xvalue xlv_%s = xvoCreateInt(xl__idx++);\n", (char*)pszKey);
		} else {
			xlCodegenIndent(pGen, pBuf);
			xlCodeBufAppend(pBuf, "xl__idx++;\n");
		}
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppendFmt(pBuf, "xvalue xlv_%s = xl__next_val;\n", (char*)pszValue);

		// 开启变量追踪
		pGen->xScopeVars = xvoCreateArray();
		pGen->bTrackScopeVars = TRUE;
		
		xlCodegenStmt(pGen, xBody);

		// 生成变量清理代码
		if ( pGen->xScopeVars ) {
			u32 iVarCount = xvoArrayItemCount(pGen->xScopeVars);
			for ( u32 i = 0; i < iVarCount; i++ ) {
				xvalue xVarName = xvoArrayGetValue(pGen->xScopeVars, i);
				str pszVarName = xvoGetText(xVarName);
				xlCodegenIndent(pGen, pBuf);
				xlCodeBufAppendFmt(pBuf, "xvoUnref(xlv_%s);\n", (char*)pszVarName);
			}
			xvoUnref(pGen->xScopeVars);
			pGen->xScopeVars = NULL;
		}
		pGen->bTrackScopeVars = FALSE;
		
		// continue 标签 (自定义迭代器分支)
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppendFmt(pBuf, "xl__continue_c_%d:;\n", iLoopLabel);
		
		pGen->iIndent--;
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "}\n");
		pGen->iIndent--;
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "}\n");
		
		pGen->iIndent--;
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "} else {\n");
		pGen->iIndent++;
		
		// 默认数组迭代逻辑
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "xvalue xl__iter = xl__iter_obj;\n");

		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "int xl__len = xvoArrayItemCount(xl__iter);\n");

		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "for ( int xl__i = 0; xl__i < xl__len; xl__i++ ) {\n");

		pGen->iIndent++;
		// 为循环体创建作用域块
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "{\n");
		pGen->iIndent++;

		if ( pszKey ) {
			xlCodegenIndent(pGen, pBuf);
			xlCodeBufAppendFmt(pBuf, "xvalue xlv_%s = xvoCreateInt(xl__i);\n", (char*)pszKey);
		}

		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppendFmt(pBuf, "xvalue xlv_%s = xvoArrayGetValue(xl__iter, xl__i);\n", (char*)pszValue);

		// 开启变量追踪
		pGen->xScopeVars = xvoCreateArray();
		pGen->bTrackScopeVars = TRUE;
		
		xlCodegenStmt(pGen, xBody);

		// 生成变量清理代码
		if ( pGen->xScopeVars ) {
			u32 iVarCount = xvoArrayItemCount(pGen->xScopeVars);
			for ( u32 i = 0; i < iVarCount; i++ ) {
				xvalue xVarName = xvoArrayGetValue(pGen->xScopeVars, i);
				str pszVarName = xvoGetText(xVarName);
				xlCodegenIndent(pGen, pBuf);
				xlCodeBufAppendFmt(pBuf, "xvoUnref(xlv_%s);\n", (char*)pszVarName);
			}
			xvoUnref(pGen->xScopeVars);
			pGen->xScopeVars = NULL;
		}
		pGen->bTrackScopeVars = FALSE;
		
		// 清理循环变量
		if ( pszKey ) {
			xlCodegenIndent(pGen, pBuf);
			xlCodeBufAppendFmt(pBuf, "xvoUnref(xlv_%s);\n", (char*)pszKey);
		}
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppendFmt(pBuf, "xvoUnref(xlv_%s);\n", (char*)pszValue);

		// continue 标签在循环体末尾 (数组迭代分支)
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppendFmt(pBuf, "xl__continue_a_%d:;\n", iLoopLabel);

		pGen->iIndent--;
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "}\n");
		pGen->iIndent--;
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "}\n");

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

	// with 语句 - 调用 enter/exit 元方法
	if ( strcmp((char*)pszKind, XL_AST_WITH_STMT) == 0 ) {
		xvalue xExpr = xlAstGetNode(xStmt, (str)"expr");
		str pszVar = xlAstGetStr(xStmt, (str)"var");
		xvalue xBody = xlAstGetNode(xStmt, (str)"body");

		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "{ /* with */\n");
		pGen->iIndent++;

		// 计算表达式并调用 enter
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "xvalue xl__with_obj = ");
		xlCodegenExpr(pGen, pBuf, xExpr);
		xlCodeBufAppend(pBuf, ";\n");

		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "xvalue xl__with_val = xl_op_enter(xl__with_obj);\n");

		// 如果有 as var，创建变量绑定
		if ( pszVar ) {
			xlCodegenIndent(pGen, pBuf);
			xlCodeBufAppendFmt(pBuf, "xvalue xlv_%s = xl__with_val;\n", (char*)pszVar);
		}

		// 生成 body
		xlCodegenStmt(pGen, xBody);

		// 调用 exit
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "xl_op_exit(xl__with_obj);\n");

		pGen->iIndent--;
		xlCodegenIndent(pGen, pBuf);
		xlCodeBufAppend(pBuf, "}\n");
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
		xvalue xParent = xlAstGetNode(xDecl, (str)"parent");
		str pszParent = NULL;
		if ( xParent ) {
			pszParent = xlAstGetStr(xParent, (str)"name");
		}

		// 记录类名和父类名
		int iClassIdx = pGen->iClassCount;
		if ( iClassIdx < 64 ) {
			strncpy(pGen->arrClassNames[iClassIdx], (char*)pszName, 63);
			pGen->arrClassNames[iClassIdx][63] = '\0';
			if ( pszParent ) {
				strncpy(pGen->arrClassParents[iClassIdx], (char*)pszParent, 63);
				pGen->arrClassParents[iClassIdx][63] = '\0';
			} else {
				pGen->arrClassParents[iClassIdx][0] = '\0';
			}
			pGen->iClassCount++;
		}

		// 记录当前类名（用于 super 处理）
		strncpy(pGen->szCurrentClass, (char*)pszName, 63);
		pGen->szCurrentClass[63] = '\0';

		// 先扫描成员，找出构造函数、析构函数和 meta 方法
		xvalue xCtor = NULL;
		xvalue xDtor = NULL;
		xvalue xMetaLet = NULL;
		xvalue xMetaGet = NULL;
		xvalue xMetaCall = NULL;
		// 运算符重载元方法
		xvalue xMetaAdd = NULL, xMetaSub = NULL, xMetaMul = NULL, xMetaDiv = NULL;
		xvalue xMetaMod = NULL, xMetaPow = NULL, xMetaNeg = NULL;
		xvalue xMetaEq = NULL, xMetaLt = NULL, xMetaLe = NULL;
		// 索引和迭代器元方法
		xvalue xMetaIndex = NULL, xMetaNewindex = NULL, xMetaLen = NULL;
		xvalue xMetaIter = NULL, xMetaNext = NULL;
		// 类型转换元方法
		xvalue xMetaTo = NULL, xMetaFrom = NULL;
		// with 语句元方法
		xvalue xMetaEnter = NULL, xMetaExit = NULL;
		int iMemberCount = xlAstArrayLen(xMembers);
		for ( int i = 0; i < iMemberCount; i++ ) {
			xvalue xMember = xlAstArrayGet(xMembers, i);
			str pszMemberKind = xlAstGetKind(xMember);
			if ( strcmp((char*)pszMemberKind, XL_AST_CTOR_DECL) == 0 ) {
				xCtor = xMember;
			} else if ( strcmp((char*)pszMemberKind, XL_AST_DTOR_DECL) == 0 ) {
				xDtor = xMember;
			} else if ( strcmp((char*)pszMemberKind, XL_AST_META_DECL) == 0 ) {
				str pszMetaType = xlAstGetStr(xMember, (str)"metaType");
				// 基础元方法
				if ( strcmp((char*)pszMetaType, "let") == 0 ) {
					xMetaLet = xMember;
				} else if ( strcmp((char*)pszMetaType, "get") == 0 ) {
					xMetaGet = xMember;
				} else if ( strcmp((char*)pszMetaType, "call") == 0 ) {
					xMetaCall = xMember;
				}
				// 运算符重载
				else if ( strcmp((char*)pszMetaType, "add") == 0 ) {
					xMetaAdd = xMember;
				} else if ( strcmp((char*)pszMetaType, "sub") == 0 ) {
					xMetaSub = xMember;
				} else if ( strcmp((char*)pszMetaType, "mul") == 0 ) {
					xMetaMul = xMember;
				} else if ( strcmp((char*)pszMetaType, "div") == 0 ) {
					xMetaDiv = xMember;
				} else if ( strcmp((char*)pszMetaType, "mod") == 0 ) {
					xMetaMod = xMember;
				} else if ( strcmp((char*)pszMetaType, "pow") == 0 ) {
					xMetaPow = xMember;
				} else if ( strcmp((char*)pszMetaType, "neg") == 0 ) {
					xMetaNeg = xMember;
				} else if ( strcmp((char*)pszMetaType, "eq") == 0 ) {
					xMetaEq = xMember;
				} else if ( strcmp((char*)pszMetaType, "lt") == 0 ) {
					xMetaLt = xMember;
				} else if ( strcmp((char*)pszMetaType, "le") == 0 ) {
					xMetaLe = xMember;
				}
				// 索引和长度
				else if ( strcmp((char*)pszMetaType, "index") == 0 ) {
					xMetaIndex = xMember;
				} else if ( strcmp((char*)pszMetaType, "newindex") == 0 ) {
					xMetaNewindex = xMember;
				} else if ( strcmp((char*)pszMetaType, "len") == 0 ) {
					xMetaLen = xMember;
				}
				// 迭代器
				else if ( strcmp((char*)pszMetaType, "iter") == 0 ) {
					xMetaIter = xMember;
				} else if ( strcmp((char*)pszMetaType, "next") == 0 ) {
					xMetaNext = xMember;
				}
				// 类型转换
				else if ( strcmp((char*)pszMetaType, "to") == 0 ) {
					xMetaTo = xMember;
				} else if ( strcmp((char*)pszMetaType, "from") == 0 ) {
					xMetaFrom = xMember;
				}
				// with 语句
				else if ( strcmp((char*)pszMetaType, "enter") == 0 ) {
					xMetaEnter = xMember;
				} else if ( strcmp((char*)pszMetaType, "exit") == 0 ) {
					xMetaExit = xMember;
				}
			}
		}

		// 记录 meta 方法状态
		pGen->arrClassHasMetaLet[iClassIdx] = (xMetaLet != NULL);
		pGen->arrClassHasMetaGet[iClassIdx] = (xMetaGet != NULL);
		pGen->arrClassHasMetaCall[iClassIdx] = (xMetaCall != NULL);
		// 运算符重载
		pGen->arrClassHasMetaAdd[iClassIdx] = (xMetaAdd != NULL);
		pGen->arrClassHasMetaSub[iClassIdx] = (xMetaSub != NULL);
		pGen->arrClassHasMetaMul[iClassIdx] = (xMetaMul != NULL);
		pGen->arrClassHasMetaDiv[iClassIdx] = (xMetaDiv != NULL);
		pGen->arrClassHasMetaMod[iClassIdx] = (xMetaMod != NULL);
		pGen->arrClassHasMetaPow[iClassIdx] = (xMetaPow != NULL);
		pGen->arrClassHasMetaNeg[iClassIdx] = (xMetaNeg != NULL);
		pGen->arrClassHasMetaEq[iClassIdx] = (xMetaEq != NULL);
		pGen->arrClassHasMetaLt[iClassIdx] = (xMetaLt != NULL);
		pGen->arrClassHasMetaLe[iClassIdx] = (xMetaLe != NULL);
		// 索引
		pGen->arrClassHasMetaIndex[iClassIdx] = (xMetaIndex != NULL);
		pGen->arrClassHasMetaNewindex[iClassIdx] = (xMetaNewindex != NULL);
		pGen->arrClassHasMetaLen[iClassIdx] = (xMetaLen != NULL);
		// 迭代器
		pGen->arrClassHasMetaIter[iClassIdx] = (xMetaIter != NULL);
		pGen->arrClassHasMetaNext[iClassIdx] = (xMetaNext != NULL);
		// 类型转换
		pGen->arrClassHasMetaTo[iClassIdx] = (xMetaTo != NULL);
		pGen->arrClassHasMetaFrom[iClassIdx] = (xMetaFrom != NULL);
		// with 语句
		pGen->arrClassHasMetaEnter[iClassIdx] = (xMetaEnter != NULL);
		pGen->arrClassHasMetaExit[iClassIdx] = (xMetaExit != NULL);

		// 类作为 table 实现
		xlCodeBufAppendFmt(pGen->pHeader, "// class %s\n", (char*)pszName);

		// 生成静态变量声明
		bool bHasStaticVars = false;
		for ( int i = 0; i < iMemberCount; i++ ) {
			xvalue xMember = xlAstArrayGet(xMembers, i);
			str pszMemberKind = xlAstGetKind(xMember);
			if ( strcmp((char*)pszMemberKind, XL_AST_VAR_DECL) == 0 ) {
				bool bIsStatic = xlAstGetBool(xMember, (str)"isStatic");
				if ( bIsStatic ) {
					str pszFieldName = xlAstGetStr(xMember, (str)"name");
					xlCodeBufAppendFmt(pGen->pHeader, "static xvalue xlc_%s_%s = NULL;\n", (char*)pszName, (char*)pszFieldName);
					bHasStaticVars = true;
				}
			}
		}

		// 生成静态初始化函数声明
		if ( bHasStaticVars ) {
			xlCodeBufAppendFmt(pGen->pHeader, "static void xlc_%s_static_init(void);\n", (char*)pszName);
		}

		// 构造函数声明（带参数）
		xlCodeBufAppendFmt(pGen->pHeader, "xvalue xlc_%s_new(", (char*)pszName);
		if ( xCtor ) {
			xvalue xCtorParams = xlAstGetNode(xCtor, (str)"params");
			int iCtorParamCount = xlAstArrayLen(xCtorParams);
			for ( int j = 0; j < iCtorParamCount; j++ ) {
				if ( j > 0 ) xlCodeBufAppend(pGen->pHeader, ", ");
				xvalue xParam = xlAstArrayGet(xCtorParams, j);
				str pszParamName = xlAstGetStr(xParam, (str)"name");
				xlCodeBufAppendFmt(pGen->pHeader, "xvalue xlv_%s", (char*)pszParamName);
			}
			if ( iCtorParamCount == 0 ) xlCodeBufAppend(pGen->pHeader, "void");
		} else {
			xlCodeBufAppend(pGen->pHeader, "void");
		}
		xlCodeBufAppend(pGen->pHeader, ");\n");

		// 析构函数声明
		if ( xDtor ) {
			xlCodeBufAppendFmt(pGen->pHeader, "void xlc_%s_destroy(xvalue xlv_self);\n", (char*)pszName);
		}

		// 构造函数实现
		xlCodeBufAppendFmt(pGen->pBody, "\nxvalue xlc_%s_new(", (char*)pszName);
		if ( xCtor ) {
			xvalue xCtorParams = xlAstGetNode(xCtor, (str)"params");
			int iCtorParamCount = xlAstArrayLen(xCtorParams);
			for ( int j = 0; j < iCtorParamCount; j++ ) {
				if ( j > 0 ) xlCodeBufAppend(pGen->pBody, ", ");
				xvalue xParam = xlAstArrayGet(xCtorParams, j);
				str pszParamName = xlAstGetStr(xParam, (str)"name");
				xlCodeBufAppendFmt(pGen->pBody, "xvalue xlv_%s", (char*)pszParamName);
			}
			if ( iCtorParamCount == 0 ) xlCodeBufAppend(pGen->pBody, "void");
		} else {
			xlCodeBufAppend(pGen->pBody, "void");
		}
		xlCodeBufAppend(pGen->pBody, ")\n{\n");
		xlCodeBufAppend(pGen->pBody, "\txvalue xlv_self = xvoCreateTable();\n");
		// 存储类名
		xlCodeBufAppendFmt(pGen->pBody, "\txvoTableSetValue(xlv_self, (str)\"__class__\", 0, xvoCreateText((str)\"%s\", 0, FALSE), FALSE);\n", (char*)pszName);
		// 存储父类名
		if ( pszParent ) {
			xlCodeBufAppendFmt(pGen->pBody, "\txvoTableSetValue(xlv_self, (str)\"__parent__\", 0, xvoCreateText((str)\"%s\", 0, FALSE), FALSE);\n", (char*)pszParent);
		}

		// 初始化方法计数
		pGen->arrMethodCounts[iClassIdx] = 0;

		// 初始化非静态成员变量
		for ( int i = 0; i < iMemberCount; i++ ) {
			xvalue xMember = xlAstArrayGet(xMembers, i);
			str pszMemberKind = xlAstGetKind(xMember);

			if ( strcmp((char*)pszMemberKind, XL_AST_VAR_DECL) == 0 ) {
				// 跳过静态成员
				bool bIsStatic = xlAstGetBool(xMember, (str)"isStatic");
				if ( bIsStatic ) continue;

				str pszFieldName = xlAstGetStr(xMember, (str)"name");
				xvalue xInit = xlAstGetNode(xMember, (str)"init");

				xlCodeBufAppendFmt(pGen->pBody, "\txvoTableSetValue(xlv_self, (str)\"%s\", 0, ", (char*)pszFieldName);
				if ( xInit ) {
					xlCodegenExpr(pGen, pGen->pBody, xInit);
				} else {
					xlCodeBufAppend(pGen->pBody, "xvoCreateNull()");
				}
				xlCodeBufAppend(pGen->pBody, ", FALSE);\n");
			}
		}

		// 执行构造函数体
		if ( xCtor ) {
			xvalue xCtorBody = xlAstGetNode(xCtor, (str)"body");
			if ( xCtorBody ) {
				xlCodeBufAppend(pGen->pBody, "\t// constructor body\n");
				pGen->iIndent = 1;
				xlCodegenStmt(pGen, xCtorBody);
				pGen->iIndent = 0;
			}
		}

		xlCodeBufAppend(pGen->pBody, "\treturn xlv_self;\n");
		xlCodeBufAppend(pGen->pBody, "}\n");

		// 析构函数实现
		if ( xDtor ) {
			xlCodeBufAppendFmt(pGen->pBody, "\nvoid xlc_%s_destroy(xvalue xlv_self)\n{\n", (char*)pszName);
			xvalue xDtorBody = xlAstGetNode(xDtor, (str)"body");
			if ( xDtorBody ) {
				pGen->iIndent = 1;
				xlCodegenStmt(pGen, xDtorBody);
				pGen->iIndent = 0;
			}
			xlCodeBufAppend(pGen->pBody, "}\n");
		}

		// 方法
		for ( int i = 0; i < iMemberCount; i++ ) {
			xvalue xMember = xlAstArrayGet(xMembers, i);
			str pszMemberKind = xlAstGetKind(xMember);

			if ( strcmp((char*)pszMemberKind, XL_AST_FUNC_DECL) == 0 ) {
				str pszMethodName = xlAstGetStr(xMember, (str)"name");
				xvalue xMethodParams = xlAstGetNode(xMember, (str)"params");
				xvalue xMethodBody = xlAstGetNode(xMember, (str)"body");
				bool bIsStatic = xlAstGetBool(xMember, (str)"isStatic");

				// 记录方法名（非静态方法用于方法分发）
				if ( !bIsStatic ) {
					int iMethodIdx = pGen->arrMethodCounts[iClassIdx];
					if ( iMethodIdx < 32 ) {
						strncpy(pGen->arrClassMethods[iClassIdx][iMethodIdx], (char*)pszMethodName, 63);
						pGen->arrClassMethods[iClassIdx][iMethodIdx][63] = '\0';
						pGen->arrMethodCounts[iClassIdx]++;
					}
				}

				if ( bIsStatic ) {
					// 静态方法声明（不需要 self 参数）
					xlCodeBufAppendFmt(pGen->pHeader, "xvalue xlc_%s_%s(", (char*)pszName, (char*)pszMethodName);

					int iMethodParamCount = xlAstArrayLen(xMethodParams);
					for ( int j = 0; j < iMethodParamCount; j++ ) {
						if ( j > 0 ) xlCodeBufAppend(pGen->pHeader, ", ");
						xvalue xParam = xlAstArrayGet(xMethodParams, j);
						str pszParamName = xlAstGetStr(xParam, (str)"name");
						xlCodeBufAppendFmt(pGen->pHeader, "xvalue xlv_%s", (char*)pszParamName);
					}
					if ( iMethodParamCount == 0 ) xlCodeBufAppend(pGen->pHeader, "void");
					xlCodeBufAppend(pGen->pHeader, ");\n");

					// 静态方法定义
					xlCodeBufAppendFmt(pGen->pBody, "\nxvalue xlc_%s_%s(", (char*)pszName, (char*)pszMethodName);

					for ( int j = 0; j < iMethodParamCount; j++ ) {
						if ( j > 0 ) xlCodeBufAppend(pGen->pBody, ", ");
						xvalue xParam = xlAstArrayGet(xMethodParams, j);
						str pszParamName = xlAstGetStr(xParam, (str)"name");
						xlCodeBufAppendFmt(pGen->pBody, "xvalue xlv_%s", (char*)pszParamName);
					}
					if ( iMethodParamCount == 0 ) xlCodeBufAppend(pGen->pBody, "void");
					xlCodeBufAppend(pGen->pBody, ")\n{\n");
				} else {
					// 实例方法声明
					xlCodeBufAppendFmt(pGen->pHeader, "xvalue xlc_%s_%s(xvalue xlv_self", (char*)pszName, (char*)pszMethodName);

					int iMethodParamCount = xlAstArrayLen(xMethodParams);
					for ( int j = 0; j < iMethodParamCount; j++ ) {
						xvalue xParam = xlAstArrayGet(xMethodParams, j);
						str pszParamName = xlAstGetStr(xParam, (str)"name");
						xlCodeBufAppendFmt(pGen->pHeader, ", xvalue xlv_%s", (char*)pszParamName);
					}
					xlCodeBufAppend(pGen->pHeader, ");\n");

					// 实例方法定义
					xlCodeBufAppendFmt(pGen->pBody, "\nxvalue xlc_%s_%s(xvalue xlv_self", (char*)pszName, (char*)pszMethodName);

					for ( int j = 0; j < iMethodParamCount; j++ ) {
						xvalue xParam = xlAstArrayGet(xMethodParams, j);
						str pszParamName = xlAstGetStr(xParam, (str)"name");
						xlCodeBufAppendFmt(pGen->pBody, ", xvalue xlv_%s", (char*)pszParamName);
					}
					xlCodeBufAppend(pGen->pBody, ")\n{\n");
				}

				pGen->iIndent = 1;
				xlCodegenStmt(pGen, xMethodBody);
				pGen->iIndent = 0;

				xlCodeBufAppend(pGen->pBody, "\treturn xvoCreateNull();\n");
				xlCodeBufAppend(pGen->pBody, "}\n");
			}
		}

		// 生成 meta 方法
		// meta get(name) - 属性获取拦截
		if ( xMetaGet ) {
			xvalue xMetaParams = xlAstGetNode(xMetaGet, (str)"params");
			xvalue xMetaBody = xlAstGetNode(xMetaGet, (str)"body");

			// 声明
			xlCodeBufAppendFmt(pGen->pHeader, "xvalue xlc_%s_meta_get(xvalue xlv_self, xvalue xlv_name);\n", (char*)pszName);

			// 实现
			xlCodeBufAppendFmt(pGen->pBody, "\nxvalue xlc_%s_meta_get(xvalue xlv_self, xvalue xlv_name)\n{\n", (char*)pszName);
			// 如果有参数名，使用它；否则使用默认的 xlv_name
			int iParamCount = xlAstArrayLen(xMetaParams);
			if ( iParamCount > 0 ) {
				xvalue xParam = xlAstArrayGet(xMetaParams, 0);
				str pszParamName = xlAstGetStr(xParam, (str)"name");
				if ( strcmp((char*)pszParamName, "name") != 0 ) {
					xlCodeBufAppendFmt(pGen->pBody, "\txvalue xlv_%s = xlv_name;\n", (char*)pszParamName);
				}
			}
			pGen->iIndent = 1;
			xlCodegenStmt(pGen, xMetaBody);
			pGen->iIndent = 0;
			xlCodeBufAppend(pGen->pBody, "\treturn xvoCreateNull();\n");
			xlCodeBufAppend(pGen->pBody, "}\n");
		}

		// meta let(name, value) - 属性设置拦截
		if ( xMetaLet ) {
			xvalue xMetaParams = xlAstGetNode(xMetaLet, (str)"params");
			xvalue xMetaBody = xlAstGetNode(xMetaLet, (str)"body");

			// 声明
			xlCodeBufAppendFmt(pGen->pHeader, "xvalue xlc_%s_meta_let(xvalue xlv_self, xvalue xlv_name, xvalue xlv_value);\n", (char*)pszName);

			// 实现
			xlCodeBufAppendFmt(pGen->pBody, "\nxvalue xlc_%s_meta_let(xvalue xlv_self, xvalue xlv_name, xvalue xlv_value)\n{\n", (char*)pszName);
			// 映射参数名
			int iParamCount = xlAstArrayLen(xMetaParams);
			if ( iParamCount >= 1 ) {
				xvalue xParam = xlAstArrayGet(xMetaParams, 0);
				str pszParamName = xlAstGetStr(xParam, (str)"name");
				if ( strcmp((char*)pszParamName, "name") != 0 ) {
					xlCodeBufAppendFmt(pGen->pBody, "\txvalue xlv_%s = xlv_name;\n", (char*)pszParamName);
				}
			}
			if ( iParamCount >= 2 ) {
				xvalue xParam = xlAstArrayGet(xMetaParams, 1);
				str pszParamName = xlAstGetStr(xParam, (str)"name");
				if ( strcmp((char*)pszParamName, "value") != 0 ) {
					xlCodeBufAppendFmt(pGen->pBody, "\txvalue xlv_%s = xlv_value;\n", (char*)pszParamName);
				}
			}
			pGen->iIndent = 1;
			xlCodegenStmt(pGen, xMetaBody);
			pGen->iIndent = 0;
			xlCodeBufAppend(pGen->pBody, "\treturn xvoCreateNull();\n");
			xlCodeBufAppend(pGen->pBody, "}\n");
		}

		// meta call(...) - 对象作为函数调用拦截
		if ( xMetaCall ) {
			xvalue xMetaParams = xlAstGetNode(xMetaCall, (str)"params");
			xvalue xMetaBody = xlAstGetNode(xMetaCall, (str)"body");

			// 声明
			xlCodeBufAppendFmt(pGen->pHeader, "xvalue xlc_%s_meta_call(xvalue xlv_self, xvalue xlv_args);\n", (char*)pszName);

			// 实现
			xlCodeBufAppendFmt(pGen->pBody, "\nxvalue xlc_%s_meta_call(xvalue xlv_self, xvalue xlv_args)\n{\n", (char*)pszName);
			// 映射参数
			int iParamCount = xlAstArrayLen(xMetaParams);
			for ( int p = 0; p < iParamCount; p++ ) {
				xvalue xParam = xlAstArrayGet(xMetaParams, p);
				str pszParamName = xlAstGetStr(xParam, (str)"name");
				xlCodeBufAppendFmt(pGen->pBody, "\txvalue xlv_%s = xvoArrayGetValue(xlv_args, %d);\n", (char*)pszParamName, p);
			}
			pGen->iIndent = 1;
			xlCodegenStmt(pGen, xMetaBody);
			pGen->iIndent = 0;
			xlCodeBufAppend(pGen->pBody, "\treturn xvoCreateNull();\n");
			xlCodeBufAppend(pGen->pBody, "}\n");
		}

		// 运算符重载元方法 - 生成辅助宏
		#define XLCODEGEN_META_BINARY(metaVar, metaName) \
			if ( metaVar ) { \
				xvalue xMetaParams = xlAstGetNode(metaVar, (str)"params"); \
				xvalue xMetaBody = xlAstGetNode(metaVar, (str)"body"); \
				xlCodeBufAppendFmt(pGen->pHeader, "xvalue xlc_%s_meta_%s(xvalue xlv_self, xvalue xlv_other);\n", (char*)pszName, metaName); \
				xlCodeBufAppendFmt(pGen->pBody, "\nxvalue xlc_%s_meta_%s(xvalue xlv_self, xvalue xlv_other)\n{\n", (char*)pszName, metaName); \
				int iPC = xlAstArrayLen(xMetaParams); \
				if ( iPC > 0 ) { \
					xvalue xP = xlAstArrayGet(xMetaParams, 0); \
					str pN = xlAstGetStr(xP, (str)"name"); \
					if ( strcmp((char*)pN, "other") != 0 ) \
						xlCodeBufAppendFmt(pGen->pBody, "\txvalue xlv_%s = xlv_other;\n", (char*)pN); \
				} \
				pGen->iIndent = 1; \
				xlCodegenStmt(pGen, xMetaBody); \
				pGen->iIndent = 0; \
				xlCodeBufAppend(pGen->pBody, "\treturn xvoCreateNull();\n"); \
				xlCodeBufAppend(pGen->pBody, "}\n"); \
			}

		#define XLCODEGEN_META_UNARY(metaVar, metaName) \
			if ( metaVar ) { \
				xvalue xMetaBody = xlAstGetNode(metaVar, (str)"body"); \
				xlCodeBufAppendFmt(pGen->pHeader, "xvalue xlc_%s_meta_%s(xvalue xlv_self);\n", (char*)pszName, metaName); \
				xlCodeBufAppendFmt(pGen->pBody, "\nxvalue xlc_%s_meta_%s(xvalue xlv_self)\n{\n", (char*)pszName, metaName); \
				pGen->iIndent = 1; \
				xlCodegenStmt(pGen, xMetaBody); \
				pGen->iIndent = 0; \
				xlCodeBufAppend(pGen->pBody, "\treturn xvoCreateNull();\n"); \
				xlCodeBufAppend(pGen->pBody, "}\n"); \
			}

		// 二元运算符
		XLCODEGEN_META_BINARY(xMetaAdd, "add")
		XLCODEGEN_META_BINARY(xMetaSub, "sub")
		XLCODEGEN_META_BINARY(xMetaMul, "mul")
		XLCODEGEN_META_BINARY(xMetaDiv, "div")
		XLCODEGEN_META_BINARY(xMetaMod, "mod")
		XLCODEGEN_META_BINARY(xMetaPow, "pow")
		XLCODEGEN_META_BINARY(xMetaEq, "eq")
		XLCODEGEN_META_BINARY(xMetaLt, "lt")
		XLCODEGEN_META_BINARY(xMetaLe, "le")

		// 一元运算符
		XLCODEGEN_META_UNARY(xMetaNeg, "neg")
		XLCODEGEN_META_UNARY(xMetaLen, "len")

		// meta index(key) - 索引读取
		if ( xMetaIndex ) {
			xvalue xMetaParams = xlAstGetNode(xMetaIndex, (str)"params");
			xvalue xMetaBody = xlAstGetNode(xMetaIndex, (str)"body");
			xlCodeBufAppendFmt(pGen->pHeader, "xvalue xlc_%s_meta_index(xvalue xlv_self, xvalue xlv_key);\n", (char*)pszName);
			xlCodeBufAppendFmt(pGen->pBody, "\nxvalue xlc_%s_meta_index(xvalue xlv_self, xvalue xlv_key)\n{\n", (char*)pszName);
			int iPC = xlAstArrayLen(xMetaParams);
			if ( iPC > 0 ) {
				xvalue xP = xlAstArrayGet(xMetaParams, 0);
				str pN = xlAstGetStr(xP, (str)"name");
				if ( strcmp((char*)pN, "key") != 0 )
					xlCodeBufAppendFmt(pGen->pBody, "\txvalue xlv_%s = xlv_key;\n", (char*)pN);
			}
			pGen->iIndent = 1;
			xlCodegenStmt(pGen, xMetaBody);
			pGen->iIndent = 0;
			xlCodeBufAppend(pGen->pBody, "\treturn xvoCreateNull();\n");
			xlCodeBufAppend(pGen->pBody, "}\n");
		}

		// meta newindex(key, val) - 索引写入
		if ( xMetaNewindex ) {
			xvalue xMetaParams = xlAstGetNode(xMetaNewindex, (str)"params");
			xvalue xMetaBody = xlAstGetNode(xMetaNewindex, (str)"body");
			xlCodeBufAppendFmt(pGen->pHeader, "xvalue xlc_%s_meta_newindex(xvalue xlv_self, xvalue xlv_key, xvalue xlv_val);\n", (char*)pszName);
			xlCodeBufAppendFmt(pGen->pBody, "\nxvalue xlc_%s_meta_newindex(xvalue xlv_self, xvalue xlv_key, xvalue xlv_val)\n{\n", (char*)pszName);
			int iPC = xlAstArrayLen(xMetaParams);
			if ( iPC >= 1 ) {
				xvalue xP = xlAstArrayGet(xMetaParams, 0);
				str pN = xlAstGetStr(xP, (str)"name");
				if ( strcmp((char*)pN, "key") != 0 )
					xlCodeBufAppendFmt(pGen->pBody, "\txvalue xlv_%s = xlv_key;\n", (char*)pN);
			}
			if ( iPC >= 2 ) {
				xvalue xP = xlAstArrayGet(xMetaParams, 1);
				str pN = xlAstGetStr(xP, (str)"name");
				if ( strcmp((char*)pN, "val") != 0 )
					xlCodeBufAppendFmt(pGen->pBody, "\txvalue xlv_%s = xlv_val;\n", (char*)pN);
			}
			pGen->iIndent = 1;
			xlCodegenStmt(pGen, xMetaBody);
			pGen->iIndent = 0;
			xlCodeBufAppend(pGen->pBody, "\treturn xvoCreateNull();\n");
			xlCodeBufAppend(pGen->pBody, "}\n");
		}

		// meta iter() - 迭代器
		XLCODEGEN_META_UNARY(xMetaIter, "iter")
		XLCODEGEN_META_UNARY(xMetaNext, "next")

		// meta to(type) - 类型转换
		if ( xMetaTo ) {
			xvalue xMetaParams = xlAstGetNode(xMetaTo, (str)"params");
			xvalue xMetaBody = xlAstGetNode(xMetaTo, (str)"body");
			xlCodeBufAppendFmt(pGen->pHeader, "xvalue xlc_%s_meta_to(xvalue xlv_self, xvalue xlv_type);\n", (char*)pszName);
			xlCodeBufAppendFmt(pGen->pBody, "\nxvalue xlc_%s_meta_to(xvalue xlv_self, xvalue xlv_type)\n{\n", (char*)pszName);
			int iPC = xlAstArrayLen(xMetaParams);
			if ( iPC > 0 ) {
				xvalue xP = xlAstArrayGet(xMetaParams, 0);
				str pN = xlAstGetStr(xP, (str)"name");
				if ( pN && (strcmp((char*)pN, "type") != 0) )
					xlCodeBufAppendFmt(pGen->pBody, "\txvalue xlv_%s = xlv_type;\n", (char*)pN);
			}
			pGen->iIndent = 1;
			xlCodegenStmt(pGen, xMetaBody);
			pGen->iIndent = 0;
			xlCodeBufAppend(pGen->pBody, "\treturn xvoCreateNull();\n");
			xlCodeBufAppend(pGen->pBody, "}\n");
		}

		// meta enter/exit - with 语句支持
		XLCODEGEN_META_UNARY(xMetaEnter, "enter")
		XLCODEGEN_META_UNARY(xMetaExit, "exit")

		#undef XLCODEGEN_META_BINARY
		#undef XLCODEGEN_META_UNARY

		// 生成静态初始化函数实现
		bool bHasStaticInit = false;
		for ( int i = 0; i < iMemberCount; i++ ) {
			xvalue xMember = xlAstArrayGet(xMembers, i);
			str pszMemberKind = xlAstGetKind(xMember);
			if ( strcmp((char*)pszMemberKind, XL_AST_VAR_DECL) == 0 ) {
				bool bIsStatic = xlAstGetBool(xMember, (str)"isStatic");
				if ( bIsStatic ) {
					bHasStaticInit = true;
					break;
				}
			}
		}

		// 记录类是否有静态成员
		pGen->arrClassHasStatic[iClassIdx] = bHasStaticInit;

		if ( bHasStaticInit ) {
			xlCodeBufAppendFmt(pGen->pBody, "\nstatic void xlc_%s_static_init(void)\n{\n", (char*)pszName);
			for ( int i = 0; i < iMemberCount; i++ ) {
				xvalue xMember = xlAstArrayGet(xMembers, i);
				str pszMemberKind = xlAstGetKind(xMember);
				if ( strcmp((char*)pszMemberKind, XL_AST_VAR_DECL) == 0 ) {
					bool bIsStatic = xlAstGetBool(xMember, (str)"isStatic");
					if ( bIsStatic ) {
						str pszFieldName = xlAstGetStr(xMember, (str)"name");
						xvalue xInit = xlAstGetNode(xMember, (str)"init");
						xlCodeBufAppendFmt(pGen->pBody, "\txlc_%s_%s = ", (char*)pszName, (char*)pszFieldName);
						if ( xInit ) {
							xlCodegenExpr(pGen, pGen->pBody, xInit);
						} else {
							xlCodeBufAppend(pGen->pBody, "xvoCreateNull()");
						}
						xlCodeBufAppend(pGen->pBody, ";\n");
					}
				}
			}
			xlCodeBufAppend(pGen->pBody, "}\n");
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
	xlCodeBufAppend(pGen->pHeader, "#include \"xrt/xrt.h\"\n");
	xlCodeBufAppend(pGen->pHeader, "\n");

	// 运行时辅助函数
	xlCodeBufAppend(pGen->pHeader, "// xlang 运行时\n");
	xlCodeBufAppend(pGen->pHeader, "#define xvoIsTrue(x) (xvoGetBool(x) || xvoGetInt(x) || xvoGetFloat(x))\n");
	xlCodeBufAppend(pGen->pHeader, "\n");

	// 运算符重载分发函数前向声明
	xlCodeBufAppend(pGen->pHeader, "// 运算符重载分发函数（在 body 中实现）\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_op_add(xvalue a, xvalue b);\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_op_sub(xvalue a, xvalue b);\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_op_mul(xvalue a, xvalue b);\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_op_div(xvalue a, xvalue b);\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_op_mod(xvalue a, xvalue b);\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_op_pow(xvalue a, xvalue b);\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_op_neg(xvalue a);\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_op_eq(xvalue a, xvalue b);\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_op_lt(xvalue a, xvalue b);\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_op_le(xvalue a, xvalue b);\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_op_index(xvalue obj, xvalue key);\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_op_newindex(xvalue obj, xvalue key, xvalue val);\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_op_len(xvalue obj);\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_op_iter(xvalue obj);\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_op_next(xvalue obj);\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_op_to(xvalue obj, xvalue type);\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_op_enter(xvalue obj);\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_op_exit(xvalue obj);\n");
	xlCodeBufAppend(pGen->pHeader, "static int xl_has_meta_op(xvalue obj, const char* op);\n");
	xlCodeBufAppend(pGen->pHeader, "\n");

	// 检查是否是自定义类对象
	xlCodeBufAppend(pGen->pHeader, "static int xl_is_custom_obj(xvalue obj) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(obj) != XVO_DT_TABLE ) return 0;\n");
	xlCodeBufAppend(pGen->pHeader, "\txvalue cn = xvoTableGetValue(obj, (str)\"__class__\", 0);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn (cn && (xvoType(cn) == XVO_DT_TEXT));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_add(xvalue a, xvalue b) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t// 检查运算符重载\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xl_is_custom_obj(a) && xl_has_meta_op(a, \"add\") ) return xl_op_add(a, b);\n");
	xlCodeBufAppend(pGen->pHeader, "\tint ta = xvoType(a), tb = xvoType(b);\n");
	xlCodeBufAppend(pGen->pHeader, "\t// 字符串拼接\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( (ta == XVO_DT_TEXT) || (tb == XVO_DT_TEXT) ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tchar sa[64] = {0}, sb[64] = {0};\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tstr pa = (ta == XVO_DT_TEXT) ? xvoGetText(a) : (str)sa;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tstr pb = (tb == XVO_DT_TEXT) ? xvoGetText(b) : (str)sb;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tif ( ta == XVO_DT_INT ) snprintf(sa, 64, \"%lld\", (long long)xvoGetInt(a));\n");
	xlCodeBufAppend(pGen->pHeader, "\t\telse if ( ta == XVO_DT_FLOAT ) snprintf(sa, 64, \"%g\", xvoGetFloat(a));\n");
	xlCodeBufAppend(pGen->pHeader, "\t\telse if ( ta == XVO_DT_BOOL ) snprintf(sa, 64, \"%s\", xvoGetBool(a) ? \"true\" : \"false\");\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tif ( tb == XVO_DT_INT ) snprintf(sb, 64, \"%lld\", (long long)xvoGetInt(b));\n");
	xlCodeBufAppend(pGen->pHeader, "\t\telse if ( tb == XVO_DT_FLOAT ) snprintf(sb, 64, \"%g\", xvoGetFloat(b));\n");
	xlCodeBufAppend(pGen->pHeader, "\t\telse if ( tb == XVO_DT_BOOL ) snprintf(sb, 64, \"%s\", xvoGetBool(b) ? \"true\" : \"false\");\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tu32 la = (u32)strlen((char*)pa), lb = (u32)strlen((char*)pb);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tstr result = (str)malloc(la + lb + 1);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tmemcpy(result, pa, la);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tmemcpy(result + la, pb, lb + 1);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn xvoCreateText(result, la + lb, TRUE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\t// 数组合并\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( (ta == XVO_DT_ARRAY) && (tb == XVO_DT_ARRAY) ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\txvalue arr = xvoCreateArray();\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tu32 la = xvoArrayItemCount(a), lb = xvoArrayItemCount(b);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tfor ( u32 i = 0; i < la; i++ ) xvoArrayAppendValue(arr, xvoArrayGetValue(a, i), FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tfor ( u32 i = 0; i < lb; i++ ) xvoArrayAppendValue(arr, xvoArrayGetValue(b, i), FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn arr;\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\t// 数字加法\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( (ta == XVO_DT_INT) && (tb == XVO_DT_INT) )\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn xvoCreateInt(xvoGetInt(a) + xvoGetInt(b));\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateFloat(xvoGetFloat(a) + xvoGetFloat(b));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_sub(xvalue a, xvalue b) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xl_is_custom_obj(a) && xl_has_meta_op(a, \"sub\") ) return xl_op_sub(a, b);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( (xvoType(a) == XVO_DT_INT) && (xvoType(b) == XVO_DT_INT) )\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn xvoCreateInt(xvoGetInt(a) - xvoGetInt(b));\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateFloat(xvoGetFloat(a) - xvoGetFloat(b));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_mul(xvalue a, xvalue b) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xl_is_custom_obj(a) && xl_has_meta_op(a, \"mul\") ) return xl_op_mul(a, b);\n");
	xlCodeBufAppend(pGen->pHeader, "\tint ta = xvoType(a), tb = xvoType(b);\n");
	xlCodeBufAppend(pGen->pHeader, "\t// 字符串重复: \"ab\" * 3 = \"ababab\"\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( (ta == XVO_DT_TEXT) && (tb == XVO_DT_INT) ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tstr s = xvoGetText(a);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\ti64 n = xvoGetInt(b);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tif ( n <= 0 ) return xvoCreateText((str)\"\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tu32 slen = xvoGetSize(a);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tu32 newlen = slen * (u32)n;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tstr result = (str)malloc(newlen + 1);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tfor ( i64 i = 0; i < n; i++ ) memcpy(result + i * slen, s, slen);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tresult[newlen] = 0;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn xvoCreateText(result, newlen, TRUE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\t// 数组重复: [1,2] * 3 = [1,2,1,2,1,2]\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( (ta == XVO_DT_ARRAY) && (tb == XVO_DT_INT) ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\ti64 n = xvoGetInt(b);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\txvalue arr = xvoCreateArray();\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tif ( n <= 0 ) return arr;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tu32 len = xvoArrayItemCount(a);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tfor ( i64 j = 0; j < n; j++ ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t\tfor ( u32 i = 0; i < len; i++ ) xvoArrayAppendValue(arr, xvoArrayGetValue(a, i), FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn arr;\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\t// 数字乘法\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( (ta == XVO_DT_INT) && (tb == XVO_DT_INT) )\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn xvoCreateInt(xvoGetInt(a) * xvoGetInt(b));\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateFloat(xvoGetFloat(a) * xvoGetFloat(b));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_div(xvalue a, xvalue b) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xl_is_custom_obj(a) && xl_has_meta_op(a, \"div\") ) return xl_op_div(a, b);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateFloat(xvoGetFloat(a) / xvoGetFloat(b));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_mod(xvalue a, xvalue b) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xl_is_custom_obj(a) && xl_has_meta_op(a, \"mod\") ) return xl_op_mod(a, b);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateInt(xvoGetInt(a) % xvoGetInt(b));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_pow(xvalue a, xvalue b) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xl_is_custom_obj(a) && xl_has_meta_op(a, \"pow\") ) return xl_op_pow(a, b);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( (xvoType(a) == XVO_DT_INT) && (xvoType(b) == XVO_DT_INT) ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\ti64 base = xvoGetInt(a), exp = xvoGetInt(b), result = 1;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tif ( exp >= 0 ) { while ( exp-- > 0 ) result *= base; return xvoCreateInt(result); }\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateFloat(pow(xvoGetFloat(a), xvoGetFloat(b)));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_neg(xvalue a) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xl_is_custom_obj(a) && xl_has_meta_op(a, \"neg\") ) return xl_op_neg(a);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(a) == XVO_DT_INT ) return xvoCreateInt(-xvoGetInt(a));\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateFloat(-xvoGetFloat(a));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// 前缀自增: ++x (先增后返回)
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_preinc(xvalue *p) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(*p) == XVO_DT_INT ) *p = xvoCreateInt(xvoGetInt(*p) + 1);\n");
	xlCodeBufAppend(pGen->pHeader, "\telse *p = xvoCreateFloat(xvoGetFloat(*p) + 1.0);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn *p;\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// 前缀自减: --x (先减后返回)
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_predec(xvalue *p) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(*p) == XVO_DT_INT ) *p = xvoCreateInt(xvoGetInt(*p) - 1);\n");
	xlCodeBufAppend(pGen->pHeader, "\telse *p = xvoCreateFloat(xvoGetFloat(*p) - 1.0);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn *p;\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// 后缀自增: x++ (先返回后增)
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_postinc(xvalue *p) {\n");
	xlCodeBufAppend(pGen->pHeader, "\txvalue old = *p;\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(*p) == XVO_DT_INT ) *p = xvoCreateInt(xvoGetInt(*p) + 1);\n");
	xlCodeBufAppend(pGen->pHeader, "\telse *p = xvoCreateFloat(xvoGetFloat(*p) + 1.0);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn old;\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// 后缀自减: x-- (先返回后减)
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_postdec(xvalue *p) {\n");
	xlCodeBufAppend(pGen->pHeader, "\txvalue old = *p;\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(*p) == XVO_DT_INT ) *p = xvoCreateInt(xvoGetInt(*p) - 1);\n");
	xlCodeBufAppend(pGen->pHeader, "\telse *p = xvoCreateFloat(xvoGetFloat(*p) - 1.0);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn old;\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_not(xvalue a) { return xvoCreateBool(!xvoIsTrue(a)); }\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_eq(xvalue a, xvalue b) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xl_is_custom_obj(a) && xl_has_meta_op(a, \"eq\") ) return xl_op_eq(a, b);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateBool(xvoGetInt(a) == xvoGetInt(b));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_ne(xvalue a, xvalue b) { return xvoCreateBool(!xvoIsTrue(xl_eq(a, b))); }\n");
	// 约等于运算符 - 用于浮点数比较，使用 1e-9 作为容差
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_approx(xvalue a, xvalue b) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tdouble da = xvoGetFloat(a), db = xvoGetFloat(b);\n");
	xlCodeBufAppend(pGen->pHeader, "\tdouble diff = da - db;\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( diff < 0 ) diff = -diff;\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateBool(diff < 1e-9);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_lt(xvalue a, xvalue b) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xl_is_custom_obj(a) && xl_has_meta_op(a, \"lt\") ) return xl_op_lt(a, b);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateBool(xvoGetInt(a) < xvoGetInt(b));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_le(xvalue a, xvalue b) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xl_is_custom_obj(a) && xl_has_meta_op(a, \"le\") ) return xl_op_le(a, b);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateBool(xvoGetInt(a) <= xvoGetInt(b));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_gt(xvalue a, xvalue b) { return xl_lt(b, a); }\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_ge(xvalue a, xvalue b) { return xl_le(b, a); }\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_and(xvalue a, xvalue b) { return xvoCreateBool(xvoIsTrue(a) && xvoIsTrue(b)); }\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_or(xvalue a, xvalue b) { return xvoCreateBool(xvoIsTrue(a) || xvoIsTrue(b)); }\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_coalesce(xvalue a, xvalue b) { return xvoType(a) == XVO_DT_NULL ? b : a; }\n");
	xlCodeBufAppend(pGen->pHeader, "static i64 xl__len_i64(xvalue obj) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xl_is_custom_obj(obj) && xl_has_meta_op(obj, \"len\") ) return xvoGetInt(xl_op_len(obj));\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(obj) == XVO_DT_TEXT ) return xvoGetSize(obj);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(obj) == XVO_DT_ARRAY ) return xvoArrayItemCount(obj);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(obj) == XVO_DT_LIST ) return xvoListItemCount(obj);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn 0;\n");
	xlCodeBufAppend(pGen->pHeader, "}\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_index(xvalue obj, xvalue idx) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xl_is_custom_obj(obj) && xl_has_meta_op(obj, \"index\") ) return xl_op_index(obj, idx);\n");
	xlCodeBufAppend(pGen->pHeader, "\ti64 i = xvoGetInt(idx);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( i < 0 ) i = xl__len_i64(obj) + i;\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoArrayGetValue(obj, (u32)i);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_index_set(xvalue obj, xvalue idx, xvalue val) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xl_is_custom_obj(obj) && xl_has_meta_op(obj, \"newindex\") ) return xl_op_newindex(obj, idx, val);\n");
	xlCodeBufAppend(pGen->pHeader, "\ti64 i = xvoGetInt(idx);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( i < 0 ) i = xl__len_i64(obj) + i;\n");
	xlCodeBufAppend(pGen->pHeader, "\txvoArraySetValue(obj, (u32)i, val, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn val;\n");
	xlCodeBufAppend(pGen->pHeader, "}\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_slice(xvalue obj, xvalue start, xvalue end) {\n");
	xlCodeBufAppend(pGen->pHeader, "\ti64 len = xl__len_i64(obj);\n");
	xlCodeBufAppend(pGen->pHeader, "\ti64 s = xvoGetInt(start);\n");
	xlCodeBufAppend(pGen->pHeader, "\ti64 e = xvoGetInt(end);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( s < 0 ) s = len + s;\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( e < 0 ) e = len + e;\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( s < 0 ) s = 0;\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( e > len ) e = len;\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(obj) == XVO_DT_TEXT ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tstr p = xvoGetText(obj);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn xvoCreateText(p + s, (u32)(e - s), FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\txvalue arr = xvoCreateArray();\n");
	xlCodeBufAppend(pGen->pHeader, "\tfor ( i64 i = s; i < e; i++ ) xvoArrayAppendValue(arr, xvoArrayGetValue(obj, (u32)i), FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn arr;\n");
	xlCodeBufAppend(pGen->pHeader, "}\n");
	// meta 方法前向声明（在 body 中实现，但 xl_member 需要使用）
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_meta_get(xvalue obj, const char* name);\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_meta_let(xvalue obj, const char* name, xvalue val);\n");
	xlCodeBufAppend(pGen->pHeader, "static int xl_has_meta_get(xvalue obj);\n");
	xlCodeBufAppend(pGen->pHeader, "static int xl_has_meta_let(xvalue obj);\n");
	// xl_member 支持 meta get
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_member(xvalue obj, const char* name) {\n");
	xlCodeBufAppend(pGen->pHeader, "\txvalue val = xvoTableGetValue(obj, (str)name, 0);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( (val == NULL) || (xvoType(val) == XVO_DT_NULL) ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tif ( xl_has_meta_get(obj) ) return xl_meta_get(obj, name);\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn val ? val : xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pHeader, "}\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_safe_member(xvalue obj, const char* name) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( !obj || (xvoType(obj) == XVO_DT_NULL) ) return xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xl_member(obj, name);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n");
	// xl_member_set 支持 meta let
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_member_set(xvalue obj, const char* name, xvalue val) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xl_has_meta_let(obj) ) return xl_meta_let(obj, name, val);\n");
	xlCodeBufAppend(pGen->pHeader, "\txvoTableSetValue(obj, (str)name, 0, val, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn val;\n");
	xlCodeBufAppend(pGen->pHeader, "}\n");
	xlCodeBufAppend(pGen->pHeader, "\n");

	// in 运算符 - 判断元素是否在容器中
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_in(xvalue val, xvalue container) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tint dt = xvoType(container);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( dt == XVO_DT_TEXT ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tstr s = xvoGetText(container);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tstr sub = xvoGetText(val);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn xvoCreateBool(strstr((char*)s, (char*)sub) != NULL);\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( dt == XVO_DT_ARRAY ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tu32 len = xvoArrayItemCount(container);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tfor ( u32 i = 0; i < len; i++ ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t\tif ( xvoIsTrue(xl_eq(val, xvoArrayGetValue(container, i))) ) return xvoCreateBool(TRUE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn xvoCreateBool(FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( dt == XVO_DT_TABLE ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tstr key = xvoGetText(val);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\txvalue v = xvoTableGetValue(container, key, 0);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn xvoCreateBool((v != NULL) && (xvoType(v) != XVO_DT_NULL));\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateBool(FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n");

	// is 运算符 - 类型判断
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_is(xvalue val, xvalue typeVal) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(typeVal) == XVO_DT_TEXT ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tstr typeName = xvoGetText(typeVal);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tint dt = xvoType(val);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tif ( strcmp((char*)typeName, \"int\") == 0 ) return xvoCreateBool(dt == XVO_DT_INT);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tif ( strcmp((char*)typeName, \"float\") == 0 ) return xvoCreateBool(dt == XVO_DT_FLOAT);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tif ( strcmp((char*)typeName, \"string\") == 0 ) return xvoCreateBool(dt == XVO_DT_TEXT);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tif ( strcmp((char*)typeName, \"bool\") == 0 ) return xvoCreateBool(dt == XVO_DT_BOOL);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tif ( strcmp((char*)typeName, \"null\") == 0 ) return xvoCreateBool(dt == XVO_DT_NULL);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tif ( strcmp((char*)typeName, \"array\") == 0 ) return xvoCreateBool(dt == XVO_DT_ARRAY);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tif ( strcmp((char*)typeName, \"list\") == 0 ) return xvoCreateBool(dt == XVO_DT_LIST);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tif ( strcmp((char*)typeName, \"table\") == 0 ) return xvoCreateBool(dt == XVO_DT_TABLE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateBool(xvoType(val) == xvoGetInt(typeVal));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n");

	// 位运算符
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_bitand(xvalue a, xvalue b) { return xvoCreateInt(xvoGetInt(a) & xvoGetInt(b)); }\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_bitor(xvalue a, xvalue b) { return xvoCreateInt(xvoGetInt(a) | xvoGetInt(b)); }\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_bitxor(xvalue a, xvalue b) { return xvoCreateInt(xvoGetInt(a) ^ xvoGetInt(b)); }\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_shl(xvalue a, xvalue b) { return xvoCreateInt(xvoGetInt(a) << xvoGetInt(b)); }\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_shr(xvalue a, xvalue b) { return xvoCreateInt(xvoGetInt(a) >> xvoGetInt(b)); }\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_bitnot(xvalue a) { return xvoCreateInt(~xvoGetInt(a)); }\n");
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

	// type 内置函数 - 返回类型名称字符串
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_type(xvalue v) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tswitch ( xvoType(v) ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_NULL:  return xvoCreateText((str)\"null\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_BOOL:  return xvoCreateText((str)\"bool\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_INT:   return xvoCreateText((str)\"int\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_FLOAT: return xvoCreateText((str)\"float\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_TEXT:  return xvoCreateText((str)\"string\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_TIME:  return xvoCreateText((str)\"time\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_ARRAY: return xvoCreateText((str)\"array\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_LIST:  return xvoCreateText((str)\"list\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_COLL:  return xvoCreateText((str)\"set\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_TABLE: {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t\txvalue cls = xvoTableGetValue(v, (str)\"__class__\", 0);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t\tif ( cls && (xvoType(cls) == XVO_DT_TEXT) ) return cls;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t\treturn xvoCreateText((str)\"table\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_FUNC:  return xvoCreateText((str)\"function\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tdefault: return xvoCreateText((str)\"unknown\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// typeof 内置函数 - 返回类型整数值 (可直接与类型字面量比较)
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_typeof(xvalue v) {\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateInt(xvoType(v));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// range 内置函数 - 创建范围数组
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_range(xvalue start, xvalue end, xvalue step) {\n");
	xlCodeBufAppend(pGen->pHeader, "\txvalue arr = xvoCreateArray();\n");
	xlCodeBufAppend(pGen->pHeader, "\ti64 iStart = xvoGetInt(start);\n");
	xlCodeBufAppend(pGen->pHeader, "\ti64 iEnd = xvoGetInt(end);\n");
	xlCodeBufAppend(pGen->pHeader, "\ti64 iStep;\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(step) == XVO_DT_NULL ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tiStep = (iStart <= iEnd) ? 1 : -1;\n");
	xlCodeBufAppend(pGen->pHeader, "\t} else {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tiStep = xvoGetInt(step);\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( iStep == 0 ) return arr;\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( iStep > 0 ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tfor ( i64 i = iStart; i < iEnd; i += iStep ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t\txvoArrayAppendValue(arr, xvoCreateInt(i), FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\t} else {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tfor ( i64 i = iStart; i > iEnd; i += iStep ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t\txvoArrayAppendValue(arr, xvoCreateInt(i), FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn arr;\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// input 内置函数 - 读取标准输入
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_input(xvalue prompt) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(prompt) == XVO_DT_TEXT ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tprintf(\"%s\", (char*)xvoGetText(prompt));\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tfflush(stdout);\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\tchar szBuf[4096] = {0};\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( fgets(szBuf, sizeof(szBuf), stdin) ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tsize_t len = strlen(szBuf);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tif ( (len > 0) && (szBuf[len-1] == '\\n') ) szBuf[--len] = 0;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tif ( (len > 0) && (szBuf[len-1] == '\\r') ) szBuf[--len] = 0;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn xvoCreateText((str)szBuf, (u32)len, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateText((str)\"\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// abs 内置函数
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_abs(xvalue v) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(v) == XVO_DT_INT ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\ti64 n = xvoGetInt(v);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn xvoCreateInt(n < 0 ? -n : n);\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\tf64 f = xvoGetFloat(v);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateFloat(f < 0 ? -f : f);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// min 内置函数
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_min(xvalue a, xvalue b) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( (xvoType(a) == XVO_DT_INT) && (xvoType(b) == XVO_DT_INT) ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\ti64 ia = xvoGetInt(a), ib = xvoGetInt(b);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn xvoCreateInt(ia < ib ? ia : ib);\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\tf64 fa = xvoGetFloat(a), fb = xvoGetFloat(b);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateFloat(fa < fb ? fa : fb);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// max 内置函数
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_max(xvalue a, xvalue b) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( (xvoType(a) == XVO_DT_INT) && (xvoType(b) == XVO_DT_INT) ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\ti64 ia = xvoGetInt(a), ib = xvoGetInt(b);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn xvoCreateInt(ia > ib ? ia : ib);\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\tf64 fa = xvoGetFloat(a), fb = xvoGetFloat(b);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateFloat(fa > fb ? fa : fb);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// round 内置函数 - 四舍五入
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_round(xvalue v) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(v) == XVO_DT_INT ) return v;\n");
	xlCodeBufAppend(pGen->pHeader, "\tf64 f = xvoGetFloat(v);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateInt((i64)(f >= 0 ? f + 0.5 : f - 0.5));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// floor 内置函数 - 向下取整
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_floor(xvalue v) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(v) == XVO_DT_INT ) return v;\n");
	xlCodeBufAppend(pGen->pHeader, "\tf64 f = xvoGetFloat(v);\n");
	xlCodeBufAppend(pGen->pHeader, "\ti64 i = (i64)f;\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateInt((f < 0 && f != i) ? i - 1 : i);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// ceil 内置函数 - 向上取整
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_ceil(xvalue v) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(v) == XVO_DT_INT ) return v;\n");
	xlCodeBufAppend(pGen->pHeader, "\tf64 f = xvoGetFloat(v);\n");
	xlCodeBufAppend(pGen->pHeader, "\ti64 i = (i64)f;\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateInt((f > 0 && f != i) ? i + 1 : i);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// chr 内置函数 - Unicode 码点转字符
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_chr(xvalue v) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tchar szBuf[8] = {0};\n");
	xlCodeBufAppend(pGen->pHeader, "\ti64 code = xvoGetInt(v);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( code < 0x80 ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tszBuf[0] = (char)code;\n");
	xlCodeBufAppend(pGen->pHeader, "\t} else if ( code < 0x800 ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tszBuf[0] = (char)(0xC0 | (code >> 6));\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tszBuf[1] = (char)(0x80 | (code & 0x3F));\n");
	xlCodeBufAppend(pGen->pHeader, "\t} else if ( code < 0x10000 ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tszBuf[0] = (char)(0xE0 | (code >> 12));\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tszBuf[1] = (char)(0x80 | ((code >> 6) & 0x3F));\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tszBuf[2] = (char)(0x80 | (code & 0x3F));\n");
	xlCodeBufAppend(pGen->pHeader, "\t} else {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tszBuf[0] = (char)(0xF0 | (code >> 18));\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tszBuf[1] = (char)(0x80 | ((code >> 12) & 0x3F));\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tszBuf[2] = (char)(0x80 | ((code >> 6) & 0x3F));\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tszBuf[3] = (char)(0x80 | (code & 0x3F));\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateText((str)szBuf, (u32)strlen(szBuf), FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// ord 内置函数 - 字符转 Unicode 码点
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_ord(xvalue v) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(v) != XVO_DT_TEXT ) return xvoCreateInt(0);\n");
	xlCodeBufAppend(pGen->pHeader, "\tconst unsigned char* s = (const unsigned char*)xvoGetText(v);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( !s || !*s ) return xvoCreateInt(0);\n");
	xlCodeBufAppend(pGen->pHeader, "\ti64 code = 0;\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( (s[0] & 0x80) == 0 ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcode = s[0];\n");
	xlCodeBufAppend(pGen->pHeader, "\t} else if ( (s[0] & 0xE0) == 0xC0 ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcode = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);\n");
	xlCodeBufAppend(pGen->pHeader, "\t} else if ( (s[0] & 0xF0) == 0xE0 ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcode = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);\n");
	xlCodeBufAppend(pGen->pHeader, "\t} else if ( (s[0] & 0xF8) == 0xF0 ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcode = ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateInt(code);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// hash 内置函数 - 获取哈希值
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_hash(xvalue v) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tu64 h = 0;\n");
	xlCodeBufAppend(pGen->pHeader, "\tswitch ( xvoType(v) ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_NULL:  h = 0; break;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_BOOL:  h = xvoGetBool(v) ? 1 : 0; break;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_INT:   h = (u64)xvoGetInt(v); break;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_FLOAT: { f64 f = xvoGetFloat(v); h = *(u64*)&f; } break;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_TEXT:  {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t\tconst char* s = (const char*)xvoGetText(v);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t\twhile ( *s ) { h = h * 31 + *s++; }\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t} break;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tdefault: h = (u64)(size_t)v; break;\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateInt((i64)h);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// id 内置函数 - 获取对象唯一标识（内存地址）
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_id(xvalue v) {\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateInt((i64)(size_t)v);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// int 类型转换函数
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_int(xvalue v) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t// 检查自定义对象的 meta to 方法\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xl_is_custom_obj(v) && xl_has_meta_op(v, \"to\") ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn xl_op_to(v, xvoCreateText((str)\"int\", 3, FALSE));\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\tswitch ( xvoType(v) ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_INT:   return v;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_FLOAT: return xvoCreateInt((i64)xvoGetFloat(v));\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_BOOL:  return xvoCreateInt(xvoGetBool(v) ? 1 : 0);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_TEXT:  return xvoCreateInt(atoll((char*)xvoGetText(v)));\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tdefault: return xvoCreateInt(0);\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// float 类型转换函数
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_float(xvalue v) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t// 检查自定义对象的 meta to 方法\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xl_is_custom_obj(v) && xl_has_meta_op(v, \"to\") ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn xl_op_to(v, xvoCreateText((str)\"float\", 5, FALSE));\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\tswitch ( xvoType(v) ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_FLOAT: return v;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_INT:   return xvoCreateFloat((f64)xvoGetInt(v));\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_BOOL:  return xvoCreateFloat(xvoGetBool(v) ? 1.0 : 0.0);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_TEXT:  return xvoCreateFloat(atof((char*)xvoGetText(v)));\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tdefault: return xvoCreateFloat(0.0);\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// string 类型转换函数
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_string(xvalue v) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t// 检查自定义对象的 meta to 方法\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xl_is_custom_obj(v) && xl_has_meta_op(v, \"to\") ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn xl_op_to(v, xvoCreateText((str)\"string\", 6, FALSE));\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\tchar szBuf[128] = {0};\n");
	xlCodeBufAppend(pGen->pHeader, "\tswitch ( xvoType(v) ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_TEXT:  return v;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_INT:   snprintf(szBuf, 128, \"%lld\", (long long)xvoGetInt(v)); break;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_FLOAT: snprintf(szBuf, 128, \"%g\", xvoGetFloat(v)); break;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_BOOL:  snprintf(szBuf, 128, \"%s\", xvoGetBool(v) ? \"true\" : \"false\"); break;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tcase XVO_DT_NULL:  snprintf(szBuf, 128, \"null\"); break;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tdefault:           snprintf(szBuf, 128, \"[object]\"); break;\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateText((str)szBuf, (u32)strlen(szBuf), FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// xl_tostring - 与 xl_string 相同，用于模板字符串
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_tostring(xvalue v) { return xl_string(v); }\n\n");

	// 模板字符串拼接函数
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_template_concat(int count, ...) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( count <= 0 ) return xvoCreateText((str)\"\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\tva_list args;\n");
	xlCodeBufAppend(pGen->pHeader, "\tva_start(args, count);\n");
	xlCodeBufAppend(pGen->pHeader, "\txvalue parts[64] = {0};\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( count > 64 ) count = 64;\n");
	xlCodeBufAppend(pGen->pHeader, "\tfor ( int i = 0; i < count; i++ ) parts[i] = va_arg(args, xvalue);\n");
	xlCodeBufAppend(pGen->pHeader, "\tva_end(args);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( count == 1 ) return parts[0];\n");
	xlCodeBufAppend(pGen->pHeader, "\t// 计算总长度并拼接\n");
	xlCodeBufAppend(pGen->pHeader, "\tsize_t iTotalLen = 0;\n");
	xlCodeBufAppend(pGen->pHeader, "\tfor ( int i = 0; i < count; i++ ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tif ( xvoType(parts[i]) == XVO_DT_TEXT ) iTotalLen += xvoGetSize(parts[i]);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\telse { parts[i] = xl_string(parts[i]); iTotalLen += xvoGetSize(parts[i]); }\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\tchar *pszResult = (char*)malloc(iTotalLen + 1);\n");
	xlCodeBufAppend(pGen->pHeader, "\tpszResult[0] = 0;\n");
	xlCodeBufAppend(pGen->pHeader, "\tfor ( int i = 0; i < count; i++ ) strcat(pszResult, (char*)xvoGetText(parts[i]));\n");
	xlCodeBufAppend(pGen->pHeader, "\txvalue result = xvoCreateText((str)pszResult, (u32)iTotalLen, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\tfree(pszResult);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn result;\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// bool 类型转换函数
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_bool(xvalue v) {\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateBool(xvoIsTrue(v));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// list 创建函数
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_list(int count, ...) {\n");
	xlCodeBufAppend(pGen->pHeader, "\txvalue lst = xvoCreateList();\n");
	xlCodeBufAppend(pGen->pHeader, "\tva_list args;\n");
	xlCodeBufAppend(pGen->pHeader, "\tva_start(args, count);\n");
	xlCodeBufAppend(pGen->pHeader, "\tfor ( int i = 0; i < count; i++ ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\txvoListSetValue(lst, (i64)i, va_arg(args, xvalue), FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\tva_end(args);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn lst;\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// list 方法调用辅助函数
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_list_method(xvalue lst, const char* method, xvalue args) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( strcmp(method, \"add\") == 0 ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tu32 len = xvoListItemCount(lst);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\txvalue val = xvoArrayGetValue(args, 0);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\txvoListSetValue(lst, (i64)len, val, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn lst;\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( strcmp(method, \"at\") == 0 ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\ti64 idx = xvoGetInt(xvoArrayGetValue(args, 0));\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn xvoListGetValue(lst, idx);\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( strcmp(method, \"put\") == 0 ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\ti64 idx = xvoGetInt(xvoArrayGetValue(args, 0));\n");
	xlCodeBufAppend(pGen->pHeader, "\t\txvalue val = xvoArrayGetValue(args, 1);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\txvoListSetValue(lst, idx, val, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn val;\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( strcmp(method, \"remove\") == 0 ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\ti64 idx = xvoGetInt(xvoArrayGetValue(args, 0));\n");
	xlCodeBufAppend(pGen->pHeader, "\t\txvoListRemove(lst, idx);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn lst;\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( strcmp(method, \"has\") == 0 ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\ti64 idx = xvoGetInt(xvoArrayGetValue(args, 0));\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn xvoCreateBool(xvoListExists(lst, idx));\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( strcmp(method, \"clear\") == 0 ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\txvoListClear(lst);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn lst;\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( strcmp(method, \"len\") == 0 ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn xvoCreateInt(xvoListItemCount(lst));\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// set 创建函数（用 table 模拟，key 是元素值的字符串表示）
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_set(int count, ...) {\n");
	xlCodeBufAppend(pGen->pHeader, "\txvalue s = xvoCreateTable();\n");
	xlCodeBufAppend(pGen->pHeader, "\tchar szKey[64];\n");
	xlCodeBufAppend(pGen->pHeader, "\tva_list args;\n");
	xlCodeBufAppend(pGen->pHeader, "\tva_start(args, count);\n");
	xlCodeBufAppend(pGen->pHeader, "\tfor ( int i = 0; i < count; i++ ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\txvalue v = va_arg(args, xvalue);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tswitch ( xvoType(v) ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t\tcase XVO_DT_INT: snprintf(szKey, 64, \"i%lld\", (long long)xvoGetInt(v)); break;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t\tcase XVO_DT_TEXT: snprintf(szKey, 64, \"s%s\", (char*)xvoGetText(v)); break;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t\tdefault: snprintf(szKey, 64, \"p%p\", (void*)v); break;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\t\txvoTableSetValue(s, (str)szKey, 0, v, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\tva_end(args);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn s;\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// set 方法调用辅助函数
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_set_method(xvalue s, const char* method, xvalue args) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tchar szKey[64];\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( strcmp(method, \"add\") == 0 ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\txvalue v = xvoArrayGetValue(args, 0);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tswitch ( xvoType(v) ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t\tcase XVO_DT_INT: snprintf(szKey, 64, \"i%lld\", (long long)xvoGetInt(v)); break;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t\tcase XVO_DT_TEXT: snprintf(szKey, 64, \"s%s\", (char*)xvoGetText(v)); break;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t\tdefault: snprintf(szKey, 64, \"p%p\", (void*)v); break;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\t\txvoTableSetValue(s, (str)szKey, 0, v, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn s;\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( strcmp(method, \"remove\") == 0 ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\txvalue v = xvoArrayGetValue(args, 0);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tswitch ( xvoType(v) ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t\tcase XVO_DT_INT: snprintf(szKey, 64, \"i%lld\", (long long)xvoGetInt(v)); break;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t\tcase XVO_DT_TEXT: snprintf(szKey, 64, \"s%s\", (char*)xvoGetText(v)); break;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t\tdefault: snprintf(szKey, 64, \"p%p\", (void*)v); break;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\t\txvoTableRemove(s, (str)szKey, 0);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn s;\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( strcmp(method, \"has\") == 0 ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\txvalue v = xvoArrayGetValue(args, 0);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tswitch ( xvoType(v) ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t\tcase XVO_DT_INT: snprintf(szKey, 64, \"i%lld\", (long long)xvoGetInt(v)); break;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t\tcase XVO_DT_TEXT: snprintf(szKey, 64, \"s%s\", (char*)xvoGetText(v)); break;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t\tdefault: snprintf(szKey, 64, \"p%p\", (void*)v); break;\n");
	xlCodeBufAppend(pGen->pHeader, "\t\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\t\txvalue found = xvoTableGetValue(s, (str)szKey, 0);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn xvoCreateBool((found != NULL) && (xvoType(found) != XVO_DT_NULL));\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( strcmp(method, \"clear\") == 0 ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\txvoTableClear(s);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn s;\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( strcmp(method, \"len\") == 0 ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn xvoCreateInt(xvoTableItemCount(s));\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// time 方法调用辅助函数
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_time_method(xvalue t, const char* method, xvalue args) {\n");
	xlCodeBufAppend(pGen->pHeader, "\txtime tv = xvoGetTime(t);\n");
	xlCodeBufAppend(pGen->pHeader, "\tint64 year; int month, day, hour, minute, second, weekday, dayOfYear;\n");
	xlCodeBufAppend(pGen->pHeader, "\txrtDecodeSerial(tv, &year, &month, &day, &hour, &minute, &second, &weekday, &dayOfYear);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( strcmp(method, \"year\") == 0 ) return xvoCreateInt(year);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( strcmp(method, \"month\") == 0 ) return xvoCreateInt(month);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( strcmp(method, \"day\") == 0 ) return xvoCreateInt(day);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( strcmp(method, \"hour\") == 0 ) return xvoCreateInt(hour);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( strcmp(method, \"minute\") == 0 ) return xvoCreateInt(minute);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( strcmp(method, \"second\") == 0 ) return xvoCreateInt(second);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( strcmp(method, \"weekday\") == 0 ) return xvoCreateInt(weekday);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( strcmp(method, \"dayOfYear\") == 0 ) return xvoCreateInt(dayOfYear);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( strcmp(method, \"format\") == 0 ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tstr fmt = xvoGetText(xvoArrayGetValue(args, 0));\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tstr s = xrtTimeFormat(tv, fmt);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\treturn xvoCreateText(s, (u32)strlen((char*)s), TRUE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( strcmp(method, \"timestamp\") == 0 ) return xvoCreateInt(tv);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// ========== 字符串内置函数 ==========

	// trim - 去除首尾空白
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_trim(xvalue v) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(v) != XVO_DT_TEXT ) return v;\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr s = xvoGetText(v);\n");
	xlCodeBufAppend(pGen->pHeader, "\tsize_t iSize = xvoGetSize(v);\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr result = xrtTrim(s, iSize, NULL, 0, FALSE, &iSize);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateText(result, (u32)iSize, TRUE);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// ltrim - 去除左侧空白
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_ltrim(xvalue v) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(v) != XVO_DT_TEXT ) return v;\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr s = xvoGetText(v);\n");
	xlCodeBufAppend(pGen->pHeader, "\tsize_t iSize = xvoGetSize(v);\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr result = xrtLTrim(s, iSize, NULL, 0, FALSE, &iSize);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateText(result, (u32)iSize, TRUE);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// rtrim - 去除右侧空白
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_rtrim(xvalue v) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(v) != XVO_DT_TEXT ) return v;\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr s = xvoGetText(v);\n");
	xlCodeBufAppend(pGen->pHeader, "\tsize_t iSize = xvoGetSize(v);\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr result = xrtRTrim(s, iSize, NULL, 0, FALSE, &iSize);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateText(result, (u32)iSize, TRUE);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// upper - 转大写
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_upper(xvalue v) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(v) != XVO_DT_TEXT ) return v;\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr s = xvoGetText(v);\n");
	xlCodeBufAppend(pGen->pHeader, "\tu32 iSize = xvoGetSize(v);\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr result = xrtUCase(s, iSize, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateText(result, iSize, TRUE);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// lower - 转小写
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_lower(xvalue v) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(v) != XVO_DT_TEXT ) return v;\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr s = xvoGetText(v);\n");
	xlCodeBufAppend(pGen->pHeader, "\tu32 iSize = xvoGetSize(v);\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr result = xrtLCase(s, iSize, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateText(result, iSize, TRUE);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// replace - 字符串替换
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_replace(xvalue src, xvalue old, xvalue rep) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(src) != XVO_DT_TEXT ) return src;\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr s = xvoGetText(src);\n");
	xlCodeBufAppend(pGen->pHeader, "\tsize_t iSize = xvoGetSize(src);\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr sOld = xvoGetText(old);\n");
	xlCodeBufAppend(pGen->pHeader, "\tsize_t iOldSize = xvoGetSize(old);\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr sRep = xvoGetText(rep);\n");
	xlCodeBufAppend(pGen->pHeader, "\tsize_t iRepSize = xvoGetSize(rep);\n");
	xlCodeBufAppend(pGen->pHeader, "\tsize_t iRetSize = 0;\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr result = xrtReplace(s, iSize, sOld, iOldSize, sRep, iRepSize, &iRetSize);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateText(result, (u32)iRetSize, TRUE);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// split - 分割字符串
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_split(xvalue src, xvalue sep) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(src) != XVO_DT_TEXT ) return xvoCreateArray();\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr s = xvoGetText(src);\n");
	xlCodeBufAppend(pGen->pHeader, "\tsize_t iSize = xvoGetSize(src);\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr sSep = xvoGetText(sep);\n");
	xlCodeBufAppend(pGen->pHeader, "\tsize_t iSepSize = xvoGetSize(sep);\n");
	xlCodeBufAppend(pGen->pHeader, "\tsize_t iRetSize = 0;\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr *arrParts = xrtSplit(s, iSize, sSep, iSepSize, FALSE, &iRetSize);\n");
	xlCodeBufAppend(pGen->pHeader, "\txvalue arr = xvoCreateArray();\n");
	xlCodeBufAppend(pGen->pHeader, "\tfor ( size_t i = 0; i < iRetSize; i++ ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\txvoArrayAppendText(arr, arrParts[i], 0, TRUE);\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn arr;\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// join - 连接数组
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_join(xvalue arr, xvalue sep) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(arr) != XVO_DT_ARRAY ) return xvoCreateText((str)\"\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr sSep = xvoType(sep) == XVO_DT_TEXT ? xvoGetText(sep) : (str)\"\";\n");
	xlCodeBufAppend(pGen->pHeader, "\tu32 iSepLen = xvoType(sep) == XVO_DT_TEXT ? xvoGetSize(sep) : 0;\n");
	xlCodeBufAppend(pGen->pHeader, "\tu32 iCount = xvoArrayItemCount(arr);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( iCount == 0 ) return xvoCreateText((str)\"\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\tsize_t iTotalLen = 0;\n");
	xlCodeBufAppend(pGen->pHeader, "\tfor ( u32 i = 0; i < iCount; i++ ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\txvalue v = xvoArrayGetValue(arr, i);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tif ( xvoType(v) == XVO_DT_TEXT ) iTotalLen += xvoGetSize(v);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tif ( i > 0 ) iTotalLen += iSepLen;\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\tchar *pszResult = (char*)malloc(iTotalLen + 1);\n");
	xlCodeBufAppend(pGen->pHeader, "\tpszResult[0] = 0;\n");
	xlCodeBufAppend(pGen->pHeader, "\tfor ( u32 i = 0; i < iCount; i++ ) {\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tif ( i > 0 && iSepLen > 0 ) strcat(pszResult, (char*)sSep);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\txvalue v = xvoArrayGetValue(arr, i);\n");
	xlCodeBufAppend(pGen->pHeader, "\t\tif ( xvoType(v) == XVO_DT_TEXT ) strcat(pszResult, (char*)xvoGetText(v));\n");
	xlCodeBufAppend(pGen->pHeader, "\t}\n");
	xlCodeBufAppend(pGen->pHeader, "\txvalue result = xvoCreateText((str)pszResult, (u32)strlen(pszResult), FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\tfree(pszResult);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn result;\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// find - 查找子串位置 (使用 strstr 避免 xrt memmem 的 off-by-one bug)
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_find(xvalue src, xvalue sub) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(src) != XVO_DT_TEXT ) return xvoCreateInt(-1);\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr s = xvoGetText(src);\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr sSub = xvoGetText(sub);\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr found = strstr(s, sSub);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateInt(found ? (i64)(found - s) : -1);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// contains - 是否包含子串 (使用 strstr 避免 xrt memmem 的 off-by-one bug)
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_contains(xvalue src, xvalue sub) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(src) != XVO_DT_TEXT ) return xvoCreateBool(FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr s = xvoGetText(src);\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr sSub = xvoGetText(sub);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateBool(strstr(s, sSub) != NULL);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// substr - 子串截取
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_substr(xvalue src, xvalue start, xvalue length) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(src) != XVO_DT_TEXT ) return xvoCreateText((str)\"\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr s = xvoGetText(src);\n");
	xlCodeBufAppend(pGen->pHeader, "\tu32 iSize = xvoGetSize(src);\n");
	xlCodeBufAppend(pGen->pHeader, "\ti64 iStart = xvoGetInt(start);\n");
	xlCodeBufAppend(pGen->pHeader, "\ti64 iLen = xvoType(length) == XVO_DT_NULL ? (i64)iSize - iStart : xvoGetInt(length);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( iStart < 0 ) iStart = 0;\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( iStart >= iSize ) return xvoCreateText((str)\"\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( iLen <= 0 ) return xvoCreateText((str)\"\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( iStart + iLen > iSize ) iLen = iSize - iStart;\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateText(s + iStart, (u32)iLen, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// startswith - 前缀判断
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_startswith(xvalue src, xvalue prefix) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(src) != XVO_DT_TEXT ) return xvoCreateBool(FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr s = xvoGetText(src);\n");
	xlCodeBufAppend(pGen->pHeader, "\tu32 iSize = xvoGetSize(src);\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr sPrefix = xvoGetText(prefix);\n");
	xlCodeBufAppend(pGen->pHeader, "\tu32 iPrefixSize = xvoGetSize(prefix);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( iPrefixSize > iSize ) return xvoCreateBool(FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateBool(memcmp(s, sPrefix, iPrefixSize) == 0);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// endswith - 后缀判断
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_endswith(xvalue src, xvalue suffix) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(src) != XVO_DT_TEXT ) return xvoCreateBool(FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr s = xvoGetText(src);\n");
	xlCodeBufAppend(pGen->pHeader, "\tu32 iSize = xvoGetSize(src);\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr sSuffix = xvoGetText(suffix);\n");
	xlCodeBufAppend(pGen->pHeader, "\tu32 iSuffixSize = xvoGetSize(suffix);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( iSuffixSize > iSize ) return xvoCreateBool(FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateBool(memcmp(s + iSize - iSuffixSize, sSuffix, iSuffixSize) == 0);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// repeat - 重复字符串
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_repeat(xvalue src, xvalue times) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(src) != XVO_DT_TEXT ) return xvoCreateText((str)\"\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr s = xvoGetText(src);\n");
	xlCodeBufAppend(pGen->pHeader, "\tu32 iSize = xvoGetSize(src);\n");
	xlCodeBufAppend(pGen->pHeader, "\ti64 iTimes = xvoGetInt(times);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( iTimes <= 0 ) return xvoCreateText((str)\"\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\tsize_t iLen = iSize * iTimes;\n");
	xlCodeBufAppend(pGen->pHeader, "\tchar *pszResult = (char*)malloc(iLen + 1);\n");
	xlCodeBufAppend(pGen->pHeader, "\tpszResult[0] = 0;\n");
	xlCodeBufAppend(pGen->pHeader, "\tfor ( i64 i = 0; i < iTimes; i++ ) strcat(pszResult, (char*)s);\n");
	xlCodeBufAppend(pGen->pHeader, "\txvalue result = xvoCreateText((str)pszResult, (u32)iLen, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\tfree(pszResult);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn result;\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// reverse - 反转字符串
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_reverse(xvalue src) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(src) != XVO_DT_TEXT ) return xvoCreateText((str)\"\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr s = xvoGetText(src);\n");
	xlCodeBufAppend(pGen->pHeader, "\tu32 iSize = xvoGetSize(src);\n");
	xlCodeBufAppend(pGen->pHeader, "\tchar *pszResult = (char*)malloc(iSize + 1);\n");
	xlCodeBufAppend(pGen->pHeader, "\tfor ( u32 i = 0; i < iSize; i++ ) pszResult[i] = s[iSize - 1 - i];\n");
	xlCodeBufAppend(pGen->pHeader, "\tpszResult[iSize] = 0;\n");
	xlCodeBufAppend(pGen->pHeader, "\txvalue result = xvoCreateText((str)pszResult, iSize, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\tfree(pszResult);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn result;\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// ========== 数学内置函数 ==========

	// rand - 随机整数
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_rand() {\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateInt((i64)xrtRand64());\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// random - 随机浮点数 0.0-1.0
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_random() {\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateFloat((f64)xrtRand64() / (f64)0xFFFFFFFFFFFFFFFFULL);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// randrange - 区间随机整数
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_randrange(xvalue a, xvalue b) {\n");
	xlCodeBufAppend(pGen->pHeader, "\ti64 iMin = xvoGetInt(a);\n");
	xlCodeBufAppend(pGen->pHeader, "\ti64 iMax = xvoGetInt(b);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( iMin > iMax ) { i64 t = iMin; iMin = iMax; iMax = t; }\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateInt(iMin + (i64)(xrtRand64() % (u64)(iMax - iMin + 1)));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// sqrt - 平方根
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_sqrt(xvalue v) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tf64 f = (xvoType(v) == XVO_DT_INT) ? (f64)xvoGetInt(v) : xvoGetFloat(v);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateFloat(sqrt(f));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// log - 自然对数
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_log(xvalue v) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tf64 f = (xvoType(v) == XVO_DT_INT) ? (f64)xvoGetInt(v) : xvoGetFloat(v);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateFloat(log(f));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// sin/cos/tan
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_sin(xvalue v) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tf64 f = (xvoType(v) == XVO_DT_INT) ? (f64)xvoGetInt(v) : xvoGetFloat(v);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateFloat(sin(f));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_cos(xvalue v) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tf64 f = (xvoType(v) == XVO_DT_INT) ? (f64)xvoGetInt(v) : xvoGetFloat(v);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateFloat(cos(f));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_tan(xvalue v) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tf64 f = (xvoType(v) == XVO_DT_INT) ? (f64)xvoGetInt(v) : xvoGetFloat(v);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateFloat(tan(f));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// ========== 时间内置函数 ==========

	// year/month/day/hour/minute/second
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_year(xvalue t) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(t) != XVO_DT_TIME ) return xvoCreateInt(0);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateInt(xrtYear(xvoGetTime(t)));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_month(xvalue t) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(t) != XVO_DT_TIME ) return xvoCreateInt(0);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateInt(xrtMonth(xvoGetTime(t)));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_day(xvalue t) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(t) != XVO_DT_TIME ) return xvoCreateInt(0);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateInt(xrtDay(xvoGetTime(t)));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_hour(xvalue t) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(t) != XVO_DT_TIME ) return xvoCreateInt(0);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateInt(xrtHour(xvoGetTime(t)));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_minute(xvalue t) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(t) != XVO_DT_TIME ) return xvoCreateInt(0);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateInt(xrtMinute(xvoGetTime(t)));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_second(xvalue t) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(t) != XVO_DT_TIME ) return xvoCreateInt(0);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateInt(xrtSecond(xvoGetTime(t)));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_weekday(xvalue t) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(t) != XVO_DT_TIME ) return xvoCreateInt(0);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateInt(xrtWeekday(xvoGetTime(t)));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// strftime - 格式化时间
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_strftime(xvalue t, xvalue fmt) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(t) != XVO_DT_TIME ) return xvoCreateText((str)\"\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr sFmt = xvoGetText(fmt);\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr s = xrtTimeFormat(xvoGetTime(t), sFmt);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateText(s, (u32)strlen((char*)s), TRUE);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// timestamp - 获取Unix时间戳
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_timestamp(xvalue t) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(t) == XVO_DT_NULL ) return xvoCreateInt(xrtToUnixTime(xrtNow()));\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(t) != XVO_DT_TIME ) return xvoCreateInt(0);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateInt(xrtToUnixTime(xvoGetTime(t)));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// fromtimestamp - 从Unix时间戳创建时间
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_fromtimestamp(xvalue ts) {\n");
	xlCodeBufAppend(pGen->pHeader, "\ti64 iTs = xvoGetInt(ts);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateTime(xrtFromUnixTime(iTs));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// sleep - 毫秒延时
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_sleep(xvalue ms) {\n");
	xlCodeBufAppend(pGen->pHeader, "\ti64 iMs = xvoGetInt(ms);\n");
	xlCodeBufAppend(pGen->pHeader, "\txrtSleep((u32)iMs);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// timer - 高精度计时器
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_timer() {\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateFloat(xrtTimer());\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// ========== 文件内置函数 ==========

	// readfile - 读取文件全部内容
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_readfile(xvalue path) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(path) != XVO_DT_TEXT ) return xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr sPath = xvoGetText(path);\n");
	xlCodeBufAppend(pGen->pHeader, "\tsize_t iSize = 0;\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr sContent = xrtFileReadAll(sPath, XRT_CP_UTF8, &iSize);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( !sContent ) return xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateText(sContent, (u32)iSize, TRUE);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// writefile - 写入文件
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_writefile(xvalue path, xvalue content) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(path) != XVO_DT_TEXT ) return xvoCreateBool(FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr sPath = xvoGetText(path);\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr sContent = xvoGetText(content);\n");
	xlCodeBufAppend(pGen->pHeader, "\tu32 iSize = xvoGetSize(content);\n");
	xlCodeBufAppend(pGen->pHeader, "\tint ret = xrtFileWriteAll(sPath, sContent, iSize, XRT_CP_UTF8);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateBool(ret > 0);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// appendfile - 追加写入文件
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_appendfile(xvalue path, xvalue content) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(path) != XVO_DT_TEXT ) return xvoCreateBool(FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr sPath = xvoGetText(path);\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr sContent = xvoGetText(content);\n");
	xlCodeBufAppend(pGen->pHeader, "\tu32 iSize = xvoGetSize(content);\n");
	xlCodeBufAppend(pGen->pHeader, "\tint ret = xrtFileAppend(sPath, sContent, iSize, XRT_CP_UTF8);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateBool(ret > 0);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// exists - 路径是否存在
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_exists(xvalue path) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(path) != XVO_DT_TEXT ) return xvoCreateBool(FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateBool(xrtPathExists(xvoGetText(path)));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// isfile - 是否为文件
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_isfile(xvalue path) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(path) != XVO_DT_TEXT ) return xvoCreateBool(FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateBool(xrtFileExists(xvoGetText(path)));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// isdir - 是否为目录
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_isdir(xvalue path) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(path) != XVO_DT_TEXT ) return xvoCreateBool(FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateBool(xrtDirExists(xvoGetText(path)));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// filesize - 获取文件大小
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_filesize(xvalue path) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(path) != XVO_DT_TEXT ) return xvoCreateInt(0);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateInt(xrtFileGetSize(xvoGetText(path)));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// mkdir - 创建目录
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_mkdir(xvalue path) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(path) != XVO_DT_TEXT ) return xvoCreateBool(FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateBool(xrtDirCreateAll(xvoGetText(path)));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// remove - 删除文件
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_remove(xvalue path) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(path) != XVO_DT_TEXT ) return xvoCreateBool(FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateBool(xrtFileDelete(xvoGetText(path)));\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// ========== JSON 内置函数 ==========

	// json_parse - 解析JSON
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_json_parse(xvalue src) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(src) != XVO_DT_TEXT ) return xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr s = xvoGetText(src);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xrtParseJSON(s, 0);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// json_stringify - 序列化JSON
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_json_stringify(xvalue v) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tsize_t iSize = 0;\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr s = xrtStringifyJSON(v, TRUE, &iSize);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( !s ) return xvoCreateText((str)\"\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateText(s, (u32)iSize, TRUE);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// ========== 编码内置函数 ==========

	// base64_encode
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_base64_encode(xvalue src) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(src) != XVO_DT_TEXT ) return xvoCreateText((str)\"\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr s = xvoGetText(src);\n");
	xlCodeBufAppend(pGen->pHeader, "\tu32 iSize = xvoGetSize(src);\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr result = xrtBase64Encode((ptr)s, iSize, NULL);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( !result ) return xvoCreateText((str)\"\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateText(result, (u32)strlen((char*)result), TRUE);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// base64_decode
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_base64_decode(xvalue src) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(src) != XVO_DT_TEXT ) return xvoCreateText((str)\"\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr s = xvoGetText(src);\n");
	xlCodeBufAppend(pGen->pHeader, "\tsize_t iSize = xvoGetSize(src);\n");
	xlCodeBufAppend(pGen->pHeader, "\tptr result = xrtBase64Decode(s, iSize, NULL);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( !result ) return xvoCreateText((str)\"\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\tsize_t iRetSize = iSize * 3 / 4;\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateText((str)result, (u32)iRetSize, TRUE);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// hex_encode
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_hex_encode(xvalue src) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(src) != XVO_DT_TEXT ) return xvoCreateText((str)\"\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr s = xvoGetText(src);\n");
	xlCodeBufAppend(pGen->pHeader, "\tu32 iSize = xvoGetSize(src);\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr result = xrtHexEncode((ptr)s, iSize);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( !result ) return xvoCreateText((str)\"\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateText(result, iSize * 2, TRUE);\n");
	xlCodeBufAppend(pGen->pHeader, "}\n\n");

	// hex_decode
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_hex_decode(xvalue src) {\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( xvoType(src) != XVO_DT_TEXT ) return xvoCreateText((str)\"\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\tstr s = xvoGetText(src);\n");
	xlCodeBufAppend(pGen->pHeader, "\tsize_t iSize = xvoGetSize(src);\n");
	xlCodeBufAppend(pGen->pHeader, "\tptr result = xrtHexDecode(s, iSize);\n");
	xlCodeBufAppend(pGen->pHeader, "\tif ( !result ) return xvoCreateText((str)\"\", 0, FALSE);\n");
	xlCodeBufAppend(pGen->pHeader, "\treturn xvoCreateText((str)result, (u32)(iSize / 2), TRUE);\n");
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

	// 前向声明方法调用函数（在 body 中实现）
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_method_call(xvalue obj, const char* method, xvalue args);\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_super_call(xvalue obj, const char* method, xvalue args);\n");
	// meta 方法分发函数前向声明
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_meta_get(xvalue obj, const char* name);\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_meta_let(xvalue obj, const char* name, xvalue val);\n");
	xlCodeBufAppend(pGen->pHeader, "static xvalue xl_meta_call(xvalue obj, xvalue args);\n");
	xlCodeBufAppend(pGen->pHeader, "static int xl_has_meta_get(xvalue obj);\n");
	xlCodeBufAppend(pGen->pHeader, "static int xl_has_meta_let(xvalue obj);\n");
	xlCodeBufAppend(pGen->pHeader, "static int xl_has_meta_call(xvalue obj);\n\n");

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

	// 生成方法分发函数
	xlCodeBufAppend(pGen->pBody, "\n// 方法分发函数\n");
	xlCodeBufAppend(pGen->pBody, "static xvalue xl_method_call(xvalue obj, const char* method, xvalue args)\n{\n");
	// 检测内置类型
	xlCodeBufAppend(pGen->pBody, "\tint objType = xvoType(obj);\n");
	xlCodeBufAppend(pGen->pBody, "\t// list 类型方法\n");
	xlCodeBufAppend(pGen->pBody, "\tif ( objType == XVO_DT_LIST ) {\n");
	xlCodeBufAppend(pGen->pBody, "\t\treturn xl_list_method(obj, method, args);\n");
	xlCodeBufAppend(pGen->pBody, "\t}\n");
	xlCodeBufAppend(pGen->pBody, "\t// time 类型方法\n");
	xlCodeBufAppend(pGen->pBody, "\tif ( objType == XVO_DT_TIME ) {\n");
	xlCodeBufAppend(pGen->pBody, "\t\treturn xl_time_method(obj, method, args);\n");
	xlCodeBufAppend(pGen->pBody, "\t}\n");
	xlCodeBufAppend(pGen->pBody, "\t// set 类型方法（table 实现）\n");
	xlCodeBufAppend(pGen->pBody, "\tif ( objType == XVO_DT_TABLE ) {\n");
	xlCodeBufAppend(pGen->pBody, "\t\txvalue classNameVal = xvoTableGetValue(obj, (str)\"__class__\", 0);\n");
	xlCodeBufAppend(pGen->pBody, "\t\tif ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) {\n");
	xlCodeBufAppend(pGen->pBody, "\t\t\t// 可能是 set（没有 __class__ 的 table）\n");
	xlCodeBufAppend(pGen->pBody, "\t\t\treturn xl_set_method(obj, method, args);\n");
	xlCodeBufAppend(pGen->pBody, "\t\t}\n");
	xlCodeBufAppend(pGen->pBody, "\t\tstr className = xvoGetText(classNameVal);\n");
	xlCodeBufAppend(pGen->pBody, "\t\txvalue parentNameVal = xvoTableGetValue(obj, (str)\"__parent__\", 0);\n");
	xlCodeBufAppend(pGen->pBody, "\t\tstr parentName = parentNameVal ? xvoGetText(parentNameVal) : NULL;\n");
	xlCodeBufAppend(pGen->pBody, "\t\tu32 argc = xvoArrayItemCount(args);\n\n");

	// 为每个类生成方法分发（包括继承的方法）
	for ( int c = 0; c < pGen->iClassCount; c++ ) {
		xlCodeBufAppendFmt(pGen->pBody, "\t\tif ( strcmp((char*)className, \"%s\") == 0 ) {\n", pGen->arrClassNames[c]);
		// 子类自己的方法
		for ( int m = 0; m < pGen->arrMethodCounts[c]; m++ ) {
			xlCodeBufAppendFmt(pGen->pBody, "\t\t\tif ( strcmp(method, \"%s\") == 0 ) return xlc_%s_%s(obj);\n",
				pGen->arrClassMethods[c][m], pGen->arrClassNames[c], pGen->arrClassMethods[c][m]);
		}
		// 继承自父类的方法
		if ( pGen->arrClassParents[c][0] != '\0' ) {
			// 查找父类的方法
			for ( int p = 0; p < pGen->iClassCount; p++ ) {
				if ( strcmp(pGen->arrClassNames[p], pGen->arrClassParents[c]) == 0 ) {
					for ( int m = 0; m < pGen->arrMethodCounts[p]; m++ ) {
						// 只添加子类没有覆盖的方法
						bool bOverridden = false;
						for ( int cm = 0; cm < pGen->arrMethodCounts[c]; cm++ ) {
							if ( strcmp(pGen->arrClassMethods[c][cm], pGen->arrClassMethods[p][m]) == 0 ) {
								bOverridden = true;
								break;
							}
						}
						if ( !bOverridden ) {
							xlCodeBufAppendFmt(pGen->pBody, "\t\t\tif ( strcmp(method, \"%s\") == 0 ) return xlc_%s_%s(obj);\n",
								pGen->arrClassMethods[p][m], pGen->arrClassNames[p], pGen->arrClassMethods[p][m]);
						}
					}
					break;
				}
			}
		}
		xlCodeBufAppend(pGen->pBody, "\t\t}\n");
	}

	// 关闭 table 类型的 if 块
	xlCodeBufAppend(pGen->pBody, "\t}\n");

	xlCodeBufAppend(pGen->pBody, "\treturn xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pBody, "}\n");

	// 生成 super 调用函数
	xlCodeBufAppend(pGen->pBody, "\n// super 方法调用\n");
	xlCodeBufAppend(pGen->pBody, "static xvalue xl_super_call(xvalue obj, const char* method, xvalue args)\n{\n");
	xlCodeBufAppend(pGen->pBody, "\txvalue parentNameVal = xvoTableGetValue(obj, (str)\"__parent__\", 0);\n");
	xlCodeBufAppend(pGen->pBody, "\tif ( !parentNameVal ) return xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pBody, "\tstr parentName = xvoGetText(parentNameVal);\n");
	xlCodeBufAppend(pGen->pBody, "\tu32 argc = xvoArrayItemCount(args);\n\n");

	// 为每个有父类的类生成 super 调用分发
	for ( int c = 0; c < pGen->iClassCount; c++ ) {
		if ( pGen->arrClassParents[c][0] != '\0' ) {
			xlCodeBufAppendFmt(pGen->pBody, "\tif ( strcmp((char*)parentName, \"%s\") == 0 ) {\n", pGen->arrClassParents[c]);
			// 处理 create 方法（构造函数）
			xlCodeBufAppend(pGen->pBody, "\t\tif ( strcmp(method, \"create\") == 0 ) {\n");
			xlCodeBufAppend(pGen->pBody, "\t\t\t// 父类构造函数不创建新对象，只初始化字段\n");
			xlCodeBufAppend(pGen->pBody, "\t\t\treturn xvoCreateNull(); // 由用户在 create 中处理\n");
			xlCodeBufAppend(pGen->pBody, "\t\t}\n");
			// 处理普通方法
			// 查找父类的方法
			for ( int p = 0; p < pGen->iClassCount; p++ ) {
				if ( strcmp(pGen->arrClassNames[p], pGen->arrClassParents[c]) == 0 ) {
					for ( int m = 0; m < pGen->arrMethodCounts[p]; m++ ) {
						xlCodeBufAppendFmt(pGen->pBody, "\t\tif ( strcmp(method, \"%s\") == 0 ) return xlc_%s_%s(obj);\n",
							pGen->arrClassMethods[p][m], pGen->arrClassNames[p], pGen->arrClassMethods[p][m]);
					}
					break;
				}
			}
			xlCodeBufAppend(pGen->pBody, "\t}\n");
		}
	}

	xlCodeBufAppend(pGen->pBody, "\treturn xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pBody, "}\n");

	// 生成 meta get 分发函数
	xlCodeBufAppend(pGen->pBody, "\n// meta get 属性获取拦截\n");
	xlCodeBufAppend(pGen->pBody, "static xvalue xl_meta_get(xvalue obj, const char* name)\n{\n");
	xlCodeBufAppend(pGen->pBody, "\tif ( xvoType(obj) != XVO_DT_TABLE ) return xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pBody, "\txvalue classNameVal = xvoTableGetValue(obj, (str)\"__class__\", 0);\n");
	xlCodeBufAppend(pGen->pBody, "\tif ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pBody, "\tstr className = xvoGetText(classNameVal);\n");
	for ( int c = 0; c < pGen->iClassCount; c++ ) {
		if ( pGen->arrClassHasMetaGet[c] ) {
			xlCodeBufAppendFmt(pGen->pBody, "\tif ( strcmp((char*)className, \"%s\") == 0 ) return xlc_%s_meta_get(obj, xvoCreateText((str)name, 0, FALSE));\n",
				pGen->arrClassNames[c], pGen->arrClassNames[c]);
		}
	}
	xlCodeBufAppend(pGen->pBody, "\treturn xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pBody, "}\n");

	// 生成 meta let 分发函数
	xlCodeBufAppend(pGen->pBody, "\n// meta let 属性设置拦截\n");
	xlCodeBufAppend(pGen->pBody, "static xvalue xl_meta_let(xvalue obj, const char* name, xvalue val)\n{\n");
	xlCodeBufAppend(pGen->pBody, "\tif ( xvoType(obj) != XVO_DT_TABLE ) return xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pBody, "\txvalue classNameVal = xvoTableGetValue(obj, (str)\"__class__\", 0);\n");
	xlCodeBufAppend(pGen->pBody, "\tif ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pBody, "\tstr className = xvoGetText(classNameVal);\n");
	for ( int c = 0; c < pGen->iClassCount; c++ ) {
		if ( pGen->arrClassHasMetaLet[c] ) {
			xlCodeBufAppendFmt(pGen->pBody, "\tif ( strcmp((char*)className, \"%s\") == 0 ) return xlc_%s_meta_let(obj, xvoCreateText((str)name, 0, FALSE), val);\n",
				pGen->arrClassNames[c], pGen->arrClassNames[c]);
		}
	}
	xlCodeBufAppend(pGen->pBody, "\treturn xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pBody, "}\n");

	// 生成 meta call 分发函数
	xlCodeBufAppend(pGen->pBody, "\n// meta call 对象调用拦截\n");
	xlCodeBufAppend(pGen->pBody, "static xvalue xl_meta_call(xvalue obj, xvalue args)\n{\n");
	xlCodeBufAppend(pGen->pBody, "\tif ( xvoType(obj) != XVO_DT_TABLE ) return xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pBody, "\txvalue classNameVal = xvoTableGetValue(obj, (str)\"__class__\", 0);\n");
	xlCodeBufAppend(pGen->pBody, "\tif ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pBody, "\tstr className = xvoGetText(classNameVal);\n");
	for ( int c = 0; c < pGen->iClassCount; c++ ) {
		if ( pGen->arrClassHasMetaCall[c] ) {
			xlCodeBufAppendFmt(pGen->pBody, "\tif ( strcmp((char*)className, \"%s\") == 0 ) return xlc_%s_meta_call(obj, args);\n",
				pGen->arrClassNames[c], pGen->arrClassNames[c]);
		}
	}
	xlCodeBufAppend(pGen->pBody, "\treturn xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pBody, "}\n");

	// 生成检查类是否有 meta 方法的辅助函数
	xlCodeBufAppend(pGen->pBody, "\n// 检查对象是否有 meta get\n");
	xlCodeBufAppend(pGen->pBody, "static int xl_has_meta_get(xvalue obj)\n{\n");
	xlCodeBufAppend(pGen->pBody, "\tif ( xvoType(obj) != XVO_DT_TABLE ) return 0;\n");
	xlCodeBufAppend(pGen->pBody, "\txvalue classNameVal = xvoTableGetValue(obj, (str)\"__class__\", 0);\n");
	xlCodeBufAppend(pGen->pBody, "\tif ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return 0;\n");
	xlCodeBufAppend(pGen->pBody, "\tstr className = xvoGetText(classNameVal);\n");
	for ( int c = 0; c < pGen->iClassCount; c++ ) {
		if ( pGen->arrClassHasMetaGet[c] ) {
			xlCodeBufAppendFmt(pGen->pBody, "\tif ( strcmp((char*)className, \"%s\") == 0 ) return 1;\n", pGen->arrClassNames[c]);
		}
	}
	xlCodeBufAppend(pGen->pBody, "\treturn 0;\n");
	xlCodeBufAppend(pGen->pBody, "}\n");

	xlCodeBufAppend(pGen->pBody, "\n// 检查对象是否有 meta let\n");
	xlCodeBufAppend(pGen->pBody, "static int xl_has_meta_let(xvalue obj)\n{\n");
	xlCodeBufAppend(pGen->pBody, "\tif ( xvoType(obj) != XVO_DT_TABLE ) return 0;\n");
	xlCodeBufAppend(pGen->pBody, "\txvalue classNameVal = xvoTableGetValue(obj, (str)\"__class__\", 0);\n");
	xlCodeBufAppend(pGen->pBody, "\tif ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return 0;\n");
	xlCodeBufAppend(pGen->pBody, "\tstr className = xvoGetText(classNameVal);\n");
	for ( int c = 0; c < pGen->iClassCount; c++ ) {
		if ( pGen->arrClassHasMetaLet[c] ) {
			xlCodeBufAppendFmt(pGen->pBody, "\tif ( strcmp((char*)className, \"%s\") == 0 ) return 1;\n", pGen->arrClassNames[c]);
		}
	}
	xlCodeBufAppend(pGen->pBody, "\treturn 0;\n");
	xlCodeBufAppend(pGen->pBody, "}\n");

	xlCodeBufAppend(pGen->pBody, "\n// 检查对象是否有 meta call\n");
	xlCodeBufAppend(pGen->pBody, "static int xl_has_meta_call(xvalue obj)\n{\n");
	xlCodeBufAppend(pGen->pBody, "\tif ( xvoType(obj) != XVO_DT_TABLE ) return 0;\n");
	xlCodeBufAppend(pGen->pBody, "\txvalue classNameVal = xvoTableGetValue(obj, (str)\"__class__\", 0);\n");
	xlCodeBufAppend(pGen->pBody, "\tif ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return 0;\n");
	xlCodeBufAppend(pGen->pBody, "\tstr className = xvoGetText(classNameVal);\n");
	for ( int c = 0; c < pGen->iClassCount; c++ ) {
		if ( pGen->arrClassHasMetaCall[c] ) {
			xlCodeBufAppendFmt(pGen->pBody, "\tif ( strcmp((char*)className, \"%s\") == 0 ) return 1;\n", pGen->arrClassNames[c]);
		}
	}
	xlCodeBufAppend(pGen->pBody, "\treturn 0;\n");
	xlCodeBufAppend(pGen->pBody, "}\n");

	// 生成 xl_has_meta_op - 检查对象是否有特定运算符重载
	xlCodeBufAppend(pGen->pBody, "\n// 检查对象是否有特定运算符重载\n");
	xlCodeBufAppend(pGen->pBody, "static int xl_has_meta_op(xvalue obj, const char* op)\n{\n");
	xlCodeBufAppend(pGen->pBody, "\tif ( xvoType(obj) != XVO_DT_TABLE ) return 0;\n");
	xlCodeBufAppend(pGen->pBody, "\txvalue classNameVal = xvoTableGetValue(obj, (str)\"__class__\", 0);\n");
	xlCodeBufAppend(pGen->pBody, "\tif ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return 0;\n");
	xlCodeBufAppend(pGen->pBody, "\tstr className = xvoGetText(classNameVal);\n");
	for ( int c = 0; c < pGen->iClassCount; c++ ) {
		xlCodeBufAppendFmt(pGen->pBody, "\tif ( strcmp((char*)className, \"%s\") == 0 ) {\n", pGen->arrClassNames[c]);
		if ( pGen->arrClassHasMetaAdd[c] ) xlCodeBufAppend(pGen->pBody, "\t\tif ( strcmp(op, \"add\") == 0 ) return 1;\n");
		if ( pGen->arrClassHasMetaSub[c] ) xlCodeBufAppend(pGen->pBody, "\t\tif ( strcmp(op, \"sub\") == 0 ) return 1;\n");
		if ( pGen->arrClassHasMetaMul[c] ) xlCodeBufAppend(pGen->pBody, "\t\tif ( strcmp(op, \"mul\") == 0 ) return 1;\n");
		if ( pGen->arrClassHasMetaDiv[c] ) xlCodeBufAppend(pGen->pBody, "\t\tif ( strcmp(op, \"div\") == 0 ) return 1;\n");
		if ( pGen->arrClassHasMetaMod[c] ) xlCodeBufAppend(pGen->pBody, "\t\tif ( strcmp(op, \"mod\") == 0 ) return 1;\n");
		if ( pGen->arrClassHasMetaPow[c] ) xlCodeBufAppend(pGen->pBody, "\t\tif ( strcmp(op, \"pow\") == 0 ) return 1;\n");
		if ( pGen->arrClassHasMetaNeg[c] ) xlCodeBufAppend(pGen->pBody, "\t\tif ( strcmp(op, \"neg\") == 0 ) return 1;\n");
		if ( pGen->arrClassHasMetaEq[c] ) xlCodeBufAppend(pGen->pBody, "\t\tif ( strcmp(op, \"eq\") == 0 ) return 1;\n");
		if ( pGen->arrClassHasMetaLt[c] ) xlCodeBufAppend(pGen->pBody, "\t\tif ( strcmp(op, \"lt\") == 0 ) return 1;\n");
		if ( pGen->arrClassHasMetaLe[c] ) xlCodeBufAppend(pGen->pBody, "\t\tif ( strcmp(op, \"le\") == 0 ) return 1;\n");
		if ( pGen->arrClassHasMetaIndex[c] ) xlCodeBufAppend(pGen->pBody, "\t\tif ( strcmp(op, \"index\") == 0 ) return 1;\n");
		if ( pGen->arrClassHasMetaNewindex[c] ) xlCodeBufAppend(pGen->pBody, "\t\tif ( strcmp(op, \"newindex\") == 0 ) return 1;\n");
		if ( pGen->arrClassHasMetaLen[c] ) xlCodeBufAppend(pGen->pBody, "\t\tif ( strcmp(op, \"len\") == 0 ) return 1;\n");
		if ( pGen->arrClassHasMetaIter[c] ) xlCodeBufAppend(pGen->pBody, "\t\tif ( strcmp(op, \"iter\") == 0 ) return 1;\n");
		if ( pGen->arrClassHasMetaNext[c] ) xlCodeBufAppend(pGen->pBody, "\t\tif ( strcmp(op, \"next\") == 0 ) return 1;\n");
		if ( pGen->arrClassHasMetaTo[c] ) xlCodeBufAppend(pGen->pBody, "\t\tif ( strcmp(op, \"to\") == 0 ) return 1;\n");
		if ( pGen->arrClassHasMetaEnter[c] ) xlCodeBufAppend(pGen->pBody, "\t\tif ( strcmp(op, \"enter\") == 0 ) return 1;\n");
		if ( pGen->arrClassHasMetaExit[c] ) xlCodeBufAppend(pGen->pBody, "\t\tif ( strcmp(op, \"exit\") == 0 ) return 1;\n");
		xlCodeBufAppend(pGen->pBody, "\t}\n");
	}
	xlCodeBufAppend(pGen->pBody, "\treturn 0;\n");
	xlCodeBufAppend(pGen->pBody, "}\n");

	// 生成运算符重载分发函数
	#define XLGEN_OP_DISPATCH_BINARY(opName, metaFlag) \
		xlCodeBufAppendFmt(pGen->pBody, "\n// 运算符重载: %s\n", opName); \
		xlCodeBufAppendFmt(pGen->pBody, "static xvalue xl_op_%s(xvalue a, xvalue b)\n{\n", opName); \
		xlCodeBufAppend(pGen->pBody, "\tif ( xvoType(a) != XVO_DT_TABLE ) return xvoCreateNull();\n"); \
		xlCodeBufAppend(pGen->pBody, "\txvalue classNameVal = xvoTableGetValue(a, (str)\"__class__\", 0);\n"); \
		xlCodeBufAppend(pGen->pBody, "\tif ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return xvoCreateNull();\n"); \
		xlCodeBufAppend(pGen->pBody, "\tstr className = xvoGetText(classNameVal);\n"); \
		for ( int c = 0; c < pGen->iClassCount; c++ ) { \
			if ( pGen->metaFlag[c] ) { \
				xlCodeBufAppendFmt(pGen->pBody, "\tif ( strcmp((char*)className, \"%s\") == 0 ) return xlc_%s_meta_%s(a, b);\n", \
					pGen->arrClassNames[c], pGen->arrClassNames[c], opName); \
			} \
		} \
		xlCodeBufAppend(pGen->pBody, "\treturn xvoCreateNull();\n"); \
		xlCodeBufAppend(pGen->pBody, "}\n");

	#define XLGEN_OP_DISPATCH_UNARY(opName, metaFlag) \
		xlCodeBufAppendFmt(pGen->pBody, "\n// 运算符重载: %s\n", opName); \
		xlCodeBufAppendFmt(pGen->pBody, "static xvalue xl_op_%s(xvalue a)\n{\n", opName); \
		xlCodeBufAppend(pGen->pBody, "\tif ( xvoType(a) != XVO_DT_TABLE ) return xvoCreateNull();\n"); \
		xlCodeBufAppend(pGen->pBody, "\txvalue classNameVal = xvoTableGetValue(a, (str)\"__class__\", 0);\n"); \
		xlCodeBufAppend(pGen->pBody, "\tif ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return xvoCreateNull();\n"); \
		xlCodeBufAppend(pGen->pBody, "\tstr className = xvoGetText(classNameVal);\n"); \
		for ( int c = 0; c < pGen->iClassCount; c++ ) { \
			if ( pGen->metaFlag[c] ) { \
				xlCodeBufAppendFmt(pGen->pBody, "\tif ( strcmp((char*)className, \"%s\") == 0 ) return xlc_%s_meta_%s(a);\n", \
					pGen->arrClassNames[c], pGen->arrClassNames[c], opName); \
			} \
		} \
		xlCodeBufAppend(pGen->pBody, "\treturn xvoCreateNull();\n"); \
		xlCodeBufAppend(pGen->pBody, "}\n");

	XLGEN_OP_DISPATCH_BINARY("add", arrClassHasMetaAdd)
	XLGEN_OP_DISPATCH_BINARY("sub", arrClassHasMetaSub)
	XLGEN_OP_DISPATCH_BINARY("mul", arrClassHasMetaMul)
	XLGEN_OP_DISPATCH_BINARY("div", arrClassHasMetaDiv)
	XLGEN_OP_DISPATCH_BINARY("mod", arrClassHasMetaMod)
	XLGEN_OP_DISPATCH_BINARY("pow", arrClassHasMetaPow)
	XLGEN_OP_DISPATCH_UNARY("neg", arrClassHasMetaNeg)
	XLGEN_OP_DISPATCH_BINARY("eq", arrClassHasMetaEq)
	XLGEN_OP_DISPATCH_BINARY("lt", arrClassHasMetaLt)
	XLGEN_OP_DISPATCH_BINARY("le", arrClassHasMetaLe)
	XLGEN_OP_DISPATCH_UNARY("len", arrClassHasMetaLen)

	// meta index(key)
	xlCodeBufAppend(pGen->pBody, "\n// 运算符重载: index\n");
	xlCodeBufAppend(pGen->pBody, "static xvalue xl_op_index(xvalue obj, xvalue key)\n{\n");
	xlCodeBufAppend(pGen->pBody, "\tif ( xvoType(obj) != XVO_DT_TABLE ) return xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pBody, "\txvalue classNameVal = xvoTableGetValue(obj, (str)\"__class__\", 0);\n");
	xlCodeBufAppend(pGen->pBody, "\tif ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pBody, "\tstr className = xvoGetText(classNameVal);\n");
	for ( int c = 0; c < pGen->iClassCount; c++ ) {
		if ( pGen->arrClassHasMetaIndex[c] ) {
			xlCodeBufAppendFmt(pGen->pBody, "\tif ( strcmp((char*)className, \"%s\") == 0 ) return xlc_%s_meta_index(obj, key);\n",
				pGen->arrClassNames[c], pGen->arrClassNames[c]);
		}
	}
	xlCodeBufAppend(pGen->pBody, "\treturn xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pBody, "}\n");

	// meta newindex(key, val)
	xlCodeBufAppend(pGen->pBody, "\n// 运算符重载: newindex\n");
	xlCodeBufAppend(pGen->pBody, "static xvalue xl_op_newindex(xvalue obj, xvalue key, xvalue val)\n{\n");
	xlCodeBufAppend(pGen->pBody, "\tif ( xvoType(obj) != XVO_DT_TABLE ) return xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pBody, "\txvalue classNameVal = xvoTableGetValue(obj, (str)\"__class__\", 0);\n");
	xlCodeBufAppend(pGen->pBody, "\tif ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pBody, "\tstr className = xvoGetText(classNameVal);\n");
	for ( int c = 0; c < pGen->iClassCount; c++ ) {
		if ( pGen->arrClassHasMetaNewindex[c] ) {
			xlCodeBufAppendFmt(pGen->pBody, "\tif ( strcmp((char*)className, \"%s\") == 0 ) return xlc_%s_meta_newindex(obj, key, val);\n",
				pGen->arrClassNames[c], pGen->arrClassNames[c]);
		}
	}
	xlCodeBufAppend(pGen->pBody, "\treturn xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pBody, "}\n");

	// meta iter() - 迭代器初始化
	xlCodeBufAppend(pGen->pBody, "\n// 元方法: iter\n");
	xlCodeBufAppend(pGen->pBody, "static xvalue xl_op_iter(xvalue obj)\n{\n");
	xlCodeBufAppend(pGen->pBody, "\tif ( xvoType(obj) != XVO_DT_TABLE ) return xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pBody, "\txvalue classNameVal = xvoTableGetValue(obj, (str)\"__class__\", 0);\n");
	xlCodeBufAppend(pGen->pBody, "\tif ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pBody, "\tstr className = xvoGetText(classNameVal);\n");
	for ( int c = 0; c < pGen->iClassCount; c++ ) {
		if ( pGen->arrClassHasMetaIter[c] ) {
			xlCodeBufAppendFmt(pGen->pBody, "\tif ( strcmp((char*)className, \"%s\") == 0 ) return xlc_%s_meta_iter(obj);\n",
				pGen->arrClassNames[c], pGen->arrClassNames[c]);
		}
	}
	xlCodeBufAppend(pGen->pBody, "\treturn xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pBody, "}\n");

	// meta next() - 迭代器获取下一个值
	xlCodeBufAppend(pGen->pBody, "\n// 元方法: next\n");
	xlCodeBufAppend(pGen->pBody, "static xvalue xl_op_next(xvalue obj)\n{\n");
	xlCodeBufAppend(pGen->pBody, "\tif ( xvoType(obj) != XVO_DT_TABLE ) return xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pBody, "\txvalue classNameVal = xvoTableGetValue(obj, (str)\"__class__\", 0);\n");
	xlCodeBufAppend(pGen->pBody, "\tif ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pBody, "\tstr className = xvoGetText(classNameVal);\n");
	for ( int c = 0; c < pGen->iClassCount; c++ ) {
		if ( pGen->arrClassHasMetaNext[c] ) {
			xlCodeBufAppendFmt(pGen->pBody, "\tif ( strcmp((char*)className, \"%s\") == 0 ) return xlc_%s_meta_next(obj);\n",
				pGen->arrClassNames[c], pGen->arrClassNames[c]);
		}
	}
	xlCodeBufAppend(pGen->pBody, "\treturn xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pBody, "}\n");

	// meta to(type) - 类型转换
	xlCodeBufAppend(pGen->pBody, "\n// 元方法: to (类型转换)\n");
	xlCodeBufAppend(pGen->pBody, "static xvalue xl_op_to(xvalue obj, xvalue type)\n{\n");
	xlCodeBufAppend(pGen->pBody, "\tif ( xvoType(obj) != XVO_DT_TABLE ) return xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pBody, "\txvalue classNameVal = xvoTableGetValue(obj, (str)\"__class__\", 0);\n");
	xlCodeBufAppend(pGen->pBody, "\tif ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pBody, "\tstr className = xvoGetText(classNameVal);\n");
	for ( int c = 0; c < pGen->iClassCount; c++ ) {
		if ( pGen->arrClassHasMetaTo[c] ) {
			xlCodeBufAppendFmt(pGen->pBody, "\tif ( strcmp((char*)className, \"%s\") == 0 ) return xlc_%s_meta_to(obj, type);\n",
				pGen->arrClassNames[c], pGen->arrClassNames[c]);
		}
	}
	xlCodeBufAppend(pGen->pBody, "\treturn xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pBody, "}\n");

	// meta enter() - with 语句进入
	xlCodeBufAppend(pGen->pBody, "\n// 元方法: enter (with 语句)\n");
	xlCodeBufAppend(pGen->pBody, "static xvalue xl_op_enter(xvalue obj)\n{\n");
	xlCodeBufAppend(pGen->pBody, "\tif ( xvoType(obj) != XVO_DT_TABLE ) return obj;\n");
	xlCodeBufAppend(pGen->pBody, "\txvalue classNameVal = xvoTableGetValue(obj, (str)\"__class__\", 0);\n");
	xlCodeBufAppend(pGen->pBody, "\tif ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return obj;\n");
	xlCodeBufAppend(pGen->pBody, "\tstr className = xvoGetText(classNameVal);\n");
	for ( int c = 0; c < pGen->iClassCount; c++ ) {
		if ( pGen->arrClassHasMetaEnter[c] ) {
			xlCodeBufAppendFmt(pGen->pBody, "\tif ( strcmp((char*)className, \"%s\") == 0 ) return xlc_%s_meta_enter(obj);\n",
				pGen->arrClassNames[c], pGen->arrClassNames[c]);
		}
	}
	xlCodeBufAppend(pGen->pBody, "\treturn obj;\n");
	xlCodeBufAppend(pGen->pBody, "}\n");

	// meta exit() - with 语句退出
	xlCodeBufAppend(pGen->pBody, "\n// 元方法: exit (with 语句)\n");
	xlCodeBufAppend(pGen->pBody, "static xvalue xl_op_exit(xvalue obj)\n{\n");
	xlCodeBufAppend(pGen->pBody, "\tif ( xvoType(obj) != XVO_DT_TABLE ) return xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pBody, "\txvalue classNameVal = xvoTableGetValue(obj, (str)\"__class__\", 0);\n");
	xlCodeBufAppend(pGen->pBody, "\tif ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pBody, "\tstr className = xvoGetText(classNameVal);\n");
	for ( int c = 0; c < pGen->iClassCount; c++ ) {
		if ( pGen->arrClassHasMetaExit[c] ) {
			xlCodeBufAppendFmt(pGen->pBody, "\tif ( strcmp((char*)className, \"%s\") == 0 ) return xlc_%s_meta_exit(obj);\n",
				pGen->arrClassNames[c], pGen->arrClassNames[c]);
		}
	}
	xlCodeBufAppend(pGen->pBody, "\treturn xvoCreateNull();\n");
	xlCodeBufAppend(pGen->pBody, "}\n");

	#undef XLGEN_OP_DISPATCH_BINARY
	#undef XLGEN_OP_DISPATCH_UNARY

	// 生成 main 函数
	xlCodeBufAppend(pGen->pBody, "\n// 程序入口\n");
	xlCodeBufAppend(pGen->pBody, "int main(int argc, char* argv[])\n{\n");
	xlCodeBufAppend(pGen->pBody, "\txrtInit();\n\n");

	// 调用所有类的静态初始化函数
	xlCodeBufAppend(pGen->pBody, "\t// 初始化类静态成员\n");
	for ( int c = 0; c < pGen->iClassCount; c++ ) {
		if ( pGen->arrClassHasStatic[c] ) {
			xlCodeBufAppendFmt(pGen->pBody, "\txlc_%s_static_init();\n", pGen->arrClassNames[c]);
		}
	}
	xlCodeBufAppend(pGen->pBody, "\n");

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
