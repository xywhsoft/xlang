/* TLS 字节流只封装 XRT 的完整拨号与有界异步队列，不参与 HTTP/WebSocket 对象模型。 */
static const char xl6TlsStreamSource[] = XL6_TLS_SOURCE(
static void xl6_tls_stream_drop(void* value)
{
    XL6_TlsStream* stream = (XL6_TlsStream*)value;
    if ( stream == 0 ) { return; }
    if ( stream->Native != 0 ) {
        (void)xrtTlsStreamClose(stream->Native);
        xrtTlsStreamDestroy(stream->Native);
    }
    xl6_xrt_tls_lease_release(stream->Lease);
    memset(stream, 0, sizeof(*stream));
    xrtFree(stream);
}

static XL6_TlsStream* xl6_tls_stream_create(xtlsstream* native, XL6_TlsLease* lease)
{
    XL6_TlsStream* stream;
    if ( native == 0 ) { return 0; }
    stream = (XL6_TlsStream*)xrtCalloc(1u, sizeof(*stream));
    if ( stream == 0 ) { return 0; }
    stream->Native = native;
    stream->Lease = lease != 0 ? xl6_xrt_tls_lease_retain(lease) : 0;
    if ( lease != 0 && stream->Lease == 0 ) { xrtFree(stream); return 0; }
    return stream;
}

static void* xl6_tls_stream_from_result(void* input, void* argument)
{
    XL6_TlsLease* lease = (XL6_TlsLease*)argument;
    xtlsstream* native = (xtlsstream*)input;
    XL6_TlsStream* stream;
    native = native != 0 ? xrtTlsStreamRef(native) : 0;
    stream = xl6_tls_stream_create(native, lease);
    if ( stream == 0 ) {
        if ( native != 0 ) { (void)xrtTlsStreamAbort(native); }
        xrtTlsStreamDestroy(native);
        return 0;
    }
    return stream;
}

static void xl6_tls_stream_release_lease(void* argument)
{
    xl6_xrt_tls_lease_release((XL6_TlsLease*)argument);
}

void* xl6_xrt_tls_stream_wrap(void* handle) { return handle; }
void* xl6_xrt_tls_stream_adopt(void* handle) { return xl6_tls_stream_create((xtlsstream*)handle, 0); }
void* xl6_xrt_tls_stream_native(void* handle) { XL6_TlsStream* stream = (XL6_TlsStream*)handle; return stream != 0 ? stream->Native : 0; }
long long xl6_xrt_tls_stream_destroy(void* handle) { xl6_tls_stream_drop(handle); return 0; }
int xl6_xrt_tls_stream_close(void* handle) { XL6_TlsStream* stream = (XL6_TlsStream*)handle; return stream != 0 && stream->Native != 0 && xrtTlsStreamClose(stream->Native); }
int xl6_xrt_tls_stream_abort(void* handle) { XL6_TlsStream* stream = (XL6_TlsStream*)handle; return stream != 0 && stream->Native != 0 && xrtTlsStreamAbort(stream->Native); }
long long xl6_xrt_tls_stream_state(void* handle) { XL6_TlsStream* stream = (XL6_TlsStream*)handle; return stream != 0 && stream->Native != 0 ? (long long)xrtTlsStreamState(stream->Native) : (long long)XTLS_STREAM_FAILED; }
long long xl6_xrt_tls_stream_available(void* handle) { XL6_TlsStream* stream = (XL6_TlsStream*)handle; return stream != 0 && stream->Native != 0 ? (long long)xrtTlsStreamAvailable(stream->Native) : 0; }
long long xl6_xrt_tls_stream_pending(void* handle) { XL6_TlsStream* stream = (XL6_TlsStream*)handle; return stream != 0 && stream->Native != 0 ? (long long)xrtTlsStreamPending(stream->Native) : 0; }
long long xl6_xrt_tls_stream_async_bytes(void* handle) { XL6_TlsStream* stream = (XL6_TlsStream*)handle; return stream != 0 && stream->Native != 0 ? (long long)xrtTlsStreamAsyncBytes(stream->Native) : 0; }
long long xl6_xrt_tls_stream_async_count(void* handle) { XL6_TlsStream* stream = (XL6_TlsStream*)handle; return stream != 0 && stream->Native != 0 ? (long long)xrtTlsStreamAsyncCount(stream->Native) : 0; }
) "\n";

