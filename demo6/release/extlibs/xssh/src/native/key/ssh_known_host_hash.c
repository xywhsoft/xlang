#include <xrt/ssh_known_host_hash.h>

#include <xrt/codec.h>
#include <xrt/crypto.h>

#include "ssh_known_host_internal.h"

#include <string.h>



#if defined(XSSH_FEATURE_KNOWN_HOST_HASH)

/* 以 ASCII 小写追加主机名，不为任意长度输入建立临时副本。 */
static bool xsshKnownHostHashHostUpdate(
	xsha1* pState,
	xstrview Host
)
{
	size_t iStart = 0u;
	size_t i;

	for ( i = 0u; i < Host.Size; ++i ) {
		unsigned char iCharacter = (unsigned char)Host.Data[i];

		if ( (iCharacter >= (unsigned char)'A') &&
			(iCharacter <= (unsigned char)'Z') ) {
			unsigned char iLower =
				(unsigned char)(iCharacter + ('a' - 'A'));

			if ( !xrtSha1Update(
				pState,
				Host.Data + iStart,
				i - iStart
			) || !xrtSha1Update(pState, &iLower, 1u) ) {
				return false;
			}
			iStart = i + 1u;
		}
	}
	return xrtSha1Update(
		pState,
		Host.Data + iStart,
		Host.Size - iStart
	);
}



/* 向摘要追加虚拟 host 或 [host]:port。 */
static bool xsshKnownHostHashTargetUpdate(
	xsha1* pState,
	const xsshknownhosttarget* pTarget
)
{
	static const char sOpen[] = "[";
	static const char sClose[] = "]:";

	if ( !pTarget->Bracketed ) {
		return xsshKnownHostHashHostUpdate(pState, pTarget->Host);
	}
	return xrtSha1Update(pState, sOpen, sizeof(sOpen) - 1u) &&
		xsshKnownHostHashHostUpdate(pState, pTarget->Host) &&
		xrtSha1Update(pState, sClose, sizeof(sClose) - 1u) &&
		xrtSha1Update(pState, pTarget->Port, pTarget->PortSize);
}



/* 使用 XRT SHA-1 组合 OpenSSH 历史格式所需的固定 20 字节 HMAC。 */
static bool xsshKnownHostHmacSha1(
	const xsshknownhosttarget* pTarget,
	xbytesview Salt,
	unsigned char pHash[XSSH_KNOWN_HOST_HASH_SIZE]
)
{
	unsigned char arrInnerPad[XRT_SHA1_BLOCK_SIZE];
	unsigned char arrOuterPad[XRT_SHA1_BLOCK_SIZE];
	unsigned char arrInner[XSSH_KNOWN_HOST_HASH_SIZE];
	xsha1 Inner;
	xsha1 Outer;
	size_t i;
	bool bResult;

	memset(arrInnerPad, 0, sizeof(arrInnerPad));
	memset(arrOuterPad, 0, sizeof(arrOuterPad));
	memcpy(arrInnerPad, Salt.Data, Salt.Size);
	memcpy(arrOuterPad, Salt.Data, Salt.Size);
	for ( i = 0u; i < sizeof(arrInnerPad); ++i ) {
		arrInnerPad[i] ^= 0x36u;
		arrOuterPad[i] ^= 0x5cu;
	}
	xrtSha1Init(&Inner);
	xrtSha1Init(&Outer);
	bResult = xrtSha1Update(&Inner, arrInnerPad, sizeof(arrInnerPad)) &&
		xsshKnownHostHashTargetUpdate(&Inner, pTarget) &&
		xrtSha1Final(&Inner, arrInner) &&
		xrtSha1Update(&Outer, arrOuterPad, sizeof(arrOuterPad)) &&
		xrtSha1Update(&Outer, arrInner, sizeof(arrInner)) &&
		xrtSha1Final(&Outer, pHash);
	xrtSecureZero(arrInnerPad, sizeof(arrInnerPad));
	xrtSecureZero(arrOuterPad, sizeof(arrOuterPad));
	xrtSecureZero(arrInner, sizeof(arrInner));
	xrtSecureZero(&Inner, sizeof(Inner));
	xrtSecureZero(&Outer, sizeof(Outer));
	return bResult;
}



