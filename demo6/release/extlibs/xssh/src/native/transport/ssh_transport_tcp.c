#include <string.h>

#include <xrt/ssh_transport_tcp.h>



#if defined(XSSH_FEATURE_TRANSPORT_TCP)

#define XSSH_TRANSPORT_TCP_GUARD UINT32_C(0x53545450)



/* 校验对象以及读写事务与缓冲借用的一致性。 */
static bool xsshTransportTcpValid(const xsshtransporttcp* pTransport)
{
	if ( !xrtMemRangeValid(pTransport, sizeof(*pTransport)) ||
		(pTransport->Guard != XSSH_TRANSPORT_TCP_GUARD) ||
		(pTransport->MaxBannerBytes < XSSH_IDENTIFICATION_MAX) ||
		(pTransport->WritePending > XSSH_TRANSPORT_TCP_PENDING_PACKET) ||
		(pTransport->ReadPending > XSSH_TRANSPORT_TCP_PENDING_PACKET) ) {
		return false;
	}
	if ( ((pTransport->ReadPending == XSSH_TRANSPORT_TCP_PENDING_NONE) !=
		 (pTransport->Input == NULL)) ||
		((pTransport->ReadPending == XSSH_TRANSPORT_TCP_PENDING_NONE) !=
		 (pTransport->ReadSize == 0u)) ||
		((pTransport->WritePending == XSSH_TRANSPORT_TCP_PENDING_NONE) !=
		 xrtNetBufEmpty(&pTransport->Output)) ||
		(pTransport->Output.Reserved != NULL) ) {
		return false;
	}
	return true;
}



/* 把适配层内部不变量错误映射到 XRT 结构化错误。 */
static void xsshTransportTcpError(xsshcode Code, cstr sMessage)
{
	xrtSetErrorInfo(
		Code == XSSH_ERROR_PROTOCOL ? XERR_PROTOCOL : XERR_INTERNAL,
		"xrt.ssh",
		(int32)Code,
		sMessage
	);
}



/* 清空读借用描述；底层网络缓冲的所有权始终属于 Stream。 */
static void xsshTransportTcpReadClear(xsshtransporttcp* pTransport)
{
	pTransport->Input = NULL;
	pTransport->ReadSize = 0u;
	pTransport->ReadPending = XSSH_TRANSPORT_TCP_PENDING_NONE;
}



/* 提交已知长度的输出预留，失败时同时回滚 packet 事务。 */
static xsshcode xsshTransportTcpOutputCommit(
	xsshtransporttcp* pTransport,
	size_t iSize,
	xsshtransporttcppending Pending
)
{
	if ( !xrtNetBufCommit(&pTransport->Output, iSize) ) {
		if ( Pending == XSSH_TRANSPORT_TCP_PENDING_PACKET ) {
			(void)xrtSshTransportCoreWriteAbort(&pTransport->Core);
		}
		(void)xrtNetBufCancel(&pTransport->Output);
		return XSSH_ERROR_STATE;
	}
	pTransport->WritePending = Pending;
	return XSSH_OK;
}



/* 查找完整 identification 行，同时限制前置 banner 总量和单行长度。 */
static xsshcode xsshTransportTcpIdentificationEnd(
	const xsshtransporttcp* pTransport,
	const xnetbuf* pInput,
	size_t* pEnd
)
{
	size_t iAvailable = xrtNetBufSize(pInput);
	size_t iLineStart = 0u;

	while ( iLineStart < iAvailable ) {
		size_t iLine = xrtNetBufFind(
			pInput,
			(uint8)'\n',
			iLineStart
		);
		unsigned char arrPrefix[4];

		if ( iLine == XRT_NPOS ) {
			break;
		}
		if ( (iLine + 1u - iLineStart) > XSSH_IDENTIFICATION_MAX ) {
			return XSSH_ERROR_OVERFLOW;
		}
		if ( (iLine + 1u) > pTransport->MaxBannerBytes ) {
			return XSSH_ERROR_OVERFLOW;
		}
		if ( ((iLine - iLineStart) >= sizeof(arrPrefix)) &&
			(xrtNetBufPeek(
				pInput,
				iLineStart,
				arrPrefix,
				sizeof(arrPrefix)
			) == sizeof(arrPrefix)) &&
			(memcmp(arrPrefix, "SSH-", sizeof(arrPrefix)) == 0) ) {
			*pEnd = iLine + 1u;
			return XSSH_OK;
		}
		iLineStart = iLine + 1u;
		if ( iLineStart > pTransport->MaxBannerBytes ) {
			return XSSH_ERROR_OVERFLOW;
		}
	}
	if ( (iAvailable - iLineStart) >= XSSH_IDENTIFICATION_MAX ) {
		return XSSH_ERROR_OVERFLOW;
	}
	if ( iAvailable >= pTransport->MaxBannerBytes ) {
		return XSSH_ERROR_OVERFLOW;
	}
	return XSSH_NEED_MORE;
}