/* Transport mechanics above are independent of the language Future carrier.
 * Each backend selects its declared adapter ABI; XLIR never reconstructs or
 * translates the legacy result layout. */
static const char xl6TlsLegacyFutureSource[] = XL6_TLS_SOURCE(
static const xl6_type_desc* xl6_type_desc_XL_Class_TlsStream(void);
static xfuture* xl6_tls_stream_future_then_owned(xfuture* source,
    int (*mapper)(const xl6_future_result*, void*, xl6_future_result*),
    void* argument, void (*drop)(void*))
{
    xfuture* mapped = xl6FutureThenOwned(source, mapper, argument, drop);
    if (mapped != 0) (void)xl6FutureLinkCancel(mapped, source);
    return mapped;
}
static xfuture* xl6_tls_stream_future_map(xfuture* source,
    int (*mapper)(const xl6_future_result*, void*, xl6_future_result*))
{
    xfuture* mapped;
    if (source == 0) return 0;
    mapped = xl6FutureThen(source, mapper, 0);
    if (mapped != 0) (void)xl6FutureLinkCancel(mapped, source);
    xl6FutureDestroy(source);
    return mapped;
}
static int xl6_tls_stream_map_native(const xl6_future_result* input,
    void* argument, xl6_future_result* output)
{
    void* stream = xl6_tls_stream_from_result(input != 0 ? input->pValue : 0, argument);
    if (stream == 0) return -1;
    return xl6_net_result_class(output, stream,
        xl6_type_desc_XL_Class_TlsStream(), xl6_tls_stream_drop);
}
) "\n";

static const char xl6TlsResidentFutureSource[] = XL6_TLS_SOURCE(
static xfuture* xl6_tls_stream_future_then_owned(xfuture* source,
    xfuturecontinueproc mapper, void* argument, void (*drop)(void*))
{
    return xlir_native_future_then_owned(source, mapper, argument, drop);
}
static xfuture* xl6_tls_stream_future_map(xfuture* source, xfuturecontinueproc mapper)
{
    xfuture* mapped;
    if (source == NULL) return NULL;
    mapped = xlir_native_future_then(source, mapper, 0);
    xrtFutureDestroy(source);
    return mapped;
}
static void xl6_tls_stream_map_native(const xfutureresult* input,
    xpromise* output, void* argument)
{
    void* stream = xl6_tls_stream_from_result(input != NULL ? input->Value : NULL, argument);
    xlir_native_future_publish_xvalue(output, xlir_net_tls_stream_wrap_owned(stream));
}
) "\n";


