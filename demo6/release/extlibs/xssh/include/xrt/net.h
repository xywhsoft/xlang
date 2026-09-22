#ifndef XRT_NET_H
#define XRT_NET_H

#include <xrt/error.h>



/* 网络累计统计按编译期级别裁剪；状态量和背压计数始终保留。 */
#define XNET_STATS_OFF 0
#define XNET_STATS_BASIC 1
#define XNET_STATS_FULL 2

#if !defined(XRT_NET_STATS_LEVEL)
	#define XRT_NET_STATS_LEVEL XNET_STATS_FULL
#endif

#if (XRT_NET_STATS_LEVEL < XNET_STATS_OFF) || \
	(XRT_NET_STATS_LEVEL > XNET_STATS_FULL)
	#error "XRT_NET_STATS_LEVEL must be XNET_STATS_OFF, XNET_STATS_BASIC or XNET_STATS_FULL"
#endif

#if defined(XRT_FEATURE_NET_RESOLVER) || \
	defined(XRT_FEATURE_NET_PORT) || \
	defined(XRT_FEATURE_NET_PORT_URING) || \
	defined(XRT_FEATURE_NET_PORT_IOCP)
	#include <xrt/atomic.h>
#endif

#if defined(XRT_FEATURE_NET_RESOLVER)
	#include <xrt/hash.h>
	#include <xrt/sync.h>
	#include <xrt/thread.h>
#endif

#if defined(XRT_FEATURE_NET_RESOLVER_FUTURE)
	#include <xrt/future.h>
#endif

#if defined(XRT_FEATURE_NET_PORT) || defined(XRT_FEATURE_NET_ENGINE)
	#include <xrt/wait.h>
#endif

#if defined(XRT_FEATURE_NET_ENGINE)
	#include <xrt/queue.h>
	#include <xrt/thread.h>
#endif



#if defined(XRT_FEATURE_NET_BUFFER) && !defined(XRT_FEATURE_NET)
	#error "XRT network buffer support requires XRT_FEATURE_NET"
#endif

#if defined(XRT_FEATURE_NET_SOCKET) && !defined(XRT_FEATURE_NET)
	#error "XRT socket support requires XRT_FEATURE_NET"
#endif

#if defined(XRT_FEATURE_NET_DNS) && !defined(XRT_FEATURE_NET)
	#error "XRT DNS support requires XRT_FEATURE_NET"
#endif

#if defined(XRT_FEATURE_NET_RESOLVER) && \
	(!defined(XRT_FEATURE_NET_DNS) || !defined(XRT_FEATURE_ATOMIC) || \
	 !defined(XRT_FEATURE_HASH64) || !defined(XRT_FEATURE_THREAD) || \
	 !defined(XRT_FEATURE_COND))
	#error "XRT resolver support requires DNS, atomic, hash64, thread and condition support"
#endif

#if defined(XRT_FEATURE_NET_RESOLVER_FUTURE) && \
	(!defined(XRT_FEATURE_NET_RESOLVER) || \
	 !defined(XRT_FEATURE_FUTURE) || \
	 !defined(XRT_FEATURE_FUTURE_BRIDGE))
	#error "XRT resolver Future support requires resolver, Future and Future bridge support"
#endif

#if defined(XRT_FEATURE_NET_SYNC) && \
	(!defined(XRT_FEATURE_NET_ENGINE) || !defined(XRT_FEATURE_FUTURE))
	#error "XRT network sync support requires network engine and Future support"
#endif

#if defined(XRT_FEATURE_NET_PORT) && \
	(!defined(XRT_FEATURE_NET_SOCKET) || !defined(XRT_FEATURE_ATOMIC) || \
	 !defined(XRT_FEATURE_WAIT) || \
	 !defined(XRT_FEATURE_MUTEX))
	#error "XRT network port support requires socket, atomic, wait and mutex support"
#endif

#if defined(XRT_FEATURE_NET_PORT_SELECT) && !defined(XRT_FEATURE_NET_PORT)
	#error "XRT select network port support requires XRT_FEATURE_NET_PORT"
#endif

#if defined(XRT_FEATURE_NET_PORT_EPOLL) && !defined(XRT_FEATURE_NET_PORT)
	#error "XRT epoll network port support requires XRT_FEATURE_NET_PORT"
#endif

#if defined(XRT_FEATURE_NET_PORT_URING) && \
	(!defined(XRT_FEATURE_NET_PORT) || !defined(XRT_FEATURE_ATOMIC))
	#error "XRT io_uring network port support requires network port and atomic support"
#endif

#if defined(XRT_FEATURE_NET_PORT_KQUEUE) && !defined(XRT_FEATURE_NET_PORT)
	#error "XRT kqueue network port support requires XRT_FEATURE_NET_PORT"
#endif

#if defined(XRT_FEATURE_NET_PORT_IOCP) && \
	(!defined(XRT_FEATURE_NET_PORT) || !defined(XRT_FEATURE_ATOMIC))
	#error "XRT IOCP network port support requires network port and atomic support"
#endif

#if defined(XRT_FEATURE_NET_ENGINE) && \
	(!defined(XRT_FEATURE_NET_PORT) || !defined(XRT_FEATURE_NET_BUFFER) || \
	 !defined(XRT_FEATURE_THREAD) || !defined(XRT_FEATURE_QUEUE_MPSC))
	#error "XRT network engine support requires network port, buffer, thread and MPSC queue support"
#endif

#if defined(XRT_FEATURE_NET_ENGINE)
	#if defined(_WIN32) || defined(_WIN64)
		#if !defined(XRT_FEATURE_NET_PORT_IOCP) || \
			!defined(XRT_FEATURE_NET_PORT_SELECT)
			#error "XRT network engine requires IOCP and select fallback on Windows"
		#endif
	#elif defined(__linux__)
		#if (!defined(XRT_FEATURE_NET_PORT_EPOLL) && \
			 !defined(XRT_FEATURE_NET_PORT_URING)) || \
			!defined(XRT_FEATURE_NET_PORT_SELECT)
			#error "XRT network engine requires epoll or io_uring plus select fallback on Linux"
		#endif
	#elif defined(__APPLE__) || defined(__FreeBSD__) || \
		defined(__OpenBSD__) || defined(__NetBSD__) || \
		defined(__DragonFly__)
		#if !defined(XRT_FEATURE_NET_PORT_KQUEUE) || \
			!defined(XRT_FEATURE_NET_PORT_SELECT)
			#error "XRT network engine requires kqueue and select fallback on Darwin/BSD"
		#endif
	#elif !defined(XRT_FEATURE_NET_PORT_SELECT)
		#error "XRT network engine requires select fallback on this platform"
	#endif
#endif



/* 地址族使用稳定值，不依赖平台 AF_INET 和 AF_INET6 常量。 */
typedef enum xnetfamily {
	XNET_FAMILY_UNSPEC = 0,
	XNET_FAMILY_IPV4 = 4,
	XNET_FAMILY_IPV6 = 6
} xnetfamily;



/* 端口使用主机字节序，地址字节始终使用网络字节序。 */
typedef struct xnetaddr {
	uint16 Family;
	uint16 Port;
	uint32 Scope;
	uint8 Address[16];
} xnetaddr;



/* 网络操作把正常控制结果与结构化错误分开表达。 */
typedef enum xnetresult {
	XNET_RESULT_ERROR = -1,
	XNET_RESULT_OK = 0,
	XNET_RESULT_AGAIN,
	XNET_RESULT_CLOSED,
	XNET_RESULT_TRUNCATED,
	XNET_RESULT_TIMEOUT,
	XNET_RESULT_CANCELLED
} xnetresult;



