#include "xl_ast.h"

static char* __xlAstDupText(const char* sText, size_t iLen)
{
	char* sCopy;

	sCopy = (char*)malloc(iLen + 1);
	if ( sCopy == NULL ) {
		return NULL;
	}

	if ( (sText != NULL) && (iLen != 0) ) {
		memcpy(sCopy, sText, iLen);
	}
	sCopy[iLen] = '\0';
	return sCopy;
}

static void __xlAstAdoptTokenRange(XL_AstNode* pNode, const XL_Token* pToken)
{
	if ( (pNode == NULL) || (pToken == NULL) ) {
		return;
	}

	pNode->iLine = pToken->iStartLine;
	pNode->iColumn = pToken->iStartColumn;
	pNode->iEndLine = pToken->iEndLine;
	pNode->iEndColumn = pToken->iEndColumn;
}

const char* xlAstKindName(XL_AstKind eKind)
{
	static const char* arrNames[] = {
		"List", "Program", "Block", "FuncDecl", "Param", "TypeRef", "ConstDecl", "VarDecl",
		"If", "ElseIf", "ForIn", "Import", "Export", "Try", "Return", "Break", "Continue", "Throw", "Assert", "Yield",
		"ExprStmt", "NamedArg", "Assign", "Binary", "Unary", "Call", "Member", "Index", "Ident", "Int", "Float",
		"String", "Template", "Bool", "Null", "Time"
	};

	if ( ((int)eKind >= 0) && (eKind <= XL_AST_TIME) ) {
		return arrNames[eKind];
	}
	return "Unknown";
}

XL_AstNode* xlAstCreateNode(XL_AstKind eKind, const XL_Token* pToken)
{
	XL_AstNode* pNode;

	pNode = (XL_AstNode*)calloc(1, sizeof(XL_AstNode));
	if ( pNode == NULL ) {
		return NULL;
	}

	pNode->eKind = eKind;
	__xlAstAdoptTokenRange(pNode, pToken);
	return pNode;
}

XL_AstNode* xlAstCreateList(void)
{
	return xlAstCreateNode(XL_AST_LIST, NULL);
}

XL_AstNode* xlAstCreateProgram(void)
{
	return xlAstCreateNode(XL_AST_PROGRAM, NULL);
}

XL_AstNode* xlAstCreateBlock(const XL_Token* pToken)
{
	return xlAstCreateNode(XL_AST_BLOCK, pToken);
}

XL_AstNode* xlAstCreateTypeRef(const XL_Token* pToken)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_TYPE_REF, pToken);
	if ( (pNode != NULL) && (pToken != NULL) ) {
		xlAstSetText(pNode, pToken->sText, pToken->iTextLen);
	}
	return pNode;
}

XL_AstNode* xlAstCreateIdent(const XL_Token* pToken)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_IDENT, pToken);
	if ( (pNode != NULL) && (pToken != NULL) ) {
		xlAstSetText(pNode, pToken->sText, pToken->iTextLen);
	}
	return pNode;
}

XL_AstNode* xlAstCreateLiteralFromToken(const XL_Token* pToken)
{
	XL_AstNode* pNode;

	if ( pToken == NULL ) {
		return NULL;
	}

	switch ( pToken->eKind ) {
		case XL_TK_INT:
		case XL_TK_INT_HEX:
		case XL_TK_INT_OCT:
		case XL_TK_INT_BIN:
			pNode = xlAstCreateNode(XL_AST_INT, pToken);
			if ( pNode != NULL ) {
				pNode->iIntValue = pToken->iIntValue;
				xlAstSetText(pNode, pToken->sText, pToken->iTextLen);
			}
			return pNode;
		case XL_TK_FLOAT:
			pNode = xlAstCreateNode(XL_AST_FLOAT, pToken);
			if ( pNode != NULL ) {
				pNode->fFloatValue = pToken->fFloatValue;
				xlAstSetText(pNode, pToken->sText, pToken->iTextLen);
			}
			return pNode;
		case XL_TK_STRING:
		case XL_TK_STRING_RAW:
			pNode = xlAstCreateNode(XL_AST_STRING, pToken);
			if ( pNode != NULL ) {
				xlAstSetText(pNode, pToken->sText, pToken->iTextLen);
			}
			return pNode;
		case XL_TK_TEMPLATE:
			pNode = xlAstCreateNode(XL_AST_TEMPLATE, pToken);
			if ( pNode != NULL ) {
				xlAstSetText(pNode, pToken->sText, pToken->iTextLen);
			}
			return pNode;
		case XL_TK_TIME:
			pNode = xlAstCreateNode(XL_AST_TIME, pToken);
			if ( pNode != NULL ) {
				xlAstSetText(pNode, pToken->sText, pToken->iTextLen);
			}
			return pNode;
		case XL_KW_TRUE:
		case XL_KW_FALSE:
			pNode = xlAstCreateNode(XL_AST_BOOL, pToken);
			if ( pNode != NULL ) {
				pNode->bFlag = (pToken->eKind == XL_KW_TRUE);
				xlAstSetText(pNode, pToken->sText, pToken->iTextLen);
			}
			return pNode;
		case XL_KW_NULL:
			pNode = xlAstCreateNode(XL_AST_NULL, pToken);
			if ( pNode != NULL ) {
				xlAstSetText(pNode, pToken->sText, pToken->iTextLen);
			}
			return pNode;
		default:
			return NULL;
	}
}

