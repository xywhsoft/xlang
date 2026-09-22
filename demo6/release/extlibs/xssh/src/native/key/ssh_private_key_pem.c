#include <xrt/ssh_private_key_pem.h>



#if defined(XSSH_FEATURE_PRIVATE_KEY_PEM)

/* 复用 XRT PEM 解码，并让结构化输出只在完整容器成功后发布。 */
xsshcode xrtSshPrivateKeyPemRead(
	xstrview Text,
	void* pBinary,
	size_t iCapacity,
	size_t* pBinarySize,
	xsshopensshprivatekey* pPrivateKey
)
{
	xpemblock Block;
	xsshopensshprivatekey PrivateKey;
	size_t iRequired;
	size_t iDecoded;
	xsshcode Code;

	if ( !xrtMemRangeValid(Text.Data, Text.Size) ||
		!xrtMemRangeValid(pBinarySize, sizeof(*pBinarySize)) ||
		xrtMemRangesOverlap(
			Text.Data,
			Text.Size,
			pBinarySize,
			sizeof(*pBinarySize)
		) || ((pBinary == NULL) &&
		 ((iCapacity != 0u) || (pPrivateKey != NULL))) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !xrtPemFind(
		Text.Data,
		Text.Size,
		XSSH_PRIVATE_KEY_PEM_LABEL,
		&Block
	) || !xrtPemDecode(&Block, NULL, 0u, &iRequired) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	if ( pBinary == NULL ) {
		*pBinarySize = iRequired;
		return XSSH_OK;
	}
	if ( !xrtMemRangeValid(pBinary, iCapacity) ||
		!xrtMemRangeValid(pPrivateKey, sizeof(*pPrivateKey)) ||
		xrtMemRangesOverlap(
			pBinary,
			iCapacity,
			Text.Data,
			Text.Size
		) || xrtMemRangesOverlap(
			pBinary,
			iCapacity,
			pBinarySize,
			sizeof(*pBinarySize)
		) || xrtMemRangesOverlap(
			pBinary,
			iCapacity,
			pPrivateKey,
			sizeof(*pPrivateKey)
		) || xrtMemRangesOverlap(
			Text.Data,
			Text.Size,
			pPrivateKey,
			sizeof(*pPrivateKey)
		) || xrtMemRangesOverlap(
			pBinarySize,
			sizeof(*pBinarySize),
			pPrivateKey,
			sizeof(*pPrivateKey)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( iCapacity < iRequired ) {
		return XSSH_ERROR_SPACE;
	}
	if ( !xrtPemDecode(
		&Block,
		pBinary,
		iCapacity,
		&iDecoded
	) || (iDecoded != iRequired) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshPrivateKeyRead(
		(xbytesview){ (const unsigned char*)pBinary, iDecoded },
		&PrivateKey
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pPrivateKey = PrivateKey;
	*pBinarySize = iDecoded;
	return XSSH_OK;
}

#endif
