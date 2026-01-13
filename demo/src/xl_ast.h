/**
 * xlang - AST 模块
 * 
 * 使用 xvalue (xrt table) 表示 AST 节点
 * 提供 AST 创建、操作和打印功能
 */

#ifndef XL_AST_H
#define XL_AST_H

#include "xl_common.h"
#include "xl_error.h"




// ============================================================================
// AST 节点类型常量
// ============================================================================

// 顶层节点
#define XL_AST_MODULE           "Module"
#define XL_AST_IMPORT           "Import"

// 声明节点
#define XL_AST_VAR_DECL         "VarDecl"
#define XL_AST_CONST_DECL       "ConstDecl"
#define XL_AST_FUNC_DECL        "FuncDecl"
#define XL_AST_CLASS_DECL       "ClassDecl"
#define XL_AST_PARAM_DECL       "ParamDecl"
#define XL_AST_FIELD_DECL       "FieldDecl"
#define XL_AST_METHOD_DECL      "MethodDecl"
#define XL_AST_ENUM_DECL        "EnumDecl"
#define XL_AST_TYPEDEF_DECL     "TypedefDecl"

// 语句节点
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
#define XL_AST_WITH_STMT        "WithStmt"
#define XL_AST_TRY_STMT         "TryStmt"
#define XL_AST_THROW_STMT       "ThrowStmt"
#define XL_AST_ASSERT_STMT      "AssertStmt"

// 表达式节点
#define XL_AST_BINARY_EXPR      "BinaryExpr"
#define XL_AST_UNARY_EXPR       "UnaryExpr"
#define XL_AST_CALL_EXPR        "CallExpr"
#define XL_AST_INDEX_EXPR       "IndexExpr"
#define XL_AST_SLICE_EXPR       "SliceExpr"
#define XL_AST_MEMBER_EXPR      "MemberExpr"
#define XL_AST_ASSIGN_EXPR      "AssignExpr"
#define XL_AST_TERNARY_EXPR     "TernaryExpr"
#define XL_AST_LAMBDA_EXPR      "LambdaExpr"
#define XL_AST_NEW_EXPR         "NewExpr"

// 字面量节点
#define XL_AST_IDENT            "Ident"
#define XL_AST_INT_LIT          "IntLit"
#define XL_AST_FLOAT_LIT        "FloatLit"
#define XL_AST_STRING_LIT       "StringLit"
#define XL_AST_BOOL_LIT         "BoolLit"
#define XL_AST_NULL_LIT         "NullLit"
#define XL_AST_TIME_LIT         "TimeLit"
#define XL_AST_ARRAY_LIT        "ArrayLit"
#define XL_AST_TABLE_LIT        "TableLit"
#define XL_AST_LIST_LIT         "ListLit"
#define XL_AST_DICT_LIT         "DictLit"
#define XL_AST_SET_LIT          "SetLit"

// 类型引用节点
#define XL_AST_TYPE_REF         "TypeRef"
#define XL_AST_GENERIC_TYPE     "GenericType"




// ============================================================================
// AST 节点创建 - 基础函数
// ============================================================================

// 创建基本 AST 节点（设置 kind, line, col）
static inline xvalue xlAstCreate( str pszKind, u32 iLine, u32 iCol )
{
	xvalue xNode = xvoCreateTable();
	xvoTableSetText(xNode, (str)"kind", 0, pszKind, 0, FALSE);
	xvoTableSetInt(xNode, (str)"line", 0, iLine);
	xvoTableSetInt(xNode, (str)"col", 0, iCol);
	return xNode;
}


// 设置字符串字段
static inline void xlAstSetStr( xvalue xNode, str pszKey, str pszValue )
{
	if ( pszValue ) {
		xvoTableSetText(xNode, pszKey, 0, pszValue, 0, FALSE);
	}
}


// 设置字符串字段（带长度）
static inline void xlAstSetStrN( xvalue xNode, str pszKey, str pszValue, u32 iLen )
{
	if ( pszValue && (iLen > 0) ) {
		str pszCopy = xlStrDup(pszValue, iLen);
		xvoTableSetText(xNode, pszKey, 0, pszCopy, iLen, TRUE);
	}
}


// 设置整数字段
static inline void xlAstSetInt( xvalue xNode, str pszKey, i64 iValue )
{
	xvoTableSetInt(xNode, pszKey, 0, iValue);
}


// 设置浮点数字段
static inline void xlAstSetFloat( xvalue xNode, str pszKey, f64 fValue )
{
	xvoTableSetFloat(xNode, pszKey, 0, fValue);
}


// 设置布尔字段
static inline void xlAstSetBool( xvalue xNode, str pszKey, bool bValue )
{
	xvoTableSetBool(xNode, pszKey, 0, bValue);
}


