#include <string.h>
#include <stdint.h>

#include <xrt/memory.h>
#include <xrt/ssh_kex_sha256.h>



#if defined(XSSH_FEATURE_KEX_SHA256)

/* 验证可编码为 SSH string 的视图。 */
static bool xsshKexHashViewValid(xbytesview Value)
{
	return !((Value.Data == NULL) && (Value.Size != 0u)) &&
		(Value.Size <= UINT32_MAX);
}



/* 以大端序追加一个 uint32。 */
static bool xsshKexHashU32(xsha256* pState, uint32 iValue)
{
	uint8 arrValue[4];

	arrValue[0] = (uint8)(iValue >> 24u);
	arrValue[1] = (uint8)(iValue >> 16u);
	arrValue[2] = (uint8)(iValue >> 8u);
	arrValue[3] = (uint8)iValue;
	return xrtSha256Update(pState, arrValue, sizeof(arrValue));
}



/* 追加一个带 uint32 长度的 SSH string。 */
static bool xsshKexHashString(xsha256* pState, xbytesview Value)
{
	return xsshKexHashViewValid(Value) &&
		xsshKexHashU32(pState, (uint32)Value.Size) &&
		xrtSha256Update(pState, Value.Data, Value.Size);
}



/* 追加由大端 magnitude 规范化得到的非负 SSH mpint。 */
static bool xsshKexHashMpint(xsha256* pState, xbytesview Magnitude)
{
	uint8 iZero = 0u;
	size_t iOffset = 0u;
	bool bPrefix;

	if ( !xsshKexHashViewValid(Magnitude) ) {
		return false;
	}
	while ( (iOffset < Magnitude.Size) &&
		(Magnitude.Data[iOffset] == 0u) ) {
		iOffset++;
	}
	if ( iOffset == Magnitude.Size ) {
		return xsshKexHashU32(pState, 0u);
	}
	Magnitude.Data += iOffset;
	Magnitude.Size -= iOffset;
	bPrefix = (Magnitude.Data[0] & 0x80u) != 0u;
	if ( Magnitude.Size > (UINT32_MAX - (bPrefix ? 1u : 0u)) ) {
		return false;
	}
	if ( !xsshKexHashU32(
		pState,
		(uint32)Magnitude.Size + (bPrefix ? 1u : 0u)
	) ) {
		return false;
	}
	if ( bPrefix && !xrtSha256Update(pState, &iZero, 1u) ) {
		return false;
	}
	return xrtSha256Update(pState, Magnitude.Data, Magnitude.Size);
}



/* 判断共享秘密是否包含非零字节。 */
static bool xsshKexHashNonZero(xbytesview Value)
{
	uint8 iAny = 0u;
	size_t i;

	if ( !xsshKexHashViewValid(Value) || (Value.Size == 0u) ) {
		return false;
	}
	for ( i = 0u; i < Value.Size; ++i ) {
		iAny |= Value.Data[i];
	}
	return iAny != 0u;
}



/* 流式计算 exchange hash，避免构造历史实现中的中间缓冲。 */
xsshcode xrtSshKexHashSha256(
	const xsshkexhashsha256* pInput,
	void* pHash
)
{
	xsha256 State;
	bool bValid;

	if ( (pInput == NULL) || (pHash == NULL) ||
		!xsshKexHashNonZero(pInput->SharedSecret) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	xrtSha256Init(&State);
	bValid = xsshKexHashString(&State, pInput->ClientVersion) &&
		xsshKexHashString(&State, pInput->ServerVersion) &&
		xsshKexHashString(&State, pInput->ClientKexInit) &&
		xsshKexHashString(&State, pInput->ServerKexInit) &&
		xsshKexHashString(&State, pInput->ServerHostKey) &&
		xsshKexHashString(&State, pInput->ClientEphemeral) &&
		xsshKexHashString(&State, pInput->ServerEphemeral) &&
		xsshKexHashMpint(&State, pInput->SharedSecret) &&
		xrtSha256Final(&State, pHash);
	xrtSecureZero(&State, sizeof(State));
	return bValid ? XSSH_OK : XSSH_ERROR_ARGUMENT;
}



/* 逐轮扩展 K || H || X || session_id 或 K || H || 已生成材料。 */
xsshcode xrtSshKexDeriveSha256(
	void* pOutput,
	size_t iOutputSize,
	xbytesview SharedSecret,
	const void* pExchangeHash,
	const void* pSessionId,
	uint8 iLetter
)
{
	uint8 arrDigest[XSSH_SHA256_SIZE];
	xsha256 State;
	bytes pBytes = (bytes)pOutput;
	size_t iDone = 0u;

	if ( ((pOutput == NULL) && (iOutputSize != 0u)) ||
		(pExchangeHash == NULL) || (pSessionId == NULL) ||
		(iLetter < (uint8)'A') || (iLetter > (uint8)'F') ||
		!xsshKexHashNonZero(SharedSecret) || xrtMemRangesOverlap(
			pOutput,
			iOutputSize,
			SharedSecret.Data,
			SharedSecret.Size
		) || xrtMemRangesOverlap(
			pOutput,
			iOutputSize,
			pExchangeHash,
			XSSH_SHA256_SIZE
		) || xrtMemRangesOverlap(
			pOutput,
			iOutputSize,
			pSessionId,
			XSSH_SHA256_SIZE
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	while ( iDone < iOutputSize ) {
		size_t iCopy = iOutputSize - iDone;
		bool bValid;

		xrtSha256Init(&State);
		bValid = xsshKexHashMpint(&State, SharedSecret) &&
			xrtSha256Update(
				&State,
				pExchangeHash,
				XSSH_SHA256_SIZE
			);
		if ( iDone == 0u ) {
			bValid = bValid && xrtSha256Update(&State, &iLetter, 1u) &&
				xrtSha256Update(&State, pSessionId, XSSH_SHA256_SIZE);
		} else {
			bValid = bValid && xrtSha256Update(&State, pOutput, iDone);
		}
		bValid = bValid && xrtSha256Final(&State, arrDigest);
		xrtSecureZero(&State, sizeof(State));
		if ( !bValid ) {
			xrtSecureZero(arrDigest, sizeof(arrDigest));
			return XSSH_ERROR_STATE;
		}
		if ( iCopy > sizeof(arrDigest) ) {
			iCopy = sizeof(arrDigest);
		}
		memcpy(pBytes + iDone, arrDigest, iCopy);
		iDone += iCopy;
	}
	xrtSecureZero(arrDigest, sizeof(arrDigest));
	return XSSH_OK;
}

#endif
