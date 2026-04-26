/**
 * xlang v4 - AST 模块
 * 
 * 使用 xvalue (xrt table) 表示 AST 节点
 * 参考第三版 demo 的做法，所有节点以 xvalue Table 存储
 * 
 * 仅支持 v4 一阶段语法：
 * - 变量/常量声明
 * - 函数声明
 * - 控制流（if/for/while）
 * - 表达式（算术/比较/逻辑/调用/索引/成员访问）
 * - 字面量（整数/浮点/字符串/布尔/null/数组/字典/时间）
 */

#ifndef XL_AST_H
#define XL_AST_H



// ============================================================================
// AST 节点类型常量
// ============================================================================

// 顶层
#define XL_AST_MODULE           "Module"

// 声明
#define XL_AST_VAR_DECL         "VarDecl"
#define XL_AST_CONST_DECL       "ConstDecl"
#define XL_AST_FUNC_DECL        "FuncDecl"
#define XL_AST_PARAM_DECL       "ParamDecl"

// 语句
#define XL_AST_BLOCK_STMT       "BlockStmt"
#define XL_AST_EXPR_STMT        "ExprStmt"
#define XL_AST_IF_STMT          "IfStmt"
#define XL_AST_FOR_STMT         "ForStmt"
#define XL_AST_FOR_IN_STMT      "ForInStmt"
#define XL_AST_WHILE_STMT       "WhileStmt"
#define XL_AST_DO_WHILE_STMT    "DoWhileStmt"
#define XL_AST_BREAK_STMT       "BreakStmt"
#define XL_AST_CONTINUE_STMT    "ContinueStmt"
#define XL_AST_RETURN_STMT      "ReturnStmt"

// 表达式
#define XL_AST_BINARY_EXPR      "BinaryExpr"
#define XL_AST_UNARY_EXPR       "UnaryExpr"
#define XL_AST_CALL_EXPR        "CallExpr"
#define XL_AST_INDEX_EXPR       "IndexExpr"
#define XL_AST_MEMBER_EXPR      "MemberExpr"
#define XL_AST_ASSIGN_EXPR      "AssignExpr"
#define XL_AST_TERNARY_EXPR     "TernaryExpr"

// 字面量
#define XL_AST_IDENT            "Ident"
#define XL_AST_INT_LIT          "IntLit"
#define XL_AST_FLOAT_LIT        "FloatLit"
#define XL_AST_STRING_LIT       "StringLit"
#define XL_AST_BOOL_LIT         "BoolLit"
#define XL_AST_NULL_LIT         "NullLit"
#define XL_AST_TIME_LIT         "TimeLit"
#define XL_AST_ARRAY_LIT        "ArrayLit"
#define XL_AST_DICT_LIT         "DictLit"

// 类型引用
#define XL_AST_TYPE_REF         "TypeRef"



// ============================================================================
// AST 基础操作
// ============================================================================

// 创建基本节点 { kind, line, col }
static inline xvalue xlAstCreate( const char* pszKind, int iLine, int iCol )
{
	xvalue xNode = xvoCreateTable();
	xvoTableSetText(xNode, (str)"kind", 0, (str)pszKind, 0, FALSE);
	xvoTableSetInt(xNode, (str)"line", 0, iLine);
	xvoTableSetInt(xNode, (str)"col", 0, iCol);
	return xNode;
}


// 设置字符串字段（带长度，复制）
static inline void xlAstSetStr( xvalue xNode, const char* pszKey, const char* pszVal, int iLen )
{
	if ( pszVal ) {
		if ( iLen <= 0 ) {
			iLen = (int)strlen(pszVal);
		}
		/* 先复制为 NUL 结尾的临时串 */
		char* pszCopy = (char*)malloc(iLen + 1);
		memcpy(pszCopy, pszVal, iLen);
		pszCopy[iLen] = '\0';
		xvoTableSetText(xNode, (str)pszKey, 0, (str)pszCopy, iLen, TRUE);
	}
}


// 设置整数字段
static inline void xlAstSetInt( xvalue xNode, const char* pszKey, int64_t iVal )
{
	xvoTableSetInt(xNode, (str)pszKey, 0, iVal);
}