// 设置子节点字段
static inline void xlAstSetNode( xvalue xNode, str pszKey, xvalue xChild )
{
	if ( xChild ) {
		xvoTableSetValue(xNode, pszKey, 0, xChild, FALSE);
	}
}


// 获取字符串字段
static inline str xlAstGetStr( xvalue xNode, str pszKey )
{
	xvalue xVal = xvoTableGetValue(xNode, pszKey, 0);
	if ( xvoType(xVal) == XVO_DT_TEXT ) {
		return xvoGetText(xVal);
	}
	return NULL;
}


// 获取整数字段
static inline i64 xlAstGetInt( xvalue xNode, str pszKey )
{
	xvalue xVal = xvoTableGetValue(xNode, pszKey, 0);
	return xvoGetInt(xVal);
}


// 获取布尔字段
static inline bool xlAstGetBool( xvalue xNode, str pszKey )
{
	xvalue xVal = xvoTableGetValue(xNode, pszKey, 0);
	return xvoGetBool(xVal);
}


// 获取子节点字段
static inline xvalue xlAstGetNode( xvalue xNode, str pszKey )
{
	return xvoTableGetValue(xNode, pszKey, 0);
}


// 获取节点类型
static inline str xlAstGetKind( xvalue xNode )
{
	return xlAstGetStr(xNode, (str)"kind");
}




// ============================================================================
// AST 数组操作
// ============================================================================

// 创建 AST 数组（用于 body、params 等列表字段）
static inline xvalue xlAstCreateArray( void )
{
	return xvoCreateArray();
}


// 向数组添加节点
static inline void xlAstArrayAppend( xvalue xArray, xvalue xNode )
{
	if ( xArray && xNode ) {
		xvoArrayAppendValue(xArray, xNode, FALSE);
	}
}


// 获取数组长度
static inline int xlAstArrayLen( xvalue xArray )
{
	if ( !xArray ) return 0;
	return xvoArrayItemCount(xArray);
}


// 获取数组元素
static inline xvalue xlAstArrayGet( xvalue xArray, int iIndex )
{
	if ( !xArray ) return 0;
	return xvoArrayGetValue(xArray, iIndex);
}




// ============================================================================
// AST 节点创建 - 顶层节点
// ============================================================================

// 创建模块节点
static inline xvalue xlAstCreateModule( str pszFile )
{
	xvalue xNode = xlAstCreate((str)XL_AST_MODULE, 1, 1);
	xlAstSetStr(xNode, (str)"file", pszFile);
	xlAstSetNode(xNode, (str)"body", xlAstCreateArray());
	return xNode;
}


// 向模块添加声明
static inline void xlAstModuleAddDecl( xvalue xModule, xvalue xDecl )
{
	xvalue xBody = xlAstGetNode(xModule, (str)"body");
	xlAstArrayAppend(xBody, xDecl);
}




// ============================================================================
// AST 节点创建 - 声明节点
// ============================================================================

// 创建变量声明节点
// VarDecl: { kind, line, col, name, type?, init?, isConst }
static inline xvalue xlAstCreateVarDecl( str pszName, u32 iNameLen,
                                         xvalue xType, xvalue xInit,
                                         bool bIsConst,
                                         u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_VAR_DECL, iLine, iCol);
	xlAstSetStrN(xNode, (str)"name", pszName, iNameLen);
	xlAstSetNode(xNode, (str)"type", xType);
	xlAstSetNode(xNode, (str)"init", xInit);
	xlAstSetBool(xNode, (str)"isConst", bIsConst);
	return xNode;
}


// 创建参数声明节点
// ParamDecl: { kind, line, col, name, type?, defaultValue?, isVariadic }
static inline xvalue xlAstCreateParamDecl( str pszName, u32 iNameLen,
                                           xvalue xType, xvalue xDefault,
                                           bool bIsVariadic,
                                           u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_PARAM_DECL, iLine, iCol);
	xlAstSetStrN(xNode, (str)"name", pszName, iNameLen);
	xlAstSetNode(xNode, (str)"type", xType);
	xlAstSetNode(xNode, (str)"default", xDefault);
	xlAstSetBool(xNode, (str)"isVariadic", bIsVariadic);
	return xNode;
}


// 创建函数声明节点
// FuncDecl: { kind, line, col, name, params[], returns[], body, isStatic }
static inline xvalue xlAstCreateFuncDecl( str pszName, u32 iNameLen,
                                          xvalue xParams, xvalue xReturns,
                                          xvalue xBody, bool bIsStatic,
                                          u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_FUNC_DECL, iLine, iCol);
	xlAstSetStrN(xNode, (str)"name", pszName, iNameLen);
	xlAstSetNode(xNode, (str)"params", xParams ? xParams : xlAstCreateArray());
	xlAstSetNode(xNode, (str)"returns", xReturns ? xReturns : xlAstCreateArray());
	xlAstSetNode(xNode, (str)"body", xBody);
	xlAstSetBool(xNode, (str)"isStatic", bIsStatic);
	return xNode;
}


