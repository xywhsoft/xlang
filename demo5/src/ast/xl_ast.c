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

static void __xlAstSetNormalizedTemplateText(XL_AstNode* pNode, const XL_Token* pToken)
{
	const char* sSource;
	char* sBuffer;
	size_t iSourceLen;
	size_t i;
	size_t iOut;

	if ( (pNode == NULL) || (pToken == NULL) || (pToken->sText == NULL) ) {
		return;
	}

	sSource = pToken->sText;
	iSourceLen = pToken->iTextLen;
	if ( (iSourceLen >= 2) && (sSource[0] == '`') && (sSource[iSourceLen - 1] == '`') ) {
		sSource += 1;
		iSourceLen -= 2;
	}

	sBuffer = (char*)malloc((iSourceLen * 2) + 3);
	if ( sBuffer == NULL ) {
		xlAstSetText(pNode, pToken->sText, pToken->iTextLen);
		return;
	}

	iOut = 0;
	sBuffer[iOut++] = '"';
	for ( i = 0; i < iSourceLen; ++i ) {
		char ch;

		ch = sSource[i];
		if ( ch == '"' ) {
			sBuffer[iOut++] = '\\';
			sBuffer[iOut++] = '"';
			continue;
		}
		if ( ch == '\\' ) {
			sBuffer[iOut++] = '\\';
			if ( (i + 1) < iSourceLen ) {
				sBuffer[iOut++] = sSource[++i];
			}
			continue;
		}
		sBuffer[iOut++] = ch;
	}
	sBuffer[iOut++] = '"';
	sBuffer[iOut] = '\0';

	xlAstSetText(pNode, sBuffer, iOut);
	free(sBuffer);
}

static void __xlAstSetNormalizedTypeText(XL_AstNode* pNode, const XL_Token* pToken)
{
	if ( (pNode == NULL) || (pToken == NULL) ) {
		return;
	}

	/* Keep source compatibility, but collapse legacy `bytes` onto the single runtime text type. */
	if ( (pToken->iTextLen == 5) && (memcmp(pToken->sText, "bytes", 5) == 0) ) {
		xlAstSetText(pNode, "string", 6);
		return;
	}

	xlAstSetText(pNode, pToken->sText, pToken->iTextLen);
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

static void __xlAstAdoptNodeRange(XL_AstNode* pNode, const XL_AstNode* pSource)
{
	if ( (pNode == NULL) || (pSource == NULL) ) {
		return;
	}

	pNode->iLine = pSource->iLine;
	pNode->iColumn = pSource->iColumn;
	pNode->iEndLine = pSource->iEndLine;
	pNode->iEndColumn = pSource->iEndColumn;
}

const char* xlAstKindName(XL_AstKind eKind)
{
	static const char* arrNames[] = {
		"List", "Program", "Block", "FuncDecl", "FuncExpr", "ClassDecl", "UnionDecl", "CreateDecl", "DestroyDecl", "Param", "TypeRef", "TypeDef", "ConstDecl", "VarDecl", "UnpackDecl",
		"If", "ElseIf", "While", "ForIn", "Import", "Export", "Try", "Return", "Break", "Continue", "Throw", "Assert", "Yield",
		"ExprStmt", "NamedArg", "Assign", "Ternary", "Binary", "Unary", "Call", "Member", "Index", "Ident", "Int", "Float",
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
		__xlAstSetNormalizedTypeText(pNode, pToken);
	}
	return pNode;
}

XL_AstNode* xlAstCreateTypeRefText(const char* sText)
{
	XL_AstNode* pNode;

	if ( sText == NULL ) {
		return NULL;
	}

	pNode = xlAstCreateNode(XL_AST_TYPE_REF, NULL);
	if ( pNode != NULL ) {
		xlAstSetText(pNode, sText, strlen(sText));
	}
	return pNode;
}

XL_AstNode* xlAstCreateTypeDef(const XL_Token* pName, XL_AstNode* pType)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_TYPEDEF, pName);
	if ( (pNode != NULL) && (pName != NULL) ) {
		xlAstSetText(pNode, pName->sText, pName->iTextLen);
		xlAstAppendChild(pNode, pType);
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
				__xlAstSetNormalizedTemplateText(pNode, pToken);
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

XL_AstNode* xlAstCreateFuncDecl(const XL_Token* pName, int bAsync, XL_AstNode* pParams, XL_AstNode* pRetTypes, XL_AstNode* pBody)
{
	XL_AstNode* pNode;
	XL_AstNode* pSafeParams;
	XL_AstNode* pSafeRetTypes;
	XL_AstNode* pSafeBody;

	pNode = xlAstCreateNode(XL_AST_FUNC_DECL, pName);
	if ( (pNode != NULL) && (pName != NULL) ) {
		pNode->bFlag = bAsync;
		xlAstSetText(pNode, pName->sText, pName->iTextLen);
		pSafeParams = (pParams != NULL) ? pParams : xlAstCreateList();
		pSafeRetTypes = (pRetTypes != NULL) ? pRetTypes : xlAstCreateList();
		pSafeBody = (pBody != NULL) ? pBody : xlAstCreateBlock(NULL);
		xlAstAppendChild(pNode, pSafeParams);
		xlAstAppendChild(pNode, pSafeRetTypes);
		xlAstAppendChild(pNode, pSafeBody);
	}
	return pNode;
}

XL_AstNode* xlAstCreateFuncExpr(const XL_Token* pToken, int bAsync, XL_AstNode* pParams, XL_AstNode* pRetTypes, XL_AstNode* pBody)
{
	XL_AstNode* pNode;
	XL_AstNode* pSafeParams;
	XL_AstNode* pSafeRetTypes;
	XL_AstNode* pSafeBody;

	pNode = xlAstCreateNode(XL_AST_FUNC_EXPR, pToken);
	if ( pNode != NULL ) {
		pNode->bFlag = bAsync;
		pSafeParams = (pParams != NULL) ? pParams : xlAstCreateList();
		pSafeRetTypes = (pRetTypes != NULL) ? pRetTypes : xlAstCreateList();
		pSafeBody = (pBody != NULL) ? pBody : xlAstCreateBlock(NULL);
		xlAstAppendChild(pNode, pSafeParams);
		xlAstAppendChild(pNode, pSafeRetTypes);
		xlAstAppendChild(pNode, pSafeBody);
	}
	return pNode;
}

XL_AstNode* xlAstCreateClassDecl(const XL_Token* pName, XL_AstNode* pBaseType, XL_AstNode* pMembers)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_CLASS_DECL, pName);
	if ( (pNode != NULL) && (pName != NULL) ) {
		xlAstSetText(pNode, pName->sText, pName->iTextLen);
		xlAstAppendChild(pNode, pBaseType);
		xlAstAppendChild(pNode, (pMembers != NULL) ? pMembers : xlAstCreateList());
	}
	return pNode;
}