// 设置浮点字段
static inline void xlAstSetFloat( xvalue xNode, const char* pszKey, double fVal )
{
	xvoTableSetFloat(xNode, (str)pszKey, 0, fVal);
}


// 设置布尔字段
static inline void xlAstSetBool( xvalue xNode, const char* pszKey, int bVal )
{
	xvoTableSetBool(xNode, (str)pszKey, 0, bVal);
}


// 设置子节点字段
static inline void xlAstSetNode( xvalue xNode, const char* pszKey, xvalue xChild )
{
	if ( xChild ) {
		xvoTableSetValue(xNode, (str)pszKey, 0, xChild, FALSE);
	}
}


// 获取字符串字段
static inline const char* xlAstGetStr( xvalue xNode, const char* pszKey )
{
	xvalue xVal = xvoTableGetValue(xNode, (str)pszKey, 0);
	if ( xvoType(xVal) == XVO_DT_TEXT ) {
		return (const char*)xvoGetText(xVal);
	}
	return NULL;
}


// 获取整数字段
static inline int64_t xlAstGetInt( xvalue xNode, const char* pszKey )
{
	xvalue xVal = xvoTableGetValue(xNode, (str)pszKey, 0);
	return xvoGetInt(xVal);
}


// 获取子节点
static inline xvalue xlAstGetNode( xvalue xNode, const char* pszKey )
{
	return xvoTableGetValue(xNode, (str)pszKey, 0);
}


// 获取节点类型
static inline const char* xlAstGetKind( xvalue xNode )
{
	return xlAstGetStr(xNode, "kind");
}



// ============================================================================
// AST 数组操作
// ============================================================================

static inline xvalue xlAstCreateArray( void )
{
	return xvoCreateArray();
}

static inline void xlAstArrayAppend( xvalue xArr, xvalue xNode )
{
	if ( xArr && xNode ) {
		xvoArrayAppendValue(xArr, xNode, FALSE);
	}
}

static inline int xlAstArrayLen( xvalue xArr )
{
	if ( !xArr ) return 0;
	return xvoArrayItemCount(xArr);
}

static inline xvalue xlAstArrayGet( xvalue xArr, int iIdx )
{
	if ( !xArr ) return 0;
	return xvoArrayGetValue(xArr, iIdx);
}



// ============================================================================
// AST 节点创建 - 顶层
// ============================================================================

// Module: { kind, file, body[] }
static inline xvalue xlAstCreateModule( const char* pszFile )
{
	xvalue xNode = xlAstCreate(XL_AST_MODULE, 1, 1);
	xlAstSetStr(xNode, "file", pszFile, 0);
	xlAstSetNode(xNode, "body", xlAstCreateArray());
	return xNode;
}

static inline void xlAstModuleAddStmt( xvalue xModule, xvalue xStmt )
{
	xvalue xBody = xlAstGetNode(xModule, "body");
	xlAstArrayAppend(xBody, xStmt);
}



// ============================================================================
// AST 节点创建 - 声明
// ============================================================================

// VarDecl: { kind, line, col, name, varType?, init?, isConst }
static inline xvalue xlAstCreateVarDecl( const char* pszName, int iNameLen,
                                         const char* pszType, int iTypeLen,
                                         xvalue xInit, int bIsConst,
                                         int iLine, int iCol )
{
	xvalue xNode = xlAstCreate(XL_AST_VAR_DECL, iLine, iCol);
	xlAstSetStr(xNode, "name", pszName, iNameLen);
	if ( pszType && (iTypeLen > 0) ) {
		xlAstSetStr(xNode, "varType", pszType, iTypeLen);
	}
	xlAstSetNode(xNode, "init", xInit);
	xlAstSetBool(xNode, "isConst", bIsConst);
	return xNode;
}


// ParamDecl: { kind, line, col, name, paramType?, default? }
static inline xvalue xlAstCreateParamDecl( const char* pszName, int iNameLen,
                                           const char* pszType, int iTypeLen,
                                           xvalue xDefault,
                                           int iLine, int iCol )
{
	xvalue xNode = xlAstCreate(XL_AST_PARAM_DECL, iLine, iCol);
	xlAstSetStr(xNode, "name", pszName, iNameLen);
	if ( pszType && (iTypeLen > 0) ) {
		xlAstSetStr(xNode, "paramType", pszType, iTypeLen);
	}
	xlAstSetNode(xNode, "default", xDefault);
	return xNode;
}


