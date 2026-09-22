static const char xl6WebSocketTypesSource[] =
	"#ifndef XL6_WEBSOCKET_RUNTIME\n"
	"#define XL6_WEBSOCKET_RUNTIME\n"
	"#include <stdbool.h>\n"
	"#include <stddef.h>\n"
	"#include <stdint.h>\n"
	"#include <string.h>\n\n"
	"#ifndef XL_RUNTIME_H\n"
	"#ifndef XL6_XERROR_TYPEDEF\n"
	"#define XL6_XERROR_TYPEDEF\n"
	"typedef struct xerror xerror;\n"
	"#endif\n"
	"typedef struct xhttpheaders xhttpheaders;\n"
	"typedef struct xpromise xpromise;\n"
	"typedef struct xcancel xcancel;\n"
	"typedef struct xrtcallable xrtcallable;\n"
	"typedef struct xwsconn xwsconn;\n"
	"typedef struct xwsopenresult xwsopenresult;\n\n"
	"#endif\n"
	"typedef struct XL6_TlsLease XL6_TlsLease;\n"
	"#ifndef XL_RUNTIME_H\n"
	"typedef struct xwsdeflate { uint32_t Flags; uint8_t ServerMaxWindowBits; uint8_t ClientMaxWindowBits; } xwsdeflate;\n"
	"typedef struct xwsinflaterconfig { uint64_t OutputLimit; uint8_t WindowBits; bool NoContextTakeover; bool Retain; } xwsinflaterconfig;\n"
	"typedef struct xwsdeflaterconfig { uint64_t OutputLimit; int32_t Level; int Strategy; uint8_t WindowBits; bool NoContextTakeover; bool Retain; } xwsdeflaterconfig;\n"
	"typedef struct xwsmessageinfo { uint32_t Flags; uint16_t Rsv; uint8_t Opcode; uint8_t FrameOpcode; uint64_t PayloadSize; size_t Offset; } xwsmessageinfo;\n"
	"typedef struct xwsconnconfig { int Role; xstrview Protocol; size_t MessageLimit; uint64_t FrameLimit; size_t SendLimit; size_t ControlReserve; uint64_t CloseTimeout; bool AutoPong; size_t AsyncBytesLimit; uint32_t AsyncCountLimit; uint32_t AsyncBatch; xwsdeflate Deflate; xwsinflaterconfig Inflater; xwsdeflaterconfig Deflater; bool DeflateEnabled; } xwsconnconfig;\n"
	"typedef struct xwsconnclose { uint32_t Flags; int Transport; uint16_t LocalCode; uint16_t RemoteCode; xstrview Reason; } xwsconnclose;\n"
	"typedef struct xwsconnevents {\n"
	"    void (*MessageBegin)(xwsconn*, const xwsmessageinfo*, void*);\n"
	"    void (*MessageData)(xwsconn*, xbytesview, void*);\n"
	"    void (*MessageEnd)(xwsconn*, void*);\n"
	"    void (*Ping)(xwsconn*, xbytesview, void*);\n"
	"    void (*Pong)(xwsconn*, xbytesview, void*);\n"
	"    void (*Backpressure)(xwsconn*, size_t, void*);\n"
	"    void (*Writable)(xwsconn*, size_t, void*);\n"
	"    void (*Drain)(xwsconn*, void*);\n"
	"    void (*Error)(xwsconn*, const xerror*, void*);\n"
	"    void (*Close)(xwsconn*, const xwsconnclose*, void*);\n"
	"} xwsconnevents;\n"
	"typedef bool (*xl6_ws_deflate_accept_proc)(const xwsdeflate*, xwsdeflate*, void*);\n"
	"typedef struct xwsserverconfig { xwsconnconfig Connection; xstrview Protocols; xl6_ws_deflate_accept_proc AcceptDeflate; void* DeflateData; bool EnableDeflate; bool RequireDeflate; } xwsserverconfig;\n"
	"typedef struct xwsserverhandshake { char Accept[29]; xstrview Protocol; xwsdeflate Deflate; char Extensions[129]; bool DeflateEnabled; } xwsserverhandshake;\n"
	"typedef struct xwsclientconfig { xwsconnconfig Connection; xhttpcalloptions Http; xstrview Protocols; xwsdeflate Deflate; bool EnableDeflate; bool RequireDeflate; } xwsclientconfig;\n"
	"typedef void (*xl6_ws_connect_proc)(xhttpcall*, int, xwsconn*, xhttpresponse*, const xerror*, void*);\n"
	"typedef void (*xl6_ws_upgrade_proc)(xhttpconn*, int, xwsconn*, const xerror*, void*);\n"
	"#endif\n\n"
	"typedef struct XL6_WsCallbacks { XL6_Function Open; XL6_Function Text; XL6_Function Binary; XL6_Function Close; XL6_Function Error; XL6_Function Ping; XL6_Function Pong; XL6_Function Backpressure; XL6_Function Writable; XL6_Function Drain; XL6_Function Handshake; } XL6_WsCallbacks;\n"
	"typedef struct XL6_WsErrorSnapshot { xerror* Error; int Result; int HttpStatus; int ProtocolError; uint16_t CloseCode; } XL6_WsErrorSnapshot;\n"
	"typedef struct XL6_WsCloseSnapshot { xwsconnclose Close; char* Reason; } XL6_WsCloseSnapshot;\n"
	"typedef struct XL6_WsClientOptions { char* Url; char* Origin; char* Protocols; xhttpheaders* Headers; xnetproxy* Proxy; size_t MessageLimit; uint64_t FrameLimit; size_t SendLimit; size_t LowWater; size_t HighWater; size_t HandshakeLimit; uint64_t ConnectTimeout; uint64_t HandshakeTimeout; uint64_t CloseTimeout; size_t CompressMin; int CompressLevel; bool Compression; bool VerifyPeer; void* TlsConfig; } XL6_WsClientOptions;\n"
	"typedef struct XL6_WsServerOptions { char* Host; char* Path; char* Origin; char* Protocols; uint16_t Port; uint32_t Backlog; uint32_t ListenerFlags; size_t MessageLimit; uint64_t FrameLimit; size_t SendLimit; size_t LowWater; size_t HighWater; size_t HandshakeLimit; uint64_t HandshakeTimeout; uint64_t CloseTimeout; size_t CompressMin; int CompressLevel; bool Compression; void* TlsConfig; } XL6_WsServerOptions;\n"
	"typedef struct XL6_WsClient XL6_WsClient;\n"
	"typedef struct XL6_WsServer XL6_WsServer;\n"
	"typedef struct XL6_WsPeer XL6_WsPeer;\n"
	"typedef struct XL6_WsWriter { xwsconn* Connection; xbuffer* Buffer; uint8_t Opcode; size_t CompressMin; bool Compression; bool Finished; } XL6_WsWriter;\n"
	"typedef struct XL6_WsHandshakeMessage { int Kind; const xhttpserverrequest* Request; const xhttpresponse* Response; } XL6_WsHandshakeMessage;\n"
	"typedef struct XL6_WsHandshakeResponse { int Action; int Status; char* Reason; char* Protocol; bool Compression; void* ConnectionData; xhttpheaders* Headers; } XL6_WsHandshakeResponse;\n"
	"struct XL6_WsClient { volatile int32_t References; xnetengine* Engine; bool OwnEngine; xhttpclient* Http; xhttpcall* Call; xfuture* OpenFuture; xpromise* OpenPromise; xwsconn* Connection; xbuffer* Message; uint8_t MessageOpcode; size_t CompressMin; bool Compression; bool Started; bool EventReference; XL6_TlsLease* TlsLease; XL6_WsClientOptions Options; XL6_WsCallbacks Callbacks; XL6_WsErrorSnapshot LastError; XL6_WsCloseSnapshot LastClose; };\n"
	"struct XL6_WsServer { volatile int32_t References; xnetengine* Engine; bool OwnEngine; XL6_Http_Server* Http; XL6_TlsLease* TlsLease; XL6_WsServerOptions Options; bool Started; XL6_WsCallbacks Callbacks; XL6_WsErrorSnapshot LastError; };\n"
	"struct XL6_WsPeer { volatile int32_t References; XL6_WsServer* Server; xwsconn* Connection; xbuffer* Message; uint8_t MessageOpcode; void* UserData; XL6_WsCloseSnapshot LastClose; XL6_WsErrorSnapshot LastError; };\n\n";



