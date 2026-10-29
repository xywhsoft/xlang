#ifndef XRT_TCP_H
#define XRT_TCP_H

#include <xrt/net.h>

#if defined(XRT_FEATURE_NET_TCP_FILE)
	#include <xrt/file.h>
#endif

#if defined(XRT_FEATURE_NET_TCP_FUTURE) || \
	defined(XRT_FEATURE_NET_TCP_DIAL_FUTURE) || \
	defined(XRT_FEATURE_NET_TCP_SYNC) || \
	defined(XRT_FEATURE_NET_TCP_DIAL_SYNC)
	#include <xrt/future.h>
#endif



#if defined(XRT_FEATURE_NET_TCP) && !defined(XRT_FEATURE_NET_ENGINE)
	#error "XRT TCP support requires network engine support"
#endif

#if defined(XRT_FEATURE_NET_TCP_FILE) && \
	(!defined(XRT_FEATURE_NET_TCP) || !defined(XRT_FEATURE_FILE))
	#error "XRT TCP file support requires TCP and file support"
#endif

#if defined(XRT_FEATURE_NET_TCP_FUTURE) && !defined(XRT_FEATURE_NET_TCP)
	#error "XRT TCP Future support requires TCP support"
#endif

#if defined(XRT_FEATURE_NET_TCP_FUTURE) && !defined(XRT_FEATURE_FUTURE)
	#error "XRT TCP Future support requires Future support"
#endif

#if defined(XRT_FEATURE_NET_TCP_DIAL) && \
	(!defined(XRT_FEATURE_NET_TCP) || !defined(XRT_FEATURE_NET_RESOLVER))
	#error "XRT TCP Dial support requires TCP and Resolver support"
#endif

#if defined(XRT_FEATURE_NET_TCP_DIAL_FUTURE) && \
	(!defined(XRT_FEATURE_NET_TCP_DIAL) || \
	 !defined(XRT_FEATURE_FUTURE) || \
	 !defined(XRT_FEATURE_FUTURE_BRIDGE))
	#error "XRT TCP Dial Future support requires TCP Dial, Future and Future bridge support"
#endif

#if defined(XRT_FEATURE_NET_TCP_SYNC) && \
	(!defined(XRT_FEATURE_NET_TCP_FUTURE) || !defined(XRT_FEATURE_NET_SYNC))
	#error "XRT TCP sync support requires TCP Future and network sync support"
#endif

#if defined(XRT_FEATURE_NET_TCP_DIAL_SYNC) && \
	(!defined(XRT_FEATURE_NET_TCP_SYNC) || \
	 !defined(XRT_FEATURE_NET_TCP_DIAL_FUTURE))
	#error "XRT TCP Dial sync support requires TCP sync and TCP Dial Future support"
#endif



#if defined(XRT_FEATURE_NET_TCP)

typedef struct xnetstream xnetstream;
typedef struct xnetlistener xnetlistener;

#if defined(XRT_FEATURE_NET_TCP_DIAL)
typedef struct xnetdial xnetdial;
#endif



/* Stream 状态只向前推进，CLOSED 是唯一终态。 */
typedef enum xnetstreamstate {
	XNET_STREAM_CONNECTING = 0,
	XNET_STREAM_OPEN,
	XNET_STREAM_CLOSING,
	XNET_STREAM_CLOSED
} xnetstreamstate;



/* Listener 状态只向前推进，关闭后不能重新监听。 */
typedef enum xnetlistenerstate {
	XNET_LISTENER_OPEN = 0,
	XNET_LISTENER_CLOSING,
	XNET_LISTENER_CLOSED
} xnetlistenerstate;



/* 接受结果可以跨 Worker 轮转，也可以固定留在 Listener 所属 Worker。 */
typedef enum xnetacceptdistribution {
	XNET_ACCEPT_ROUND_ROBIN = 0,
	XNET_ACCEPT_LOCAL
} xnetacceptdistribution;



#if defined(XRT_FEATURE_NET_TCP_DIAL)
/* Dial 状态只向前推进，连接成功、失败和取消都是不可变终态。 */
typedef enum xnetdialstate {
	XNET_DIAL_RESOLVING = 0,
	XNET_DIAL_CONNECTING,
	XNET_DIAL_CONNECTED,
	XNET_DIAL_FAILED,
	XNET_DIAL_CANCELLED
} xnetdialstate;
#endif