// FuncDecl: { kind, line, col, name, params[], returnType?, body }
static inline xvalue xlAstCreateFuncDecl( const char* pszName, int iNameLen,
                                          xvalue xParams,
                                          const char* pszRetType, int iRetTypeLen,
                                          xvalue xBody,
                                          int iLine, int iCol )
{
	xvalue xNode = xlAstCreate(XL_AST_FUNC_DECL, iLine, iCol);
	xlAstSetStr(xNode, "name", pszName, iNameLen);
	xlAstSetNode(xNode, "params", xParams ? xParams : xlAstCreateArray());
	if ( pszRetType && (iRetTypeLen > 0) ) {
		xlAstSetStr(xNode, "returnType", pszRetType, iRetTypeLen);
	}
	xlAstSetNode(xNode, "body", xBody);
	return xNode;
}



// ============================================================================
// AST 节点创建 - 语句
// ============================================================================

// BlockStmt: { kind, line, col, stmts[] }
static inline xvalue xlAstCreateBlockStmt( int iLine, int iCol )
{
	xvalue xNode = xlAstCreate(XL_AST_BLOCK_STMT, iLine, iCol);
	xlAstSetNode(xNode, "stmts", xlAstCreateArray());
	return xNode;
}

static inline void xlAstBlockAddStmt( xvalue xBlock, xvalue xStmt )
{
	xvalue xStmts = xlAstGetNode(xBlock, "stmts");
	xlAstArrayAppend(xStmts, xStmt);
}


// ExprStmt: { kind, line, col, expr }
static inline xvalue xlAstCreateExprStmt( xvalue xExpr, int iLine, int iCol )
{
	xvalue xNode = xlAstCreate(XL_AST_EXPR_STMT, iLine, iCol);
	xlAstSetNode(xNode, "expr", xExpr);
	return xNode;
}


// IfStmt: { kind, line, col, cond, then, elseifs[], else? }
static inline xvalue xlAstCreateIfStmt( xvalue xCond, xvalue xThen,
                                        xvalue xElseIfs, xvalue xElse,
                                        int iLine, int iCol )
{
	xvalue xNode = xlAstCreate(XL_AST_IF_STMT, iLine, iCol);
	xlAstSetNode(xNode, "cond", xCond);
	xlAstSetNode(xNode, "then", xThen);
	xlAstSetNode(xNode, "elseifs", xElseIfs ? xElseIfs : xlAstCreateArray());
	xlAstSetNode(xNode, "else", xElse);
	return xNode;
}


// ForStmt: { kind, line, col, var, start, end, step?, body }
static inline xvalue xlAstCreateForStmt( const char* pszVar, int iVarLen,
                                         xvalue xStart, xvalue xEnd, xvalue xStep,
                                         xvalue xBody, int iLine, int iCol )
{
	xvalue xNode = xlAstCreate(XL_AST_FOR_STMT, iLine, iCol);
	xlAstSetStr(xNode, "var", pszVar, iVarLen);
	xlAstSetNode(xNode, "start", xStart);
	xlAstSetNode(xNode, "end", xEnd);
	xlAstSetNode(xNode, "step", xStep);
	xlAstSetNode(xNode, "body", xBody);
	return xNode;
}


// ForInStmt: { kind, line, col, key?, value, iter, body }
static inline xvalue xlAstCreateForInStmt( const char* pszKey, int iKeyLen,
                                           const char* pszValue, int iValueLen,
                                           xvalue xIter, xvalue xBody,
                                           int iLine, int iCol )
{
	xvalue xNode = xlAstCreate(XL_AST_FOR_IN_STMT, iLine, iCol);
	if ( pszKey && (iKeyLen > 0) ) {
		xlAstSetStr(xNode, "key", pszKey, iKeyLen);
	}
	xlAstSetStr(xNode, "value", pszValue, iValueLen);
	xlAstSetNode(xNode, "iter", xIter);
	xlAstSetNode(xNode, "body", xBody);
	return xNode;
}


