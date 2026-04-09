#include "xl_symbol.h"

static char* __xlSymbolDupText(const char* sText)
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

static int __xlSymbolTableGrow(XL_SymbolTable* pTable)
{
	XL_SymbolInfo* pNewItems;
	size_t iNewCapacity;

	if ( pTable == NULL ) {
		return XL_RC_BAD_ARG;
	}
	if ( pTable->iCount < pTable->iCapacity ) {
		return XL_RC_OK;
	}

	iNewCapacity = (pTable->iCapacity == 0) ? 16 : (pTable->iCapacity * 2);
	pNewItems = (XL_SymbolInfo*)realloc(pTable->pItems, sizeof(XL_SymbolInfo) * iNewCapacity);
	if ( pNewItems == NULL ) {
		return XL_RC_IO_ERROR;
	}

	pTable->pItems = pNewItems;
	pTable->iCapacity = iNewCapacity;
	return XL_RC_OK;
}

const char* xlSymbolKindName(XL_SymbolKind eKind)
{
	switch ( eKind ) {
		case XL_SYMBOL_FUNC:
			return "func";
		case XL_SYMBOL_PARAM:
			return "param";
		case XL_SYMBOL_CONST:
			return "const";
		case XL_SYMBOL_VAR:
			return "var";
		case XL_SYMBOL_IMPORT:
			return "import";
		case XL_SYMBOL_EXPORT:
			return "export";
		default:
			return "unknown";
	}
}

void xlSymbolTableInit(XL_SymbolTable* pTable)
{
	if ( pTable != NULL ) {
		memset(pTable, 0, sizeof(*pTable));
	}
}

void xlSymbolTableFree(XL_SymbolTable* pTable)
{
	size_t i;

	if ( pTable == NULL ) {
		return;
	}

	for ( i = 0; i < pTable->iCount; ++i ) {
		free(pTable->pItems[i].sName);
		free(pTable->pItems[i].sTypeName);
	}
	free(pTable->pItems);
	memset(pTable, 0, sizeof(*pTable));
}

int xlSymbolTableAdd(
	XL_SymbolTable* pTable,
	XL_SymbolKind eKind,
	int iScopeId,
	const char* sName,
	const char* sTypeName,
	int iLine,
	int iColumn,
	int bAsync,
	int bGenerator
)
{
	XL_SymbolInfo* pItem;

	if ( (pTable == NULL) || (sName == NULL) || (sName[0] == '\0') ) {
		return XL_RC_BAD_ARG;
	}
	if ( __xlSymbolTableGrow(pTable) != XL_RC_OK ) {
		return XL_RC_IO_ERROR;
	}

	pItem = &pTable->pItems[pTable->iCount];
	memset(pItem, 0, sizeof(*pItem));
	pItem->eKind = eKind;
	pItem->iScopeId = iScopeId;
	pItem->sName = __xlSymbolDupText(sName);
	pItem->sTypeName = __xlSymbolDupText((sTypeName != NULL) ? sTypeName : "");
	if ( (pItem->sName == NULL) || (pItem->sTypeName == NULL) ) {
		free(pItem->sName);
		free(pItem->sTypeName);
		memset(pItem, 0, sizeof(*pItem));
		return XL_RC_IO_ERROR;
	}

	pItem->iLine = iLine;
	pItem->iColumn = iColumn;
	pItem->bAsync = bAsync;
	pItem->bGenerator = bGenerator;
	pTable->iCount += 1;
	return XL_RC_OK;
}

int xlSymbolTableFindInScope(const XL_SymbolTable* pTable, int iScopeId, const char* sName)
{
	size_t i;

	if ( (pTable == NULL) || (sName == NULL) ) {
		return -1;
	}

	for ( i = 0; i < pTable->iCount; ++i ) {
		if ( (pTable->pItems[i].iScopeId == iScopeId) && (strcmp(pTable->pItems[i].sName, sName) == 0) ) {
			return (int)i;
		}
	}

	return -1;
}

const XL_SymbolInfo* xlSymbolTableFindVisible(
	const XL_SymbolTable* pTable,
	const XL_ScopeTable* pScopes,
	int iScopeId,
	const char* sName
)
{
	const XL_ScopeInfo* pScope;
	int iScanScope;
	int iSymbol;

	if ( (pTable == NULL) || (pScopes == NULL) || (sName == NULL) ) {
		return NULL;
	}

	iScanScope = iScopeId;
	while ( iScanScope >= 0 ) {
		iSymbol = xlSymbolTableFindInScope(pTable, iScanScope, sName);
		if ( iSymbol >= 0 ) {
			return &pTable->pItems[iSymbol];
		}

		pScope = xlScopeTableGet(pScopes, iScanScope);
		if ( pScope == NULL ) {
			break;
		}
		iScanScope = pScope->iParentId;
	}

	return NULL;
}

const XL_SymbolInfo* xlSymbolTableGet(const XL_SymbolTable* pTable, size_t iIndex)
{
	if ( (pTable == NULL) || (iIndex >= pTable->iCount) ) {
		return NULL;
	}

	return &pTable->pItems[iIndex];
}
