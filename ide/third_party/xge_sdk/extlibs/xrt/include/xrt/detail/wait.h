#ifndef XRT_DETAIL_XRT_WAIT_H
#define XRT_DETAIL_XRT_WAIT_H
#include <xrt.h>




XRT_EXTERN_C_BEGIN
#if (defined(XRT_FEATURE_CHANNEL))
XRT_API xwaitresult __xrtChannelSendUntil(
	xchannel* pChannel,
	ptr pItem,
	double iDeadline
);
#endif
#if (defined(XRT_FEATURE_CHANNEL))
XRT_API xwaitresult __xrtChannelRecvUntil(
	xchannel* pChannel,
	ptr* pItem,
	double iDeadline
);
#endif
#if (defined(XRT_FEATURE_CHANNEL)) && (defined(XRT_FEATURE_CHANNEL_CANCEL))
XRT_API xwaitresult __xrtChannelSendUntilCancel(
	xchannel* pChannel,
	ptr pItem,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_CHANNEL)) && (defined(XRT_FEATURE_CHANNEL_CANCEL))
XRT_API xwaitresult __xrtChannelRecvUntilCancel(
	xchannel* pChannel,
	ptr* pItem,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_CHANNEL)) && (defined(XRT_FEATURE_CHANNEL_SELECT))
XRT_API xchannelselectresult __xrtChannelSelectUntil(
	const xchannelcase* pCases,
	size_t iCount,
	double iDeadline
);
#endif
#if (defined(XRT_FEATURE_CHANNEL)) && (defined(XRT_FEATURE_CHANNEL_SELECT_CANCEL))
XRT_API xchannelselectresult __xrtChannelSelectUntilCancel(
	const xchannelcase* pCases,
	size_t iCount,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_CHANNEL)) && (defined(XRT_FEATURE_CHANNEL_COROUTINE))
XRT_API xwaitresult __xrtChannelSendAwaitUntil(
	xchannel* pChannel,
	ptr pItem,
	double iDeadline
);
#endif
#if (defined(XRT_FEATURE_CHANNEL)) && (defined(XRT_FEATURE_CHANNEL_COROUTINE))
XRT_API xwaitresult __xrtChannelRecvAwaitUntil(
	xchannel* pChannel,
	ptr* pItem,
	double iDeadline
);
#endif
#if (defined(XRT_FEATURE_CHANNEL)) && (defined(XRT_FEATURE_CHANNEL_COROUTINE))
XRT_API xchannelselectresult __xrtChannelSelectAwaitUntil(
	const xchannelcase* pCases,
	size_t iCount,
	double iDeadline
);
#endif
#if (defined(XRT_FEATURE_COROUTINE_SCHEDULER))
XRT_API xwaitresult __xrtCoSchedPollUntil(xcosched* pSched, double iDeadline);
#endif
#if (defined(XRT_FEATURE_COROUTINE_SCHEDULER))
XRT_API xwaitresult __xrtCoParkUntil(double iDeadline);
#endif
#if (defined(XRT_FEATURE_COROUTINE_SCHEDULER))
XRT_API xwaitresult __xrtCoSleepUntil(double iDeadline);
#endif
#if (defined(XRT_FEATURE_COROUTINE_SCHEDULER))
XRT_API xwaitresult __xrtCoJoinUntil(xcoro* pCo, double iDeadline);
#endif
#if (defined(XRT_FEATURE_COROUTINE_EVENT))
XRT_API xwaitresult __xrtCoEventAwaitUntil(
	xcoevent* pEvent,
	double iDeadline
);
#endif
#if (defined(XRT_FEATURE_EXECUTOR))
XRT_API xwaitresult __xrtExecutorWaitUntil(
	xexecutor* pExecutor,
	double iDeadline
);
#endif


#if (defined(XRT_FEATURE_FUTURE_COROUTINE))
XRT_API xwaitresult __xrtFutureAwaitUntil(xfuture* pFuture, double iDeadline);
#endif
#if (defined(XRT_FEATURE_NET_PORT))
XRT_API xnetresult __xrtNetPortWait(xnetport* pPort,
	xnetportevent* pEvents, size_t iCapacity,
	double iDeadline, size_t* pCount);
#endif
#if (defined(XRT_FEATURE_NET_ENGINE))
XRT_API uint64 __xrtNetEngineScheduleOwnedV1(xnetengine* pEngine, uint64 iAffinity,
	double iDeadline, ptr pData, const xnettimerownershipv1* pPolicy);
#endif

#if (defined(XRT_FEATURE_PROCESS))
XRT_API xwaitresult __xrtProcessWaitUntil(
	xprocess* pProcess,
	double iDeadline
);
#endif
#if (defined(XRT_FEATURE_PROCESS_RUN))
XRT_API xwaitresult __xrtProcessWaitUntilCancel(
	xprocess* pProcess,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_COND))
