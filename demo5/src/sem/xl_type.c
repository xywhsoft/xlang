#include "xl_type.h"

static char* __xlTypeDupText(const char* sText)
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

static int __xlTypeTableGrow(XL_TypeTable* pTable)
{
	XL_TypeInfo* pNewItems;
	size_t iNewCapacity;

	if ( pTable == NULL ) {
		return XL_RC_BAD_ARG;
	}
	if ( pTable->iCount < pTable->iCapacity ) {
		return XL_RC_OK;
	}

	iNewCapacity = (pTable->iCapacity == 0) ? 8 : (pTable->iCapacity * 2);
	pNewItems = (XL_TypeInfo*)realloc(pTable->pItems, sizeof(XL_TypeInfo) * iNewCapacity);
	if ( pNewItems == NULL ) {
		return XL_RC_IO_ERROR;
	}

	pTable->pItems = pNewItems;
	pTable->iCapacity = iNewCapacity;
	return XL_RC_OK;
}

static int __xlProtocolTableGrow(XL_ProtocolTable* pTable)
{
	XL_ProtocolInfo* pNewItems;
	size_t iNewCapacity;

	if ( pTable == NULL ) {
		return XL_RC_BAD_ARG;
	}
	if ( pTable->iCount < pTable->iCapacity ) {
		return XL_RC_OK;
	}

	iNewCapacity = (pTable->iCapacity == 0) ? 8 : (pTable->iCapacity * 2);
	pNewItems = (XL_ProtocolInfo*)realloc(pTable->pItems, sizeof(XL_ProtocolInfo) * iNewCapacity);
	if ( pNewItems == NULL ) {
		return XL_RC_IO_ERROR;
	}

	pTable->pItems = pNewItems;
	pTable->iCapacity = iNewCapacity;
	return XL_RC_OK;
}

static int __xlProtocolTypeMatches(const char* sRegisteredType, const char* sQueryType)
{
	size_t iLen;

	if ( (sRegisteredType == NULL) || (sQueryType == NULL) ) {
		return 0;
	}
	if ( strcmp(sRegisteredType, sQueryType) == 0 ) {
		return 1;
	}

	iLen = strlen(sRegisteredType);
	return (strncmp(sRegisteredType, sQueryType, iLen) == 0) &&
		(sQueryType[iLen] == '<');
}

void xlTypeTableInit(XL_TypeTable* pTable)
{
	if ( pTable != NULL ) {
		memset(pTable, 0, sizeof(*pTable));
	}
}

void xlTypeTableFree(XL_TypeTable* pTable)
{
	size_t i;

	if ( pTable == NULL ) {
		return;
	}

	for ( i = 0; i < pTable->iCount; ++i ) {
		free(pTable->pItems[i].sName);
		free(pTable->pItems[i].sAliasTarget);
	}
	free(pTable->pItems);
	memset(pTable, 0, sizeof(*pTable));
}

int xlTypeTableAdd(XL_TypeTable* pTable, const char* sName, int bBuiltin, int iLine, int iColumn)
{
	XL_TypeInfo* pItem;

	if ( (pTable == NULL) || (sName == NULL) || (sName[0] == '\0') ) {
		return XL_RC_BAD_ARG;
	}
	if ( xlTypeTableFind(pTable, sName) != NULL ) {
		return XL_RC_OK;
	}
	if ( __xlTypeTableGrow(pTable) != XL_RC_OK ) {
		return XL_RC_IO_ERROR;
	}

	pItem = &pTable->pItems[pTable->iCount++];
	memset(pItem, 0, sizeof(*pItem));
	pItem->sName = __xlTypeDupText(sName);
	if ( pItem->sName == NULL ) {
		pTable->iCount -= 1;
		return XL_RC_IO_ERROR;
	}

	pItem->bBuiltin = bBuiltin;
	pItem->iLine = iLine;
	pItem->iColumn = iColumn;
	return XL_RC_OK;
}