/* TLS 瀹㈡埛绔嫧鍙风嫭绔嬭緭鍑猴紝鏈嶅姟绔▼搴忎笉鎼哄甫 DNS 鍜屽鎴风楠岃瘉閾捐矾銆?*/
static const char xl6TlsDialSource[] = XL6_TLS_SOURCE(
static XL6_TlsStream* xl6_tls_stream_take(XL6_TlsStream* source)
{
    XL6_TlsStream* stream;
    if ( source == 0 || source->Native == 0 ) { return 0; }
    stream = (XL6_TlsStream*)xrtCalloc(1u, sizeof(*stream));
    if ( stream == 0 ) { return 0; }
    stream->Native = source->Native;
    stream->Lease = source->Lease;
    source->Native = 0;
    source->Lease = 0;
    return stream;
}

static void xl6_tls_client_config(
    XL6_TlsLease* lease,
    xtlsclientconfig* config
)
{
    xrtTlsClientConfigInit(config);
    config->Context = lease->Context;
    config->ServerName = xl6_tls_view(lease->Host);
    config->VerifyName = xl6_tls_view(lease->Host);
    config->Protocols = lease->Protocols;
    config->ProtocolCount = lease->ProtocolCount;
    config->Verifier = lease->Verifier;
}

static xfuture* xl6_tls_stream_dial(
    const char* host,
    uint16_t port,
    void* configHandle,
	int64_t timeoutMs
)
{
    XL6_TlsLease* lease;
    xtlsclientconfig client;
    xtlsdialconfig dial;
    xfuture* source;
    xfuture* mapped;
    if ( xl6_net_engine == 0 || xl6_net_resolver == 0 || host == 0 || host[0] == 0 || port == 0 ) { return 0; }
    lease = xl6_xrt_tls_lease_create(configHandle);
    if ( lease == 0 || lease->Invalid ) { xl6_xrt_tls_lease_release(lease); return 0; }
    xl6_tls_client_config(lease, &client);
    xrtTlsDialConfigInit(&dial);
    dial.Timeout = xl6_net_timeout_us(timeoutMs);
    dial.Transport.Timeout = dial.Timeout;
    dial.Transport.Stream.ConnectTimeout = dial.Timeout;
    source = xrtTlsDialAsync(
        xl6_net_engine,
        xl6_net_resolver,
        host,
        port,
        &client,
        &dial,
        0,
        0
    );
    if ( source == 0 ) { xl6_xrt_tls_lease_release(lease); return 0; }
    mapped = xl6_tls_stream_future_then_owned(
        source,
        xl6_tls_stream_map_native,
        lease,
        xl6_tls_stream_release_lease
    );
    xl6FutureDestroy(source);
    if ( mapped == 0 ) { xl6_xrt_tls_lease_release(lease); }
    return mapped;
}

void* xl6_xrt_tls_connect(
    const char* host,
    uint16_t port,
    void* configHandle,
    int64_t timeoutMs
)
{
    xfuture* future = xl6_tls_stream_dial(host, port, configHandle, timeoutMs);
    XL6_TlsStream* stream = 0;
    xvalue** box;
    xvalue* value;
    if ( future == 0 ) { return 0; }
    if ( timeoutMs < 0 ? xl6FutureWait(future) : xl6FutureWaitFor(future, timeoutMs) ) {
        box = (xvalue**)xl6FutureValue(future);
        value = box != 0 ? *box : 0;
        stream = value != 0 ? (XL6_TlsStream*)xl6_tls_stream_result_handle(value) : 0;
        stream = xl6_tls_stream_take(stream);
    } else {
        (void)xl6FutureCancel(future);
    }
    xl6FutureDestroy(future);
    return stream;
}

xfuture* xl6_xrt_tls_connect_async(const char* host, uint16_t port, void* configHandle, int64_t timeoutMs)
{
    return xl6_tls_stream_dial(host, port, configHandle, timeoutMs);
}
) "\n";



