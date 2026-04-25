#ifndef XL_AST_H
#define XL_AST_H

#include "../common/xl_common.h"
#include "../lexer/xl_token.h"

typedef enum XL_AstKind {
	XL_AST_LIST = 0,
	XL_AST_PROGRAM,
	XL_AST_BLOCK,
	XL_AST_FUNC_DECL,
	XL_AST_FUNC_EXPR,
	XL_AST_CLASS_DECL,
	XL_AST_UNION_DECL,
	XL_AST_CREATE_DECL,
	XL_AST_DESTROY_DECL,
	XL_AST_NOCOPY_DECL,
	XL_AST_PARAM,
	XL_AST_TYPE_REF,
	XL_AST_TYPEDEF,
	XL_AST_CONST_DECL,
	XL_AST_VAR_DECL,
	XL_AST_UNPACK_DECL,
	XL_AST_IF,
	XL_AST_ELSEIF,
	XL_AST_WHILE,
	XL_AST_FORIN,
	XL_AST_IMPORT,
	XL_AST_EXPORT,
	XL_AST_TRY,
	XL_AST_RETURN,
	XL_AST_BREAK,
	XL_AST_CONTINUE,
	XL_AST_THROW,
	XL_AST_ASSERT,
	XL_AST_YIELD,
	XL_AST_EXPR_STMT,
	XL_AST_NAMED_ARG,
	XL_AST_ASSIGN,
	XL_AST_TERNARY,
	XL_AST_BINARY,
	XL_AST_UNARY,
	XL_AST_CALL,
	XL_AST_MEMBER,
	XL_AST_INDEX,
	XL_AST_IDENT,
	XL_AST_INT,
	XL_AST_FLOAT,
	XL_AST_STRING,
	XL_AST_TEMPLATE,
	XL_AST_BOOL,
	XL_AST_NULL,
	XL_AST_TIME
} XL_AstKind;

typedef struct XL_AstNode {
	XL_AstKind eKind;
	char* sText;
	char* sText2;
	int64_t iIntValue;
	double fFloatValue;
	int bFlag;
	int iLine;
	int iColumn;
	int iEndLine;
	int iEndColumn;
	struct XL_AstNode** ppChildren;
	size_t iChildCount;
	size_t iChildCapacity;
} XL_AstNode;

#define XL_CLASS_KIND_PLAIN 0
#define XL_CLASS_KIND_CUSTOM 1

#define XL_PARAM_VARIADIC_NONE 0
#define XL_PARAM_VARIADIC_ARRAY 1
#define XL_PARAM_VARIADIC_DICT 2

XL_AstNode* xlAstCreateNode(XL_AstKind eKind, const XL_Token* pToken);
XL_AstNode* xlAstCreateList(void);
XL_AstNode* xlAstCreateProgram(void);
XL_AstNode* xlAstCreateBlock(const XL_Token* pToken);
XL_AstNode* xlAstCreateTypeRef(const XL_Token* pToken);
XL_AstNode* xlAstCreateTypeRefText(const char* sText);
XL_AstNode* xlAstCreateTypeDef(const XL_Token* pName, XL_AstNode* pType);
XL_AstNode* xlAstCreateIdent(const XL_Token* pToken);
XL_AstNode* xlAstCreateLiteralFromToken(const XL_Token* pToken);

