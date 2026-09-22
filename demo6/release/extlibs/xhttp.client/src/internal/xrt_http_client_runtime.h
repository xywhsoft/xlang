#ifndef XRT_INTERNAL_HTTP_CLIENT_RUNTIME_H
#define XRT_INTERNAL_HTTP_CLIENT_RUNTIME_H

#include "xrt_http_client_stream.h"

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT)

#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
typedef struct __xrt_http_client_origin
	__xrt_http_client_origin;
typedef struct __xrt_http_client_idle
	__xrt_http_client_idle;
typedef struct __xrt_http_client_pool_shard
	__xrt_http_client_pool_shard;
#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_FUTURE)
typedef struct __xrt_http_client_wait
	__xrt_http_client_wait;
#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME)
typedef struct __xrt_http_client_resume
	__xrt_http_client_resume;



/* TLS ticket 缓存只需要稳定的 HTTP 路由身份，不依赖完整 Call。 */
typedef struct __xrt_http_resume_route {
	xhttpclient* Client;
	xstrview Host;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		xnetproxy* Proxy;
	#endif
	uint16 Port;
} __xrt_http_resume_route;
#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)

#define XRT_HTTP_CLIENT_CACHE_BOUNDARY_PREFIX "xrt-cache-"
#define XRT_HTTP_CLIENT_CACHE_BOUNDARY_PREFIX_SIZE 10u
#define XRT_HTTP_CLIENT_CACHE_BOUNDARY_RANDOM_SIZE 16u
#define XRT_HTTP_CLIENT_CACHE_BOUNDARY_SIZE \
	(XRT_HTTP_CLIENT_CACHE_BOUNDARY_PREFIX_SIZE + \
	 (XRT_HTTP_CLIENT_CACHE_BOUNDARY_RANDOM_SIZE * 2u))



/* 缓存重放范围状态只描述已经完成协议选择的交付方式。 */
typedef enum __xrt_http_client_cache_range {
	__XRT_HTTP_CLIENT_CACHE_RANGE_NONE = 0,
	__XRT_HTTP_CLIENT_CACHE_RANGE_PARTIAL,
	__XRT_HTTP_CLIENT_CACHE_RANGE_MULTIPART,
	__XRT_HTTP_CLIENT_CACHE_RANGE_UNSATISFIABLE
} __xrt_http_client_cache_range;



/* 源站 multipart/byteranges 规划结果区分非目标类型、保守跳过和可存储。 */
typedef enum __xrt_http_client_cache_multipart_decision {
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_ERROR = -1,
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_NONE = 0,
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_SKIP,
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_STORE
} __xrt_http_client_cache_multipart_decision;



/* 跳过原因保留协议边界，便于测试和后续诊断扩展。 */
typedef enum __xrt_http_client_cache_multipart_reason {
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_NONE = 0,
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_BOUNDARY =
		UINT32_C(0x00000001),
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_BODY =
		UINT32_C(0x00000002),
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_PARTS =
		UINT32_C(0x00000004),
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_HEADERS =
		UINT32_C(0x00000008),
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_RANGE =
		UINT32_C(0x00000010),
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_LENGTH =
		UINT32_C(0x00000020),
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_CONTENT_TYPE =
		UINT32_C(0x00000040),
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_ENCODING =
		UINT32_C(0x00000080),
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_OVERLAP =
		UINT32_C(0x00000100)
} __xrt_http_client_cache_multipart_reason;



typedef enum __xrt_http_client_cache_multipart_flag {
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_FLAG_NONE = 0,
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_HAS_CONTENT_TYPE =
		UINT32_C(0x00000001),
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_HAS_LENGTH =
		UINT32_C(0x00000002),
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_COMPLETE =
		UINT32_C(0x00000004)
} __xrt_http_client_cache_multipart_flag;



/*
	Parts 数组由规划结果拥有，正文视图借用输入 multipart 正文。
	ContentType 借用首个有效 Part Header，不能超过输入正文寿命。
*/
typedef struct __xrt_http_client_cache_multipart {
	xhttpcachepart* Parts;
	xstrview ContentType;
	uint64 Length;
	size_t PartCount;
	__xrt_http_client_cache_multipart_decision Decision;
	uint32 Flags;
	uint32 Reasons;
} __xrt_http_client_cache_multipart;

#endif



