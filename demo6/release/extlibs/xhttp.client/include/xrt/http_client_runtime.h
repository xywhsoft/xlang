#ifndef XRT_HTTP_CLIENT_RUNTIME_H
#define XRT_HTTP_CLIENT_RUNTIME_H


#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM) || \
	 !defined(XRT_FEATURE_NET_TCP_DIAL) || \
	 !defined(XRT_FEATURE_CANCEL))
	#error "XRT HTTP client support requires HTTP stream, TCP Dial and cancellation"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE) && \
	(!defined(XHTTP_FEATURE_HTTP_CACHE_STORE) || \
	 !defined(XHTTP_FEATURE_HTTP_CACHE_RANGE) || \
	 !defined(XHTTP_FEATURE_HTTP_RANGE_MULTIPART) || \
	 !defined(XHTTP_FEATURE_MULTIPART) || \
	 !defined(XRT_FEATURE_CODEC_HEX) || \
	 !defined(XRT_FEATURE_RANDOM_SECURE))
	#error "XRT HTTP client cache requires cache storage, ranges, range multipart, generic multipart, hexadecimal codec and secure random"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT) || \
	 !defined(XHTTP_FEATURE_HTTP_CLIENT_TLS) || \
	 !defined(XRT_FEATURE_TLS_STREAM_DIAL) || \
	 !defined(XRT_FEATURE_TLS_CLIENT_VERIFY) || \
	 !defined(XRT_FEATURE_X509_STORE_SYSTEM))
	#error "XRT HTTPS client support requires HTTP client, TLS Dial, verification and system trust"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS) || \
	 !defined(XRT_FEATURE_TLS_CLIENT_RESUME) || \
	 !defined(XRT_FEATURE_MUTEX))
	#error "XRT HTTP client resumption requires HTTPS, TLS client resumption and mutex"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL) && \
	!defined(XHTTP_FEATURE_HTTP_CLIENT)
	#error "XRT HTTP client pool support requires the high-level HTTP client"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT) && \
	!defined(XHTTP_FEATURE_HTTP_CLIENT)
	#error "XRT HTTP client redirect support requires the high-level HTTP client"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT) || \
	 !defined(XHTTP_FEATURE_HTTP_RETRY) || \
	 !defined(XRT_FEATURE_RANDOM))
	#error "XRT HTTP client retry requires the HTTP client, HTTP retry protocol and random support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT) || \
	 !defined(XRT_FEATURE_NET_PROXY_DIAL))
	#error "XRT HTTP client proxy support requires the HTTP client and proxy Dial"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT) || \
	 !defined(XHTTP_FEATURE_COOKIE_JAR_HEADERS))
	#error "XRT HTTP client cookies require the HTTP client and CookieJar Header integration"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT) || \
	 !defined(XRT_FEATURE_HTTP_ENCODING) || \
	 !defined(XRT_FEATURE_INFLATE))
	#error "XRT HTTP client decompression requires the HTTP client, HTTP Encoding and Inflate"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT) || \
	 !defined(XHTTP_FEATURE_HTTP_CACHE_STORE))
	#error "XRT HTTP client cache requires the HTTP client and cache store"
#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT)

#define XHTTP_CLIENT_TIMEOUT_DEFAULT UINT64_C(30000000)
#define XHTTP_CLIENT_IDLE_TIMEOUT_DEFAULT UINT64_C(30000000)
#define XHTTP_CLIENT_TIMEOUT_NONE UINT64_MAX



/* 可选实现只裁剪代码，公开配置中的借用句柄声明始终可见。 */
typedef struct xhttpcache xhttpcache;
typedef struct xnetproxy xnetproxy;
typedef struct xcookiejar xcookiejar;
typedef struct xtlscontext xtlscontext;
typedef struct xtlsverifier xtlsverifier;



/* Client 保存共享网络策略、解析缓存和可选 TLS 信任快照。 */
typedef struct xhttpclient xhttpclient;



/* Call 表示从请求快照到最终响应或升级传输的一次完整执行。 */
typedef struct xhttpcall xhttpcall;



