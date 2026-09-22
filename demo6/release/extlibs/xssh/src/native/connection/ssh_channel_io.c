#include <string.h>

#include <xrt/ssh_channel_io.h>



#if defined(XSSH_FEATURE_CHANNEL_IO)

#define XSSH_CHANNEL_IO_GUARD UINT32_C(0x4348494f)



/* 校验公开流类型。 */
static bool xsshChannelIoStreamValid(xsshchanneliostream Stream)
{
	return (Stream == XSSH_CHANNEL_IO_DATA) ||
		(Stream == XSSH_CHANNEL_IO_STDERR);
}



/* 返回指定接收流的内部缓冲。 */
static xnetbuf* xsshChannelIoReceiveBuffer(
	xsshchannelio* pIo,
	xsshchanneliostream Stream
)
{
	return Stream == XSSH_CHANNEL_IO_DATA ?
		&pIo->ReceiveData : &pIo->ReceiveError;
}



/* 返回指定发送流的内部缓冲。 */
static xnetbuf* xsshChannelIoSendBuffer(
	xsshchannelio* pIo,
	xsshchanneliostream Stream
)
{
	return Stream == XSSH_CHANNEL_IO_DATA ?
		&pIo->SendData : &pIo->SendError;
}



/* 安全相加两条缓冲长度。 */
static bool xsshChannelIoPairSize(
	const xnetbuf* pFirst,
	const xnetbuf* pSecond,
	size_t* pSize
)
{
	size_t iFirst = xrtNetBufSize(pFirst);
	size_t iSecond = xrtNetBufSize(pSecond);

	if ( iFirst > (SIZE_MAX - iSecond) ) {
		return false;
	}
	*pSize = iFirst + iSecond;
	return true;
}



/* 校验对象固定字段和内部缓冲没有活动写预留。 */
static bool xsshChannelIoValid(const xsshchannelio* pIo)
{
	return xrtMemRangeValid(pIo, sizeof(*pIo)) &&
		(pIo->Guard == XSSH_CHANNEL_IO_GUARD) && pIo->Initialized &&
		xrtMemRangeValid(pIo->Channel, sizeof(*pIo->Channel)) &&
		pIo->Channel->Initialized && !xrtMemRangesOverlap(
			pIo,
			sizeof(*pIo),
			pIo->Channel,
			sizeof(*pIo->Channel)
		) && (pIo->ReceiveLimit <= UINT32_MAX) &&
		(pIo->Pending >= XSSH_CHANNEL_IO_PENDING_NONE) &&
		(pIo->Pending <= XSSH_CHANNEL_IO_PENDING_SEND) &&
		xsshChannelIoStreamValid(pIo->PendingStream) &&
		(pIo->ReceiveData.Reserved == NULL) &&
		(pIo->ReceiveError.Reserved == NULL) &&
		(pIo->SendData.Reserved == NULL) &&
		(pIo->SendError.Reserved == NULL) &&
		(pIo->ReceiveStaging.Reserved == NULL);
}



/* 校验没有在途事务，且动态接收量与 channel 窗口计数一致。 */
static bool xsshChannelIoStable(const xsshchannelio* pIo)
{
	size_t iReadable;

	return xsshChannelIoValid(pIo) &&
		(pIo->Pending == XSSH_CHANNEL_IO_PENDING_NONE) &&
		xsshChannelIoPairSize(
			&pIo->ReceiveData,
			&pIo->ReceiveError,
			&iReadable
		) && ((uint64)iReadable == pIo->Channel->Window.ReceiveBuffered);
}



/* 判断一段外部数据不会覆盖 I/O 对象或绑定 channel。 */
static bool xsshChannelIoDataValid(
	const xsshchannelio* pIo,
	const void* pData,
	size_t iSize
)
{
	return xrtMemRangeValid(pData, iSize) && !xrtMemRangesOverlap(
		pIo,
		sizeof(*pIo),
		pData,
		iSize
	) && !xrtMemRangesOverlap(
		pIo->Channel,
		sizeof(*pIo->Channel),
		pData,
		iSize
	);
}