// WhileStmt: { kind, line, col, cond, body }
static inline xvalue xlAstCreateWhileStmt( xvalue xCond, xvalue xBody,
                                           int iLine, int iCol )
{
	xvalue xNode = xlAstCreate(XL_AST_WHILE_STMT, iLine, iCol);
	xlAstSetNode(xNode, "cond", xCond);
	xlAstSetNode(xNode, "body", xBody);
	return xNode;
}


// DoWhileStmt: { kind, line, col, body, cond }
static inline xvalue xlAstCreateDoWhileStmt( xvalue xBody, xvalue xCond,
                                             int iLine, int iCol )
{
	xvalue xNode = xlAstCreate(XL_AST_DO_WHILE_STMT, iLine, iCol);
	xlAstSetNode(xNode, "body", xBody);
	xlAstSetNode(xNode, "cond", xCond);
	return xNode;
}


// BreakStmt: { kind, line, col, level }
static inline xvalue xlAstCreateBreakStmt( int iLevel, int iLine, int iCol )
{
	xvalue xNode = xlAstCreate(XL_AST_BREAK_STMT, iLine, iCol);
	xlAstSetInt(xNode, "level", iLevel);
	return xNode;
}


// ContinueStmt: { kind, line, col, level }
static inline xvalue xlAstCreateContinueStmt( int iLevel, int iLine, int iCol )
{
	xvalue xNode = xlAstCreate(XL_AST_CONTINUE_STMT, iLine, iCol);
	xlAstSetInt(xNode, "level", iLevel);
	return xNode;
}


// ReturnStmt: { kind, line, col, value? }
static inline xvalue xlAstCreateReturnStmt( xvalue xValue, int iLine, int iCol )
{
	xvalue xNode = xlAstCreate(XL_AST_RETURN_STMT, iLine, iCol);
	xlAstSetNode(xNode, "value", xValue);
	return xNode;
}



// ============================================================================
// AST 节点创建 - 表达式
// ============================================================================

// BinaryExpr: { kind, line, col, op, left, right }
static inline xvalue xlAstCreateBinaryExpr( const char* pszOp,
                                            xvalue xLeft, xvalue xRight,
                                            int iLine, int iCol )
{
	xvalue xNode = xlAstCreate(XL_AST_BINARY_EXPR, iLine, iCol);
	xlAstSetStr(xNode, "op", pszOp, 0);
	xlAstSetNode(xNode, "left", xLeft);
	xlAstSetNode(xNode, "right", xRight);
	return xNode;
}


// UnaryExpr: { kind, line, col, op, operand, isPrefix }
static inline xvalue xlAstCreateUnaryExpr( const char* pszOp, xvalue xOperand,
                                           int bIsPrefix, int iLine, int iCol )
{
	xvalue xNode = xlAstCreate(XL_AST_UNARY_EXPR, iLine, iCol);
	xlAstSetStr(xNode, "op", pszOp, 0);
	xlAstSetNode(xNode, "operand", xOperand);
	xlAstSetBool(xNode, "isPrefix", bIsPrefix);
	return xNode;
}


// AssignExpr: { kind, line, col, op, target, value }
static inline xvalue xlAstCreateAssignExpr( const char* pszOp,
                                            xvalue xTarget, xvalue xValue,
                                            int iLine, int iCol )
{
	xvalue xNode = xlAstCreate(XL_AST_ASSIGN_EXPR, iLine, iCol);
	xlAstSetStr(xNode, "op", pszOp, 0);
	xlAstSetNode(xNode, "target", xTarget);
	xlAstSetNode(xNode, "value", xValue);
	return xNode;
}


// CallExpr: { kind, line, col, callee, args[] }
static inline xvalue xlAstCreateCallExpr( xvalue xCallee, xvalue xArgs,
                                          int iLine, int iCol )
{
	xvalue xNode = xlAstCreate(XL_AST_CALL_EXPR, iLine, iCol);
	xlAstSetNode(xNode, "callee", xCallee);
	xlAstSetNode(xNode, "args", xArgs ? xArgs : xlAstCreateArray());
	return xNode;
}


