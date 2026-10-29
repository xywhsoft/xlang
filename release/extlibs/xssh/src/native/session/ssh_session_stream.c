#include <string.h>

#include <xrt/time.h>
#include <xrt/ssh_session_stream.h>



#if defined(XSSH_FEATURE_SESSION_STREAM)

#define XSSH_SESSION_STREAM_GUARD UINT32_C(0x53535354)



static void xsshSessionStreamOpen(xnetstream* pStream, ptr pData);
static void xsshSessionStreamRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
);
static void xsshSessionStreamEnd(xnetstream* pStream, ptr pData);
static void xsshSessionStreamHighWater(
	xnetstream* pStream,
	size_t iQueued,
	ptr pData
);
static void xsshSessionStreamLowWater(
	xnetstream* pStream,
	size_t iQueued,
	ptr pData
);
static void xsshSessionStreamDrain(xnetstream* pStream, ptr pData);
static void xsshSessionStreamClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
);



static const xnetstreamevents xsshSessionStreamEvents = {
	xsshSessionStreamOpen,
	xsshSessionStreamRead,
	xsshSessionStreamEnd,
	xsshSessionStreamHighWater,
	xsshSessionStreamLowWater,
	xsshSessionStreamDrain,
	xsshSessionStreamClose
};



/* 验证驱动哨兵和公开状态，不触碰尚未初始化的会话对象。 */
static bool xsshSessionStreamValid(const xsshsessionstream* pSession)
{
	return xrtMemRangeValid(pSession, sizeof(*pSession)) &&
		(pSession->Guard == XSSH_SESSION_STREAM_GUARD) &&
		(pSession->State >= XSSH_SESSION_STREAM_CREATED) &&
		(pSession->State <= XSSH_SESSION_STREAM_CLOSED);
}



/* 验证活动驱动仍在绑定 Stream 的所属 Worker 上执行。 */
static bool xsshSessionStreamCurrent(const xsshsessionstream* pSession)
{
	xnetworker* pWorker;

	if ( !xsshSessionStreamValid(pSession) ||
		(pSession->Stream == NULL) ) {
		return false;
	}
	pWorker = xrtNetStreamWorker(pSession->Stream);
	return (pWorker != NULL) && xrtNetWorkerIsCurrent(pWorker);
}



/* SSH rekey 时钟使用毫秒，XRT 单调时钟使用微秒。 */
static uint64 xsshSessionStreamNow(void)
{
	return xrtClock() / 1000u;
}



/* 暂停新的 TCP 读取；已经到达的 completion 仍由同一缓冲链承接。 */
static void xsshSessionStreamPause(xsshsessionstream* pSession)
{
	if ( !pSession->Paused && (pSession->Stream != NULL) ) {
		xrtNetStreamPause(pSession->Stream);
		pSession->Paused = true;
	}
}



/* 仅在没有 HOLD、OOM 或写背压原因时恢复读取。 */
static bool xsshSessionStreamResume(xsshsessionstream* pSession)
{
	if ( !pSession->Paused || pSession->WritePaused ||
		(pSession->State != XSSH_SESSION_STREAM_OPEN) ) {
		return true;
	}
	if ( pSession->ReadEnded ) {
		pSession->Paused = false;
		return true;
	}
	if ( !xrtNetStreamResume(pSession->Stream) ) {
		return false;
	}
	pSession->Paused = false;
	return true;
}



/* 把协议错误补成结构化错误并同步通知应用。 */
static void xsshSessionStreamErrorNotify(
	xsshsessionstream* pSession,
	xsshcode Code,
	xerrkind Kind,
	cstr sMessage
)
{
	const xerror* pError = xrtGetError();
	bool bRelevant = (pError != NULL) && (
		(xrtErrorFind(pError, "xrt.ssh", (int32)Code) != NULL) ||
		(((Kind == XERR_IO) || (Kind == XERR_MEMORY)) &&
		 (xrtErrorIs(pError, Kind) != NULL))
	);

	if ( !bRelevant ) {
		xrtSetErrorInfo(Kind, "xrt.ssh", (int32)Code, sMessage);
		pError = xrtGetError();
	}
	if ( pSession->Events.Error != NULL ) {
		pSession->Events.Error(
			pSession,
			Code,
			pError,
			pSession->UserData
		);
	}
}