// 创建类声明节点
// ClassDecl: { kind, line, col, name, parent?, members[] }
static inline xvalue xlAstCreateClassDecl( str pszName, u32 iNameLen,
                                           xvalue xParent, xvalue xMembers,
                                           u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_CLASS_DECL, iLine, iCol);
	xlAstSetStrN(xNode, (str)"name", pszName, iNameLen);
	xlAstSetNode(xNode, (str)"parent", xParent);
	xlAstSetNode(xNode, (str)"members", xMembers ? xMembers : xlAstCreateArray());
	return xNode;
}




// ============================================================================
// AST 节点创建 - 语句节点
// ============================================================================

// 创建块语句节点
// BlockStmt: { kind, line, col, stmts[] }
static inline xvalue xlAstCreateBlockStmt( u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_BLOCK_STMT, iLine, iCol);
	xlAstSetNode(xNode, (str)"stmts", xlAstCreateArray());
	return xNode;
}


// 向块语句添加语句
static inline void xlAstBlockAddStmt( xvalue xBlock, xvalue xStmt )
{
	xvalue xStmts = xlAstGetNode(xBlock, (str)"stmts");
	xlAstArrayAppend(xStmts, xStmt);
}


// 创建表达式语句节点
// ExprStmt: { kind, line, col, expr }
static inline xvalue xlAstCreateExprStmt( xvalue xExpr, u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_EXPR_STMT, iLine, iCol);
	xlAstSetNode(xNode, (str)"expr", xExpr);
	return xNode;
}


// 创建 if 语句节点
// IfStmt: { kind, line, col, cond, then, elseifs[], else? }
static inline xvalue xlAstCreateIfStmt( xvalue xCond, xvalue xThen,
                                        xvalue xElseIfs, xvalue xElse,
                                        u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_IF_STMT, iLine, iCol);
	xlAstSetNode(xNode, (str)"cond", xCond);
	xlAstSetNode(xNode, (str)"then", xThen);
	xlAstSetNode(xNode, (str)"elseifs", xElseIfs ? xElseIfs : xlAstCreateArray());
	xlAstSetNode(xNode, (str)"else", xElse);
	return xNode;
}


// 创建 for 语句节点（传统 for: i = 1 to 10 step 1）
// ForStmt: { kind, line, col, var, start, end, step?, body }
static inline xvalue xlAstCreateForStmt( str pszVar, u32 iVarLen,
                                         xvalue xStart, xvalue xEnd, xvalue xStep,
                                         xvalue xBody, u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_FOR_STMT, iLine, iCol);
	xlAstSetStrN(xNode, (str)"var", pszVar, iVarLen);
	xlAstSetNode(xNode, (str)"start", xStart);
	xlAstSetNode(xNode, (str)"end", xEnd);
	xlAstSetNode(xNode, (str)"step", xStep);
	xlAstSetNode(xNode, (str)"body", xBody);
	return xNode;
}


// 创建 for-in 语句节点
// ForInStmt: { kind, line, col, key?, value, iter, body }
static inline xvalue xlAstCreateForInStmt( str pszKey, u32 iKeyLen,
                                           str pszValue, u32 iValueLen,
                                           xvalue xIter, xvalue xBody,
                                           u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_FOR_IN_STMT, iLine, iCol);
	if ( pszKey && (iKeyLen > 0) ) {
		xlAstSetStrN(xNode, (str)"key", pszKey, iKeyLen);
	}
	xlAstSetStrN(xNode, (str)"value", pszValue, iValueLen);
	xlAstSetNode(xNode, (str)"iter", xIter);
	xlAstSetNode(xNode, (str)"body", xBody);
	return xNode;
}


// 创建 while 语句节点
// WhileStmt: { kind, line, col, cond, body }
static inline xvalue xlAstCreateWhileStmt( xvalue xCond, xvalue xBody,
                                           u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_WHILE_STMT, iLine, iCol);
	xlAstSetNode(xNode, (str)"cond", xCond);
	xlAstSetNode(xNode, (str)"body", xBody);
	return xNode;
}


// 创建 do-while 语句节点
// DoWhileStmt: { kind, line, col, body, cond }
static inline xvalue xlAstCreateDoWhileStmt( xvalue xBody, xvalue xCond,
                                             u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_DO_WHILE_STMT, iLine, iCol);
	xlAstSetNode(xNode, (str)"body", xBody);
	xlAstSetNode(xNode, (str)"cond", xCond);
	return xNode;
}


// 创建 break 语句节点
// BreakStmt: { kind, line, col, level }
static inline xvalue xlAstCreateBreakStmt( i32 iLevel, u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_BREAK_STMT, iLine, iCol);
	xlAstSetNode(xNode, (str)"level", xvoCreateInt(iLevel));
	return xNode;
}