#if defined(XRT_FEATURE_NET_TCP_FUTURE)
/* Stream 等待条件是水平条件；Future 只表示本次等待，不接管 Stream。 */
typedef enum xnetstreamwait {
	XNET_STREAM_WAIT_OPEN = 0,
	XNET_STREAM_WAIT_READ,
	XNET_STREAM_WAIT_WRITE,
	XNET_STREAM_WAIT_DRAIN,
	XNET_STREAM_WAIT_CLOSE
} xnetstreamwait;
#endif



/* Stream 回调全部在所属 Worker 上串行执行。 */
typedef struct xnetstreamevents {
	void (*Open)(xnetstream* pStream, ptr pData);
	void (*Read)(xnetstream* pStream, xnetbuf* pBuffer, ptr pData);
	void (*End)(xnetstream* pStream, ptr pData);
	void (*HighWater)(xnetstream* pStream, size_t iQueued, ptr pData);
	void (*LowWater)(xnetstream* pStream, size_t iQueued, ptr pData);
	void (*Drain)(xnetstream* pStream, ptr pData);
	void (*Close)(xnetstream* pStream, xnetresult Result,
		const xerror* pError, ptr pData);
} xnetstreamevents;



/* Accept 成功返回 true 并接管一个 Stream 引用，返回 false 会立即拒绝连接。 */
typedef struct xnetlistenerevents {
	bool (*Accept)(xnetlistener* pListener,
		xnetstream* pStream, ptr pData);
	void (*Error)(xnetlistener* pListener,
		const xerror* pError, ptr pData);
	void (*Close)(xnetlistener* pListener, ptr pData);
} xnetlistenerevents;



/* 完成式读取可在吞吐、空闲内存和两者自适应之间选择。 */
typedef enum xnetstreamreadmode {
	XNET_STREAM_READ_ADAPTIVE = 0,
	XNET_STREAM_READ_DIRECT,
	XNET_STREAM_READ_PROBE
} xnetstreamreadmode;



/* 所有字节容量都是硬边界，ConnectTimeout 使用微秒。 */
typedef struct xnetstreamconfig {
	size_t ReadSize;
	size_t ReadLimit;
	size_t WriteHighWater;
	size_t WriteLowWater;
	size_t WriteLimit;
	uint64 ConnectTimeout;
	xnetstreamreadmode ReadMode;
	bool NoDelay;
	bool KeepAlive;
} xnetstreamconfig;



/* Listener 只绑定一个地址；多端口和复用端口由后续 Server 层管理。 */
typedef struct xnetlistenconfig {
	xnetaddr Address;
	xnetstreamconfig Stream;
	uint64 Affinity;
	uint32 AcceptConcurrency;
	uint32 AcceptQueueLimit;
	int Backlog;
	xnetacceptdistribution Distribution;
	bool ReuseAddress;
	bool ReusePort;
	bool ExclusiveAddress;
	bool IPv6Only;
} xnetlistenconfig;



/* Stream 统计是无锁并发快照，累计值在关闭后仍可读取。 */
typedef struct xnetstreamstats {
	xnetstreamstate State;
	uint64 ReceivedBytes;
	uint64 SentBytes;
	uint64 ReadEvents;
	uint64 WriteEvents;
	uint64 SendRejected;
	size_t BufferedBytes;
	size_t QueuedBytes;
	size_t PeakQueuedBytes;
	bool ReadPaused;
	bool ReadBlocked;
	bool ReadEnded;
	bool WriteEnded;
	bool WriteBackpressured;
} xnetstreamstats;



#if defined(XRT_FEATURE_NET_TCP_DIAL)
/* Timeout 和 FallbackDelay 使用微秒；MaxAttempts 是解析结果的硬上限。 */
typedef struct xnetdialconfig {
	xnetstreamconfig Stream;
	xnetfamily Family;
	uint64 Affinity;
	uint64 Timeout;
	uint64 FallbackDelay;
	uint32 MaxAttempts;
} xnetdialconfig;



/* Dial 统计是无锁快照，WinnerIndex 只在 HasWinner 为真时有效。 */
typedef struct xnetdialstats {
	xnetdialstate State;
	uint32 Addresses;
	uint32 AttemptsStarted;
	uint32 AttemptsFailed;
	uint32 ActiveAttempts;
	uint32 PeakAttempts;
	size_t WinnerIndex;
	bool HasWinner;
} xnetdialstats;



/*
	完成回调在 Affinity Worker 上至多执行一次，不会从 xrtNetDial 调用栈重入。
	pDial 和 Error 只在回调期间借用；成功回调接管 Stream 引用。
*/
typedef void (*xnetdialproc)(
	xnetdial* pDial,
	xnetresult Result,
	xnetstream* pStream,
	const xerror* pError,
	ptr pData
);
#endif