/* 清除短事务快照，不触碰任何动态块。 */
static void xsshChannelIoPendingClear(xsshchannelio* pIo)
{
	memset(&pIo->ChannelBefore, 0, sizeof(pIo->ChannelBefore));
	memset(&pIo->ChannelAfter, 0, sizeof(pIo->ChannelAfter));
	pIo->SendHead = NULL;
	pIo->PendingBytes = 0u;
	pIo->PendingStream = XSSH_CHANNEL_IO_DATA;
	pIo->Pending = XSSH_CHANNEL_IO_PENDING_NONE;
}



/* 返回共享发送预算，结构损坏时返回零。 */
static size_t xsshChannelIoWriteAvailable(const xsshchannelio* pIo)
{
	size_t iQueued;

	if ( !xsshChannelIoPairSize(
		&pIo->SendData,
		&pIo->SendError,
		&iQueued
	) || (iQueued >= pIo->SendLimit) ) {
		return 0u;
	}
	return pIo->SendLimit - iQueued;
}



/* 校验一次发送追加，并返回目标缓冲。 */
static xsshcode xsshChannelIoWriteCheck(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	const void* pData,
	size_t iSize,
	xnetbuf** ppBuffer
)
{
	if ( !xsshChannelIoStable(pIo) ||
		!xsshChannelIoStreamValid(Stream) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xsshChannelIoDataValid(pIo, pData, iSize) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( iSize > xsshChannelIoWriteAvailable(pIo) ) {
		return XSSH_ERROR_SPACE;
	}
	*ppBuffer = xsshChannelIoSendBuffer(pIo, Stream);
	return XSSH_OK;
}



/* 写入默认动态缓冲预算。 */
void xrtSshChannelIoConfigInit(xsshchannelioconfig* pConfig)
{
	if ( xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		pConfig->ReceiveLimit = XSSH_CHANNEL_IO_LIMIT_DEFAULT;
		pConfig->SendLimit = XSSH_CHANNEL_IO_LIMIT_DEFAULT;
	}
}



/* 初始化绑定单个 channel 的动态收发缓冲。 */
bool xrtSshChannelIoInit(
	xsshchannelio* pIo,
	xnetbufpool* pPool,
	xsshchannelcore* pChannel,
	const xsshchannelioconfig* pConfig
)
{
	xsshchannelioconfig Config;
	xsshchannelio Io;

	if ( !xrtMemRangeValid(pIo, sizeof(*pIo)) ||
		!xrtMemRangeValid(pChannel, sizeof(*pChannel)) ||
		!pChannel->Initialized || xrtMemRangesOverlap(
			pIo,
			sizeof(*pIo),
			pChannel,
			sizeof(*pChannel)
		) || (pChannel->Window.ReceiveBuffered != 0u) ||
		(pChannel->Window.ReceivePending != 0u) ) {
		return false;
	}
	xrtSshChannelIoConfigInit(&Config);
	if ( pConfig != NULL ) {
		if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ||
			xrtMemRangesOverlap(
				pIo,
				sizeof(*pIo),
				pConfig,
				sizeof(*pConfig)
			) || xrtMemRangesOverlap(
				pChannel,
				sizeof(*pChannel),
				pConfig,
				sizeof(*pConfig)
			) ) {
			return false;
		}
		Config = *pConfig;
	}
	if ( (Config.ReceiveLimit > UINT32_MAX) ||
		(Config.ReceiveLimit < pChannel->Window.ReceiveWindow) ) {
		return false;
	}
	memset(&Io, 0, sizeof(Io));
	if ( !xrtNetBufInit(&Io.ReceiveData, pPool) ||
		!xrtNetBufInit(&Io.ReceiveError, pPool) ||
		!xrtNetBufInit(&Io.SendData, pPool) ||
		!xrtNetBufInit(&Io.SendError, pPool) ||
		!xrtNetBufInit(&Io.ReceiveStaging, pPool) ) {
		return false;
	}
	Io.Channel = pChannel;
	Io.ReceiveLimit = Config.ReceiveLimit;
	Io.SendLimit = Config.SendLimit;
	Io.PendingStream = XSSH_CHANNEL_IO_DATA;
	Io.Initialized = true;
	Io.Guard = XSSH_CHANNEL_IO_GUARD;
	*pIo = Io;
	return true;
}