XL_AstNode* xlAstCreateUnionDecl(const XL_Token* pName, XL_AstNode* pMembers)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_UNION_DECL, pName);
	if ( (pNode != NULL) && (pName != NULL) ) {
		xlAstSetText(pNode, pName->sText, pName->iTextLen);
		xlAstAppendChild(pNode, (pMembers != NULL) ? pMembers : xlAstCreateList());
	}
	return pNode;
}

XL_AstNode* xlAstCreateCreateDecl(const XL_Token* pToken, XL_AstNode* pParams, XL_AstNode* pBody)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_CREATE_DECL, pToken);
	if ( pNode != NULL ) {
		xlAstSetText(pNode, "create", 6);
		xlAstAppendChild(pNode, (pParams != NULL) ? pParams : xlAstCreateList());
		xlAstAppendChild(pNode, xlAstCreateList());
		xlAstAppendChild(pNode, (pBody != NULL) ? pBody : xlAstCreateBlock(NULL));
	}
	return pNode;
}

XL_AstNode* xlAstCreateDestroyDecl(const XL_Token* pToken, XL_AstNode* pBody)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_DESTROY_DECL, pToken);
	if ( pNode != NULL ) {
		xlAstSetText(pNode, "destroy", 7);
		xlAstAppendChild(pNode, xlAstCreateList());
		xlAstAppendChild(pNode, xlAstCreateList());
		xlAstAppendChild(pNode, (pBody != NULL) ? pBody : xlAstCreateBlock(NULL));
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

XL_AstNode* xlAstCreateUnpackDecl(const XL_Token* pTypeToken, XL_AstNode* pTargets, XL_AstNode* pValue)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_UNPACK_DECL, pTypeToken);
	if ( pNode != NULL ) {
		if ( pTypeToken != NULL ) {
			xlAstSetText(pNode, pTypeToken->sText, pTypeToken->iTextLen);
		}
		xlAstAppendChild(pNode, (pTargets != NULL) ? pTargets : xlAstCreateList());
		xlAstAppendChild(pNode, pValue);
	}
	return pNode;
}

XL_AstNode* xlAstCreateWhile(const XL_Token* pToken, XL_AstNode* pCond, XL_AstNode* pBody)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_WHILE, pToken);
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

XL_AstNode* xlAstCreateImport(const XL_Token* pToken, XL_AstNode* pTarget, const XL_Token* pAlias)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_IMPORT, pToken);
	if ( pNode != NULL ) {
		if ( (pToken == NULL) && (pTarget != NULL) ) {
			__xlAstAdoptNodeRange(pNode, pTarget);
		}
		xlAstAppendChild(pNode, pTarget);
		if ( (pAlias != NULL) && (pAlias->sText != NULL) && (pAlias->iTextLen > 0) ) {
			xlAstSetText2(pNode, pAlias->sText, pAlias->iTextLen);
		}
	}
	return pNode;
}

XL_AstNode* xlAstCreateIdentText(const char* sText)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_IDENT, NULL);
	if ( pNode != NULL ) {
		xlAstSetText(pNode, (sText != NULL) ? sText : "", (sText != NULL) ? strlen(sText) : 0);
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

XL_AstNode* xlAstCreateReturn(const XL_Token* pToken, XL_AstNode* pValues)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_RETURN, pToken);
	if ( pNode != NULL ) {
		xlAstAppendChild(pNode, (pValues != NULL) ? pValues : xlAstCreateList());
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

XL_AstNode* xlAstCreateAssign(const XL_Token* pToken, XL_AstNode* pLeft, XL_AstNode* pRight)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_ASSIGN, pToken);
	if ( pNode != NULL ) {
		if ( (pToken != NULL) && (pToken->sText != NULL) && (pToken->iTextLen > 0) ) {
			xlAstSetText(pNode, pToken->sText, pToken->iTextLen);
		} else {
			xlAstSetText(pNode, "=", 1);
		}
		xlAstAppendChild(pNode, pLeft);
		xlAstAppendChild(pNode, pRight);
	}
	return pNode;
}

XL_AstNode* xlAstCreateTernary(const XL_Token* pToken, XL_AstNode* pCond, XL_AstNode* pThenValue, XL_AstNode* pElseValue)
{
	XL_AstNode* pNode;

	pNode = xlAstCreateNode(XL_AST_TERNARY, pToken);
	if ( pNode != NULL ) {
		xlAstSetText(pNode, "?:", 2);
		xlAstAppendChild(pNode, pCond);
		xlAstAppendChild(pNode, pThenValue);
		xlAstAppendChild(pNode, pElseValue);
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
