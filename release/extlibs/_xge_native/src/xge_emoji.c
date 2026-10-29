#define XGE_EMOJI_PACKAGE_VERSION 1u
#define XGE_EMOJI_PACKAGE_HEADER_SIZE 68u
#define XGE_EMOJI_PACKAGE_ENTRY_SIZE 24u
#define XGE_EMOJI_CACHE_MAX_COUNT 256
#define XGE_EMOJI_CACHE_MAX_SOURCE_BYTES (16u * 1024u * 1024u)
#define XGE_EMOJI_MAX_SVG_SIZE (1024u * 1024u)

typedef struct xge_emoji_entry_t {
	char* sSequence;
	int iSequenceSize;
	unsigned char* pSvgData;
	int iSvgSize;
	const unsigned char* pCompressedData;
	int iCompressedSize;
	xge_emoji_metrics_t tMetrics;
	xge_svg pSvg;
	uint64_t iCacheStamp;
	int bOwnSequence;
	int bOwnSvgData;
	volatile int bLoadFailed;
} xge_emoji_entry_t;

typedef struct xge_emoji_trie_node_t {
	uint32_t iCodepoint;
	int iChild;
	int iSibling;
	int iEntry;
	uint32_t iFlags;
} xge_emoji_trie_node_t;

struct xge_emoji_pack_t {
	volatile int iRefCount;
	xge_emoji_entry_t* pEntries;
	int iEntryCount;
	int iEntryCapacity;
	xge_emoji_trie_node_t* pNodes;
	int iNodeCount;
	int iNodeCapacity;
	const unsigned char* pDictionary;
	int iDictionarySize;
	void* pZstdContext;
	void* pZstdDictionary;
	xmutex* pCacheMutex;
	uint64_t iCacheClock;
	int iCachedSvgCount;
	uint64_t iCachedSourceBytes;
	int bBuiltin;
};

#include "xge_builtin_emoji.inc"

static xge_emoji_pack g_xgeBuiltinEmojiPack;
static xge_emoji_pack g_xgeDefaultEmojiPack;
static xspinlock g_xgeEmojiGlobalSpin = XRT_SPIN_INIT;

static void __xgeEmojiGlobalLock(void)
{
	(void)xrtSpinLock(&g_xgeEmojiGlobalSpin);
}

static void __xgeEmojiGlobalUnlock(void)
{
	(void)xrtSpinUnlock(&g_xgeEmojiGlobalSpin);
}

static uint32_t __xgeEmojiReadU32(const unsigned char* pData)
{
	return (uint32_t)pData[0] |
	       ((uint32_t)pData[1] << 8) |
	       ((uint32_t)pData[2] << 16) |
	       ((uint32_t)pData[3] << 24);
}

static int __xgeEmojiRangeInside(uint32_t iOffset, uint64_t iLength, size_t iSize)
{
	return ((uint64_t)iOffset <= (uint64_t)iSize) &&
	       (iLength <= (uint64_t)iSize - (uint64_t)iOffset);
}

static uint32_t __xgeEmojiCrc32(const void* pData, size_t iSize)
{
	const unsigned char* pScan;
	uint32_t iCrc;
	size_t i;
	int j;

	pScan = (const unsigned char*)pData;
	iCrc = 0xffffffffu;
	for ( i = 0; i < iSize; i++ ) {
		iCrc ^= pScan[i];
		for ( j = 0; j < 8; j++ ) {
			iCrc = (iCrc >> 1) ^ (0xedb88320u & (uint32_t)-(int)(iCrc & 1u));
		}
	}
	return iCrc ^ 0xffffffffu;
}

