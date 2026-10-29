#include <string.h>

#include <xrt/ssh_connection_session.h>



#if defined(XSSH_FEATURE_CONNECTION_SESSION)

#define XSSH_CONNECTION_SESSION_GUARD UINT32_C(0x434f4e4e)



/* 校验调用方 reply FIFO 的公开环形状态。 */
static bool xsshConnectionQueueValid(const xsshreplyqueue* pQueue)
{
	size_t iBytes;

	if ( !xrtMemRangeValid(pQueue, sizeof(*pQueue)) ||
		!pQueue->Initialized || (pQueue->Count > pQueue->Capacity) ||
		(pQueue->Capacity > (SIZE_MAX / sizeof(uint64))) ) {
		return false;
	}
	iBytes = pQueue->Capacity * sizeof(uint64);
	if ( (pQueue->Capacity == 0u) &&
		((pQueue->Head != 0u) || (pQueue->Count != 0u)) ) {
		return false;
	}
	if ( (pQueue->Capacity != 0u) &&
		(pQueue->Head >= pQueue->Capacity) ) {
		return false;
	}
	return xrtMemRangeValid(pQueue->Tokens, iBytes) &&
		!xrtMemRangesOverlap(
			pQueue,
			sizeof(*pQueue),
			pQueue->Tokens,
			iBytes
		);
}



/* 校验会话固定状态，外部 channel 与 FIFO 在事务入口单独检查。 */
static bool xsshConnectionSessionValid(
	const xsshconnectionsession* pSession
)
{
	return xrtMemRangeValid(pSession, sizeof(*pSession)) &&
		(pSession->ObjectGuard == XSSH_CONNECTION_SESSION_GUARD) &&
		((pSession->Role == XSSH_ROLE_CLIENT) ||
		 (pSession->Role == XSSH_ROLE_SERVER)) &&
		(pSession->WritePending >= XSSH_CONNECTION_PACKET_NONE) &&
		(pSession->WritePending <=
		 XSSH_CONNECTION_PACKET_CHANNEL_FAILURE) &&
		(pSession->ReadPending >= XSSH_CONNECTION_PACKET_NONE) &&
		(pSession->ReadPending <=
		 XSSH_CONNECTION_PACKET_CHANNEL_FAILURE) &&
		(pSession->QueueAction >= XSSH_CONNECTION_QUEUE_NONE) &&
		(pSession->QueueAction <= XSSH_CONNECTION_QUEUE_POP);
}



/* 清除单个待提交事务，不修改调用方对象。 */
static void xsshConnectionSessionPendingClear(
	xsshconnectionsession* pSession
)
{
	memset(&pSession->ChannelBefore, 0, sizeof(pSession->ChannelBefore));
	memset(&pSession->ChannelPending, 0, sizeof(pSession->ChannelPending));
	memset(&pSession->QueueBefore, 0, sizeof(pSession->QueueBefore));
	pSession->Channel = NULL;
	pSession->Queue = NULL;
	pSession->QueueToken = 0u;
	pSession->QueueAction = XSSH_CONNECTION_QUEUE_NONE;
}



/* 失败会话丢弃全部内部事务快照，外部 channel 与 FIFO 保持原状。 */
static void xsshConnectionSessionSetFailed(
	xsshconnectionsession* pSession
)
{
	xsshConnectionSessionPendingClear(pSession);
	pSession->WritePending = XSSH_CONNECTION_PACKET_NONE;
	pSession->ReadPending = XSSH_CONNECTION_PACKET_NONE;
	pSession->WriteOrdinal = 0u;
	pSession->ReadOrdinal = 0u;
	pSession->Failed = true;
}



/* 返回当前角色应观察的 server USERAUTH_SUCCESS 方向。 */
static bool xsshConnectionCoreAuthenticated(
	const xsshconnectionsession* pSession,
	const xsshtransportcore* pCore
)
{
	return pSession->Role == XSSH_ROLE_SERVER ?
		pCore->State.LocalAuthSuccess : pCore->State.PeerAuthSuccess;
}



/* 校验 transport 地址、角色和无并行 packet 事务边界。 */
static bool xsshConnectionCoreValid(
	const xsshconnectionsession* pSession,
	const xsshtransportcore* pCore
)
{
	return xrtMemRangeValid(pCore, sizeof(*pCore)) &&
		!xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pCore,
			sizeof(*pCore)
		) && (pCore->State.Role == pSession->Role);
}



