#include <string.h>

#include <xrt/ssh_session_reader.h>



#if defined(XSSH_FEATURE_SESSION_READER)

#define XSSH_SESSION_READER_GUARD UINT32_C(0x53535244)



/* 校验读取器、动态链和借用会话仍属于同一个缓冲池。 */
static bool xsshSessionReaderValid(const xsshsessionreader* pReader)
{
	const xsshtransporttcp* pTransport;

	if ( !xrtMemRangeValid(pReader, sizeof(*pReader)) ||
		(pReader->Guard != XSSH_SESSION_READER_GUARD) ||
		(pReader->Session == NULL) ||
		(pReader->State < XSSH_SESSION_READER_IDLE) ||
		(pReader->State > XSSH_SESSION_READER_READY) ) {
		return false;
	}
	pTransport = xrtSshSessionTcpTransportConst(pReader->Session);
	return (pTransport != NULL) &&
		(pReader->Plain.Pool == pTransport->Output.Pool) &&
		(pReader->HostKey.Pool == pTransport->Output.Pool);
}



/* 放弃尚未提交的动态尾部，并清空 packet 借用元数据。 */
static bool xsshSessionReaderReset(xsshsessionreader* pReader)
{
	bool bSuccess = true;

	if ( pReader->Plain.Reserved != NULL ) {
		bSuccess = xrtNetBufCancel(&pReader->Plain) && bSuccess;
	}
	if ( pReader->HostKey.Reserved != NULL ) {
		bSuccess = xrtNetBufCancel(&pReader->HostKey) && bSuccess;
	}
	pReader->Input = NULL;
	pReader->PlainSpan = (xnetwspan){ NULL, 0u };
	pReader->HostKeySpan = (xnetwspan){ NULL, 0u };
	memset(&pReader->Need, 0, sizeof(pReader->Need));
	pReader->HostKeyOldSize = 0u;
	pReader->HostKeySize = 0u;
	pReader->State = XSSH_SESSION_READER_IDLE;
	return bSuccess;
}



/* 线路或动态工作区错误终止读取器绑定的完整会话。 */
static xsshcode xsshSessionReaderFail(
	xsshsessionreader* pReader,
	xsshcode Code,
	cstr sMessage
)
{
	xrtSetErrorInfo(
		XERR_INTERNAL,
		"xrt.ssh",
		(int32)Code,
		sMessage
	);
	xrtSshSessionCoreFail(&pReader->Session->Session);
	xrtSshTransportCoreClose(&pReader->Session->Transport.Core);
	(void)xsshSessionReaderReset(pReader);
	return Code;
}



/* 校验 Prepare 的输入和输出不会覆盖读取器或绑定会话。 */
static bool xsshSessionReaderPrepareArguments(
	const xsshsessionreader* pReader,
	const xnetbuf* pInput,
	const xsshsessiontcppacket* pPacket
)
{
	return (pInput != NULL) &&
		xrtMemRangeValid(pPacket, sizeof(*pPacket)) &&
		!xrtMemRangesOverlap(
			pReader,
			sizeof(*pReader),
			pInput,
			sizeof(*pInput)
		) && !xrtMemRangesOverlap(
			pReader,
			sizeof(*pReader),
			pPacket,
			sizeof(*pPacket)
		) && !xrtMemRangesOverlap(
			pReader->Session,
			sizeof(*pReader->Session),
			pPacket,
			sizeof(*pPacket)
		) && !xrtMemRangesOverlap(
			pInput,
			sizeof(*pInput),
			pPacket,
			sizeof(*pPacket)
		);
}



/* 首次探测后让明文包借用输入，加密包只申请本次解密需要的连续空间。 */
static xsshcode xsshSessionReaderPlainPrepare(
	xsshsessionreader* pReader,
	xnetbuf* pInput
)
{
	xsshcode Code;

	Code = xrtSshSessionTcpReadInspect(
		pReader->Session,
		pInput,
		&pReader->Need
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtNetBufSize(pInput) < pReader->Need.WireSize ) {
		return XSSH_NEED_MORE;
	}
	if ( pReader->Need.PlainSize != 0u ) {
		if ( !xrtNetBufReserve(
			&pReader->Plain,
			pReader->Need.PlainSize,
			&pReader->PlainSpan
		) ) {
			return XSSH_ERROR_SPACE;
		}
	} else {
		pReader->PlainSpan = (xnetwspan){ NULL, 0u };
	}
	pReader->Input = pInput;
	return XSSH_OK;
}