/*
	高层 Call 事件始终携带当前 Call，避免异步提交返回值与首个回调之间的发布竞态。
	Response 只在回调期间借用；返回 false 会终止当前 Call，并保留当前线程错误作为原因。
*/
typedef bool (*xhttpcallinformationalproc)(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	ptr pData
);

typedef bool (*xhttpcallheadersproc)(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	ptr pData
);

typedef bool (*xhttpcallbodyproc)(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	xbytesview Data,
	ptr pData
);



/*
	Body 为空时缓冲正文；非空时流式交付，Data 必须覆盖 Call 生命周期。
	Body 回调内 WireBodyBytes 已包含当前块，BodyBytes 在回调成功返回后累计。
*/
typedef struct xhttpcallevents {
	xhttpcallinformationalproc Informational;
	xhttpcallheadersproc Headers;
	xhttpcallbodyproc Body;
	ptr Data;
} xhttpcallevents;



#define XHTTP_CLIENT_CACHE_BODY_DEFAULT \
	(UINT64_C(8) * 1024u * 1024u)
#define XHTTP_CLIENT_CACHE_HEURISTIC_MAX_DEFAULT \
	(UINT64_C(24) * 60u * 60u * 1000000u)
#define XHTTP_CLIENT_CACHE_HEURISTIC_PERCENT_DEFAULT \
	UINT32_C(10)
#define XHTTP_CLIENT_CACHE_MAX_RANGES_DEFAULT 16u



/* 每次调用可以继承、禁用、强制验证或禁止回源。 */
typedef enum xhttpclientcachemode {
	XHTTP_CLIENT_CACHE_DEFAULT = 0,
	XHTTP_CLIENT_CACHE_DISABLED,
	XHTTP_CLIENT_CACHE_RELOAD,
	XHTTP_CLIENT_CACHE_ONLY
} xhttpclientcachemode;



/* Cache 结果描述最终可见响应的来源，不混入具体存储后端状态。 */
typedef enum xhttpclientcacheoutcome {
	XHTTP_CLIENT_CACHE_NONE = 0,
	XHTTP_CLIENT_CACHE_MISS,
	XHTTP_CLIENT_CACHE_HIT,
	XHTTP_CLIENT_CACHE_STALE,
	XHTTP_CLIENT_CACHE_REVALIDATED,
	XHTTP_CLIENT_CACHE_UPDATED,
	XHTTP_CLIENT_CACHE_ONLY_MISS,
	XHTTP_CLIENT_CACHE_BYPASS
} xhttpclientcacheoutcome;



/*
	Store 为空时禁用 Client 自动缓存；创建 Client 时增加一个引用。
	MaxBody 是单次回源允许暂存的编码正文上限，避免无界中间内存。
	MaxRanges 限制一次本地 Range 解析的输入项数，防止多范围放大。
	Heuristic 使用 Last-Modified 的相对年龄并受百分比和最大寿命共同限制。
	Strict 让存储后端错误终止调用；默认采用不中断网络响应的 fail-open。
*/
typedef struct xhttpclientcacheconfig {
	xhttpcache* Store;
	uint64 MaxBody;
	uint64 HeuristicMax;
	size_t MaxRanges;
	uint32 HeuristicPercent;
	bool Shared;
	bool Heuristic;
	bool Strict;
} xhttpclientcacheconfig;



/* PartitionKey 隔离用户、站点或租户；空视图表示未分区缓存。 */
typedef struct xhttpclientcacheoptions {
	xhttpclientcachemode Mode;
	xstrview PartitionKey;
} xhttpclientcacheoptions;

/* Client 状态单向进入排空、终止和关闭终态。 */
typedef enum xhttpclientstate {
	XHTTP_CLIENT_RUNNING = 0,
	XHTTP_CLIENT_DRAINING,
	XHTTP_CLIENT_ABORTING,
	XHTTP_CLIENT_CLOSED
} xhttpclientstate;