/* 严格分类一条 RFC 4254 connection payload。 */
static xsshcode xsshConnectionPacketRead(
	xbytesview Payload,
	xsshconnectionpacket* pPacket,
	bool* pRecognized
)
{
	xsshconnectionpacket Packet;
	uint8 iMessage;
	xsshcode Code;

	memset(&Packet, 0, sizeof(Packet));
	if ( pRecognized != NULL ) {
		*pRecognized = false;
	}
	Code = xrtSshMessageType(Payload, &iMessage);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iMessage == XSSH_MSG_GLOBAL_REQUEST ) {
		Packet.Kind = XSSH_CONNECTION_PACKET_GLOBAL_REQUEST;
		Code = xrtSshGlobalRequestRead(
			Payload,
			&Packet.Message.GlobalRequest
		);
	} else if ( iMessage == XSSH_MSG_REQUEST_SUCCESS ) {
		Packet.Kind = XSSH_CONNECTION_PACKET_GLOBAL_SUCCESS;
		Code = xrtSshGlobalSuccessRead(
			Payload,
			&Packet.Message.GlobalSuccess
		);
	} else if ( iMessage == XSSH_MSG_REQUEST_FAILURE ) {
		Packet.Kind = XSSH_CONNECTION_PACKET_GLOBAL_FAILURE;
		Code = xrtSshGlobalFailureRead(Payload);
	} else if ( iMessage == XSSH_MSG_CHANNEL_OPEN ) {
		Packet.Kind = XSSH_CONNECTION_PACKET_CHANNEL_OPEN;
		Code = xrtSshChannelOpenRead(
			Payload,
			&Packet.Message.ChannelOpen
		);
	} else if ( iMessage == XSSH_MSG_CHANNEL_OPEN_CONFIRMATION ) {
		Packet.Kind = XSSH_CONNECTION_PACKET_CHANNEL_CONFIRMATION;
		Code = xrtSshChannelOpenConfirmationRead(
			Payload,
			&Packet.Message.ChannelConfirmation
		);
	} else if ( iMessage == XSSH_MSG_CHANNEL_OPEN_FAILURE ) {
		Packet.Kind = XSSH_CONNECTION_PACKET_CHANNEL_OPEN_FAILURE;
		Code = xrtSshChannelOpenFailureRead(
			Payload,
			&Packet.Message.ChannelOpenFailure
		);
	} else if ( iMessage == XSSH_MSG_CHANNEL_WINDOW_ADJUST ) {
		Packet.Kind = XSSH_CONNECTION_PACKET_CHANNEL_ADJUST;
		Code = xrtSshChannelWindowAdjustRead(
			Payload,
			&Packet.Message.ChannelAdjust
		);
	} else if ( iMessage == XSSH_MSG_CHANNEL_DATA ) {
		Packet.Kind = XSSH_CONNECTION_PACKET_CHANNEL_DATA;
		Code = xrtSshChannelDataRead(
			Payload,
			&Packet.Message.ChannelData
		);
	} else if ( iMessage == XSSH_MSG_CHANNEL_EXTENDED_DATA ) {
		Packet.Kind = XSSH_CONNECTION_PACKET_CHANNEL_EXTENDED_DATA;
		Code = xrtSshChannelExtendedDataRead(
			Payload,
			&Packet.Message.ChannelExtendedData
		);
	} else if ( iMessage == XSSH_MSG_CHANNEL_EOF ) {
		Packet.Kind = XSSH_CONNECTION_PACKET_CHANNEL_EOF;
		Code = xrtSshChannelEofRead(
			Payload,
			&Packet.Message.Recipient
		);
	} else if ( iMessage == XSSH_MSG_CHANNEL_CLOSE ) {
		Packet.Kind = XSSH_CONNECTION_PACKET_CHANNEL_CLOSE;
		Code = xrtSshChannelCloseRead(
			Payload,
			&Packet.Message.Recipient
		);
	} else if ( iMessage == XSSH_MSG_CHANNEL_REQUEST ) {
		Packet.Kind = XSSH_CONNECTION_PACKET_CHANNEL_REQUEST;
		Code = xrtSshChannelRequestRead(
			Payload,
			&Packet.Message.ChannelRequest
		);
	} else if ( iMessage == XSSH_MSG_CHANNEL_SUCCESS ) {
		Packet.Kind = XSSH_CONNECTION_PACKET_CHANNEL_SUCCESS;
		Code = xrtSshChannelSuccessRead(
			Payload,
			&Packet.Message.Recipient
		);
	} else if ( iMessage == XSSH_MSG_CHANNEL_FAILURE ) {
		Packet.Kind = XSSH_CONNECTION_PACKET_CHANNEL_FAILURE;
		Code = xrtSshChannelFailureRead(
			Payload,
			&Packet.Message.Recipient
		);
	} else {
		return XSSH_ERROR_UNSUPPORTED;
	}
	if ( pRecognized != NULL ) {
		*pRecognized = true;
	}
	if ( Code == XSSH_OK ) {
		*pPacket = Packet;
	}
	return Code;
}



/* 判断分类是否需要一个已经存在的 channel。 */
static bool xsshConnectionPacketHasChannel(
	xsshconnectionpacketkind Kind
)
{
	return (Kind >= XSSH_CONNECTION_PACKET_CHANNEL_CONFIRMATION) &&
		(Kind <= XSSH_CONNECTION_PACKET_CHANNEL_FAILURE);
}