/* 按 ECDH_REPLY 给出的精确长度申请新主机公钥尾部。 */
static xsshcode xsshSessionReaderHostKeyPrepare(
	xsshsessionreader* pReader
)
{
	if ( pReader->HostKeySize == 0u ) {
		return XSSH_ERROR_STATE;
	}
	if ( (pReader->HostKey.Reserved != NULL) &&
		(pReader->HostKeySpan.Size < pReader->HostKeySize) ) {
		if ( !xrtNetBufCancel(&pReader->HostKey) ) {
			return XSSH_ERROR_STATE;
		}
		pReader->HostKeySpan = (xnetwspan){ NULL, 0u };
	}
	if ( pReader->HostKey.Reserved == NULL ) {
		pReader->HostKeyOldSize = xrtNetBufSize(&pReader->HostKey);
		if ( !xrtNetBufReserve(
			&pReader->HostKey,
			pReader->HostKeySize,
			&pReader->HostKeySpan
		) ) {
			return XSSH_ERROR_SPACE;
		}
	}
	return XSSH_OK;
}



/* 使用当前动态空间重试同一未消费 packet 的上层解析。 */
static xsshcode xsshSessionReaderPacketPrepare(
	xsshsessionreader* pReader,
	uint64 iNowMs,
	xsshsessiontcppacket* pPacket
)
{
	xsshsessiontcppacket Packet;
	size_t iHostKeySize = pReader->HostKeySize;
	xsshcode Code;

	memset(&Packet, 0, sizeof(Packet));
	Code = xrtSshSessionTcpReadPrepare(
		pReader->Session,
		pReader->Input,
		pReader->PlainSpan.Data,
		pReader->PlainSpan.Size,
		pReader->HostKey.Reserved != NULL ?
			pReader->HostKeySpan.Data : NULL,
		pReader->HostKey.Reserved != NULL ?
			pReader->HostKeySpan.Size : 0u,
		&iHostKeySize,
		iNowMs,
		&Packet
	);
	pReader->HostKeySize = iHostKeySize;
	if ( Code == XSSH_OK ) {
		pReader->State = XSSH_SESSION_READER_READY;
		*pPacket = Packet;
	} else if ( Code == XSSH_ERROR_SPACE ) {
		pReader->State = (pReader->HostKeySize >
			pReader->HostKeySpan.Size) ?
			XSSH_SESSION_READER_HOST_KEY :
			XSSH_SESSION_READER_RETRY;
	}
	return Code;
}



/* 初始化两个空动态链并绑定已经有效的 TCP 会话。 */
bool xrtSshSessionReaderInit(
	xsshsessionreader* pReader,
	xnetbufpool* pPool,
	xsshsessiontcp* pSession
)
{
	xsshsessionreader Reader;
	const xsshtransporttcp* pTransport;

	if ( !xrtMemRangeValid(pReader, sizeof(*pReader)) ||
		!xrtMemRangeValid(pSession, sizeof(*pSession)) ||
		xrtMemRangesOverlap(
			pReader,
			sizeof(*pReader),
			pSession,
			sizeof(*pSession)
		) ) {
		return false;
	}
	pTransport = xrtSshSessionTcpTransportConst(pSession);
	if ( (pTransport == NULL) || (pTransport->Output.Pool != pPool) ) {
		return false;
	}
	memset(&Reader, 0, sizeof(Reader));
	if ( !xrtNetBufInit(&Reader.Plain, pPool) ||
		!xrtNetBufInit(&Reader.HostKey, pPool) ) {
		xrtNetBufClear(&Reader.Plain);
		xrtNetBufClear(&Reader.HostKey);
		return false;
	}
	Reader.Session = pSession;
	Reader.State = XSSH_SESSION_READER_IDLE;
	Reader.Guard = XSSH_SESSION_READER_GUARD;
	*pReader = Reader;
	return true;
}



/* 清理前必须终止仍借用输入或动态空间的 packet 事务。 */
void xrtSshSessionReaderClear(xsshsessionreader* pReader)
{
	if ( pReader == NULL ) {
		return;
	}
	if ( xsshSessionReaderValid(pReader) ) {
		if ( pReader->State != XSSH_SESSION_READER_IDLE ) {
			(void)xrtSshSessionTcpReadAbort(pReader->Session);
		}
		(void)xsshSessionReaderReset(pReader);
		xrtNetBufClear(&pReader->Plain);
		xrtNetBufClear(&pReader->HostKey);
	}
	memset(pReader, 0, sizeof(*pReader));
}



/* 返回借用的可变 TCP 会话。 */
xsshsessiontcp* xrtSshSessionReaderSession(xsshsessionreader* pReader)
{
	return xsshSessionReaderValid(pReader) ? pReader->Session : NULL;
}



/* 返回借用的只读 TCP 会话。 */
const xsshsessiontcp* xrtSshSessionReaderSessionConst(
	const xsshsessionreader* pReader
)
{
	return xsshSessionReaderValid(pReader) ? pReader->Session : NULL;
}



/* 无效对象返回不会与任何可提交事务混淆的独立状态。 */
xsshsessionreaderstate xrtSshSessionReaderState(
	const xsshsessionreader* pReader
)
{
	return xsshSessionReaderValid(pReader) ?
		pReader->State : XSSH_SESSION_READER_INVALID;
}