/* Call 状态只向前进入一个不可变终态。 */
typedef enum xhttpcallstate {
	XHTTP_CALL_QUEUED = 0,
	XHTTP_CALL_DIALING,
	XHTTP_CALL_HANDSHAKING,
	XHTTP_CALL_EXCHANGING,
	XHTTP_CALL_SUCCEEDED,
	XHTTP_CALL_FAILED,
	XHTTP_CALL_CANCELLED
} xhttpcallstate;



/* Phase 表示最近开始的实际阶段，终态后保留成功或失败发生的位置。 */
typedef enum xhttpcallphase {
	XHTTP_CALL_PHASE_QUEUED = 0,
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		XHTTP_CALL_PHASE_CACHE = 1,
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		XHTTP_CALL_PHASE_RETRY = 2,
	#endif
	XHTTP_CALL_PHASE_POOL = 3,
	XHTTP_CALL_PHASE_CONNECT = 4,
	XHTTP_CALL_PHASE_PROXY = 5,
	XHTTP_CALL_PHASE_TLS = 6,
	XHTTP_CALL_PHASE_REQUEST = 7,
	XHTTP_CALL_PHASE_RESPONSE_HEADERS = 8,
	XHTTP_CALL_PHASE_RESPONSE_BODY = 9
} xhttpcallphase;



/* 客户端错误按请求、传输、响应处理、协议和策略阶段稳定分类。 */
typedef enum xhttpclienterror {
	XHTTP_CLIENT_ERROR_NONE = 0,
	XHTTP_CLIENT_ERROR_ARGUMENT = 1,
	XHTTP_CLIENT_ERROR_STATE = 2,
	XHTTP_CLIENT_ERROR_CONFIG = 3,
	XHTTP_CLIENT_ERROR_REQUEST = 4,
	XHTTP_CLIENT_ERROR_RESPONSE = 5,
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		XHTTP_CLIENT_ERROR_CACHE = 6,
	#endif
	XHTTP_CLIENT_ERROR_POOL = 7,
	XHTTP_CLIENT_ERROR_DIAL = 8,
	XHTTP_CLIENT_ERROR_PROXY = 9,
	XHTTP_CLIENT_ERROR_TLS = 10,
	XHTTP_CLIENT_ERROR_TRANSPORT = 11,
	XHTTP_CLIENT_ERROR_PROTOCOL = 12,
	XHTTP_CLIENT_ERROR_CALLBACK = 13,
	XHTTP_CLIENT_ERROR_TIMEOUT_TOTAL = 14,
	XHTTP_CLIENT_ERROR_TIMEOUT_IDLE = 15,
	XHTTP_CLIENT_ERROR_CANCELLED = 16,
	XHTTP_CLIENT_ERROR_REDIRECT = 17,
	XHTTP_CLIENT_ERROR_REDIRECT_LIMIT = 18,
	XHTTP_CLIENT_ERROR_REDIRECT_REPLAY = 19,
	XHTTP_CLIENT_ERROR_REDIRECT_DOWNGRADE = 20,
	XHTTP_CLIENT_ERROR_RETRY = 21,
	XHTTP_CLIENT_ERROR_COOKIE = 22,
	XHTTP_CLIENT_ERROR_DECOMPRESSION = 23,
	XHTTP_CLIENT_ERROR_INTERNAL = 24
} xhttpclienterror;



/*
	Info 是可并发读取的单调时钟快照，时间单位均为微秒。
	未到达的时间点为零；Result 在运行期间为 AGAIN，终态后不再变化。
*/
typedef struct xhttpcallinfo {
	/* 当前生命周期、实际阶段和稳定客户端错误。 */
	xhttpcallstate State;
	xhttpcallphase Phase;
	xnetresult Result;
	xhttpclienterror Error;

	/* 完整 Call 的单调时间点。 */
	uint64 Submitted;
	uint64 Started;
	uint64 TransportReady;
	uint64 RequestSent;
	uint64 FirstByte;
	uint64 Headers;
	uint64 LastProgress;
	uint64 Completed;

	/* 线路累计量与最终可见响应正文量。 */
	uint64 RequestWireBytes;
	uint64 ResponseWireBytes;
	uint64 ResponseBodyBytes;

	/* 完整调用的重定向、复用和最终传输事实。 */
	size_t Redirects;
	size_t Retries;
	bool ReusedConnection;
	bool Secure;
	xhttpclientcacheoutcome Cache;
} xhttpcallinfo;