// IndexExpr: { kind, line, col, object, index }
static inline xvalue xlAstCreateIndexExpr( xvalue xObject, xvalue xIndex,
                                           int iLine, int iCol )
{
	xvalue xNode = xlAstCreate(XL_AST_INDEX_EXPR, iLine, iCol);
	xlAstSetNode(xNode, "object", xObject);
	xlAstSetNode(xNode, "index", xIndex);
	return xNode;
}


// MemberExpr: { kind, line, col, object, member, isSafe }
static inline xvalue xlAstCreateMemberExpr( xvalue xObject,
                                            const char* pszMember, int iMemberLen,
                                            int bIsSafe, int iLine, int iCol )
{
	xvalue xNode = xlAstCreate(XL_AST_MEMBER_EXPR, iLine, iCol);
	xlAstSetNode(xNode, "object", xObject);
	xlAstSetStr(xNode, "member", pszMember, iMemberLen);
	xlAstSetBool(xNode, "isSafe", bIsSafe);
	return xNode;
}


// TernaryExpr: { kind, line, col, cond, then, else }
static inline xvalue xlAstCreateTernaryExpr( xvalue xCond, xvalue xThen,
                                             xvalue xElse, int iLine, int iCol )
{
	xvalue xNode = xlAstCreate(XL_AST_TERNARY_EXPR, iLine, iCol);
	xlAstSetNode(xNode, "cond", xCond);
	xlAstSetNode(xNode, "then", xThen);
	xlAstSetNode(xNode, "else", xElse);
	return xNode;
}



// ============================================================================
// AST 节点创建 - 字面量
// ============================================================================

// Ident: { kind, line, col, name }
static inline xvalue xlAstCreateIdent( const char* pszName, int iLen,
                                       int iLine, int iCol )
{
	xvalue xNode = xlAstCreate(XL_AST_IDENT, iLine, iCol);
	xlAstSetStr(xNode, "name", pszName, iLen);
	return xNode;
}


// IntLit: { kind, line, col, value }
static inline xvalue xlAstCreateIntLit( int64_t iVal, int iLine, int iCol )
{
	xvalue xNode = xlAstCreate(XL_AST_INT_LIT, iLine, iCol);
	xlAstSetInt(xNode, "value", iVal);
	return xNode;
}


// FloatLit: { kind, line, col, value }
static inline xvalue xlAstCreateFloatLit( double fVal, int iLine, int iCol )
{
	xvalue xNode = xlAstCreate(XL_AST_FLOAT_LIT, iLine, iCol);
	xlAstSetFloat(xNode, "value", fVal);
	return xNode;
}


// StringLit: { kind, line, col, value }
static inline xvalue xlAstCreateStringLit( const char* pszVal, int iLen,
                                           int iLine, int iCol )
{
	xvalue xNode = xlAstCreate(XL_AST_STRING_LIT, iLine, iCol);
	xlAstSetStr(xNode, "value", pszVal, iLen);
	return xNode;
}


// BoolLit: { kind, line, col, value }
static inline xvalue xlAstCreateBoolLit( int bVal, int iLine, int iCol )
{
	xvalue xNode = xlAstCreate(XL_AST_BOOL_LIT, iLine, iCol);
	xlAstSetBool(xNode, "value", bVal);
	return xNode;
}


// NullLit: { kind, line, col }
static inline xvalue xlAstCreateNullLit( int iLine, int iCol )
{
	return xlAstCreate(XL_AST_NULL_LIT, iLine, iCol);
}


// TimeLit: { kind, line, col, value }
static inline xvalue xlAstCreateTimeLit( const char* pszVal, int iLen,
                                         int iLine, int iCol )
{
	xvalue xNode = xlAstCreate(XL_AST_TIME_LIT, iLine, iCol);
	xlAstSetStr(xNode, "value", pszVal, iLen);
	return xNode;
}


// ArrayLit: { kind, line, col, elements[] }
static inline xvalue xlAstCreateArrayLit( xvalue xElements, int iLine, int iCol )
{
	xvalue xNode = xlAstCreate(XL_AST_ARRAY_LIT, iLine, iCol);
	xlAstSetNode(xNode, "elements", xElements ? xElements : xlAstCreateArray());
	return xNode;
}