// 创建 continue 语句节点
// ContinueStmt: { kind, line, col, level }
static inline xvalue xlAstCreateContinueStmt( i32 iLevel, u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_CONTINUE_STMT, iLine, iCol);
	xlAstSetNode(xNode, (str)"level", xvoCreateInt(iLevel));
	return xNode;
}


// 创建 return 语句节点
// ReturnStmt: { kind, line, col, values[] }
static inline xvalue xlAstCreateReturnStmt( xvalue xValues, u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_RETURN_STMT, iLine, iCol);
	xlAstSetNode(xNode, (str)"values", xValues ? xValues : xlAstCreateArray());
	return xNode;
}


// 创建 try 语句节点
// TryStmt: { kind, line, col, try, catch?, catchVar?, finally? }
static inline xvalue xlAstCreateTryStmt( xvalue xTry, xvalue xCatch,
                                         str pszCatchVar, u32 iCatchVarLen,
                                         xvalue xFinally, u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_TRY_STMT, iLine, iCol);
	xlAstSetNode(xNode, (str)"try", xTry);
	xlAstSetNode(xNode, (str)"catch", xCatch);
	if ( pszCatchVar && (iCatchVarLen > 0) ) {
		xlAstSetStrN(xNode, (str)"catchVar", pszCatchVar, iCatchVarLen);
	}
	xlAstSetNode(xNode, (str)"finally", xFinally);
	return xNode;
}


// 创建 throw 语句节点
// ThrowStmt: { kind, line, col, expr }
static inline xvalue xlAstCreateThrowStmt( xvalue xExpr, u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_THROW_STMT, iLine, iCol);
	xlAstSetNode(xNode, (str)"expr", xExpr);
	return xNode;
}




// ============================================================================
// AST 节点创建 - 表达式节点
// ============================================================================

// 创建二元表达式节点
// BinaryExpr: { kind, line, col, op, left, right }
static inline xvalue xlAstCreateBinaryExpr( str pszOp, xvalue xLeft, xvalue xRight,
                                            u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_BINARY_EXPR, iLine, iCol);
	xlAstSetStr(xNode, (str)"op", pszOp);
	xlAstSetNode(xNode, (str)"left", xLeft);
	xlAstSetNode(xNode, (str)"right", xRight);
	return xNode;
}


// 创建一元表达式节点
// UnaryExpr: { kind, line, col, op, operand, isPrefix }
static inline xvalue xlAstCreateUnaryExpr( str pszOp, xvalue xOperand,
                                           bool bIsPrefix, u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_UNARY_EXPR, iLine, iCol);
	xlAstSetStr(xNode, (str)"op", pszOp);
	xlAstSetNode(xNode, (str)"operand", xOperand);
	xlAstSetBool(xNode, (str)"isPrefix", bIsPrefix);
	return xNode;
}


// 创建赋值表达式节点
// AssignExpr: { kind, line, col, op, target, value }
static inline xvalue xlAstCreateAssignExpr( str pszOp, xvalue xTarget, xvalue xValue,
                                            u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_ASSIGN_EXPR, iLine, iCol);
	xlAstSetStr(xNode, (str)"op", pszOp);
	xlAstSetNode(xNode, (str)"target", xTarget);
	xlAstSetNode(xNode, (str)"value", xValue);
	return xNode;
}


// 创建函数调用表达式节点
// CallExpr: { kind, line, col, callee, args[] }
static inline xvalue xlAstCreateCallExpr( xvalue xCallee, xvalue xArgs,
                                          u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_CALL_EXPR, iLine, iCol);
	xlAstSetNode(xNode, (str)"callee", xCallee);
	xlAstSetNode(xNode, (str)"args", xArgs ? xArgs : xlAstCreateArray());
	return xNode;
}


// 创建索引表达式节点
// IndexExpr: { kind, line, col, object, index }
static inline xvalue xlAstCreateIndexExpr( xvalue xObject, xvalue xIndex,
                                           u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_INDEX_EXPR, iLine, iCol);
	xlAstSetNode(xNode, (str)"object", xObject);
	xlAstSetNode(xNode, (str)"index", xIndex);
	return xNode;
}


// 创建切片表达式节点
// SliceExpr: { kind, line, col, object, start, end }
static inline xvalue xlAstCreateSliceExpr( xvalue xObject, xvalue xStart, xvalue xEnd,
                                           u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_SLICE_EXPR, iLine, iCol);
	xlAstSetNode(xNode, (str)"object", xObject);
	xlAstSetNode(xNode, (str)"start", xStart);
	xlAstSetNode(xNode, (str)"end", xEnd);
	return xNode;
}