int xlTypeTableAddAlias(XL_TypeTable* pTable, const char* sName, const char* sAliasTarget, int iLine, int iColumn)
{
	XL_TypeInfo* pItem;

	if ( (pTable == NULL) || (sName == NULL) || (sName[0] == '\0') || (sAliasTarget == NULL) || (sAliasTarget[0] == '\0') ) {
		return XL_RC_BAD_ARG;
	}
	if ( xlTypeTableFind(pTable, sName) != NULL ) {
		return XL_RC_BAD_ARG;
	}
	if ( __xlTypeTableGrow(pTable) != XL_RC_OK ) {
		return XL_RC_IO_ERROR;
	}

	pItem = &pTable->pItems[pTable->iCount++];
	memset(pItem, 0, sizeof(*pItem));
	pItem->sName = __xlTypeDupText(sName);
	pItem->sAliasTarget = __xlTypeDupText(sAliasTarget);
	if ( (pItem->sName == NULL) || (pItem->sAliasTarget == NULL) ) {
		free(pItem->sName);
		free(pItem->sAliasTarget);
		memset(pItem, 0, sizeof(*pItem));
		pTable->iCount -= 1;
		return XL_RC_IO_ERROR;
	}

	pItem->bBuiltin = 0;
	pItem->iLine = iLine;
	pItem->iColumn = iColumn;
	return XL_RC_OK;
}

const XL_TypeInfo* xlTypeTableFind(const XL_TypeTable* pTable, const char* sName)
{
	size_t i;

	if ( (pTable == NULL) || (sName == NULL) ) {
		return NULL;
	}

	for ( i = 0; i < pTable->iCount; ++i ) {
		if ( strcmp(pTable->pItems[i].sName, sName) == 0 ) {
			return &pTable->pItems[i];
		}
	}

	return NULL;
}

const XL_TypeInfo* xlTypeTableGet(const XL_TypeTable* pTable, size_t iIndex)
{
	if ( (pTable == NULL) || (iIndex >= pTable->iCount) ) {
		return NULL;
	}

	return &pTable->pItems[iIndex];
}

void xlProtocolTableInit(XL_ProtocolTable* pTable)
{
	if ( pTable != NULL ) {
		memset(pTable, 0, sizeof(*pTable));
	}
}

void xlProtocolTableFree(XL_ProtocolTable* pTable)
{
	size_t i;

	if ( pTable == NULL ) {
		return;
	}

	for ( i = 0; i < pTable->iCount; ++i ) {
		free(pTable->pItems[i].sTypeName);
	}
	free(pTable->pItems);
	memset(pTable, 0, sizeof(*pTable));
}

const XL_ProtocolInfo* xlProtocolTableFind(const XL_ProtocolTable* pTable, const char* sTypeName)
{
	size_t i;

	if ( (pTable == NULL) || (sTypeName == NULL) ) {
		return NULL;
	}

	for ( i = 0; i < pTable->iCount; ++i ) {
		if ( __xlProtocolTypeMatches(pTable->pItems[i].sTypeName, sTypeName) ) {
			return &pTable->pItems[i];
		}
	}

	return NULL;
}

int xlProtocolTableAdd(XL_ProtocolTable* pTable, const char* sTypeName, XL_ProtocolKind eProtocol)
{
	XL_ProtocolInfo* pItem;

	if ( (pTable == NULL) || (sTypeName == NULL) || (sTypeName[0] == '\0') || (eProtocol == XL_PROTOCOL_NONE) ) {
		return XL_RC_BAD_ARG;
	}

	pItem = (XL_ProtocolInfo*)xlProtocolTableFind(pTable, sTypeName);
	if ( pItem != NULL ) {
		pItem->uMask |= (unsigned int)eProtocol;
		return XL_RC_OK;
	}
	if ( __xlProtocolTableGrow(pTable) != XL_RC_OK ) {
		return XL_RC_IO_ERROR;
	}

	pItem = &pTable->pItems[pTable->iCount++];
	memset(pItem, 0, sizeof(*pItem));
	pItem->sTypeName = __xlTypeDupText(sTypeName);
	if ( pItem->sTypeName == NULL ) {
		pTable->iCount -= 1;
		return XL_RC_IO_ERROR;
	}

	pItem->uMask = (unsigned int)eProtocol;
	return XL_RC_OK;
}

int xlProtocolTableHas(const XL_ProtocolTable* pTable, const char* sTypeName, XL_ProtocolKind eProtocol)
{
	const XL_ProtocolInfo* pItem;

	if ( (pTable == NULL) || (sTypeName == NULL) || (eProtocol == XL_PROTOCOL_NONE) ) {
		return 0;
	}

	pItem = xlProtocolTableFind(pTable, sTypeName);
	return (pItem != NULL) && ((pItem->uMask & (unsigned int)eProtocol) != 0U);
}