/*
	DEFAULT 继承 Client 默认代理，DIRECT 显式绕过默认代理，
	EXPLICIT 使用本次 Call 提供的不可变代理对象。
*/
typedef enum xhttpproxymode {
	XHTTP_PROXY_DEFAULT = 0,
	XHTTP_PROXY_DIRECT,
	XHTTP_PROXY_EXPLICIT
} xhttpproxymode;



/* EXPLICIT 要求 Proxy 非空；其他模式要求 Proxy 为空，避免忽略配置。 */
typedef struct xhttpproxyoptions {
	xhttpproxymode Mode;
	const xnetproxy* Proxy;
} xhttpproxyoptions;



#define XHTTP_REDIRECT_MAX_DEFAULT UINT32_C(10)

#define XHTTP_REDIRECT_POST_TO_GET \
	UINT32_C(0x00000001)
#define XHTTP_REDIRECT_FORWARD_CREDENTIALS \
	UINT32_C(0x00000002)
#define XHTTP_REDIRECT_ALLOW_DOWNGRADE \
	UINT32_C(0x00000004)



/* 重定向配置用零跳明确关闭自动跟随，其余字段只调整安全策略。 */
typedef struct xhttpredirectconfig {
	uint32 Flags;
	uint32 MaxHops;
} xhttpredirectconfig;



/* 每次调用可以继承客户端策略、强制跟随、返回原响应或拒绝重定向。 */
typedef enum xhttpredirectmode {
	XHTTP_REDIRECT_DEFAULT = 0,
	XHTTP_REDIRECT_FOLLOW,
	XHTTP_REDIRECT_MANUAL,
	XHTTP_REDIRECT_ERROR
} xhttpredirectmode;



#define XHTTP_RETRY_MAX_DEFAULT UINT32_C(2)
#define XHTTP_RETRY_BASE_DEFAULT UINT64_C(250000)
#define XHTTP_RETRY_DELAY_MAX_DEFAULT UINT64_C(30000000)

#define XHTTP_RETRY_STATUS UINT32_C(0x00000001)
#define XHTTP_RETRY_TRANSPORT UINT32_C(0x00000002)
#define XHTTP_RETRY_RESPECT_AFTER UINT32_C(0x00000004)
#define XHTTP_RETRY_JITTER UINT32_C(0x00000008)

#define XHTTP_RETRY_UNSAFE UINT32_C(0x00000001)



/*
	MaxRetries 为零时关闭 Client 默认重试。
	BaseDelay 与 MaxDelay 使用微秒；Flags 分别控制状态、传输、服务端建议和抖动。
*/
typedef struct xhttpretryconfig {
	uint64 BaseDelay;
	uint64 MaxDelay;
	uint32 Flags;
	uint32 MaxRetries;
} xhttpretryconfig;



/* 单次调用可以继承、显式启用或显式关闭 Client 重试策略。 */
typedef enum xhttpretrymode {
	XHTTP_RETRY_DEFAULT = 0,
	XHTTP_RETRY_ENABLED,
	XHTTP_RETRY_DISABLED
} xhttpretrymode;



/* UNSAFE 只对本次调用显式允许非幂等方法重放。 */
typedef struct xhttpretryoptions {
	xhttpretrymode Mode;
	uint32 Flags;
} xhttpretryoptions;



#define XHTTP_COOKIE_DISABLED	UINT32_C(0x00000001)
#define XHTTP_COOKIE_SAME_SITE	UINT32_C(0x00000002)
#define XHTTP_COOKIE_TOP_LEVEL	UINT32_C(0x00000004)