/* 返回 channel 消息中的本地或远端 recipient。 */
static uint32 xsshConnectionPacketRecipient(
	const xsshconnectionpacket* pPacket
)
{
	switch ( pPacket->Kind ) {
		case XSSH_CONNECTION_PACKET_CHANNEL_CONFIRMATION:
			return pPacket->Message.ChannelConfirmation.Recipient;
		case XSSH_CONNECTION_PACKET_CHANNEL_OPEN_FAILURE:
			return pPacket->Message.ChannelOpenFailure.Recipient;
		case XSSH_CONNECTION_PACKET_CHANNEL_ADJUST:
			return pPacket->Message.ChannelAdjust.Recipient;
		case XSSH_CONNECTION_PACKET_CHANNEL_DATA:
			return pPacket->Message.ChannelData.Recipient;
		case XSSH_CONNECTION_PACKET_CHANNEL_EXTENDED_DATA:
			return pPacket->Message.ChannelExtendedData.Recipient;
		case XSSH_CONNECTION_PACKET_CHANNEL_REQUEST:
			return pPacket->Message.ChannelRequest.Recipient;
		default:
			return pPacket->Message.Recipient;
	}
}



/* 校验短事务借用的 channel 不覆盖其他状态或 payload。 */
static bool xsshConnectionChannelValid(
	const xsshconnectionsession* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	const xsshchannelcore* pChannel
)
{
	return xrtMemRangeValid(pChannel, sizeof(*pChannel)) &&
		pChannel->Initialized &&
		!xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pChannel,
			sizeof(*pChannel)
		) && !xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			pChannel,
			sizeof(*pChannel)
		) && !xrtMemRangesOverlap(
			Payload.Data,
			Payload.Size,
			pChannel,
			sizeof(*pChannel)
		);
}



/* 校验短事务借用的 reply FIFO 及 token 存储不覆盖协议状态。 */
static bool xsshConnectionQueueBorrowValid(
	const xsshconnectionsession* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	const xsshchannelcore* pChannel,
	const xsshreplyqueue* pQueue
)
{
	size_t iBytes;

	if ( !xsshConnectionQueueValid(pQueue) ) {
		return false;
	}
	if ( (pChannel != NULL) && (pQueue == pSession->GlobalReplies) ) {
		return false;
	}
	iBytes = pQueue->Capacity * sizeof(uint64);
	return !xrtMemRangesOverlap(
		pSession,
		sizeof(*pSession),
		pQueue,
		sizeof(*pQueue)
	) && !xrtMemRangesOverlap(
		pCore,
		sizeof(*pCore),
		pQueue,
		sizeof(*pQueue)
	) && !xrtMemRangesOverlap(
		Payload.Data,
		Payload.Size,
		pQueue,
		sizeof(*pQueue)
	) && ((pChannel == NULL) ||
		!xrtMemRangesOverlap(
			pChannel,
			sizeof(*pChannel),
			pQueue,
			sizeof(*pQueue)
		)) && !xrtMemRangesOverlap(
		pSession,
		sizeof(*pSession),
		pQueue->Tokens,
		iBytes
	) && !xrtMemRangesOverlap(
		pCore,
		sizeof(*pCore),
		pQueue->Tokens,
		iBytes
	) && !xrtMemRangesOverlap(
		Payload.Data,
		Payload.Size,
		pQueue->Tokens,
		iBytes
	) && ((pChannel == NULL) ||
		!xrtMemRangesOverlap(
			pChannel,
			sizeof(*pChannel),
			pQueue->Tokens,
			iBytes
		));
}



/* 预留一次可靠提交后执行的 FIFO push。 */
static xsshcode xsshConnectionQueuePushPrepare(
	xsshconnectionsession* pSession,
	xsshreplyqueue* pQueue,
	uint64 iToken
)
{
	if ( pQueue->Count >= pQueue->Capacity ) {
		return XSSH_ERROR_SPACE;
	}
	pSession->Queue = pQueue;
	pSession->QueueBefore = *pQueue;
	pSession->QueueToken = iToken;
	pSession->QueueAction = XSSH_CONNECTION_QUEUE_PUSH;
	return XSSH_OK;
}



/* 预留一次可靠提交后执行的 FIFO pop，并借出当前关联 token。 */
static xsshcode xsshConnectionQueuePopPrepare(
	xsshconnectionsession* pSession,
	xsshreplyqueue* pQueue,
	uint64* pToken
)
{
	xsshcode Code = xrtSshReplyQueueFront(pQueue, pToken);

	if ( Code != XSSH_OK ) {
		return XSSH_ERROR_PROTOCOL;
	}
	pSession->Queue = pQueue;
	pSession->QueueBefore = *pQueue;
	pSession->QueueToken = *pToken;
	pSession->QueueAction = XSSH_CONNECTION_QUEUE_POP;
	return XSSH_OK;
}