/* TLS Stream 鏀跺彂涓庣瓑寰呭彧渚濊禆宸插缓绔嬬殑娴侊紝涓嶅寘鍚嫧鍙锋垨鐩戝惉銆?*/
static const char xl6TlsStreamIoSource[] = XL6_TLS_SOURCE(

static const xnetbuf* xl6_tls_stream_buffer_view(void* transport)
{
    return xrtTlsStreamBuffer((xtlsstream*)transport);
}

static size_t xl6_tls_stream_buffer_consume(void* transport, size_t size)
{
    return xrtTlsStreamConsume((xtlsstream*)transport, size) ? size : 0u;
}

void* xl6_xrt_tls_stream_peek_buffer(void* handle, long long maxBytes)
{
    XL6_TlsStream* wrapper = (XL6_TlsStream*)handle;
    xtlsstream* stream = wrapper != 0 && wrapper->Native != 0 && maxBytes >= 0 ?
        xrtTlsStreamRef(wrapper->Native) : 0;
    xnetstream* transport = stream != 0 ? xrtTlsStreamTransport(stream) : 0;
    xnetworker* worker = transport != 0 ? xrtNetStreamWorker(transport) : 0;
    xl6_net_buffer_command command = { 0 };
    if ( stream == 0 || (uint64_t)maxBytes > (uint64_t)SIZE_MAX ) {
        xrtTlsStreamDestroy(stream);
        return 0;
    }
    command.Transport = stream;
    command.View = xl6_tls_stream_buffer_view;
    command.Kind = XL6_NET_BUFFER_PEEK;
    command.Size = (size_t)maxBytes;
    if ( !xl6_net_buffer_command_execute(worker, &command) ) { command.Buffer = 0; }
    xrtTlsStreamDestroy(stream);
    return command.Buffer;
}

int xl6_xrt_tls_stream_consume_exact(void* handle, long long size)
{
    XL6_TlsStream* wrapper = (XL6_TlsStream*)handle;
    xtlsstream* stream = wrapper != 0 && wrapper->Native != 0 && size >= 0 ?
        xrtTlsStreamRef(wrapper->Native) : 0;
    xnetstream* transport = stream != 0 ? xrtTlsStreamTransport(stream) : 0;
    xnetworker* worker = transport != 0 ? xrtNetStreamWorker(transport) : 0;
    xl6_net_buffer_command command = { 0 };
    if ( stream == 0 || (uint64_t)size > (uint64_t)SIZE_MAX ) {
        xrtTlsStreamDestroy(stream);
        return 0;
    }
    command.Transport = stream;
    command.View = xl6_tls_stream_buffer_view;
    command.Consume = xl6_tls_stream_buffer_consume;
    command.Kind = XL6_NET_BUFFER_CONSUME;
    command.Size = (size_t)size;
    if ( !xl6_net_buffer_command_execute(worker, &command) ) { command.Boolean = false; }
    xrtTlsStreamDestroy(stream);
    return command.Boolean;
}

xfuture* xl6_xrt_tls_stream_send_bytes_async(void* handle, void* data, size_t size)
{
    XL6_TlsStream* stream = (XL6_TlsStream*)handle;
    return stream != 0 && stream->Native != 0 && (data != 0 || size == 0u) ?
        xl6_tls_stream_future_map(xrtTlsStreamSendAsync(stream->Native, data, size), xl6_net_map_bool) : 0;
}

xfuture* xl6_xrt_tls_stream_send_text_async(void* handle, const char* text)
{
    return text != 0 ? xl6_xrt_tls_stream_send_bytes_async(handle, (void*)text, strlen(text)) : 0;
}

xfuture* xl6_xrt_tls_stream_send_vec_async(void* handle, xvalue* buffers)
{
    XL6_TlsStream* stream = (XL6_TlsStream*)handle;
    xnetspan local[16];
    size_t count = buffers != 0 ? xrtValueCount(buffers) : 0u;
    size_t index;
    xnetspan* spans;
    xfuture* future;
    if ( stream == 0 || stream->Native == 0 || count == 0u ) { return 0; }
    spans = count <= 16u ? local : (xnetspan*)xrtCalloc(count, sizeof(*spans));
    if ( spans == 0 ) { return 0; }
    for ( index = 0u; index < count; index++ ) {
        xbuffer* buffer = (xbuffer*)xl6ValueHandleData(xrtValueArrayGet(buffers, index));
        if ( buffer == 0 ) { if ( spans != local ) { xrtFree(spans); } return 0; }
        spans[index].Data = buffer->Data;
        spans[index].Size = buffer->Size;
    }
    future = xrtTlsStreamSendVecAsync(stream->Native, spans, count);
    if ( spans != local ) { xrtFree(spans); }
    return xl6_tls_stream_future_map(future, xl6_net_map_bool);
}

static bool xl6_tls_stream_wait_future(xfuture* future, long long timeoutMs)
{
    bool result;
    if ( future == 0 ) { return false; }
    result = timeoutMs < 0 ? xl6FutureWait(future) : xl6FutureWaitFor(future, timeoutMs);
    if ( !result ) { (void)xl6FutureCancel(future); }
    xl6FutureDestroy(future);
    return result;
}

int xl6_xrt_tls_stream_send_bytes(void* handle, void* data, size_t size, long long timeoutMs)
{
    return xl6_tls_stream_wait_future(xl6_xrt_tls_stream_send_bytes_async(handle, data, size), timeoutMs);
}

int xl6_xrt_tls_stream_send_text(void* handle, const char* text, long long timeoutMs)
{
    return text != 0 && xl6_xrt_tls_stream_send_bytes(handle, (void*)text, strlen(text), timeoutMs);
}

xfuture* xl6_xrt_tls_stream_recv_owned_async(void* handle, size_t maxBytes)
{
    XL6_TlsStream* stream = (XL6_TlsStream*)handle;
    return stream != 0 && stream->Native != 0 ?
        xl6_tls_stream_future_map(xrtTlsStreamRecvAsync(stream->Native, maxBytes), xl6_net_map_bytes) : 0;
}

xfuture* xl6_xrt_tls_stream_recv_text_async(void* handle, size_t maxBytes)
{
    XL6_TlsStream* stream = (XL6_TlsStream*)handle;
    return stream != 0 && stream->Native != 0 ?
        xl6_tls_stream_future_map(xrtTlsStreamRecvAsync(stream->Native, maxBytes), xl6_net_map_text) : 0;
}

void* xl6_xrt_tls_stream_recv_owned(void* handle, size_t maxBytes, long long timeoutMs)
{
    xfuture* future = xl6_xrt_tls_stream_recv_owned_async(handle, maxBytes);
    xnetbytes* bytes = 0;
    xvalue** box;
    xvalue* value;
    if ( future == 0 ) { return 0; }
    if ( timeoutMs < 0 ? xl6FutureWait(future) : xl6FutureWaitFor(future, timeoutMs) ) {
        box = (xvalue**)xl6FutureValue(future);
        value = box != 0 ? *box : 0;
        bytes = value != 0 ? (xnetbytes*)xl6_tls_bytes_result_handle(value) : 0;
        bytes = bytes != 0 ? xrtNetBytesRef(bytes) : 0;
    } else {
        (void)xl6FutureCancel(future);
    }
    xl6FutureDestroy(future);
    return bytes;
}

xfuture* xl6_xrt_tls_stream_wait_async(void* handle, long long kind)
{
    static const xtlsstreamwait waits[6] = {
        XTLS_STREAM_WAIT_OPEN,
        XTLS_STREAM_WAIT_READ,
        XTLS_STREAM_WAIT_WRITE,
        XTLS_STREAM_WAIT_DRAIN,
        XTLS_STREAM_WAIT_END,
        XTLS_STREAM_WAIT_CLOSE
    };
    XL6_TlsStream* stream = (XL6_TlsStream*)handle;
    return stream != 0 && stream->Native != 0 && kind >= 0 && kind < 6 ?
        xl6_tls_stream_future_map(xrtTlsStreamWaitAsync(stream->Native, waits[kind]), xl6_net_map_bool) : 0;
}

int xl6_xrt_tls_stream_wait(void* handle, long long kind, long long timeoutMs)
{
    return xl6_tls_stream_wait_future(xl6_xrt_tls_stream_wait_async(handle, kind), timeoutMs);
}

static char* xl6_tls_stream_address(void* handle, bool remote)
{
    XL6_TlsStream* stream = (XL6_TlsStream*)handle;
    xnetstream* transport = stream != 0 && stream->Native != 0 ? xrtTlsStreamTransport(stream->Native) : 0;
    xnetaddr address;
    bool ok = transport != 0 && (remote ? xrtNetStreamRemote(transport, &address) : xrtNetStreamLocal(transport, &address));
    return ok ? xl6_net_address_string(&address) : xl6_xrt_string_dup("", 0u);
}

void* xl6_xrt_tls_stream_local_addr(void* handle) { return xl6_tls_stream_address(handle, false); }
void* xl6_xrt_tls_stream_remote_addr(void* handle) { return xl6_tls_stream_address(handle, true); }
) "\n";