/*
	默认按普通同站 HTTP API 请求选择 Cookie。
	PartitionKey 由 Call 复制，空视图表示未分区请求。
*/
typedef struct xhttpcookieoptions {
	uint32 Flags;
	xstrview PartitionKey;
} xhttpcookieoptions;



/*
	连接上限的零值表示不限制，等待上限的零值也表示不限制。
	任一 Idle 上限为零都会关闭复用；IdleTimeout 为零只关闭过期清扫。
*/
typedef struct xhttpclientpoolconfig {
	/* 全 Client 正在拨号、使用或保留配额的连接上限；零表示不限制。 */
	size_t MaxConnections;
	/* 单 Origin 连接上限；零表示不限制。 */
	size_t MaxConnectionsPerOrigin;
	/* 全 Client 等待 Call 上限；零表示不限制。 */
	size_t MaxWaiting;
	/* 单 Origin 等待 Call 上限；零表示不限制。 */
	size_t MaxWaitingPerOrigin;
	/* 全 Client 可复用空闲连接上限；零表示不保留。 */
	size_t MaxIdle;
	/* 单 Origin 可复用空闲连接上限；零表示不保留。 */
	size_t MaxIdlePerOrigin;
	/* 空闲连接保留时间，单位为微秒；零表示不按时间清扫。 */
	uint64 IdleTimeout;
} xhttpclientpoolconfig;



/*
	当前数量和累计计数都可并发读取，但不承诺来自同一个全局时刻。
	累计计数从 Client 创建起单调递增。
*/
typedef struct xhttpclientstats {
	/* 正在拨号、使用或已保留配额，但尚未进入空闲 LRU 的连接。 */
	size_t ActiveConnections;
	/* 当前位于空闲 LRU 的可复用连接。 */
	size_t IdleConnections;
	/* 已释放池配额、正在异步关闭的空闲连接。 */
	size_t ClosingConnections;
	/* 尚未取得连接或连接配额的 Call。 */
	size_t WaitingCalls;
	/* 已经被 Client 接受的 Call 总数。 */
	uint64 RequestsStarted;
	/* 已经进入唯一终态的 Call 总数。 */
	uint64 RequestsCompleted;
	/* 已经完成 TCP 或 TLS 建立的物理连接总数。 */
	uint64 ConnectionsOpened;
	/* 从空闲池或同源直交取得传输的次数。 */
	uint64 ConnectionsReused;
	/* 由池确认关闭或淘汰的物理连接总数。 */
	uint64 ConnectionsClosed;
	/* 曾经进入连接等待队列的 Call 总数。 */
	uint64 PoolWaits;
	/* 因全局或单 Origin 等待上限被拒绝的 Call 总数。 */
	uint64 PoolRejected;
	/* 全部 Call 实际已经跟随的重定向跳数。 */
	uint64 RedirectsFollowed;
} xhttpclientstats;

#define XHTTP_RESUME_ENTRIES_DEFAULT ((size_t)64u)
#define XHTTP_RESUME_ORIGIN_DEFAULT ((size_t)4u)



/*
	票据按验证 host、目标端口和代理对象身份隔离。
	任一上限为零都会关闭缓存，但不关闭基础 HTTPS。
*/
typedef struct xhttpresumeconfig {
	size_t MaxEntries;
	size_t MaxEntriesPerOrigin;
} xhttpresumeconfig;



/* 所有字段在同一缓存锁内读取，构成一致快照。 */
typedef struct xhttpresumestats {
	size_t Entries;
	uint64 Hits;
	uint64 Misses;
	uint64 Stored;
	uint64 Evicted;
	uint64 Expired;
	uint64 Dropped;
} xhttpresumestats;

#define XHTTP_DECOMPRESS_BODY_DEFAULT UINT64_C(67108864)
#define XHTTP_DECOMPRESS_CODINGS_DEFAULT UINT32_C(4)
#define XHTTP_DECOMPRESS_CODINGS_MAX UINT32_C(16)