/* 网络基础层稳定错误代码。 */
typedef enum xneterror {
	XNET_ERROR_NONE = 0,
	XNET_ERROR_FORMAT,
	XNET_ERROR_FAMILY,
	XNET_ERROR_PORT,
	XNET_ERROR_SCOPE,
	XNET_ERROR_BUFFER,
	XNET_ERROR_NATIVE,
	XNET_ERROR_SYSTEM,
	XNET_ERROR_INTERFACE_QUERY,
	XNET_ERROR_INTERFACE_NAME,
	XNET_ERROR_INTERFACE_INDEX,
	XNET_ERROR_INTERFACE_ADDRESS,
	XNET_ERROR_INTERFACE_HARDWARE,
	XNET_ERROR_HOST_NAME,
	XNET_ERROR_DNS_RESOLVE,
	XNET_ERROR_DNS_REVERSE,
	XNET_ERROR_DNS_RESULT,
	XNET_ERROR_RESOLVER_CREATE,
	XNET_ERROR_RESOLVER_SUBMIT,
	XNET_ERROR_RESOLVER_CLOSED,
	XNET_ERROR_RESOLVER_QUERY,
	XNET_ERROR_BUFFER_STATE,
	XNET_ERROR_POOL_BUSY,
	XNET_ERROR_FRAME_CONFIG,
	XNET_ERROR_FRAME_STATE,
	XNET_ERROR_FRAME_LIMIT,
	XNET_ERROR_FRAME_LENGTH,
	XNET_ERROR_SOCKET_OPEN,
	XNET_ERROR_SOCKET_CLOSE,
	XNET_ERROR_SOCKET_OPTION,
	XNET_ERROR_SOCKET_BIND,
	XNET_ERROR_SOCKET_LISTEN,
	XNET_ERROR_SOCKET_ACCEPT,
	XNET_ERROR_SOCKET_CONNECT,
	XNET_ERROR_SOCKET_SHUTDOWN,
	XNET_ERROR_SOCKET_READ,
	XNET_ERROR_SOCKET_WRITE,
	XNET_ERROR_SOCKET_DGRAM_ERROR,
	XNET_ERROR_PORT_CREATE,
	XNET_ERROR_PORT_CLOSE,
	XNET_ERROR_PORT_WATCH,
	XNET_ERROR_PORT_WAIT,
	XNET_ERROR_PORT_POST,
	XNET_ERROR_PORT_SUBMIT,
	XNET_ERROR_PORT_CANCEL,
	XNET_ERROR_ENGINE_CREATE,
	XNET_ERROR_ENGINE_START,
	XNET_ERROR_ENGINE_STOP,
	XNET_ERROR_ENGINE_POST,
	XNET_ERROR_ENGINE_TIMER,
	XNET_ERROR_STREAM_CONFIG,
	XNET_ERROR_STREAM_CREATE,
	XNET_ERROR_STREAM_CONNECT,
	XNET_ERROR_STREAM_READ,
	XNET_ERROR_STREAM_WRITE,
	XNET_ERROR_STREAM_CLOSE,
	XNET_ERROR_DIAL_CONFIG,
	XNET_ERROR_DIAL_CREATE,
	XNET_ERROR_DIAL_RESOLVE,
	XNET_ERROR_DIAL_CONNECT,
	XNET_ERROR_LISTENER_CREATE,
	XNET_ERROR_LISTENER_ACCEPT,
	XNET_ERROR_LISTENER_CLOSE,
	XNET_ERROR_SERVER_CONFIG,
	XNET_ERROR_SERVER_START,
	XNET_ERROR_SERVER_ACCEPT,
	XNET_ERROR_UDP_CONFIG,
	XNET_ERROR_UDP_CREATE,
	XNET_ERROR_UDP_RECEIVE,
	XNET_ERROR_UDP_RECEIVE_QUEUE,
	XNET_ERROR_UDP_SEND,
	XNET_ERROR_UDP_CLOSE,
	XNET_ERROR_PROXY_CONFIG,
	XNET_ERROR_PROXY_CREATE,
	XNET_ERROR_PROXY_PROTOCOL,
	XNET_ERROR_PROXY_AUTH,
	XNET_ERROR_PROXY_CONNECT,
	XNET_ERROR_PROXY_LIMIT,
	XNET_ERROR_PROXY_UNSUPPORTED
} xneterror;



/* 只读 Span 借用调用方内存，不拥有数据。 */
typedef struct xnetspan {
	cbytes Data;
	size_t Size;
} xnetspan;



/* 可写 Span 借用调用方内存，不拥有数据。 */
typedef struct xnetwspan {
	bytes Data;
	size_t Size;
} xnetwspan;



#if defined(XRT_FEATURE_NET_DNS)

/* 地址列表是不可变共享结果，解析器缓存和调用方可以独立持有引用。 */
typedef struct xnetaddrlist xnetaddrlist;

#endif



#if defined(XRT_FEATURE_NET_RESOLVER)

/* Resolver 与解析操作均保持不透明，解析操作可以独立于调用方引用继续执行。 */
typedef struct xnetresolver xnetresolver;
typedef struct xnetresolveop xnetresolveop;



/* 解析操作只允许从等待态进入运行态，再进入一个不可变终态。 */
typedef enum xnetresolveopstate {
	XNET_RESOLVE_PENDING = 0,
	XNET_RESOLVE_RUNNING,
	XNET_RESOLVE_RESOLVED,
	XNET_RESOLVE_FAILED,
	XNET_RESOLVE_CANCELLED
} xnetresolveopstate;



/* 自定义查询过程必须返回端口为零的不可变地址列表，并在失败时设置结构化错误。 */
typedef xnetaddrlist* (*xnetresolverlookup)(
	cstr sHost,
	xnetfamily Family,
	ptr pData
);



/* 完成回调借用操作对象；保留到回调之后时必须显式增加引用。 */
typedef void (*xnetresolveproc)(xnetresolveop* pOperation, ptr pData);



/* 所有限额都是硬边界；TTL 使用单调微秒，零值关闭对应缓存。 */
typedef struct xnetresolverconfig {
	uint32 Workers;
	size_t RequestLimit;
	size_t QueryLimit;
	size_t CacheEntries;
	uint64 SuccessTTL;
	uint64 FailureTTL;
	size_t HostLimit;
	size_t ThreadStack;
	xnetresolverlookup Lookup;
	ptr LookupData;
} xnetresolverconfig;



/* 统计值在 Resolver 锁内取得一致快照，不要求调用方停止提交。 */
typedef struct xnetresolverstats {
	uint32 Workers;
	uint64 Submitted;
	uint64 Rejected;
	uint64 CacheHits;
	uint64 CacheMisses;
	uint64 Coalesced;
	uint64 QueriesStarted;
	uint64 Resolved;
	uint64 Failed;
	uint64 Cancelled;
	size_t Outstanding;
	size_t ActiveQueries;
	size_t QueuedQueries;
	size_t RunningQueries;
	size_t ReadyCallbacks;
	size_t CachedResults;
} xnetresolverstats;

#endif



#if defined(XRT_FEATURE_NET_SOCKET)

#define XNET_DGRAM_BATCH_MAX 64u



/* Socket 对象拥有一个原生句柄，生命周期操作由调用方串行化。 */
typedef struct xnetsocket_impl* xnetsocket;



/* Socket 类型使用稳定值，不直接暴露平台 SOCK_* 常量。 */
typedef enum xnetsockettype {
	XNET_SOCKET_STREAM = 1,
	XNET_SOCKET_DGRAM = 2
} xnetsockettype;



/* 数据报元数据位既用于能力、启用配置，也用于标记每个报文的有效字段。 */
typedef enum xnetdgrammetaflag {
	XNET_DGRAM_META_DESTINATION = 0x0001,
	XNET_DGRAM_META_INTERFACE = 0x0002,
	XNET_DGRAM_META_HOP_LIMIT = 0x0004,
	XNET_DGRAM_META_TRAFFIC_CLASS = 0x0008,
	/* 合并接收时表示每个原始数据报的分段大小，最后一段允许更短。 */
	XNET_DGRAM_META_SEGMENT_SIZE = 0x0010,
	/* 控制缓冲被平台截断时保留已解析字段，并显式标记结果不完整。 */
	XNET_DGRAM_META_TRUNCATED = 0x40000000
} xnetdgrammetaflag;



/* Destination 的端口恒为零；Flags 决定其余字段是否有效。 */
typedef struct xnetdgrammeta {
	uint32 Flags;
	xnetaddr Destination;
	uint32 Interface;
	int HopLimit;
	int TrafficClass;
	uint32 SegmentSize;
} xnetdgrammeta;



/* 逐数据报发送控制位与接收元数据分离，避免 Source 和 Destination 语义混淆。 */
typedef enum xnetdgramcontrolflag {
	XNET_DGRAM_CONTROL_SOURCE = 0x0001,
	XNET_DGRAM_CONTROL_INTERFACE = 0x0002,
	XNET_DGRAM_CONTROL_HOP_LIMIT = 0x0004,
	XNET_DGRAM_CONTROL_TRAFFIC_CLASS = 0x0008,
	/* 把一个聚合负载分段发送；最后一个数据报允许短于 SegmentSize。 */
	XNET_DGRAM_CONTROL_SEGMENT_SIZE = 0x0010
} xnetdgramcontrolflag;



