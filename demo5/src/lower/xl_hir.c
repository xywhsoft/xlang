#include "xl_hir.h"

static char* __xlHirDupText(const char* sText)
{
	size_t iLen;
	char* sCopy;

	if ( sText == NULL ) {
		sText = "";
	}

	iLen = strlen(sText);
	sCopy = (char*)malloc(iLen + 1);
	if ( sCopy == NULL ) {
		return NULL;
	}

	memcpy(sCopy, sText, iLen + 1);
	return sCopy;
}

XL_HirNode* xlHirCreateNode(XL_HirKind eKind, const XL_AstNode* pAst)
{
	XL_HirNode* pNode;

	pNode = (XL_HirNode*)calloc(1, sizeof(XL_HirNode));
	if ( pNode == NULL ) {
		return NULL;
	}

	pNode->eKind = eKind;
	pNode->iScopeId = -1;
	pNode->iSymbolIndex = -1;
	pNode->iFuncIndex = -1;
	if ( pAst != NULL ) {
		pNode->iLine = pAst->iLine;
		pNode->iColumn = pAst->iColumn;
	}
	return pNode;
}

void xlHirSetText(XL_HirNode* pNode, const char* sText)
{
	if ( pNode == NULL ) {
		return;
	}

	free(pNode->sText);
	pNode->sText = __xlHirDupText(sText);
}

void xlHirSetAbiName(XL_HirNode* pNode, const char* sAbiName)
{
	if ( pNode == NULL ) {
		return;
	}

	free(pNode->sAbiName);
	pNode->sAbiName = __xlHirDupText(sAbiName);
}

void xlHirSetExternName(XL_HirNode* pNode, const char* sExternName)
{
	if ( pNode == NULL ) {
		return;
	}

	free(pNode->sExternName);
	pNode->sExternName = __xlHirDupText(sExternName);
}

void xlHirSetTypeName(XL_HirNode* pNode, const char* sTypeName)
{
	if ( pNode == NULL ) {
		return;
	}

	free(pNode->sTypeName);
	pNode->sTypeName = __xlHirDupText(sTypeName);
}

void xlHirAppendChild(XL_HirNode* pNode, XL_HirNode* pChild)
{
	XL_HirNode** ppNewItems;
	size_t iNewCap;

	if ( (pNode == NULL) || (pChild == NULL) ) {
		return;
	}
	if ( pNode->iChildCount >= pNode->iChildCapacity ) {
		iNewCap = (pNode->iChildCapacity == 0) ? 4 : (pNode->iChildCapacity * 2);
		ppNewItems = (XL_HirNode**)realloc(pNode->ppChildren, sizeof(XL_HirNode*) * iNewCap);
		if ( ppNewItems == NULL ) {
			return;
		}

		pNode->ppChildren = ppNewItems;
		pNode->iChildCapacity = iNewCap;
	}

	pNode->ppChildren[pNode->iChildCount++] = pChild;
}

void xlHirFree(XL_HirNode* pNode)
{
	size_t i;

	if ( pNode == NULL ) {
		return;
	}

	for ( i = 0; i < pNode->iChildCount; ++i ) {
		xlHirFree(pNode->ppChildren[i]);
	}
	free(pNode->ppChildren);
	free(pNode->sText);
	free(pNode->sAbiName);
	free(pNode->sExternName);
	free(pNode->sTypeName);
	free(pNode);
}

const char* xlHirKindName(XL_HirKind eKind)
{
	static const char* arrNames[] = {
		"Program",
		"Func",
		"Param",
		"Block",
		"Const",
		"Var",
		"If",
		"Try",
		"ForIn",
		"Return",
		"Break",
		"Continue",
		"Throw",
		"Assert",
		"Yield",
		"Import",
		"Export",
		"Call",
		"Member",
		"Index",
		"Binary",
		"Unary",
		"Name",
		"Literal"
	};

	if ( ((int)eKind >= 0) && (eKind <= XL_HIR_LITERAL) ) {
		return arrNames[eKind];
	}

	return "Unknown";
}
