#include <string.h>
#include <stdint.h>

#include <xrt/memory.h>
#include <xrt/ssh_packet_aes_gcm.h>



#if defined(XSSH_FEATURE_PACKET_AES_GCM)

#define XSSH_AES_GCM_GUARD UINT32_C(0x53414743)



/* 读取大端 invocation counter。 */
static uint64 xsshAesGcmLoadU64(const uint8* pData)
{
	uint64 iValue = 0u;
	size_t i;

	for ( i = 0u; i < 8u; ++i ) {
		iValue = (iValue << 8u) | (uint64)pData[i];
	}
	return iValue;
}



/* 写入大端 invocation counter。 */
static void xsshAesGcmStoreU64(uint8* pData, uint64 iValue)
{
	size_t i;

	for ( i = 0u; i < 8u; ++i ) {
		pData[7u - i] = (uint8)(iValue & UINT64_C(0xff));
		iValue >>= 8u;
	}
}



/* 组合 OpenSSH AES-GCM 使用的 fixed || invocation nonce。 */
static void xsshAesGcmNonce(
	const xsshaesgcm* pState,
	uint8 pNonce[XSSH_AES_GCM_IV_SIZE]
)
{
	memcpy(pNonce, pState->FixedIV, XSSH_AES_GCM_FIXED_IV_SIZE);
	xsshAesGcmStoreU64(
		pNonce + XSSH_AES_GCM_FIXED_IV_SIZE,
		pState->Invocation
	);
}



/* 验证状态和 counter，避免 nonce 回绕后复用。 */
static bool xsshAesGcmCanUse(const xsshaesgcm* pState)
{
	return (pState != NULL) && (pState->Guard == XSSH_AES_GCM_GUARD) &&
		(pState->Invocation != UINT64_MAX) &&
		(xrtAesGcmTagSize(&pState->Cipher) == XSSH_AES_GCM_TAG_SIZE);
}