XL_AstNode* xlAstCreateFuncDecl(const XL_Token* pName, int bAsync, XL_AstNode* pParams, XL_AstNode* pRetType, XL_AstNode* pBody)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_FUNC_DECL, pName);
	if ( (pNode != NULL) && (pName != NULL) ) {
		pNode->bFlag = bAsync;
		xlAstSetText(pNode, pName->sText, pName->iTextLen);
		xlAstAppendChild(pNode, pParams);
		xlAstAppendChild(pNode, pRetType);
		xlAstAppendChild(pNode, pBody);
	}
	return pNode;
}

XL_AstNode* xlAstCreateParam(const XL_Token* pName, XL_AstNode* pType, XL_AstNode* pDefaultValue)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_PARAM, pName);
	if ( (pNode != NULL) && (pName != NULL) ) {
		xlAstSetText(pNode, pName->sText, pName->iTextLen);
		xlAstAppendChild(pNode, pType);
		xlAstAppendChild(pNode, pDefaultValue);
	}
	return pNode;
}

XL_AstNode* xlAstCreateConstDecl(const XL_Token* pName, XL_AstNode* pType, XL_AstNode* pValue)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_CONST_DECL, pName);
	if ( (pNode != NULL) && (pName != NULL) ) {
		xlAstSetText(pNode, pName->sText, pName->iTextLen);
		xlAstAppendChild(pNode, pType);
		xlAstAppendChild(pNode, pValue);
	}
	return pNode;
}

XL_AstNode* xlAstCreateVarDecl(const XL_Token* pName, XL_AstNode* pType, XL_AstNode* pValue)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_VAR_DECL, pName);
	if ( (pNode != NULL) && (pName != NULL) ) {
		xlAstSetText(pNode, pName->sText, pName->iTextLen);
		xlAstAppendChild(pNode, pType);
		xlAstAppendChild(pNode, pValue);
	}
	return pNode;
}

XL_AstNode* xlAstCreateIf(const XL_Token* pToken, XL_AstNode* pCond, XL_AstNode* pThenBlock, XL_AstNode* pElseIfList, XL_AstNode* pElseBlock)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_IF, pToken);
	if ( pNode != NULL ) {
		xlAstAppendChild(pNode, pCond);
		xlAstAppendChild(pNode, pThenBlock);
		xlAstAppendChild(pNode, pElseIfList);
		xlAstAppendChild(pNode, pElseBlock);
	}
	return pNode;
}

XL_AstNode* xlAstCreateElseIf(const XL_Token* pToken, XL_AstNode* pCond, XL_AstNode* pBody)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_ELSEIF, pToken);
	if ( pNode != NULL ) {
		xlAstAppendChild(pNode, pCond);
		xlAstAppendChild(pNode, pBody);
	}
	return pNode;
}

XL_AstNode* xlAstCreateForIn(const XL_Token* pToken, XL_AstNode* pTargets, XL_AstNode* pIterable, XL_AstNode* pBody)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_FORIN, pToken);
	if ( pNode != NULL ) {
		xlAstAppendChild(pNode, pTargets);
		xlAstAppendChild(pNode, pIterable);
		xlAstAppendChild(pNode, pBody);
	}
	return pNode;
}