/* Listener 统计区分系统接受、用户拒绝和内部错误。 */
typedef struct xnetlistenerstats {
	xnetlistenerstate State;
	uint64 Accepted;
	uint64 Rejected;
	uint64 Errors;
	uint32 ActiveAccepts;
	uint32 ActiveDispatches;
	uint32 QueuedAccepts;
	uint32 PeakQueuedAccepts;
	uint32 AcceptWaiters;
} xnetlistenerstats;



XRT_EXTERN_C_BEGIN



/* 初始化 Stream 的自适应读取、背压和连接超时默认值。 */
XRT_API void xrtNetStreamConfigInit(xnetstreamconfig* pConfig);



/* 初始化 IPv4 动态端口 Listener 及其默认 Stream 配置。 */
XRT_API void xrtNetListenConfigInit(xnetlistenconfig* pConfig);



#if defined(XRT_FEATURE_NET_TCP_DIAL)
/* 初始化双栈交错、总超时和单地址 Stream 的默认策略。 */
XRT_API void xrtNetDialConfigInit(xnetdialconfig* pConfig);



/* 完整验证 Dial 策略及其嵌套 Stream 配置。 */
XRT_API bool xrtNetDialConfigValid(const xnetdialconfig* pConfig);
#endif



/* 异步连接一个数字地址；返回值拥有一个调用方引用。 */
XRT_API xnetstream* xrtNetStreamConnect(
	xnetengine* pEngine,
	const xnetaddr* pRemote,
	uint64 iAffinity,
	const xnetstreamconfig* pConfig,
	const xnetstreamevents* pEvents,
	ptr pData
);



#if defined(XRT_FEATURE_NET_TCP_DIAL)
/*
	解析主机并延迟竞争多个地址；成功 Stream 引用转移给完成回调。
	非 Worker 提交者可能与完成回调并发，不能依赖返回值已经完成赋值。
*/
XRT_API xnetdial* xrtNetDial(
	xnetengine* pEngine,
	xnetresolver* pResolver,
	cstr sHost,
	uint16 iPort,
	const xnetdialconfig* pConfig,
	const xnetstreamevents* pStreamEvents,
	ptr pStreamData,
	xnetdialproc pDone,
	ptr pDoneData
);



/* 增加 Dial 引用并返回原指针。 */
XRT_API xnetdial* xrtNetDialRef(xnetdial* pDial);



/* 释放 Dial 引用；空指针视为空操作。 */
XRT_API void xrtNetDialDestroy(xnetdial* pDial);



/* 原子争取取消终态；返回真后完成结果必为 CANCELLED。 */
XRT_API bool xrtNetDialCancel(xnetdial* pDial);



/* 返回 Dial 当前状态的原子快照。 */
XRT_API xnetdialstate xrtNetDialState(const xnetdial* pDial);



/* 失败或取消后返回借用的结构化错误，其他状态返回空指针。 */
XRT_API const xerror* xrtNetDialError(const xnetdial* pDial);



/* 取得解析地址、并发尝试和获胜地址的无锁统计快照。 */
XRT_API bool xrtNetDialStats(
	const xnetdial* pDial,
	xnetdialstats* pStats
);
#endif



/* 同步完成创建、选项、绑定和监听，再异步预投递 Accept。 */
XRT_API xnetlistener* xrtNetListen(
	xnetengine* pEngine,
	const xnetlistenconfig* pConfig,
	const xnetlistenerevents* pEvents,
	const xnetstreamevents* pStreamEvents,
	ptr pData
);



/* 线程安全地增加 Stream 引用；无效计数或溢出时返回空。 */
XRT_API xnetstream* xrtNetStreamRef(xnetstream* pStream);



/* 释放 Stream 引用；空指针视为空操作。 */
XRT_API void xrtNetStreamDestroy(xnetstream* pStream);



/* 线程安全地增加 Listener 引用；无效计数或溢出时返回空。 */
XRT_API xnetlistener* xrtNetListenerRef(xnetlistener* pListener);



/* 释放 Listener 引用；关闭操作必须另行请求。 */
XRT_API void xrtNetListenerDestroy(xnetlistener* pListener);



/* 拉取模式下非阻塞取走一个已接受 Stream；空队列返回空指针，Stream 不继承 Listener 数据。 */
XRT_API xnetstream* xrtNetListenerAccept(xnetlistener* pListener);



/* 有界复制发送；队列达到 WriteLimit 时返回 AGAIN。 */
XRT_API xnetresult xrtNetStreamSend(
	xnetstream* pStream,
	const void* pData,
	size_t iSize
);



