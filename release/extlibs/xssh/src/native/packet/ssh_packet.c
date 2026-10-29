#include <string.h>

#include <xrt/ssh_packet.h>



#if defined(XSSH_FEATURE_PACKET)

/* 规范化可选块长并拒绝无法放入 padding_length 的值。 */
static xsshcode xsshPacketBlock(size_t iRequested, size_t* pBlockSize)
{
	if ( pBlockSize == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( iRequested == 0u ) {
		*pBlockSize = XSSH_PACKET_BLOCK_MIN;
		return XSSH_OK;
	}
	if ( (iRequested < XSSH_PACKET_BLOCK_MIN) ||
		(iRequested > XSSH_PACKET_PADDING_MAX) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	*pBlockSize = iRequested;
	return XSSH_OK;
}



/* 计算 plain packet 的规范 padding 与线路长度。 */
xsshcode xrtSshPacketMeasure(
	size_t iPayloadSize,
	size_t iBlockSize,
	uint8* pPaddingSize,
	uint32* pPacketSize
)
{
	size_t iBaseSize;
	size_t iPaddingSize;
	size_t iPacketSize;
	xsshcode Code;

	if ( (pPaddingSize == NULL) || (pPacketSize == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshPacketBlock(iBlockSize, &iBlockSize);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iPayloadSize > ((size_t)UINT32_MAX - 1u) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	if ( iPayloadSize > (SIZE_MAX - 5u) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iBaseSize = 5u + iPayloadSize;
	iPaddingSize = iBaseSize % iBlockSize;
	iPaddingSize = iPaddingSize == 0u ? 0u : iBlockSize - iPaddingSize;
	while ( iPaddingSize < XSSH_PACKET_PADDING_MIN ) {
		if ( iPaddingSize > (XSSH_PACKET_PADDING_MAX - iBlockSize) ) {
			return XSSH_ERROR_OVERFLOW;
		}
		iPaddingSize += iBlockSize;
	}
	iPacketSize = 1u + iPayloadSize + iPaddingSize;
	if ( (iPaddingSize > XSSH_PACKET_PADDING_MAX) ||
		(iPacketSize > UINT32_MAX) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	*pPaddingSize = (uint8)iPaddingSize;
	*pPacketSize = (uint32)iPacketSize;
	return XSSH_OK;
}



/* 使用临时 padding 先完成所有可失败工作，再提交 packet。 */
xsshcode xrtSshPacketWrite(
	xsshwriter* pWriter,
	xbytesview Payload,
	size_t iBlockSize,
	uint32* pSequence,
	xsshpaddingproc pPadding,
	ptr pUserData
)
{
	unsigned char arrPadding[XSSH_PACKET_PADDING_MAX];
	xsshwriter Writer;
	xbytesview Padding;
	xbytesview Input;
	uint8 iPaddingSize;
	uint32 iPacketSize;
	size_t iTotalSize;
	xsshcode Code;

	if ( (pWriter == NULL) || (pPadding == NULL) ||
		((Payload.Data == NULL) && (Payload.Size != 0u)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshPacketMeasure(
		Payload.Size,
		iBlockSize,
		&iPaddingSize,
		&iPacketSize
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	iTotalSize = 4u + (size_t)iPacketSize;
	Input = Payload;
	Code = xrtSshWriterReserveInputs(
		pWriter,
		iTotalSize,
		&Input,
		1u
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	if ( !pPadding(arrPadding, iPaddingSize, pUserData) ) {
		return XSSH_ERROR_CALLBACK;
	}
	Padding.Data = arrPadding;
	Padding.Size = iPaddingSize;
	if ( (xrtSshWriteU32(&Writer, iPacketSize) != XSSH_OK) ||
		(xrtSshWriteByte(&Writer, iPaddingSize) != XSSH_OK) ||
		(xrtSshWriteBytes(&Writer, Payload) != XSSH_OK) ||
		(xrtSshWriteBytes(&Writer, Padding) != XSSH_OK) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pWriter = Writer;
	if ( pSequence != NULL ) {
		*pSequence = *pSequence + 1u;
	}
	return XSSH_OK;
}



/* 在 reader 副本中完成 packet 校验，成功后统一发布视图与序列。 */
xsshcode xrtSshPacketRead(
	xsshreader* pReader,
	size_t iBlockSize,
	uint32 iMaxPacketSize,
	uint32* pSequence,
	xsshpacketview* pPacket
)
{
	xsshreader Reader;
	xsshpacketview Packet;
	uint32 iPacketSize;
	size_t iPayloadSize;
	size_t iTotalSize;
	uint8 iPaddingSize;
	xsshcode Code;

	if ( (pReader == NULL) || (pPacket == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Reader = *pReader;
	Code = xsshPacketBlock(iBlockSize, &iBlockSize);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadU32(&Reader, &iPacketSize);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iPacketSize < (1u + XSSH_PACKET_PADDING_MIN) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	if ( iMaxPacketSize == 0u ) {
		iMaxPacketSize = XSSH_PACKET_MAX_DEFAULT;
	}
	if ( iPacketSize > iMaxPacketSize ) {
		return XSSH_ERROR_OVERFLOW;
	}
	#if SIZE_MAX <= UINT32_MAX
		if ( (size_t)iPacketSize > (SIZE_MAX - 4u) ) {
			return XSSH_ERROR_OVERFLOW;
		}
	#endif
	iTotalSize = 4u + (size_t)iPacketSize;
	if ( (iTotalSize % iBlockSize) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	if ( xrtSshReaderRemaining(&Reader) < (size_t)iPacketSize ) {
		return XSSH_NEED_MORE;
	}
	Code = xrtSshReadByte(&Reader, &iPaddingSize);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (iPaddingSize < XSSH_PACKET_PADDING_MIN) ||
		((uint32)iPaddingSize + 1u > iPacketSize) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	iPayloadSize = (size_t)iPacketSize - (size_t)iPaddingSize - 1u;
	Code = xrtSshReadBytes(&Reader, iPayloadSize, &Packet.Payload);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadBytes(&Reader, iPaddingSize, &Packet.Padding);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Packet.Sequence = pSequence == NULL ? 0u : *pSequence;
	Packet.PacketSize = iPacketSize;
	Packet.PaddingSize = iPaddingSize;
	*pReader = Reader;
	*pPacket = Packet;
	if ( pSequence != NULL ) {
		*pSequence = *pSequence + 1u;
	}
	return XSSH_OK;
}

#endif