/* 高频诊断字段使用原子快照，避免为每次收发进度获取 Call 锁。 */
typedef struct __xrt_http_call_info {
	xatomic32 Phase;
	xatomic32 Result;
	xatomic32 Error;
	xatomic32 Reused;
	xatomic32 Secure;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		xatomic32 Cache;
	#endif
	xatomic64 Submitted;
	xatomic64 Started;
	xatomic64 TransportReady;
	xatomic64 RequestSent;
	xatomic64 FirstByte;
	xatomic64 Headers;
	xatomic64 LastProgress;
	xatomic64 Completed;
	xatomic64 RequestWireBytes;
	xatomic64 ResponseWireBytes;
	xatomic64 ResponseBodyBytes;
	xatomic64 Redirects;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		xatomic64 Retries;
	#endif
} __xrt_http_call_info;



/* Client 深复制或保留全部跨调用共享配置。 */
struct xhttpclient {
	volatile int32 References;
	volatile int32 Owners;
	xatomic64 NextAffinity;
	xatomic32 State;
	xspinlock LifecycleLock;
	xhttpcall* CallHead;
	xhttpcall* CallTail;
	size_t ActiveCalls;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_FUTURE)
		__xrt_http_client_wait* CloseWaitHead;
		__xrt_http_client_wait* CloseWaitTail;
	#endif
	xnetengine* Engine;
	xnetresolver* Resolver;
	xhttpclientconfig Config;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		__xrt_http_client_pool_shard* PoolShards;
		uint32 PoolShardCount;
		xatomic64 PoolConnections;
		xatomic64 PoolIdle;
		xatomic64 PoolClosing;
		xatomic64 PoolWaiting;
		xatomic64 PoolTimersPending;
		xatomic64 PoolLive;
		xatomic64 RequestsStarted;
		xatomic64 RequestsCompleted;
		xatomic64 ConnectionsOpened;
		xatomic64 ConnectionsReused;
		xatomic64 ConnectionsClosed;
		xatomic64 PoolWaits;
		xatomic64 PoolRejected;
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
			xatomic64 RedirectsFollowed;
		#endif
		bool PoolReady;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		xtlscontext* TlsContext;
		xtlsverifier* TlsVerifier;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME)
		xmutex ResumeLock;
		__xrt_http_client_resume* ResumeHead;
		__xrt_http_client_resume* ResumeTail;
		size_t ResumeCount;
		uint64 ResumeHits;
		uint64 ResumeMisses;
		uint64 ResumeStored;
		uint64 ResumeEvicted;
		uint64 ResumeExpired;
		uint64 ResumeDropped;
		bool ResumeReady;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
		xcookiejar* Cookies;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		xhttpcache* Cache;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		xnetproxy* Proxy;
	#endif
	bool OwnResolver;
	bool EngineHeld;
};