/* TLS 服务端直接公开 XRT 的有界监听与已完成握手流，不引入协议对象层。 */
static const char xl6TlsServerSource[] = XL6_TLS_SOURCE(
typedef struct XL6_TlsServer {
    xtlslistener* Native;
    XL6_TlsLease* Lease;
} XL6_TlsServer;

static void xl6_tls_server_drop(void* value)
{
    XL6_TlsServer* server = (XL6_TlsServer*)value;
    if ( server == 0 ) { return; }
    if ( server->Native != 0 ) {
        (void)xrtTlsListenerClose(server->Native);
        xrtTlsListenerDestroy(server->Native);
    }
    xl6_xrt_tls_lease_release(server->Lease);
    memset(server, 0, sizeof(*server));
    xrtFree(server);
}

void* xl6_xrt_tls_listen(
    const char* host,
    uint16_t port,
    void* configHandle,
    uint32_t backlog
)
{
    XL6_TlsLease* lease;
    XL6_TlsServer* server;
    xtlslistenerconfig config;
    xtlslistener* native;
    if ( xl6_net_engine == 0 ) { return 0; }
    lease = xl6_tls_lease_create_mode(configHandle, false);
    if ( lease == 0 || lease->Invalid || lease->Identity == 0 ) {
        xl6_xrt_tls_lease_release(lease);
        return 0;
    }
    xrtTlsListenerConfigInit(&config);
    if ( !xl6_net_address_parse(&config.Listen.Address, host, port, true) ) {
        xl6_xrt_tls_lease_release(lease);
        return 0;
    }
    if ( backlog != 0u ) {
        config.Listen.Backlog = backlog > INT_MAX ? INT_MAX : (int)backlog;
    }
    config.Tls.Context = lease->Context;
    config.Tls.Identity = lease->Identity;
    config.Tls.Protocols = lease->Protocols;
    config.Tls.ProtocolCount = lease->ProtocolCount;
    native = xrtTlsListenerStart(xl6_net_engine, &config, 0, 0, 0);
    if ( native == 0 ) {
        xl6_xrt_tls_lease_release(lease);
        return 0;
    }
    server = (XL6_TlsServer*)xrtCalloc(1u, sizeof(*server));
    if ( server == 0 ) {
        (void)xrtTlsListenerClose(native);
        xrtTlsListenerDestroy(native);
        xl6_xrt_tls_lease_release(lease);
        return 0;
    }
    server->Native = native;
    server->Lease = lease;
    return server;
}

void* xl6_xrt_tls_server_wrap(void* handle) { return handle; }
long long xl6_xrt_tls_server_destroy(void* handle) { xl6_tls_server_drop(handle); return 0; }

int xl6_xrt_tls_server_close(void* handle)
{
    XL6_TlsServer* server = (XL6_TlsServer*)handle;
    return server != 0 && server->Native != 0 && xrtTlsListenerClose(server->Native);
}

long long xl6_xrt_tls_server_state(void* handle)
{
    XL6_TlsServer* server = (XL6_TlsServer*)handle;
    return server != 0 && server->Native != 0 ?
        (long long)xrtTlsListenerState(server->Native) :
        (long long)XTLS_LISTENER_CLOSED;
}

long long xl6_xrt_tls_server_port(void* handle)
{
    XL6_TlsServer* server = (XL6_TlsServer*)handle;
    xnetaddr address;
    return server != 0 && server->Native != 0 && xrtTlsListenerLocal(server->Native, &address) ?
        (long long)address.Port : 0;
}

void* xl6_xrt_tls_server_local_addr(void* handle)
{
    XL6_TlsServer* server = (XL6_TlsServer*)handle;
    xnetaddr address;
    return server != 0 && server->Native != 0 && xrtTlsListenerLocal(server->Native, &address) ?
        xl6_net_address_string(&address) : xl6_xrt_string_dup("", 0u);
}

void* xl6_xrt_tls_server_accept(void* handle, long long timeoutMs)
{
    XL6_TlsServer* server = (XL6_TlsServer*)handle;
    xtlsstream* native;
    XL6_TlsStream* stream;
    if ( server == 0 || server->Native == 0 || server->Lease == 0 ) { return 0; }
    native = xrtTlsListenerAcceptWait(server->Native, xl6_net_deadline(timeoutMs), 0);
    if ( native == 0 ) { return 0; }
    stream = xl6_tls_stream_create(native, server->Lease);
    if ( stream == 0 ) {
        (void)xrtTlsStreamAbort(native);
        xrtTlsStreamDestroy(native);
    }
    return stream;
}

xfuture* xl6_xrt_tls_server_accept_async(void* handle)
{
    XL6_TlsServer* server = (XL6_TlsServer*)handle;
    XL6_TlsLease* lease;
    xfuture* source;
    xfuture* mapped;
    if ( server == 0 || server->Native == 0 ) { return 0; }
    lease = xl6_xrt_tls_lease_retain(server->Lease);
    if ( lease == 0 ) { return 0; }
    source = xrtTlsListenerAcceptAsync(server->Native);
    if ( source == 0 ) {
        xl6_xrt_tls_lease_release(lease);
        return 0;
    }
    mapped = xl6_tls_stream_future_then_owned(
        source,
        xl6_tls_stream_map_native,
        lease,
        xl6_tls_stream_release_lease
    );
    xl6FutureDestroy(source);
    if ( mapped == 0 ) { xl6_xrt_tls_lease_release(lease); }
    return mapped;
}

void* xl6_xrt_tls_server_stats(void* handle)
{
    XL6_TlsServer* server = (XL6_TlsServer*)handle;
    xtlslistenerstats value;
    xl6_net_stats_snapshot* snapshot;
    if ( server == 0 || server->Native == 0 || !xrtTlsListenerStats(server->Native, &value) ) { return 0; }
    snapshot = xl6_net_stats_create(10u);
    if ( snapshot == 0 ) { return 0; }
    snapshot->Values[0] = (uint64_t)value.State;
    snapshot->Values[1] = value.Handshakes;
    snapshot->Values[2] = value.Accepted;
    snapshot->Values[3] = value.Rejected;
    snapshot->Values[4] = value.HandshakeErrors;
    snapshot->Values[5] = value.ActiveHandshakes;
    snapshot->Values[6] = value.PeakHandshakes;
    snapshot->Values[7] = value.QueuedAccepts;
    snapshot->Values[8] = value.PeakQueuedAccepts;
    snapshot->Values[9] = value.AcceptWaiters;
    return snapshot;
}
) "\n";