XL_AstNode* xlAstCreateImport(const XL_Token* pToken, XL_AstNode* pTarget)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_IMPORT, pToken);
	if ( pNode != NULL ) {
		xlAstAppendChild(pNode, pTarget);
	}
	return pNode;
}

XL_AstNode* xlAstCreateExport(const XL_Token* pToken, XL_AstNode* pValue)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_EXPORT, pToken);
	if ( pNode != NULL ) {
		xlAstAppendChild(pNode, pValue);
	}
	return pNode;
}

XL_AstNode* xlAstCreateTry(const XL_Token* pToken, XL_AstNode* pTryBlock, XL_AstNode* pCatchName, XL_AstNode* pCatchBlock, XL_AstNode* pFinallyBlock)
{
	XL_AstNode* pNode;
	XL_AstNode* pSafeTryBlock;
	XL_AstNode* pSafeCatchName;
	XL_AstNode* pSafeCatchBlock;
	XL_AstNode* pSafeFinallyBlock;

	pNode = xlAstCreateNode(XL_AST_TRY, pToken);
	if ( pNode != NULL ) {
		pNode->bFlag = 0;
		if ( pCatchBlock != NULL ) {
			pNode->bFlag |= 1;
		}
		if ( pFinallyBlock != NULL ) {
			pNode->bFlag |= 2;
		}
		pSafeTryBlock = (pTryBlock != NULL) ? pTryBlock : xlAstCreateBlock(NULL);
		pSafeCatchName = (pCatchName != NULL) ? pCatchName : xlAstCreateIdent(NULL);
		pSafeCatchBlock = (pCatchBlock != NULL) ? pCatchBlock : xlAstCreateBlock(NULL);
		pSafeFinallyBlock = (pFinallyBlock != NULL) ? pFinallyBlock : xlAstCreateBlock(NULL);
		xlAstAppendChild(pNode, pSafeTryBlock);
		xlAstAppendChild(pNode, pSafeCatchName);
		xlAstAppendChild(pNode, pSafeCatchBlock);
		xlAstAppendChild(pNode, pSafeFinallyBlock);
	}
	return pNode;
}

XL_AstNode* xlAstCreateReturn(const XL_Token* pToken, XL_AstNode* pValue)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_RETURN, pToken);
	if ( pNode != NULL ) {
		xlAstAppendChild(pNode, pValue);
	}
	return pNode;
}

XL_AstNode* xlAstCreateBreak(const XL_Token* pToken, int iLevel)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_BREAK, pToken);
	if ( pNode != NULL ) {
		pNode->iIntValue = iLevel;
	}
	return pNode;
}

XL_AstNode* xlAstCreateContinue(const XL_Token* pToken, int iLevel)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_CONTINUE, pToken);
	if ( pNode != NULL ) {
		pNode->iIntValue = iLevel;
	}
	return pNode;
}

XL_AstNode* xlAstCreateThrow(const XL_Token* pToken, XL_AstNode* pValue)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_THROW, pToken);
	if ( pNode != NULL ) {
		xlAstAppendChild(pNode, pValue);
	}
	return pNode;
}

XL_AstNode* xlAstCreateAssert(const XL_Token* pToken, XL_AstNode* pValue, XL_AstNode* pMessage)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_ASSERT, pToken);
	if ( pNode != NULL ) {
		xlAstAppendChild(pNode, pValue);
		xlAstAppendChild(pNode, pMessage);
	}
	return pNode;
}

XL_AstNode* xlAstCreateYield(const XL_Token* pToken, XL_AstNode* pValue)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_YIELD, pToken);
	if ( pNode != NULL ) {
		xlAstAppendChild(pNode, pValue);
	}
	return pNode;
}

XL_AstNode* xlAstCreateExprStmt(XL_AstNode* pExpr)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_EXPR_STMT, NULL);
	if ( pNode != NULL ) {
		xlAstAppendChild(pNode, pExpr);
	}
	return pNode;
}

XL_AstNode* xlAstCreateNamedArg(const XL_Token* pName, XL_AstNode* pValue)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_NAMED_ARG, pName);
	if ( (pNode != NULL) && (pName != NULL) ) {
		xlAstSetText(pNode, pName->sText, pName->iTextLen);
		xlAstAppendChild(pNode, pValue);
	}
	return pNode;
}

