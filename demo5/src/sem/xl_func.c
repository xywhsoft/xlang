#include "xl_func.h"

static char* __xlFuncDupText(const char* sText)
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

static int __xlFuncTableGrow(XL_FuncTable* pTable)
{
	XL_FuncInfo* pNewItems;
	size_t iNewCapacity;

	if ( pTable == NULL ) {
		return XL_RC_BAD_ARG;
	}
	if ( pTable->iCount < pTable->iCapacity ) {
		return XL_RC_OK;
	}

	iNewCapacity = (pTable->iCapacity == 0) ? 8 : (pTable->iCapacity * 2);
	pNewItems = (XL_FuncInfo*)realloc(pTable->pItems, sizeof(XL_FuncInfo) * iNewCapacity);
	if ( pNewItems == NULL ) {
		return XL_RC_IO_ERROR;
	}

	pTable->pItems = pNewItems;
	pTable->iCapacity = iNewCapacity;
	return XL_RC_OK;
}

static void __xlFuncInfoFree(XL_FuncInfo* pInfo)
{
	size_t i;

	if ( pInfo == NULL ) {
		return;
	}

	free(pInfo->sName);
	free(pInfo->sReturnTypeName);
	for ( i = 0; i < pInfo->iParamCount; ++i ) {
		free(pInfo->arrParamNames[i]);
		free(pInfo->arrParamTypes[i]);
	}
	free(pInfo->arrParamNames);
	free(pInfo->arrParamTypes);
	memset(pInfo, 0, sizeof(*pInfo));
}

void xlFuncTableInit(XL_FuncTable* pTable)
{
	if ( pTable != NULL ) {
		memset(pTable, 0, sizeof(*pTable));
	}
}

void xlFuncTableFree(XL_FuncTable* pTable)
{
	size_t i;

	if ( pTable == NULL ) {
		return;
	}

	for ( i = 0; i < pTable->iCount; ++i ) {
		__xlFuncInfoFree(&pTable->pItems[i]);
	}
	free(pTable->pItems);
	memset(pTable, 0, sizeof(*pTable));
}

int xlFuncTableAdd(
	XL_FuncTable* pTable,
	const char* sName,
	const char* sReturnTypeName,
	const char* const* arrParamNames,
	const char* const* arrParamTypes,
	size_t iParamCount,
	int iScopeId,
	int iLine,
	int iColumn,
	int bAsync,
	int bGenerator
)
{
	XL_FuncInfo* pInfo;
	size_t i;

	if ( (pTable == NULL) || (sName == NULL) || (sName[0] == '\0') ) {
		return XL_RC_BAD_ARG;
	}
	if ( __xlFuncTableGrow(pTable) != XL_RC_OK ) {
		return XL_RC_IO_ERROR;
	}

	pInfo = &pTable->pItems[pTable->iCount];
	memset(pInfo, 0, sizeof(*pInfo));

	pInfo->sName = __xlFuncDupText(sName);
	pInfo->sReturnTypeName = __xlFuncDupText(sReturnTypeName);
	pInfo->iScopeId = iScopeId;
	pInfo->iLine = iLine;
	pInfo->iColumn = iColumn;
	pInfo->bAsync = bAsync;
	pInfo->bGenerator = bGenerator;
	pInfo->iParamCount = iParamCount;

	if ( iParamCount != 0 ) {
		pInfo->arrParamNames = (char**)calloc(iParamCount, sizeof(char*));
		pInfo->arrParamTypes = (char**)calloc(iParamCount, sizeof(char*));
		if ( (pInfo->arrParamNames == NULL) || (pInfo->arrParamTypes == NULL) ) {
			__xlFuncInfoFree(pInfo);
			return XL_RC_IO_ERROR;
		}

		for ( i = 0; i < iParamCount; ++i ) {
			pInfo->arrParamNames[i] = __xlFuncDupText((arrParamNames != NULL) ? arrParamNames[i] : "");
			pInfo->arrParamTypes[i] = __xlFuncDupText((arrParamTypes != NULL) ? arrParamTypes[i] : "");
			if ( (pInfo->arrParamNames[i] == NULL) || (pInfo->arrParamTypes[i] == NULL) ) {
				__xlFuncInfoFree(pInfo);
				return XL_RC_IO_ERROR;
			}
		}
	}

	if ( (pInfo->sName == NULL) || (pInfo->sReturnTypeName == NULL) ) {
		__xlFuncInfoFree(pInfo);
		return XL_RC_IO_ERROR;
	}

	pTable->iCount += 1;
	return XL_RC_OK;
}

const XL_FuncInfo* xlFuncTableGet(const XL_FuncTable* pTable, size_t iIndex)
{
	if ( (pTable == NULL) || (iIndex >= pTable->iCount) ) {
		return NULL;
	}

	return &pTable->pItems[iIndex];
}
