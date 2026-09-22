#include <xrt/ssh_fingerprint.h>

#include <xrt/codec.h>
#include <xrt/crypto.h>

#include <string.h>



#if defined(XSSH_FEATURE_FINGERPRINT)

/* 先写局部摘要，确保失败不会发布部分结果。 */
xsshcode xrtSshHostKeyDigestSha256(
	xbytesview HostKey,
	void* pDigest
)
{
	unsigned char arrDigest[XSSH_FINGERPRINT_SHA256_SIZE];

	if ( !xrtMemRangeValid(HostKey.Data, HostKey.Size) ||
		!xrtMemRangeValid(pDigest, sizeof(arrDigest)) ||
		xrtMemRangesOverlap(
			HostKey.Data,
			HostKey.Size,
			pDigest,
			sizeof(arrDigest)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !xrtSha256(HostKey.Data, HostKey.Size, arrDigest) ) {
		xrtSecureZero(arrDigest, sizeof(arrDigest));
		return XSSH_ERROR_STATE;
	}
	memcpy(pDigest, arrDigest, sizeof(arrDigest));
	xrtSecureZero(arrDigest, sizeof(arrDigest));
	return XSSH_OK;
}



/* 复用 XRT 无填充 Base64，生成 OpenSSH 标准展示格式。 */
xsshcode xrtSshHostKeyFingerprintSha256(
	xbytesview HostKey,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	static const char sPrefix[] = "SHA256:";
	unsigned char arrDigest[XSSH_FINGERPRINT_SHA256_SIZE];
	char sBase64[48];
	xbase64config Config = { NULL, XBASE64_NO_PADDING };
	size_t iBase64Size;
	size_t iRequired;
	xsshcode Code;

	if ( !xrtMemRangeValid(HostKey.Data, HostKey.Size) ||
		!xrtMemRangeValid(sOutput, iCapacity) ||
		!xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		xrtMemRangesOverlap(
			HostKey.Data,
			HostKey.Size,
			pOutputSize,
			sizeof(*pOutputSize)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshHostKeyDigestSha256(HostKey, arrDigest);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtBase64Encode(
		arrDigest,
		sizeof(arrDigest),
		sBase64,
		sizeof(sBase64),
		&iBase64Size,
		&Config
	) ) {
		xrtSecureZero(arrDigest, sizeof(arrDigest));
		return XSSH_ERROR_STATE;
	}
	iRequired = (sizeof(sPrefix) - 1u) + iBase64Size;
	if ( sOutput == NULL ) {
		*pOutputSize = iRequired;
		xrtSecureZero(arrDigest, sizeof(arrDigest));
		return XSSH_OK;
	}
	if ( (iCapacity <= iRequired) ||
		xrtMemRangesOverlap(
			sOutput,
			iRequired + 1u,
			HostKey.Data,
			HostKey.Size
		) || xrtMemRangesOverlap(
			sOutput,
			iRequired + 1u,
			pOutputSize,
			sizeof(*pOutputSize)
		) ) {
		xrtSecureZero(arrDigest, sizeof(arrDigest));
		return iCapacity <= iRequired ?
			XSSH_ERROR_SPACE : XSSH_ERROR_ARGUMENT;
	}
	memcpy(sOutput, sPrefix, sizeof(sPrefix) - 1u);
	memcpy(sOutput + sizeof(sPrefix) - 1u, sBase64, iBase64Size);
	sOutput[iRequired] = '\0';
	*pOutputSize = iRequired;
	xrtSecureZero(arrDigest, sizeof(arrDigest));
	return XSSH_OK;
}

#endif