/* 丢弃全部队列，并把已经计入窗口的数据视为已消费。 */
void xrtSshChannelIoClear(xsshchannelio* pIo)
{
	if ( xsshChannelIoValid(pIo) ) {
		size_t iReadable;
		size_t iStaged = xrtNetBufSize(&pIo->ReceiveStaging);

		if ( xsshChannelIoPairSize(
			&pIo->ReceiveData,
			&pIo->ReceiveError,
			&iReadable
		) ) {
			size_t iCounted = iReadable;

			if ( (pIo->Pending == XSSH_CHANNEL_IO_PENDING_RECEIVE) &&
				(iCounted <= (SIZE_MAX - iStaged)) &&
				((uint64)(iCounted + iStaged) ==
				 pIo->Channel->Window.ReceiveBuffered) ) {
				iCounted += iStaged;
			}
			if ( ((uint64)iCounted ==
				pIo->Channel->Window.ReceiveBuffered) &&
				(iCounted <= UINT32_MAX) ) {
				(void)xrtSshChannelCoreDataConsume(
					pIo->Channel,
					(uint32)iCounted
				);
			}
		}
		xrtNetBufClear(&pIo->ReceiveData);
		xrtNetBufClear(&pIo->ReceiveError);
		xrtNetBufClear(&pIo->SendData);
		xrtNetBufClear(&pIo->SendError);
		xrtNetBufClear(&pIo->ReceiveStaging);
	}
	if ( xrtMemRangeValid(pIo, sizeof(*pIo)) ) {
		memset(pIo, 0, sizeof(*pIo));
	}
}



/* 查询已可靠接收的数据量。 */
size_t xrtSshChannelIoReadable(
	const xsshchannelio* pIo,
	xsshchanneliostream Stream
)
{
	if ( !xsshChannelIoValid(pIo) ||
		!xsshChannelIoStreamValid(Stream) ) {
		return 0u;
	}
	return xrtNetBufSize(xsshChannelIoReceiveBuffer(
		(xsshchannelio*)pIo,
		Stream
	));
}



/* 借出只读接收缓冲。 */
const xnetbuf* xrtSshChannelIoReadBuffer(
	const xsshchannelio* pIo,
	xsshchanneliostream Stream
)
{
	if ( !xsshChannelIoValid(pIo) ||
		!xsshChannelIoStreamValid(Stream) ) {
		return NULL;
	}
	return xsshChannelIoReceiveBuffer((xsshchannelio*)pIo, Stream);
}