/* Source 的端口必须为零；Flags 决定本次发送覆盖哪些 Socket 默认值。 */
typedef struct xnetdgramcontrol {
	uint32 Flags;
	xnetaddr Source;
	uint32 Interface;
	int HopLimit;
	int TrafficClass;
	uint32 SegmentSize;
} xnetdgramcontrol;



/* 数据报高级能力按当前平台和 Socket 类型查询，不能用编译平台作运行时假设。 */
typedef enum xnetdgramcap {
	XNET_DGRAM_CAP_PATH_MTU_MODE = 0x0001,
	XNET_DGRAM_CAP_PATH_MTU_QUERY = 0x0002,
	XNET_DGRAM_CAP_ERROR_QUEUE = 0x0004,
	XNET_DGRAM_CAP_SEGMENT_SEND = 0x0008,
	XNET_DGRAM_CAP_SEGMENT_RECEIVE = 0x0010
} xnetdgramcap;



/* SYSTEM 保留平台默认；DISCOVER 禁止 IP 分片；FRAGMENT 允许分片；PROBE 忽略路径缓存。 */
typedef enum xnetpmtumode {
	XNET_PMTU_SYSTEM = 0,
	XNET_PMTU_DISCOVER,
	XNET_PMTU_FRAGMENT,
	XNET_PMTU_PROBE
} xnetpmtumode;



/* 错误来源独立于 ICMP Type/Code，LOCAL 也可以携带路径 MTU。 */
typedef enum xnetdgramerrororigin {
	XNET_DGRAM_ERROR_UNKNOWN = 0,
	XNET_DGRAM_ERROR_LOCAL,
	XNET_DGRAM_ERROR_ICMP,
	XNET_DGRAM_ERROR_ICMP6
} xnetdgramerrororigin;



/* Flags 精确说明地址、路径 MTU 与负载截断字段是否有效。 */
typedef enum xnetdgramerrorflag {
	XNET_DGRAM_ERROR_REMOTE = 0x0001,
	XNET_DGRAM_ERROR_OFFENDER = 0x0002,
	XNET_DGRAM_ERROR_PATH_MTU = 0x0004,
	XNET_DGRAM_ERROR_PAYLOAD_TRUNCATED = 0x0008,
	XNET_DGRAM_ERROR_META_TRUNCATED = 0x0010
} xnetdgramerrorflag;



/* 异步网络错误是可读取数据，不会污染当前执行上下文的错误对象。 */
typedef struct xnetdgramerror {
	uint32 Flags;
	xnetdgramerrororigin Origin;
	xerrkind Kind;
	int SystemCode;
	int Type;
	int Code;
	uint32 Info;
	uint32 Data;
	size_t PathMtu;
	xnetaddr Remote;
	xnetaddr Offender;
} xnetdgramerror;



/* 批量接收项由调用方提供缓冲，函数写入来源、可见长度和单报文结果。 */
typedef struct xnetdgramrecv {
	void* Data;
	size_t Capacity;
	xnetaddr Remote;
	xnetdgrammeta Meta;
	size_t Size;
	xnetresult Result;
} xnetdgramrecv;



/* 批量发送项在调用期间借用数据；空远端表示使用连接式 UDP 的固定 Peer。 */
typedef struct xnetdgramsend {
	const xnetaddr* Remote;
	const void* Data;
	size_t Size;
} xnetdgramsend;



/* Socket 打开标志可以组合；系统句柄始终禁止被子进程继承。 */
typedef enum xnetsocketflag {
	XNET_SOCKET_NONBLOCK = 0x01
} xnetsocketflag;



/* 半关闭方向与平台 SHUT_* 常量保持隔离。 */
typedef enum xnetshutdown {
	XNET_SHUTDOWN_READ = 1,
	XNET_SHUTDOWN_WRITE,
	XNET_SHUTDOWN_BOTH
} xnetshutdown;



/* 通用 Socket 选项以 int64 表达；负 linger 值表示关闭 linger。 */
typedef enum xnetoption {
	XNET_OPTION_NONBLOCK = 1,
	XNET_OPTION_REUSE_ADDRESS,
	XNET_OPTION_REUSE_PORT,
	XNET_OPTION_EXCLUSIVE_ADDRESS,
	XNET_OPTION_NO_DELAY,
	XNET_OPTION_KEEP_ALIVE,
	XNET_OPTION_BROADCAST,
	XNET_OPTION_IPV6_ONLY,
	XNET_OPTION_RECEIVE_BUFFER,
	XNET_OPTION_SEND_BUFFER,
	XNET_OPTION_LINGER,
	XNET_OPTION_HOP_LIMIT,
	XNET_OPTION_TRAFFIC_CLASS,
	XNET_OPTION_PATH_MTU_MODE,
	XNET_OPTION_PATH_MTU,
	XNET_OPTION_DGRAM_ERRORS,
	XNET_OPTION_ERROR
} xnetoption;

#endif



#if defined(XRT_FEATURE_NET_PORT)

/* 网络端口隐藏平台事件对象、注册表和跨线程唤醒资源。 */
typedef struct xnetport_impl xnetport;



/* AUTO 选择当前平台最优后端，显式值用于测试、降级和部署控制。 */
typedef enum xnetportbackend {
	XNET_PORT_AUTO = 0,
	XNET_PORT_IOCP,
	XNET_PORT_URING,
	XNET_PORT_EPOLL,
	XNET_PORT_KQUEUE,
	XNET_PORT_SELECT
} xnetportbackend;



/* 能力位明确区分完成式 IO 与 readiness，避免后端伪造同一种语义。 */
typedef enum xnetportcap {
	XNET_PORT_CAP_READINESS = 0x0001,
	XNET_PORT_CAP_COMPLETION = 0x0002,
	XNET_PORT_CAP_ONESHOT = 0x0004,
	XNET_PORT_CAP_EDGE = 0x0008,
	XNET_PORT_CAP_BATCH = 0x0010,
	XNET_PORT_CAP_WAKE = 0x0020,
	XNET_PORT_CAP_POST = 0x0040,
	XNET_PORT_CAP_CANCEL = 0x0080,
	XNET_PORT_CAP_READ_PROBE = 0x0100,
	XNET_PORT_CAP_DGRAM_ERROR = 0x0200,
	XNET_PORT_CAP_SEND_FILE = 0x0400,
	XNET_PORT_CAP_FILE_IO = 0x0800
} xnetportcap;



/* readiness 关注位；错误与挂断始终隐式观察。 */
typedef enum xnetpoll {
	XNET_POLL_READ = 0x01,
	XNET_POLL_WRITE = 0x02
} xnetpoll;



/* 一个事件只属于一个稳定类别，具体 readiness 状态由 Flags 组合表达。 */
typedef enum xnetporteventtype {
	XNET_PORT_EVENT_READY = 1,
	XNET_PORT_EVENT_ACCEPT,
	XNET_PORT_EVENT_CONNECT,
	XNET_PORT_EVENT_READ_PROBE,
	XNET_PORT_EVENT_RECV,
	XNET_PORT_EVENT_SEND,
	XNET_PORT_EVENT_RECV_FROM,
	XNET_PORT_EVENT_RECV_MSG,
	XNET_PORT_EVENT_RECV_ERROR,
	XNET_PORT_EVENT_SEND_TO,
	XNET_PORT_EVENT_SEND_MSG,
	XNET_PORT_EVENT_SEND_FILE,
	XNET_PORT_EVENT_FILE_READ,
	XNET_PORT_EVENT_FILE_WRITE,
	XNET_PORT_EVENT_USER,
	XNET_PORT_EVENT_WAKE
} xnetporteventtype;



/* 事件标志表达 readiness 方向及完成式 EOF、错误等稳定状态。 */
typedef enum xnetporteventflag {
	XNET_PORT_EVENT_READ = 0x0001,
	XNET_PORT_EVENT_WRITE = 0x0002,
	XNET_PORT_EVENT_ERROR = 0x0004,
	XNET_PORT_EVENT_HANGUP = 0x0008,
	XNET_PORT_EVENT_EOF = 0x0010,
	XNET_PORT_EVENT_MORE = 0x0020
} xnetporteventflag;



/* 配置不带版本字段；OperationCache 是每个完成操作尺寸类的缓存上限，零值关闭缓存。 */
typedef struct xnetportconfig {
	xnetportbackend Backend;
	uint32 Flags;
	size_t PostLimit;
	/* Watch/Operation 为零时按实际后端选择可扩展默认值。 */
	size_t WatchLimit;
	size_t OperationLimit;
	size_t OperationCache;
} xnetportconfig;