XRT_API xwaitresult __xrtCondWaitUntil(
	xcond* pCond,
	xmutex* pMutex,
	double iDeadline
);
#endif
#if (defined(XRT_FEATURE_SEM))
XRT_API xwaitresult __xrtSemWaitUntil(xsem* pSem, double iDeadline);
#endif
#if (defined(XRT_FEATURE_EVENT))
XRT_API xwaitresult __xrtEventWaitUntil(xevent* pEvent, double iDeadline);
#endif
#if (defined(XRT_FEATURE_TASK_GROUP))
XRT_API xwaitresult __xrtTaskGroupWaitUntil(
	xtaskgroup* pGroup,
	double iDeadline
);
#endif
#if (defined(XRT_FEATURE_TASK_GROUP))
XRT_API xwaitresult __xrtTaskGroupWaitUntilCancel(
	xtaskgroup* pGroup,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_TASK_POOL))
XRT_API xfuture* __xrtTaskSubmitUntil(
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	double iDeadline
);
#endif
#if (defined(XRT_FEATURE_TASK_POOL))
XRT_API xfuture* __xrtTaskSubmitUntilCancel(
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_TASK_POOL))
XRT_API xwaitresult __xrtTaskPoolWaitUntil(xtaskpool* pPool, double iDeadline);
#endif
#if (defined(XRT_FEATURE_TASK_POOL))
XRT_API xwaitresult __xrtTaskPoolWaitUntilCancel(
	xtaskpool* pPool,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_TASK_GROUP_POOL))
XRT_API xfuture* __xrtTaskGroupSubmitUntil(
	xtaskgroup* pGroup,
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	double iDeadline
);
#endif
#if (defined(XRT_FEATURE_TASK_GROUP_POOL))
XRT_API xfuture* __xrtTaskGroupSubmitUntilCancel(
	xtaskgroup* pGroup,
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_TASK_NET))
XRT_API xfuture* __xrtTaskNetUntil(
	xnetengine* pEngine,
	uint64 iAffinity,
	xtasknetproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	double iDeadline
);
#endif
#if (defined(XRT_FEATURE_TASK_GROUP_NET))
XRT_API xfuture* __xrtTaskGroupNetUntil(
	xtaskgroup* pGroup,
	xnetengine* pEngine,
	uint64 iAffinity,
	xtasknetproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	double iDeadline
);
#endif
#if (defined(XRT_FEATURE_NET_TCP)) && (defined(XRT_FEATURE_NET_TCP_SYNC))
XRT_API bool __xrtNetStreamWait(
	xnetstream* pStream,
	xnetstreamwait Wait,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_NET_TCP)) && (defined(XRT_FEATURE_NET_TCP_SYNC))
XRT_API bool __xrtNetStreamWaitAvailable(
	xnetstream* pStream,
	size_t iMinimum,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_NET_TCP)) && (defined(XRT_FEATURE_NET_TCP_SYNC))
XRT_API xnetstream* __xrtNetListenerAcceptWait(
	xnetlistener* pListener,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_NET_TCP)) && (defined(XRT_FEATURE_NET_TCP_SYNC))
XRT_API xnetbytes* __xrtNetStreamRecv(
	xnetstream* pStream,
	size_t iMaxBytes,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_NET_TCP)) && (defined(XRT_FEATURE_NET_TCP_DIAL_SYNC))
XRT_API xnetstream* __xrtNetConnect(
	xnetengine* pEngine,
	xnetresolver* pResolver,
	cstr sHost,
	uint16 iPort,
	const xnetdialconfig* pConfig,
	const xnetstreamevents* pStreamEvents,
	ptr pStreamData,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_NET_TCP_SERVER)) && (defined(XRT_FEATURE_NET_TCP_SERVER_SYNC))
XRT_API xnetstream* __xrtNetServerAcceptWait(
	xnetserver* pServer,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_THREAD))
XRT_API xwaitresult __xrtThreadWaitUntil(xthread* pThread, double iDeadline);
#endif
#if (defined(XRT_FEATURE_TLS_STREAM)) && (defined(XRT_FEATURE_TLS_STREAM_LISTENER_SYNC))
XRT_API xtlsstream* __xrtTlsListenerAcceptWait(
	xtlslistener* pListener,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_NET_UDP)) && (defined(XRT_FEATURE_NET_UDP_SYNC))
XRT_API bool __xrtNetUdpWait(
	xnetudp* pUdp,
	xnetudpwait Wait,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_NET_UDP)) && (defined(XRT_FEATURE_NET_UDP_SYNC))
XRT_API bool __xrtNetUdpWritable(
	xnetudp* pUdp,
	size_t iSize,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_NET_UDP)) && (defined(XRT_FEATURE_NET_UDP_SYNC))
XRT_API xnetudppacket* __xrtNetUdpReceiveWait(
	xnetudp* pUdp,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_NET_UDP)) && (defined(XRT_FEATURE_NET_UDP_SYNC))
XRT_API xnetudperrorpacket* __xrtNetUdpReceiveErrorWait(
	xnetudp* pUdp,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_NET_UDP)) && (defined(XRT_FEATURE_NET_UDP_SYNC))
XRT_API xnetudpbatch* __xrtNetUdpReceiveBatchWait(
	xnetudp* pUdp,
	size_t iCapacity,
	double iDeadline,
	xcancel* pCancel
);
#endif
XRT_EXTERN_C_END
#endif