/* 当前 XRT 与 xlang Future 桥接所需的公开函数。 */
static const char xl6WebSocketApiSource[] = XL6_WS_SOURCE(
static inline char* xl6_xrt_string_dup(const char*, size_t);

XL6_TlsLease* xl6_xrt_tls_lease_create(void*);
void xl6_xrt_tls_lease_release(XL6_TlsLease*);
typedef int (*xl6_ws_future_cont_fn)(const xl6_future_result*, void*, xl6_future_result*);
xfuture* xl6FutureThen(xfuture*, xl6_ws_future_cont_fn, void*);
bool xl6FutureLinkCancel(xfuture*, xfuture*);
bool xl6FutureWaitFor(xfuture*, long long);
bool xl6FutureCancel(xfuture*);
int xl6FutureStatus(xfuture*);
void* xl6FutureValue(xfuture*);
void xl6FutureDestroy(xfuture*);


bool xl6_net_runtime_init(void);
void* xl6_xrt_net_engine_create(long long);
int xl6_xrt_net_engine_start(void*);
long long xl6_xrt_net_engine_destroy(void*);

bool xrtHttpServerRoute(xhttpserverrouter*, xstrview, xstrview, void (*)(xhttpserver*, xhttpconn*, const xhttpserverrequest*, const xhttprouteparam*, size_t, void*), void*);
static XL6_Http_Server* xl6_http_server_create_core(xnetengine*, bool, const char*, int, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
bool xl6_xrt_http_server_start(void*);
void xl6_xrt_http_server_stop(void*);
void xl6_xrt_http_server_destroy(void*);
int xl6_xrt_http_server_port(void*);

static xvalue* xl6_function_call_var(XL6_Function, int, ...);
static XL6_Function xl6_function_copy_value(XL6_Function);
static void xl6_function_release_value(XL6_Function);
static XL6_Function xl6_function_from_callable(xrtcallable*);
xrtcallable* xrtValueGetCallable(const xvalue*);
xvalue* xrtValuePointer(void*);
xvalue* xrtValueInt(long long);
void xrtValueRelease(xvalue*);
static bool xl6_xrt_value_bool(const xvalue*);
static long long xl6_xrt_value_int(const xvalue*);
static xvalue* xl6_xrt_value_text_create(void*, uint32_t, bool);
) "\n";



/* 小型值、错误和回调工具集中在一处，避免客户端与服务端重复实现。 */