/* 端口事件借用原 Socket；只有 Accepted 在接受完成时转移新对象所有权。 */
typedef struct xnetportevent {
	xnetporteventtype Type;
	uint32 Flags;
	xnetresult Result;
	int SystemCode;
	size_t Bytes;
	uint64 Id;
	xnetsocket Socket;
	xnetsocket Accepted;
	xnetaddr Address;
	xnetdgrammeta Meta;
	xnetdgramerror DgramError;
	ptr User;
} xnetportevent;

#endif



#if defined(XRT_FEATURE_NET_BUFFER)
typedef struct xnetbufpoolconfig xnetbufpoolconfig;
typedef struct xnetbufpool xnetbufpool;
typedef struct xnetbytes xnetbytes;
#endif



#if defined(XRT_FEATURE_NET_ENGINE)

/* Engine 与 Worker 对外保持不透明，所有 Worker 资源都归所属 Engine 管理。 */
typedef struct xnetengine xnetengine;
typedef struct xnetworker xnetworker;



/* Engine 生命周期状态可安全地跨线程查询。 */
typedef enum xnetenginestate {
	XNET_ENGINE_STOPPED = 0,
	XNET_ENGINE_STARTING,
	XNET_ENGINE_RUNNING,
	XNET_ENGINE_STOPPING,
	XNET_ENGINE_DESTROYING
} xnetenginestate;



/* Engine 任务始终在选定 Worker 上串行执行。 */
typedef void (*xnettaskproc)(xnetworker* pWorker, ptr pData);



/* 嵌入式 Post 提供不分配内存且不占公开命令队列容量的 Worker 投递。 */
#define XNET_POST_STORAGE_SIZE 64u



/* Post 的队列节点与并发门保持不透明，允许嵌入网络对象。 */
typedef union xnetpost {
	uint64 Alignment;
	uint8 Storage[XNET_POST_STORAGE_SIZE];
} xnetpost;



/* Timer 受理后恰好终结一次，结果区分到期、取消、停止和内部失败。 */
typedef void (*xnettimerproc)(xnetworker* pWorker,
	uint64 Id, xnetresult Result, ptr pData);



/* Engine 内的端口事件通过 Completion 回到所属 Worker。 */
typedef void (*xnetcompletionproc)(xnetworker* pWorker,
	const xnetportevent* pEvent, ptr pData);



/* Completion 借用过程与数据，必须存活到对应端口事件回调结束。 */
typedef struct xnetcompletion {
	xnetcompletionproc Proc;
	ptr Data;
} xnetcompletion;



/* 容量字段都是硬边界；CommandCapacity 会向上取整为 2 次幂。 */
typedef struct xnetengineconfig {
	xnetportbackend Backend;
	uint32 Workers;
	const xnetbufpoolconfig* BufferPool;
	size_t CommandCapacity;
	size_t NodeCacheBytes;
	size_t TimerLimit;
	size_t EventBatch;
	size_t PortPostLimit;
	/* 两个端口容量为零时由每个 Worker 的实际后端解析。 */
	size_t PortWatchLimit;
	size_t PortOperationLimit;
	size_t PortOperationCache;
	uint64 IdleWait;
	size_t ThreadStack;
} xnetengineconfig;



/* Worker 统计是并发快照，计数在 Engine 重启后继续累计。 */
typedef struct xnetworkerstats {
	uint64 PostsAccepted;
	uint64 PostsRejected;
	uint64 PostsExecuted;
	uint64 TimersAccepted;
	uint64 TimersRejected;
	uint64 TimersFired;
	uint64 TimersCancelled;
	uint64 TimersClosed;
	uint64 TimerErrors;
	uint64 Events;
	uint64 WaitErrors;
	/* 最近一次端口等待错误；没有错误时 Code 为 XNET_ERROR_NONE。 */
	xneterror LastWaitError;
	int LastWaitSystemCode;
	uint64 NodeCacheHits;
	uint64 NodeCacheMisses;
	size_t PendingCommands;
	size_t ActiveTimers;
	size_t NodeCachedBytes;
} xnetworkerstats;



/* Engine 统计聚合全部 Worker，并附带当前生命周期状态。 */
typedef struct xnetenginestats {
	xnetenginestate State;
	uint32 Workers;
	uint64 PostsAccepted;
	uint64 PostsRejected;
	uint64 PostsExecuted;
	uint64 TimersAccepted;
	uint64 TimersRejected;
	uint64 TimersFired;
	uint64 TimersCancelled;
	uint64 TimersClosed;
	uint64 TimerErrors;
	uint64 Events;
	uint64 WaitErrors;
	uint64 NodeCacheHits;
	uint64 NodeCacheMisses;
	size_t PendingCommands;
	size_t ActiveTimers;
	size_t NodeCachedBytes;
	size_t LiveObjects;
} xnetenginestats;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_NET)

/* 初始化一个指定地址族的未指定地址。 */
XRT_API bool xrtNetAddrAny(xnetaddr* pAddr, xnetfamily Family, uint16 iPort);



/* 初始化一个指定地址族的回环地址。 */
XRT_API bool xrtNetAddrLoopback(xnetaddr* pAddr, xnetfamily Family, uint16 iPort);



/* 严格解析数字 IPv4 或 IPv6；接口模块启用时 IPv6 Scope 也接受接口名称。 */
XRT_API bool xrtNetAddrParse(xnetaddr* pAddr, cstr sIP, uint16 iPort);



/* 解析 IPv4:port、[IPv6]:port 或使用默认端口的裸地址。 */
XRT_API bool xrtNetAddrParseEndpoint(xnetaddr* pAddr, cstr sEndpoint, uint16 iDefaultPort);



/* 输出规范 IP 文本，返回不含结尾零字节的所需长度。 */
XRT_API size_t xrtNetAddrText(const xnetaddr* pAddr, char* sText, size_t iCapacity);



/* 输出带端口的规范端点文本，IPv6 始终使用方括号。 */
XRT_API size_t xrtNetAddrEndpointText(const xnetaddr* pAddr, char* sText, size_t iCapacity);



/* 分配并返回规范 IP 文本。 */
XRT_API str xrtNetAddrString(const xnetaddr* pAddr);



/* 分配并返回带端口的规范端点文本。 */
XRT_API str xrtNetAddrEndpointString(const xnetaddr* pAddr);



/* 比较完整端点，包括地址族、地址、Scope 和端口。 */
XRT_API bool xrtNetAddrEqual(const xnetaddr* pLeft, const xnetaddr* pRight);



/* 只比较地址族、地址和 IPv6 Scope，不比较端口。 */
XRT_API bool xrtNetAddrSameIP(const xnetaddr* pLeft, const xnetaddr* pRight);



/* 为 Map、排序和稳定去重提供完整端点全序。 */
XRT_API int xrtNetAddrCompare(const xnetaddr* pLeft, const xnetaddr* pRight);



/* 判断地址是否为 IPv4 0.0.0.0 或 IPv6 ::。 */
XRT_API bool xrtNetAddrIsUnspecified(const xnetaddr* pAddr);



/* 判断地址是否属于 IPv4 127/8 或 IPv6 ::1。 */
XRT_API bool xrtNetAddrIsLoopback(const xnetaddr* pAddr);



/* 判断地址是否属于 IPv4 224/4 或 IPv6 ff00::/8。 */
XRT_API bool xrtNetAddrIsMulticast(const xnetaddr* pAddr);



/* 判断地址是否属于 IPv4 169.254/16 或 IPv6 fe80::/10。 */
XRT_API bool xrtNetAddrIsLinkLocal(const xnetaddr* pAddr);



/* 判断地址是否属于 RFC 1918 IPv4 或 RFC 4193 IPv6 私有范围。 */
XRT_API bool xrtNetAddrIsPrivate(const xnetaddr* pAddr);



/* 判断 IPv6 地址是否为 ::ffff:0:0/96 IPv4 映射地址。 */
XRT_API bool xrtNetAddrIsMapped(const xnetaddr* pAddr);



/* 把 IPv4 映射 IPv6 地址转换为 IPv4，其他地址原样复制。 */
XRT_API bool xrtNetAddrUnmap(const xnetaddr* pAddr, xnetaddr* pResult);



/* 转换为平台 sockaddr；空输出可查询所需大小。 */
XRT_API bool xrtNetAddrToNative(const xnetaddr* pAddr, void* pNative, size_t* pSize);



/* 从平台 sockaddr 转换为稳定地址结构。 */
XRT_API bool xrtNetAddrFromNative(xnetaddr* pAddr, const void* pNative, size_t iSize);