// 创建成员访问表达式节点
// MemberExpr: { kind, line, col, object, member, isSafe }
static inline xvalue xlAstCreateMemberExpr( xvalue xObject, str pszMember, u32 iMemberLen,
                                            bool bIsSafe, u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_MEMBER_EXPR, iLine, iCol);
	xlAstSetNode(xNode, (str)"object", xObject);
	xlAstSetStrN(xNode, (str)"member", pszMember, iMemberLen);
	xlAstSetBool(xNode, (str)"isSafe", bIsSafe);
	return xNode;
}


// 创建三元表达式节点
// TernaryExpr: { kind, line, col, cond, then, else }
static inline xvalue xlAstCreateTernaryExpr( xvalue xCond, xvalue xThen, xvalue xElse,
                                             u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_TERNARY_EXPR, iLine, iCol);
	xlAstSetNode(xNode, (str)"cond", xCond);
	xlAstSetNode(xNode, (str)"then", xThen);
	xlAstSetNode(xNode, (str)"else", xElse);
	return xNode;
}


// 创建 lambda 表达式节点
// LambdaExpr: { kind, line, col, params[], returns[], body }
static inline xvalue xlAstCreateLambdaExpr( xvalue xParams, xvalue xReturns,
                                            xvalue xBody, u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_LAMBDA_EXPR, iLine, iCol);
	xlAstSetNode(xNode, (str)"params", xParams ? xParams : xlAstCreateArray());
	xlAstSetNode(xNode, (str)"returns", xReturns);
	xlAstSetNode(xNode, (str)"body", xBody);
	return xNode;
}


// 创建 new 表达式节点
// NewExpr: { kind, line, col, type, args[] }
static inline xvalue xlAstCreateNewExpr( xvalue xType, xvalue xArgs,
                                         u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_NEW_EXPR, iLine, iCol);
	xlAstSetNode(xNode, (str)"type", xType);
	xlAstSetNode(xNode, (str)"args", xArgs ? xArgs : xlAstCreateArray());
	return xNode;
}




// ============================================================================
// AST 节点创建 - 字面量节点
// ============================================================================

// 创建标识符节点
// Ident: { kind, line, col, name }
static inline xvalue xlAstCreateIdent( str pszName, u32 iLen, u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_IDENT, iLine, iCol);
	xlAstSetStrN(xNode, (str)"name", pszName, iLen);
	return xNode;
}


// 创建整数字面量节点
// IntLit: { kind, line, col, value }
static inline xvalue xlAstCreateIntLit( i64 iValue, u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_INT_LIT, iLine, iCol);
	xlAstSetInt(xNode, (str)"value", iValue);
	return xNode;
}


// 创建浮点数字面量节点
// FloatLit: { kind, line, col, value }
static inline xvalue xlAstCreateFloatLit( f64 fValue, u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_FLOAT_LIT, iLine, iCol);
	xlAstSetFloat(xNode, (str)"value", fValue);
	return xNode;
}


// 创建字符串字面量节点
// StringLit: { kind, line, col, value }
static inline xvalue xlAstCreateStringLit( str pszValue, u32 iLen, u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_STRING_LIT, iLine, iCol);
	xlAstSetStrN(xNode, (str)"value", pszValue, iLen);
	return xNode;
}


// 创建布尔字面量节点
// BoolLit: { kind, line, col, value }
static inline xvalue xlAstCreateBoolLit( bool bValue, u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_BOOL_LIT, iLine, iCol);
	xlAstSetBool(xNode, (str)"value", bValue);
	return xNode;
}


// 创建 null 字面量节点
// NullLit: { kind, line, col }
static inline xvalue xlAstCreateNullLit( u32 iLine, u32 iCol )
{
	return xlAstCreate((str)XL_AST_NULL_LIT, iLine, iCol);
}


// 创建时间字面量节点
// TimeLit: { kind, line, col, value }
static inline xvalue xlAstCreateTimeLit( str pszValue, u32 iLen, u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_TIME_LIT, iLine, iCol);
	xlAstSetStrN(xNode, (str)"value", pszValue, iLen);
	return xNode;
}


// 创建数组字面量节点
// ArrayLit: { kind, line, col, elements[] }
static inline xvalue xlAstCreateArrayLit( xvalue xElements, u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_ARRAY_LIT, iLine, iCol);
	xlAstSetNode(xNode, (str)"elements", xElements ? xElements : xlAstCreateArray());
	return xNode;
}


// 创建表字面量节点
// TableLit: { kind, line, col, entries[] }  // entries 为 {key, value} 数组
static inline xvalue xlAstCreateTableLit( xvalue xEntries, u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_TABLE_LIT, iLine, iCol);
	xlAstSetNode(xNode, (str)"entries", xEntries ? xEntries : xlAstCreateArray());
	return xNode;
}


// 创建列表字面量节点
// ListLit: { kind, line, col, entries[] }  // entries 为 {key, value} 数组，key 为 int64
static inline xvalue xlAstCreateListLit( xvalue xEntries, u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_LIST_LIT, iLine, iCol);
	xlAstSetNode(xNode, (str)"entries", xEntries ? xEntries : xlAstCreateArray());
	return xNode;
}


