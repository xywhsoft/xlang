#include <string.h>

#include <xrt/ssh_channels.h>



#if defined(XSSH_FEATURE_CHANNELS)

#define XSSH_CHANNEL_GUARD UINT32_C(0x43484e4c)
#define XSSH_CHANNELS_GUARD UINT32_C(0x43484e53)



/* 设置当前执行上下文的参数错误。 */
static xsshcode xsshChannelsArgument(void)
{
	xrtSetErrorKind(XERR_ARGUMENT);
	return XSSH_ERROR_ARGUMENT;
}



/* 设置当前执行上下文的状态错误。 */
static xsshcode xsshChannelsState(void)
{
	xrtSetErrorKind(XERR_STATE);
	return XSSH_ERROR_STATE;
}



/* 把底层已经设置的容量或分配错误收窄为 SSH 空间错误。 */
static xsshcode xsshChannelsSpace(void)
{
	return XSSH_ERROR_SPACE;
}



/* 校验单个动态 channel 的拥有关系和固定字段。 */
static bool xsshChannelValid(const xsshchannel* pChannel)
{
	if ( !xrtMemRangeValid(pChannel, sizeof(*pChannel)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( (pChannel->Guard != XSSH_CHANNEL_GUARD) ||
		 !pChannel->Initialized || !pChannel->Core.Initialized ||
		 !pChannel->Io.Initialized ||
		 (pChannel->Io.Channel != &pChannel->Core) ||
		 (pChannel->Replies.Capacity != pChannel->ReplyCapacity) ||
		 (pChannel->Replies.Tokens != pChannel->ReplyTokens) ||
		 (pChannel->ReplyCapacity > pChannel->ReplyLimit) ||
		 ((pChannel->ReplyCapacity == 0u) !=
		  (pChannel->ReplyTokens == NULL)) ) {
		xrtSetErrorKind(XERR_STATE);
		return false;
	}
	return true;
}



/* 校验集合和默认配置没有被外部破坏。 */
static bool xsshChannelsValid(const xsshchannels* pChannels)
{
	if ( !xrtMemRangeValid(pChannels, sizeof(*pChannels)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( (pChannels->Guard != XSSH_CHANNELS_GUARD) ||
		 !pChannels->Initialized ||
		 (pChannels->Config.MaxChannels == 0u) ||
		 (pChannels->Config.MaxChannels > UINT32_MAX) ||
		 (pChannels->Config.ReceiveWindow == 0u) ||
		 (pChannels->Config.ReceiveMaxPacket == 0u) ||
		 (pChannels->Config.AdjustThreshold == 0u) ||
		 (pChannels->Config.AdjustThreshold >
		  pChannels->Config.ReceiveWindow) ||
		 (pChannels->Config.Io.ReceiveLimit <
		  pChannels->Config.ReceiveWindow) ) {
		xrtSetErrorKind(XERR_STATE);
		return false;
	}
	return true;
}



/* 释放整数映射内联 channel 拥有的全部资源。 */
static void xsshChannelsDrop(int64 iKey, ptr pValue, ptr pUserData)
{
	xsshchannel* pChannel = (xsshchannel*)pValue;

	(void)iKey;
	(void)pUserData;
	if ( pChannel->Initialized ) {
		xrtSshChannelIoClear(&pChannel->Io);
		xrtSshChannelCoreClear(&pChannel->Core);
		xrtFree(pChannel->ReplyTokens);
	}
	memset(pChannel, 0, sizeof(*pChannel));
}



/* 从单调游标开始寻找当前未占用的 uint32 channel id。 */
static bool xsshChannelsLocal(
	xsshchannels* pChannels,
	uint32* pLocal
)
{
	size_t i;
	uint32 iLocal = pChannels->NextLocal;
	size_t iCount = xrtIntMapCount(&pChannels->Map);

	for ( i = 0u; i <= iCount; ++i ) {
		if ( xrtIntMapGet(&pChannels->Map, (int64)iLocal) == NULL ) {
			*pLocal = iLocal;
			pChannels->NextLocal = iLocal + 1u;
			return true;
		}
		iLocal++;
	}
	xrtSetErrorKind(XERR_RANGE);
	return false;
}



/* 在已经清零的映射值槽中建立 core、I/O 和空回复队列。 */
static xsshcode xsshChannelsChannelInit(
	xsshchannels* pChannels,
	xsshchannel* pChannel,
	uint32 iLocal,
	const xsshchannelopen* pOpen
)
{
	bool bCore;

	if ( pOpen == NULL ) {
		bCore = xrtSshChannelCoreOpenInit(
			&pChannel->Core,
			iLocal,
			pChannels->Config.ReceiveWindow,
			pChannels->Config.ReceiveMaxPacket,
			pChannels->Config.AdjustThreshold
		);
	} else {
		bCore = xrtSshChannelCoreAcceptInit(
			&pChannel->Core,
			iLocal,
			pOpen,
			pChannels->Config.ReceiveWindow,
			pChannels->Config.ReceiveMaxPacket,
			pChannels->Config.AdjustThreshold
		);
	}
	if ( !bCore ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !xrtSshChannelIoInit(
		&pChannel->Io,
		pChannels->Pool,
		&pChannel->Core,
		&pChannels->Config.Io
	) ) {
		xrtSshChannelCoreClear(&pChannel->Core);
		return xsshChannelsSpace();
	}
	if ( !xrtSshReplyQueueInit(&pChannel->Replies, NULL, 0u) ) {
		xrtSshChannelIoClear(&pChannel->Io);
		xrtSshChannelCoreClear(&pChannel->Core);
		return XSSH_ERROR_STATE;
	}
	pChannel->Incoming = pOpen != NULL;
	pChannel->ReplyLimit = pChannels->Config.ReplyLimit;
	pChannel->Initialized = true;
	pChannel->Guard = XSSH_CHANNEL_GUARD;
	return XSSH_OK;
}



/* 创建本端或对端发起的 channel，并保持失败时映射不留空槽。 */
static xsshcode xsshChannelsAdd(
	xsshchannels* pChannels,
	const xsshchannelopen* pOpen,
	xsshchannel** ppChannel
)
{
	xsshchannel* pChannel;
	xsshcode Code;
	uint32 iLocal;
	bool bNew = false;

	if ( ppChannel == NULL ) {
		return xsshChannelsArgument();
	}
	*ppChannel = NULL;
	if ( !xsshChannelsValid(pChannels) ) {
		return XSSH_ERROR_STATE;
	}
	if ( xrtIntMapCount(&pChannels->Map) >=
		 pChannels->Config.MaxChannels ) {
		xrtSetErrorKind(XERR_RANGE);
		return XSSH_ERROR_SPACE;
	}
	if ( !xsshChannelsLocal(pChannels, &iLocal) ) {
		return XSSH_ERROR_SPACE;
	}
	pChannel = (xsshchannel*)xrtIntMapGetOrAdd(
		&pChannels->Map,
		(int64)iLocal,
		&bNew
	);
	if ( (pChannel == NULL) || !bNew ) {
		return xsshChannelsSpace();
	}
	Code = xsshChannelsChannelInit(
		pChannels,
		pChannel,
		iLocal,
		pOpen
	);
	if ( Code != XSSH_OK ) {
		(void)xrtIntMapRemove(&pChannels->Map, (int64)iLocal);
		return Code;
	}
	*ppChannel = pChannel;
	return XSSH_OK;
}



/* 写入不会预分配 channel 或 reply token 的默认预算。 */
XRT_API void xrtSshChannelsConfigInit(xsshchannelsconfig* pConfig)
{
	if ( pConfig == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->MaxChannels = XSSH_CHANNELS_MAX_DEFAULT;
	pConfig->ReplyLimit = XSSH_CHANNELS_REPLY_LIMIT_DEFAULT;
	pConfig->ReceiveWindow = XSSH_CHANNELS_WINDOW_DEFAULT;
	pConfig->ReceiveMaxPacket = XSSH_CHANNELS_PACKET_DEFAULT;
	pConfig->AdjustThreshold = XSSH_CHANNELS_ADJUST_DEFAULT;
	xrtSshChannelIoConfigInit(&pConfig->Io);
}



/* 初始化拥有式整数映射，channel 节点地址在删除前保持稳定。 */
XRT_API bool xrtSshChannelsInit(
	xsshchannels* pChannels,
	xnetbufpool* pPool,
	const xsshchannelsconfig* pConfig
)
{
	xsshchannelsconfig Config;

	if ( pChannels == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( pConfig == NULL ) {
		xrtSshChannelsConfigInit(&Config);
		pConfig = &Config;
	}
	if ( (pConfig->MaxChannels == 0u) ||
		 (pConfig->MaxChannels > UINT32_MAX) ||
		 (pConfig->ReceiveWindow == 0u) ||
		 (pConfig->ReceiveMaxPacket == 0u) ||
		 (pConfig->AdjustThreshold == 0u) ||
		 (pConfig->AdjustThreshold > pConfig->ReceiveWindow) ||
		 (pConfig->Io.ReceiveLimit < pConfig->ReceiveWindow) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memset(pChannels, 0, sizeof(*pChannels));
	if ( !xrtIntMapInit(&pChannels->Map, sizeof(xsshchannel)) ) {
		return false;
	}
	if ( !xrtIntMapSetDrop(&pChannels->Map, xsshChannelsDrop, NULL) ) {
		xrtIntMapUnit(&pChannels->Map);
		memset(pChannels, 0, sizeof(*pChannels));
		return false;
	}
	pChannels->Config = *pConfig;
	pChannels->Pool = pPool;
	pChannels->Initialized = true;
	pChannels->Guard = XSSH_CHANNELS_GUARD;
	return true;
}



/* 清理映射会逐项释放动态 I/O 和 reply token。 */
XRT_API void xrtSshChannelsClear(xsshchannels* pChannels)
{
	if ( !xsshChannelsValid(pChannels) ) {
		return;
	}
	xrtIntMapUnit(&pChannels->Map);
	memset(pChannels, 0, sizeof(*pChannels));
}



/* 返回活动节点数。 */
XRT_API size_t xrtSshChannelsCount(const xsshchannels* pChannels)
{
	if ( !xsshChannelsValid(pChannels) ) {
		return 0u;
	}
	return xrtIntMapCount(&pChannels->Map);
}



/* 创建本端发起的 channel。 */
XRT_API xsshcode xrtSshChannelsOpen(
	xsshchannels* pChannels,
	xsshchannel** ppChannel
)
{
	return xsshChannelsAdd(pChannels, NULL, ppChannel);
}



/* 创建对端发起的 channel。 */
XRT_API xsshcode xrtSshChannelsAccept(
	xsshchannels* pChannels,
	const xsshchannelopen* pOpen,
	xsshchannel** ppChannel
)
{
	if ( pOpen == NULL ) {
		if ( ppChannel != NULL ) {
			*ppChannel = NULL;
		}
		return xsshChannelsArgument();
	}
	return xsshChannelsAdd(pChannels, pOpen, ppChannel);
}



/* 按线路 recipient 查询可写 channel。 */
XRT_API xsshchannel* xrtSshChannelsGet(
	xsshchannels* pChannels,
	uint32 iLocal
)
{
	if ( !xsshChannelsValid(pChannels) ) {
		return NULL;
	}
	return (xsshchannel*)xrtIntMapGet(&pChannels->Map, (int64)iLocal);
}



/* 按线路 recipient 查询只读 channel。 */
XRT_API const xsshchannel* xrtSshChannelsConstGet(
	const xsshchannels* pChannels,
	uint32 iLocal
)
{
	if ( !xsshChannelsValid(pChannels) ) {
		return NULL;
	}
	return (const xsshchannel*)xrtIntMapConstGet(
		&pChannels->Map,
		(int64)iLocal
	);
}



/* 使用非重叠新存储迁移回复队列，避免 realloc 后留下悬空借用。 */
XRT_API xsshcode xrtSshChannelReplyReserve(
	xsshchannel* pChannel,
	size_t iCapacity
)
{
	uint64* pTokens;
	size_t iNewCapacity;
	xsshcode Code;

	if ( !xsshChannelValid(pChannel) ) {
		return XSSH_ERROR_STATE;
	}
	if ( iCapacity > pChannel->ReplyLimit ) {
		xrtSetErrorKind(XERR_RANGE);
		return XSSH_ERROR_SPACE;
	}
	if ( iCapacity <= pChannel->ReplyCapacity ) {
		return XSSH_OK;
	}
	iNewCapacity = pChannel->ReplyCapacity != 0u ?
		pChannel->ReplyCapacity : 4u;
	while ( iNewCapacity < iCapacity ) {
		if ( iNewCapacity > (SIZE_MAX / 2u) ) {
			iNewCapacity = iCapacity;
			break;
		}
		iNewCapacity *= 2u;
	}
	if ( iNewCapacity > pChannel->ReplyLimit ) {
		iNewCapacity = pChannel->ReplyLimit;
	}
	if ( iNewCapacity > (SIZE_MAX / sizeof(uint64)) ) {
		xrtSetErrorKind(XERR_RANGE);
		return XSSH_ERROR_OVERFLOW;
	}
	pTokens = (uint64*)xrtMalloc(iNewCapacity * sizeof(uint64));
	if ( pTokens == NULL ) {
		return xsshChannelsSpace();
	}
	Code = xrtSshReplyQueueRebind(
		&pChannel->Replies,
		pTokens,
		iNewCapacity
	);
	if ( Code != XSSH_OK ) {
		xrtFree(pTokens);
		return Code;
	}
	xrtFree(pChannel->ReplyTokens);
	pChannel->ReplyTokens = pTokens;
	pChannel->ReplyCapacity = iNewCapacity;
	return XSSH_OK;
}



/* 检查结束状态和所有可观察数据都已消费。 */
XRT_API bool xrtSshChannelsRemove(
	xsshchannels* pChannels,
	uint32 iLocal
)
{
	xsshchannel* pChannel;
	xsshchannelcorephase Phase;

	if ( !xsshChannelsValid(pChannels) ) {
		return false;
	}
	pChannel = (xsshchannel*)xrtIntMapGet(
		&pChannels->Map,
		(int64)iLocal
	);
	if ( pChannel == NULL ) {
		xrtSetErrorKind(XERR_NOT_FOUND);
		return false;
	}
	if ( !xsshChannelValid(pChannel) ) {
		return false;
	}
	Phase = xrtSshChannelCorePhase(&pChannel->Core);
	if ( ((Phase != XSSH_CHANNEL_CORE_FAILED) &&
		  (Phase != XSSH_CHANNEL_CORE_CLOSED)) ||
		 (pChannel->Io.Pending != XSSH_CHANNEL_IO_PENDING_NONE) ||
		 (xrtSshReplyQueueCount(&pChannel->Replies) != 0u) ||
		 (xrtSshChannelIoReadable(
			&pChannel->Io,
			XSSH_CHANNEL_IO_DATA
		 ) != 0u) ||
		 (xrtSshChannelIoReadable(
			&pChannel->Io,
			XSSH_CHANNEL_IO_STDERR
		 ) != 0u) ||
		 (xrtSshChannelIoQueued(
			&pChannel->Io,
			XSSH_CHANNEL_IO_DATA
		 ) != 0u) ||
		 (xrtSshChannelIoQueued(
			&pChannel->Io,
			XSSH_CHANNEL_IO_STDERR
		 ) != 0u) ) {
		(void)xsshChannelsState();
		return false;
	}
	return xrtIntMapRemove(&pChannels->Map, (int64)iLocal);
}



/* 强制删除仍活动或仍有应用数据的 channel。 */
XRT_API bool xrtSshChannelsDiscard(
	xsshchannels* pChannels,
	uint32 iLocal
)
{
	if ( !xsshChannelsValid(pChannels) ) {
		return false;
	}
	if ( !xrtIntMapHas(&pChannels->Map, (int64)iLocal) ) {
		xrtSetErrorKind(XERR_NOT_FOUND);
		return false;
	}
	return xrtIntMapRemove(&pChannels->Map, (int64)iLocal);
}



/* 把集合查询适配为 connection session 的无拥有权 resolver。 */
XRT_API bool xrtSshChannelsResolve(
	ptr pUserData,
	uint32 iLocal,
	xsshchannelcore** ppChannel,
	xsshreplyqueue** ppReplies
)
{
	xsshchannel* pChannel;

	if ( (ppChannel == NULL) || (ppReplies == NULL) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	*ppChannel = NULL;
	*ppReplies = NULL;
	pChannel = xrtSshChannelsGet((xsshchannels*)pUserData, iLocal);
	if ( (pChannel == NULL) || !xsshChannelValid(pChannel) ) {
		return false;
	}
	*ppChannel = &pChannel->Core;
	*ppReplies = &pChannel->Replies;
	return true;
}



/* 启动稳定地址节点的升序遍历。 */
XRT_API bool xrtSshChannelsIterBegin(
	xsshchannels* pChannels,
	xsshchannelsiter* pIterator
)
{
	if ( !xsshChannelsValid(pChannels) || (pIterator == NULL) ) {
		if ( pIterator == NULL ) {
			xrtSetErrorKind(XERR_ARGUMENT);
		}
		return false;
	}
	memset(pIterator, 0, sizeof(*pIterator));
	if ( !xrtIntMapIterBegin(&pChannels->Map, &pIterator->Base) ) {
		return false;
	}
	pIterator->Active = true;
	return true;
}



/* 读取下一节点并安全收窄非负 uint32 键。 */
XRT_API xsshchannel* xrtSshChannelsIterNext(
	xsshchannelsiter* pIterator,
	uint32* pLocal
)
{
	xsshchannel* pChannel;
	int64 iKey = 0;

	if ( (pIterator == NULL) || !pIterator->Active ) {
		xrtSetErrorKind(XERR_STATE);
		return NULL;
	}
	pChannel = (xsshchannel*)xrtIntMapIterNext(
		&pIterator->Base,
		&iKey
	);
	if ( pChannel == NULL ) {
		return NULL;
	}
	if ( (iKey < 0) || (iKey > UINT32_MAX) ||
		 !xsshChannelValid(pChannel) ) {
		(void)xsshChannelsState();
		return NULL;
	}
	if ( pLocal != NULL ) {
		*pLocal = (uint32)iKey;
	}
	return pChannel;
}



/* 结束外置迭代。 */
XRT_API void xrtSshChannelsIterEnd(xsshchannelsiter* pIterator)
{
	if ( (pIterator == NULL) || !pIterator->Active ) {
		xrtSetErrorKind(XERR_STATE);
		return;
	}
	xrtIntMapIterEnd(&pIterator->Base);
	memset(pIterator, 0, sizeof(*pIterator));
}

#endif
