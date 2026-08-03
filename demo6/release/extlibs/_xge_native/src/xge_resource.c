static char* __xgeResourceXPackMakePath(const char* sRoot, const char* sPath)
{
	char* sFull;
	size_t iRootLen;
	size_t iPathLen;
	size_t iNeed;
	int bNeedSlash;

	if ( sPath == NULL ) {
		return NULL;
	}
	while ( (*sPath == '/') || (*sPath == '\\') ) {
		sPath++;
	}
	if ( (sRoot == NULL) || (sRoot[0] == 0) ) {
		return __xgeStrDup(sPath);
	}
	iRootLen = strlen(sRoot);
	iPathLen = strlen(sPath);
	bNeedSlash = ((iRootLen > 0) && (sRoot[iRootLen - 1] != '/') && (sRoot[iRootLen - 1] != '\\')) ? 1 : 0;
	if ( iRootLen > (SIZE_MAX - iPathLen - 2u) ) {
		return NULL;
	}
	iNeed = iRootLen + iPathLen + (size_t)bNeedSlash + 1u;
	sFull = (char*)xrtMalloc(iNeed);
	if ( sFull == NULL ) {
		return NULL;
	}
	memcpy(sFull, sRoot, iRootLen);
	if ( bNeedSlash ) {
		sFull[iRootLen] = '/';
		iRootLen++;
	}
	memcpy(sFull + iRootLen, sPath, iPathLen + 1u);
	return sFull;
}