// DictLit: { kind, line, col, entries[] }
// entries 中每项为 { key, value } 的 table
static inline xvalue xlAstCreateDictLit( xvalue xEntries, int iLine, int iCol )
{
	xvalue xNode = xlAstCreate(XL_AST_DICT_LIT, iLine, iCol);
	xlAstSetNode(xNode, "entries", xEntries ? xEntries : xlAstCreateArray());
	return xNode;
}



// ============================================================================
// AST 打印
// ============================================================================

static void xlAstPrintNode( xvalue xNode, int iIndent );

static inline void xlAstPrintIndent( int iIndent )
{
	for ( int i = 0; i < iIndent; i++ ) {
		printf("\t");
	}
}

static inline void xlAstPrintArray( xvalue xArr, const char* pszName, int iIndent )
{
	if ( !xArr ) return;
	int iLen = xlAstArrayLen(xArr);
	if ( iLen == 0 ) {
		xlAstPrintIndent(iIndent);
		printf("%s: []\n", pszName);
		return;
	}
	xlAstPrintIndent(iIndent);
	printf("%s: [\n", pszName);
	for ( int i = 0; i < iLen; i++ ) {
		xlAstPrintNode(xlAstArrayGet(xArr, i), iIndent + 1);
	}
	xlAstPrintIndent(iIndent);
	printf("]\n");
}


