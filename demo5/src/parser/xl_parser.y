%name xlParse
%token_prefix XLPG_
%token_type { XL_Token }
%default_type { XL_AstNode* }
%extra_argument { XL_ParseContext* pCtx }
%fallback TOK_IDENT KW_VAR.

%include {
#include "src/ast/xl_ast.h"
#include "src/diag/xl_diag.h"

typedef struct XL_ParseContext {
	XL_AstNode* pRoot;
	XL_DiagContext* pDiag;
	const char* sSourceFile;
	int bHadError;
	int iErrorCount;
} XL_ParseContext;

static XL_AstNode* xlPList(void)
{
	return xlAstCreateList();
}

static XL_AstNode* xlPCreateVariadicParam(const XL_Token* pToken, int iKind)
{
	XL_AstNode* pParam;
	const char* sTypeName;

	sTypeName = (iKind == XL_PARAM_VARIADIC_DICT) ? "dict<var>" : "array<var>";
	pParam = xlAstCreateParam(pToken, xlAstCreateTypeRefText(sTypeName), NULL);
	if ( pParam != NULL ) {
		xlAstSetText(pParam, "arg", 3);
		pParam->bFlag = iKind;
	}
	return pParam;
}

static void xlPAppendStmt(XL_AstNode* pList, XL_AstNode* pStmt);

static XL_AstNode* xlPCreateDefaultValueForType(const XL_AstNode* pTypeNode)
{
	XL_AstNode* pNode;
	const char* sTypeText;
	size_t iTypeLen;

	if ( (pTypeNode == NULL) || (pTypeNode->sText == NULL) ) {
		return NULL;
	}
	sTypeText = pTypeNode->sText;
	iTypeLen = strlen(sTypeText);

	if ( (iTypeLen == 3) && (memcmp(sTypeText, "int", 3) == 0) ) {
		pNode = xlAstCreateNode(XL_AST_INT, NULL);
		if ( pNode != NULL ) {
			pNode->iIntValue = 0;
			xlAstSetText(pNode, "0", 1);
			pNode->iLine = pTypeNode->iLine;
			pNode->iColumn = pTypeNode->iColumn;
		}
		return pNode;
	}

	if ( (iTypeLen == 5) && (memcmp(sTypeText, "float", 5) == 0) ) {
		pNode = xlAstCreateNode(XL_AST_FLOAT, NULL);
		if ( pNode != NULL ) {
			pNode->fFloatValue = 0.0;
			xlAstSetText(pNode, "0.0", 3);
			pNode->iLine = pTypeNode->iLine;
			pNode->iColumn = pTypeNode->iColumn;
		}
		return pNode;
	}

	if ( (iTypeLen == 4) && (memcmp(sTypeText, "bool", 4) == 0) ) {
		pNode = xlAstCreateNode(XL_AST_BOOL, NULL);
		if ( pNode != NULL ) {
			pNode->bFlag = 0;
			xlAstSetText(pNode, "false", 5);
			pNode->iLine = pTypeNode->iLine;
			pNode->iColumn = pTypeNode->iColumn;
		}
		return pNode;
	}

	if ( (iTypeLen == 3) && (memcmp(sTypeText, "var", 3) == 0) ) {
		pNode = xlAstCreateNode(XL_AST_NULL, NULL);
		if ( pNode != NULL ) {
			xlAstSetText(pNode, "null", 4);
			pNode->iLine = pTypeNode->iLine;
			pNode->iColumn = pTypeNode->iColumn;
		}
		return pNode;
	}

	return NULL;
}

static int xlPTokenEquals(const XL_Token* pToken, const char* sText)
{
	size_t iLen;

	if ( (pToken == NULL) || (pToken->sText == NULL) || (sText == NULL) ) {
		return 0;
	}

	iLen = strlen(sText);
	return (pToken->iTextLen == iLen) && (memcmp(pToken->sText, sText, iLen) == 0);
}

static int xlPNodeTextEquals(const XL_AstNode* pNode, const char* sText)
{
	size_t iLen;

	if ( (pNode == NULL) || (pNode->sText == NULL) || (sText == NULL) ) {
		return 0;
	}

	iLen = strlen(sText);
	return (strlen(pNode->sText) == iLen) && (memcmp(pNode->sText, sText, iLen) == 0);
}

static XL_AstNode* xlPCreateTypedVarDecl(XL_ParseContext* pCtx, XL_AstNode* pTypeNode, const XL_Token* pNameToken, XL_AstNode* pValue)
{
	XL_AstNode* pDecl;

	if ( (pTypeNode == NULL) || (pNameToken == NULL) ) {
		xlAstFree(pTypeNode);
		xlAstFree(pValue);
		return NULL;
	}

	if ( xlPNodeTextEquals(pTypeNode, "auto") ) {
		if ( pValue == NULL ) {
			if ( pCtx != NULL ) {
				pCtx->bHadError = 1;
				pCtx->iErrorCount += 1;
				if ( pCtx->pDiag != NULL ) {
					xlDiagEmitSource(
						pCtx->pDiag,
						XL_DIAG_LEVEL_ERROR,
						"parser",
						__FILE__,
						__LINE__,
						pCtx->sSourceFile,
						pTypeNode->iLine,
						pTypeNode->iColumn,
						"'auto' requires an initializer"
					);
				}
			}
			xlAstFree(pTypeNode);
			return NULL;
		}

		pDecl = xlAstCreateVarDecl(pNameToken, pTypeNode, pValue);
		if ( pDecl == NULL ) {
			xlAstFree(pTypeNode);
			xlAstFree(pValue);
		}
		return pDecl;
	}

	if ( pValue == NULL ) {
		if ( pCtx != NULL ) {
			pCtx->bHadError = 1;
			pCtx->iErrorCount += 1;
			if ( pCtx->pDiag != NULL ) {
				xlDiagEmitSource(
					pCtx->pDiag,
						XL_DIAG_LEVEL_ERROR,
						"parser",
						__FILE__,
						__LINE__,
						pCtx->sSourceFile,
						pTypeNode->iLine,
						pTypeNode->iColumn,
						"type %.*s requires an initializer",
						(int)strlen(pTypeNode->sText),
						pTypeNode->sText
					);
				}
			}
		xlAstFree(pTypeNode);
		return NULL;
	}

	pDecl = xlAstCreateVarDecl(pNameToken, pTypeNode, pValue);
	if ( pDecl == NULL ) {
		xlAstFree(pTypeNode);
		xlAstFree(pValue);
	}
	return pDecl;
}

static XL_AstNode* xlPCreateTypedVarDeclFromToken(XL_ParseContext* pCtx, const XL_Token* pTypeToken, const XL_Token* pNameToken, int bWithDefaultValue, XL_AstNode* pValue)
{
	XL_AstNode* pTypeNode;

	pTypeNode = xlAstCreateTypeRef(pTypeToken);
	if ( pTypeNode == NULL ) {
		xlAstFree(pValue);
		return NULL;
	}
	if ( bWithDefaultValue ) {
		pValue = xlPCreateDefaultValueForType(pTypeNode);
	}
	return xlPCreateTypedVarDecl(pCtx, pTypeNode, pNameToken, pValue);
}

static XL_AstNode* xlPCreateTypedUnpackDecl(XL_ParseContext* pCtx, XL_AstNode* pTypeNode, XL_AstNode* pTargets, XL_AstNode* pValue)
{
	XL_AstNode* pDecl;

	if ( pTypeNode == NULL ) {
		xlAstFree(pTargets);
		xlAstFree(pValue);
		return NULL;
	}

	if ( !xlPNodeTextEquals(pTypeNode, "auto") ) {
		if ( pCtx != NULL ) {
			pCtx->bHadError = 1;
			pCtx->iErrorCount += 1;
			if ( pCtx->pDiag != NULL ) {
				xlDiagEmitSource(
					pCtx->pDiag,
						XL_DIAG_LEVEL_ERROR,
						"parser",
						__FILE__,
						__LINE__,
						pCtx->sSourceFile,
						pTypeNode->iLine,
						pTypeNode->iColumn,
						"destructuring declarations currently require 'auto'"
					);
				}
			}
		xlAstFree(pTypeNode);
		xlAstFree(pTargets);
		xlAstFree(pValue);
		return NULL;
	}

	if ( pValue == NULL ) {
		if ( pCtx != NULL ) {
			pCtx->bHadError = 1;
			pCtx->iErrorCount += 1;
			if ( pCtx->pDiag != NULL ) {
				xlDiagEmitSource(
					pCtx->pDiag,
						XL_DIAG_LEVEL_ERROR,
						"parser",
						__FILE__,
						__LINE__,
						pCtx->sSourceFile,
						pTypeNode->iLine,
						pTypeNode->iColumn,
						"'auto' requires an initializer"
					);
				}
			}
		xlAstFree(pTypeNode);
		xlAstFree(pTargets);
		return NULL;
	}

	pDecl = xlAstCreateUnpackDecl(NULL, pTargets, pValue);
	if ( pDecl != NULL ) {
		xlAstSetText(pDecl, pTypeNode->sText, strlen(pTypeNode->sText));
		pDecl->iLine = pTypeNode->iLine;
		pDecl->iColumn = pTypeNode->iColumn;
		pDecl->iEndLine = pTypeNode->iEndLine;
		pDecl->iEndColumn = pTypeNode->iEndColumn;
	}
	xlAstFree(pTypeNode);
	return pDecl;
}

static XL_AstNode* xlPCreateTypedUnpackDeclFromToken(XL_ParseContext* pCtx, const XL_Token* pTypeToken, XL_AstNode* pTargets, XL_AstNode* pValue)
{
	XL_AstNode* pTypeNode;

	pTypeNode = xlAstCreateTypeRef(pTypeToken);
	if ( pTypeNode == NULL ) {
		xlAstFree(pTargets);
		xlAstFree(pValue);
		return NULL;
	}
	return xlPCreateTypedUnpackDecl(pCtx, pTypeNode, pTargets, pValue);
}

static XL_AstNode* xlPCreateExportedDeclList(const XL_Token* pExportToken, XL_AstNode* pDecl, const XL_Token* pNameToken)
{
	XL_AstNode* pList;
	XL_AstNode* pExportValue;
	XL_AstNode* pExportStmt;

	pList = xlPList();
	if ( pList == NULL ) {
		xlAstFree(pDecl);
		return NULL;
	}

	pExportValue = xlAstCreateIdent(pNameToken);
	pExportStmt = xlAstCreateExport(pExportToken, pExportValue);
	if ( pExportStmt == NULL ) {
		xlAstFree(pExportValue);
		xlAstFree(pDecl);
		xlAstFree(pList);
		return NULL;
	}

	xlPAppendStmt(pList, pDecl);
	xlPAppendStmt(pList, pExportStmt);
	return pList;
}

static XL_AstNode* xlPCreateExportedDeclListFromDecl(const XL_Token* pExportToken, XL_AstNode* pDecl)
{
	XL_AstNode* pList;
	XL_AstNode* pExportValue;
	XL_AstNode* pExportStmt;

	if ( (pDecl == NULL) || (pDecl->sText == NULL) || (pDecl->sText[0] == '\0') ) {
		xlAstFree(pDecl);
		return NULL;
	}

	pList = xlPList();
	if ( pList == NULL ) {
		xlAstFree(pDecl);
		return NULL;
	}

	pExportValue = xlAstCreateIdentText(pDecl->sText);
	pExportStmt = xlAstCreateExport(pExportToken, pExportValue);
	if ( pExportStmt == NULL ) {
		xlAstFree(pExportValue);
		xlAstFree(pDecl);
		xlAstFree(pList);
		return NULL;
	}

	xlPAppendStmt(pList, pDecl);
	xlPAppendStmt(pList, pExportStmt);
	return pList;
}

static XL_AstNode* xlPCreateBlockFromStmt(XL_AstNode* pStmt)
{
	XL_AstNode* pBlock;

	if ( pStmt == NULL ) {
		return NULL;
	}

	pBlock = xlAstCreateBlock(NULL);
	if ( pBlock == NULL ) {
		xlAstFree(pStmt);
		return NULL;
	}

	xlPAppendStmt(pBlock, pStmt);
	return pBlock;
}

static XL_AstNode* xlPCreateMethodFuncDecl(
	XL_AstNode* pReceiverType,
	const XL_Token* pNameToken,
	int bAsync,
	XL_AstNode* pParams,
	XL_AstNode* pRetTypes,
	XL_AstNode* pBody
)
{
	XL_AstNode* pDecl;

	if ( (pReceiverType == NULL) || (pNameToken == NULL) ) {
		xlAstFree(pReceiverType);
		xlAstFree(pParams);
		xlAstFree(pRetTypes);
		xlAstFree(pBody);
		return NULL;
	}

	pDecl = xlAstCreateFuncDecl(pNameToken, bAsync, pParams, pRetTypes, pBody);
	if ( pDecl == NULL ) {
		xlAstFree(pReceiverType);
		return NULL;
	}

	xlAstSetText2(pDecl, pNameToken->sText, pNameToken->iTextLen);
	xlAstAppendChild(pDecl, pReceiverType);
	return pDecl;
}

static void xlPAttachExternSymbol(XL_AstNode* pDecl, const XL_Token* pNameToken, const XL_Token* pSymbolToken)
{
	const char* sText;
	size_t iLen;

	if ( pDecl == NULL ) {
		return;
	}

	if ( (pSymbolToken == NULL) || (pSymbolToken->sText == NULL) ) {
		if ( (pNameToken != NULL) && (pNameToken->sText != NULL) ) {
			xlAstSetText2(pDecl, pNameToken->sText, pNameToken->iTextLen);
		}
		return;
	}

	sText = pSymbolToken->sText;
	iLen = pSymbolToken->iTextLen;
	if ( (iLen >= 2) && ((sText[0] == '"') || (sText[0] == '\'')) ) {
		sText += 1;
		iLen -= 2;
	}
	xlAstSetText2(pDecl, sText, iLen);
}

static void xlPAppendTypeText(char** psBuf, size_t* piLen, size_t* piCap, const char* sText)
{
	size_t iNeed;
	char* sNewBuf;

	if ( (psBuf == NULL) || (piLen == NULL) || (piCap == NULL) || (sText == NULL) ) {
		return;
	}

	iNeed = strlen(sText);
	if ( (*piLen + iNeed + 1) > *piCap ) {
		size_t iNewCap = (*piCap == 0) ? 64 : *piCap;

		while ( iNewCap < (*piLen + iNeed + 1) ) {
			iNewCap *= 2;
		}
		sNewBuf = (char*)realloc(*psBuf, iNewCap);
		if ( sNewBuf == NULL ) {
			return;
		}
		*psBuf = sNewBuf;
		*piCap = iNewCap;
	}

	memcpy((*psBuf) + (*piLen), sText, iNeed);
	*piLen += iNeed;
	(*psBuf)[*piLen] = '\0';
}

static XL_AstNode* xlPCreateGenericTypeRef(XL_AstNode* pBaseType, XL_AstNode* pTypeArgs)
{
	XL_AstNode* pNode;
	XL_AstNode* pLastArg;
	char* sBuf;
	size_t iLen;
	size_t iCap;
	size_t i;

	if ( (pBaseType == NULL) || (pTypeArgs == NULL) || (pBaseType->sText == NULL) || (pTypeArgs->iChildCount == 0) ) {
		xlAstFree(pBaseType);
		xlAstFree(pTypeArgs);
		return NULL;
	}

	sBuf = NULL;
	iLen = 0;
	iCap = 0;
	xlPAppendTypeText(&sBuf, &iLen, &iCap, pBaseType->sText);
	xlPAppendTypeText(&sBuf, &iLen, &iCap, "<");
	for ( i = 0; i < pTypeArgs->iChildCount; ++i ) {
		if ( i != 0 ) {
			xlPAppendTypeText(&sBuf, &iLen, &iCap, ",");
		}
		xlPAppendTypeText(&sBuf, &iLen, &iCap, pTypeArgs->ppChildren[i]->sText);
	}
	xlPAppendTypeText(&sBuf, &iLen, &iCap, ">");

	pNode = xlAstCreateTypeRefText(sBuf);
	if ( pNode != NULL ) {
		pNode->iLine = pBaseType->iLine;
		pNode->iColumn = pBaseType->iColumn;
		pNode->iEndLine = pBaseType->iEndLine;
		pNode->iEndColumn = pBaseType->iEndColumn;
		pLastArg = pTypeArgs->ppChildren[pTypeArgs->iChildCount - 1];
		if ( pLastArg != NULL ) {
			pNode->iEndLine = pLastArg->iEndLine;
			pNode->iEndColumn = pLastArg->iEndColumn;
		}
	}

	free(sBuf);
	xlAstFree(pBaseType);
	xlAstFree(pTypeArgs);
	return pNode;
}

static XL_AstNode* xlPCreateFunctionTypeRef(const XL_Token* pNameToken, XL_AstNode* pReturnType, XL_AstNode* pParamTypes)
{
	XL_AstNode* pNode;
	char* sBuf;
	size_t iLen;
	size_t iCap;
	size_t i;

	if ( (pNameToken == NULL) || !xlPTokenEquals(pNameToken, "function") || (pReturnType == NULL) || (pParamTypes == NULL) ) {
		xlAstFree(pReturnType);
		xlAstFree(pParamTypes);
		return NULL;
	}

	sBuf = NULL;
	iLen = 0;
	iCap = 0;
	xlPAppendTypeText(&sBuf, &iLen, &iCap, "function<");
	xlPAppendTypeText(&sBuf, &iLen, &iCap, pReturnType->sText);
	xlPAppendTypeText(&sBuf, &iLen, &iCap, "(");
	for ( i = 0; i < pParamTypes->iChildCount; ++i ) {
		if ( i != 0 ) {
			xlPAppendTypeText(&sBuf, &iLen, &iCap, ",");
		}
		xlPAppendTypeText(&sBuf, &iLen, &iCap, pParamTypes->ppChildren[i]->sText);
	}
	xlPAppendTypeText(&sBuf, &iLen, &iCap, ")>");

	pNode = xlAstCreateNode(XL_AST_TYPE_REF, pNameToken);
	if ( (pNode != NULL) && (sBuf != NULL) ) {
		xlAstSetText(pNode, sBuf, iLen);
	}

	free(sBuf);
	xlAstFree(pReturnType);
	xlAstFree(pParamTypes);
	return pNode;
}

static XL_AstNode* xlPCreateHiddenCall(const char* sName, XL_AstNode* pArgs, const XL_Token* pToken)
{
	XL_AstNode* pCallee;
	XL_AstNode* pCall;

	pCallee = xlAstCreateIdentText(sName);
	if ( pCallee == NULL ) {
		xlAstFree(pArgs);
		return NULL;
	}

	if ( pArgs == NULL ) {
		pArgs = xlPList();
		if ( pArgs == NULL ) {
			xlAstFree(pCallee);
			return NULL;
		}
	}

	pCall = xlAstCreateCall(pCallee, pArgs, pToken);
	if ( pCall == NULL ) {
		xlAstFree(pCallee);
		xlAstFree(pArgs);
	}
	return pCall;
}

static XL_AstNode* xlPCreateDictLiteralItems(XL_AstNode* pKey, XL_AstNode* pValue)
{
	XL_AstNode* pItems;

	pItems = xlPList();
	if ( pItems == NULL ) {
		xlAstFree(pKey);
		xlAstFree(pValue);
		return NULL;
	}

	xlAstAppendChild(pItems, pKey);
	xlAstAppendChild(pItems, pValue);
	return pItems;
}

static XL_AstNode* xlPCreateBareBraceLiteral(XL_AstNode* pItems, int bIsDict, const XL_Token* pToken)
{
	if ( bIsDict ) {
		return xlPCreateHiddenCall("__dict_literal", pItems, pToken);
	}
	return xlPCreateHiddenCall("__set_literal", pItems, pToken);
}

static XL_AstNode* xlPCreateBareBracketLiteral(XL_AstNode* pItems, int bIsList, const XL_Token* pToken)
{
	if ( bIsList ) {
		return xlPCreateHiddenCall("__list_literal_indexed", pItems, pToken);
	}
	return xlPCreateHiddenCall("__array_literal", pItems, pToken);
}

static XL_AstNode* xlPCreateBracedValueLiteral(
	XL_ParseContext* pCtx,
	const XL_Token* pNameToken,
	XL_AstNode* pItems
)
{
	if ( (pNameToken == NULL) || (pNameToken->sText == NULL) ) {
		xlAstFree(pItems);
		return NULL;
	}

	if ( xlPTokenEquals(pNameToken, "list") ) {
		return xlPCreateHiddenCall("__list_literal", pItems, pNameToken);
	}
	if ( xlPTokenEquals(pNameToken, "set") ) {
		return xlPCreateHiddenCall("__set_literal", pItems, pNameToken);
	}
	if ( xlPTokenEquals(pNameToken, "dict") && (pItems != NULL) && (pItems->iChildCount == 0) ) {
		return xlPCreateHiddenCall("__dict_literal", pItems, pNameToken);
	}

	if ( pCtx != NULL ) {
		pCtx->bHadError = 1;
		pCtx->iErrorCount += 1;
		if ( pCtx->pDiag != NULL ) {
			xlDiagEmitSource(
				pCtx->pDiag,
				XL_DIAG_LEVEL_ERROR,
				"parser",
				__FILE__,
				__LINE__,
				pCtx->sSourceFile,
				pNameToken->iStartLine,
				pNameToken->iStartColumn,
				"'%.*s{...}' is not a supported literal form",
				(int)pNameToken->iTextLen,
				pNameToken->sText
			);
		}
	}

	xlAstFree(pItems);
	return NULL;
}

static XL_AstNode* xlPCreateBracedDictLiteral(
	XL_ParseContext* pCtx,
	const XL_Token* pNameToken,
	XL_AstNode* pItems
)
{
	if ( (pNameToken != NULL) && xlPTokenEquals(pNameToken, "dict") ) {
		return xlPCreateHiddenCall("__dict_literal", pItems, pNameToken);
	}

	if ( (pCtx != NULL) && (pNameToken != NULL) ) {
		pCtx->bHadError = 1;
		pCtx->iErrorCount += 1;
		if ( pCtx->pDiag != NULL ) {
			xlDiagEmitSource(
				pCtx->pDiag,
				XL_DIAG_LEVEL_ERROR,
				"parser",
				__FILE__,
				__LINE__,
				pCtx->sSourceFile,
				pNameToken->iStartLine,
				pNameToken->iStartColumn,
				"'%.*s{key: value}' is only supported for dict literals",
				(int)pNameToken->iTextLen,
				pNameToken->sText
			);
		}
	}

	xlAstFree(pItems);
	return NULL;
}

static void xlPAppendStmt(XL_AstNode* pList, XL_AstNode* pStmt)
{
	if ( (pList == NULL) || (pStmt == NULL) ) {
		return;
	}
	if ( pStmt->eKind == XL_AST_LIST ) {
		xlAstAppendChildren(pList, pStmt);
		return;
	}
	xlAstAppendChild(pList, pStmt);
}
}