/* Call 的终态门、可取消对象和短生命周期标志都由 Lock 保护。 */
struct xhttpcall {
	volatile int32 References;
	xatomic32 State;
	xatomic32 CancelGate;
	xatomic32 FinishGate;
	xatomic32 TimeoutCause;
	xatomic32 TotalTimerDone;
	xatomic32 IdleTimerDone;
	xatomic64 TotalTimer;
	xatomic64 IdleTimer;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		xatomic32 RetryTimerDone;
		xatomic64 RetryTimer;
	#endif
	xdeadline TotalDeadline;
	xatomic64 IdleDeadline;
	__xrt_http_call_info Info;
	xspinlock Lock;
	xhttpclient* Client;
	xhttpcall* ClientPrevious;
	xhttpcall* ClientNext;
	xhttpcall* AbortNext;
	xnetworker* Worker;
	xhttp1exchange* Exchange;
	xnetdial* TcpDial;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		xnetproxydial* ProxyDial;
		xnetproxy* Proxy;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		xtlsdial* TlsDial;
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
			xtlsstream* ProxyTls;
			bool ProxyTlsStarting;
			bool ProxyTlsCallback;
		#endif
	#endif
	xhttp1call* StreamCall;
	xcancel* Cancel;
	xcancelwatch* CancelWatch;
	xhttpcallproc Done;
	ptr Data;
	xerror* Error;
	str Host;
	uint16 Port;
	uint64 Affinity;
	uint64 Timeout;
	uint64 IdleTimeout;
	uint64 ResponseBodyLimit;
	xhttprequest* Request;
	xhttp1requestoptions RequestOptions;
	xhttpcallevents Events;
	xhttp1exchangeevents InfoEvents;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)
		xhttp1exchangeevents DecompressEvents;
		xhttp1exchangeevents DecompressNext;
		xinflate** Inflaters;
		xhttpresponse* DecompressResponse;
		size_t InflaterCount;
		uint64 DecompressLimit;
		uint64 DecompressBodyBytes;
		uint32 DecompressMaxCodings;
		bool DecompressEnabled;
		bool DecompressActive;
		bool DecompressFailed;
		bool DecompressForwardFailed;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
		xhttprequest* RedirectRequest;
		xhttp1exchangeevents RedirectEvents;
		xhttp1exchangeevents RedirectNext;
		xhttpredirectconfig RedirectConfig;
		xhttpredirectmode RedirectMode;
		xhttpclienterror RedirectError;
		size_t Redirects;
		bool RedirectPending;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		xhttp1exchangeevents RetryEvents;
		xhttp1exchangeevents RetryNext;
		xhttpretryconfig RetryConfig;
		xrng RetryRng;
		uint64 RetryDelay;
		uint64 RetryResponseWireStart;
		uint32 RetryMax;
		size_t Retries;
		bool RetryEnabled;
		bool RetryUnsafe;
		bool RetryPending;
		bool RetryRngReady;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
		xhttp1exchangeevents CookieEvents;
		xhttp1exchangeevents CookieNext;
		str CookiePartitionKey;
		size_t CookiePartitionSize;
		uint32 CookieFlags;
		xhttpclienterror CookieError;
		bool CookiesEnabled;
		bool CookieAutomatic;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		xhttp1exchangeevents CacheEvents;
		xhttp1exchangeevents CacheNext;
		xhttpcacherecord* CacheCandidate;
		xhttpheaders* CacheRequestFields;
		xhttpheaders* CacheResponseFields;
		bytes CacheBody;
		size_t CacheBodySize;
		size_t CacheBodyCapacity;
		str CachePartitionKey;
		size_t CachePartitionSize;
		xhttpbyterange* CacheRanges;
		size_t CacheRangeCount;
		uint64 CacheRequestClock;
		uint64 CacheResponseClock;
		uint64 CacheRangeBodyLength;
		xtime CacheResponseTime;
		xhttpbyterange CacheRange;
		char CacheBoundary[
			XRT_HTTP_CLIENT_CACHE_BOUNDARY_SIZE + 1u
		];
		xhttpclientcachemode CacheMode;
		__xrt_http_client_cache_range CacheRangeState;
		bool CacheEnabled;
		bool CacheReady;
		bool CacheCapture;
		bool CacheRangeRequest;
		bool CacheRangeCovered;
		bool CacheRangeFill;
		bool CacheValidating;
		bool CacheNotModified;
		bool CacheIfNoneMatch;
		bool CacheIfModifiedSince;
		bool CacheIfRange;
		bool CacheFailed;
	#endif
	xnetpost StartPost;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		xnetpost PoolPost;
		__xrt_http_client_origin* PoolOrigin;
		__xrt_http_client_idle* PoolIdle;
		xhttpcall* PoolPrevious;
		xhttpcall* PoolNext;
		xnetstream* PooledTcp;
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
			xtlsstream* PooledTls;
		#endif
		bool PoolWaiting;
		bool PoolReserved;
		bool PoolOpened;
		uint32 PoolShardIndex;
	#endif
	bool Secure;
	bool ClientLinked;
	bool RuntimeHeld;
};