static void xlAstPrintNode( xvalue xNode, int iIndent )
{
	if ( !xNode || (xvoType(xNode) != XVO_DT_TABLE) ) {
		xlAstPrintIndent(iIndent);
		printf("(null)\n");
		return;
	}

	const char* pszKind = xlAstGetKind(xNode);
	int64_t iLine = xlAstGetInt(xNode, "line");
	int64_t iCol = xlAstGetInt(xNode, "col");

	xlAstPrintIndent(iIndent);
	printf("%s [%lld:%lld] {\n", pszKind ? pszKind : "?", (long long)iLine, (long long)iCol);

	if ( !pszKind ) {
		xlAstPrintIndent(iIndent);
		printf("}\n");
		return;
	}

	/* 通用字段 */
	const char* pszName = xlAstGetStr(xNode, "name");
	if ( pszName ) {
		xlAstPrintIndent(iIndent + 1);
		printf("name: \"%s\"\n", pszName);
	}

	const char* pszOp = xlAstGetStr(xNode, "op");
	if ( pszOp ) {
		xlAstPrintIndent(iIndent + 1);
		printf("op: \"%s\"\n", pszOp);
	}

	const char* pszVarType = xlAstGetStr(xNode, "varType");
	if ( pszVarType ) {
		xlAstPrintIndent(iIndent + 1);
		printf("varType: \"%s\"\n", pszVarType);
	}

	const char* pszParamType = xlAstGetStr(xNode, "paramType");
	if ( pszParamType ) {
		xlAstPrintIndent(iIndent + 1);
		printf("paramType: \"%s\"\n", pszParamType);
	}

	const char* pszReturnType = xlAstGetStr(xNode, "returnType");
	if ( pszReturnType ) {
		xlAstPrintIndent(iIndent + 1);
		printf("returnType: \"%s\"\n", pszReturnType);
	}

	const char* pszMember = xlAstGetStr(xNode, "member");
	if ( pszMember ) {
		xlAstPrintIndent(iIndent + 1);
		printf("member: \"%s\"\n", pszMember);
	}

	const char* pszFile = xlAstGetStr(xNode, "file");
	if ( pszFile ) {
		xlAstPrintIndent(iIndent + 1);
		printf("file: \"%s\"\n", pszFile);
	}

	const char* pszVar = xlAstGetStr(xNode, "var");
	if ( pszVar ) {
		xlAstPrintIndent(iIndent + 1);
		printf("var: \"%s\"\n", pszVar);
	}

	const char* pszKey = xlAstGetStr(xNode, "key");
	if ( pszKey ) {
		xlAstPrintIndent(iIndent + 1);
		printf("key: \"%s\"\n", pszKey);
	}

	const char* pszValue = xlAstGetStr(xNode, "value");
	if ( pszValue && (strcmp(pszKind, XL_AST_FOR_IN_STMT) == 0) ) {
		xlAstPrintIndent(iIndent + 1);
		printf("value: \"%s\"\n", pszValue);
	}

	/* 字面量值 */
	if ( strcmp(pszKind, XL_AST_INT_LIT) == 0 ) {
		xlAstPrintIndent(iIndent + 1);
		printf("value: %lld\n", (long long)xlAstGetInt(xNode, "value"));
	} else if ( strcmp(pszKind, XL_AST_FLOAT_LIT) == 0 ) {
		xlAstPrintIndent(iIndent + 1);
		xvalue xVal = xvoTableGetValue(xNode, (str)"value", 0);
		printf("value: %g\n", xvoGetFloat(xVal));
	} else if ( (strcmp(pszKind, XL_AST_STRING_LIT) == 0) || (strcmp(pszKind, XL_AST_TIME_LIT) == 0) ) {
		const char* pszV = xlAstGetStr(xNode, "value");
		xlAstPrintIndent(iIndent + 1);
		printf("value: \"%s\"\n", pszV ? pszV : "");
	} else if ( strcmp(pszKind, XL_AST_BOOL_LIT) == 0 ) {
		xlAstPrintIndent(iIndent + 1);
		xvalue xVal = xvoTableGetValue(xNode, (str)"value", 0);
		printf("value: %s\n", xvoGetBool(xVal) ? "true" : "false");
	}

	/* isConst / isPrefix / isSafe */
	xvalue xIsConst = xvoTableGetValue(xNode, (str)"isConst", 0);
	if ( xIsConst && (xvoType(xIsConst) == XVO_DT_BOOL) && xvoGetBool(xIsConst) ) {
		xlAstPrintIndent(iIndent + 1);
		printf("isConst: true\n");
	}
	xvalue xIsPrefix = xvoTableGetValue(xNode, (str)"isPrefix", 0);
	if ( xIsPrefix && (xvoType(xIsPrefix) == XVO_DT_BOOL) ) {
		xlAstPrintIndent(iIndent + 1);
		printf("isPrefix: %s\n", xvoGetBool(xIsPrefix) ? "true" : "false");
	}
	xvalue xIsSafe = xvoTableGetValue(xNode, (str)"isSafe", 0);
	if ( xIsSafe && (xvoType(xIsSafe) == XVO_DT_BOOL) && xvoGetBool(xIsSafe) ) {
		xlAstPrintIndent(iIndent + 1);
		printf("isSafe: true\n");
	}

	/* break/continue level */
	if ( (strcmp(pszKind, XL_AST_BREAK_STMT) == 0) || (strcmp(pszKind, XL_AST_CONTINUE_STMT) == 0) ) {
		xvalue xLevel = xvoTableGetValue(xNode, (str)"level", 0);
		if ( xLevel && (xvoType(xLevel) == XVO_DT_INT) ) {
			int64_t iLevel = xvoGetInt(xLevel);
			if ( iLevel > 1 ) {
				xlAstPrintIndent(iIndent + 1);
				printf("level: %lld\n", (long long)iLevel);
			}
		}
	}

	/* 子节点（单个） */
	#define XL_AST_PRINT_CHILD(field) do { \
		xvalue x = xlAstGetNode(xNode, field); \
		if ( x && (xvoType(x) == XVO_DT_TABLE) ) { \
			xlAstPrintIndent(iIndent + 1); \
			printf(field ":\n"); \
			xlAstPrintNode(x, iIndent + 2); \
		} \
	} while(0)

	XL_AST_PRINT_CHILD("init");
	XL_AST_PRINT_CHILD("left");
	XL_AST_PRINT_CHILD("right");
	XL_AST_PRINT_CHILD("operand");
	XL_AST_PRINT_CHILD("target");
	XL_AST_PRINT_CHILD("cond");
	XL_AST_PRINT_CHILD("callee");
	XL_AST_PRINT_CHILD("object");
	XL_AST_PRINT_CHILD("index");
	XL_AST_PRINT_CHILD("expr");
	XL_AST_PRINT_CHILD("start");
	XL_AST_PRINT_CHILD("end");
	XL_AST_PRINT_CHILD("step");
	XL_AST_PRINT_CHILD("iter");
	XL_AST_PRINT_CHILD("key");
	XL_AST_PRINT_CHILD("default");

	/* then / else（可能是 table 或 null） */
	xvalue xThen = xlAstGetNode(xNode, "then");
	if ( xThen && (xvoType(xThen) == XVO_DT_TABLE) ) {
		xlAstPrintIndent(iIndent + 1);
		printf("then:\n");
		xlAstPrintNode(xThen, iIndent + 2);
	}
	xvalue xElse = xlAstGetNode(xNode, "else");
	if ( xElse && (xvoType(xElse) == XVO_DT_TABLE) ) {
		xlAstPrintIndent(iIndent + 1);
		printf("else:\n");
		xlAstPrintNode(xElse, iIndent + 2);
	}

	/* return value */
	xvalue xRetVal = xlAstGetNode(xNode, "value");
	if ( xRetVal && (xvoType(xRetVal) == XVO_DT_TABLE) && (strcmp(pszKind, XL_AST_RETURN_STMT) == 0) ) {
		xlAstPrintIndent(iIndent + 1);
		printf("value:\n");
		xlAstPrintNode(xRetVal, iIndent + 2);
	}

	/* assign value */
	xvalue xAssignVal = xlAstGetNode(xNode, "value");
	if ( xAssignVal && (xvoType(xAssignVal) == XVO_DT_TABLE) && (strcmp(pszKind, XL_AST_ASSIGN_EXPR) == 0) ) {
		xlAstPrintIndent(iIndent + 1);
		printf("value:\n");
		xlAstPrintNode(xAssignVal, iIndent + 2);
	}

	/* dict entry value */
	if ( xAssignVal && (xvoType(xAssignVal) == XVO_DT_TABLE) && (strcmp(pszKind, "DictEntry") == 0) ) {
		xlAstPrintIndent(iIndent + 1);
		printf("value:\n");
		xlAstPrintNode(xAssignVal, iIndent + 2);
	}

	/* 数组字段 */
	xvalue xBody = xlAstGetNode(xNode, "body");
	if ( xBody ) {
		if ( xvoType(xBody) == XVO_DT_ARRAY ) {
			xlAstPrintArray(xBody, "body", iIndent + 1);
		} else if ( xvoType(xBody) == XVO_DT_TABLE ) {
			xlAstPrintIndent(iIndent + 1);
			printf("body:\n");
			xlAstPrintNode(xBody, iIndent + 2);
		}
	}

	xvalue xParams = xlAstGetNode(xNode, "params");
	if ( xParams && (xvoType(xParams) == XVO_DT_ARRAY) ) {
		xlAstPrintArray(xParams, "params", iIndent + 1);
	}

	xvalue xArgs = xlAstGetNode(xNode, "args");
	if ( xArgs && (xvoType(xArgs) == XVO_DT_ARRAY) ) {
		xlAstPrintArray(xArgs, "args", iIndent + 1);
	}

	xvalue xStmts = xlAstGetNode(xNode, "stmts");
	if ( xStmts && (xvoType(xStmts) == XVO_DT_ARRAY) ) {
		xlAstPrintArray(xStmts, "stmts", iIndent + 1);
	}

	xvalue xElements = xlAstGetNode(xNode, "elements");
	if ( xElements && (xvoType(xElements) == XVO_DT_ARRAY) ) {
		xlAstPrintArray(xElements, "elements", iIndent + 1);
	}

	xvalue xEntries = xlAstGetNode(xNode, "entries");
	if ( xEntries && (xvoType(xEntries) == XVO_DT_ARRAY) ) {
		xlAstPrintArray(xEntries, "entries", iIndent + 1);
	}

	xvalue xElseIfs = xlAstGetNode(xNode, "elseifs");
	if ( xElseIfs && (xvoType(xElseIfs) == XVO_DT_ARRAY) ) {
		xlAstPrintArray(xElseIfs, "elseifs", iIndent + 1);
	}

	#undef XL_AST_PRINT_CHILD

	xlAstPrintIndent(iIndent);
	printf("}\n");
}


// 打印 AST 入口
static inline void xlAstPrint( xvalue xRoot )
{
	printf("=== AST ===\n");
	xlAstPrintNode(xRoot, 0);
}


#endif /* XL_AST_H */