%syntax_error {
	pCtx->bHadError = 1;
	pCtx->iErrorCount += 1;
	if ( pCtx->pDiag != NULL ) {
		xlDiagEmitSource(
			pCtx->pDiag,
			XL_DIAG_LEVEL_ERROR,
			"parser",
			__FILE__,
			__LINE__,
			pCtx->sSourceFile,
			TOKEN.iStartLine,
			TOKEN.iStartColumn,
			"syntax error near %s",
			xlTokenKindName(TOKEN.eKind)
		);
	}
}

%parse_failure {
	if ( (pCtx->pDiag != NULL) && (pCtx->iErrorCount == 0) ) {
		xlDiagEmit(pCtx->pDiag, XL_DIAG_LEVEL_ERROR, "parser", __FILE__, __LINE__, "parse failure");
	}
}

%stack_overflow {
	if ( pCtx->pDiag != NULL ) {
		xlDiagEmit(pCtx->pDiag, XL_DIAG_LEVEL_ERROR, "parser", __FILE__, __LINE__, "parser stack overflow");
	}
}

%type async_opt { int }
%type then_opt { int }
%type for_bindings { XL_AstNode* }
%type import_items { XL_AstNode* }
%type import_item { XL_AstNode* }
%type import_path { XL_AstNode* }
%type import_alias_opt { XL_Token }
%type type_name { XL_Token }
%type generic_type_ref { XL_AstNode* }
%type generic_type_args { XL_AstNode* }
%type func_decl_type { XL_AstNode* }
%type function_type_ref { XL_AstNode* }
%type func_type_param_list { XL_AstNode* }
%type func_type_params { XL_AstNode* }
%type export_func_decl { XL_AstNode* }
%type extern_decl { XL_AstNode* }
%type extern_symbol_opt { XL_Token }
%type typed_var_decl { XL_AstNode* }
%type typedef_decl { XL_AstNode* }
%type assign_stmt { XL_AstNode* }
%type while_stmt { XL_AstNode* }
%type do_while_stmt { XL_AstNode* }
%type do_while_block { XL_AstNode* }
%type do_while_stmt_items { XL_AstNode* }
%type do_while_stmt_item { XL_AstNode* }
%type inline_stmt { XL_AstNode* }
%type bare_brace_literal { XL_AstNode* }
%type bare_bracket_literal { XL_AstNode* }
%type dict_items { XL_AstNode* }
%type dict_item { XL_AstNode* }
%type indexed_items { XL_AstNode* }
%type indexed_item { XL_AstNode* }
%type yield_expr { XL_AstNode* }