#endif



#if defined(XRT_FEATURE_NET_DNS)

/* 解析主机的全部地址；保留系统顺序、去除重复项并允许端口为零。 */
/* 只查询主机地址，返回列表中的端口全部为零。 */
XRT_API xnetaddrlist* xrtNetLookup(cstr sHost, xnetfamily Family);



XRT_API xnetaddrlist* xrtNetResolve(
	cstr sHost,
	uint16 iPort,
	xnetfamily Family
);



/* 解析并复制系统顺序中的第一个地址，常见单地址场景无需管理列表。 */
XRT_API bool xrtNetResolveOne(
	xnetaddr* pAddr,
	cstr sHost,
	uint16 iPort,
	xnetfamily Family
);



/* 反向解析一个数字地址；成功返回调用方拥有的主机名。 */
XRT_API str xrtNetReverse(const xnetaddr* pAddr);



/* 复制、校验并去重调用方地址，建立一份不可变地址列表。 */
XRT_API xnetaddrlist* xrtNetAddrListCreate(
	const xnetaddr* pAddresses,
	size_t iCount
);



/* 复制列表并统一替换端口；端口已经一致时只增加引用。 */
XRT_API xnetaddrlist* xrtNetAddrListWithPort(
	xnetaddrlist* pList,
	uint16 iPort
);



/* 增加不可变地址列表引用并返回原指针。 */
XRT_API xnetaddrlist* xrtNetAddrListRef(xnetaddrlist* pList);



/* 释放地址列表引用；空指针视为空操作。 */
XRT_API void xrtNetAddrListDestroy(xnetaddrlist* pList);



/* 返回地址数量；空列表返回零。 */
XRT_API size_t xrtNetAddrListCount(const xnetaddrlist* pList);



/* 返回借用地址；索引越界返回空指针并设置范围错误。 */
XRT_API const xnetaddr* xrtNetAddrListGet(
	const xnetaddrlist* pList,
	size_t iIndex
);

#endif



#if defined(XRT_FEATURE_NET_RESOLVER)

/* 写入兼顾桌面与高并发服务的默认配置。 */
XRT_API void xrtNetResolverConfigInit(xnetresolverconfig* pConfig);



/* 创建并立即启动独立解析工作池；空配置使用默认值。 */
XRT_API xnetresolver* xrtNetResolverCreate(
	const xnetresolverconfig* pConfig
);



/* 排空已受理请求并等待全部回调；必须与其他 Resolver 所有者操作串行，返回后指针失效。 */
XRT_API bool xrtNetResolverDestroy(xnetresolver* pResolver);



/* 提交主机查询；同一规范化主机与地址族只执行一次底层查询。 */
XRT_API xnetresolveop* xrtNetResolverResolve(
	xnetresolver* pResolver,
	cstr sHost,
	xnetfamily Family,
	xnetresolveproc pDone,
	ptr pData
);



/* 清空成功和失败缓存；已经运行或排队的查询不受影响。 */
XRT_API bool xrtNetResolverClear(xnetresolver* pResolver);



/* 取得 Resolver 的并发一致统计快照。 */
XRT_API bool xrtNetResolverStats(
	const xnetresolver* pResolver,
	xnetresolverstats* pStats
);



/* 增加解析操作引用并返回原指针。 */
XRT_API xnetresolveop* xrtNetResolveOpRef(xnetresolveop* pOperation);



/* 释放解析操作引用；空指针视为空操作。 */
XRT_API void xrtNetResolveOpDestroy(xnetresolveop* pOperation);



/* 协作取消尚未进入终态的操作；回调仍在 Resolver Worker 上执行一次。 */
XRT_API bool xrtNetResolveOpCancel(xnetresolveop* pOperation);



/* 返回解析操作当前状态的原子快照。 */
XRT_API xnetresolveopstate xrtNetResolveOpState(
	const xnetresolveop* pOperation
);



/* 成功时返回增加引用的完整地址列表，其他状态返回空指针并设置对应错误。 */
XRT_API xnetaddrlist* xrtNetResolveOpResult(
	const xnetresolveop* pOperation
);



/* 失败或取消时返回借用的结构化错误，其他状态返回空指针。 */
XRT_API const xerror* xrtNetResolveOpError(
	const xnetresolveop* pOperation
);

#endif



#if defined(XRT_FEATURE_NET_RESOLVER_FUTURE)

/* 把 Resolver 查询包装为 Future；成功值是由 Future 持有的地址列表。 */
XRT_API xfuture* xrtNetResolveAsync(
	xnetresolver* pResolver,
	cstr sHost,
	xnetfamily Family
);

#endif



#if defined(XRT_FEATURE_NET_SOCKET)

/* 打开一个流式或数据报 Socket；成功返回的对象拥有原生句柄。 */
XRT_API xnetsocket xrtNetSocketOpen(xnetfamily Family,
	xnetsockettype Type, uint32 iFlags);



/* 关闭原生句柄并销毁对象；即使系统关闭失败，对象也立即失效。 */
XRT_API bool xrtNetSocketClose(xnetsocket Socket);



/* 返回借用的原生句柄，调用方不得自行关闭。 */
XRT_API intptr_t xrtNetSocketNative(xnetsocket Socket);



/* 返回 Socket 创建时确定的地址族。 */
XRT_API xnetfamily xrtNetSocketFamily(xnetsocket Socket);



/* 返回 Socket 创建时确定的类型。 */
XRT_API xnetsockettype xrtNetSocketType(xnetsocket Socket);



/* 查询当前可立即读取的字节数；成功才修改输出。 */
XRT_API bool xrtNetSocketAvailable(xnetsocket Socket, size_t* pSize);



/* 设置一个通用 Socket 选项。 */
XRT_API bool xrtNetSocketSet(xnetsocket Socket,
	xnetoption Option, int64 iValue);



/* 查询一个通用 Socket 选项，成功才修改输出。 */
XRT_API bool xrtNetSocketGet(xnetsocket Socket,
	xnetoption Option, int64* pValue);



/* 返回当前平台和地址族可能提供的数据报接收元数据位。 */
XRT_API uint32 xrtNetSocketDgramMetaAvailable(xnetsocket Socket);



/* 返回 Socket 当前已经启用的数据报接收元数据位。 */
XRT_API uint32 xrtNetSocketDgramMetaEnabled(xnetsocket Socket);



/* 成功后精确设置接收元数据位；失败后可查询平台已经生效的实际状态。 */
XRT_API bool xrtNetSocketDgramMetaSet(xnetsocket Socket, uint32 iFlags);



/* 返回当前平台、地址族和 Socket Provider 可用的逐数据报发送控制位。 */
XRT_API uint32 xrtNetSocketDgramControlAvailable(xnetsocket Socket);



/* 返回 PMTU、错误队列及后续高级数据报能力；非数据报 Socket 返回零。 */
XRT_API uint32 xrtNetSocketDgramCapabilities(xnetsocket Socket);



/* 将数据报 Socket 加入一个同地址族多播组；IPv6 接口使用 Scope 作为接口索引。 */
XRT_API bool xrtNetSocketMulticastJoin(xnetsocket Socket,
	const xnetaddr* pGroup, const xnetaddr* pInterface);



/* 将数据报 Socket 移出一个同地址族多播组。 */
XRT_API bool xrtNetSocketMulticastLeave(xnetsocket Socket,
	const xnetaddr* pGroup, const xnetaddr* pInterface);



/* 设置数据报 Socket 是否接收自己发出的多播报文。 */
XRT_API bool xrtNetSocketMulticastLoop(xnetsocket Socket, bool bEnabled);



/* 设置数据报 Socket 的多播跳数，合法范围为 0 到 255。 */
XRT_API bool xrtNetSocketMulticastHopLimit(xnetsocket Socket, int iHopLimit);



/* 选择多播发送接口；空接口恢复系统默认，IPv6 使用 Scope 接口索引。 */
XRT_API bool xrtNetSocketMulticastInterface(xnetsocket Socket,
	const xnetaddr* pInterface);



/* 把 Socket 绑定到本地地址，端口为零时由系统分配端口。 */
XRT_API bool xrtNetSocketBind(xnetsocket Socket, const xnetaddr* pAddress);



/* 把已绑定的流式 Socket 转为监听状态。 */
XRT_API bool xrtNetSocketListen(xnetsocket Socket, int iBacklog);



/* 接受一个连接；非阻塞 Socket 暂无连接时返回 AGAIN。 */
XRT_API xnetresult xrtNetSocketAccept(xnetsocket Socket,
	xnetsocket* pClient, xnetaddr* pRemote);