XL_AstNode* xlAstCreateBinary(const XL_Token* pToken, const char* sOp, XL_AstNode* pLeft, XL_AstNode* pRight)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_BINARY, pToken);
	if ( pNode != NULL ) {
		xlAstSetText(pNode, sOp, strlen(sOp));
		xlAstAppendChild(pNode, pLeft);
		xlAstAppendChild(pNode, pRight);
	}
	return pNode;
}

XL_AstNode* xlAstCreateUnary(const XL_Token* pToken, const char* sOp, XL_AstNode* pValue)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_UNARY, pToken);
	if ( pNode != NULL ) {
		xlAstSetText(pNode, sOp, strlen(sOp));
		xlAstAppendChild(pNode, pValue);
	}
	return pNode;
}

XL_AstNode* xlAstCreateCall(XL_AstNode* pCallee, XL_AstNode* pArgs, const XL_Token* pToken)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_CALL, pToken);
	if ( pNode != NULL ) {
		xlAstAppendChild(pNode, pCallee);
		xlAstAppendChild(pNode, pArgs);
	}
	return pNode;
}

XL_AstNode* xlAstCreateMember(XL_AstNode* pBase, const XL_Token* pName, int bSafe, const XL_Token* pToken)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_MEMBER, pToken);
	if ( (pNode != NULL) && (pName != NULL) ) {
		pNode->bFlag = bSafe;
		xlAstSetText(pNode, pName->sText, pName->iTextLen);
		xlAstAppendChild(pNode, pBase);
	}
	return pNode;
}

XL_AstNode* xlAstCreateIndex(XL_AstNode* pBase, XL_AstNode* pIndex, int bSafe, const XL_Token* pToken)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_INDEX, pToken);
	if ( pNode != NULL ) {
		pNode->bFlag = bSafe;
		xlAstAppendChild(pNode, pBase);
		xlAstAppendChild(pNode, pIndex);
	}
	return pNode;
}

void xlAstSetText(XL_AstNode* pNode, const char* sText, size_t iLen)
{
	if ( pNode == NULL ) {
		return;
	}
	free(pNode->sText);
	pNode->sText = __xlAstDupText((sText != NULL) ? sText : "", iLen);
}

void xlAstSetText2(XL_AstNode* pNode, const char* sText, size_t iLen)
{
	if ( pNode == NULL ) {
		return;
	}
	free(pNode->sText2);
	pNode->sText2 = __xlAstDupText((sText != NULL) ? sText : "", iLen);
}

void xlAstAppendChild(XL_AstNode* pNode, XL_AstNode* pChild)
{
	XL_AstNode** ppNewItems;
	size_t iNewCap;

	if ( (pNode == NULL) || (pChild == NULL) ) {
		return;
	}

	if ( pNode->iChildCount >= pNode->iChildCapacity ) {
		iNewCap = (pNode->iChildCapacity == 0) ? 4 : (pNode->iChildCapacity * 2);
		ppNewItems = (XL_AstNode**)realloc(pNode->ppChildren, sizeof(XL_AstNode*) * iNewCap);
		if ( ppNewItems == NULL ) {
			return;
		}
		pNode->ppChildren = ppNewItems;
		pNode->iChildCapacity = iNewCap;
	}

	pNode->ppChildren[pNode->iChildCount++] = pChild;
}

void xlAstAppendChildren(XL_AstNode* pNode, XL_AstNode* pList)
{
	size_t i;

	if ( (pNode == NULL) || (pList == NULL) ) {
		return;
	}

	for ( i = 0; i < pList->iChildCount; ++i ) {
		xlAstAppendChild(pNode, pList->ppChildren[i]);
		pList->ppChildren[i] = NULL;
	}
	xlAstFree(pList);
}

void xlAstFree(XL_AstNode* pNode)
{
	size_t i;

	if ( pNode == NULL ) {
		return;
	}

	for ( i = 0; i < pNode->iChildCount; ++i ) {
		xlAstFree(pNode->ppChildren[i]);
	}
	free(pNode->ppChildren);
	free(pNode->sText);
	free(pNode->sText2);
	free(pNode);
}