/* 在副本中准备一条本端 channel 输出。 */
static xsshcode xsshConnectionWriteChannelPrepare(
	xsshconnectionsession* pSession,
	const xsshconnectionpacket* pPacket,
	xsshchannelcore* pChannel
)
{
	xsshchannelcore Channel = *pChannel;
	uint32 iLocal;
	uint32 iRemote;
	size_t iSize;
	xsshcode Code = XSSH_OK;

	switch ( pPacket->Kind ) {
		case XSSH_CONNECTION_PACKET_CHANNEL_OPEN:
			if ( (xrtSshChannelCorePhase(pChannel) !=
				XSSH_CHANNEL_CORE_OPENING) ||
				(pPacket->Message.ChannelOpen.Sender != pChannel->Local) ||
				(pPacket->Message.ChannelOpen.Window !=
				 pChannel->Window.ReceiveWindow) ||
				(pPacket->Message.ChannelOpen.MaxPacket !=
				 pChannel->Window.ReceiveMaxPacket) ) {
				return XSSH_ERROR_STATE;
			}
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_CONFIRMATION:
			if ( (xrtSshChannelCorePhase(pChannel) !=
				XSSH_CHANNEL_CORE_ACCEPTING) ||
				!xrtSshChannelCoreIds(pChannel, &iLocal, &iRemote) ||
				(pPacket->Message.ChannelConfirmation.Recipient != iRemote) ||
				(pPacket->Message.ChannelConfirmation.Sender != iLocal) ||
				(pPacket->Message.ChannelConfirmation.Window !=
				 pChannel->Window.ReceiveWindow) ||
				(pPacket->Message.ChannelConfirmation.MaxPacket !=
				 pChannel->Window.ReceiveMaxPacket) ) {
				return XSSH_ERROR_STATE;
			}
			Code = xrtSshChannelCoreAcceptCommit(&Channel);
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_OPEN_FAILURE:
			if ( (xrtSshChannelCorePhase(pChannel) !=
				XSSH_CHANNEL_CORE_ACCEPTING) ||
				!xrtSshChannelCoreIds(pChannel, &iLocal, &iRemote) ||
				(pPacket->Message.ChannelOpenFailure.Recipient != iRemote) ) {
				return XSSH_ERROR_STATE;
			}
			Code = xrtSshChannelCoreRejectCommit(
				&Channel,
				pPacket->Message.ChannelOpenFailure.Reason
			);
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_ADJUST:
			if ( !xrtSshChannelCoreIds(pChannel, &iLocal, &iRemote) ||
				(pPacket->Message.ChannelAdjust.Recipient != iRemote) ) {
				return XSSH_ERROR_STATE;
			}
			Code = xrtSshChannelCoreAdjustSendCommit(
				&Channel,
				pPacket->Message.ChannelAdjust.Bytes
			);
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_DATA:
			iSize = pPacket->Message.ChannelData.Data.Size;
			if ( (iSize > UINT32_MAX) ||
				!xrtSshChannelCoreIds(pChannel, &iLocal, &iRemote) ||
				(pPacket->Message.ChannelData.Recipient != iRemote) ) {
				return XSSH_ERROR_STATE;
			}
			Code = xrtSshChannelCoreDataSendCommit(
				&Channel,
				(uint32)iSize
			);
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_EXTENDED_DATA:
			iSize = pPacket->Message.ChannelExtendedData.Data.Size;
			if ( (iSize > UINT32_MAX) ||
				!xrtSshChannelCoreIds(pChannel, &iLocal, &iRemote) ||
				(pPacket->Message.ChannelExtendedData.Recipient != iRemote) ) {
				return XSSH_ERROR_STATE;
			}
			Code = xrtSshChannelCoreDataSendCommit(
				&Channel,
				(uint32)iSize
			);
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_EOF:
			if ( !xrtSshChannelCoreIds(pChannel, &iLocal, &iRemote) ||
				(pPacket->Message.Recipient != iRemote) ) {
				return XSSH_ERROR_STATE;
			}
			Code = xrtSshChannelCoreEofSendCommit(&Channel);
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_CLOSE:
			if ( !xrtSshChannelCoreIds(pChannel, &iLocal, &iRemote) ||
				(pPacket->Message.Recipient != iRemote) ) {
				return XSSH_ERROR_STATE;
			}
			Code = xrtSshChannelCoreCloseSendCommit(&Channel);
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_REQUEST:
			if ( !xrtSshChannelCoreIds(pChannel, &iLocal, &iRemote) ||
				(pPacket->Message.ChannelRequest.Recipient != iRemote) ||
				!xrtSshChannelCoreCanSendRequest(pChannel) ) {
				return XSSH_ERROR_STATE;
			}
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_SUCCESS:
		case XSSH_CONNECTION_PACKET_CHANNEL_FAILURE:
			if ( !xrtSshChannelCoreIds(pChannel, &iLocal, &iRemote) ||
				(pPacket->Message.Recipient != iRemote) ||
				!xrtSshChannelCoreCanSendRequest(pChannel) ) {
				return XSSH_ERROR_STATE;
			}
			break;
		default:
			return XSSH_ERROR_STATE;
	}
	if ( Code != XSSH_OK ) {
		return Code;
	}
	pSession->Channel = pChannel;
	pSession->ChannelBefore = *pChannel;
	pSession->ChannelPending = Channel;
	return XSSH_OK;
}