static int __xgeEmojiUTF8Decode(const char** ppScan, const char* sEnd, uint32_t* pCodepoint)
{
	const unsigned char* s;
	uint32_t c;
	int iLength;
	int i;

	if ( (ppScan == NULL) || (*ppScan == NULL) || (pCodepoint == NULL) || (*ppScan >= sEnd) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	s = (const unsigned char*)*ppScan;
	if ( s[0] < 0x80u ) {
		*pCodepoint = s[0];
		*ppScan += 1;
		return XGE_OK;
	}
	if ( (s[0] & 0xe0u) == 0xc0u ) {
		c = s[0] & 0x1fu;
		iLength = 2;
	} else if ( (s[0] & 0xf0u) == 0xe0u ) {
		c = s[0] & 0x0fu;
		iLength = 3;
	} else if ( (s[0] & 0xf8u) == 0xf0u ) {
		c = s[0] & 0x07u;
		iLength = 4;
	} else {
		return XGE_ERROR_RESOURCE_FAILED;
	}
	if ( *ppScan + iLength > sEnd ) return XGE_ERROR_RESOURCE_FAILED;
	for ( i = 1; i < iLength; i++ ) {
		if ( (s[i] & 0xc0u) != 0x80u ) return XGE_ERROR_RESOURCE_FAILED;
		c = (c << 6) | (uint32_t)(s[i] & 0x3fu);
	}
	if ( ((iLength == 2) && (c < 0x80u)) ||
	     ((iLength == 3) && (c < 0x800u)) ||
	     ((iLength == 4) && (c < 0x10000u)) ||
	     (c > 0x10ffffu) || ((c >= 0xd800u) && (c <= 0xdfffu)) ) {
		return XGE_ERROR_RESOURCE_FAILED;
	}
	*pCodepoint = c;
	*ppScan += iLength;
	return XGE_OK;
}

static int __xgeEmojiEnsureEntries(xge_emoji_pack pPack, int iRequired)
{
	xge_emoji_entry_t* pEntries;
	int iCapacity;

	if ( iRequired <= pPack->iEntryCapacity ) return XGE_OK;
	iCapacity = (pPack->iEntryCapacity > 0) ? pPack->iEntryCapacity * 2 : 16;
	while ( iCapacity < iRequired ) iCapacity *= 2;
	pEntries = (xge_emoji_entry_t*)xrtRealloc(pPack->pEntries, sizeof(*pEntries) * (size_t)iCapacity);
	if ( pEntries == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
	memset(pEntries + pPack->iEntryCapacity, 0, sizeof(*pEntries) * (size_t)(iCapacity - pPack->iEntryCapacity));
	pPack->pEntries = pEntries;
	pPack->iEntryCapacity = iCapacity;
	return XGE_OK;
}

static int __xgeEmojiEnsureNodes(xge_emoji_pack pPack, int iRequired)
{
	xge_emoji_trie_node_t* pNodes;
	int iCapacity;

	if ( iRequired <= pPack->iNodeCapacity ) return XGE_OK;
	iCapacity = (pPack->iNodeCapacity > 0) ? pPack->iNodeCapacity * 2 : 32;
	while ( iCapacity < iRequired ) iCapacity *= 2;
	pNodes = (xge_emoji_trie_node_t*)xrtRealloc(pPack->pNodes, sizeof(*pNodes) * (size_t)iCapacity);
	if ( pNodes == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
	memset(pNodes + pPack->iNodeCapacity, 0, sizeof(*pNodes) * (size_t)(iCapacity - pPack->iNodeCapacity));
	pPack->pNodes = pNodes;
	pPack->iNodeCapacity = iCapacity;
	return XGE_OK;
}

static int __xgeEmojiFindChild(xge_emoji_pack pPack, int iParent, uint32_t iCodepoint)
{
	int iNode;

	iNode = pPack->pNodes[iParent].iChild;
	while ( iNode >= 0 ) {
		if ( pPack->pNodes[iNode].iCodepoint == iCodepoint ) return iNode;
		iNode = pPack->pNodes[iNode].iSibling;
	}
	return -1;
}

static int __xgeEmojiTrieRegister(xge_emoji_pack pPack, const char* sSequence, int iSequenceSize,
	int iEntry, uint32_t iFlags, int bReplace)
{
	const char* sScan;
	const char* sEnd;
	uint32_t iCodepoint;
	int iCodepointCount;
	int iParent;
	int iNode;
	int iRet;

	if ( (sSequence == NULL) || (iSequenceSize <= 0) ||
	     (iEntry < 0) || (iEntry >= pPack->iEntryCount) ) return XGE_ERROR_INVALID_ARGUMENT;
	sScan = sSequence;
	sEnd = sSequence + iSequenceSize;
	iCodepointCount = 0;
	while ( sScan < sEnd ) {
		if ( __xgeEmojiUTF8Decode(&sScan, sEnd, &iCodepoint) != XGE_OK ) return XGE_ERROR_RESOURCE_FAILED;
		iCodepointCount++;
	}
	iRet = __xgeEmojiEnsureNodes(pPack, pPack->iNodeCount + iCodepointCount);
	if ( iRet != XGE_OK ) return iRet;
	iParent = 0;
	sScan = sSequence;
	while ( sScan < sEnd ) {
		(void)__xgeEmojiUTF8Decode(&sScan, sEnd, &iCodepoint);
		iNode = __xgeEmojiFindChild(pPack, iParent, iCodepoint);
		if ( iNode < 0 ) {
			iNode = pPack->iNodeCount++;
			pPack->pNodes[iNode].iCodepoint = iCodepoint;
			pPack->pNodes[iNode].iChild = -1;
			pPack->pNodes[iNode].iSibling = pPack->pNodes[iParent].iChild;
			pPack->pNodes[iNode].iEntry = -1;
			pPack->pNodes[iParent].iChild = iNode;
		}
		iParent = iNode;
	}
	if ( bReplace || (pPack->pNodes[iParent].iEntry < 0) ) {
		pPack->pNodes[iParent].iEntry = iEntry;
		pPack->pNodes[iParent].iFlags = iFlags;
	}
	return XGE_OK;
}

static int __xgeEmojiTrieRegisterTextAlias(xge_emoji_pack pPack, const char* sSequence,
	int iSequenceSize, int iEntry)
{
	char* sAlias;
	const char* sScan;
	const char* sBefore;
	const char* sEnd;
	uint32_t iCodepoint;
	int iAliasSize;
	int bRemoved;
	int iRet;

	sAlias = (char*)xrtMalloc((size_t)iSequenceSize);
	if ( sAlias == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
	sScan = sSequence;
	sEnd = sSequence + iSequenceSize;
	iAliasSize = 0;
	bRemoved = 0;
	while ( sScan < sEnd ) {
		sBefore = sScan;
		if ( __xgeEmojiUTF8Decode(&sScan, sEnd, &iCodepoint) != XGE_OK ) {
			xrtFree(sAlias);
			return XGE_ERROR_RESOURCE_FAILED;
		}
		if ( iCodepoint == 0xfe0fu ) {
			bRemoved = 1;
			continue;
		}
		memcpy(sAlias + iAliasSize, sBefore, (size_t)(sScan - sBefore));
		iAliasSize += (int)(sScan - sBefore);
	}
	iRet = XGE_OK;
	if ( bRemoved && (iAliasSize > 0) ) {
		iRet = __xgeEmojiTrieRegister(
			pPack, sAlias, iAliasSize, iEntry, XGE_EMOJI_ENTRY_TEXT_DEFAULT, 0
		);
	}
	xrtFree(sAlias);
	return iRet;
}

static xge_emoji_metrics_t __xgeEmojiMetricsDefault(void)
{
	xge_emoji_metrics_t tMetrics;

	memset(&tMetrics, 0, sizeof(tMetrics));
	tMetrics.iSize = sizeof(tMetrics);
	tMetrics.fAdvanceEm = 1.0f;
	tMetrics.fWidthEm = 1.0f;
	tMetrics.fHeightEm = 1.0f;
	tMetrics.fBaselineRatio = 0.82f;
	return tMetrics;
}

int xgeEmojiPackCreate(xge_emoji_pack* ppPack)
{
	xge_emoji_pack pPack;

	if ( ppPack == NULL ) return XGE_ERROR_INVALID_ARGUMENT;
	*ppPack = NULL;
	pPack = (xge_emoji_pack)xrtCalloc(1, sizeof(*pPack));
	if ( pPack == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
	if ( __xgeEmojiEnsureNodes(pPack, 1) != XGE_OK ) {
		xrtFree(pPack);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pPack->pCacheMutex = xrtMutexCreate();
	if ( pPack->pCacheMutex == NULL ) {
		xrtFree(pPack->pNodes);
		xrtFree(pPack);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pPack->iRefCount = 1;
	pPack->iNodeCount = 1;
	pPack->pNodes[0].iChild = -1;
	pPack->pNodes[0].iSibling = -1;
	pPack->pNodes[0].iEntry = -1;
	*ppPack = pPack;
	return XGE_OK;
}

int xgeEmojiPackAddRef(xge_emoji_pack pPack)
{
	if ( (pPack == NULL) || (pPack->iRefCount <= 0) ) return XGE_ERROR_INVALID_ARGUMENT;
	return (int)xrtRefRetain((volatile int32*)&pPack->iRefCount);
}

void xgeEmojiPackFree(xge_emoji_pack pPack)
{
	int i;

	if ( (pPack == NULL) || (pPack->iRefCount <= 0) ) return;
	if ( xrtRefRelease((volatile int32*)&pPack->iRefCount) > 0 ) return;
	xrtMutexLock(pPack->pCacheMutex);
	for ( i = 0; i < pPack->iEntryCount; i++ ) {
		if ( pPack->pEntries[i].bOwnSequence ) xrtFree(pPack->pEntries[i].sSequence);
		if ( pPack->pEntries[i].bOwnSvgData ) xrtFree(pPack->pEntries[i].pSvgData);
		xgeSvgDestroy(pPack->pEntries[i].pSvg);
	}
	ZSTD_freeDDict((ZSTD_DDict*)pPack->pZstdDictionary);
	ZSTD_freeDCtx((ZSTD_DCtx*)pPack->pZstdContext);
	xrtMutexUnlock(pPack->pCacheMutex);
	xrtMutexDestroy(pPack->pCacheMutex);
	xrtFree(pPack->pEntries);
	xrtFree(pPack->pNodes);
	xrtFree(pPack);
}

int xgeEmojiPackAddSvgMemory(xge_emoji_pack pPack, const char* sSequence, const void* pSvgData,
	int iSvgSize, const xge_emoji_metrics_t* pMetrics, uint32_t* pEmojiId)
{
	xge_emoji_metrics_t tMetrics;
	xge_emoji_entry_t* pEntry;
	unsigned char* pSvgCopy;
	char* sSequenceCopy;
	int iEntry;
	int iExistingNode;
	int iRet;

	if ( (pPack == NULL) || pPack->bBuiltin || (sSequence == NULL) || (*sSequence == 0) ||
	     (pSvgData == NULL) || (iSvgSize <= 0) ) return XGE_ERROR_INVALID_ARGUMENT;
	tMetrics = __xgeEmojiMetricsDefault();
	if ( pMetrics != NULL ) {
		if ( (pMetrics->iSize != 0) && (pMetrics->iSize < sizeof(*pMetrics)) ) return XGE_ERROR_INVALID_ARGUMENT;
		tMetrics = *pMetrics;
		tMetrics.iSize = sizeof(tMetrics);
		if ( !(tMetrics.fAdvanceEm > 0.0f) ) tMetrics.fAdvanceEm = 1.0f;
		if ( !(tMetrics.fWidthEm > 0.0f) ) tMetrics.fWidthEm = 1.0f;
		if ( !(tMetrics.fHeightEm > 0.0f) ) tMetrics.fHeightEm = 1.0f;
		if ( !(tMetrics.fBaselineRatio > 0.0f) || !(tMetrics.fBaselineRatio < 1.0f) ) tMetrics.fBaselineRatio = 0.82f;
	}
	pSvgCopy = (unsigned char*)xrtMalloc((size_t)iSvgSize);
	sSequenceCopy = (char*)xrtMalloc(strlen(sSequence) + 1u);
	if ( (pSvgCopy == NULL) || (sSequenceCopy == NULL) ) {
		xrtFree(pSvgCopy);
		xrtFree(sSequenceCopy);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	memcpy(pSvgCopy, pSvgData, (size_t)iSvgSize);
	memcpy(sSequenceCopy, sSequence, strlen(sSequence) + 1u);

	iExistingNode = 0;
	{
		const char* sScan = sSequence;
		const char* sEnd = sSequence + strlen(sSequence);
		uint32_t iCodepoint;
		while ( (sScan < sEnd) && (iExistingNode >= 0) ) {
			if ( __xgeEmojiUTF8Decode(&sScan, sEnd, &iCodepoint) != XGE_OK ) {
				xrtFree(pSvgCopy);
				xrtFree(sSequenceCopy);
				return XGE_ERROR_RESOURCE_FAILED;
			}
			iExistingNode = __xgeEmojiFindChild(pPack, iExistingNode, iCodepoint);
		}
	}
	iEntry = (iExistingNode >= 0) ? pPack->pNodes[iExistingNode].iEntry : -1;
	if ( iEntry < 0 ) {
		iRet = __xgeEmojiEnsureEntries(pPack, pPack->iEntryCount + 1);
		if ( iRet != XGE_OK ) {
			xrtFree(pSvgCopy);
			xrtFree(sSequenceCopy);
			return iRet;
		}
		iEntry = pPack->iEntryCount++;
	} else {
		pEntry = &pPack->pEntries[iEntry];
		if ( pEntry->bOwnSequence ) xrtFree(pEntry->sSequence);
		if ( pEntry->bOwnSvgData ) xrtFree(pEntry->pSvgData);
		xgeSvgDestroy(pEntry->pSvg);
		memset(pEntry, 0, sizeof(*pEntry));
	}
	pEntry = &pPack->pEntries[iEntry];
	pEntry->sSequence = sSequenceCopy;
	pEntry->iSequenceSize = (int)strlen(sSequence);
	pEntry->pSvgData = pSvgCopy;
	pEntry->iSvgSize = iSvgSize;
	pEntry->tMetrics = tMetrics;
	pEntry->bOwnSequence = 1;
	pEntry->bOwnSvgData = 1;
	iRet = __xgeEmojiTrieRegister(
		pPack, sSequence, (int)strlen(sSequence), iEntry, tMetrics.iFlags, 1
	);
	if ( iRet != XGE_OK ) return iRet;
	if ( pEmojiId != NULL ) *pEmojiId = (uint32_t)iEntry + 1u;
	return XGE_OK;
}

static int __xgeEmojiMatchPath(xge_emoji_pack pPack, const char* sText, const char* sEnd,
	int bSkipSkin, int* pEntry, const char** ppMatchEnd, uint32_t* pFlags)
{
	const char* sScan;
	const char* sBestEnd;
	uint32_t iCodepoint;
	uint32_t iBestFlags;
	int iNode;
	int iBestEntry;

	sScan = sText;
	sBestEnd = NULL;
	iNode = 0;
	iBestEntry = -1;
	iBestFlags = 0;
	while ( sScan < sEnd ) {
		const char* sBefore = sScan;
		if ( __xgeEmojiUTF8Decode(&sScan, sEnd, &iCodepoint) != XGE_OK ) break;
		if ( bSkipSkin && (iCodepoint >= 0x1f3fbu) && (iCodepoint <= 0x1f3ffu) ) {
			if ( iBestEntry >= 0 ) sBestEnd = sScan;
			continue;
		}
		iNode = __xgeEmojiFindChild(pPack, iNode, iCodepoint);
		if ( iNode < 0 ) {
			sScan = sBefore;
			break;
		}
		if ( pPack->pNodes[iNode].iEntry >= 0 ) {
			iBestEntry = pPack->pNodes[iNode].iEntry;
			iBestFlags = pPack->pNodes[iNode].iFlags;
			sBestEnd = sScan;
		}
	}
	if ( iBestEntry < 0 ) return XGE_ERROR_NOT_FOUND;
	*pEntry = iBestEntry;
	*ppMatchEnd = sBestEnd;
	*pFlags = iBestFlags;
	return XGE_OK;
}

int xgeEmojiPackMatch(xge_emoji_pack pPack, const char* sText, int iTextSize, xge_emoji_match_t* pMatch)
{
	const char* sEnd;
	const char* sExactEnd;
	const char* sNormalizedEnd;
	const char* sBestEnd;
	uint32_t iExactFlags;
	uint32_t iNormalizedFlags;
	uint32_t iBestFlags;
	int iExactEntry;
	int iNormalizedEntry;
	int iBestEntry;
	int iExactRet;
	int iNormalizedRet;

	if ( (pPack == NULL) || (sText == NULL) || (pMatch == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( (pMatch->iSize != 0) && (pMatch->iSize < sizeof(*pMatch)) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( iTextSize < 0 ) iTextSize = (int)strlen(sText);
	if ( iTextSize <= 0 ) return XGE_ERROR_NOT_FOUND;
	sEnd = sText + iTextSize;
	sExactEnd = NULL;
	sNormalizedEnd = NULL;
	iExactRet = __xgeEmojiMatchPath(
		pPack, sText, sEnd, 0, &iExactEntry, &sExactEnd, &iExactFlags
	);
	iNormalizedRet = __xgeEmojiMatchPath(
		pPack, sText, sEnd, 1, &iNormalizedEntry, &sNormalizedEnd, &iNormalizedFlags
	);
	if ( (iExactRet != XGE_OK) && (iNormalizedRet != XGE_OK) ) return XGE_ERROR_NOT_FOUND;
	if ( (iNormalizedRet == XGE_OK) &&
	     ((iExactRet != XGE_OK) || (sNormalizedEnd > sExactEnd)) ) {
		iBestEntry = iNormalizedEntry;
		sBestEnd = sNormalizedEnd;
		iBestFlags = iNormalizedFlags;
	} else {
		iBestEntry = iExactEntry;
		sBestEnd = sExactEnd;
		iBestFlags = iExactFlags;
	}
	memset(pMatch, 0, sizeof(*pMatch));
	pMatch->iSize = sizeof(*pMatch);
	pMatch->iEmojiId = (uint32_t)iBestEntry + 1u;
	pMatch->iTextSize = (int)(sBestEnd - sText);
	pMatch->tMetrics = pPack->pEntries[iBestEntry].tMetrics;
	pMatch->tMetrics.iFlags = iBestFlags;
	return XGE_OK;
}

static int __xgeEmojiBuiltinLoad(xge_emoji_pack* ppPack)
{
	static const unsigned char arrMagic[8] = {'X', 'G', 'E', 'M', 'O', 'J', 'I', 0};
	const unsigned char* pPackage;
	const unsigned char* pHeader;
	const unsigned char* pIndex;
	const unsigned char* pEntryData;
	size_t iPackageSize;
	uint32_t iVersion;
	uint32_t iHeaderSize;
	uint32_t iEntryCount;
	uint32_t iEntrySize;
	uint32_t iIndexOffset;
	uint32_t iSequenceOffset;
	uint32_t iSequenceSize;
	uint32_t iDictionaryOffset;
	uint32_t iDictionarySize;
	uint32_t iPayloadOffset;
	uint32_t iPayloadSize;
	uint32_t iPackageCrc;
	uint32_t i;
	xge_emoji_pack pPack;
	xge_emoji_entry_t* pEntry;
	int iRet;

	pPackage = g_xgeBuiltinEmojiPackage;
	iPackageSize = sizeof(g_xgeBuiltinEmojiPackage);
	if ( iPackageSize < XGE_EMOJI_PACKAGE_HEADER_SIZE || memcmp(pPackage, arrMagic, 8) != 0 ) {
		return XGE_ERROR_RESOURCE_FAILED;
	}
	pHeader = pPackage + 8;
	iVersion = __xgeEmojiReadU32(pHeader + 0);
	iHeaderSize = __xgeEmojiReadU32(pHeader + 4);
	iEntryCount = __xgeEmojiReadU32(pHeader + 8);
	iEntrySize = __xgeEmojiReadU32(pHeader + 12);
	iIndexOffset = __xgeEmojiReadU32(pHeader + 16);
	iSequenceOffset = __xgeEmojiReadU32(pHeader + 20);
	iSequenceSize = __xgeEmojiReadU32(pHeader + 24);
	iDictionaryOffset = __xgeEmojiReadU32(pHeader + 28);
	iDictionarySize = __xgeEmojiReadU32(pHeader + 32);
	iPayloadOffset = __xgeEmojiReadU32(pHeader + 36);
	iPayloadSize = __xgeEmojiReadU32(pHeader + 40);
	iPackageCrc = __xgeEmojiReadU32(pHeader + 44);
	if ( (iVersion != XGE_EMOJI_PACKAGE_VERSION) ||
	     (iHeaderSize != XGE_EMOJI_PACKAGE_HEADER_SIZE) ||
	     (iEntrySize != XGE_EMOJI_PACKAGE_ENTRY_SIZE) || (iEntryCount == 0) ||
	     (iIndexOffset != iHeaderSize) ||
	     (iSequenceOffset != iIndexOffset + iEntryCount * iEntrySize) ||
	     (iDictionaryOffset != iSequenceOffset + iSequenceSize) ||
	     (iPayloadOffset != iDictionaryOffset + iDictionarySize) ||
	     !__xgeEmojiRangeInside(iIndexOffset, (uint64_t)iEntryCount * iEntrySize, iPackageSize) ||
	     !__xgeEmojiRangeInside(iSequenceOffset, iSequenceSize, iPackageSize) ||
	     !__xgeEmojiRangeInside(iDictionaryOffset, iDictionarySize, iPackageSize) ||
	     !__xgeEmojiRangeInside(iPayloadOffset, iPayloadSize, iPackageSize) ||
	     ((uint64_t)iPayloadOffset + iPayloadSize != iPackageSize) ||
	     (__xgeEmojiCrc32(pPackage + iHeaderSize, iPackageSize - iHeaderSize) != iPackageCrc) ) {
		return XGE_ERROR_RESOURCE_FAILED;
	}
	iRet = xgeEmojiPackCreate(&pPack);
	if ( iRet != XGE_OK ) return iRet;
	pPack->bBuiltin = 1;
	pPack->pDictionary = pPackage + iDictionaryOffset;
	pPack->iDictionarySize = (int)iDictionarySize;
	pPack->pZstdContext = ZSTD_createDCtx();
	pPack->pZstdDictionary = ZSTD_createDDict(pPack->pDictionary, (size_t)pPack->iDictionarySize);
	if ( (pPack->pZstdContext == NULL) || (pPack->pZstdDictionary == NULL) ) {
		xgeEmojiPackFree(pPack);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iRet = __xgeEmojiEnsureEntries(pPack, (int)iEntryCount);
	if ( iRet != XGE_OK ) {
		xgeEmojiPackFree(pPack);
		return iRet;
	}
	pPack->iEntryCount = (int)iEntryCount;
	pIndex = pPackage + iIndexOffset;
	for ( i = 0; i < iEntryCount; i++ ) {
		uint32_t iSequenceRelative;
		uint32_t iSequenceLength;
		uint32_t iPayloadRelative;
		uint32_t iCompressedSize;
		uint32_t iSvgSize;
		uint32_t iFlags;

		pEntryData = pIndex + (size_t)i * iEntrySize;
		iSequenceRelative = __xgeEmojiReadU32(pEntryData + 0);
		iSequenceLength = __xgeEmojiReadU32(pEntryData + 4);
		iPayloadRelative = __xgeEmojiReadU32(pEntryData + 8);
		iCompressedSize = __xgeEmojiReadU32(pEntryData + 12);
		iSvgSize = __xgeEmojiReadU32(pEntryData + 16);
		iFlags = __xgeEmojiReadU32(pEntryData + 20);
		if ( (iSequenceLength == 0) || (iCompressedSize == 0) ||
		     (iSvgSize == 0) || (iSvgSize > XGE_EMOJI_MAX_SVG_SIZE) ||
		     ((uint64_t)iSequenceRelative + iSequenceLength > iSequenceSize) ||
		     ((uint64_t)iPayloadRelative + iCompressedSize > iPayloadSize) ) {
			xgeEmojiPackFree(pPack);
			return XGE_ERROR_RESOURCE_FAILED;
		}
		pEntry = &pPack->pEntries[i];
		pEntry->sSequence = (char*)(pPackage + iSequenceOffset + iSequenceRelative);
		pEntry->iSequenceSize = (int)iSequenceLength;
		pEntry->pCompressedData = pPackage + iPayloadOffset + iPayloadRelative;
		pEntry->iCompressedSize = (int)iCompressedSize;
		pEntry->iSvgSize = (int)iSvgSize;
		pEntry->tMetrics = __xgeEmojiMetricsDefault();
		pEntry->tMetrics.iFlags = iFlags;
		iRet = __xgeEmojiTrieRegister(
			pPack, pEntry->sSequence, pEntry->iSequenceSize, (int)i, iFlags, 1
		);
		if ( iRet == XGE_OK ) {
			iRet = __xgeEmojiTrieRegisterTextAlias(
				pPack, pEntry->sSequence, pEntry->iSequenceSize, (int)i
			);
		}
		if ( iRet != XGE_OK ) {
			xgeEmojiPackFree(pPack);
			return iRet;
		}
	}
	*ppPack = pPack;
	return XGE_OK;
}

int xgeEmojiPackLoadBuiltin(xge_emoji_pack* ppPack)
{
	int iRet;

	if ( ppPack == NULL ) return XGE_ERROR_INVALID_ARGUMENT;
	*ppPack = NULL;
	__xgeEmojiGlobalLock();
	if ( g_xgeBuiltinEmojiPack == NULL ) {
		iRet = __xgeEmojiBuiltinLoad(&g_xgeBuiltinEmojiPack);
		if ( iRet != XGE_OK ) {
			__xgeReportErrorInternal(iRet, 1, "emoji", "load built-in package",
				"package validation or initialization failed");
			__xgeEmojiGlobalUnlock();
			return iRet;
		}
	}
	iRet = xgeEmojiPackAddRef(g_xgeBuiltinEmojiPack);
	if ( iRet <= 0 ) {
		__xgeEmojiGlobalUnlock();
		return iRet;
	}
	*ppPack = g_xgeBuiltinEmojiPack;
	__xgeEmojiGlobalUnlock();
	return XGE_OK;
}

int xgeEmojiPackSetDefault(xge_emoji_pack pPack)
{
	xge_emoji_pack pOld;

	if ( pPack != NULL ) {
		if ( xgeEmojiPackAddRef(pPack) <= 0 ) return XGE_ERROR_INVALID_ARGUMENT;
	}
	__xgeEmojiGlobalLock();
	pOld = g_xgeDefaultEmojiPack;
	g_xgeDefaultEmojiPack = pPack;
	__xgeEmojiGlobalUnlock();
	xgeEmojiPackFree(pOld);
	return XGE_OK;
}

static xge_emoji_pack __xgeEmojiDefaultAcquire(void)
{
	xge_emoji_pack pPack;
	xge_emoji_pack pResult;

	__xgeEmojiGlobalLock();
	pResult = g_xgeDefaultEmojiPack;
	if ( pResult != NULL ) (void)xgeEmojiPackAddRef(pResult);
	__xgeEmojiGlobalUnlock();
	if ( pResult != NULL ) return pResult;
	pPack = NULL;
	if ( xgeEmojiPackLoadBuiltin(&pPack) != XGE_OK ) return NULL;
	__xgeEmojiGlobalLock();
	if ( g_xgeDefaultEmojiPack == NULL ) {
		g_xgeDefaultEmojiPack = pPack;
		pPack = NULL;
	}
	pResult = g_xgeDefaultEmojiPack;
	if ( pResult != NULL ) (void)xgeEmojiPackAddRef(pResult);
	__xgeEmojiGlobalUnlock();
	xgeEmojiPackFree(pPack);
	return pResult;
}

int xgeEmojiPackGetDefault(xge_emoji_pack* ppPack)
{
	xge_emoji_pack pPack;

	if ( ppPack == NULL ) return XGE_ERROR_INVALID_ARGUMENT;
	*ppPack = NULL;
	pPack = __xgeEmojiDefaultAcquire();
	if ( pPack == NULL ) return XGE_ERROR_RESOURCE_FAILED;
	*ppPack = pPack;
	return XGE_OK;
}

void xgeEmojiPackClearDefault(void)
{
	xge_emoji_pack pPack;

	__xgeEmojiGlobalLock();
	pPack = g_xgeDefaultEmojiPack;
	g_xgeDefaultEmojiPack = NULL;
	__xgeEmojiGlobalUnlock();
	xgeEmojiPackFree(pPack);
}

static void __xgeEmojiGlobalClear(void)
{
	xge_emoji_pack pDefault;
	xge_emoji_pack pBuiltin;

	__xgeEmojiGlobalLock();
	pDefault = g_xgeDefaultEmojiPack;
	pBuiltin = g_xgeBuiltinEmojiPack;
	g_xgeDefaultEmojiPack = NULL;
	g_xgeBuiltinEmojiPack = NULL;
	__xgeEmojiGlobalUnlock();
	xgeEmojiPackFree(pDefault);
	xgeEmojiPackFree(pBuiltin);
}

static int __xgeEmojiMayStart(uint32_t iCodepoint)
{
	return (iCodepoint == 0x23u) || (iCodepoint == 0x2au) ||
	       ((iCodepoint >= 0x30u) && (iCodepoint <= 0x39u)) ||
	       (iCodepoint == 0xa9u) || (iCodepoint == 0xaeu) ||
	       ((iCodepoint >= 0x2000u) && (iCodepoint <= 0x2bffu)) ||
	       (iCodepoint == 0x3030u) || (iCodepoint == 0x303du) ||
	       (iCodepoint == 0x3297u) || (iCodepoint == 0x3299u) ||
	       ((iCodepoint >= 0x1f000u) && (iCodepoint <= 0x1faffu));
}

static int __xgeEmojiMatchForText(xge_emoji_pack pPack, const char* sText, const char* sEnd,
	int iPresentation, xge_emoji_match_t* pMatch)
{
	xge_emoji_pack pOwnedPack;
	const char* sNext;
	uint32_t iNext;
	int iRet;

	if ( (iPresentation == XGE_EMOJI_PRESENTATION_TEXT) ||
	     (iPresentation == XGE_EMOJI_PRESENTATION_DISABLED) ) return XGE_ERROR_NOT_FOUND;
	pOwnedPack = NULL;
	if ( pPack == NULL ) {
		pOwnedPack = __xgeEmojiDefaultAcquire();
		pPack = pOwnedPack;
	}
	if ( pPack == NULL ) return XGE_ERROR_NOT_FOUND;
	memset(pMatch, 0, sizeof(*pMatch));
	pMatch->iSize = sizeof(*pMatch);
	iRet = xgeEmojiPackMatch(pPack, sText, (int)(sEnd - sText), pMatch);
	if ( iRet != XGE_OK ) {
		xgeEmojiPackFree(pOwnedPack);
		return iRet;
	}

	sNext = sText + pMatch->iTextSize;
	if ( sNext < sEnd ) {
		const char* sAfter = sNext;
		if ( __xgeEmojiUTF8Decode(&sAfter, sEnd, &iNext) == XGE_OK ) {
			if ( (iNext == 0xfe0eu) || (iNext == 0x200du) ) {
				xgeEmojiPackFree(pOwnedPack);
				return XGE_ERROR_NOT_FOUND;
			}
			if ( iNext == 0xfe0fu ) pMatch->iTextSize = (int)(sAfter - sText);
		}
	}
	if ( (iPresentation == XGE_EMOJI_PRESENTATION_AUTO) &&
	     ((pMatch->tMetrics.iFlags & XGE_EMOJI_ENTRY_TEXT_DEFAULT) != 0) ) {
		xgeEmojiPackFree(pOwnedPack);
		return XGE_ERROR_NOT_FOUND;
	}
	xgeEmojiPackFree(pOwnedPack);
	return XGE_OK;
}

static void __xgeEmojiResolveLayout(xge_font pFont, const xge_emoji_metrics_t* pMetrics,
	float fEmojiScale, int iLinePolicy, float* pAdvance, float* pWidth, float* pHeight,
	float* pAbove, float* pBelow)
{
	float fPixelSize;
	float fRatio;
	float fScale;
	float fFit;

	fPixelSize = (pFont->fSize > 0.0f) ? pFont->fSize : pFont->fLineHeight;
	fScale = (fEmojiScale > 0.0f) ? fEmojiScale : 1.0f;
	*pHeight = fPixelSize * fScale * pMetrics->fHeightEm;
	*pWidth = fPixelSize * fScale * pMetrics->fWidthEm;
	*pAdvance = fPixelSize * fScale * pMetrics->fAdvanceEm;
	fRatio = pMetrics->fBaselineRatio;
	*pAbove = *pHeight * fRatio;
	*pBelow = *pHeight - *pAbove;
	if ( iLinePolicy == XGE_EMOJI_LINE_STABLE ) {
		fFit = 1.0f;
		if ( (*pAbove > pFont->fAscent) && (*pAbove > 0.0f) ) fFit = pFont->fAscent / *pAbove;
		if ( (*pBelow > -pFont->fDescent) && (*pBelow > 0.0f) ) {
			float fDescentFit = -pFont->fDescent / *pBelow;
			if ( fDescentFit < fFit ) fFit = fDescentFit;
		}
		if ( fFit < 1.0f ) {
			*pHeight *= fFit;
			*pWidth *= fFit;
			*pAdvance *= fFit;
			*pAbove *= fFit;
			*pBelow *= fFit;
		}
	}
}

static void __xgeEmojiCacheTrim(xge_emoji_pack pPack, int iProtectedEntry)
{
	while ( (pPack->iCachedSvgCount > XGE_EMOJI_CACHE_MAX_COUNT) ||
	        (pPack->iCachedSourceBytes > XGE_EMOJI_CACHE_MAX_SOURCE_BYTES) ) {
		uint64_t iOldestStamp = UINT64_MAX;
		int iOldestEntry = -1;
		int i;

		for ( i = 0; i < pPack->iEntryCount; i++ ) {
			if ( (i != iProtectedEntry) && (pPack->pEntries[i].pSvg != NULL) &&
			     (pPack->pEntries[i].iCacheStamp < iOldestStamp) ) {
				iOldestStamp = pPack->pEntries[i].iCacheStamp;
				iOldestEntry = i;
			}
		}
		if ( iOldestEntry < 0 ) break;
		xgeSvgDestroy(pPack->pEntries[iOldestEntry].pSvg);
		pPack->pEntries[iOldestEntry].pSvg = NULL;
		pPack->pEntries[iOldestEntry].iCacheStamp = 0;
		pPack->iCachedSvgCount--;
		pPack->iCachedSourceBytes -= (uint64_t)pPack->pEntries[iOldestEntry].iSvgSize;
	}
}

static int __xgeEmojiEntryLoadLocked(xge_emoji_pack pPack, int iEntry)
{
	xge_emoji_entry_t* pEntry;
	unsigned char* pSvgData;
	xge_svg pSvg;
	size_t iDecoded;
	int iRet;

	pEntry = &pPack->pEntries[iEntry];
	if ( pEntry->pSvg != NULL ) {
		pEntry->iCacheStamp = ++pPack->iCacheClock;
		return XGE_OK;
	}
	if ( pEntry->bLoadFailed ) return XGE_ERROR_RESOURCE_FAILED;
	pSvgData = pEntry->pSvgData;
	if ( pEntry->pCompressedData != NULL ) {
		pSvgData = (unsigned char*)xrtMalloc((size_t)pEntry->iSvgSize);
		if ( pSvgData == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
		iDecoded = ZSTD_decompress_usingDDict(
			(ZSTD_DCtx*)pPack->pZstdContext, pSvgData, (size_t)pEntry->iSvgSize,
			pEntry->pCompressedData, (size_t)pEntry->iCompressedSize,
			(const ZSTD_DDict*)pPack->pZstdDictionary
		);
		if ( ZSTD_isError(iDecoded) || (iDecoded != (size_t)pEntry->iSvgSize) ) {
			xrtFree(pSvgData);
			pEntry->bLoadFailed = 1;
			__xgeReportErrorInternal(XGE_ERROR_RESOURCE_FAILED, 1, "emoji",
				"decompress SVG", "built-in Emoji entry is corrupt");
			return XGE_ERROR_RESOURCE_FAILED;
		}
	}
	pSvg = NULL;
	iRet = xgeSvgCreate(&pSvg);
	if ( iRet == XGE_OK ) iRet = xgeSvgLoadMemory(pSvg, pSvgData, pEntry->iSvgSize);
	if ( pEntry->pCompressedData != NULL ) xrtFree(pSvgData);
	if ( iRet != XGE_OK ) {
		xgeSvgDestroy(pSvg);
		pEntry->bLoadFailed = 1;
		__xgeReportErrorInternal(iRet, 1, "emoji", "parse SVG",
			"built-in Emoji SVG could not be parsed");
		return iRet;
	}
	pEntry->pSvg = pSvg;
	pEntry->iCacheStamp = ++pPack->iCacheClock;
	if ( pPack->bBuiltin ) {
		pPack->iCachedSvgCount++;
		pPack->iCachedSourceBytes += (uint64_t)pEntry->iSvgSize;
		__xgeEmojiCacheTrim(pPack, iEntry);
	}
	return XGE_OK;
}

static int __xgeEmojiDraw(xge_emoji_pack pPack, uint32_t iEmojiId, xge_rect_t tDst,
	int bScreenSpace)
{
	xge_svg pSvg;
	int iEntry;
	int iRet;

	if ( (pPack == NULL) || (iEmojiId == 0) || (iEmojiId > (uint32_t)pPack->iEntryCount) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iEntry = (int)iEmojiId - 1;
	pSvg = NULL;
	xrtMutexLock(pPack->pCacheMutex);
	iRet = __xgeEmojiEntryLoadLocked(pPack, iEntry);
	if ( iRet == XGE_OK ) {
		pSvg = pPack->pEntries[iEntry].pSvg;
		iRet = xgeSvgAddRef(pSvg);
	}
	xrtMutexUnlock(pPack->pCacheMutex);
	if ( iRet != XGE_OK ) return iRet;
	iRet = bScreenSpace ? xgeSvgDrawPx(pSvg, tDst, 0.35f) : xgeSvgDraw(pSvg, tDst, 0.35f);
	xgeSvgDestroy(pSvg);
	return iRet;
}