%right OP_LT OP_GT OP_LE OP_GE OP_EQ OP_NE OP_APPROX KW_IN.
%left OP_SHL OP_SHR.
%left OP_PLUS OP_MINUS.
%left OP_STAR OP_SLASH OP_PERCENT.
%right OP_POWER.
%right KW_THEN.
%right KW_ELSE.
%right UMINUS KW_NOT OP_BANG KW_AWAIT.
%right TK_LBRACE.
%left OP_DOT OP_SAFE_DOT TK_LBRACKET OP_SAFE_LBRACKET TK_LPAREN.

program ::= opt_stmt_sep stmt_list(L). {
	XL_AstNode* pProgram = xlAstCreateProgram();
	xlAstAppendChildren(pProgram, L);
	pCtx->pRoot = pProgram;
}

stmt_list(A) ::= . {
	A = xlPList();
}
stmt_list(A) ::= stmt_items(L) opt_stmt_sep. {
	A = L;
}

stmt_items(A) ::= stmt(S). {
	A = xlPList();
	xlPAppendStmt(A, S);
}
stmt_items(A) ::= stmt_items(L) stmt_sep stmt(S). {
	A = L;
	xlPAppendStmt(A, S);
}

stmt_sep ::= STMT_END.
stmt_sep ::= stmt_sep STMT_END.
opt_stmt_sep ::= .
opt_stmt_sep ::= stmt_sep.