/* 通过调用方 resolver 取得当前本地 recipient 的状态对象。 */
static xsshcode xsshConnectionResolve(
	xsshconnectionsession* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	uint32 iLocal,
	xsshchannelcore** ppChannel,
	xsshreplyqueue** ppReplies
)
{
	xsshchannelcore* pChannel = NULL;
	xsshreplyqueue* pReplies = NULL;

	if ( (pSession->Resolve == NULL) || !pSession->Resolve(
		pSession->UserData,
		iLocal,
		&pChannel,
		&pReplies
	) || !xsshConnectionChannelValid(
		pSession,
		pCore,
		Payload,
		pChannel
	) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*ppChannel = pChannel;
	*ppReplies = pReplies;
	return XSSH_OK;
}



/* 在副本中准备一条 peer channel 输入。 */
static xsshcode xsshConnectionReadChannelPrepare(
	xsshconnectionsession* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	xsshconnectionpacket* pPacket
)
{
	xsshchannelcore* pChannel;
	xsshreplyqueue* pReplies;
	xsshchannelcore Channel;
	uint32 iRecipient = xsshConnectionPacketRecipient(pPacket);
	size_t iSize;
	xsshcode Code;

	Code = xsshConnectionResolve(
		pSession,
		pCore,
		Payload,
		iRecipient,
		&pChannel,
		&pReplies
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Channel = *pChannel;
	switch ( pPacket->Kind ) {
		case XSSH_CONNECTION_PACKET_CHANNEL_CONFIRMATION:
			Code = xrtSshChannelCoreConfirmationCommit(
				&Channel,
				&pPacket->Message.ChannelConfirmation
			);
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_OPEN_FAILURE:
			Code = xrtSshChannelCoreFailureCommit(
				&Channel,
				&pPacket->Message.ChannelOpenFailure
			);
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_ADJUST:
			Code = xrtSshChannelCoreAdjustReceiveCommit(
				&Channel,
				&pPacket->Message.ChannelAdjust
			);
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_DATA:
			iSize = pPacket->Message.ChannelData.Data.Size;
			Code = iSize > UINT32_MAX ? XSSH_ERROR_PROTOCOL :
				xrtSshChannelCoreDataReceiveCommit(
					&Channel,
					iRecipient,
					(uint32)iSize
				);
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_EXTENDED_DATA:
			iSize = pPacket->Message.ChannelExtendedData.Data.Size;
			Code = iSize > UINT32_MAX ? XSSH_ERROR_PROTOCOL :
				xrtSshChannelCoreDataReceiveCommit(
					&Channel,
					iRecipient,
					(uint32)iSize
				);
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_EOF:
			Code = xrtSshChannelCoreEofReceiveCommit(
				&Channel,
				iRecipient
			);
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_CLOSE:
			Code = xrtSshChannelCoreCloseReceiveCommit(
				&Channel,
				iRecipient
			);
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_REQUEST:
			Code = xrtSshChannelCoreRecipientCheck(
				pChannel,
				iRecipient
			);
			if ( (Code == XSSH_OK) &&
				!xrtSshChannelCoreCanReceiveRequest(pChannel) ) {
				Code = XSSH_ERROR_PROTOCOL;
			}
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_SUCCESS:
		case XSSH_CONNECTION_PACKET_CHANNEL_FAILURE:
			Code = xrtSshChannelCoreRecipientCheck(
				pChannel,
				iRecipient
			);
			if ( Code == XSSH_OK ) {
				if ( !xsshConnectionQueueBorrowValid(
					pSession,
					pCore,
					Payload,
					pChannel,
					pReplies
				) ) {
					Code = XSSH_ERROR_PROTOCOL;
				} else {
					Code = xsshConnectionQueuePopPrepare(
						pSession,
						pReplies,
						&pPacket->ReplyToken
					);
					pPacket->HasReplyToken = Code == XSSH_OK;
				}
			}
			break;
		default:
			return XSSH_ERROR_STATE;
	}
	if ( Code != XSSH_OK ) {
		return Code;
	}
	pSession->Channel = pChannel;
	pSession->ChannelBefore = *pChannel;
	pSession->ChannelPending = Channel;
	return XSSH_OK;
}



/* 校验外部对象未在短事务期间被其他执行流修改。 */
static bool xsshConnectionPendingStable(
	const xsshconnectionsession* pSession
)
{
	if ( (pSession->Channel != NULL) &&
		(memcmp(
			pSession->Channel,
			&pSession->ChannelBefore,
			sizeof(pSession->ChannelBefore)
		) != 0) ) {
		return false;
	}
	return (pSession->Queue == NULL) ||
		(memcmp(
			pSession->Queue,
			&pSession->QueueBefore,
			sizeof(pSession->QueueBefore)
		) == 0);
}



/* 在 transport 之后提交 FIFO 与 channel 候选状态。 */
static xsshcode xsshConnectionPendingCommit(
	xsshconnectionsession* pSession
)
{
	uint64 iToken;
	xsshcode Code = XSSH_OK;

	if ( !xsshConnectionPendingStable(pSession) ) {
		return XSSH_ERROR_STATE;
	}
	if ( pSession->QueueAction == XSSH_CONNECTION_QUEUE_POP ) {
		Code = xrtSshReplyQueueFront(pSession->Queue, &iToken);
		if ( (Code != XSSH_OK) || (iToken != pSession->QueueToken) ) {
			return XSSH_ERROR_STATE;
		}
	}
	if ( pSession->QueueAction == XSSH_CONNECTION_QUEUE_PUSH ) {
		Code = xrtSshReplyQueuePush(
			pSession->Queue,
			pSession->QueueToken
		);
	} else if ( pSession->QueueAction == XSSH_CONNECTION_QUEUE_POP ) {
		Code = xrtSshReplyQueuePop(pSession->Queue, &iToken);
	}
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( pSession->Channel != NULL ) {
		*pSession->Channel = pSession->ChannelPending;
	}
	xsshConnectionSessionPendingClear(pSession);
	return XSSH_OK;
}



/* 初始化调用方存储路由的 connection 会话。 */
bool xrtSshConnectionSessionInit(
	xsshconnectionsession* pSession,
	xsshrole Role,
	xsshchannelresolveproc pResolve,
	ptr pUserData,
	xsshreplyqueue* pGlobalReplies
)
{
	xsshconnectionsession Session;
	size_t iTokenBytes;

	if ( !xrtMemRangeValid(pSession, sizeof(*pSession)) ||
		((Role != XSSH_ROLE_CLIENT) && (Role != XSSH_ROLE_SERVER)) ) {
		return false;
	}
	if ( pGlobalReplies != NULL ) {
		if ( !xsshConnectionQueueValid(pGlobalReplies) ||
			(pGlobalReplies->Count != 0u) ) {
			return false;
		}
		iTokenBytes = pGlobalReplies->Capacity * sizeof(uint64);
		if ( xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pGlobalReplies,
			sizeof(*pGlobalReplies)
		) || xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pGlobalReplies->Tokens,
			iTokenBytes
		) ) {
			return false;
		}
	}
	memset(&Session, 0, sizeof(Session));
	Session.Role = Role;
	Session.Resolve = pResolve;
	Session.UserData = pUserData;
	Session.GlobalReplies = pGlobalReplies;
	Session.ObjectGuard = XSSH_CONNECTION_SESSION_GUARD;
	*pSession = Session;
	return true;
}