static int __xgeResourceXPackLoad(const char* sURI, void** ppData, int* pSize, void* pUser)
{
	xge_xpack_provider_t* pProvider;
	const char* sPath;
	char* sPackPath;
	void* pData;
	uint64_t iSize;
	int iSchemeLen;

	if ( (sURI == NULL) || (ppData == NULL) || (pSize == NULL) || (pUser == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pProvider = (xge_xpack_provider_t*)pUser;
	if ( (pProvider->pPack == NULL) || (pProvider->read == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iSchemeLen = __xgeUriSchemeLen(sURI);
	sPath = sURI;
	if ( iSchemeLen > 0 ) {
		sPath += iSchemeLen + 3;
	}
	sPackPath = __xgeResourceXPackMakePath(pProvider->sRoot, sPath);
	if ( sPackPath == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iSize = 0;
	pData = pProvider->read(pProvider->pPack, sPackPath, &iSize, pProvider->pUser);
	xrtFree(sPackPath);
	if ( pData == NULL ) {
		return XGE_ERROR_FILE_NOT_FOUND;
	}
	if ( iSize > (uint64_t)INT32_MAX ) {
		if ( pProvider->free != NULL ) {
			pProvider->free(pData, pProvider->pUser);
		} else {
			xrtFree(pData);
		}
		return XGE_ERROR_RESOURCE_FAILED;
	}
	*ppData = pData;
	*pSize = (int)iSize;
	return XGE_OK;
}

static void __xgeResourceXPackFree(void* pData, void* pUser)
{
	xge_xpack_provider_t* pProvider;

	if ( pData == NULL ) {
		return;
	}
	pProvider = (xge_xpack_provider_t*)pUser;
	if ( (pProvider != NULL) && (pProvider->free != NULL) ) {
		pProvider->free(pData, pProvider->pUser);
	} else {
		xrtFree(pData);
	}
}

int xgeResourceProviderAdd(const xge_resource_provider_t* pProvider)
{
	if ( (pProvider == NULL) || (pProvider->sScheme == NULL) || (pProvider->load == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( g_xge.iResourceProviderCount >= XGE_RESOURCE_PROVIDER_MAX ) {
		return XGE_ERROR_RESOURCE_FAILED;
	}
	g_xge.arrResourceProviders[g_xge.iResourceProviderCount] = *pProvider;
	g_xge.iResourceProviderCount++;
	return XGE_OK;
}

void xgeResourceProviderClear(void)
{
	memset(g_xge.arrResourceProviders, 0, sizeof(g_xge.arrResourceProviders));
	g_xge.iResourceProviderCount = 0;
	g_xge.iXPackProviderCount = 0;
}

int xgeResourceXPackProviderAdd(const xge_xpack_provider_t* pProvider)
{
	xge_resource_provider_t tProvider;
	int iIndex;

	if ( (pProvider == NULL) || (pProvider->pPack == NULL) || (pProvider->read == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( g_xge.iXPackProviderCount >= XGE_XPACK_PROVIDER_MAX ) {
		return XGE_ERROR_RESOURCE_FAILED;
	}
	iIndex = g_xge.iXPackProviderCount;
	g_xge.arrXPackProviders[iIndex] = *pProvider;
	memset(&tProvider, 0, sizeof(tProvider));
	tProvider.sScheme = "res";
	tProvider.load = __xgeResourceXPackLoad;
	tProvider.free = __xgeResourceXPackFree;
	tProvider.pUser = &g_xge.arrXPackProviders[iIndex];
	if ( xgeResourceProviderAdd(&tProvider) != XGE_OK ) {
		memset(&g_xge.arrXPackProviders[iIndex], 0, sizeof(g_xge.arrXPackProviders[iIndex]));
		return XGE_ERROR_RESOURCE_FAILED;
	}
	g_xge.iXPackProviderCount++;
	return XGE_OK;
}

int xgeResourceLoad(const char* sURI, xge_resource_t* pResource)
{
	int iSchemeLen;
	int iProvider;
	int iRet;
	size_t iSize;
	void* pData;

	if ( (sURI == NULL) || (pResource == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(pResource, 0, sizeof(*pResource));

	iSchemeLen = __xgeUriSchemeLen(sURI);
	if ( iSchemeLen > 0 ) {
		iProvider = __xgeResourceFindProvider(sURI, iSchemeLen);
		if ( iProvider >= 0 ) {
			iRet = g_xge.arrResourceProviders[iProvider].load(sURI, &pResource->pData, &pResource->iSize, g_xge.arrResourceProviders[iProvider].pUser);
			if ( iRet != XGE_OK ) {
				memset(pResource, 0, sizeof(*pResource));
				return iRet;
			}
			pResource->iProvider = iProvider + 1;
			pResource->free = g_xge.arrResourceProviders[iProvider].free;
			pResource->pUser = g_xge.arrResourceProviders[iProvider].pUser;
			return XGE_OK;
		}
		if ( __xgeSchemeEqual(sURI, iSchemeLen, "res") ) {
			sURI += iSchemeLen + 3;
		} else if ( __xgeSchemeEqual(sURI, iSchemeLen, "file") ) {
			sURI += iSchemeLen + 3;
		} else {
			__xgeLogFormat(XGE_LOG_WARN, "resource", "unsupported resource scheme: %s", sURI);
			return XGE_ERROR_UNSUPPORTED;
		}
	}

	pData = __xgeFileGetAll(sURI, &iSize);
	if ( pData == NULL ) {
		__xgeLogFormat(XGE_LOG_WARN, "resource", "resource file not found: %s", sURI);
		return XGE_ERROR_FILE_NOT_FOUND;
	}
	if ( iSize > (size_t)INT32_MAX ) {
		xrtFree(pData);
		__xgeLogFormat(XGE_LOG_ERROR, "resource", "resource file too large: %s", sURI);
		return XGE_ERROR_RESOURCE_FAILED;
	}
	pResource->pData = pData;
	pResource->iSize = (int)iSize;
	pResource->iProvider = 0;
	return XGE_OK;
}

int xgeResourceLoadMemory(const void* pData, int iSize, xge_resource_t* pResource)
{
	void* pCopy;

	if ( (pData == NULL) || (iSize < 0) || (pResource == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(pResource, 0, sizeof(*pResource));
	if ( iSize == 0 ) {
		return XGE_OK;
	}
	pCopy = xrtMalloc((size_t)iSize);
	if ( pCopy == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	memcpy(pCopy, pData, (size_t)iSize);
	pResource->pData = pCopy;
	pResource->iSize = iSize;
	pResource->iProvider = 0;
	return XGE_OK;
}

void xgeResourceFree(xge_resource_t* pResource)
{
	if ( (pResource == NULL) || (pResource->pData == NULL) ) {
		return;
	}
	if ( pResource->free != NULL ) {
		pResource->free(pResource->pData, pResource->pUser);
	} else {
		xrtFree(pResource->pData);
	}
	memset(pResource, 0, sizeof(*pResource));
}