/* 发起连接；非阻塞连接尚未完成时返回 AGAIN。 */
XRT_API xnetresult xrtNetSocketConnect(xnetsocket Socket,
	const xnetaddr* pRemote);



/* 在可写事件到达后读取 SO_ERROR，完成非阻塞连接判定。 */
XRT_API xnetresult xrtNetSocketFinishConnect(xnetsocket Socket);



/* 半关闭指定方向，不销毁 Socket 对象。 */
XRT_API bool xrtNetSocketShutdown(xnetsocket Socket, xnetshutdown Direction);



/* 查询实际本地地址，成功才修改输出。 */
XRT_API bool xrtNetSocketLocal(xnetsocket Socket, xnetaddr* pAddress);



/* 查询已连接的对端地址，成功才修改输出。 */
XRT_API bool xrtNetSocketRemote(xnetsocket Socket, xnetaddr* pAddress);



/* 单次接收；流式 EOF 返回 CLOSED，数据报过长返回 TRUNCATED。 */
XRT_API xnetresult xrtNetSocketRecv(xnetsocket Socket,
	void* pData, size_t iSize, size_t* pReceived);



/* 单次发送；允许成功短写，非阻塞无法推进时返回 AGAIN。 */
XRT_API xnetresult xrtNetSocketSend(xnetsocket Socket,
	const void* pData, size_t iSize, size_t* pSent);



/* 单次分散接收；数据报过长返回 TRUNCATED，Span 数量不能超过 64。 */
XRT_API xnetresult xrtNetSocketRecvVec(xnetsocket Socket,
	xnetwspan* pSpans, size_t iCount, size_t* pReceived);



/* 单次聚集发送，Span 数量不能超过 64。 */
XRT_API xnetresult xrtNetSocketSendVec(xnetsocket Socket,
	const xnetspan* pSpans, size_t iCount, size_t* pSent);



/* 单次接收数据报；零长度返回 OK，缓冲不足返回 TRUNCATED。 */
XRT_API xnetresult xrtNetSocketRecvFrom(xnetsocket Socket,
	void* pData, size_t iSize, size_t* pReceived, xnetaddr* pRemote);



/* 单次发送数据报；允许发送零长度数据报。 */
XRT_API xnetresult xrtNetSocketSendTo(xnetsocket Socket,
	const void* pData, size_t iSize, size_t* pSent, const xnetaddr* pRemote);



/* 单次分散接收数据报；缓冲不足返回 TRUNCATED，Span 数量不能超过 64。 */
XRT_API xnetresult xrtNetSocketRecvFromVec(xnetsocket Socket,
	xnetwspan* pSpans, size_t iCount, size_t* pReceived, xnetaddr* pRemote);



/* 接收数据报及已启用的目标、接口、Hop Limit 和 Traffic Class 元数据。 */
XRT_API xnetresult xrtNetSocketRecvMsg(xnetsocket Socket,
	void* pData, size_t iSize, size_t* pReceived,
	xnetaddr* pRemote, xnetdgrammeta* pMeta);



/* 分散接收数据报及已启用元数据，Span 数量不能超过 64。 */
XRT_API xnetresult xrtNetSocketRecvMsgVec(xnetsocket Socket,
	xnetwspan* pSpans, size_t iCount, size_t* pReceived,
	xnetaddr* pRemote, xnetdgrammeta* pMeta);



/* 非阻塞读取一个已启用的异步数据报错误；空队列返回 AGAIN。 */
XRT_API xnetresult xrtNetSocketDgramRecvError(xnetsocket Socket,
	void* pData, size_t iSize, size_t* pReceived,
	xnetdgramerror* pError);



/* 单次聚集发送数据报，Span 数量不能超过 64。 */
XRT_API xnetresult xrtNetSocketSendToVec(xnetsocket Socket,
	const xnetspan* pSpans, size_t iCount, size_t* pSent,
	const xnetaddr* pRemote);



/* 发送数据报并覆盖本包源地址、接口、Hop Limit 或 Traffic Class。 */
XRT_API xnetresult xrtNetSocketSendMsg(xnetsocket Socket,
	const void* pData, size_t iSize, size_t* pSent,
	const xnetaddr* pRemote, const xnetdgramcontrol* pControl);



/* 聚集发送带逐包控制的数据报；Span 数量不能超过 64。 */
XRT_API xnetresult xrtNetSocketSendMsgVec(xnetsocket Socket,
	const xnetspan* pSpans, size_t iCount, size_t* pSent,
	const xnetaddr* pRemote, const xnetdgramcontrol* pControl);



/* 接收最多 64 个数据报；返回已消费前缀，每项独立记录 OK 或 TRUNCATED。 */
XRT_API xnetresult xrtNetSocketRecvBatch(xnetsocket Socket,
	xnetdgramrecv* pItems, size_t iCapacity, size_t* pReceived);



/* 发送最多 64 个数据报；返回已经完整发送的输入前缀。 */
XRT_API xnetresult xrtNetSocketSendBatch(xnetsocket Socket,
	const xnetdgramsend* pItems, size_t iCount, size_t* pSent);

#endif



#if defined(XRT_FEATURE_NET_PORT)

/* 初始化网络端口默认配置。 */
XRT_API void xrtNetPortConfigInit(xnetportconfig* pConfig);



/* 创建事件端口；AUTO 在当前已编译后端中选择最高能力实现。 */
XRT_API xnetport* xrtNetPortCreate(const xnetportconfig* pConfig);



/* 取消并排空在途 IO，再销毁端口、观察、用户事件及唤醒资源。 */
XRT_API bool xrtNetPortDestroy(xnetport* pPort);



/* 返回实际启用的后端。 */
XRT_API xnetportbackend xrtNetPortBackend(const xnetport* pPort);



/* 返回已经解析 AUTO 容量和实际后端的有效配置。 */
XRT_API bool xrtNetPortGetConfig(
	const xnetport* pPort,
	xnetportconfig* pConfig
);



/* 返回静态后端名称。 */
XRT_API cstr xrtNetPortName(const xnetport* pPort);



/* 返回实际后端能力位。 */
XRT_API uint32 xrtNetPortCapabilities(const xnetport* pPort);



/* 替换一个 Socket 的 readiness 关注位；事件为零等价于 Unwatch。 */
XRT_API bool xrtNetPortWatch(xnetport* pPort, xnetsocket Socket,
	uint64 Id, uint32 iEvents, ptr pUser);



/* 幂等移除观察；失败也会退休用户身份，调用方随后必须关闭 Socket。 */
XRT_API bool xrtNetPortUnwatch(xnetport* pPort, xnetsocket Socket);



/* 异步接受一个连接；成功提交后 Accepted 由终态事件转移给调用方。 */
XRT_API bool xrtNetPortAccept(xnetport* pPort,
	xnetsocket Socket, uint64 Id, ptr pUser);



/* 异步连接远端地址；终态事件到达前 Socket 必须保持有效。 */
XRT_API bool xrtNetPortConnect(xnetport* pPort, xnetsocket Socket,
	const xnetaddr* pRemote, uint64 Id, ptr pUser);



/* 异步等待流 Socket 可读；不借用数据缓冲，终态到达后再提交 Recv。 */
XRT_API bool xrtNetPortReadProbe(xnetport* pPort,
	xnetsocket Socket, uint64 Id, ptr pUser);



/* 异步接收到调用方缓冲；支持流和已连接数据报，单次最多 INT_MAX 字节。 */
XRT_API bool xrtNetPortRecv(xnetport* pPort, xnetsocket Socket,
	void* pData, size_t iSize, uint64 Id, ptr pUser);



/* 异步分散接收；支持流和已连接数据报，总长度最多 INT_MAX 字节。 */
XRT_API bool xrtNetPortRecvVec(xnetport* pPort, xnetsocket Socket,
	const xnetwspan* pSpans, size_t iCount, uint64 Id, ptr pUser);



/* 异步发送调用方缓冲；支持流和已连接数据报，单次最多 INT_MAX 字节。 */
XRT_API bool xrtNetPortSend(xnetport* pPort, xnetsocket Socket,
	const void* pData, size_t iSize, uint64 Id, ptr pUser);



/* 异步聚集发送；支持流和已连接数据报，总长度最多 INT_MAX 字节。 */
XRT_API bool xrtNetPortSendVec(xnetport* pPort, xnetsocket Socket,
	const xnetspan* pSpans, size_t iCount, uint64 Id, ptr pUser);