stmt(A) ::= error. {
	A = NULL;
}
stmt(A) ::= func_decl(F). { A = F; }
stmt(A) ::= extern_decl(E). { A = E; }
stmt(A) ::= typedef_decl(T). { A = T; }
stmt(A) ::= const_decl(C). { A = C; }
stmt(A) ::= var_decl(V). { A = V; }
stmt(A) ::= typed_var_decl(V). { A = V; }
stmt(A) ::= assign_stmt(S). { A = S; }
stmt(A) ::= if_stmt(I). { A = I; }
stmt(A) ::= while_stmt(W). { A = W; }
stmt(A) ::= do_while_stmt(W). { A = W; }
stmt(A) ::= try_stmt(T). { A = T; }
stmt(A) ::= for_in_stmt(F). { A = F; }
stmt(A) ::= import_stmt(I). { A = I; }
stmt(A) ::= export_stmt(E). { A = E; }
stmt(A) ::= KW_RETURN(T) return_value_list(V). { A = xlAstCreateReturn(&T, V); }
stmt(A) ::= KW_BREAK(T) INT(N). { A = xlAstCreateBreak(&T, (int)N.iIntValue); }
stmt(A) ::= KW_BREAK(T). { A = xlAstCreateBreak(&T, 1); }
stmt(A) ::= KW_CONTINUE(T) INT(N). { A = xlAstCreateContinue(&T, (int)N.iIntValue); }
stmt(A) ::= KW_CONTINUE(T). { A = xlAstCreateContinue(&T, 1); }
stmt(A) ::= KW_THROW(T) expr(E). { A = xlAstCreateThrow(&T, E); }
stmt(A) ::= KW_ASSERT(T) expr(E) assert_message(M). { A = xlAstCreateAssert(&T, E, M); }
stmt(A) ::= KW_YIELD(T) expr_opt(E). { A = xlAstCreateYield(&T, E); }
stmt(A) ::= expr(E). { A = xlAstCreateExprStmt(E); }

assert_message(A) ::= TK_COMMA expr(E). { A = E; }
assert_message(A) ::= . { A = NULL; }

async_opt(A) ::= KW_ASYNC. { A = 1; }
async_opt(A) ::= . { A = 0; }

func_decl(A) ::= async_opt(B) KW_FUNC TOK_IDENT(N) TK_LPAREN param_list(P) TK_RPAREN ret_type(R) opt_stmt_sep block(BODY) KW_END. {
	A = xlAstCreateFuncDecl(&N, B, P, R, BODY);
}
func_decl(A) ::= async_opt(B) KW_FUNC type_name(TY) OP_DOT TOK_IDENT(N) TK_LPAREN param_list(P) TK_RPAREN ret_type(R) opt_stmt_sep block(BODY) KW_END. {
	A = xlPCreateMethodFuncDecl(xlAstCreateTypeRef(&TY), &N, B, P, R, BODY);
}
func_decl(A) ::= async_opt(B) KW_FUNC generic_type_ref(TY) OP_DOT TOK_IDENT(N) TK_LPAREN param_list(P) TK_RPAREN ret_type(R) opt_stmt_sep block(BODY) KW_END. {
	A = xlPCreateMethodFuncDecl(TY, &N, B, P, R, BODY);
}
func_decl(A) ::= async_opt(B) KW_FUNC function_type_ref(TY) OP_DOT TOK_IDENT(N) TK_LPAREN param_list(P) TK_RPAREN ret_type(R) opt_stmt_sep block(BODY) KW_END. {
	A = xlPCreateMethodFuncDecl(TY, &N, B, P, R, BODY);
}

extern_symbol_opt(A) ::= STRING(S). { A = S; }
extern_symbol_opt(A) ::= . {
	memset(&A, 0, sizeof(A));
}

extern_decl(A) ::= KW_EXTERN(E) extern_symbol_opt(S) KW_FUNC TOK_IDENT(N) TK_LPAREN param_list(P) TK_RPAREN ret_type(R). {
	A = xlAstCreateFuncDecl(&N, 0, P, R, xlAstCreateBlock(&E));
	xlPAttachExternSymbol(A, &N, (S.eKind != 0) ? &S : NULL);
}

typedef_decl(A) ::= KW_TYPEDEF TOK_IDENT(N) OP_ASSIGN type_name(T). {
	A = xlAstCreateTypeDef(&N, xlAstCreateTypeRef(&T));
}
typedef_decl(A) ::= KW_TYPEDEF TOK_IDENT(N) OP_ASSIGN generic_type_ref(T). {
	A = xlAstCreateTypeDef(&N, T);
}
typedef_decl(A) ::= KW_TYPEDEF TOK_IDENT(N) OP_ASSIGN function_type_ref(T). {
	A = xlAstCreateTypeDef(&N, T);
}

param_list(A) ::= . {
	A = xlPList();
}
param_list(A) ::= params(P). {
	A = P;
}

params(A) ::= param(P). {
	A = xlPList();
	xlAstAppendChild(A, P);
}
params(A) ::= params(L) TK_COMMA param(P). {
	A = L;
	xlAstAppendChild(A, P);
}

param(A) ::= TOK_IDENT(N). {
	A = xlAstCreateParam(&N, NULL, NULL);
}
param(A) ::= type_name(T) TOK_IDENT(N). {
	A = xlAstCreateParam(&N, xlAstCreateTypeRef(&T), NULL);
}
param(A) ::= generic_type_ref(T) TOK_IDENT(N). {
	A = xlAstCreateParam(&N, T, NULL);
}
param(A) ::= function_type_ref(T) TOK_IDENT(N). {
	A = xlAstCreateParam(&N, T, NULL);
}
param(A) ::= TOK_IDENT(N) OP_ASSIGN expr(E). {
	A = xlAstCreateParam(&N, NULL, E);
}
param(A) ::= type_name(T) TOK_IDENT(N) OP_ASSIGN expr(E). {
	A = xlAstCreateParam(&N, xlAstCreateTypeRef(&T), E);
}
param(A) ::= generic_type_ref(T) TOK_IDENT(N) OP_ASSIGN expr(E). {
	A = xlAstCreateParam(&N, T, E);
}
param(A) ::= function_type_ref(T) TOK_IDENT(N) OP_ASSIGN expr(E). {
	A = xlAstCreateParam(&N, T, E);
}
param(A) ::= OP_ELLIPSIS(T). {
	A = xlPCreateVariadicParam(&T, XL_PARAM_VARIADIC_ARRAY);
}
param(A) ::= OP_POWER(T) OP_STAR. {
	A = xlPCreateVariadicParam(&T, XL_PARAM_VARIADIC_DICT);
}

