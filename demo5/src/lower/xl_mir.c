#include "xl_mir.h"

static char* __xlMirDupText(const char* sText)
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

XL_MirNode* xlMirCreateNode(XL_MirKind eKind, const XL_HirNode* pHir)
{
	XL_MirNode* pNode;

	pNode = (XL_MirNode*)calloc(1, sizeof(XL_MirNode));
	if ( pNode == NULL ) {
		return NULL;
	}

	pNode->eKind = eKind;
	pNode->iScopeId = -1;
	pNode->iSymbolIndex = -1;
	pNode->iFuncIndex = -1;
	if ( pHir != NULL ) {
		pNode->iLine = pHir->iLine;
		pNode->iColumn = pHir->iColumn;
	}
	return pNode;
}

void xlMirSetText(XL_MirNode* pNode, const char* sText)
{
	if ( pNode == NULL ) {
		return;
	}

	free(pNode->sText);
	pNode->sText = __xlMirDupText(sText);
}

void xlMirSetAbiName(XL_MirNode* pNode, const char* sAbiName)
{
	if ( pNode == NULL ) {
		return;
	}

	free(pNode->sAbiName);
	pNode->sAbiName = __xlMirDupText(sAbiName);
}

void xlMirSetExternName(XL_MirNode* pNode, const char* sExternName)
{
	if ( pNode == NULL ) {
		return;
	}

	free(pNode->sExternName);
	pNode->sExternName = __xlMirDupText(sExternName);
}

void xlMirSetTypeName(XL_MirNode* pNode, const char* sTypeName)
{
	if ( pNode == NULL ) {
		return;
	}

	free(pNode->sTypeName);
	pNode->sTypeName = __xlMirDupText(sTypeName);
}

void xlMirSetConversion(XL_MirNode* pNode, const char* sConversion)
{
	if ( pNode == NULL ) {
		return;
	}

	free(pNode->sConversion);
	pNode->sConversion = __xlMirDupText(sConversion);
}

void xlMirAppendChild(XL_MirNode* pNode, XL_MirNode* pChild)
{
	XL_MirNode** ppNewItems;
	size_t iNewCap;

	if ( (pNode == NULL) || (pChild == NULL) ) {
		return;
	}
	if ( pNode->iChildCount >= pNode->iChildCapacity ) {
		iNewCap = (pNode->iChildCapacity == 0) ? 4 : (pNode->iChildCapacity * 2);
		ppNewItems = (XL_MirNode**)realloc(pNode->ppChildren, sizeof(XL_MirNode*) * iNewCap);
		if ( ppNewItems == NULL ) {
			return;
		}

		pNode->ppChildren = ppNewItems;
		pNode->iChildCapacity = iNewCap;
	}

	pNode->ppChildren[pNode->iChildCount++] = pChild;
}

void xlMirFree(XL_MirNode* pNode)
{
	size_t i;

	if ( pNode == NULL ) {
		return;
	}

	for ( i = 0; i < pNode->iChildCount; ++i ) {
		xlMirFree(pNode->ppChildren[i]);
	}
	free(pNode->ppChildren);
	free(pNode->sText);
	free(pNode->sAbiName);
	free(pNode->sExternName);
	free(pNode->sTypeName);
	free(pNode->sConversion);
	free(pNode);
}

const char* xlMirKindName(XL_MirKind eKind)
{
	static const char* arrNames[] = {
		"Program",
		"Func",
		"Block",
		"Bind",
		"Branch",
		"Try",
		"While",
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
		"Value"
	};

	if ( ((int)eKind >= 0) && (eKind <= XL_MIR_VALUE) ) {
		return arrNames[eKind];
	}

	return "Unknown";
}