// 创建字典字面量节点
// DictLit: { kind, line, col, entries[] }  // entries 为 {key, value} 数组，key 为 string
static inline xvalue xlAstCreateDictLit( xvalue xEntries, u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_DICT_LIT, iLine, iCol);
	xlAstSetNode(xNode, (str)"entries", xEntries ? xEntries : xlAstCreateArray());
	return xNode;
}


// 创建集合字面量节点
// SetLit: { kind, line, col, elements[] }
static inline xvalue xlAstCreateSetLit( xvalue xElements, u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_SET_LIT, iLine, iCol);
	xlAstSetNode(xNode, (str)"elements", xElements ? xElements : xlAstCreateArray());
	return xNode;
}




// ============================================================================
// AST 节点创建 - 类型引用节点
// ============================================================================

// 创建类型引用节点
// TypeRef: { kind, line, col, name, isArray?, isNullable? }
static inline xvalue xlAstCreateTypeRef( str pszName, u32 iLen,
                                         bool bIsArray, bool bIsNullable,
                                         u32 iLine, u32 iCol )
{
	xvalue xNode = xlAstCreate((str)XL_AST_TYPE_REF, iLine, iCol);
	xlAstSetStrN(xNode, (str)"name", pszName, iLen);
	xlAstSetBool(xNode, (str)"isArray", bIsArray);
	xlAstSetBool(xNode, (str)"isNullable", bIsNullable);
	return xNode;
}




// ============================================================================
// AST 打印
// ============================================================================

// 前向声明
static void xlAstPrintNode( xvalue xNode, int iIndent );


// 打印缩进
static inline void xlAstPrintIndent( int iIndent )
{
	for ( int i = 0; i < iIndent; i++ ) {
		printf("\t");
	}
}


// 打印数组
static inline void xlAstPrintArray( xvalue xArray, str pszName, int iIndent )
{
	if ( !xArray ) return;

	int iLen = xlAstArrayLen(xArray);
	if ( iLen == 0 ) {
		xlAstPrintIndent(iIndent);
		printf("%s: []\n", (char*)pszName);
		return;
	}

	xlAstPrintIndent(iIndent);
	printf("%s: [\n", (char*)pszName);

	for ( int i = 0; i < iLen; i++ ) {
		xlAstPrintNode(xlAstArrayGet(xArray, i), iIndent + 1);
	}

	xlAstPrintIndent(iIndent);
	printf("]\n");
}