ret_type(A) ::= . { A = xlPList(); }
ret_type(A) ::= KW_AS type_name(T). {
	A = xlPList();
	xlAstAppendChild(A, xlAstCreateTypeRef(&T));
}
ret_type(A) ::= KW_AS generic_type_ref(T). {
	A = xlPList();
	xlAstAppendChild(A, T);
}
ret_type(A) ::= KW_AS function_type_ref(T). {
	A = xlPList();
	xlAstAppendChild(A, T);
}
ret_type(A) ::= KW_AS type_name(T) ret_type_tail(L). {
	A = xlPList();
	xlAstAppendChild(A, xlAstCreateTypeRef(&T));
	xlAstAppendChildren(A, L);
}
ret_type(A) ::= KW_AS generic_type_ref(T) ret_type_tail(L). {
	A = xlPList();
	xlAstAppendChild(A, T);
	xlAstAppendChildren(A, L);
}
ret_type(A) ::= KW_AS function_type_ref(T) ret_type_tail(L). {
	A = xlPList();
	xlAstAppendChild(A, T);
	xlAstAppendChildren(A, L);
}

ret_type_tail(A) ::= TK_COMMA type_name(T). {
	A = xlPList();
	xlAstAppendChild(A, xlAstCreateTypeRef(&T));
}
ret_type_tail(A) ::= TK_COMMA generic_type_ref(T). {
	A = xlPList();
	xlAstAppendChild(A, T);
}
ret_type_tail(A) ::= TK_COMMA function_type_ref(T). {
	A = xlPList();
	xlAstAppendChild(A, T);
}
ret_type_tail(A) ::= ret_type_tail(L) TK_COMMA type_name(T). {
	A = L;
	xlAstAppendChild(A, xlAstCreateTypeRef(&T));
}
ret_type_tail(A) ::= ret_type_tail(L) TK_COMMA generic_type_ref(T). {
	A = L;
	xlAstAppendChild(A, T);
}
ret_type_tail(A) ::= ret_type_tail(L) TK_COMMA function_type_ref(T). {
	A = L;
	xlAstAppendChild(A, T);
}

type_name(A) ::= TOK_IDENT(T). [OP_LT] { A = T; }
type_name(A) ::= KW_VAR(T). [OP_LT] { A = T; }
type_name(A) ::= KW_TYPE(T). [OP_LT] { A = T; }

generic_type_ref(A) ::= type_name(T) OP_LT generic_type_args(L) OP_GT. {
	A = xlPCreateGenericTypeRef(xlAstCreateTypeRef(&T), L);
}

generic_type_args(A) ::= func_decl_type(T). {
	A = xlPList();
	xlAstAppendChild(A, T);
}
generic_type_args(A) ::= generic_type_args(L) TK_COMMA func_decl_type(T). {
	A = L;
	xlAstAppendChild(A, T);
}

func_decl_type(A) ::= type_name(T). {
	A = xlAstCreateTypeRef(&T);
}
func_decl_type(A) ::= generic_type_ref(T). {
	A = T;
}
func_decl_type(A) ::= function_type_ref(T). {
	A = T;
}

function_type_ref(A) ::= KW_FUNCTION(T) OP_LT func_decl_type(R) TK_LPAREN func_type_param_list(P) TK_RPAREN OP_GT. {
	A = xlPCreateFunctionTypeRef(&T, R, P);
}

func_type_param_list(A) ::= . {
	A = xlPList();
}
func_type_param_list(A) ::= func_type_params(P). {
	A = P;
}

func_type_params(A) ::= func_decl_type(T). {
	A = xlPList();
	xlAstAppendChild(A, T);
}
func_type_params(A) ::= func_type_params(L) TK_COMMA func_decl_type(T). {
	A = L;
	xlAstAppendChild(A, T);
}

const_decl(A) ::= KW_CONST TOK_IDENT(N) OP_ASSIGN expr(E). {
	A = xlAstCreateConstDecl(&N, NULL, E);
}
const_decl(A) ::= KW_CONST type_name(T) TOK_IDENT(N) OP_ASSIGN expr(E). {
	A = xlAstCreateConstDecl(&N, xlAstCreateTypeRef(&T), E);
}
const_decl(A) ::= KW_CONST generic_type_ref(T) TOK_IDENT(N) OP_ASSIGN expr(E). {
	A = xlAstCreateConstDecl(&N, T, E);
}
const_decl(A) ::= KW_CONST function_type_ref(T) TOK_IDENT(N) OP_ASSIGN expr(E). {
	A = xlAstCreateConstDecl(&N, T, E);
}

var_decl(A) ::= KW_VAR TOK_IDENT(N) OP_ASSIGN expr(E). {
	A = xlAstCreateVarDecl(&N, NULL, E);
}
var_decl(A) ::= KW_VAR type_name(T) TOK_IDENT(N) OP_ASSIGN expr(E). {
	A = xlAstCreateVarDecl(&N, xlAstCreateTypeRef(&T), E);
}
var_decl(A) ::= KW_VAR generic_type_ref(T) TOK_IDENT(N) OP_ASSIGN expr(E). {
	A = xlAstCreateVarDecl(&N, T, E);
}
var_decl(A) ::= KW_VAR function_type_ref(T) TOK_IDENT(N) OP_ASSIGN expr(E). {
	A = xlAstCreateVarDecl(&N, T, E);
}

typed_var_decl(A) ::= TOK_IDENT(T) TOK_IDENT(N). {
	A = xlPCreateTypedVarDeclFromToken(pCtx, &T, &N, 1, NULL);
}
typed_var_decl(A) ::= TOK_IDENT(T) TOK_IDENT(N) OP_ASSIGN expr(E). {
	A = xlPCreateTypedVarDeclFromToken(pCtx, &T, &N, 0, E);
}
typed_var_decl(A) ::= TOK_IDENT(T) unpack_target_list(L) OP_ASSIGN expr(E). {
	A = xlPCreateTypedUnpackDeclFromToken(pCtx, &T, L, E);
}
typed_var_decl(A) ::= generic_type_ref(T) TOK_IDENT(N) OP_ASSIGN expr(E). {
	A = xlPCreateTypedVarDecl(pCtx, T, &N, E);
}
typed_var_decl(A) ::= generic_type_ref(T) unpack_target_list(L) OP_ASSIGN expr(E). {
	A = xlPCreateTypedUnpackDecl(pCtx, T, L, E);
}
typed_var_decl(A) ::= function_type_ref(T) TOK_IDENT(N) OP_ASSIGN expr(E). {
	A = xlPCreateTypedVarDecl(pCtx, T, &N, E);
}
typed_var_decl(A) ::= function_type_ref(T) unpack_target_list(L) OP_ASSIGN expr(E). {
	A = xlPCreateTypedUnpackDecl(pCtx, T, L, E);
}

unpack_target_list(A) ::= TOK_IDENT(N) TK_COMMA TOK_IDENT(M). {
	A = xlPList();
	xlAstAppendChild(A, xlAstCreateIdent(&N));
	xlAstAppendChild(A, xlAstCreateIdent(&M));
}
unpack_target_list(A) ::= unpack_target_list(L) TK_COMMA TOK_IDENT(N). {
	A = L;
	xlAstAppendChild(A, xlAstCreateIdent(&N));
}