/* 尽力回滚未接管事务，随后让 Stream 进入唯一异常关闭路径。 */
static void xsshSessionStreamStop(xsshsessionstream* pSession)
{
	if ( !xsshSessionStreamValid(pSession) ||
		(pSession->State >= XSSH_SESSION_STREAM_CLOSING) ) {
		return;
	}
	if ( pSession->SessionReady ) {
		xsshsessionreaderstate ReaderState =
			xrtSshSessionReaderState(&pSession->Reader);
		xsshsessionaction Action =
			xrtSshSessionTcpAction(&pSession->Session);

		if ( (ReaderState != XSSH_SESSION_READER_IDLE) &&
			(ReaderState != XSSH_SESSION_READER_INVALID) ) {
			(void)xrtSshSessionReaderAbort(&pSession->Reader);
		} else if ( Action == XSSH_SESSION_ACTION_READ_PENDING ) {
			(void)xrtSshSessionTcpReadAbort(&pSession->Session);
		}
		if ( xrtSshSessionTcpAction(&pSession->Session) ==
			XSSH_SESSION_ACTION_WRITE_PENDING ) {
			(void)xrtSshSessionTcpWriteAbort(&pSession->Session);
		}
	}
	pSession->State = XSSH_SESSION_STREAM_CLOSING;
	if ( pSession->Stream != NULL ) {
		(void)xrtNetStreamAbort(pSession->Stream);
	}
}



/* 通知非空 rekey 建议；是否开始 KEXINIT 仍由应用决定。 */
static void xsshSessionStreamRekey(
	xsshsessionstream* pSession,
	xsshrekeydecision Decision
)
{
	if ( (Decision != XSSH_REKEY_NONE) &&
		(pSession->Events.Rekey != NULL) ) {
		pSession->Events.Rekey(
			pSession,
			Decision,
			pSession->UserData
		);
	}
}



/* 判断当前动作是否允许从 TCP 缓冲准备一个 SSH packet。 */
static bool xsshSessionStreamPacketAction(xsshsessionaction Action)
{
	return (Action == XSSH_SESSION_ACTION_READ_KEXINIT) ||
		(Action == XSSH_SESSION_ACTION_READ_ECDH_INIT) ||
		(Action == XSSH_SESSION_ACTION_READ_ECDH_REPLY) ||
		(Action == XSSH_SESSION_ACTION_READ_NEWKEYS) ||
		(Action == XSSH_SESSION_ACTION_READ_SERVICE_REQUEST) ||
		(Action == XSSH_SESSION_ACTION_READ_SERVICE_ACCEPT) ||
		(Action == XSSH_SESSION_ACTION_READ_AUTH_REQUEST) ||
		(Action == XSSH_SESSION_ACTION_READ_AUTH_RESULT) ||
		(Action == XSSH_SESSION_ACTION_CONNECTION);
}



/* 只在动作变化时通知，避免未处理动作形成忙循环。 */
static bool xsshSessionStreamActionNotify(
	xsshsessionstream* pSession,
	xsshsessionaction Action
)
{
	xsshsessionaction After;

	if ( pSession->NotifiedAction == Action ) {
		return false;
	}
	pSession->NotifiedAction = Action;
	if ( pSession->Events.Action != NULL ) {
		pSession->Events.Action(
			pSession,
			Action,
			pSession->UserData
		);
	}
	if ( (pSession->State != XSSH_SESSION_STREAM_OPEN) ||
		(xrtNetStreamState(pSession->Stream) != XNET_STREAM_OPEN) ) {
		return true;
	}
	After = xrtSshSessionTcpAction(&pSession->Session);
	return After != Action;
}



/* 自动把上层已经准备好的唯一输出事务交给有界 TCP 队列。 */
static xsshcode xsshSessionStreamWrite(
	xsshsessionstream* pSession,
	bool* pProgress
)
{
	xsshrekeydecision Decision = XSSH_REKEY_NONE;
	xnetresult Result;

	Result = xrtSshSessionTcpWriteSubmit(
		&pSession->Session,
		pSession->Stream,
		xsshSessionStreamNow(),
		&Decision
	);
	if ( Result == XNET_RESULT_AGAIN ) {
		pSession->WritePaused = true;
		xsshSessionStreamPause(pSession);
		return XSSH_OK;
	}
	if ( Result != XNET_RESULT_OK ) {
		xsshSessionStreamErrorNotify(
			pSession,
			XSSH_ERROR_STATE,
			XERR_IO,
			"SSH output could not enter the TCP queue"
		);
		xsshSessionStreamStop(pSession);
		return XSSH_ERROR_STATE;
	}
	pSession->WritePaused = false;
	pSession->NotifiedAction = XSSH_SESSION_ACTION_NONE;
	*pProgress = true;
	xsshSessionStreamRekey(pSession, Decision);
	if ( !xsshSessionStreamResume(pSession) ) {
		xsshSessionStreamErrorNotify(
			pSession,
			XSSH_ERROR_STATE,
			XERR_IO,
			"SSH input could not resume after TCP backpressure"
		);
		xsshSessionStreamStop(pSession);
		return XSSH_ERROR_STATE;
	}
	return XSSH_OK;
}