// 打印节点
static void xlAstPrintNode( xvalue xNode, int iIndent )
{
	if ( !xNode || (xvoType(xNode) != XVO_DT_TABLE) ) {
		xlAstPrintIndent(iIndent);
		printf("(null)\n");
		return;
	}

	str pszKind = xlAstGetKind(xNode);
	i64 iLine = xlAstGetInt(xNode, (str)"line");
	i64 iCol = xlAstGetInt(xNode, (str)"col");

	xlAstPrintIndent(iIndent);
	printf("%s [%lld:%lld] {\n", pszKind ? (char*)pszKind : "?", iLine, iCol);

	// 根据节点类型打印不同字段
	if ( !pszKind ) {
		xlAstPrintIndent(iIndent);
		printf("}\n");
		return;
	}

	// 通用字段
	str pszName = xlAstGetStr(xNode, (str)"name");
	if ( pszName ) {
		xlAstPrintIndent(iIndent + 1);
		printf("name: \"%s\"\n", (char*)pszName);
	}

	str pszOp = xlAstGetStr(xNode, (str)"op");
	if ( pszOp ) {
		xlAstPrintIndent(iIndent + 1);
		printf("op: \"%s\"\n", (char*)pszOp);
	}

	// 字面量值
	if ( strcmp((char*)pszKind, XL_AST_INT_LIT) == 0 ) {
		xlAstPrintIndent(iIndent + 1);
		printf("value: %lld\n", xlAstGetInt(xNode, (str)"value"));
	} else if ( strcmp((char*)pszKind, XL_AST_FLOAT_LIT) == 0 ) {
		xlAstPrintIndent(iIndent + 1);
		xvalue xVal = xvoTableGetValue(xNode, (str)"value", 0);
		printf("value: %g\n", xvoGetFloat(xVal));
	} else if ( strcmp((char*)pszKind, XL_AST_STRING_LIT) == 0 ) {
		str pszVal = xlAstGetStr(xNode, (str)"value");
		xlAstPrintIndent(iIndent + 1);
		printf("value: \"%s\"\n", pszVal ? (char*)pszVal : "");
	} else if ( strcmp((char*)pszKind, XL_AST_BOOL_LIT) == 0 ) {
		xlAstPrintIndent(iIndent + 1);
		xvalue xVal = xvoTableGetValue(xNode, (str)"value", 0);
		printf("value: %s\n", xvoGetBool(xVal) ? "true" : "false");
	}

	// 子节点
	xvalue xType = xlAstGetNode(xNode, (str)"type");
	if ( xType && (xvoType(xType) == XVO_DT_TABLE) ) {
		xlAstPrintIndent(iIndent + 1);
		printf("type:\n");
		xlAstPrintNode(xType, iIndent + 2);
	}

	xvalue xInit = xlAstGetNode(xNode, (str)"init");
	if ( xInit && (xvoType(xInit) == XVO_DT_TABLE) ) {
		xlAstPrintIndent(iIndent + 1);
		printf("init:\n");
		xlAstPrintNode(xInit, iIndent + 2);
	}

	xvalue xLeft = xlAstGetNode(xNode, (str)"left");
	if ( xLeft && (xvoType(xLeft) == XVO_DT_TABLE) ) {
		xlAstPrintIndent(iIndent + 1);
		printf("left:\n");
		xlAstPrintNode(xLeft, iIndent + 2);
	}

	xvalue xRight = xlAstGetNode(xNode, (str)"right");
	if ( xRight && (xvoType(xRight) == XVO_DT_TABLE) ) {
		xlAstPrintIndent(iIndent + 1);
		printf("right:\n");
		xlAstPrintNode(xRight, iIndent + 2);
	}

	xvalue xOperand = xlAstGetNode(xNode, (str)"operand");
	if ( xOperand && (xvoType(xOperand) == XVO_DT_TABLE) ) {
		xlAstPrintIndent(iIndent + 1);
		printf("operand:\n");
		xlAstPrintNode(xOperand, iIndent + 2);
	}

	xvalue xTarget = xlAstGetNode(xNode, (str)"target");
	if ( xTarget && (xvoType(xTarget) == XVO_DT_TABLE) ) {
		xlAstPrintIndent(iIndent + 1);
		printf("target:\n");
		xlAstPrintNode(xTarget, iIndent + 2);
	}

	xvalue xValue = xlAstGetNode(xNode, (str)"value");
	if ( xValue && (xvoType(xValue) == XVO_DT_TABLE) ) {
		xlAstPrintIndent(iIndent + 1);
		printf("value:\n");
		xlAstPrintNode(xValue, iIndent + 2);
	}

	xvalue xCond = xlAstGetNode(xNode, (str)"cond");
	if ( xCond && (xvoType(xCond) == XVO_DT_TABLE) ) {
		xlAstPrintIndent(iIndent + 1);
		printf("cond:\n");
		xlAstPrintNode(xCond, iIndent + 2);
	}

	// ForStmt 字段
	str pszVar = xlAstGetStr(xNode, (str)"var");
	if ( pszVar ) {
		xlAstPrintIndent(iIndent + 1);
		printf("var: \"%s\"\n", (char*)pszVar);
	}

	xvalue xStart = xlAstGetNode(xNode, (str)"start");
	if ( xStart && (xvoType(xStart) == XVO_DT_TABLE) ) {
		xlAstPrintIndent(iIndent + 1);
		printf("start:\n");
		xlAstPrintNode(xStart, iIndent + 2);
	}

	xvalue xEnd = xlAstGetNode(xNode, (str)"end");
	if ( xEnd && (xvoType(xEnd) == XVO_DT_TABLE) ) {
		xlAstPrintIndent(iIndent + 1);
		printf("end:\n");
		xlAstPrintNode(xEnd, iIndent + 2);
	}

	xvalue xStep = xlAstGetNode(xNode, (str)"step");
	if ( xStep && (xvoType(xStep) == XVO_DT_TABLE) ) {
		xlAstPrintIndent(iIndent + 1);
		printf("step:\n");
		xlAstPrintNode(xStep, iIndent + 2);
	}

	// ForInStmt 字段
	str pszKey = xlAstGetStr(xNode, (str)"key");
	if ( pszKey ) {
		xlAstPrintIndent(iIndent + 1);
		printf("key: \"%s\"\n", (char*)pszKey);
	}

	// TableEntry key 字段（子节点）
	xvalue xKeyNode = xlAstGetNode(xNode, (str)"key");
	if ( xKeyNode && (xvoType(xKeyNode) == XVO_DT_TABLE) ) {
		xlAstPrintIndent(iIndent + 1);
		printf("key:\n");
		xlAstPrintNode(xKeyNode, iIndent + 2);
	}

	str pszValueName = xlAstGetStr(xNode, (str)"value");
	if ( pszValueName && (strcmp((char*)pszKind, XL_AST_FOR_IN_STMT) == 0) ) {
		xlAstPrintIndent(iIndent + 1);
		printf("value: \"%s\"\n", (char*)pszValueName);
	}

	xvalue xIter = xlAstGetNode(xNode, (str)"iter");
	if ( xIter && (xvoType(xIter) == XVO_DT_TABLE) ) {
		xlAstPrintIndent(iIndent + 1);
		printf("iter:\n");
		xlAstPrintNode(xIter, iIndent + 2);
	}

	xvalue xThen = xlAstGetNode(xNode, (str)"then");
	if ( xThen && (xvoType(xThen) == XVO_DT_TABLE) ) {
		xlAstPrintIndent(iIndent + 1);
		printf("then:\n");
		xlAstPrintNode(xThen, iIndent + 2);
	}

	xvalue xElse = xlAstGetNode(xNode, (str)"else");
	if ( xElse && (xvoType(xElse) == XVO_DT_TABLE) ) {
		xlAstPrintIndent(iIndent + 1);
		printf("else:\n");
		xlAstPrintNode(xElse, iIndent + 2);
	}

	xvalue xCallee = xlAstGetNode(xNode, (str)"callee");
	if ( xCallee && (xvoType(xCallee) == XVO_DT_TABLE) ) {
		xlAstPrintIndent(iIndent + 1);
		printf("callee:\n");
		xlAstPrintNode(xCallee, iIndent + 2);
	}

	xvalue xObject = xlAstGetNode(xNode, (str)"object");
	if ( xObject && (xvoType(xObject) == XVO_DT_TABLE) ) {
		xlAstPrintIndent(iIndent + 1);
		printf("object:\n");
		xlAstPrintNode(xObject, iIndent + 2);
	}

	xvalue xIndex = xlAstGetNode(xNode, (str)"index");
	if ( xIndex && (xvoType(xIndex) == XVO_DT_TABLE) ) {
		xlAstPrintIndent(iIndent + 1);
		printf("index:\n");
		xlAstPrintNode(xIndex, iIndent + 2);
	}

	xvalue xExpr = xlAstGetNode(xNode, (str)"expr");
	if ( xExpr && (xvoType(xExpr) == XVO_DT_TABLE) ) {
		xlAstPrintIndent(iIndent + 1);
		printf("expr:\n");
		xlAstPrintNode(xExpr, iIndent + 2);
	}

	// 数组字段
	xvalue xBody = xlAstGetNode(xNode, (str)"body");
	if ( xBody ) {
		if ( xvoType(xBody) == XVO_DT_ARRAY ) {
			xlAstPrintArray(xBody, (str)"body", iIndent + 1);
		} else if ( xvoType(xBody) == XVO_DT_TABLE ) {
			xlAstPrintIndent(iIndent + 1);
			printf("body:\n");
			xlAstPrintNode(xBody, iIndent + 2);
		}
	}

	xvalue xParams = xlAstGetNode(xNode, (str)"params");
	if ( xParams && (xvoType(xParams) == XVO_DT_ARRAY) ) {
		xlAstPrintArray(xParams, (str)"params", iIndent + 1);
	}

	xvalue xArgs = xlAstGetNode(xNode, (str)"args");
	if ( xArgs && (xvoType(xArgs) == XVO_DT_ARRAY) ) {
		xlAstPrintArray(xArgs, (str)"args", iIndent + 1);
	}

	xvalue xReturns = xlAstGetNode(xNode, (str)"returns");
	if ( xReturns && (xvoType(xReturns) == XVO_DT_ARRAY) ) {
		xlAstPrintArray(xReturns, (str)"returns", iIndent + 1);
	}

	xvalue xStmts = xlAstGetNode(xNode, (str)"stmts");
	if ( xStmts && (xvoType(xStmts) == XVO_DT_ARRAY) ) {
		xlAstPrintArray(xStmts, (str)"stmts", iIndent + 1);
	}

	xvalue xMembers = xlAstGetNode(xNode, (str)"members");
	if ( xMembers && (xvoType(xMembers) == XVO_DT_ARRAY) ) {
		xlAstPrintArray(xMembers, (str)"members", iIndent + 1);
	}

	xvalue xElements = xlAstGetNode(xNode, (str)"elements");
	if ( xElements && (xvoType(xElements) == XVO_DT_ARRAY) ) {
		xlAstPrintArray(xElements, (str)"elements", iIndent + 1);
	}

	xvalue xEntries = xlAstGetNode(xNode, (str)"entries");
	if ( xEntries && (xvoType(xEntries) == XVO_DT_ARRAY) ) {
		xlAstPrintArray(xEntries, (str)"entries", iIndent + 1);
	}

	xvalue xValues = xlAstGetNode(xNode, (str)"values");
	if ( xValues && (xvoType(xValues) == XVO_DT_ARRAY) ) {
		xlAstPrintArray(xValues, (str)"values", iIndent + 1);
	}

	xlAstPrintIndent(iIndent);
	printf("}\n");
}


// 打印 AST（入口函数）
static inline void xlAstPrint( xvalue xRoot )
{
	printf("=== AST ===\n");
	xlAstPrintNode(xRoot, 0);
}


#endif // XL_AST_H