assign_stmt(A) ::= postfix_expr(L) OP_ASSIGN(T) expr(E). {
	A = xlAstCreateExprStmt(xlAstCreateAssign(&T, L, E));
}
assign_stmt(A) ::= postfix_expr(L) OP_PLUS_ASSIGN(T) expr(E). {
	A = xlAstCreateExprStmt(xlAstCreateAssign(&T, L, E));
}
assign_stmt(A) ::= postfix_expr(L) OP_MINUS_ASSIGN(T) expr(E). {
	A = xlAstCreateExprStmt(xlAstCreateAssign(&T, L, E));
}
assign_stmt(A) ::= postfix_expr(L) OP_STAR_ASSIGN(T) expr(E). {
	A = xlAstCreateExprStmt(xlAstCreateAssign(&T, L, E));
}
assign_stmt(A) ::= postfix_expr(L) OP_SLASH_ASSIGN(T) expr(E). {
	A = xlAstCreateExprStmt(xlAstCreateAssign(&T, L, E));
}
assign_stmt(A) ::= postfix_expr(L) OP_PERCENT_ASSIGN(T) expr(E). {
	A = xlAstCreateExprStmt(xlAstCreateAssign(&T, L, E));
}
assign_stmt(A) ::= postfix_expr(L) OP_POWER_ASSIGN(T) expr(E). {
	A = xlAstCreateExprStmt(xlAstCreateAssign(&T, L, E));
}
assign_stmt(A) ::= postfix_expr(L) OP_NULL_ASSIGN(T) expr(E). {
	A = xlAstCreateExprStmt(xlAstCreateAssign(&T, L, E));
}

if_stmt(A) ::= KW_IF(T) expr(C) stmt_sep block(B) elseif_list(EI) else_opt(EL) KW_END. {
	A = xlAstCreateIf(&T, C, B, EI, EL);
}
if_stmt(A) ::= KW_IF(T) expr(C) KW_THEN stmt_sep block(B) elseif_list(EI) else_opt(EL) KW_END. {
	A = xlAstCreateIf(&T, C, B, EI, EL);
}
if_stmt(A) ::= KW_IF(T) expr(C) KW_THEN inline_stmt(S) KW_ELSE inline_stmt(EL). {
	A = xlAstCreateIf(&T, C, xlPCreateBlockFromStmt(S), xlPList(), xlPCreateBlockFromStmt(EL));
}
if_stmt(A) ::= KW_IF(T) expr(C) KW_THEN inline_stmt(S). [KW_THEN] {
	A = xlAstCreateIf(&T, C, xlPCreateBlockFromStmt(S), xlPList(), NULL);
}

while_stmt(A) ::= KW_WHILE(T) expr(C) KW_DO opt_stmt_sep block(B) KW_END. {
	A = xlAstCreateWhile(&T, C, B);
}
do_while_stmt(A) ::= KW_DO(T) opt_stmt_sep do_while_block(B) KW_WHILE expr(C) opt_stmt_sep KW_END. {
	A = xlAstCreateWhile(&T, C, B);
	if ( A != NULL ) {
		A->bFlag = 1;
	}
}

try_stmt(A) ::= KW_TRY(T) opt_stmt_sep block(B) KW_CATCH TOK_IDENT(N) opt_stmt_sep block(C) KW_END. {
	A = xlAstCreateTry(&T, B, xlAstCreateIdent(&N), C, NULL);
}
try_stmt(A) ::= KW_TRY(T) opt_stmt_sep block(B) KW_CATCH TOK_IDENT(N) opt_stmt_sep block(C) KW_FINALLY opt_stmt_sep block(F) KW_END. {
	A = xlAstCreateTry(&T, B, xlAstCreateIdent(&N), C, F);
}
try_stmt(A) ::= KW_TRY(T) opt_stmt_sep block(B) KW_FINALLY opt_stmt_sep block(F) KW_END. {
	A = xlAstCreateTry(&T, B, NULL, NULL, F);
}

for_in_stmt(A) ::= KW_FOR(T) for_bindings(BS) KW_IN expr(E) opt_stmt_sep block(B) KW_END. {
	A = xlAstCreateForIn(&T, BS, E, B);
}

for_bindings(A) ::= TOK_IDENT(N). {
	A = xlPList();
	xlAstAppendChild(A, xlAstCreateIdent(&N));
}
for_bindings(A) ::= for_bindings(L) TK_COMMA TOK_IDENT(N). {
	A = L;
	xlAstAppendChild(A, xlAstCreateIdent(&N));
}

then_opt(A) ::= KW_THEN. { A = 1; }
then_opt(A) ::= . { A = 0; }

elseif_list(A) ::= . {
	A = xlPList();
}
elseif_list(A) ::= elseif_list(L) KW_ELSEIF(T) expr(C) then_opt opt_stmt_sep block(B). {
	A = L;
	xlAstAppendChild(A, xlAstCreateElseIf(&T, C, B));
}

else_opt(A) ::= . { A = NULL; }
else_opt(A) ::= KW_ELSE opt_stmt_sep block(B). { A = B; }

block(A) ::= stmt_list(L). {
	A = xlAstCreateBlock(NULL);
	xlAstAppendChildren(A, L);
}

do_while_block(A) ::= . {
	A = xlAstCreateBlock(NULL);
}
do_while_block(A) ::= do_while_stmt_items(L) opt_stmt_sep. {
	A = xlAstCreateBlock(NULL);
	xlAstAppendChildren(A, L);
}

do_while_stmt_items(A) ::= do_while_stmt_item(S). {
	A = xlPList();
	xlPAppendStmt(A, S);
}
do_while_stmt_items(A) ::= do_while_stmt_items(L) stmt_sep do_while_stmt_item(S). {
	A = L;
	xlPAppendStmt(A, S);
}

do_while_stmt_item(A) ::= error. {
	A = NULL;
}
do_while_stmt_item(A) ::= func_decl(F). { A = F; }
do_while_stmt_item(A) ::= typedef_decl(T). { A = T; }
do_while_stmt_item(A) ::= const_decl(C). { A = C; }
do_while_stmt_item(A) ::= var_decl(V). { A = V; }
do_while_stmt_item(A) ::= typed_var_decl(V). { A = V; }
do_while_stmt_item(A) ::= assign_stmt(S). { A = S; }
do_while_stmt_item(A) ::= if_stmt(I). { A = I; }
do_while_stmt_item(A) ::= try_stmt(T). { A = T; }
do_while_stmt_item(A) ::= for_in_stmt(F). { A = F; }
do_while_stmt_item(A) ::= import_stmt(I). { A = I; }
do_while_stmt_item(A) ::= export_stmt(E). { A = E; }
do_while_stmt_item(A) ::= KW_RETURN(T) return_value_list(V). { A = xlAstCreateReturn(&T, V); }
do_while_stmt_item(A) ::= KW_BREAK(T) INT(N). { A = xlAstCreateBreak(&T, (int)N.iIntValue); }
do_while_stmt_item(A) ::= KW_BREAK(T). { A = xlAstCreateBreak(&T, 1); }
do_while_stmt_item(A) ::= KW_CONTINUE(T) INT(N). { A = xlAstCreateContinue(&T, (int)N.iIntValue); }
do_while_stmt_item(A) ::= KW_CONTINUE(T). { A = xlAstCreateContinue(&T, 1); }
do_while_stmt_item(A) ::= KW_THROW(T) expr(E). { A = xlAstCreateThrow(&T, E); }
do_while_stmt_item(A) ::= KW_ASSERT(T) expr(E) assert_message(M). { A = xlAstCreateAssert(&T, E, M); }
do_while_stmt_item(A) ::= KW_YIELD(T) expr_opt(E). { A = xlAstCreateYield(&T, E); }
do_while_stmt_item(A) ::= expr(E). { A = xlAstCreateExprStmt(E); }

inline_stmt(A) ::= const_decl(C). { A = C; }
inline_stmt(A) ::= var_decl(V). { A = V; }
inline_stmt(A) ::= typed_var_decl(V). { A = V; }
inline_stmt(A) ::= assign_stmt(S). { A = S; }
inline_stmt(A) ::= KW_RETURN(T) return_value_list(V). { A = xlAstCreateReturn(&T, V); }
inline_stmt(A) ::= KW_BREAK(T) INT(N). { A = xlAstCreateBreak(&T, (int)N.iIntValue); }
inline_stmt(A) ::= KW_BREAK(T). { A = xlAstCreateBreak(&T, 1); }
inline_stmt(A) ::= KW_CONTINUE(T) INT(N). { A = xlAstCreateContinue(&T, (int)N.iIntValue); }
inline_stmt(A) ::= KW_CONTINUE(T). { A = xlAstCreateContinue(&T, 1); }
inline_stmt(A) ::= KW_THROW(T) expr(E). { A = xlAstCreateThrow(&T, E); }
inline_stmt(A) ::= KW_ASSERT(T) expr(E) assert_message(M). { A = xlAstCreateAssert(&T, E, M); }
inline_stmt(A) ::= KW_YIELD(T) expr_opt(E). { A = xlAstCreateYield(&T, E); }
inline_stmt(A) ::= expr(E). { A = xlAstCreateExprStmt(E); }