/* 计算 AES-GCM 明文体的十六字节对齐 padding。 */
xsshcode xrtSshAesGcmMeasure(
	size_t iPayloadSize,
	uint8* pPaddingSize,
	uint32* pPacketSize
)
{
	size_t iBaseSize;
	size_t iPaddingSize;
	size_t iPacketSize;

	if ( (pPaddingSize == NULL) || (pPacketSize == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (iPayloadSize > ((size_t)UINT32_MAX - 1u)) ||
		(iPayloadSize > (SIZE_MAX - 1u)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iBaseSize = 1u + iPayloadSize;
	iPaddingSize = iBaseSize % XSSH_AES_GCM_BLOCK_SIZE;
	iPaddingSize = iPaddingSize == 0u ? 0u :
		XSSH_AES_GCM_BLOCK_SIZE - iPaddingSize;
	while ( iPaddingSize < XSSH_PACKET_PADDING_MIN ) {
		iPaddingSize += XSSH_AES_GCM_BLOCK_SIZE;
	}
	if ( iBaseSize > (SIZE_MAX - iPaddingSize) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iPacketSize = iBaseSize + iPaddingSize;
	if ( (iPaddingSize > XSSH_PACKET_PADDING_MAX) ||
		(iPacketSize > UINT32_MAX) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	*pPaddingSize = (uint8)iPaddingSize;
	*pPacketSize = (uint32)iPacketSize;
	return XSSH_OK;
}



/* 初始化单向状态，禁止没有可用 nonce 的最大初始 counter。 */
xsshcode xrtSshAesGcmInit(
	xsshaesgcm* pState,
	xbytesview Key,
	xbytesview InitialIV
)
{
	xsshaesgcm State;
	uint64 iInvocation;

	if ( (pState == NULL) || (Key.Data == NULL) ||
		((Key.Size != 16u) && (Key.Size != 32u)) ||
		(InitialIV.Data == NULL) ||
		(InitialIV.Size != XSSH_AES_GCM_IV_SIZE) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	iInvocation = xsshAesGcmLoadU64(
		InitialIV.Data + XSSH_AES_GCM_FIXED_IV_SIZE
	);
	if ( iInvocation == UINT64_MAX ) {
		return XSSH_ERROR_STATE;
	}
	memset(&State, 0, sizeof(State));
	if ( !xrtAesGcmInit(
		&State.Cipher,
		Key.Data,
		Key.Size,
		XSSH_AES_GCM_TAG_SIZE
	) ) {
		xrtSecureZero(&State, sizeof(State));
		return XSSH_ERROR_ARGUMENT;
	}
	memcpy(
		State.FixedIV,
		InitialIV.Data,
		XSSH_AES_GCM_FIXED_IV_SIZE
	);
	State.Invocation = iInvocation;
	State.Guard = XSSH_AES_GCM_GUARD;
	xrtSecureZero(pState, sizeof(*pState));
	*pState = State;
	xrtSecureZero(&State, sizeof(State));
	return XSSH_OK;
}



/* 清除完整的 SSH AES-GCM 状态。 */
void xrtSshAesGcmClear(xsshaesgcm* pState)
{
	if ( pState == NULL ) {
		return;
	}
	xrtSecureZero(pState, sizeof(*pState));
}



/* 查询 counter 时保持无效调用的输出不变。 */
xsshcode xrtSshAesGcmInvocation(
	const xsshaesgcm* pState,
	uint64* pInvocation
)
{
	if ( (pInvocation == NULL) || (pState == NULL) ||
		(pState->Guard != XSSH_AES_GCM_GUARD) ||
		(xrtAesGcmTagSize(&pState->Cipher) != XSSH_AES_GCM_TAG_SIZE) ) {
		return XSSH_ERROR_STATE;
	}
	*pInvocation = pState->Invocation;
	return XSSH_OK;
}



/* 在目标缓冲区构建明文体，再用 XRT AES-GCM 原位加密。 */
xsshcode xrtSshAesGcmWrite(
	xsshwriter* pWriter,
	xbytesview Payload,
	xsshaesgcm* pState,
	uint32* pSequence,
	xsshpaddingproc pPadding,
	ptr pUserData
)
{
	unsigned char arrPadding[XSSH_PACKET_PADDING_MAX];
	uint8 arrNonce[XSSH_AES_GCM_IV_SIZE];
	xsshwriter Writer;
	xbytesview arrInputs[2];
	bytes pBody;
	bytes pTag;
	uint8 iPaddingSize;
	uint32 iPacketSize;
	size_t iTotalSize;
	xsshcode Code;

	if ( (pWriter == NULL) || (pPadding == NULL) ||
		((Payload.Data == NULL) && (Payload.Size != 0u)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !xsshAesGcmCanUse(pState) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshAesGcmMeasure(
		Payload.Size,
		&iPaddingSize,
		&iPacketSize
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	iTotalSize = 4u + (size_t)iPacketSize + XSSH_AES_GCM_TAG_SIZE;
	arrInputs[0].Data = (const unsigned char*)pState;
	arrInputs[0].Size = sizeof(*pState);
	arrInputs[1] = Payload;
	Code = xrtSshWriterReserveInputs(
		pWriter,
		iTotalSize,
		arrInputs,
		2u
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	if ( !pPadding(arrPadding, iPaddingSize, pUserData) ) {
		xrtSecureZero(arrPadding, sizeof(arrPadding));
		return XSSH_ERROR_CALLBACK;
	}
	if ( xrtSshWriteU32(&Writer, iPacketSize) != XSSH_OK ) {
		xrtSecureZero(arrPadding, sizeof(arrPadding));
		return XSSH_ERROR_PROTOCOL;
	}
	pBody = Writer.Data + Writer.Size;
	pTag = pBody + iPacketSize;
	pBody[0] = iPaddingSize;
	if ( Payload.Size != 0u ) {
		memmove(pBody + 1u, Payload.Data, Payload.Size);
	}
	memcpy(pBody + 1u + Payload.Size, arrPadding, iPaddingSize);
	xrtSecureZero(arrPadding, sizeof(arrPadding));
	xsshAesGcmNonce(pState, arrNonce);
	if ( !xrtAesGcmEncrypt(
		&pState->Cipher,
		arrNonce,
		sizeof(arrNonce),
		Writer.Data + Writer.Size - 4u,
		4u,
		pBody,
		iPacketSize,
		pBody,
		pTag
	) ) {
		xrtSecureZero(arrNonce, sizeof(arrNonce));
		xrtSecureZero(pBody, iPacketSize);
		return XSSH_ERROR_STATE;
	}
	xrtSecureZero(arrNonce, sizeof(arrNonce));
	Writer.Size += (size_t)iPacketSize + XSSH_AES_GCM_TAG_SIZE;
	*pWriter = Writer;
	pState->Invocation++;
	if ( pSequence != NULL ) {
		*pSequence = *pSequence + 1u;
	}
	return XSSH_OK;
}



/* 先完成长度与容量校验，再认证、解密和发布 packet view。 */
xsshcode xrtSshAesGcmRead(
	xsshreader* pReader,
	xsshaesgcm* pState,
	uint32 iMaxPacketSize,
	uint32* pSequence,
	xsshpacketview* pPacket,
	void* pPlain,
	size_t iPlainCapacity
)
{
	uint8 arrNonce[XSSH_AES_GCM_IV_SIZE];
	xsshreader Reader;
	xsshpacketview Packet;
	const uint8* pHeader;
	const uint8* pCipher;
	const uint8* pTag;
	bytes pBody = (bytes)pPlain;
	uint32 iPacketSize;
	uint8 iPaddingSize;
	size_t iPayloadSize;
	size_t iCipherAndTagSize;
	xsshcode Code;

	if ( (pReader == NULL) || (pPacket == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !xsshAesGcmCanUse(pState) ) {
		return XSSH_ERROR_STATE;
	}
	Reader = *pReader;
	pHeader = Reader.Source.Data == NULL ? NULL :
		Reader.Source.Data + Reader.Position;
	Code = xrtSshReadU32(&Reader, &iPacketSize);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (iPacketSize < (1u + XSSH_PACKET_PADDING_MIN)) ||
		((iPacketSize % XSSH_AES_GCM_BLOCK_SIZE) != 0u) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	if ( iMaxPacketSize == 0u ) {
		iMaxPacketSize = XSSH_PACKET_MAX_DEFAULT;
	}
	if ( iPacketSize > iMaxPacketSize ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iCipherAndTagSize = (size_t)iPacketSize + XSSH_AES_GCM_TAG_SIZE;
	if ( xrtSshReaderRemaining(&Reader) < iCipherAndTagSize ) {
		return XSSH_NEED_MORE;
	}
	if ( (pPlain == NULL) || (iPlainCapacity < (size_t)iPacketSize) ) {
		return XSSH_ERROR_SPACE;
	}
	pCipher = Reader.Source.Data + Reader.Position;
	pTag = pCipher + iPacketSize;
	if ( xrtMemRangesOverlap(
		pPlain,
		iPacketSize,
		pHeader,
		4u + iCipherAndTagSize
	) || xrtMemRangesOverlap(
		pPlain,
		iPacketSize,
		pState,
		sizeof(*pState)
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	xsshAesGcmNonce(pState, arrNonce);
	if ( !xrtAesGcmDecrypt(
		&pState->Cipher,
		arrNonce,
		sizeof(arrNonce),
		pHeader,
		4u,
		pCipher,
		iPacketSize,
		pTag,
		pPlain
	) ) {
		xrtSecureZero(arrNonce, sizeof(arrNonce));
		return XSSH_ERROR_AUTHENTICATION;
	}
	xrtSecureZero(arrNonce, sizeof(arrNonce));
	iPaddingSize = pBody[0];
	if ( (iPaddingSize < XSSH_PACKET_PADDING_MIN) ||
		((uint32)iPaddingSize + 1u > iPacketSize) ) {
		xrtSecureZero(pPlain, iPacketSize);
		return XSSH_ERROR_PROTOCOL;
	}
	iPayloadSize = (size_t)iPacketSize - (size_t)iPaddingSize - 1u;
	Packet.Sequence = pSequence == NULL ? 0u : *pSequence;
	Packet.PacketSize = iPacketSize;
	Packet.PaddingSize = iPaddingSize;
	Packet.Payload.Data = pBody + 1u;
	Packet.Payload.Size = iPayloadSize;
	Packet.Padding.Data = pBody + 1u + iPayloadSize;
	Packet.Padding.Size = iPaddingSize;
	Reader.Position += iCipherAndTagSize;
	*pReader = Reader;
	*pPacket = Packet;
	pState->Invocation++;
	if ( pSequence != NULL ) {
		*pSequence = *pSequence + 1u;
	}
	return XSSH_OK;
}

#endif