/* 单次调用可继承 Client、强制自动解码或保留线路正文。 */
typedef enum xhttpdecompressmode {
	XHTTP_DECOMPRESS_DEFAULT = 0,
	XHTTP_DECOMPRESS_AUTO,
	XHTTP_DECOMPRESS_RAW
} xhttpdecompressmode;



/*
	Enabled 控制 Client 默认行为；MaxBody 约束最终和中间解码层的输出。
	MaxCodings 限制可自动处理的 Content-Encoding 叠加层数。
*/
typedef struct xhttpdecompressconfig {
	uint64 MaxBody;
	uint32 MaxCodings;
	bool Enabled;
} xhttpdecompressconfig;



/*
	Timeout 覆盖排队、DNS、TCP、代理、TLS 和 HTTP I/O 的总时长。
	IdleTimeout 限制没有传输进度的连续时长；两者单位均为微秒。
	Resolver 只在 xrtHttpClientCreate 创建私有解析器时使用。
*/
typedef struct xhttpclientconfig {
	xnetresolverconfig Resolver;
	xnetdialconfig Dial;
	xhttp1callconfig Call;
	xhttp1exchangeconfig Exchange;
	uint64 Timeout;
	uint64 IdleTimeout;
	/* Client 创建时保留默认代理；空指针表示默认直连。 */
	const xnetproxy* Proxy;
	xhttpredirectconfig Redirect;
	xhttpretryconfig Retry;
	/* 可选共享 Jar 在 Client 创建时增加引用。 */
	xcookiejar* Cookies;
	xhttpdecompressconfig Decompress;
	xhttpclientcacheconfig Cache;
	xhttpclientpoolconfig Pool;
	xtlsstreamconfig TlsStream;
	const xtlscontext* TlsContext;
	const xtlsverifier* TlsVerifier;
	bool SystemTrust;
	xhttpresumeconfig Resume;
} xhttpclientconfig;



/*
	Timeout 和 IdleTimeout 为零时分别继承 Client，NONE 显式关闭对应限制。
	Cancel 在构造期间增加引用；Events 回调数据必须覆盖 Call 生命周期。
*/
typedef struct xhttpcalloptions {
	xhttp1requestoptions Request;
	xhttpcallevents Events;
	xcancel* Cancel;
	uint64 Timeout;
	uint64 IdleTimeout;
	/* 零值继承 Client；限制解码前表示正文，UINT64_MAX 允许无界流。 */
	uint64 ResponseBodyLimit;
	xhttpproxyoptions Proxy;
	xhttpredirectmode Redirect;
	xhttpretryoptions Retry;
	xhttpcookieoptions Cookies;
	xhttpdecompressmode Decompress;
	xhttpclientcacheoptions Cache;
} xhttpcalloptions;



/*
	结果只在完成回调期间借用。
	Response 及升级后的 Tcp/Tls 调用方引用在回调入口转移给调用方。
*/
typedef struct xhttpcallresult {
	xnetresult Result;
	xhttpresponse* Response;
	xnetstream* Tcp;
	xtlsstream* Tls;
	const xerror* Error;
	xhttpcallinfo Info;
	size_t Buffered;
	bool Upgraded;
} xhttpcallresult;



/*
	完成回调在本次 Call 选择的网络 Worker 上执行且至多一次。
	回调可能与提交线程并发执行，必须使用参数中的 pCall 识别本次调用。
*/
typedef void (*xhttpcallproc)(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
);



XRT_EXTERN_C_BEGIN



/* 初始化安全公网限额、两类 30 秒超时、Happy Eyeballs 和可选系统信任；允许未对齐存储。 */
XRT_API void xrtHttpClientConfigInit(xhttpclientconfig* pConfig);



/* 初始化 origin-form、无回调、无取消并继承 Client 的两类超时；允许未对齐存储。 */
XRT_API void xrtHttpCallOptionsInit(xhttpcalloptions* pOptions);



#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)

/* 初始化为继承 Client 默认代理；允许未对齐完整存储并拒绝地址回绕。 */
XRT_API void xrtHttpProxyOptionsInit(
	xhttpproxyoptions* pOptions
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)