import_stmt(A) ::= KW_IMPORT(T) import_items(L). {
	XL_UNUSED(T);
	A = L;
}

import_items(A) ::= import_item(I). {
	A = xlPList();
	xlPAppendStmt(A, I);
}
import_items(A) ::= import_items(L) TK_COMMA import_item(I). {
	A = L;
	xlPAppendStmt(A, I);
}

import_item(A) ::= import_path(P) import_alias_opt(N). {
	A = xlAstCreateImport(NULL, P, &N);
}

import_path(A) ::= import_target(P). {
	A = P;
}
import_path(A) ::= STRING(T). {
	A = xlAstCreateLiteralFromToken(&T);
}
import_path(A) ::= STRING_RAW(T). {
	A = xlAstCreateLiteralFromToken(&T);
}

import_target(A) ::= TOK_IDENT(N). {
	A = xlAstCreateIdent(&N);
}
import_target(A) ::= import_target(B) OP_DOT(T) TOK_IDENT(N). {
	A = xlAstCreateMember(B, &N, 0, &T);
}

import_alias_opt(A) ::= KW_AS TOK_IDENT(N). {
	A = N;
}
import_alias_opt(A) ::= . {
	memset(&A, 0, sizeof(A));
}

export_stmt(A) ::= KW_EXPORT(T) expr(E). {
	A = xlAstCreateExport(&T, E);
}
export_stmt(A) ::= KW_EXPORT(T) export_func_decl(F). {
	XL_UNUSED(T);
	A = F;
}
export_stmt(A) ::= KW_EXPORT(T) const_decl(C). {
	A = xlPCreateExportedDeclListFromDecl(&T, C);
}
export_stmt(A) ::= KW_EXPORT(T) var_decl(V). {
	A = xlPCreateExportedDeclListFromDecl(&T, V);
}

export_func_decl(A) ::= async_opt(B) KW_FUNC TOK_IDENT(N) TK_LPAREN param_list(P) TK_RPAREN ret_type(R) opt_stmt_sep block(BODY) KW_END. {
	A = xlPCreateExportedDeclList(&N, xlAstCreateFuncDecl(&N, B, P, R, BODY), &N);
}
export_func_decl(A) ::= async_opt(B) KW_FUNC type_name(TY) OP_DOT TOK_IDENT(N) TK_LPAREN param_list(P) TK_RPAREN ret_type(R) opt_stmt_sep block(BODY) KW_END. {
	A = xlPCreateExportedDeclList(&N, xlPCreateMethodFuncDecl(xlAstCreateTypeRef(&TY), &N, B, P, R, BODY), &N);
}
export_func_decl(A) ::= async_opt(B) KW_FUNC generic_type_ref(TY) OP_DOT TOK_IDENT(N) TK_LPAREN param_list(P) TK_RPAREN ret_type(R) opt_stmt_sep block(BODY) KW_END. {
	A = xlPCreateExportedDeclList(&N, xlPCreateMethodFuncDecl(TY, &N, B, P, R, BODY), &N);
}
export_func_decl(A) ::= async_opt(B) KW_FUNC function_type_ref(TY) OP_DOT TOK_IDENT(N) TK_LPAREN param_list(P) TK_RPAREN ret_type(R) opt_stmt_sep block(BODY) KW_END. {
	A = xlPCreateExportedDeclList(&N, xlPCreateMethodFuncDecl(TY, &N, B, P, R, BODY), &N);
}

expr_opt(A) ::= . { A = NULL; }
expr_opt(A) ::= expr(E). { A = E; }

return_value_list(A) ::= . { A = xlPList(); }
return_value_list(A) ::= return_values(V). { A = V; }

return_values(A) ::= expr(E). {
	A = xlPList();
	xlAstAppendChild(A, E);
}
return_values(A) ::= return_values(L) TK_COMMA expr(E). {
	A = L;
	xlAstAppendChild(A, E);
}

arg_list(A) ::= . {
	A = xlPList();
}
arg_list(A) ::= args(L). {
	A = L;
}

args(A) ::= expr(E). {
	A = xlPList();
	xlAstAppendChild(A, E);
}
args(A) ::= args(L) TK_COMMA expr(E). {
	A = L;
	xlAstAppendChild(A, E);
}
args(A) ::= TOK_IDENT(N) OP_ASSIGN expr(E). {
	A = xlPList();
	xlAstAppendChild(A, xlAstCreateNamedArg(&N, E));
}
args(A) ::= args(L) TK_COMMA TOK_IDENT(N) OP_ASSIGN expr(E). {
	A = L;
	xlAstAppendChild(A, xlAstCreateNamedArg(&N, E));
}

expr_items(A) ::= expr(E). {
	A = xlPList();
	xlAstAppendChild(A, E);
}
expr_items(A) ::= expr_items(L) TK_COMMA expr(E). {
	A = L;
	xlAstAppendChild(A, E);
}

dict_items(A) ::= dict_item(I). {
	A = I;
}
dict_items(A) ::= dict_items(L) TK_COMMA dict_item(I). {
	A = L;
	xlAstAppendChildren(A, I);
}

dict_item(A) ::= expr(K) OP_COLON expr(V). {
	A = xlPCreateDictLiteralItems(K, V);
}

indexed_items(A) ::= indexed_item(I). {
	A = I;
}
indexed_items(A) ::= indexed_items(L) TK_COMMA indexed_item(I). {
	A = L;
	xlAstAppendChildren(A, I);
}

indexed_item(A) ::= expr(K) OP_COLON expr(V). {
	A = xlPCreateDictLiteralItems(K, V);
}

expr(A) ::= null_expr(E). {
	A = E;
}

null_expr(A) ::= conditional_expr(E). {
	A = E;
}
null_expr(A) ::= conditional_expr(L) OP_NULL_COALESCE(T) null_expr(R). {
	A = xlAstCreateBinary(&T, "??", L, R);
}

conditional_expr(A) ::= logical_or_expr(E). {
	A = E;
}
conditional_expr(A) ::= logical_or_expr(C) OP_QUESTION(T) expr(TE) OP_COLON conditional_expr(EE). {
	A = xlAstCreateTernary(&T, C, TE, EE);
}

logical_or_expr(A) ::= logical_and_expr(E). {
	A = E;
}
logical_or_expr(A) ::= logical_or_expr(L) KW_OR(T) logical_and_expr(R). {
	A = xlAstCreateBinary(&T, "or", L, R);
}

logical_and_expr(A) ::= bit_or_expr(E). {
	A = E;
}
logical_and_expr(A) ::= logical_and_expr(L) KW_AND(T) bit_or_expr(R). {
	A = xlAstCreateBinary(&T, "and", L, R);
}

bit_or_expr(A) ::= bit_xor_expr(E). {
	A = E;
}
bit_or_expr(A) ::= bit_or_expr(L) OP_PIPE(T) bit_xor_expr(R). {
	A = xlAstCreateBinary(&T, "|", L, R);
}

bit_xor_expr(A) ::= bit_and_expr(E). {
	A = E;
}
bit_xor_expr(A) ::= bit_xor_expr(L) OP_CARET(T) bit_and_expr(R). {
	A = xlAstCreateBinary(&T, "^", L, R);
}

bit_and_expr(A) ::= equality_expr(E). {
	A = E;
}
bit_and_expr(A) ::= bit_and_expr(L) OP_AMP(T) equality_expr(R). {
	A = xlAstCreateBinary(&T, "&", L, R);
}