/* 从缓冲链复制公开的四字节长度头，不连续化后续 packet。 */
static xsshcode xsshTransportTcpInspect(
	const xsshtransporttcp* pTransport,
	const xnetbuf* pInput,
	xsshpacketneed* pNeed
)
{
	unsigned char arrHead[4];
	xsshreader Reader;

	if ( xrtNetBufSize(pInput) < sizeof(arrHead) ) {
		return XSSH_NEED_MORE;
	}
	if ( xrtNetBufPeek(
		pInput,
		0u,
		arrHead,
		sizeof(arrHead)
	) != sizeof(arrHead) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrHead, sizeof(arrHead) }
	) ) {
		return XSSH_ERROR_STATE;
	}
	return xrtSshTransportCoreInspect(
		&pTransport->Core,
		&Reader,
		pNeed
	);
}



/* 默认配置保留协议要求的 packet 能力并给前置 banner 明确硬边界。 */
bool xrtSshTransportTcpConfigInit(
	xsshtransporttcpconfig* pConfig,
	xsshrole Role
)
{
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ||
		((Role != XSSH_ROLE_CLIENT) && (Role != XSSH_ROLE_SERVER)) ) {
		return false;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	xrtSshRekeyPolicyInit(&pConfig->Rekey);
	pConfig->MaxBannerBytes = XSSH_TRANSPORT_TCP_BANNER_LIMIT_DEFAULT;
	pConfig->MaxPacketSize = XSSH_PACKET_MAX_DEFAULT;
	pConfig->Role = Role;
	return true;
}



/* 初始化结果先在局部对象闭合，失败不会发布半初始化资源。 */
bool xrtSshTransportTcpInit(
	xsshtransporttcp* pTransport,
	xnetbufpool* pPool,
	const xsshtransporttcpconfig* pConfig,
	uint64 iNowMs
)
{
	xsshtransporttcp Transport;
	xsshtransporttcpconfig Config;

	if ( !xrtMemRangeValid(pTransport, sizeof(*pTransport)) ||
		!xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		return false;
	}
	Config = *pConfig;
	if ( Config.MaxBannerBytes < XSSH_IDENTIFICATION_MAX ) {
		return false;
	}
	memset(&Transport, 0, sizeof(Transport));
	if ( !xrtNetBufInit(&Transport.Output, pPool) ||
		!xrtSshTransportCoreInit(
			&Transport.Core,
			Config.Role,
			Config.MaxPacketSize,
			&Config.Rekey,
			iNowMs
		) ) {
		xrtNetBufClear(&Transport.Output);
		xrtSshTransportCoreClear(&Transport.Core);
		return false;
	}
	Transport.MaxBannerBytes = Config.MaxBannerBytes;
	Transport.Guard = XSSH_TRANSPORT_TCP_GUARD;
	xrtSecureZero(pTransport, sizeof(*pTransport));
	*pTransport = Transport;
	xrtSecureZero(&Transport, sizeof(Transport));
	return true;
}



/* 未发送 packet 先回滚 core，再释放输出与 cipher。 */
void xrtSshTransportTcpClear(xsshtransporttcp* pTransport)
{
	if ( pTransport == NULL ) {
		return;
	}
	if ( xsshTransportTcpValid(pTransport) ) {
		if ( pTransport->WritePending ==
			XSSH_TRANSPORT_TCP_PENDING_PACKET ) {
			(void)xrtSshTransportCoreWriteAbort(&pTransport->Core);
		}
		if ( pTransport->ReadPending ==
			XSSH_TRANSPORT_TCP_PENDING_PACKET ) {
			(void)xrtSshTransportCoreReadAbort(&pTransport->Core);
		}
		xrtNetBufClear(&pTransport->Output);
		xrtSshTransportCoreClear(&pTransport->Core);
	}
	xrtSecureZero(pTransport, sizeof(*pTransport));
}