/* 清除 connection 会话本身。 */
void xrtSshConnectionSessionClear(xsshconnectionsession* pSession)
{
	if ( xrtMemRangeValid(pSession, sizeof(*pSession)) ) {
		memset(pSession, 0, sizeof(*pSession));
	}
}



/* 在认证成功的 transport 上开始 connection 编排。 */
xsshcode xrtSshConnectionSessionBegin(
	xsshconnectionsession* pSession,
	const xsshtransportcore* pCore
)
{
	if ( !xsshConnectionSessionValid(pSession) ||
		!xsshConnectionCoreValid(pSession, pCore) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pSession->Active || pSession->Failed ||
		pCore->Write.Active || pCore->Read.Active ||
		((pSession->GlobalReplies != NULL) &&
		 (!xsshConnectionQueueValid(pSession->GlobalReplies) ||
		  (pSession->GlobalReplies->Count != 0u))) ||
		!xrtSshTransportCoreKexComplete(pCore) ||
		!xsshConnectionCoreAuthenticated(pSession, pCore) ) {
		return XSSH_ERROR_STATE;
	}
	pSession->Active = true;
	return XSSH_OK;
}



/* 查询会话可用状态。 */
bool xrtSshConnectionSessionActive(
	const xsshconnectionsession* pSession
)
{
	return xsshConnectionSessionValid(pSession) &&
		pSession->Active && !pSession->Failed;
}