/* 初始化十跳上限和安全重定向策略；允许未对齐完整存储并拒绝地址回绕。 */
XRT_API void xrtHttpRedirectConfigInit(
	xhttpredirectconfig* pConfig
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)

/* 初始化为零次自动重试及完整安全策略；允许未对齐完整存储并拒绝地址回绕。 */
XRT_API void xrtHttpRetryConfigInit(
	xhttpretryconfig* pConfig
);



/* 初始化为继承 Client 且不允许非幂等重放；允许未对齐完整存储并拒绝地址回绕。 */
XRT_API void xrtHttpRetryOptionsInit(
	xhttpretryoptions* pOptions
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)

/* 初始化默认 Cookie 策略；接受未对齐的完整存储并拒绝回绕地址。 */
XRT_API void xrtHttpCookieOptionsInit(
	xhttpcookieoptions* pOptions
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)

/* 初始化自动解码、64 MiB 正文上限和四层编码上限；允许未对齐存储。 */
XRT_API void xrtHttpDecompressConfigInit(
	xhttpdecompressconfig* pConfig
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)

/* 初始化默认缓存策略；接受未对齐的完整存储并拒绝回绕地址。 */
XRT_API void xrtHttpClientCacheConfigInit(
	xhttpclientcacheconfig* pConfig
);



/* 初始化继承且未分区的缓存选项；存储可以未对齐但必须完整。 */
XRT_API void xrtHttpClientCacheOptionsInit(
	xhttpclientcacheoptions* pOptions
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)

/*
	初始化无连接硬上限、128/8 空闲上限和 90 秒空闲过期。
	输出允许未对齐，但必须是完整且不发生地址环绕的结构存储。
*/
XRT_API void xrtHttpClientPoolConfigInit(
	xhttpclientpoolconfig* pConfig
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME)

/* 初始化默认恢复缓存；接受未对齐的完整存储并拒绝回绕地址。 */
XRT_API void xrtHttpResumeConfigInit(
	xhttpresumeconfig* pConfig
);

#endif



/*
	创建拥有私有异步 Resolver 的 Client。
	Client 占用运行中的 Engine 生命周期，直到最后一个 Client/Call 引用释放。
*/
XRT_API xhttpclient* xrtHttpClientCreate(
	xnetengine* pEngine,
	const xhttpclientconfig* pConfig
);



#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)

/*
	使用默认 HTTP 策略和显式 TLS 对象创建 Client。
	Context 为空时创建默认上下文；Verifier 为空时使用系统信任。
*/
XRT_API xhttpclient* xrtHttpClientCreateTls(
	xnetengine* pEngine,
	const xtlscontext* pContext,
	const xtlsverifier* pVerifier
);

#endif



/*
	创建借用共享 Resolver 的 Client。
	Resolver 必须保持有效到最后一个 Client/Call 引用释放。
*/
XRT_API xhttpclient* xrtHttpClientCreateWithResolver(
	xnetengine* pEngine,
	xnetresolver* pResolver,
	const xhttpclientconfig* pConfig
);



/* 增加 Client 公开所有者引用并返回原指针。 */
XRT_API xhttpclient* xrtHttpClientRef(xhttpclient* pClient);



/*
	释放 Client 公开所有者引用。
	最后一个所有者会隐式开始平滑排空，但不会等待排空完成。
*/
XRT_API void xrtHttpClientDestroy(xhttpclient* pClient);



/* 停止接收新 Call，关闭空闲连接并让全部已提交 Call 自然完成。 */
XRT_API bool xrtHttpClientDrain(xhttpclient* pClient);



/* 停止接收新 Call，关闭空闲连接并协作取消全部已提交 Call。 */
XRT_API bool xrtHttpClientAbort(xhttpclient* pClient);



/* 返回 Client 当前生命周期状态。 */
XRT_API xhttpclientstate xrtHttpClientState(
	const xhttpclient* pClient
);



/* 返回 Client 生命周期绑定的借用网络 Engine；空 Client 返回空指针。 */
XRT_API xnetengine* xrtHttpClientEngine(
	const xhttpclient* pClient
);



