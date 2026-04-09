#include "xl_scope.h"

static char* __xlScopeDupText(const char* sText)
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

static int __xlScopeTableGrow(XL_ScopeTable* pTable)
{
	XL_ScopeInfo* pNewItems;
	size_t iNewCapacity;

	if ( pTable == NULL ) {
		return XL_RC_BAD_ARG;
	}
	if ( pTable->iCount < pTable->iCapacity ) {
		return XL_RC_OK;
	}

	iNewCapacity = (pTable->iCapacity == 0) ? 8 : (pTable->iCapacity * 2);
	pNewItems = (XL_ScopeInfo*)realloc(pTable->pItems, sizeof(XL_ScopeInfo) * iNewCapacity);
	if ( pNewItems == NULL ) {
		return XL_RC_IO_ERROR;
	}

	pTable->pItems = pNewItems;
	pTable->iCapacity = iNewCapacity;
	return XL_RC_OK;
}

const char* xlScopeKindName(XL_ScopeKind eKind)
{
	switch ( eKind ) {
		case XL_SCOPE_MODULE:
			return "module";
		case XL_SCOPE_FUNC:
			return "func";
		case XL_SCOPE_BLOCK:
			return "block";
		default:
			return "unknown";
	}
}

void xlScopeTableInit(XL_ScopeTable* pTable)
{
	if ( pTable != NULL ) {
		memset(pTable, 0, sizeof(*pTable));
	}
}

void xlScopeTableFree(XL_ScopeTable* pTable)
{
	size_t i;

	if ( pTable == NULL ) {
		return;
	}

	for ( i = 0; i < pTable->iCount; ++i ) {
		free(pTable->pItems[i].sName);
	}
	free(pTable->pItems);
	memset(pTable, 0, sizeof(*pTable));
}

int xlScopeTableAdd(
	XL_ScopeTable* pTable,
	XL_ScopeKind eKind,
	int iParentId,
	const char* sName,
	int iLine,
	int iColumn,
	int* piScopeId
)
{
	XL_ScopeInfo* pItem;
	const XL_ScopeInfo* pParent;

	if ( pTable == NULL ) {
		return XL_RC_BAD_ARG;
	}
	if ( __xlScopeTableGrow(pTable) != XL_RC_OK ) {
		return XL_RC_IO_ERROR;
	}

	pItem = &pTable->pItems[pTable->iCount];
	memset(pItem, 0, sizeof(*pItem));
	pItem->iId = (int)pTable->iCount;
	pItem->iParentId = iParentId;
	pItem->eKind = eKind;
	pItem->sName = __xlScopeDupText(sName);
	if ( pItem->sName == NULL ) {
		return XL_RC_IO_ERROR;
	}

	pParent = xlScopeTableGet(pTable, iParentId);
	pItem->iDepth = (pParent != NULL) ? (pParent->iDepth + 1) : 0;
	pItem->iLine = iLine;
	pItem->iColumn = iColumn;
	pTable->iCount += 1;

	if ( piScopeId != NULL ) {
		*piScopeId = pItem->iId;
	}

	return XL_RC_OK;
}

const XL_ScopeInfo* xlScopeTableGet(const XL_ScopeTable* pTable, int iScopeId)
{
	if ( (pTable == NULL) || (iScopeId < 0) || ((size_t)iScopeId >= pTable->iCount) ) {
		return NULL;
	}

	return &pTable->pItems[iScopeId];
}