/* 准备本端 connection 输出及其外部状态候选。 */
xsshcode xrtSshConnectionSessionWritePrepare(
	xsshconnectionsession* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	xsshchannelcore* pChannel,
	xsshreplyqueue* pReplies,
	uint64 iReplyToken
)
{
	xsshconnectionpacket Packet;
	xsshreplyqueue* pQueue = NULL;
	bool bWantReply = false;
	xsshcode Code;

	if ( !xrtSshConnectionSessionActive(pSession) ||
		!xsshConnectionCoreValid(pSession, pCore) ||
		pCore->Write.Active || pCore->Read.Active ||
		(pSession->WritePending != XSSH_CONNECTION_PACKET_NONE) ||
		(pSession->ReadPending != XSSH_CONNECTION_PACKET_NONE) ||
		!xrtSshTransportCoreCanApplication(
			pCore,
			XSSH_TRANSPORT_LOCAL
		) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(Payload.Data, Payload.Size) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			Payload.Data,
			Payload.Size
		) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			Payload.Data,
			Payload.Size
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshConnectionPacketRead(Payload, &Packet, NULL);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( Packet.Kind == XSSH_CONNECTION_PACKET_CHANNEL_OPEN ) {
		if ( !xsshConnectionChannelValid(
			pSession,
			pCore,
			Payload,
			pChannel
		) ) {
			return XSSH_ERROR_ARGUMENT;
		}
		Code = xsshConnectionWriteChannelPrepare(
			pSession,
			&Packet,
			pChannel
		);
	} else if ( xsshConnectionPacketHasChannel(Packet.Kind) ) {
		if ( !xsshConnectionChannelValid(
			pSession,
			pCore,
			Payload,
			pChannel
		) ) {
			return XSSH_ERROR_ARGUMENT;
		}
		Code = xsshConnectionWriteChannelPrepare(
			pSession,
			&Packet,
			pChannel
		);
	} else if ( pChannel != NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( Code != XSSH_OK ) {
		xsshConnectionSessionPendingClear(pSession);
		return Code;
	}
	if ( Packet.Kind == XSSH_CONNECTION_PACKET_GLOBAL_REQUEST ) {
		if ( pReplies != NULL ) {
			xsshConnectionSessionPendingClear(pSession);
			return XSSH_ERROR_ARGUMENT;
		}
		bWantReply = Packet.Message.GlobalRequest.WantReply;
		pQueue = pSession->GlobalReplies;
	} else if ( Packet.Kind == XSSH_CONNECTION_PACKET_CHANNEL_REQUEST ) {
		bWantReply = Packet.Message.ChannelRequest.WantReply;
		pQueue = pReplies;
	}
	if ( bWantReply ) {
		if ( !xsshConnectionQueueBorrowValid(
			pSession,
			pCore,
			Payload,
			pChannel,
			pQueue
		) ) {
			xsshConnectionSessionPendingClear(pSession);
			return XSSH_ERROR_ARGUMENT;
		}
		Code = xsshConnectionQueuePushPrepare(
			pSession,
			pQueue,
			iReplyToken
		);
		if ( Code != XSSH_OK ) {
			xsshConnectionSessionPendingClear(pSession);
			return Code;
		}
	} else if ( pReplies != NULL ) {
		xsshConnectionSessionPendingClear(pSession);
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pCore->State.LocalPackets == UINT64_MAX ) {
		xsshConnectionSessionPendingClear(pSession);
		return XSSH_ERROR_STATE;
	}
	pSession->WritePending = Packet.Kind;
	pSession->WriteOrdinal = pCore->State.LocalPackets + 1u;
	return XSSH_OK;
}



/* 提交本端 connection 输出。 */
xsshcode xrtSshConnectionSessionWriteCommit(
	xsshconnectionsession* pSession,
	const xsshtransportcore* pCore
)
{
	xsshcode Code;

	if ( !xrtSshConnectionSessionActive(pSession) ||
		(pSession->WritePending == XSSH_CONNECTION_PACKET_NONE) ||
		!xsshConnectionCoreValid(pSession, pCore) || pCore->Write.Active ||
		!xrtSshTransportCoreCanApplication(
			pCore,
			XSSH_TRANSPORT_LOCAL
		) ||
		(pCore->State.LocalPackets != pSession->WriteOrdinal) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xsshConnectionPendingCommit(pSession);
	if ( Code != XSSH_OK ) {
		xsshConnectionSessionSetFailed(pSession);
		return Code;
	}
	pSession->WritePending = XSSH_CONNECTION_PACKET_NONE;
	pSession->WriteOrdinal = 0u;
	return XSSH_OK;
}



/* 无损放弃本端 connection 输出候选。 */
xsshcode xrtSshConnectionSessionWriteAbort(
	xsshconnectionsession* pSession
)
{
	if ( !xrtSshConnectionSessionActive(pSession) ||
		(pSession->WritePending == XSSH_CONNECTION_PACKET_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	xsshConnectionSessionPendingClear(pSession);
	pSession->WritePending = XSSH_CONNECTION_PACKET_NONE;
	pSession->WriteOrdinal = 0u;
	return XSSH_OK;
}



/* 准备 peer connection 输入及其外部状态候选。 */
xsshcode xrtSshConnectionSessionReadPrepare(
	xsshconnectionsession* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	xsshconnectionpacket* pPacket
)
{
	xsshconnectionpacket Packet;
	uint8 iMessage;
	bool bRecognized;
	xsshcode Code;

	if ( !xrtSshConnectionSessionActive(pSession) ||
		!xsshConnectionCoreValid(pSession, pCore) ||
		!pCore->Read.Active || pCore->Write.Active ||
		(pSession->WritePending != XSSH_CONNECTION_PACKET_NONE) ||
		(pSession->ReadPending != XSSH_CONNECTION_PACKET_NONE) ||
		!xrtSshTransportCoreCanApplication(
			pCore,
			XSSH_TRANSPORT_PEER
		) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(Payload.Data, Payload.Size) ||
		!xrtMemRangeValid(pPacket, sizeof(*pPacket)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			Payload.Data,
			Payload.Size
		) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			Payload.Data,
			Payload.Size
		) || xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pPacket,
			sizeof(*pPacket)
		) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			pPacket,
			sizeof(*pPacket)
		) || xrtMemRangesOverlap(
			Payload.Data,
			Payload.Size,
			pPacket,
			sizeof(*pPacket)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshMessageType(Payload, &iMessage);
	if ( Code != XSSH_OK ) {
		xsshConnectionSessionSetFailed(pSession);
		return XSSH_ERROR_PROTOCOL;
	}
	if ( iMessage != pCore->Read.Message ) {
		return XSSH_ERROR_STATE;
	}
	Code = xsshConnectionPacketRead(Payload, &Packet, &bRecognized);
	if ( Code != XSSH_OK ) {
		if ( bRecognized ) {
			xsshConnectionSessionSetFailed(pSession);
			return XSSH_ERROR_PROTOCOL;
		}
		return Code;
	}
	if ( xsshConnectionPacketHasChannel(Packet.Kind) ) {
		Code = xsshConnectionReadChannelPrepare(
			pSession,
			pCore,
			Payload,
			&Packet
		);
	} else if ( (Packet.Kind == XSSH_CONNECTION_PACKET_GLOBAL_SUCCESS) ||
		(Packet.Kind == XSSH_CONNECTION_PACKET_GLOBAL_FAILURE) ) {
		if ( !xsshConnectionQueueBorrowValid(
			pSession,
			pCore,
			Payload,
			NULL,
			pSession->GlobalReplies
		) ) {
			Code = XSSH_ERROR_PROTOCOL;
		} else {
			Code = xsshConnectionQueuePopPrepare(
				pSession,
				pSession->GlobalReplies,
				&Packet.ReplyToken
			);
			Packet.HasReplyToken = Code == XSSH_OK;
		}
	}
	if ( Code != XSSH_OK ) {
		xsshConnectionSessionSetFailed(pSession);
		return Code;
	}
	if ( pCore->State.PeerPackets == UINT64_MAX ) {
		xsshConnectionSessionSetFailed(pSession);
		return XSSH_ERROR_STATE;
	}
	pSession->ReadPending = Packet.Kind;
	pSession->ReadOrdinal = pCore->State.PeerPackets + 1u;
	*pPacket = Packet;
	return XSSH_OK;
}



/* 提交 peer connection 输入。 */
xsshcode xrtSshConnectionSessionReadCommit(
	xsshconnectionsession* pSession,
	const xsshtransportcore* pCore
)
{
	xsshcode Code;

	if ( !xrtSshConnectionSessionActive(pSession) ||
		(pSession->ReadPending == XSSH_CONNECTION_PACKET_NONE) ||
		!xsshConnectionCoreValid(pSession, pCore) || pCore->Read.Active ||
		!xrtSshTransportCoreCanApplication(
			pCore,
			XSSH_TRANSPORT_PEER
		) ||
		(pCore->State.PeerPackets != pSession->ReadOrdinal) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xsshConnectionPendingCommit(pSession);
	if ( Code != XSSH_OK ) {
		xsshConnectionSessionSetFailed(pSession);
		return Code;
	}
	pSession->ReadPending = XSSH_CONNECTION_PACKET_NONE;
	pSession->ReadOrdinal = 0u;
	return XSSH_OK;
}



/* 放弃不可回滚的 peer 输入并终止会话。 */
xsshcode xrtSshConnectionSessionReadAbort(
	xsshconnectionsession* pSession
)
{
	if ( !xrtSshConnectionSessionActive(pSession) ||
		(pSession->ReadPending == XSSH_CONNECTION_PACKET_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	xsshConnectionSessionSetFailed(pSession);
	return XSSH_OK;
}



/* 显式终止 connection 会话。 */
void xrtSshConnectionSessionFail(xsshconnectionsession* pSession)
{
	if ( xsshConnectionSessionValid(pSession) && pSession->Active ) {
		xsshConnectionSessionSetFailed(pSession);
	}
}

#endif
