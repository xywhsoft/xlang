#include <xrt/ssh_packet_codec.h>

#include <string.h>



#if defined(XSSH_FEATURE_PACKET_CODEC)

#define XSSH_PACKET_CODEC_GUARD UINT32_C(0x53504344)



/* 校验公开状态，避免伪造 mode 绕过 cipher 生命周期。 */
static bool xsshPacketCodecValid(const xsshpacketcodec* pCodec)
{
	if ( !xrtMemRangeValid(pCodec, sizeof(*pCodec)) ||
		(pCodec->Guard != XSSH_PACKET_CODEC_GUARD) ||
		(pCodec->MaxPacketSize < (1u + XSSH_PACKET_PADDING_MIN)) ||
		((pCodec->ReadMode != XSSH_PACKET_MODE_PLAIN) &&
		 (pCodec->ReadMode != XSSH_PACKET_MODE_AES_GCM)) ||
		((pCodec->WriteMode != XSSH_PACKET_MODE_PLAIN) &&
		 (pCodec->WriteMode != XSSH_PACKET_MODE_AES_GCM)) ) {
		return false;
	}
	return true;
}



/* 在目标状态保持有效时提交新 cipher，失败不会清除旧密钥。 */
static xsshcode xsshPacketCodecSetAesGcm(
	xsshaesgcm* pTarget,
	xsshpacketmode* pMode,
	xbytesview Key,
	xbytesview InitialIV
)
{
	xsshaesgcm State;
	xsshcode Code;

	memset(&State, 0, sizeof(State));
	Code = xrtSshAesGcmInit(&State, Key, InitialIV);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	xrtSshAesGcmClear(pTarget);
	*pTarget = State;
	*pMode = XSSH_PACKET_MODE_AES_GCM;
	xrtSecureZero(&State, sizeof(State));
	return XSSH_OK;
}