/* 输出完整 TLS 适配层。 */
void xl6CgenEmitTlsRuntime(
    XL6_Output* out,
    int hasTlsStreamRuntime,
    int hasTlsDialRuntime,
    int hasTlsServerRuntime,
    const char* streamResultHandle,
    const char* bytesResultHandle,
    XL6_CgenTlsFutureAbi futureAbi
)
{
    xl6OutputPuts(xl6TlsTypesSource, out);
    xl6OutputPuts(xl6TlsConfigSource, out);
    xl6OutputPuts(xl6TlsCallbackSource, out);
    xl6OutputPuts(xl6TlsLeaseSource, out);
    xl6OutputPuts(xl6TlsConfigApiSource, out);
    xl6OutputPuts(xl6TlsSessionSource, out);
    if ( hasTlsStreamRuntime || hasTlsDialRuntime || hasTlsServerRuntime ) {
        xl6OutputPrintf(out,
            "static void* xl6_tls_stream_result_handle(const xvalue* value) "
            "{ return %s(value); }\n", streamResultHandle);
        xl6OutputPuts(xl6TlsStreamSource, out);
        xl6OutputPuts(futureAbi == XL6_CGEN_TLS_FUTURE_RESIDENT_EXCLUSIVE_V1
            ? xl6TlsResidentFutureSource : xl6TlsLegacyFutureSource, out);
    }
    if ( hasTlsDialRuntime ) {
        xl6OutputPuts(xl6TlsDialSource, out);
    }
    if ( hasTlsStreamRuntime ) {
        xl6OutputPrintf(out,
            "static void* xl6_tls_bytes_result_handle(const xvalue* value) "
            "{ return %s(value); }\n", bytesResultHandle);
        xl6OutputPuts(xl6TlsStreamIoSource, out);
    }
    if ( hasTlsServerRuntime ) {
        xl6OutputPuts(xl6TlsServerSource, out);
    }
}



#undef XL6_TLS_SOURCE