/* 建立客户端域错误并保留明确的下层原因。 */
xerror* __xrtHttpClientErrorCreate(
	xerrkind Kind,
	xhttpclienterror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



/* 建立稳定的取消或超时终态错误并接管旧错误。 */
xerror* __xrtHttpClientTerminalError(
	xerror* pCause,
	xhttpclienterror Code
);



/* 设置创建或同步准备阶段的当前线程客户端错误。 */
void __xrtHttpClientSetError(
	xerrkind Kind,
	xhttpclienterror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



/* 沿原因链返回最内层有效错误类别。 */
xerrkind __xrtHttpClientCauseKind(
	const xerror* pError,
	xerrkind Fallback
);



/* 发布高层阶段；提交后仅由所属 Worker 调用。 */
void __xrtHttpCallSetPhase(
	xhttpcall* pCall,
	xhttpcallphase Phase
);



/* 标记当前 Hop 的 TCP、代理或 TLS 传输已经可以承载 HTTP。 */
void __xrtHttpCallTransportReady(xhttpcall* pCall);



/* 标记当前 Hop 使用了连接池中的既有传输。 */
void __xrtHttpCallReused(xhttpcall* pCall);



/* 建立低级 HTTP/1 Call 使用的完成与真实 I/O 进度事件。 */
void __xrtHttpCallStreamEvents(
	xhttpcall* pCall,
	xhttp1callevents* pEvents
);



/* 增加只供 Call、Idle 和 Timer 使用的内部 Client 引用。 */
xhttpclient* __xrtHttpClientHold(xhttpclient* pClient);



/* 释放内部 Client 引用，不改变公开 Owner 数量。 */
void __xrtHttpClientRelease(xhttpclient* pClient);



/* 在 Client 仍运行时原子登记一个已经准备完成的 Call。 */
bool __xrtHttpClientCallAttach(xhttpcall* pCall);



/* 用户完成回调返回后摘除 Call，并尝试发布 Client 关闭终态。 */
void __xrtHttpClientCallDetach(xhttpcall* pCall);



/* 在全部 Call、池传输和池 Timer 退出后发布唯一关闭终态。 */
void __xrtHttpClientTryFinish(xhttpclient* pClient);



#if defined(XHTTP_FEATURE_HTTP_CLIENT_FUTURE)

/* 摘除并完成 Client 的全部关闭等待。 */
void __xrtHttpClientFutureFinish(xhttpclient* pClient);

#endif



/* 发布一个没有响应或升级传输的失败终态。 */
void __xrtHttpCallFail(
	xhttpcall* pCall,
	xnetresult Result,
	xhttpclienterror Code,
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



/* 发布带响应和可选升级传输的成功终态。 */
void __xrtHttpCallSucceed(
	xhttpcall* pCall,
	xhttpresponse* pResponse,
	xnetstream* pTcp,
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		xtlsstream* pTls,
	#endif
	size_t iBuffered,
	bool bUpgraded
);



/* 把低级 HTTP/1 Call 的唯一终态提升为完整客户端结果。 */
void __xrtHttpClientStreamDone(
	xhttp1call* pStreamCall,
	const xhttp1callresult* pResult,
	ptr pData
);



/* 为当前请求快照建立下一跳 Plan、Exchange 和传输端点。 */
bool __xrtHttpCallPrepareHop(
	xhttpcall* pCall
);



/* 在不重复安装总 Timer 和取消观察器的前提下获取并启动一跳传输。 */
void __xrtHttpCallStartHop(xhttpcall* pCall);



/* 在目标 Worker 上开始明文 TCP Dial。 */
bool __xrtHttpCallStartTcp(xhttpcall* pCall);



/* 接管一条已经建立的普通 TCP 传输并进入 HTTP/1。 */
void __xrtHttpCallTcpConnected(
	xhttpcall* pCall,
	xnetstream* pStream,
	cstr sOperation
);



#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)

/* 验证配置并让 Client 保留统一缓存句柄。 */
bool __xrtHttpClientCacheOpen(xhttpclient* pClient);



/* 释放 Client 持有的统一缓存句柄。 */
void __xrtHttpClientCacheClose(xhttpclient* pClient);



/* 释放源站 multipart 范围计划拥有的片段数组。 */
void __xrtHttpClientCacheMultipartUnit(
	__xrt_http_client_cache_multipart* pPlan
);



/* 把完整源站 multipart/byteranges 正文规划为排序、互不重叠的缓存片段。 */
__xrt_http_client_cache_multipart_decision
__xrtHttpClientCacheMultipartPlan(
	const xhttpcachefragmentinput* pBase,
	xstrview ContentType,
	xbytesview Body,
	size_t iMaxParts,
	__xrt_http_client_cache_multipart* pPlan
);



/* 冻结调用级缓存模式与分区，并验证 Client 缓存配置。 */
bool __xrtHttpClientCacheInit(
	xhttpcall* pCall,
	const xhttpcalloptions* pOptions
);



/* 释放 Call 持有的候选、字段快照、正文和分区文本。 */
void __xrtHttpClientCacheUnit(xhttpcall* pCall);



/* 在有效请求字段就绪后选择缓存条目或准备条件请求。 */
bool __xrtHttpClientCachePrepare(xhttpcall* pCall);



/* 把缓存观察器放在重定向外层，并保留每一跳原始表示。 */
const xhttp1exchangeevents* __xrtHttpClientCacheEvents(
	xhttpcall* pCall,
	const xhttp1exchangeevents* pNext
);



/* 在获取连接池配额前交付命中或 only-if-cached 响应。 */
bool __xrtHttpClientCacheStart(
	xhttpcall* pCall,
	bool* pHandled
);



/* 在重定向和解压收尾前提交网络响应或 304 更新。 */
bool __xrtHttpClientCacheDone(
	xhttpcall* pCall,
	xhttpresponse* pResponse,
	bool* pReplayed
);



/* 把严格模式下的缓存回调错误提升为高层终态。 */
bool __xrtHttpClientCacheFail(
	xhttpcall* pCall,
	const xerror* pCause
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)

/* 冻结本次调用的默认、直连或显式代理选择。 */
bool __xrtHttpProxyInit(
	xhttpcall* pCall,
	const xhttpcalloptions* pOptions
);



/* 经选定代理建立目标隧道，HTTPS 在隧道内继续 TLS 握手。 */
bool __xrtHttpCallStartProxy(xhttpcall* pCall);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)

/* 建立调用拥有的初始请求快照和重定向事件包装器。 */
bool __xrtHttpRedirectInit(
	xhttpcall* pCall,
	const xhttpcalloptions* pOptions
);



/* 释放 Call 尚未提交的重定向请求。 */
void __xrtHttpRedirectUnit(xhttpcall* pCall);



/* 返回仅向最终响应转发 Header 和正文的 Exchange 事件。 */
const xhttp1exchangeevents* __xrtHttpRedirectEvents(
	xhttpcall* pCall,
	const xhttp1exchangeevents* pNext
);



/* 判断低层失败是否由重定向策略产生，并发布对应高层错误。 */
bool __xrtHttpRedirectFail(
	xhttpcall* pCall,
	const xerror* pCause
);



/* 提交已经排空的中间响应并准备下一跳 Exchange。 */
bool __xrtHttpRedirectAdvance(xhttpcall* pCall);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)

/* 冻结单次调用的重试模式、限额和非幂等授权。 */
bool __xrtHttpRetryInit(
	xhttpcall* pCall,
	const xhttpcalloptions* pOptions
);



/* 返回先隐藏待重试响应、再向缓存及用户事件转发的包装器。 */
const xhttp1exchangeevents* __xrtHttpRetryEvents(
	xhttpcall* pCall,
	const xhttp1exchangeevents* pNext
);



/* 判断当前成功响应是否已经被重试策略接管。 */
bool __xrtHttpRetryPending(const xhttpcall* pCall);



/* 排空当前响应或结束失败尝试后，安排下一次重放。 */
bool __xrtHttpRetrySchedule(xhttpcall* pCall);



/* 判断可重放的临时传输失败并在命中策略时安排下一次尝试。 */
bool __xrtHttpRetryFailure(
	xhttpcall* pCall,
	xnetresult Result,
	xhttpclienterror Code,
	xerrkind Kind,
	const xerror* pCause
);



/* 取消仍在退避等待中的 Timer；完成回调负责发布取消终态。 */
bool __xrtHttpRetryCancel(xhttpcall* pCall);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)