/* 按回调决定提交、保留或拒绝 peer identification。 */
static xsshcode xsshSessionStreamIdentification(
	xsshsessionstream* pSession,
	bool* pProgress
)
{
	xsshsessionstreamdecision Decision = XSSH_SESSION_STREAM_ACCEPT;
	xsshrekeydecision Rekey = XSSH_REKEY_NONE;
	xstrview Version;
	xsshcode Code;

	if ( pSession->Input == NULL ) {
		return XSSH_NEED_MORE;
	}
	Code = xrtSshSessionTcpIdentificationReadPrepare(
		&pSession->Session,
		pSession->Input,
		&Version
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	pSession->Version = Version;
	if ( pSession->Events.Identification != NULL ) {
		Decision = pSession->Events.Identification(
			pSession,
			Version,
			pSession->UserData
		);
	}
	if ( Decision == XSSH_SESSION_STREAM_HOLD ) {
		pSession->State = XSSH_SESSION_STREAM_HOLD_IDENTIFICATION;
		xsshSessionStreamPause(pSession);
		return XSSH_OK;
	}
	if ( Decision != XSSH_SESSION_STREAM_ACCEPT ) {
		(void)xrtSshSessionTcpReadAbort(&pSession->Session);
		pSession->State = XSSH_SESSION_STREAM_CLOSING;
		(void)xrtNetStreamAbort(pSession->Stream);
		return Decision == XSSH_SESSION_STREAM_ABORT ?
			XSSH_OK : XSSH_ERROR_CALLBACK;
	}
	Code = xrtSshSessionTcpReadCommit(
		&pSession->Session,
		xsshSessionStreamNow(),
		&Rekey
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	pSession->Version = (xstrview){ NULL, 0u };
	pSession->NotifiedAction = XSSH_SESSION_ACTION_NONE;
	*pProgress = true;
	xsshSessionStreamRekey(pSession, Rekey);
	return XSSH_OK;
}



/* 按回调决定提交、保留或拒绝一个已经认证的 SSH packet。 */
static xsshcode xsshSessionStreamPacketRead(
	xsshsessionstream* pSession,
	bool* pProgress
)
{
	xsshsessionstreamdecision Decision = XSSH_SESSION_STREAM_ACCEPT;
	xsshrekeydecision Rekey = XSSH_REKEY_NONE;
	xsshsessiontcppacket Packet;
	xsshcode Code;

	if ( (pSession->Input == NULL) ||
		xrtNetBufEmpty(pSession->Input) ) {
		return XSSH_NEED_MORE;
	}
	Code = xrtSshSessionReaderPrepare(
		&pSession->Reader,
		pSession->Input,
		xsshSessionStreamNow(),
		&Packet
	);
	if ( Code != XSSH_OK ) {
		if ( Code == XSSH_ERROR_SPACE ) {
			pSession->State = XSSH_SESSION_STREAM_RETRY;
			xsshSessionStreamPause(pSession);
		}
		return Code;
	}
	pSession->Packet = Packet;
	if ( pSession->Events.Packet != NULL ) {
		Decision = pSession->Events.Packet(
			pSession,
			&pSession->Packet,
			pSession->UserData
		);
	}
	if ( Decision == XSSH_SESSION_STREAM_HOLD ) {
		pSession->State = XSSH_SESSION_STREAM_HOLD_PACKET;
		xsshSessionStreamPause(pSession);
		return XSSH_OK;
	}
	if ( Decision != XSSH_SESSION_STREAM_ACCEPT ) {
		(void)xrtSshSessionReaderAbort(&pSession->Reader);
		pSession->State = XSSH_SESSION_STREAM_CLOSING;
		(void)xrtNetStreamAbort(pSession->Stream);
		return Decision == XSSH_SESSION_STREAM_ABORT ?
			XSSH_OK : XSSH_ERROR_CALLBACK;
	}
	Code = xrtSshSessionReaderCommit(
		&pSession->Reader,
		xsshSessionStreamNow(),
		&Rekey
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	memset(&pSession->Packet, 0, sizeof(pSession->Packet));
	pSession->NotifiedAction = XSSH_SESSION_ACTION_NONE;
	*pProgress = true;
	xsshSessionStreamRekey(pSession, Rekey);
	return XSSH_OK;
}



/* 推进一步，返回是否发生了可继续循环的状态推进。 */
static xsshcode xsshSessionStreamStep(
	xsshsessionstream* pSession,
	bool* pProgress
)
{
	xsshsessionaction Action;
	xsshcode Code;

	*pProgress = false;
	Action = xrtSshSessionTcpAction(&pSession->Session);
	if ( Action == XSSH_SESSION_ACTION_FAILED ) {
		return XSSH_ERROR_STATE;
	}
	if ( Action == XSSH_SESSION_ACTION_CLOSING ) {
		pSession->State = XSSH_SESSION_STREAM_CLOSING;
		(void)xrtNetStreamClose(pSession->Stream);
		return XSSH_OK;
	}
	if ( Action == XSSH_SESSION_ACTION_READ_PENDING ) {
		return XSSH_ERROR_STATE;
	}
	if ( xsshSessionStreamActionNotify(pSession, Action) ) {
		*pProgress = pSession->State == XSSH_SESSION_STREAM_OPEN;
		return XSSH_OK;
	}
	if ( Action == XSSH_SESSION_ACTION_WRITE_PENDING ) {
		return xsshSessionStreamWrite(pSession, pProgress);
	}
	if ( Action == XSSH_SESSION_ACTION_READ_IDENTIFICATION ) {
		Code = xsshSessionStreamIdentification(pSession, pProgress);
	} else if ( xsshSessionStreamPacketAction(Action) ) {
		Code = xsshSessionStreamPacketRead(pSession, pProgress);
	} else {
		return XSSH_OK;
	}
	if ( (Code != XSSH_OK) && (Code != XSSH_NEED_MORE) ) {
		xsshSessionStreamErrorNotify(
			pSession,
			Code,
			Code == XSSH_ERROR_SPACE ? XERR_MEMORY : XERR_PROTOCOL,
			Code == XSSH_ERROR_SPACE ?
				"SSH stream needs memory before retrying the same input" :
				"SSH stream rejected peer input"
		);
		if ( Code != XSSH_ERROR_SPACE ) {
			xsshSessionStreamStop(pSession);
		}
	}
	return Code;
}



/* 串行推进并合并回调中的递归 Drive 请求。 */
static xsshcode xsshSessionStreamRun(xsshsessionstream* pSession)
{
	xsshcode Result = XSSH_OK;

	if ( pSession->Driving ) {
		pSession->DriveAgain = true;
		return XSSH_OK;
	}
	pSession->Driving = true;
	do {
		bool bProgress;
		xsshcode Code;

		pSession->DriveAgain = false;
		for ( ;; ) {
			if ( pSession->State != XSSH_SESSION_STREAM_OPEN ) {
				break;
			}
			Code = xsshSessionStreamStep(pSession, &bProgress);
			if ( (Code != XSSH_OK) || !bProgress ) {
				if ( Code != XSSH_OK ) {
					Result = Code;
				}
				break;
			}
		}
	} while ( pSession->DriveAgain &&
		(pSession->State == XSSH_SESSION_STREAM_OPEN) );
	pSession->Driving = false;
	return Result;
}



/* EOF 后等待未入队输出完成；空输入正常关闭，残留输入按截断消息拒绝。 */
static xsshcode xsshSessionStreamFinish(
	xsshsessionstream* pSession,
	xsshcode Code
)
{
	xsshsessionaction Action;

	if ( !pSession->ReadEnded ||
		(pSession->State != XSSH_SESSION_STREAM_OPEN) ) {
		return Code;
	}
	if ( (pSession->Input != NULL) &&
		!xrtNetBufEmpty(pSession->Input) ) {
		xsshSessionStreamErrorNotify(
			pSession,
			XSSH_ERROR_PROTOCOL,
			XERR_PROTOCOL,
			"SSH stream ended with a truncated message"
		);
		xsshSessionStreamStop(pSession);
		return XSSH_ERROR_PROTOCOL;
	}
	Action = xrtSshSessionTcpAction(&pSession->Session);
	if ( pSession->WritePaused ||
		(Action == XSSH_SESSION_ACTION_WRITE_PENDING) ) {
		return Code;
	}
	pSession->State = XSSH_SESSION_STREAM_CLOSING;
	(void)xrtNetStreamClose(pSession->Stream);
	return Code;
}



/* 在 Open 或已打开 Attach 路径上绑定 Worker 池并创建唯一会话。 */
static bool xsshSessionStreamStart(
	xsshsessionstream* pSession,
	xnetstream* pStream
)
{
	xnetworker* pWorker;
	xnetbufpool* pPool;

	if ( !xsshSessionStreamValid(pSession) ||
		(pSession->State != XSSH_SESSION_STREAM_CREATED) ||
		((pSession->Stream != NULL) && (pSession->Stream != pStream)) ) {
		return false;
	}
	pWorker = xrtNetStreamWorker(pStream);
	if ( (pWorker == NULL) || !xrtNetWorkerIsCurrent(pWorker) ) {
		return false;
	}
	pPool = xrtNetWorkerBufPool(pWorker);
	if ( (pPool == NULL) || !xrtSshSessionTcpInit(
		&pSession->Session,
		pPool,
		&pSession->Config,
		xsshSessionStreamNow()
	) ) {
		return false;
	}
	if ( !xrtSshSessionReaderInit(
		&pSession->Reader,
		pPool,
		&pSession->Session
	) ) {
		xrtSshSessionTcpClear(&pSession->Session);
		return false;
	}
	pSession->Stream = pStream;
	pSession->SessionReady = true;
	pSession->State = XSSH_SESSION_STREAM_OPEN;
	if ( pSession->Events.Open != NULL ) {
		pSession->Events.Open(pSession, pSession->UserData);
	}
	if ( pSession->State == XSSH_SESSION_STREAM_OPEN ) {
		(void)xsshSessionStreamRun(pSession);
	}
	return true;
}



/* 复制配置和用户事件，不提前占用 Worker 或网络资源。 */
bool xrtSshSessionStreamInit(
	xsshsessionstream* pSession,
	const xsshsessiontcpconfig* pConfig,
	const xsshsessionstreamevents* pEvents,
	ptr pData
)
{
	xsshsessionstream Session;

	if ( !xrtMemRangeValid(pSession, sizeof(*pSession)) ||
		!xrtMemRangeValid(pConfig, sizeof(*pConfig)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pConfig,
			sizeof(*pConfig)
		) || ((pEvents != NULL) && (
			!xrtMemRangeValid(pEvents, sizeof(*pEvents)) ||
			xrtMemRangesOverlap(
				pSession,
				sizeof(*pSession),
				pEvents,
				sizeof(*pEvents)
		))) ) {
		return false;
	}
	memset(&Session, 0, sizeof(Session));
	Session.Config = *pConfig;
	if ( pEvents != NULL ) {
		Session.Events = *pEvents;
	}
	Session.UserData = pData;
	Session.State = XSSH_SESSION_STREAM_CREATED;
	Session.Guard = XSSH_SESSION_STREAM_GUARD;
	*pSession = Session;
	return true;
}



/* 清理未附着或已经完成 Close 清理的驱动。 */
bool xrtSshSessionStreamClear(xsshsessionstream* pSession)
{
	if ( !xsshSessionStreamValid(pSession) ||
		((pSession->State != XSSH_SESSION_STREAM_CREATED) &&
		 (pSession->State != XSSH_SESSION_STREAM_CLOSED)) ) {
		return false;
	}
	memset(pSession, 0, sizeof(*pSession));
	return true;
}



/* 返回可由 TCP client 和 server 共同使用的内部事件表。 */
const xnetstreamevents* xrtSshSessionStreamNetEvents(void)
{
	return &xsshSessionStreamEvents;
}



/* 在 Worker 上替换事件；Open 尚未发布时由网络层稍后完成启动。 */
bool xrtSshSessionStreamAttach(
	xsshsessionstream* pSession,
	xnetstream* pStream
)
{
	xnetstreamstate State;
	xnetworker* pWorker;

	if ( !xsshSessionStreamValid(pSession) ||
		(pSession->State != XSSH_SESSION_STREAM_CREATED) ||
		(pSession->Stream != NULL) || (pStream == NULL) ) {
		return false;
	}
	pWorker = xrtNetStreamWorker(pStream);
	State = xrtNetStreamState(pStream);
	if ( (pWorker == NULL) || !xrtNetWorkerIsCurrent(pWorker) ||
		(State >= XNET_STREAM_CLOSING) ||
		((State == XNET_STREAM_OPEN) &&
		 (xrtNetStreamAvailable(pStream) != 0u)) ||
		!xrtNetStreamSetEvents(
			pStream,
			&xsshSessionStreamEvents,
			pSession
		) ) {
		return false;
	}
	pSession->Stream = pStream;
	if ( State != XNET_STREAM_OPEN ) {
		return true;
	}
	if ( xsshSessionStreamStart(pSession, pStream) ) {
		return true;
	}
	xsshSessionStreamErrorNotify(
		pSession,
		XSSH_ERROR_STATE,
		XERR_STATE,
		"SSH stream could not bind its Worker buffer pool"
	);
	pSession->State = XSSH_SESSION_STREAM_CLOSING;
	(void)xrtNetStreamAbort(pStream);
	return false;
}



/* 无效对象返回不会与活动状态混淆的独立值。 */
xsshsessionstreamstate xrtSshSessionStreamState(
	const xsshsessionstream* pSession
)
{
	return xsshSessionStreamValid(pSession) ?
		pSession->State : XSSH_SESSION_STREAM_INVALID;
}



/* 借出仍处于连接生命周期中的 TCP Stream。 */
xnetstream* xrtSshSessionStreamTcp(xsshsessionstream* pSession)
{
	return xsshSessionStreamValid(pSession) &&
		(pSession->State != XSSH_SESSION_STREAM_CLOSED) ?
		pSession->Stream : NULL;
}



/* 借出已经绑定 Worker 池的 SSH TCP 会话。 */
xsshsessiontcp* xrtSshSessionStreamSession(
	xsshsessionstream* pSession
)
{
	return xsshSessionStreamValid(pSession) && pSession->SessionReady ?
		&pSession->Session : NULL;
}



/* 借出已经绑定同一 Worker 池的动态 Reader。 */
xsshsessionreader* xrtSshSessionStreamReader(
	xsshsessionstream* pSession
)
{
	return xsshSessionStreamValid(pSession) && pSession->SessionReady ?
		&pSession->Reader : NULL;
}



/* HOLD identification 以外的状态不暴露陈旧视图。 */
xstrview xrtSshSessionStreamVersion(
	const xsshsessionstream* pSession
)
{
	return xsshSessionStreamValid(pSession) &&
		(pSession->State == XSSH_SESSION_STREAM_HOLD_IDENTIFICATION) ?
		pSession->Version : (xstrview){ NULL, 0u };
}



/* HOLD packet 以外的状态不暴露陈旧解析结果。 */
const xsshsessiontcppacket* xrtSshSessionStreamPacket(
	const xsshsessionstream* pSession
)
{
	return xsshSessionStreamValid(pSession) &&
		(pSession->State == XSSH_SESSION_STREAM_HOLD_PACKET) ?
		&pSession->Packet : NULL;
}



/* 显式推进会重新通知当前动作，并允许 RETRY 再次申请内存。 */
xsshcode xrtSshSessionStreamDrive(xsshsessionstream* pSession)
{
	xsshcode Code;

	if ( !xsshSessionStreamCurrent(pSession) ||
		!pSession->SessionReady ||
		((pSession->State != XSSH_SESSION_STREAM_OPEN) &&
		 (pSession->State != XSSH_SESSION_STREAM_RETRY)) ) {
		return XSSH_ERROR_STATE;
	}
	if ( pSession->State == XSSH_SESSION_STREAM_RETRY ) {
		pSession->State = XSSH_SESSION_STREAM_OPEN;
	}
	pSession->NotifiedAction = XSSH_SESSION_ACTION_NONE;
	Code = xsshSessionStreamRun(pSession);
	return xsshSessionStreamFinish(pSession, Code);
}



/* 提交 HOLD 事务，再恢复由驱动暂停的读取。 */
xsshcode xrtSshSessionStreamAccept(xsshsessionstream* pSession)
{
	xsshrekeydecision Decision = XSSH_REKEY_NONE;
	xsshcode Code;

	if ( !xsshSessionStreamCurrent(pSession) ||
		!pSession->SessionReady ) {
		return XSSH_ERROR_STATE;
	}
	if ( pSession->State == XSSH_SESSION_STREAM_HOLD_IDENTIFICATION ) {
		Code = xrtSshSessionTcpReadCommit(
			&pSession->Session,
			xsshSessionStreamNow(),
			&Decision
		);
	} else if ( pSession->State == XSSH_SESSION_STREAM_HOLD_PACKET ) {
		Code = xrtSshSessionReaderCommit(
			&pSession->Reader,
			xsshSessionStreamNow(),
			&Decision
		);
	} else {
		return XSSH_ERROR_STATE;
	}
	if ( Code != XSSH_OK ) {
		xsshSessionStreamErrorNotify(
			pSession,
			Code,
			XERR_PROTOCOL,
			"SSH held input could not be committed"
		);
		xsshSessionStreamStop(pSession);
		return Code;
	}
	pSession->Version = (xstrview){ NULL, 0u };
	memset(&pSession->Packet, 0, sizeof(pSession->Packet));
	pSession->State = XSSH_SESSION_STREAM_OPEN;
	pSession->NotifiedAction = XSSH_SESSION_ACTION_NONE;
	xsshSessionStreamRekey(pSession, Decision);
	if ( !xsshSessionStreamResume(pSession) ) {
		xsshSessionStreamStop(pSession);
		return XSSH_ERROR_STATE;
	}
	Code = xsshSessionStreamRun(pSession);
	return xsshSessionStreamFinish(pSession, Code);
}



/* 拒绝当前 HOLD，并把底层认证后输入连同连接一起终止。 */
xsshcode xrtSshSessionStreamReject(xsshsessionstream* pSession)
{
	xsshcode Code;

	if ( !xsshSessionStreamCurrent(pSession) ||
		!pSession->SessionReady ) {
		return XSSH_ERROR_STATE;
	}
	if ( pSession->State == XSSH_SESSION_STREAM_HOLD_IDENTIFICATION ) {
		Code = xrtSshSessionTcpReadAbort(&pSession->Session);
	} else if ( pSession->State == XSSH_SESSION_STREAM_HOLD_PACKET ) {
		Code = xrtSshSessionReaderAbort(&pSession->Reader);
	} else {
		return XSSH_ERROR_STATE;
	}
	pSession->State = XSSH_SESSION_STREAM_CLOSING;
	(void)xrtNetStreamAbort(pSession->Stream);
	return Code;
}



/* 在所属 Worker 上从任意活动状态请求唯一的异常关闭。 */
bool xrtSshSessionStreamAbort(xsshsessionstream* pSession)
{
	if ( !xsshSessionStreamCurrent(pSession) ||
		(pSession->State >= XSSH_SESSION_STREAM_CLOSING) ) {
		return false;
	}
	xsshSessionStreamStop(pSession);
	return true;
}



/* 客户端直连与服务端 Attach 最终都从 Open 绑定 Worker 缓冲池。 */
static void xsshSessionStreamOpen(xnetstream* pStream, ptr pData)
{
	xsshsessionstream* pSession = (xsshsessionstream*)pData;

	/* Accept 内 Attach 已经完成启动时，忽略网络层随后发布的正式 Open。 */
	if ( xsshSessionStreamValid(pSession) &&
		pSession->SessionReady &&
		(pSession->Stream == pStream) &&
		(pSession->State == XSSH_SESSION_STREAM_OPEN) ) {
		return;
	}
	if ( !xsshSessionStreamStart(pSession, pStream) ) {
		if ( xsshSessionStreamValid(pSession) ) {
			xsshSessionStreamErrorNotify(
				pSession,
				XSSH_ERROR_STATE,
				XERR_STATE,
				"SSH stream could not bind its Worker buffer pool"
			);
			pSession->Stream = pStream;
			pSession->State = XSSH_SESSION_STREAM_CLOSING;
		}
		(void)xrtNetStreamAbort(pStream);
	}
}



/* 借用 Stream 的可变接收链并增量处理所有当前可推进输入。 */
static void xsshSessionStreamRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	xsshsessionstream* pSession = (xsshsessionstream*)pData;

	if ( !xsshSessionStreamValid(pSession) ||
		(pSession->Stream != pStream) || !pSession->SessionReady ) {
		(void)xrtNetStreamAbort(pStream);
		return;
	}
	pSession->Input = pBuffer;
	if ( pSession->State == XSSH_SESSION_STREAM_OPEN ) {
		(void)xsshSessionStreamRun(pSession);
	}
}



/* EOF 后先发布事件，再处理已缓存尾部并关闭或拒绝截断报文。 */
static void xsshSessionStreamEnd(xnetstream* pStream, ptr pData)
{
	xsshsessionstream* pSession = (xsshsessionstream*)pData;

	if ( !xsshSessionStreamValid(pSession) ||
		(pSession->Stream != pStream) ) {
		(void)xrtNetStreamAbort(pStream);
		return;
	}
	pSession->ReadEnded = true;
	if ( pSession->Events.End != NULL ) {
		pSession->Events.End(pSession, pSession->UserData);
	}
	if ( pSession->State == XSSH_SESSION_STREAM_OPEN ) {
		xsshcode Code = xsshSessionStreamRun(pSession);

		(void)xsshSessionStreamFinish(pSession, Code);
	}
}



/* 转发首次高水位通知，不改变已被 TCP 接管的输出。 */
static void xsshSessionStreamHighWater(
	xnetstream* pStream,
	size_t iQueued,
	ptr pData
)
{
	xsshsessionstream* pSession = (xsshsessionstream*)pData;

	if ( xsshSessionStreamValid(pSession) &&
		(pSession->Stream == pStream) &&
		(pSession->Events.HighWater != NULL) ) {
		pSession->Events.HighWater(
			pSession,
			iQueued,
			pSession->UserData
		);
	}
}



/* 低水位允许重试仍由 SSH transport 持有的完整输出事务。 */
static void xsshSessionStreamLowWater(
	xnetstream* pStream,
	size_t iQueued,
	ptr pData
)
{
	xsshsessionstream* pSession = (xsshsessionstream*)pData;
	xsshcode Code = XSSH_OK;

	if ( !xsshSessionStreamValid(pSession) ||
		(pSession->Stream != pStream) ) {
		return;
	}
	if ( (pSession->State == XSSH_SESSION_STREAM_OPEN) &&
		pSession->WritePaused ) {
		Code = xsshSessionStreamRun(pSession);
		(void)xsshSessionStreamFinish(pSession, Code);
	}
	if ( (Code == XSSH_OK) && !pSession->WritePaused &&
		(pSession->State == XSSH_SESSION_STREAM_OPEN) &&
		(pSession->Events.LowWater != NULL) ) {
		pSession->Events.LowWater(
			pSession,
			xrtNetStreamPending(pStream),
			pSession->UserData
		);
	}
	(void)iQueued;
}



/* 排空通知与低水位使用相同写重试契约。 */
static void xsshSessionStreamDrain(xnetstream* pStream, ptr pData)
{
	xsshsessionstream* pSession = (xsshsessionstream*)pData;
	xsshcode Code = XSSH_OK;

	if ( !xsshSessionStreamValid(pSession) ||
		(pSession->Stream != pStream) ) {
		return;
	}
	if ( (pSession->State == XSSH_SESSION_STREAM_OPEN) &&
		pSession->WritePaused ) {
		Code = xsshSessionStreamRun(pSession);
		(void)xsshSessionStreamFinish(pSession, Code);
	}
	if ( (Code == XSSH_OK) && !pSession->WritePaused &&
		(pSession->State == XSSH_SESSION_STREAM_OPEN) &&
		(xrtNetStreamPending(pStream) == 0u) &&
		(pSession->Events.Drain != NULL) ) {
		pSession->Events.Drain(pSession, pSession->UserData);
	}
}



/* Close 回调期间底层对象仍可检查，回调返回后统一释放 Worker 池动态块。 */
static void xsshSessionStreamClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	xsshsessionstream* pSession = (xsshsessionstream*)pData;

	if ( !xsshSessionStreamValid(pSession) ||
		((pSession->Stream != NULL) &&
		 (pSession->Stream != pStream)) ) {
		return;
	}
	pSession->Stream = pStream;
	pSession->State = XSSH_SESSION_STREAM_CLOSING;
	if ( pSession->Events.Close != NULL ) {
		pSession->Events.Close(
			pSession,
			Result,
			pError,
			pSession->UserData
		);
	}
	if ( pSession->SessionReady ) {
		xrtSshSessionReaderClear(&pSession->Reader);
		xrtSshSessionTcpClear(&pSession->Session);
	}
	pSession->SessionReady = false;
	pSession->Driving = false;
	pSession->DriveAgain = false;
	pSession->Paused = false;
	pSession->WritePaused = false;
	pSession->Input = NULL;
	pSession->Stream = NULL;
	pSession->Version = (xstrview){ NULL, 0u };
	memset(&pSession->Packet, 0, sizeof(pSession->Packet));
	pSession->State = XSSH_SESSION_STREAM_CLOSED;
}

#endif