XL_AstNode* xlAstCreateFuncDecl(const XL_Token* pName, int bAsync, XL_AstNode* pParams, XL_AstNode* pRetTypes, XL_AstNode* pBody);
XL_AstNode* xlAstCreateFuncExpr(const XL_Token* pToken, int bAsync, XL_AstNode* pParams, XL_AstNode* pRetTypes, XL_AstNode* pBody);
XL_AstNode* xlAstCreateClassDecl(const XL_Token* pName, XL_AstNode* pBaseType, XL_AstNode* pMembers);
XL_AstNode* xlAstCreateUnionDecl(const XL_Token* pName, XL_AstNode* pMembers);
XL_AstNode* xlAstCreateCreateDecl(const XL_Token* pToken, XL_AstNode* pParams, XL_AstNode* pBody);
XL_AstNode* xlAstCreateDestroyDecl(const XL_Token* pToken, XL_AstNode* pBody);
XL_AstNode* xlAstCreateNoCopyDecl(const XL_Token* pToken);
XL_AstNode* xlAstCreateParam(const XL_Token* pName, XL_AstNode* pType, XL_AstNode* pDefaultValue);
XL_AstNode* xlAstCreateConstDecl(const XL_Token* pName, XL_AstNode* pType, XL_AstNode* pValue);
XL_AstNode* xlAstCreateVarDecl(const XL_Token* pName, XL_AstNode* pType, XL_AstNode* pValue);
XL_AstNode* xlAstCreateUnpackDecl(const XL_Token* pTypeToken, XL_AstNode* pTargets, XL_AstNode* pValue);
XL_AstNode* xlAstCreateIf(const XL_Token* pToken, XL_AstNode* pCond, XL_AstNode* pThenBlock, XL_AstNode* pElseIfList, XL_AstNode* pElseBlock);
XL_AstNode* xlAstCreateElseIf(const XL_Token* pToken, XL_AstNode* pCond, XL_AstNode* pBody);
XL_AstNode* xlAstCreateWhile(const XL_Token* pToken, XL_AstNode* pCond, XL_AstNode* pBody);
XL_AstNode* xlAstCreateForIn(const XL_Token* pToken, XL_AstNode* pTargets, XL_AstNode* pIterable, XL_AstNode* pBody);
XL_AstNode* xlAstCreateImport(const XL_Token* pToken, XL_AstNode* pTarget, const XL_Token* pAlias);
XL_AstNode* xlAstCreateExport(const XL_Token* pToken, XL_AstNode* pValue);
XL_AstNode* xlAstCreateTry(const XL_Token* pToken, XL_AstNode* pTryBlock, XL_AstNode* pCatchName, XL_AstNode* pCatchBlock, XL_AstNode* pFinallyBlock);
XL_AstNode* xlAstCreateReturn(const XL_Token* pToken, XL_AstNode* pValues);
XL_AstNode* xlAstCreateBreak(const XL_Token* pToken, int iLevel);
XL_AstNode* xlAstCreateContinue(const XL_Token* pToken, int iLevel);
XL_AstNode* xlAstCreateThrow(const XL_Token* pToken, XL_AstNode* pValue);
XL_AstNode* xlAstCreateAssert(const XL_Token* pToken, XL_AstNode* pValue, XL_AstNode* pMessage);
XL_AstNode* xlAstCreateYield(const XL_Token* pToken, XL_AstNode* pValue);
XL_AstNode* xlAstCreateExprStmt(XL_AstNode* pExpr);
XL_AstNode* xlAstCreateNamedArg(const XL_Token* pName, XL_AstNode* pValue);
XL_AstNode* xlAstCreateAssign(const XL_Token* pToken, XL_AstNode* pLeft, XL_AstNode* pRight);
XL_AstNode* xlAstCreateTernary(const XL_Token* pToken, XL_AstNode* pCond, XL_AstNode* pThenValue, XL_AstNode* pElseValue);
XL_AstNode* xlAstCreateBinary(const XL_Token* pToken, const char* sOp, XL_AstNode* pLeft, XL_AstNode* pRight);
XL_AstNode* xlAstCreateUnary(const XL_Token* pToken, const char* sOp, XL_AstNode* pValue);
XL_AstNode* xlAstCreateCall(XL_AstNode* pCallee, XL_AstNode* pArgs, const XL_Token* pToken);
XL_AstNode* xlAstCreateMember(XL_AstNode* pBase, const XL_Token* pName, int bSafe, const XL_Token* pToken);
XL_AstNode* xlAstCreateIndex(XL_AstNode* pBase, XL_AstNode* pIndex, int bSafe, const XL_Token* pToken);
XL_AstNode* xlAstCreateIdentText(const char* sText);

void xlAstSetText(XL_AstNode* pNode, const char* sText, size_t iLen);
void xlAstSetText2(XL_AstNode* pNode, const char* sText, size_t iLen);
void xlAstAppendChild(XL_AstNode* pNode, XL_AstNode* pChild);
void xlAstAppendChildren(XL_AstNode* pNode, XL_AstNode* pList);
void xlAstFree(XL_AstNode* pNode);

const char* xlAstKindName(XL_AstKind eKind);

#endif