/* 冻结单次调用的自动解码策略并按需注入 Accept-Encoding。 */
bool __xrtHttpDecompressInit(
	xhttpcall* pCall,
	const xhttpcalloptions* pOptions
);



/* 释放当前响应的全部解码器并清除每跳状态。 */
void __xrtHttpDecompressReset(xhttpcall* pCall);



/* 返回只向最终响应应用内容解码的事件包装器。 */
const xhttp1exchangeevents* __xrtHttpDecompressEvents(
	xhttpcall* pCall,
	const xhttp1exchangeevents* pNext
);



/* 完成全部叠加编码并发布准确的解码正文长度。 */
bool __xrtHttpDecompressFinish(
	xhttpcall* pCall,
	xhttpresponse* pResponse
);



/* 把解码器失败映射为稳定的高层客户端错误。 */
bool __xrtHttpDecompressFail(
	xhttpcall* pCall,
	const xerror* pCause
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)

/* 复制每次调用的 Cookie 上下文并建立启用状态。 */
bool __xrtHttpCookieInit(
	xhttpcall* pCall,
	const xhttpcalloptions* pOptions
);



/* 释放 Call 拥有的 Cookie 上下文。 */
void __xrtHttpCookieUnit(xhttpcall* pCall);



/* 在当前请求快照上移除旧自动字段并重新选择 Cookie。 */
bool __xrtHttpCookiePrepare(xhttpcall* pCall);



/* 把同步准备阶段的 Cookie 根因包装为当前线程客户端错误。 */
void __xrtHttpCookieSetSubmitError(xhttpcall* pCall);