/* 严格拆分 |1|salt|hash，具体 Base64 长度由解码结果约束。 */
static xsshcode xsshKnownHostHashFields(
	xstrview HashedHost,
	xstrview* pSalt,
	xstrview* pHash
)
{
	size_t i;
	size_t iDelimiter = SIZE_MAX;

	if ( !xrtMemRangeValid(HashedHost.Data, HashedHost.Size) ||
		(pSalt == NULL) || (pHash == NULL) ||
		(HashedHost.Size < 5u) ||
		(memcmp(HashedHost.Data, "|1|", 3u) != 0) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	for ( i = 3u; i < HashedHost.Size; ++i ) {
		if ( HashedHost.Data[i] == '|' ) {
			if ( iDelimiter != SIZE_MAX ) {
				return XSSH_ERROR_PROTOCOL;
			}
			iDelimiter = i;
		}
	}
	if ( (iDelimiter == SIZE_MAX) || (iDelimiter == 3u) ||
		((iDelimiter + 1u) == HashedHost.Size) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	pSalt->Data = HashedHost.Data + 3u;
	pSalt->Size = iDelimiter - 3u;
	pHash->Data = HashedHost.Data + iDelimiter + 1u;
	pHash->Size = HashedHost.Size - iDelimiter - 1u;
	return XSSH_OK;
}



/* 常量时间比较固定长度摘要。 */
static bool xsshKnownHostHashEqual(
	const unsigned char* pLeft,
	const unsigned char* pRight
)
{
	unsigned int iDifference = 0u;
	size_t i;

	for ( i = 0u; i < XSSH_KNOWN_HOST_HASH_SIZE; ++i ) {
		iDifference |= (unsigned int)(pLeft[i] ^ pRight[i]);
	}
	return iDifference == 0u;
}



/* 公开无文本编码的 HMAC-SHA1 原语，供数据库生成器复用。 */
xsshcode xrtSshKnownHostHash(
	xstrview Host,
	uint32 iPort,
	xbytesview Salt,
	void* pHash
)
{
	xsshknownhosttarget Target;
	unsigned char arrHash[XSSH_KNOWN_HOST_HASH_SIZE];

	if ( (Salt.Size != XSSH_KNOWN_HOST_HASH_SIZE) ||
		!xrtMemRangeValid(Salt.Data, Salt.Size) ||
		!xrtMemRangeValid(pHash, XSSH_KNOWN_HOST_HASH_SIZE) ||
		!xsshKnownHostTargetInit(&Target, Host, iPort) ||
		xrtMemRangesOverlap(
			Host.Data,
			Host.Size,
			pHash,
			XSSH_KNOWN_HOST_HASH_SIZE
		) || xrtMemRangesOverlap(
			Salt.Data,
			Salt.Size,
			pHash,
			XSSH_KNOWN_HOST_HASH_SIZE
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !xsshKnownHostHmacSha1(&Target, Salt, arrHash) ) {
		xrtSecureZero(arrHash, sizeof(arrHash));
		return XSSH_ERROR_STATE;
	}
	memcpy(pHash, arrHash, sizeof(arrHash));
	xrtSecureZero(arrHash, sizeof(arrHash));
	return XSSH_OK;
}



/* 预计算完整容量后直接生成 OpenSSH hashed-host token。 */
xsshcode xrtSshKnownHostHashWrite(
	xstrview Host,
	uint32 iPort,
	xbytesview Salt,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	unsigned char arrHash[XSSH_KNOWN_HOST_HASH_SIZE];
	char sSalt[32];
	char sHash[32];
	size_t iSaltSize;
	size_t iHashSize;
	size_t iRequired;
	xsshcode Code;

	if ( !xrtMemRangeValid(Host.Data, Host.Size) ||
		!xrtMemRangeValid(Salt.Data, Salt.Size) ||
		!xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		!xrtMemRangeValid(sOutput, iCapacity) ||
		xrtMemRangesOverlap(
			pOutputSize,
			sizeof(*pOutputSize),
			Host.Data,
			Host.Size
		) || xrtMemRangesOverlap(
			pOutputSize,
			sizeof(*pOutputSize),
			Salt.Data,
			Salt.Size
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshKnownHostHash(Host, iPort, Salt, arrHash);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtBase64Encode(
		Salt.Data,
		Salt.Size,
		sSalt,
		sizeof(sSalt),
		&iSaltSize,
		NULL
	) || !xrtBase64Encode(
		arrHash,
		sizeof(arrHash),
		sHash,
		sizeof(sHash),
		&iHashSize,
		NULL
	) ) {
		xrtSecureZero(arrHash, sizeof(arrHash));
		return XSSH_ERROR_STATE;
	}
	iRequired = 4u + iSaltSize + iHashSize;
	if ( sOutput == NULL ) {
		*pOutputSize = iRequired;
		xrtSecureZero(arrHash, sizeof(arrHash));
		return XSSH_OK;
	}
	if ( (iCapacity <= iRequired) ||
		xrtMemRangesOverlap(
			sOutput,
			iRequired + 1u,
			Host.Data,
			Host.Size
		) || xrtMemRangesOverlap(
			sOutput,
			iRequired + 1u,
			Salt.Data,
			Salt.Size
		) || xrtMemRangesOverlap(
			sOutput,
			iRequired + 1u,
			pOutputSize,
			sizeof(*pOutputSize)
		) ) {
		xrtSecureZero(arrHash, sizeof(arrHash));
		return iCapacity <= iRequired ?
			XSSH_ERROR_SPACE : XSSH_ERROR_ARGUMENT;
	}
	memcpy(sOutput, "|1|", 3u);
	memcpy(sOutput + 3u, sSalt, iSaltSize);
	sOutput[3u + iSaltSize] = '|';
	memcpy(sOutput + 4u + iSaltSize, sHash, iHashSize);
	sOutput[iRequired] = '\0';
	*pOutputSize = iRequired;
	xrtSecureZero(arrHash, sizeof(arrHash));
	return XSSH_OK;
}



/* 解码 salt/hash，重新计算并在末尾一次发布匹配结果。 */
xsshcode xrtSshKnownHostHashMatch(
	xstrview HashedHost,
	xstrview Host,
	uint32 iPort,
	bool* pMatch
)
{
	unsigned char arrSalt[XSSH_KNOWN_HOST_HASH_SIZE];
	unsigned char arrExpected[XSSH_KNOWN_HOST_HASH_SIZE];
	unsigned char arrActual[XSSH_KNOWN_HOST_HASH_SIZE];
	xstrview SaltText;
	xstrview HashText;
	size_t iSaltSize;
	size_t iHashSize;
	bool bMatch;
	xsshcode Code;

	if ( !xrtMemRangeValid(HashedHost.Data, HashedHost.Size) ||
		!xrtMemRangeValid(Host.Data, Host.Size) ||
		!xrtMemRangeValid(pMatch, sizeof(*pMatch)) ||
		xrtMemRangesOverlap(
			HashedHost.Data,
			HashedHost.Size,
			pMatch,
			sizeof(*pMatch)
		) || xrtMemRangesOverlap(
			Host.Data,
			Host.Size,
			pMatch,
			sizeof(*pMatch)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshKnownHostHashFields(HashedHost, &SaltText, &HashText);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtBase64Decode(
		SaltText.Data,
		SaltText.Size,
		arrSalt,
		sizeof(arrSalt),
		&iSaltSize,
		NULL
	) || !xrtBase64Decode(
		HashText.Data,
		HashText.Size,
		arrExpected,
		sizeof(arrExpected),
		&iHashSize,
		NULL
	) || (iSaltSize != sizeof(arrSalt)) ||
		(iHashSize != sizeof(arrExpected)) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xrtSshKnownHostHash(
		Host,
		iPort,
		(xbytesview){ arrSalt, sizeof(arrSalt) },
		arrActual
	);
	if ( Code != XSSH_OK ) {
		xrtSecureZero(arrSalt, sizeof(arrSalt));
		xrtSecureZero(arrExpected, sizeof(arrExpected));
		return Code;
	}
	bMatch = xsshKnownHostHashEqual(arrExpected, arrActual);
	xrtSecureZero(arrSalt, sizeof(arrSalt));
	xrtSecureZero(arrExpected, sizeof(arrExpected));
	xrtSecureZero(arrActual, sizeof(arrActual));
	*pMatch = bMatch;
	return XSSH_OK;
}



/* 验证行类型后复用 hashed-host token matcher。 */
xsshcode xrtSshKnownHostLineHashMatch(
	const xsshknownhostline* pKnownHost,
	xstrview Host,
	uint32 iPort,
	bool* pMatch
)
{
	if ( !xrtMemRangeValid(pKnownHost, sizeof(*pKnownHost)) ||
		!xrtMemRangeValid(pMatch, sizeof(*pMatch)) ||
		xrtMemRangesOverlap(
			pKnownHost,
			sizeof(*pKnownHost),
			pMatch,
			sizeof(*pMatch)
		) || !pKnownHost->Hashed ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xrtSshKnownHostHashMatch(
		pKnownHost->Hosts,
		Host,
		iPort,
		pMatch
	);
}

#endif