/* 有界聚集复制发送；所有片段在返回前完成复制。 */
XRT_API xnetresult xrtNetStreamSendVec(
	xnetstream* pStream,
	const xnetspan* pSpans,
	size_t iCount
);



/* 有界零复制发送；成功后在数据离开队列时执行一次释放过程。 */
XRT_API xnetresult xrtNetStreamSendRef(
	xnetstream* pStream,
	const void* pData,
	size_t iSize,
	xnetreleaseproc pRelease,
	ptr pContext
);



/* 原子受理一组零复制引用；失败时全部所有权仍归调用方。 */
XRT_API xnetresult xrtNetStreamSendRefs(
	xnetstream* pStream,
	const xnetref* pRefs,
	size_t iCount
);



/* 有界接管非空数据；NULL,0 是无操作，非空指针配零长度是参数错误。 */
XRT_API xnetresult xrtNetStreamSendTake(
	xnetstream* pStream,
	ptr pData,
	size_t iSize
);



/* 在所属 Worker 上零复制接管缓冲链；失败时源缓冲保持不变。 */
XRT_API xnetresult xrtNetStreamSendBuffer(
	xnetstream* pStream,
	xnetbuf* pBuffer
);



#if defined(XRT_FEATURE_NET_TCP_FILE)

/*
	有界发送文件区间；成功后复制原生文件句柄，调用方可以立即关闭 File。
	明文 TCP 使用内核文件发送，区间大小不能超过 Stream WriteLimit。
*/
XRT_API xnetresult xrtNetStreamSendFile(
	xnetstream* pStream,
	xfile File,
	uint64 iOffset,
	size_t iSize
);

#endif



/* 暂停新读取；一个已经提交的 completion 仍可能到达。 */
XRT_API void xrtNetStreamPause(xnetstream* pStream);



/* 无分配恢复读取，并把并发请求合并后唤醒所属 Worker。 */
XRT_API bool xrtNetStreamResume(xnetstream* pStream);



/* 排空发送队列后执行 TCP 写半关闭，读取方向继续工作。 */
XRT_API bool xrtNetStreamShutdownWrite(xnetstream* pStream);



/* 停止读取并在发送队列排空后正常关闭。 */
XRT_API bool xrtNetStreamClose(xnetstream* pStream);



/* 取消在途 IO、丢弃发送队列并立即异常关闭。 */
XRT_API bool xrtNetStreamAbort(xnetstream* pStream);



/* 请求停止接受新连接并排空在途 Accept。 */
XRT_API bool xrtNetListenerClose(xnetlistener* pListener);



/* 返回 Stream 当前状态的并发快照。 */
XRT_API xnetstreamstate xrtNetStreamState(const xnetstream* pStream);



/* 返回 Listener 当前状态的并发快照。 */
XRT_API xnetlistenerstate xrtNetListenerState(const xnetlistener* pListener);



/* 返回已经占用发送预算但尚未离开队列的字节数。 */
XRT_API size_t xrtNetStreamPending(const xnetstream* pStream);



/* 返回创建 Stream 时固定的发送硬上限。 */
XRT_API size_t xrtNetStreamWriteLimit(const xnetstream* pStream);



/* 返回当前仍可受理的发送硬预算快照。 */
XRT_API size_t xrtNetStreamWritable(const xnetstream* pStream);



/* 返回当前累积的可读字节数；该并发快照不会借出缓冲。 */
XRT_API size_t xrtNetStreamAvailable(const xnetstream* pStream);



/* 在所属 Worker 内返回借用的只读接收缓冲；不能保存或直接消费。 */
XRT_API const xnetbuf* xrtNetStreamBuffer(xnetstream* pStream);



/* 在所属 Worker 内复制并消费最多指定字节。 */
XRT_API size_t xrtNetStreamRead(
	xnetstream* pStream,
	void* pOutput,
	size_t iSize
);



/* 在所属 Worker 内消费最多指定字节而不复制。 */
XRT_API size_t xrtNetStreamConsume(xnetstream* pStream, size_t iSize);



/* 复制 Stream 本地地址，成功才修改输出。 */
XRT_API bool xrtNetStreamLocal(const xnetstream* pStream, xnetaddr* pAddress);



/* 复制 Stream 远端地址，成功才修改输出。 */
XRT_API bool xrtNetStreamRemote(const xnetstream* pStream, xnetaddr* pAddress);