/* 返回先存储 Set-Cookie、再转发到下层事件的包装器。 */
const xhttp1exchangeevents* __xrtHttpCookieEvents(
	xhttpcall* pCall,
	const xhttp1exchangeevents* pNext
);



/* 判断低层失败是否由 Cookie 策略产生，并发布高层错误。 */
bool __xrtHttpCookieFail(
	xhttpcall* pCall,
	const xerror* pCause
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)

/* 建立 Client 连接池状态。 */
bool __xrtHttpPoolInit(xhttpclient* pClient);



/* 释放已经排空的 Client 连接池状态。 */
void __xrtHttpPoolUnit(xhttpclient* pClient);



/* 安排 Call 复用、创建新连接或进入有界等待队列。 */
bool __xrtHttpPoolAcquire(xhttpcall* pCall, bool* pReady);



/* 取消仍在连接池等待队列中的 Call。 */
bool __xrtHttpPoolCancel(xhttpcall* pCall);



/* 标记新拨号已经建立一条实际连接。 */
void __xrtHttpPoolOpened(xhttpcall* pCall);



/* 把可复用传输直接交给等待者或放入空闲 LRU。 */
bool __xrtHttpPoolPut(
	xhttpcall* pCall,
	const xhttp1callresult* pResult
);



/* 已取出的空闲传输成功交给低级 HTTP/1 Call。 */
void __xrtHttpPoolPooledUsed(xhttpcall* pCall);



/* 已取出的空闲传输失效，保留连接配额并准备重新拨号。 */
void __xrtHttpPoolPooledStale(xhttpcall* pCall);



/* 升级或隧道传输离开池管理，但不计为物理关闭。 */
void __xrtHttpPoolTransferred(xhttpcall* pCall);



/* 唯一终态清理等待、待接管空闲传输和连接配额。 */
void __xrtHttpPoolFinish(xhttpcall* pCall);



/* 在空闲 TCP Stream 所属 Worker 上重新附加 HTTP/1 Call。 */
bool __xrtHttpCallStartPooledTcp(xhttpcall* pCall);



/* 连接池唤醒后在选定 Worker 上继续复用或拨号。 */
void __xrtHttpCallPoolReady(
	xnetworker* pWorker,
	ptr pData
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)

/* 为 Client 建立或保留 TLS Context 与验证器。 */
bool __xrtHttpClientTlsInit(xhttpclient* pClient);



/* 为目标 Host 建立一致的 Context、SNI、ALPN 与验证器配置。 */
void __xrtHttpCallTlsConfig(
	xhttpcall* pCall,
	xtlsclientconfig* pConfig
);



/* 释放 Client 持有的 TLS 共享对象。 */
void __xrtHttpClientTlsUnit(xhttpclient* pClient);



/* 在目标 Worker 上开始带验证和 ALPN 的 TLS Dial。 */
bool __xrtHttpCallStartTls(xhttpcall* pCall);



/* 接管一条已经完成验证的 TLS 传输并进入 HTTP/1。 */
void __xrtHttpCallTlsConnected(
	xhttpcall* pCall,
	xtlsstream* pStream,
	cstr sOperation
);



#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)

/* 在空闲 TLS Stream 所属 Worker 上重新附加 HTTP/1 Call。 */
bool __xrtHttpCallStartPooledTls(xhttpcall* pCall);

#endif

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME)

/* 建立 Client 独享的有界 TLS 恢复票据缓存。 */
bool __xrtHttpResumeInit(xhttpclient* pClient);



/* 释放 Client 票据缓存及其代理和恢复对象引用。 */
void __xrtHttpResumeUnit(xhttpclient* pClient);



/* 为本次 TLS 握手取出一张同路由、仍有效的单次票据。 */
xtlsresume* __xrtHttpResumeTake(xhttpcall* pCall);



/*
	把一张票据按 HTTP 路由加入缓存。
	无论是否缓存成功，函数都会接管 pResume 的调用方引用。
*/
void __xrtHttpResumeStore(
	const __xrt_http_resume_route* pRoute,
	xtlsresume* pResume
);



/* 从完成 HTTP 事务的 TLS 会话接管全部新票据。 */
void __xrtHttpResumeCollect(
	xhttpcall* pCall,
	xtlsstream* pStream
);



/* 从活动 Call 之外的同路由 TLS Stream 接管新票据。 */
void __xrtHttpResumeCollectRoute(
	const __xrt_http_resume_route* pRoute,
	xtlsstream* pStream
);

#endif



#endif

#endif