/*
	冻结 Request 并异步执行一次 HTTP/1 请求。
	成功后 Call 独立于 Request；同步失败不接管 Request，也不会调用 Done。
*/
XRT_API xhttpcall* xrtHttpClientDo(
	xhttpclient* pClient,
	const xhttprequest* pRequest,
	const xhttpcalloptions* pOptions,
	xhttpcallproc pDone,
	ptr pData
);



/* 增加 Call 引用并返回原指针。 */
XRT_API xhttpcall* xrtHttpCallRef(xhttpcall* pCall);



/* 释放 Call 引用；空指针视为空操作。 */
XRT_API void xrtHttpCallDestroy(xhttpcall* pCall);



/*
	从任意线程协作取消 DNS、TCP、TLS 或 HTTP/1 阶段。
	返回 true 表示取消已被接纳，最终结果必为 CANCELLED；
	返回 false 表示已经接纳过取消或不可变终态已经提交。
*/
XRT_API bool xrtHttpCallCancel(xhttpcall* pCall);



/* 在当前网络 Worker 上暂停正在交付的响应正文和传输读取。 */
XRT_API bool xrtHttpCallPause(xhttpcall* pCall);



/* 从任意线程恢复当前 HTTP/1 响应输入。 */
XRT_API bool xrtHttpCallResume(xhttpcall* pCall);



/* 返回当前 HTTP/1 响应输入暂停门的并发快照。 */
XRT_API bool xrtHttpCallPaused(const xhttpcall* pCall);



/* 返回 Call 从提交到终态始终所属的借用网络 Worker。 */
XRT_API xnetworker* xrtHttpCallWorker(const xhttpcall* pCall);



/* 从任意线程克隆 Call 当前有效请求；调用方负责销毁返回的拥有型快照。 */
XRT_API xhttprequest* xrtHttpCallRequestClone(
	const xhttpcall* pCall
);



/* 返回 Call 当前阶段或不可变终态的并发快照。 */
XRT_API xhttpcallstate xrtHttpCallState(const xhttpcall* pCall);



/* 失败、超时或取消后返回借用的稳定错误原因链。 */
XRT_API const xerror* xrtHttpCallError(const xhttpcall* pCall);



/* 复制 Call 快照；输出存储可以未对齐但必须完整且地址不回绕。 */
XRT_API bool xrtHttpCallInfo(
	const xhttpcall* pCall,
	xhttpcallinfo* pInfo
);



#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)

/*
	返回 Client 借用的默认代理。
	未配置默认代理时返回空指针且不设置错误。
*/
XRT_API const xnetproxy* xrtHttpClientProxy(
	const xhttpclient* pClient
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)

/*
	返回 Client 借用的共享 CookieJar。
	未配置自动 Cookie 时返回空指针且不设置错误。
*/
XRT_API xcookiejar* xrtHttpClientCookieJar(
	const xhttpclient* pClient
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)

/*
	返回 Client 借用的统一 Cache 句柄。
	未配置自动缓存时返回空指针且不设置错误。
*/
XRT_API xhttpcache* xrtHttpClientCache(
	const xhttpclient* pClient
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)

/* 关闭当前全部空闲连接并返回从池中摘除的数量，不影响活动 Call。 */
XRT_API size_t xrtHttpClientCloseIdle(xhttpclient* pClient);



/* 读取并发快照；输出存储可以未对齐但必须完整且地址不回绕。 */
XRT_API bool xrtHttpClientStats(
	const xhttpclient* pClient,
	xhttpclientstats* pStats
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME)

/* 清空尚未被连接取用的 TLS 恢复票据，并返回释放数量。 */
XRT_API size_t xrtHttpClientResumeClear(xhttpclient* pClient);



/* 读取恢复缓存一致快照；输出存储可以未对齐但必须完整。 */
XRT_API bool xrtHttpClientResumeStats(
	const xhttpclient* pClient,
	xhttpresumestats* pStats
);

#endif



XRT_EXTERN_C_END

#endif

#endif