/*
	从原生文件句柄的绝对偏移异步读取到调用方缓冲。
	文件必须以 XFILE_ASYNC 打开，句柄和缓冲必须存活到唯一终态事件。
*/
/* 原生文件完成提交由 net_file 层绑定文件对象后提供。 */



/*
	把调用方缓冲异步写入原生文件句柄的绝对偏移。
	文件必须以 XFILE_ASYNC 打开，句柄和缓冲必须存活到唯一终态事件。
*/
/* 公共端口层不暴露无法表达 owner 的裸文件句柄接口。 */



/* 异步接收数据报；远端地址由终态事件返回。 */
XRT_API bool xrtNetPortRecvFrom(xnetport* pPort, xnetsocket Socket,
	void* pData, size_t iSize, uint64 Id, ptr pUser);



/* 异步分散接收数据报；缓冲不足由终态事件返回 TRUNCATED。 */
XRT_API bool xrtNetPortRecvFromVec(xnetport* pPort, xnetsocket Socket,
	const xnetwspan* pSpans, size_t iCount, uint64 Id, ptr pUser);



/* 异步接收数据报及 Socket 已启用的元数据。 */
XRT_API bool xrtNetPortRecvMsg(xnetport* pPort, xnetsocket Socket,
	void* pData, size_t iSize, uint64 Id, ptr pUser);



/* 异步分散接收数据报及元数据；终态事件同时返回 Address 和 Meta。 */
XRT_API bool xrtNetPortRecvMsgVec(xnetport* pPort, xnetsocket Socket,
	const xnetwspan* pSpans, size_t iCount, uint64 Id, ptr pUser);



/* 异步等待并读取一个数据报错误；终态同时返回原负载前缀和 DgramError。 */
XRT_API bool xrtNetPortRecvError(xnetport* pPort, xnetsocket Socket,
	void* pData, size_t iSize, uint64 Id, ptr pUser);



/* 异步发送数据报；远端地址在提交时复制。 */
XRT_API bool xrtNetPortSendTo(xnetport* pPort, xnetsocket Socket,
	const void* pData, size_t iSize, const xnetaddr* pRemote,
	uint64 Id, ptr pUser);



/* 异步聚集发送数据报；远端地址和 Span 描述符在提交时复制。 */
XRT_API bool xrtNetPortSendToVec(xnetport* pPort, xnetsocket Socket,
	const xnetspan* pSpans, size_t iCount, const xnetaddr* pRemote,
	uint64 Id, ptr pUser);



/* 异步发送带逐包控制的数据报；地址和控制值在提交时复制。 */
XRT_API bool xrtNetPortSendMsg(xnetport* pPort, xnetsocket Socket,
	const void* pData, size_t iSize, const xnetaddr* pRemote,
	const xnetdgramcontrol* pControl, uint64 Id, ptr pUser);



/* 异步聚集发送带逐包控制的数据报；终态类型为 SEND_MSG。 */
XRT_API bool xrtNetPortSendMsgVec(xnetport* pPort, xnetsocket Socket,
	const xnetspan* pSpans, size_t iCount, const xnetaddr* pRemote,
	const xnetdgramcontrol* pControl, uint64 Id, ptr pUser);



/* 请求取消指定 ID 的在途操作；操作仍以一个终态事件结束。 */
XRT_API bool xrtNetPortCancel(xnetport* pPort, uint64 Id);



/* 跨线程投递一个不会合并的用户事件。 */
XRT_API bool xrtNetPortPost(xnetport* pPort, uint64 Id, ptr pUser);



/* 跨线程请求一个可合并的 WAKE 事件。 */
XRT_API bool xrtNetPortWake(xnetport* pPort);



/* 等待到事件、截止时间或错误；成功和超时都会先清零输出数量。 */
XRT_API xnetresult xrtNetPortWait(xnetport* pPort,
	xnetportevent* pEvents, size_t iCapacity,
	xdeadline iDeadline, size_t* pCount);

#endif



#if defined(XRT_FEATURE_NET_ENGINE)

/* 初始化兼顾吞吐与内存占用的 Engine 默认配置。 */
XRT_API void xrtNetEngineConfigInit(xnetengineconfig* pConfig);



/* 初始化一个借用过程和数据的端口 Completion。 */
XRT_API void xrtNetCompletionInit(xnetcompletion* pCompletion,
	xnetcompletionproc pProc, ptr pData);



/* 创建停止状态的 Engine；Worker 线程和端口在 Start 时建立。 */
XRT_API xnetengine* xrtNetEngineCreate(const xnetengineconfig* pConfig);



/* 停止并销毁 Engine；仍有高层对象或外借池块时失败并保留对象。 */
XRT_API bool xrtNetEngineDestroy(xnetengine* pEngine);



/* 建立全部 Worker、端口和线程；已经运行时幂等成功。 */
XRT_API bool xrtNetEngineStart(xnetengine* pEngine);



/* 排空任务并释放运行资源；外借池块会使清理失败但允许重启后归还。 */
XRT_API bool xrtNetEngineStop(xnetengine* pEngine);



/* 返回当前生命周期状态。 */
XRT_API xnetenginestate xrtNetEngineState(const xnetengine* pEngine);



/* 返回 Engine 固定的 Worker 数量。 */
XRT_API uint32 xrtNetEngineWorkerCount(const xnetengine* pEngine);



/* 返回借用的指定 Worker；索引越界时返回空指针。 */
XRT_API xnetworker* xrtNetEngineWorker(xnetengine* pEngine, uint32 iIndex);



/* 返回当前线程所属的借用 Worker，不属于该 Engine 时返回空指针。 */
XRT_API xnetworker* xrtNetEngineCurrent(xnetengine* pEngine);



/* 返回 Worker 所属的借用 Engine。 */
XRT_API xnetengine* xrtNetWorkerEngine(const xnetworker* pWorker);



/* 返回 Worker 在所属 Engine 内的稳定索引。 */
XRT_API uint32 xrtNetWorkerIndex(const xnetworker* pWorker);



/* 判断调用线程是否正是指定 Worker。 */
XRT_API bool xrtNetWorkerIsCurrent(const xnetworker* pWorker);



/* 返回运行期间借用的端口；调用方必须保证借用操作先于 Stop 结束。 */
XRT_API xnetport* xrtNetWorkerPort(xnetworker* pWorker);



/* 返回 Worker 独占的自适应缓冲池；只能从该 Worker 的回调中调用。 */
XRT_API xnetbufpool* xrtNetWorkerBufPool(xnetworker* pWorker);



/* 从 Worker 的线程安全分级缓存分配并清零一块内存；调用方必须保持 Worker 生命周期有效。 */
XRT_API ptr xrtNetWorkerAlloc(xnetworker* pWorker, size_t iSize);



/* 把 Worker 分配的内存归还同一 Worker；空指针可以直接释放。 */
XRT_API void xrtNetWorkerFree(
	xnetworker* pWorker,
	ptr pMemory,
	size_t iSize
);



/* 分配 Engine 内唯一的非零端口操作 ID，可从任意线程调用。 */
XRT_API uint64 xrtNetWorkerOperationId(xnetworker* pWorker);



/* 初始化一个尚未投递的嵌入式 Post。 */
XRT_API bool xrtNetPostInit(xnetpost* pPost);



/* 判断嵌入式 Post 是否仍在 Worker 队列中等待执行。 */
XRT_API bool xrtNetPostPending(const xnetpost* pPost);



/*
	无分配地投递到指定 Worker；同一 Post 在出队前不能再次投递。
	调用方必须通过网络对象引用保证 Worker 和 Post 存活到回调返回。
*/
XRT_API bool xrtNetPost(
	xnetworker* pWorker,
	xnetpost* pPost,
	xnettaskproc pProc,
	ptr pData
);



/* 有界投递任务；成功受理后必在亲和 Worker 上执行一次。 */
XRT_API bool xrtNetEnginePost(xnetengine* pEngine,
	uint64 iAffinity, xnettaskproc pProc, ptr pData);



/* 按单调时钟截止时间调度 Timer；成功返回非零 ID。 */
XRT_API uint64 xrtNetEngineSchedule(xnetengine* pEngine,
	uint64 iAffinity, xdeadline iDeadline,
	xnettimerproc pProc, ptr pData);



/* 按相对微秒数调度 Timer；零表示在下一次 Worker 循环到期。 */
XRT_API uint64 xrtNetEngineAfter(xnetengine* pEngine,
	uint64 iAffinity, uint64 iTimeout,
	xnettimerproc pProc, ptr pData);