equality_expr(A) ::= compare_expr(E). {
	A = E;
}
equality_expr(A) ::= equality_expr(L) OP_EQ(T) compare_expr(R). {
	A = xlAstCreateBinary(&T, "==", L, R);
}
equality_expr(A) ::= equality_expr(L) OP_NE(T) compare_expr(R). {
	A = xlAstCreateBinary(&T, "!=", L, R);
}
equality_expr(A) ::= equality_expr(L) OP_APPROX(T) compare_expr(R). {
	A = xlAstCreateBinary(&T, "~=", L, R);
}

compare_expr(A) ::= shift_expr(E). {
	A = E;
}
compare_expr(A) ::= compare_expr(L) OP_LT(T) shift_expr(R). {
	A = xlAstCreateBinary(&T, "<", L, R);
}
compare_expr(A) ::= compare_expr(L) OP_LE(T) shift_expr(R). {
	A = xlAstCreateBinary(&T, "<=", L, R);
}
compare_expr(A) ::= compare_expr(L) OP_GT(T) shift_expr(R). {
	A = xlAstCreateBinary(&T, ">", L, R);
}
compare_expr(A) ::= compare_expr(L) OP_GE(T) shift_expr(R). {
	A = xlAstCreateBinary(&T, ">=", L, R);
}
compare_expr(A) ::= compare_expr(L) KW_IN(T) shift_expr(R). {
	A = xlAstCreateBinary(&T, "in", L, R);
}

shift_expr(A) ::= add_expr(E). [OP_SHL] {
	A = E;
}
shift_expr(A) ::= shift_expr(L) OP_SHL(T) add_expr(R). {
	A = xlAstCreateBinary(&T, "<<", L, R);
}
shift_expr(A) ::= shift_expr(L) OP_SHR(T) add_expr(R). {
	A = xlAstCreateBinary(&T, ">>", L, R);
}

add_expr(A) ::= mul_expr(E). {
	A = E;
}
add_expr(A) ::= add_expr(L) OP_PLUS(T) mul_expr(R). {
	A = xlAstCreateBinary(&T, "+", L, R);
}
add_expr(A) ::= add_expr(L) OP_MINUS(T) mul_expr(R). {
	A = xlAstCreateBinary(&T, "-", L, R);
}

mul_expr(A) ::= power_expr(E). {
	A = E;
}
mul_expr(A) ::= mul_expr(L) OP_STAR(T) power_expr(R). {
	A = xlAstCreateBinary(&T, "*", L, R);
}
mul_expr(A) ::= mul_expr(L) OP_SLASH(T) power_expr(R). {
	A = xlAstCreateBinary(&T, "/", L, R);
}
mul_expr(A) ::= mul_expr(L) OP_PERCENT(T) power_expr(R). {
	A = xlAstCreateBinary(&T, "%", L, R);
}

power_expr(A) ::= unary_expr(E). {
	A = E;
}
power_expr(A) ::= unary_expr(L) OP_POWER(T) power_expr(R). {
	A = xlAstCreateBinary(&T, "**", L, R);
}

yield_expr(A) ::= KW_YIELD(T) expr_opt(E). {
	A = xlAstCreateYield(&T, E);
}

unary_expr(A) ::= postfix_expr(E). [KW_AWAIT] {
	A = E;
}
unary_expr(A) ::= OP_MINUS(T) unary_expr(E). {
	A = xlAstCreateUnary(&T, "-", E);
}
unary_expr(A) ::= KW_NOT(T) unary_expr(E). {
	A = xlAstCreateUnary(&T, "not", E);
}
unary_expr(A) ::= OP_BANG(T) unary_expr(E). {
	A = xlAstCreateUnary(&T, "!", E);
}
unary_expr(A) ::= KW_AWAIT(T) unary_expr(E). {
	A = xlAstCreateUnary(&T, "await", E);
}

postfix_expr(A) ::= primary_expr(E). {
	A = E;
}
postfix_expr(A) ::= postfix_expr(C) TK_LPAREN arg_list(ARGS) TK_RPAREN(T). {
	A = xlAstCreateCall(C, ARGS, &T);
}
postfix_expr(A) ::= postfix_expr(B) OP_DOT(T) TOK_IDENT(N). {
	A = xlAstCreateMember(B, &N, 0, &T);
}
postfix_expr(A) ::= postfix_expr(B) OP_SAFE_DOT(T) TOK_IDENT(N). {
	A = xlAstCreateMember(B, &N, 1, &T);
}
postfix_expr(A) ::= postfix_expr(B) TK_LBRACKET(T) expr(E) TK_RBRACKET. {
	A = xlAstCreateIndex(B, E, 0, &T);
}
postfix_expr(A) ::= postfix_expr(B) OP_SAFE_LBRACKET(T) expr(E) TK_RBRACKET. {
	A = xlAstCreateIndex(B, E, 1, &T);
}

primary_expr(A) ::= literal(L). {
	A = L;
}
primary_expr(A) ::= KW_VAR(N). [TK_LBRACE] {
	A = xlAstCreateIdent(&N);
}
primary_expr(A) ::= TOK_IDENT(N). [TK_LBRACE] {
	A = xlAstCreateIdent(&N);
}
primary_expr(A) ::= TK_LPAREN expr(E) TK_RPAREN. {
	A = E;
}
primary_expr(A) ::= TK_LPAREN yield_expr(E) TK_RPAREN. {
	A = E;
}
primary_expr(A) ::= bare_bracket_literal(L). {
	A = L;
}
primary_expr(A) ::= bare_brace_literal(L). {
	A = L;
}
primary_expr(A) ::= TOK_IDENT(N) TK_LBRACE TK_RBRACE. {
	A = xlPCreateBracedValueLiteral(pCtx, &N, xlPList());
}
primary_expr(A) ::= TOK_IDENT(N) TK_LBRACE expr_items(ITEMS) TK_RBRACE. {
	A = xlPCreateBracedValueLiteral(pCtx, &N, ITEMS);
}
primary_expr(A) ::= TOK_IDENT(N) TK_LBRACE dict_items(ITEMS) TK_RBRACE. {
	A = xlPCreateBracedDictLiteral(pCtx, &N, ITEMS);
}

bare_bracket_literal(A) ::= TK_LBRACKET(T) TK_RBRACKET. {
	A = xlPCreateHiddenCall("__array_literal", xlPList(), &T);
}
bare_bracket_literal(A) ::= TK_LBRACKET(T) expr_items(ITEMS) TK_RBRACKET. {
	A = xlPCreateHiddenCall("__array_literal", ITEMS, &T);
}
bare_bracket_literal(A) ::= TK_LBRACKET(T) indexed_items(ITEMS) TK_RBRACKET. {
	A = xlPCreateBareBracketLiteral(ITEMS, 1, &T);
}

bare_brace_literal(A) ::= TK_LBRACE(T) TK_RBRACE. {
	A = xlPCreateBareBraceLiteral(xlPList(), 1, &T);
}
bare_brace_literal(A) ::= TK_LBRACE(T) expr_items(ITEMS) TK_RBRACE. {
	A = xlPCreateBareBraceLiteral(ITEMS, 0, &T);
}
bare_brace_literal(A) ::= TK_LBRACE(T) dict_items(ITEMS) TK_RBRACE. {
	A = xlPCreateBareBraceLiteral(ITEMS, 1, &T);
}

literal(A) ::= INT(T). { A = xlAstCreateLiteralFromToken(&T); }
literal(A) ::= INT_HEX(T). { A = xlAstCreateLiteralFromToken(&T); }
literal(A) ::= INT_OCT(T). { A = xlAstCreateLiteralFromToken(&T); }
literal(A) ::= INT_BIN(T). { A = xlAstCreateLiteralFromToken(&T); }
literal(A) ::= FLOAT(T). { A = xlAstCreateLiteralFromToken(&T); }
literal(A) ::= STRING(T). { A = xlAstCreateLiteralFromToken(&T); }
literal(A) ::= STRING_RAW(T). { A = xlAstCreateLiteralFromToken(&T); }
literal(A) ::= TEMPLATE(T). { A = xlAstCreateLiteralFromToken(&T); }
literal(A) ::= TIME(T). { A = xlAstCreateLiteralFromToken(&T); }
literal(A) ::= KW_TRUE(T). { A = xlAstCreateLiteralFromToken(&T); }
literal(A) ::= KW_FALSE(T). { A = xlAstCreateLiteralFromToken(&T); }
literal(A) ::= KW_NULL(T). { A = xlAstCreateLiteralFromToken(&T); }