/* 原子消费接收前缀和对应窗口计数。 */
xsshcode xrtSshChannelIoConsume(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	size_t iSize
)
{
	xsshchannelcore Channel;
	xnetbuf* pBuffer;
	xsshcode Code;

	if ( !xsshChannelIoStable(pIo) ||
		!xsshChannelIoStreamValid(Stream) ) {
		return XSSH_ERROR_STATE;
	}
	pBuffer = xsshChannelIoReceiveBuffer(pIo, Stream);
	if ( (iSize > UINT32_MAX) || (iSize > xrtNetBufSize(pBuffer)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Channel = *pIo->Channel;
	Code = xrtSshChannelCoreDataConsume(&Channel, (uint32)iSize);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtNetBufConsume(pBuffer, iSize) != iSize ) {
		return XSSH_ERROR_STATE;
	}
	*pIo->Channel = Channel;
	return XSSH_OK;
}



/* 复制并消费接收数据。 */
xsshcode xrtSshChannelIoRead(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	void* pOutput,
	size_t iCapacity,
	size_t* pRead
)
{
	xnetbuf* pBuffer;
	size_t iRead;
	xsshcode Code;

	if ( !xsshChannelIoStable(pIo) ||
		!xsshChannelIoStreamValid(Stream) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pOutput, iCapacity) ||
		!xrtMemRangeValid(pRead, sizeof(*pRead)) ||
		xrtMemRangesOverlap(
			pIo,
			sizeof(*pIo),
			pOutput,
			iCapacity
		) || xrtMemRangesOverlap(
			pIo->Channel,
			sizeof(*pIo->Channel),
			pOutput,
			iCapacity
		) || xrtMemRangesOverlap(
			pIo,
			sizeof(*pIo),
			pRead,
			sizeof(*pRead)
		) || xrtMemRangesOverlap(
			pIo->Channel,
			sizeof(*pIo->Channel),
			pRead,
			sizeof(*pRead)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	pBuffer = xsshChannelIoReceiveBuffer(pIo, Stream);
	iRead = xrtNetBufSize(pBuffer);
	if ( iRead > iCapacity ) {
		iRead = iCapacity;
	}
	if ( xrtNetBufPeek(pBuffer, 0u, pOutput, iRead) != iRead ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshChannelIoConsume(pIo, Stream, iRead);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pRead = iRead;
	return XSSH_OK;
}



/* 查询单条发送流排队量。 */
size_t xrtSshChannelIoQueued(
	const xsshchannelio* pIo,
	xsshchanneliostream Stream
)
{
	if ( !xsshChannelIoValid(pIo) ||
		!xsshChannelIoStreamValid(Stream) ) {
		return 0u;
	}
	return xrtNetBufSize(xsshChannelIoSendBuffer(
		(xsshchannelio*)pIo,
		Stream
	));
}



/* 查询共享发送预算。 */
size_t xrtSshChannelIoWritable(const xsshchannelio* pIo)
{
	return xsshChannelIoValid(pIo) ?
		xsshChannelIoWriteAvailable(pIo) : 0u;
}



/* 查询下一条 payload 可直接使用的连续队首。 */
size_t xrtSshChannelIoSendLimit(
	const xsshchannelio* pIo,
	xsshchanneliostream Stream
)
{
	xnetspan Span;
	uint32 iLimit;

	if ( !xsshChannelIoStable(pIo) ||
		!xsshChannelIoStreamValid(Stream) || !xrtNetBufFront(
			xsshChannelIoSendBuffer((xsshchannelio*)pIo, Stream),
			&Span
		) ) {
		return 0u;
	}
	iLimit = xrtSshChannelCoreSendLimit(pIo->Channel);
	return Span.Size < (size_t)iLimit ? Span.Size : (size_t)iLimit;
}



/* 复制追加发送数据。 */
xsshcode xrtSshChannelIoWrite(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	const void* pData,
	size_t iSize
)
{
	xnetbuf* pBuffer;
	xsshcode Code = xsshChannelIoWriteCheck(
		pIo,
		Stream,
		pData,
		iSize,
		&pBuffer
	);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	return xrtNetBufAppend(pBuffer, pData, iSize) ?
		XSSH_OK : XSSH_ERROR_SPACE;
}



/* 追加借用发送数据。 */
xsshcode xrtSshChannelIoWriteBorrow(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	const void* pData,
	size_t iSize
)
{
	xnetbuf* pBuffer;
	xsshcode Code = xsshChannelIoWriteCheck(
		pIo,
		Stream,
		pData,
		iSize,
		&pBuffer
	);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iSize == 0u ) {
		return XSSH_OK;
	}
	return xrtNetBufAppendBorrow(pBuffer, pData, iSize) ?
		XSSH_OK : XSSH_ERROR_SPACE;
}



/* 接管 XRT 分配的发送数据。 */
xsshcode xrtSshChannelIoWriteTake(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	void* pData,
	size_t iSize
)
{
	xnetbuf* pBuffer;
	xsshcode Code = xsshChannelIoWriteCheck(
		pIo,
		Stream,
		pData,
		iSize,
		&pBuffer
	);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iSize == 0u ) {
		return XSSH_OK;
	}
	return xrtNetBufAppendTake(pBuffer, pData, iSize) ?
		XSSH_OK : XSSH_ERROR_SPACE;
}



/* 接管自定义释放的发送数据。 */
xsshcode xrtSshChannelIoWriteRef(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	const void* pData,
	size_t iSize,
	xnetreleaseproc pRelease,
	ptr pContext
)
{
	xnetbuf* pBuffer;
	xsshcode Code;

	if ( (pRelease == NULL) && (iSize != 0u) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshChannelIoWriteCheck(
		pIo,
		Stream,
		pData,
		iSize,
		&pBuffer
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iSize == 0u ) {
		return XSSH_OK;
	}
	return xrtNetBufAppendRef(
		pBuffer,
		pData,
		iSize,
		pRelease,
		pContext
	) ? XSSH_OK : XSSH_ERROR_SPACE;
}



/* 移动调用方发送缓冲。 */
xsshcode xrtSshChannelIoWriteBuffer(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	xnetbuf* pBuffer
)
{
	xnetbuf* pTarget;
	size_t iSize;

	if ( !xsshChannelIoStable(pIo) ||
		!xsshChannelIoStreamValid(Stream) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pBuffer, sizeof(*pBuffer)) ||
		xrtMemRangesOverlap(
			pIo,
			sizeof(*pIo),
			pBuffer,
			sizeof(*pBuffer)
		) || xrtMemRangesOverlap(
			pIo->Channel,
			sizeof(*pIo->Channel),
			pBuffer,
			sizeof(*pBuffer)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	iSize = xrtNetBufSize(pBuffer);
	if ( iSize > xsshChannelIoWriteAvailable(pIo) ) {
		return XSSH_ERROR_SPACE;
	}
	if ( iSize == 0u ) {
		return XSSH_OK;
	}
	pTarget = xsshChannelIoSendBuffer(pIo, Stream);
	return xrtNetBufMove(pTarget, pBuffer) ?
		XSSH_OK : XSSH_ERROR_STATE;
}



/* 为认证后的 peer data 预分配动态接收块。 */
xsshcode xrtSshChannelIoReceivePrepare(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	uint32 iRecipient,
	xbytesview Data
)
{
	xsshchannelcore Channel;
	size_t iReadable;
	xsshcode Code;

	if ( !xsshChannelIoStable(pIo) ||
		!xsshChannelIoStreamValid(Stream) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xsshChannelIoDataValid(pIo, Data.Data, Data.Size) ||
		(Data.Size > UINT32_MAX) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !xsshChannelIoPairSize(
		&pIo->ReceiveData,
		&pIo->ReceiveError,
		&iReadable
	) || (iReadable > pIo->ReceiveLimit) ||
		(Data.Size > (pIo->ReceiveLimit - iReadable)) ) {
		return XSSH_ERROR_SPACE;
	}
	Channel = *pIo->Channel;
	Code = xrtSshChannelCoreDataReceiveCommit(
		&Channel,
		iRecipient,
		(uint32)Data.Size
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtNetBufAppend(&pIo->ReceiveStaging, Data.Data, Data.Size) ) {
		return XSSH_ERROR_SPACE;
	}
	pIo->ChannelBefore = *pIo->Channel;
	pIo->ChannelAfter = Channel;
	pIo->PendingBytes = Data.Size;
	pIo->PendingStream = Stream;
	pIo->Pending = XSSH_CHANNEL_IO_PENDING_RECEIVE;
	return XSSH_OK;
}



/* 在 channel 状态提交后发布预分配接收数据。 */
xsshcode xrtSshChannelIoReceiveCommit(xsshchannelio* pIo)
{
	xnetbuf* pTarget;
	size_t iReadable;

	if ( !xsshChannelIoValid(pIo) ||
		(pIo->Pending != XSSH_CHANNEL_IO_PENDING_RECEIVE) ||
		(memcmp(
			pIo->Channel,
			&pIo->ChannelAfter,
			sizeof(pIo->ChannelAfter)
		) != 0) || !xsshChannelIoPairSize(
			&pIo->ReceiveData,
			&pIo->ReceiveError,
			&iReadable
		) || (iReadable > (SIZE_MAX - pIo->PendingBytes)) ||
		((uint64)(iReadable + pIo->PendingBytes) !=
		 pIo->Channel->Window.ReceiveBuffered) ) {
		return XSSH_ERROR_STATE;
	}
	pTarget = xsshChannelIoReceiveBuffer(pIo, pIo->PendingStream);
	if ( !xrtNetBufMove(pTarget, &pIo->ReceiveStaging) ) {
		return XSSH_ERROR_STATE;
	}
	xsshChannelIoPendingClear(pIo);
	return XSSH_OK;
}



/* 无损放弃尚未提交的接收数据。 */
xsshcode xrtSshChannelIoReceiveAbort(xsshchannelio* pIo)
{
	if ( !xsshChannelIoValid(pIo) ||
		(pIo->Pending != XSSH_CHANNEL_IO_PENDING_RECEIVE) ||
		(memcmp(
			pIo->Channel,
			&pIo->ChannelBefore,
			sizeof(pIo->ChannelBefore)
		) != 0) ) {
		return XSSH_ERROR_STATE;
	}
	xrtNetBufClear(&pIo->ReceiveStaging);
	xsshChannelIoPendingClear(pIo);
	return XSSH_OK;
}



/* 从动态发送队首直接构建一条最终 channel payload。 */
xsshcode xrtSshChannelIoSendPrepare(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	xsshwriter* pWriter,
	xbytesview* pPayload
)
{
	xsshchannelcore Channel;
	xsshwriter Writer;
	xnetbuf* pBuffer;
	xnetspan Span;
	xbytesview Data;
	size_t iHeader;
	size_t iLimit;
	size_t iBegin;
	uint32 iLocal;
	uint32 iRemote;
	xsshcode Code;

	if ( !xsshChannelIoStable(pIo) ||
		!xsshChannelIoStreamValid(Stream) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pWriter, sizeof(*pWriter)) ||
		!xrtMemRangeValid(pPayload, sizeof(*pPayload)) ||
		xrtMemRangesOverlap(
			pIo,
			sizeof(*pIo),
			pWriter,
			sizeof(*pWriter)
		) || xrtMemRangesOverlap(
			pIo,
			sizeof(*pIo),
			pPayload,
			sizeof(*pPayload)
		) || xrtMemRangesOverlap(
			pIo->Channel,
			sizeof(*pIo->Channel),
			pWriter,
			sizeof(*pWriter)
		) || xrtMemRangesOverlap(
			pIo->Channel,
			sizeof(*pIo->Channel),
			pPayload,
			sizeof(*pPayload)
		) || !xrtMemRangeValid(pWriter->Data, pWriter->Capacity) ||
		(pWriter->Size > pWriter->Capacity) || xrtMemRangesOverlap(
			pIo,
			sizeof(*pIo),
			pWriter->Data,
			pWriter->Capacity
		) || xrtMemRangesOverlap(
			pIo->Channel,
			sizeof(*pIo->Channel),
			pWriter->Data,
			pWriter->Capacity
		) || xrtMemRangesOverlap(
			pPayload,
			sizeof(*pPayload),
			pWriter->Data,
			pWriter->Capacity
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !xrtSshChannelCoreIds(pIo->Channel, &iLocal, &iRemote) ) {
		return XSSH_ERROR_STATE;
	}
	(void)iLocal;
	pBuffer = xsshChannelIoSendBuffer(pIo, Stream);
	if ( !xrtNetBufFront(pBuffer, &Span) ) {
		return XSSH_NEED_MORE;
	}
	iLimit = (size_t)xrtSshChannelCoreSendLimit(pIo->Channel);
	if ( iLimit == 0u ) {
		return XSSH_NEED_MORE;
	}
	iHeader = Stream == XSSH_CHANNEL_IO_DATA ? 9u : 13u;
	if ( xrtSshWriterRemaining(pWriter) <= iHeader ) {
		return XSSH_ERROR_SPACE;
	}
	if ( iLimit > Span.Size ) {
		iLimit = Span.Size;
	}
	if ( iLimit > (xrtSshWriterRemaining(pWriter) - iHeader) ) {
		iLimit = xrtSshWriterRemaining(pWriter) - iHeader;
	}
	if ( iLimit == 0u ) {
		return XSSH_ERROR_SPACE;
	}
	Data = (xbytesview){ Span.Data, iLimit };
	Channel = *pIo->Channel;
	Code = xrtSshChannelCoreDataSendCommit(
		&Channel,
		(uint32)iLimit
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	iBegin = Writer.Size;
	Code = Stream == XSSH_CHANNEL_IO_DATA ?
		xrtSshChannelDataWrite(&Writer, iRemote, Data) :
		xrtSshChannelExtendedDataWrite(
			&Writer,
			iRemote,
			XSSH_CHANNEL_EXTENDED_DATA_STDERR,
			Data
		);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	pIo->ChannelBefore = *pIo->Channel;
	pIo->ChannelAfter = Channel;
	pIo->SendHead = Span.Data;
	pIo->PendingBytes = iLimit;
	pIo->PendingStream = Stream;
	pIo->Pending = XSSH_CHANNEL_IO_PENDING_SEND;
	*pWriter = Writer;
	pPayload->Data = Writer.Data + iBegin;
	pPayload->Size = Writer.Size - iBegin;
	return XSSH_OK;
}



/* 在 channel 发送额度已经提交后消费队首。 */
xsshcode xrtSshChannelIoSendCommit(xsshchannelio* pIo)
{
	xnetbuf* pBuffer;
	xnetspan Span;

	if ( !xsshChannelIoValid(pIo) ||
		(pIo->Pending != XSSH_CHANNEL_IO_PENDING_SEND) ||
		(memcmp(
			pIo->Channel,
			&pIo->ChannelAfter,
			sizeof(pIo->ChannelAfter)
		) != 0) ) {
		return XSSH_ERROR_STATE;
	}
	pBuffer = xsshChannelIoSendBuffer(pIo, pIo->PendingStream);
	if ( !xrtNetBufFront(pBuffer, &Span) ||
		(Span.Data != pIo->SendHead) ||
		(Span.Size < pIo->PendingBytes) ) {
		return XSSH_ERROR_STATE;
	}
	if ( xrtNetBufConsume(pBuffer, pIo->PendingBytes) !=
		pIo->PendingBytes ) {
		return XSSH_ERROR_STATE;
	}
	xsshChannelIoPendingClear(pIo);
	return XSSH_OK;
}



/* 无损放弃尚未提交的发送队首。 */
xsshcode xrtSshChannelIoSendAbort(xsshchannelio* pIo)
{
	if ( !xsshChannelIoValid(pIo) ||
		(pIo->Pending != XSSH_CHANNEL_IO_PENDING_SEND) ||
		(memcmp(
			pIo->Channel,
			&pIo->ChannelBefore,
			sizeof(pIo->ChannelBefore)
		) != 0) ) {
		return XSSH_ERROR_STATE;
	}
	xsshChannelIoPendingClear(pIo);
	return XSSH_OK;
}

#endif