/* 异步请求取消 Timer；成功只表示请求已进入目标 Worker。 */
XRT_API bool xrtNetEngineTimerCancel(xnetengine* pEngine, uint64 Id);



/*
	只在 Timer 所属 Worker 上立即取消且不分配内存。
	不在所属 Worker、Timer 尚未入堆或已经终结时返回 false，且不修改线程错误。
*/
XRT_API bool xrtNetEngineTimerCancelCurrent(
	xnetengine* pEngine,
	uint64 Id
);



/* 读取一个 Worker 的统计快照。 */
XRT_API bool xrtNetWorkerStats(const xnetworker* pWorker,
	xnetworkerstats* pStats);



/* 聚合全部 Worker 的统计快照。 */
XRT_API bool xrtNetEngineStats(const xnetengine* pEngine,
	xnetenginestats* pStats);



/*
	占用一个正在运行的 Engine 生命周期，供组合网络对象保存借用指针。
	每次成功占用必须由一次 xrtNetEngineUnpin 配对释放。
*/
XRT_API bool xrtNetEnginePin(xnetengine* pEngine);



/* 释放一次 Engine 生命周期占用；没有匹配占用时返回状态错误。 */
XRT_API bool xrtNetEngineUnpin(xnetengine* pEngine);

#endif



XRT_EXTERN_C_END



#if defined(XRT_FEATURE_NET_BUFFER)

/* 网络缓冲池包含四个可配置尺寸类。 */
#define XNET_BUFFER_CLASS_COUNT 4u



/* 外部引用的释放过程在最后一段数据离开缓冲时执行一次。 */
typedef void (*xnetreleaseproc)(ptr pContext, cbytes pData, size_t iSize);



/* 引用 Span 在受理后转移释放责任，零长度 Span 不转移所有权。 */
typedef struct xnetref {
	cbytes Data;
	size_t Size;
	xnetreleaseproc Release;
	ptr Context;
} xnetref;



/* 每个尺寸类独立控制块容量和最大缓存数量，总缓存还受字节上限约束。 */
struct xnetbufpoolconfig {
	size_t BlockSize[XNET_BUFFER_CLASS_COUNT];
	size_t CacheLimit[XNET_BUFFER_CLASS_COUNT];
	size_t MaxCacheBytes;
};



/* 缓冲池统计区分实时、缓存、分配、复用和动态大块。 */
typedef struct xnetbufpoolinfo {
	size_t LiveBlocks;
	size_t LiveBytes;
	size_t PeakBlocks;
	size_t PeakBytes;
	size_t CachedBlocks;
	size_t CachedBytes;
	uint64 AllocCount;
	uint64 ReuseCount;
	uint64 DynamicCount;
	uint64 RefCount;
} xnetbufpoolinfo;



typedef struct xnetblock xnetblock;



/* 缓冲链可栈上使用；字段用于零分配查询，调用方不得直接修改。 */
typedef struct xnetbuf {
	xnetblock* Head;
	xnetblock* Tail;
	xnetblock* Reserved;
	xnetbufpool* Pool;
	size_t Size;
	size_t Blocks;
	bool ReservedNew;
} xnetbuf;



XRT_EXTERN_C_BEGIN



/* 增加拥有型网络字节结果的引用并返回原指针。 */
XRT_API xnetbytes* xrtNetBytesRef(xnetbytes* pBytes);



/* 释放拥有型网络字节结果；空指针视为空操作。 */
XRT_API void xrtNetBytesDestroy(xnetbytes* pBytes);



/* 返回拥有型网络字节结果的借用视图。 */
XRT_API xbytesview xrtNetBytesView(const xnetbytes* pBytes);



/* 写入默认尺寸类 512、2048、8192、32768 字节及有界缓存策略。 */
XRT_API void xrtNetBufPoolConfigInit(xnetbufpoolconfig* pConfig);



/* 创建线程归属的缓冲池；空配置使用默认值。 */
XRT_API xnetbufpool* xrtNetBufPoolCreate(const xnetbufpoolconfig* pConfig);



/* 销毁没有实时块的缓冲池；仍有块时失败且保持池有效。 */
XRT_API bool xrtNetBufPoolDestroy(xnetbufpool* pPool);



/* 将缓存裁剪到不超过指定字节数，返回真正释放的块数。 */
XRT_API size_t xrtNetBufPoolTrim(xnetbufpool* pPool, size_t iRetainBytes);



/* 复制缓冲池当前统计，不分配内存。 */
XRT_API void xrtNetBufPoolGet(const xnetbufpool* pPool, xnetbufpoolinfo* pInfo);



/* 初始化空缓冲链；池为空时所有块直接使用全局分配器。 */
XRT_API bool xrtNetBufInit(xnetbuf* pBuffer, xnetbufpool* pPool);



/* 释放全部块并取消尚未提交的写入预留。 */
XRT_API void xrtNetBufClear(xnetbuf* pBuffer);



/* 返回缓冲链总字节数。 */
XRT_API size_t xrtNetBufSize(const xnetbuf* pBuffer);



/* 返回缓冲链是否为空。 */
XRT_API bool xrtNetBufEmpty(const xnetbuf* pBuffer);



/* 返回当前非空 Span 数。 */
XRT_API size_t xrtNetBufSpanCount(const xnetbuf* pBuffer);



/* 获取最多指定数量的只读 Span。 */
XRT_API size_t xrtNetBufSpans(const xnetbuf* pBuffer, xnetspan* pSpans, size_t iCapacity);



/* 获取首个非空连续可读 Span；缓冲为空时清空输出并返回 false。 */
XRT_API bool xrtNetBufFront(const xnetbuf* pBuffer, xnetspan* pSpan);



/* 原子追加一份数据副本，失败时缓冲内容保持不变。 */
XRT_API bool xrtNetBufAppend(xnetbuf* pBuffer, const void* pData, size_t iSize);



/* 在缓冲链首部复制一段数据；成功后原有块和外部引用保持不动。 */
XRT_API bool xrtNetBufPrepend(
	xnetbuf* pBuffer,
	const void* pData,
	size_t iSize
);



/* 追加借用数据，调用方必须保证数据存活到该段被消费或清除。 */
XRT_API bool xrtNetBufAppendBorrow(xnetbuf* pBuffer, const void* pData, size_t iSize);



/* 接管一段由 xrtMalloc 家族分配的数据，成功后由缓冲调用 xrtFree。 */
XRT_API bool xrtNetBufAppendTake(xnetbuf* pBuffer, ptr pData, size_t iSize);



/* 接管带自定义释放过程的外部数据；失败时所有权仍归调用方。 */
XRT_API bool xrtNetBufAppendRef(xnetbuf* pBuffer, const void* pData,
	size_t iSize, xnetreleaseproc pRelease, ptr pContext);



/* 预留至少指定大小的连续尾部空间，供 Socket 或编解码器直接写入。 */
XRT_API bool xrtNetBufReserve(xnetbuf* pBuffer, size_t iMinimum, xnetwspan* pSpan);



/* 提交预留空间中已经写入的字节数。 */
XRT_API bool xrtNetBufCommit(xnetbuf* pBuffer, size_t iSize);



/* 放弃当前预留，缓冲内容保持不变。 */
XRT_API bool xrtNetBufCancel(xnetbuf* pBuffer);



/* 把源缓冲的全部块移动到目标尾部，源缓冲恢复为空。 */
XRT_API bool xrtNetBufMove(xnetbuf* pTarget, xnetbuf* pSource);



/* 确保指定长度的前缀连续并返回借用 Span。 */
XRT_API bool xrtNetBufPullup(xnetbuf* pBuffer, size_t iSize, xnetspan* pSpan);



/* 从指定偏移复制最多给定字节，但不消费数据。 */
XRT_API size_t xrtNetBufPeek(const xnetbuf* pBuffer,
	size_t iOffset, void* pOutput, size_t iSize);



/* 复制并消费最多给定字节；活动尾部写预留不阻止消费已提交前缀。 */
XRT_API size_t xrtNetBufRead(xnetbuf* pBuffer, void* pOutput, size_t iSize);



/* 从指定偏移查找一个字节，未找到时返回 XRT_NPOS。 */
XRT_API size_t xrtNetBufFind(const xnetbuf* pBuffer, uint8 iByte, size_t iOffset);



/* 消费最多给定字节；不会释放仍被活动写预留借用的尾块。 */
XRT_API size_t xrtNetBufConsume(xnetbuf* pBuffer, size_t iSize);



XRT_EXTERN_C_END

#endif

#endif