/* 首次探测明文，空间不足时在同一调用中按精确主机公钥长度重试。 */
xsshcode xrtSshSessionReaderPrepare(
	xsshsessionreader* pReader,
	xnetbuf* pInput,
	uint64 iNowMs,
	xsshsessiontcppacket* pPacket
)
{
	xsshcode Code;

	if ( !xsshSessionReaderValid(pReader) ||
		!xsshSessionReaderPrepareArguments(pReader, pInput, pPacket) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pReader->State == XSSH_SESSION_READER_READY ) {
		return XSSH_ERROR_STATE;
	}
	if ( pReader->State == XSSH_SESSION_READER_IDLE ) {
		Code = xsshSessionReaderPlainPrepare(pReader, pInput);
		if ( Code != XSSH_OK ) {
			return Code;
		}
	} else if ( pReader->Input != pInput ) {
		return XSSH_ERROR_ARGUMENT;
	}
	for (;;) {
		if ( pReader->State == XSSH_SESSION_READER_HOST_KEY ) {
			Code = xsshSessionReaderHostKeyPrepare(pReader);
			if ( Code != XSSH_OK ) {
				return Code;
			}
		}
		Code = xsshSessionReaderPacketPrepare(
			pReader,
			iNowMs,
			pPacket
		);
		if ( (Code == XSSH_ERROR_SPACE) &&
			(pReader->State == XSSH_SESSION_READER_HOST_KEY) ) {
			continue;
		}
		if ( (Code != XSSH_OK) && (Code != XSSH_ERROR_SPACE) ) {
			(void)xsshSessionReaderReset(pReader);
		}
		return Code;
	}
}



/* 发布可持久借用的主机公钥，再提交唯一 packet 并释放明文工作区。 */
xsshcode xrtSshSessionReaderCommit(
	xsshsessionreader* pReader,
	uint64 iNowMs,
	xsshrekeydecision* pDecision
)
{
	bool bHostKey;
	xsshcode Code;
	xnetbufpool* pPool;

	if ( !xsshSessionReaderValid(pReader) ||
		(pReader->State != XSSH_SESSION_READER_READY) ||
		!xrtMemRangeValid(pDecision, sizeof(*pDecision)) ||
		xrtMemRangesOverlap(
			pReader,
			sizeof(*pReader),
			pDecision,
			sizeof(*pDecision)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	pPool = pReader->Plain.Pool;
	bHostKey = pReader->HostKey.Reserved != NULL;
	if ( bHostKey && !xrtNetBufCommit(
		&pReader->HostKey,
		pReader->HostKeySize
	) ) {
		return xsshSessionReaderFail(
			pReader,
			XSSH_ERROR_STATE,
			"SSH host key workspace commit failed"
		);
	}
	Code = xrtSshSessionTcpReadCommit(
		pReader->Session,
		iNowMs,
		pDecision
	);
	if ( Code != XSSH_OK ) {
		xrtNetBufClear(&pReader->HostKey);
		if ( !xrtNetBufInit(
			&pReader->HostKey,
			pPool
		) ) {
			return xsshSessionReaderFail(
				pReader,
				XSSH_ERROR_STATE,
				"SSH host key buffer reset failed"
			);
		}
		(void)xsshSessionReaderReset(pReader);
		return Code;
	}
	if ( bHostKey && (xrtNetBufConsume(
		&pReader->HostKey,
		pReader->HostKeyOldSize
	) != pReader->HostKeyOldSize) ) {
		return xsshSessionReaderFail(
			pReader,
			XSSH_ERROR_STATE,
			"SSH previous host key release failed"
		);
	}
	if ( !xsshSessionReaderReset(pReader) ) {
		return xsshSessionReaderFail(
			pReader,
			XSSH_ERROR_STATE,
			"SSH packet workspace release failed"
		);
	}
	return XSSH_OK;
}



/* transport 消费由基础组合层负责，读取器只释放自己的动态工作区。 */
xsshcode xrtSshSessionReaderAbort(xsshsessionreader* pReader)
{
	xsshcode Code;

	if ( !xsshSessionReaderValid(pReader) ||
		(pReader->State == XSSH_SESSION_READER_IDLE) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshSessionTcpReadAbort(pReader->Session);
	if ( !xsshSessionReaderReset(pReader) && (Code == XSSH_OK) ) {
		Code = XSSH_ERROR_STATE;
	}
	return Code;
}



/* 未提交新 key 优先返回 staging，其他状态返回最近提交的连续 key。 */
xbytesview xrtSshSessionReaderHostKey(
	const xsshsessionreader* pReader
)
{
	xnetspan Span;

	if ( !xsshSessionReaderValid(pReader) ) {
		return (xbytesview){ NULL, 0u };
	}
	if ( pReader->HostKey.Reserved != NULL ) {
		return (xbytesview){
			pReader->HostKeySpan.Data,
			pReader->HostKeySize
		};
	}
	if ( !xrtNetBufFront(&pReader->HostKey, &Span) ||
		(Span.Size < xrtNetBufSize(&pReader->HostKey)) ) {
		return (xbytesview){ NULL, 0u };
	}
	return (xbytesview){ Span.Data, xrtNetBufSize(&pReader->HostKey) };
}

#endif