/* Core 借用不会改变 transport 生命周期。 */
xsshtransportcore* xrtSshTransportTcpCore(xsshtransporttcp* pTransport)
{
	return xsshTransportTcpValid(pTransport) ? &pTransport->Core : NULL;
}



/* 只读 Core 借用不会改变 transport 生命周期。 */
const xsshtransportcore* xrtSshTransportTcpCoreConst(
	const xsshtransporttcp* pTransport
)
{
	return xsshTransportTcpValid(pTransport) ? &pTransport->Core : NULL;
}



/* identification 最大只有 255 字节，仍复用池化动态块而不内嵌数组。 */
xsshcode xrtSshTransportTcpIdentificationPrepare(
	xsshtransporttcp* pTransport,
	xstrview Banner
)
{
	xnetwspan Span;
	xsshwriter Writer;
	xsshcode Code;

	if ( !xsshTransportTcpValid(pTransport) ||
		(pTransport->WritePending != XSSH_TRANSPORT_TCP_PENDING_NONE) ||
		!xrtNetBufEmpty(&pTransport->Output) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(Banner.Data, Banner.Size) ||
		(Banner.Size > (XSSH_IDENTIFICATION_MAX - 2u)) ||
		xrtMemRangesOverlap(
			pTransport,
			sizeof(*pTransport),
			Banner.Data,
			Banner.Size
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !xrtNetBufReserve(
		&pTransport->Output,
		Banner.Size + 2u,
		&Span
	) ) {
		return XSSH_ERROR_SPACE;
	}
	if ( !xrtSshWriterInit(
		&Writer,
		Span.Data,
		Banner.Size + 2u
	) ) {
		(void)xrtNetBufCancel(&pTransport->Output);
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshBannerWrite(&Writer, Banner);
	if ( Code != XSSH_OK ) {
		(void)xrtNetBufCancel(&pTransport->Output);
		return Code;
	}
	return xsshTransportTcpOutputCommit(
		pTransport,
		Writer.Size,
		XSSH_TRANSPORT_TCP_PENDING_IDENTIFICATION
	);
}



/* 精确预留一包最终线长，并让 core 保持网络受理前事务。 */
xsshcode xrtSshTransportTcpWritePrepareWithPadding(
	xsshtransporttcp* pTransport,
	xbytesview Payload,
	xsshpaddingproc pPadding,
	ptr pUserData,
	uint64 iNowMs
)
{
	xsshpacketneed Need;
	xnetwspan Span;
	xsshwriter Writer;
	xsshcode Code;

	if ( !xsshTransportTcpValid(pTransport) ||
		(pTransport->WritePending != XSSH_TRANSPORT_TCP_PENDING_NONE) ||
		!xrtNetBufEmpty(&pTransport->Output) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(Payload.Data, Payload.Size) ||
		xrtMemRangesOverlap(
			pTransport,
			sizeof(*pTransport),
			Payload.Data,
			Payload.Size
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshPacketCodecWriteMeasure(
		&pTransport->Core.Codec,
		Payload.Size,
		&Need
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtNetBufReserve(
		&pTransport->Output,
		Need.WireSize,
		&Span
	) ) {
		return XSSH_ERROR_SPACE;
	}
	if ( !xrtSshWriterInit(&Writer, Span.Data, Need.WireSize) ) {
		(void)xrtNetBufCancel(&pTransport->Output);
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshTransportCoreWritePrepareWithPadding(
		&pTransport->Core,
		&Writer,
		Payload,
		pPadding,
		pUserData,
		iNowMs
	);
	if ( Code != XSSH_OK ) {
		(void)xrtNetBufCancel(&pTransport->Output);
		return Code;
	}
	if ( Writer.Size != Need.WireSize ) {
		(void)xrtSshTransportCoreWriteAbort(&pTransport->Core);
		(void)xrtNetBufCancel(&pTransport->Output);
		return XSSH_ERROR_STATE;
	}
	return xsshTransportTcpOutputCommit(
		pTransport,
		Writer.Size,
		XSSH_TRANSPORT_TCP_PENDING_PACKET
	);
}



/* TCP 成功接管动态链就是唯一可靠写提交边界。 */
xnetresult xrtSshTransportTcpWriteSubmit(
	xsshtransporttcp* pTransport,
	xnetstream* pStream,
	uint64 iNowMs,
	xsshrekeydecision* pDecision
)
{
	xsshtransporttcppending Pending;
	xsshrekeydecision Decision = XSSH_REKEY_NONE;
	xnetresult Result;
	xsshcode Code;

	if ( !xsshTransportTcpValid(pTransport) ||
		(pTransport->WritePending == XSSH_TRANSPORT_TCP_PENDING_NONE) ||
		!xrtMemRangeValid(pDecision, sizeof(*pDecision)) ||
		xrtMemRangesOverlap(
			pTransport,
			sizeof(*pTransport),
			pDecision,
			sizeof(*pDecision)
		) ) {
		xrtSetErrorInfo(
			XERR_ARGUMENT,
			"xrt.ssh",
			(int32)XSSH_ERROR_ARGUMENT,
			"invalid SSH TCP write submission"
		);
		return XNET_RESULT_ERROR;
	}
	Pending = pTransport->WritePending;
	Result = xrtNetStreamSendBuffer(pStream, &pTransport->Output);
	if ( Result != XNET_RESULT_OK ) {
		return Result;
	}
	if ( Pending == XSSH_TRANSPORT_TCP_PENDING_IDENTIFICATION ) {
		Code = xrtSshTransportCoreIdentificationCommit(
			&pTransport->Core,
			XSSH_TRANSPORT_LOCAL
		);
	} else {
		Code = xrtSshTransportCoreWriteCommit(
			&pTransport->Core,
			iNowMs,
			&Decision
		);
	}
	pTransport->WritePending = XSSH_TRANSPORT_TCP_PENDING_NONE;
	if ( Code != XSSH_OK ) {
		xsshTransportTcpError(
			Code,
			"SSH state rejected TCP-accepted output"
		);
		xrtSshTransportCoreClose(&pTransport->Core);
		(void)xrtNetStreamAbort(pStream);
		return XNET_RESULT_ERROR;
	}
	*pDecision = Decision;
	return XNET_RESULT_OK;
}



/* 未进入 TCP 队列的动态链与 packet 事务同时回滚。 */
xsshcode xrtSshTransportTcpWriteAbort(xsshtransporttcp* pTransport)
{
	xsshcode Code = XSSH_OK;

	if ( !xsshTransportTcpValid(pTransport) ||
		(pTransport->WritePending == XSSH_TRANSPORT_TCP_PENDING_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	if ( pTransport->WritePending ==
		XSSH_TRANSPORT_TCP_PENDING_PACKET ) {
		Code = xrtSshTransportCoreWriteAbort(&pTransport->Core);
	}
	xrtNetBufClear(&pTransport->Output);
	pTransport->WritePending = XSSH_TRANSPORT_TCP_PENDING_NONE;
	return Code;
}



/* 输出链是唯一待重试 packet 存储。 */
size_t xrtSshTransportTcpWriteSize(
	const xsshtransporttcp* pTransport
)
{
	return xsshTransportTcpValid(pTransport) ?
		xrtNetBufSize(&pTransport->Output) : 0u;
}



/* identification 只连续化最终需要借出的前缀。 */
xsshcode xrtSshTransportTcpIdentificationReadPrepare(
	xsshtransporttcp* pTransport,
	xnetbuf* pInput,
	xstrview* pBanner
)
{
	xnetspan Span;
	xstrview Banner;
	size_t iConsumed;
	size_t iEnd;
	xsshcode Code;

	if ( !xsshTransportTcpValid(pTransport) ||
		(pTransport->ReadPending != XSSH_TRANSPORT_TCP_PENDING_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	if ( (pInput == NULL) ||
		!xrtMemRangeValid(pBanner, sizeof(*pBanner)) ||
		xrtMemRangesOverlap(
			pTransport,
			sizeof(*pTransport),
			pInput,
			sizeof(*pInput)
		) || xrtMemRangesOverlap(
			pInput,
			sizeof(*pInput),
			pBanner,
			sizeof(*pBanner)
		) ||
		xrtMemRangesOverlap(
			pTransport,
			sizeof(*pTransport),
			pBanner,
			sizeof(*pBanner)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshTransportTcpIdentificationEnd(
		pTransport,
		pInput,
		&iEnd
	);
	if ( Code != XSSH_OK ) {
		if ( (Code == XSSH_ERROR_PROTOCOL) ||
			(Code == XSSH_ERROR_OVERFLOW) ||
			(Code == XSSH_ERROR_UNSUPPORTED) ) {
			xrtSshTransportCoreClose(&pTransport->Core);
		}
		return Code;
	}
	if ( !xrtNetBufPullup(pInput, iEnd, &Span) ) {
		return XSSH_ERROR_SPACE;
	}
	Code = xrtSshBannerRead(
		(xstrview){ (const char*)Span.Data, iEnd },
		&Banner,
		&iConsumed
	);
	if ( (Code != XSSH_OK) || (iConsumed != iEnd) ) {
		if ( (Code == XSSH_ERROR_PROTOCOL) ||
			(Code == XSSH_ERROR_OVERFLOW) ||
			(Code == XSSH_ERROR_UNSUPPORTED) ||
			(Code == XSSH_OK) ) {
			xrtSshTransportCoreClose(&pTransport->Core);
		}
		return Code == XSSH_OK ? XSSH_ERROR_STATE : Code;
	}
	pTransport->Input = pInput;
	pTransport->ReadSize = iConsumed;
	pTransport->ReadPending =
		XSSH_TRANSPORT_TCP_PENDING_IDENTIFICATION;
	*pBanner = Banner;
	return XSSH_OK;
}



/* Packet 探测不改变输入链或 transport。 */
xsshcode xrtSshTransportTcpReadInspect(
	const xsshtransporttcp* pTransport,
	const xnetbuf* pInput,
	xsshpacketneed* pNeed
)
{
	if ( !xsshTransportTcpValid(pTransport) ||
		(pTransport->ReadPending != XSSH_TRANSPORT_TCP_PENDING_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	if ( (pInput == NULL) ||
		!xrtMemRangeValid(pNeed, sizeof(*pNeed)) ||
		xrtMemRangesOverlap(
			pTransport,
			sizeof(*pTransport),
			pInput,
			sizeof(*pInput)
		) || xrtMemRangesOverlap(
			pInput,
			sizeof(*pInput),
			pNeed,
			sizeof(*pNeed)
		) ||
		xrtMemRangesOverlap(
			pTransport,
			sizeof(*pTransport),
			pNeed,
			sizeof(*pNeed)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xsshTransportTcpInspect(pTransport, pInput, pNeed);
}



/* 完整 packet 只在本次借用期间连续，后续 packet 不参与复制。 */
xsshcode xrtSshTransportTcpReadPrepare(
	xsshtransporttcp* pTransport,
	xnetbuf* pInput,
	xsshpacketview* pPacket,
	void* pPlain,
	size_t iPlainCapacity,
	uint64 iNowMs
)
{
	xsshpacketneed Need;
	xnetspan Span;
	xsshreader Reader;
	xsshcode Code;

	if ( !xsshTransportTcpValid(pTransport) ||
		(pTransport->ReadPending != XSSH_TRANSPORT_TCP_PENDING_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	if ( (pInput == NULL) ||
		!xrtMemRangeValid(pPacket, sizeof(*pPacket)) ||
		xrtMemRangesOverlap(
			pTransport,
			sizeof(*pTransport),
			pInput,
			sizeof(*pInput)
		) || xrtMemRangesOverlap(
			pTransport,
			sizeof(*pTransport),
			pPacket,
			sizeof(*pPacket)
		) || xrtMemRangesOverlap(
			pTransport,
			sizeof(*pTransport),
			pPlain,
			iPlainCapacity
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshTransportTcpInspect(pTransport, pInput, &Need);
	if ( Code != XSSH_OK ) {
		if ( (Code == XSSH_ERROR_PROTOCOL) ||
			(Code == XSSH_ERROR_OVERFLOW) ||
			(Code == XSSH_ERROR_AUTHENTICATION) ||
			(Code == XSSH_ERROR_STATE) ) {
			xrtSshTransportCoreClose(&pTransport->Core);
		}
		return Code;
	}
	if ( xrtNetBufSize(pInput) < Need.WireSize ) {
		return XSSH_NEED_MORE;
	}
	if ( !xrtNetBufPullup(pInput, Need.WireSize, &Span) ) {
		return XSSH_ERROR_SPACE;
	}
	if ( !xrtSshReaderInit(
		&Reader,
		(xbytesview){ Span.Data, Need.WireSize }
	) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshTransportCoreReadPrepare(
		&pTransport->Core,
		&Reader,
		pPacket,
		pPlain,
		iPlainCapacity,
		iNowMs
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( Reader.Position != Need.WireSize ) {
		(void)xrtSshTransportCoreReadAbort(&pTransport->Core);
		return XSSH_ERROR_STATE;
	}
	pTransport->Input = pInput;
	pTransport->ReadSize = Need.WireSize;
	pTransport->ReadPending = XSSH_TRANSPORT_TCP_PENDING_PACKET;
	return XSSH_OK;
}



/* Core 先提交，随后底层链必须精确消费同一借用前缀。 */
xsshcode xrtSshTransportTcpReadCommit(
	xsshtransporttcp* pTransport,
	uint64 iNowMs,
	xsshrekeydecision* pDecision
)
{
	xsshrekeydecision Decision = XSSH_REKEY_NONE;
	xsshcode Code;
	size_t iConsumed;

	if ( !xsshTransportTcpValid(pTransport) ||
		(pTransport->ReadPending == XSSH_TRANSPORT_TCP_PENDING_NONE) ||
		!xrtMemRangeValid(pDecision, sizeof(*pDecision)) ||
		xrtMemRangesOverlap(
			pTransport,
			sizeof(*pTransport),
			pDecision,
			sizeof(*pDecision)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pTransport->ReadPending ==
		XSSH_TRANSPORT_TCP_PENDING_IDENTIFICATION ) {
		Code = xrtSshTransportCoreIdentificationCommit(
			&pTransport->Core,
			XSSH_TRANSPORT_PEER
		);
	} else {
		Code = xrtSshTransportCoreReadCommit(
			&pTransport->Core,
			iNowMs,
			&Decision
		);
	}
	if ( Code != XSSH_OK ) {
		xrtSshTransportCoreClose(&pTransport->Core);
		(void)xrtNetBufConsume(
			pTransport->Input,
			pTransport->ReadSize
		);
		xsshTransportTcpReadClear(pTransport);
		return Code;
	}
	iConsumed = xrtNetBufConsume(
		pTransport->Input,
		pTransport->ReadSize
	);
	if ( iConsumed != pTransport->ReadSize ) {
		xrtSshTransportCoreClose(&pTransport->Core);
		xsshTransportTcpReadClear(pTransport);
		return XSSH_ERROR_STATE;
	}
	xsshTransportTcpReadClear(pTransport);
	*pDecision = Decision;
	return XSSH_OK;
}



/* 上层拒绝借用数据后关闭协议状态，并释放对应网络前缀。 */
xsshcode xrtSshTransportTcpReadAbort(xsshtransporttcp* pTransport)
{
	xsshcode Code = XSSH_OK;
	size_t iConsumed;

	if ( !xsshTransportTcpValid(pTransport) ||
		(pTransport->ReadPending == XSSH_TRANSPORT_TCP_PENDING_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	if ( pTransport->ReadPending == XSSH_TRANSPORT_TCP_PENDING_PACKET ) {
		Code = xrtSshTransportCoreReadAbort(&pTransport->Core);
	} else {
		xrtSshTransportCoreClose(&pTransport->Core);
	}
	iConsumed = xrtNetBufConsume(
		pTransport->Input,
		pTransport->ReadSize
	);
	if ( iConsumed != pTransport->ReadSize ) {
		Code = XSSH_ERROR_STATE;
	}
	xsshTransportTcpReadClear(pTransport);
	return Code;
}

#endif