/* 预验 writer 的本次输出区不会覆盖 codec 状态。 */
static xsshcode xsshPacketCodecWriterCheck(
	const xsshpacketcodec* pCodec,
	const xsshwriter* pWriter,
	size_t iWireSize
)
{
	const unsigned char* pOutput;

	if ( !xrtMemRangeValid(pWriter, sizeof(*pWriter)) ||
		!xrtMemRangeValid(pWriter->Data, pWriter->Capacity) ||
		(pWriter->Size > pWriter->Capacity) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( xrtMemRangesOverlap(
		pWriter,
		sizeof(*pWriter),
		pCodec,
		sizeof(*pCodec)
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( iWireSize > (pWriter->Capacity - pWriter->Size) ) {
		return XSSH_ERROR_SPACE;
	}
	pOutput = pWriter->Data + pWriter->Size;
	if ( xrtMemRangesOverlap(
		pOutput,
		iWireSize,
		pCodec,
		sizeof(*pCodec)
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return XSSH_OK;
}



/* 复用底层测量函数，统一 plain 与 AES-GCM 的线路尺寸口径。 */
static xsshcode xsshPacketCodecWriteNeed(
	const xsshpacketcodec* pCodec,
	size_t iPayloadSize,
	xsshpacketneed* pNeed
)
{
	xsshpacketneed Need;
	uint8 iPaddingSize;
	xsshcode Code;

	if ( pCodec->WriteMode == XSSH_PACKET_MODE_PLAIN ) {
		Code = xrtSshPacketMeasure(
			iPayloadSize,
			XSSH_PACKET_BLOCK_MIN,
			&iPaddingSize,
			&Need.PacketSize
		);
	} else {
		Code = xrtSshAesGcmMeasure(
			iPayloadSize,
			&iPaddingSize,
			&Need.PacketSize
		);
	}
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( Need.PacketSize > pCodec->MaxPacketSize ) {
		return XSSH_ERROR_OVERFLOW;
	}
	#if SIZE_MAX <= UINT32_MAX
		if ( (size_t)Need.PacketSize > (SIZE_MAX - 4u -
			(pCodec->WriteMode == XSSH_PACKET_MODE_AES_GCM ?
			 XSSH_AES_GCM_TAG_SIZE : 0u)) ) {
			return XSSH_ERROR_OVERFLOW;
		}
	#endif
	Need.WireSize = 4u + (size_t)Need.PacketSize;
	Need.PlainSize = 0u;
	if ( pCodec->WriteMode == XSSH_PACKET_MODE_AES_GCM ) {
		Need.WireSize += XSSH_AES_GCM_TAG_SIZE;
	}
	*pNeed = Need;
	return XSSH_OK;
}



/* 初始化时明确写入默认上限，避免零值在后续被解释为不同策略。 */
xsshcode xrtSshPacketCodecInit(
	xsshpacketcodec* pCodec,
	uint32 iMaxPacketSize
)
{
	xsshpacketcodec Codec;

	if ( !xrtMemRangeValid(pCodec, sizeof(*pCodec)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( iMaxPacketSize == 0u ) {
		iMaxPacketSize = XSSH_PACKET_MAX_DEFAULT;
	}
	if ( iMaxPacketSize < (1u + XSSH_PACKET_PADDING_MIN) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	memset(&Codec, 0, sizeof(Codec));
	Codec.MaxPacketSize = iMaxPacketSize;
	Codec.ReadMode = XSSH_PACKET_MODE_PLAIN;
	Codec.WriteMode = XSSH_PACKET_MODE_PLAIN;
	Codec.Guard = XSSH_PACKET_CODEC_GUARD;
	xrtSecureZero(pCodec, sizeof(*pCodec));
	*pCodec = Codec;
	xrtSecureZero(&Codec, sizeof(Codec));
	return XSSH_OK;
}



/* Cipher 状态始终按秘密材料处理，即使 codec 尚未初始化。 */
void xrtSshPacketCodecClear(xsshpacketcodec* pCodec)
{
	if ( pCodec == NULL ) {
		return;
	}
	xrtSecureZero(pCodec, sizeof(*pCodec));
}



/* 读取方向在 peer NEWKEYS 已认证后独立提交。 */
xsshcode xrtSshPacketCodecSetReadAesGcm(
	xsshpacketcodec* pCodec,
	xbytesview Key,
	xbytesview InitialIV
)
{
	if ( !xsshPacketCodecValid(pCodec) ) {
		return XSSH_ERROR_STATE;
	}
	return xsshPacketCodecSetAesGcm(
		&pCodec->ReadAesGcm,
		&pCodec->ReadMode,
		Key,
		InitialIV
	);
}



/* 写入方向在本端 NEWKEYS 已入队后独立提交。 */
xsshcode xrtSshPacketCodecSetWriteAesGcm(
	xsshpacketcodec* pCodec,
	xbytesview Key,
	xbytesview InitialIV
)
{
	if ( !xsshPacketCodecValid(pCodec) ) {
		return XSSH_ERROR_STATE;
	}
	if ( pCodec->WritePending ) {
		return XSSH_ERROR_STATE;
	}
	return xsshPacketCodecSetAesGcm(
		&pCodec->WriteAesGcm,
		&pCodec->WriteMode,
		Key,
		InitialIV
	);
}



/* strict-kex 只重置完成 NEWKEYS 的对应读取方向。 */
xsshcode xrtSshPacketCodecResetReadSequence(xsshpacketcodec* pCodec)
{
	if ( !xsshPacketCodecValid(pCodec) ) {
		return XSSH_ERROR_STATE;
	}
	pCodec->ReadSequence = 0u;
	return XSSH_OK;
}



/* strict-kex 只重置完成 NEWKEYS 的对应写入方向。 */
xsshcode xrtSshPacketCodecResetWriteSequence(xsshpacketcodec* pCodec)
{
	if ( !xsshPacketCodecValid(pCodec) ) {
		return XSSH_ERROR_STATE;
	}
	if ( pCodec->WritePending ) {
		return XSSH_ERROR_STATE;
	}
	pCodec->WriteSequence = 0u;
	return XSSH_OK;
}



/* 长度头保持明文，因此 plain 与 AES-GCM 都能先确定完整缓冲需求。 */
xsshcode xrtSshPacketCodecInspect(
	const xsshpacketcodec* pCodec,
	const xsshreader* pReader,
	xsshpacketneed* pNeed
)
{
	xsshpacketneed Need;
	xsshreader Reader;
	uint32 iPacketSize;
	xsshcode Code;

	if ( !xsshPacketCodecValid(pCodec) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pReader, sizeof(*pReader)) ||
		!xrtMemRangeValid(pNeed, sizeof(*pNeed)) ||
		xrtMemRangesOverlap(
			pCodec,
			sizeof(*pCodec),
			pNeed,
			sizeof(*pNeed)
		) || xrtMemRangesOverlap(
			pReader,
			sizeof(*pReader),
			pNeed,
			sizeof(*pNeed)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Reader = *pReader;
	Code = xrtSshReadU32(&Reader, &iPacketSize);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iPacketSize < (1u + XSSH_PACKET_PADDING_MIN) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	if ( iPacketSize > pCodec->MaxPacketSize ) {
		return XSSH_ERROR_OVERFLOW;
	}
	Need.PacketSize = iPacketSize;
	Need.PlainSize = 0u;
	#if SIZE_MAX <= UINT32_MAX
		if ( (size_t)iPacketSize > (SIZE_MAX - 4u -
			(pCodec->ReadMode == XSSH_PACKET_MODE_AES_GCM ?
			 XSSH_AES_GCM_TAG_SIZE : 0u)) ) {
			return XSSH_ERROR_OVERFLOW;
		}
	#endif
	Need.WireSize = 4u + (size_t)iPacketSize;
	if ( pCodec->ReadMode == XSSH_PACKET_MODE_PLAIN ) {
		if ( (Need.WireSize % XSSH_PACKET_BLOCK_MIN) != 0u ) {
			return XSSH_ERROR_PROTOCOL;
		}
	} else {
		if ( (iPacketSize % XSSH_AES_GCM_BLOCK_SIZE) != 0u ) {
			return XSSH_ERROR_PROTOCOL;
		}
		Need.PlainSize = (size_t)iPacketSize;
		Need.WireSize += XSSH_AES_GCM_TAG_SIZE;
	}
	if ( xrtMemRangesOverlap(
		pReader->Source.Data,
		pReader->Source.Size,
		pNeed,
		sizeof(*pNeed)
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	*pNeed = Need;
	return XSSH_OK;
}



/* 尺寸探测不推进 sequence、nonce 或唯一写事务。 */
xsshcode xrtSshPacketCodecWriteMeasure(
	const xsshpacketcodec* pCodec,
	size_t iPayloadSize,
	xsshpacketneed* pNeed
)
{
	xsshpacketneed Need;
	xsshcode Code;

	if ( !xsshPacketCodecValid(pCodec) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pNeed, sizeof(*pNeed)) ||
		xrtMemRangesOverlap(
			pCodec,
			sizeof(*pCodec),
			pNeed,
			sizeof(*pNeed)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshPacketCodecWriteNeed(pCodec, iPayloadSize, &Need);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pNeed = Need;
	return XSSH_OK;
}



/*
	准备线路包后回退底层一次性 writer 推进的计数。
	WritePending 阻止同一 sequence/nonce 在提交或放弃前被再次使用。
*/
xsshcode xrtSshPacketCodecWritePrepareWithPadding(
	xsshpacketcodec* pCodec,
	xsshwriter* pWriter,
	xbytesview Payload,
	xsshpaddingproc pPadding,
	ptr pUserData
)
{
	xsshpacketneed Need;
	xsshcode Code;

	if ( !xsshPacketCodecValid(pCodec) ) {
		return XSSH_ERROR_STATE;
	}
	if ( pCodec->WritePending ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pWriter, sizeof(*pWriter)) ||
		!xrtMemRangeValid(Payload.Data, Payload.Size) ||
		(pPadding == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshPacketCodecWriteNeed(pCodec, Payload.Size, &Need);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshPacketCodecWriterCheck(
		pCodec,
		pWriter,
		Need.WireSize
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( pCodec->WriteMode == XSSH_PACKET_MODE_PLAIN ) {
		Code = xrtSshPacketWrite(
			pWriter,
			Payload,
			XSSH_PACKET_BLOCK_MIN,
			&pCodec->WriteSequence,
			pPadding,
			pUserData
		);
	} else {
		Code = xrtSshAesGcmWrite(
			pWriter,
			Payload,
			&pCodec->WriteAesGcm,
			&pCodec->WriteSequence,
			pPadding,
			pUserData
		);
	}
	if ( Code != XSSH_OK ) {
		return Code;
	}
	pCodec->WriteSequence--;
	if ( pCodec->WriteMode == XSSH_PACKET_MODE_AES_GCM ) {
		pCodec->WriteAesGcm.Invocation--;
	}
	pCodec->WritePending = true;
	return XSSH_OK;
}



/* 只有可靠入队边界能够消费本次 sequence 与 nonce。 */
xsshcode xrtSshPacketCodecWriteCommit(xsshpacketcodec* pCodec)
{
	if ( !xsshPacketCodecValid(pCodec) || !pCodec->WritePending ) {
		return XSSH_ERROR_STATE;
	}
	pCodec->WriteSequence++;
	if ( pCodec->WriteMode == XSSH_PACKET_MODE_AES_GCM ) {
		pCodec->WriteAesGcm.Invocation++;
	}
	pCodec->WritePending = false;
	return XSSH_OK;
}



/* 未发送的线路包可以丢弃，不留下 sequence 或 nonce 缺口。 */
xsshcode xrtSshPacketCodecWriteAbort(xsshpacketcodec* pCodec)
{
	if ( !xsshPacketCodecValid(pCodec) || !pCodec->WritePending ) {
		return XSSH_ERROR_STATE;
	}
	pCodec->WritePending = false;
	return XSSH_OK;
}



/* 无背压便利路径仍复用同一事务边界。 */
xsshcode xrtSshPacketCodecWriteWithPadding(
	xsshpacketcodec* pCodec,
	xsshwriter* pWriter,
	xbytesview Payload,
	xsshpaddingproc pPadding,
	ptr pUserData
)
{
	xsshcode Code = xrtSshPacketCodecWritePrepareWithPadding(
		pCodec,
		pWriter,
		Payload,
		pPadding,
		pUserData
	);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	return xrtSshPacketCodecWriteCommit(pCodec);
}



/* 解码成功后才推进 reader、序列号和 AEAD invocation。 */
xsshcode xrtSshPacketCodecRead(
	xsshpacketcodec* pCodec,
	xsshreader* pReader,
	xsshpacketview* pPacket,
	void* pPlain,
	size_t iPlainCapacity
)
{
	if ( !xsshPacketCodecValid(pCodec) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pReader, sizeof(*pReader)) ||
		!xrtMemRangeValid(pPacket, sizeof(*pPacket)) ||
		xrtMemRangesOverlap(
			pCodec,
			sizeof(*pCodec),
			pReader,
			sizeof(*pReader)
		) || xrtMemRangesOverlap(
			pCodec,
			sizeof(*pCodec),
			pPacket,
			sizeof(*pPacket)
		) || xrtMemRangesOverlap(
			pReader,
			sizeof(*pReader),
			pPacket,
			sizeof(*pPacket)
		) || xrtMemRangesOverlap(
			pReader->Source.Data,
			pReader->Source.Size,
			pCodec,
			sizeof(*pCodec)
		) || xrtMemRangesOverlap(
			pReader->Source.Data,
			pReader->Source.Size,
			pPacket,
			sizeof(*pPacket)
		) || xrtMemRangesOverlap(
			pReader->Source.Data,
			pReader->Source.Size,
			pReader,
			sizeof(*pReader)
		) || xrtMemRangesOverlap(
			pPlain,
			iPlainCapacity,
			pPacket,
			sizeof(*pPacket)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (pCodec->ReadMode == XSSH_PACKET_MODE_AES_GCM) &&
		(!xrtMemRangeValid(pPlain, iPlainCapacity) ||
		 xrtMemRangesOverlap(
			pPlain,
			iPlainCapacity,
			pCodec,
			sizeof(*pCodec)
		 ) || xrtMemRangesOverlap(
			pPlain,
			iPlainCapacity,
			pReader,
			sizeof(*pReader)
		 )) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pCodec->ReadMode == XSSH_PACKET_MODE_PLAIN ) {
		return xrtSshPacketRead(
			pReader,
			XSSH_PACKET_BLOCK_MIN,
			pCodec->MaxPacketSize,
			&pCodec->ReadSequence,
			pPacket
		);
	}
	return xrtSshAesGcmRead(
		pReader,
		&pCodec->ReadAesGcm,
		pCodec->MaxPacketSize,
		&pCodec->ReadSequence,
		pPacket,
		pPlain,
		iPlainCapacity
	);
}

#endif