/* 复制 Listener 实际绑定地址，支持查询动态端口。 */
XRT_API bool xrtNetListenerLocal(
	const xnetlistener* pListener,
	xnetaddr* pAddress
);



/* 返回 Stream 所属的借用 Worker。 */
XRT_API xnetworker* xrtNetStreamWorker(const xnetstream* pStream);



/* 返回 Listener 所属的借用 Worker。 */
XRT_API xnetworker* xrtNetListenerWorker(const xnetlistener* pListener);



/* 只在 Stream Worker 回调内返回借用 Socket；不能关闭或接管其 IO。 */
XRT_API xnetsocket xrtNetStreamSocket(xnetstream* pStream);



/*
	在所属 Worker 上替换 Stream 事件与用户数据。
	允许在 Accept 回调或已打开 Stream 的回调中调用，不自动重放当前接收缓冲。
*/
XRT_API bool xrtNetStreamSetEvents(
	xnetstream* pStream,
	const xnetstreamevents* pEvents,
	ptr pData
);



/* 只在 Stream Worker 回调内替换用户数据。 */
XRT_API bool xrtNetStreamSetData(xnetstream* pStream, ptr pData);



/* 返回线程安全的 Stream 用户数据指针快照，不延长目标生命周期。 */
XRT_API ptr xrtNetStreamData(const xnetstream* pStream);



/* 返回创建时保存的 Listener 用户数据，不延长目标生命周期。 */
XRT_API ptr xrtNetListenerData(const xnetlistener* pListener);



/* 返回导致 Stream 关闭的借用错误；正常关闭时为空。 */
XRT_API const xerror* xrtNetStreamError(const xnetstream* pStream);



/* 复制 Stream 并发统计。 */
XRT_API bool xrtNetStreamStats(
	const xnetstream* pStream,
	xnetstreamstats* pStats
);



/* 复制 Listener 并发统计。 */
XRT_API bool xrtNetListenerStats(
	const xnetlistener* pListener,
	xnetlistenerstats* pStats
);



#if defined(XRT_FEATURE_NET_TCP_FUTURE)
/* 拉取模式下异步接受一个连接；成功值由 Future 持有一个 Stream 引用。 */
XRT_API xfuture* xrtNetListenerAcceptAsync(xnetlistener* pListener);



/* 异步等待 Stream 条件；成功、失败、取消和关闭映射到统一 Future 终态。 */
XRT_API xfuture* xrtNetStreamWaitAsync(
	xnetstream* pStream,
	xnetstreamwait Wait
);



/* 拉取模式下异步接收当前可用字节；成功值是借用的 xnetbytes。 */
XRT_API xfuture* xrtNetStreamRecvAsync(
	xnetstream* pStream,
	size_t iMaxBytes
);
#endif



#if defined(XRT_FEATURE_NET_TCP_SYNC)
/* 阻塞等待一个 Stream 条件；禁止从该 Stream 所属 Worker 调用。 */
XRT_API bool xrtNetStreamWait(
	xnetstream* pStream,
	xnetstreamwait Wait,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 阻塞接受一个不继承 Listener 数据的连接并返回调用方引用；禁止从 Listener Worker 调用。 */
XRT_API xnetstream* xrtNetListenerAcceptWait(
	xnetlistener* pListener,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 阻塞接收一段拥有型字节；零上限表示读取全部当前缓冲。 */
XRT_API xnetbytes* xrtNetStreamRecv(
	xnetstream* pStream,
	size_t iMaxBytes,
	xdeadline iDeadline,
	xcancel* pCancel
);
#endif



#if defined(XRT_FEATURE_NET_TCP_DIAL_FUTURE)
/* 把托管连接包装为 Future；成功值是由 Future 持有的 Stream。 */
XRT_API xfuture* xrtNetDialAsync(
	xnetengine* pEngine,
	xnetresolver* pResolver,
	cstr sHost,
	uint16 iPort,
	const xnetdialconfig* pConfig,
	const xnetstreamevents* pStreamEvents,
	ptr pStreamData
);
#endif



#if defined(XRT_FEATURE_NET_TCP_DIAL_SYNC)
/* 阻塞完成主机解析、候选竞速与连接，并返回调用方 Stream 引用。 */
XRT_API xnetstream* xrtNetConnect(
	xnetengine* pEngine,
	xnetresolver* pResolver,
	cstr sHost,
	uint16 iPort,
	const xnetdialconfig* pConfig,
	const xnetstreamevents* pStreamEvents,
	ptr pStreamData,
	xdeadline iDeadline,
	xcancel* pCancel
);
#endif



XRT_EXTERN_C_END

#endif

#endif
