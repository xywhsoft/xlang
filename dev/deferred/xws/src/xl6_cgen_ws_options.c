static const char xl6WebSocketOptionsSource[] = XL6_WS_SOURCE(
static uint64_t xl6_ws_timeout(long long milliseconds)
{
    if ( milliseconds < 0 ) { return UINT64_MAX; }
    if ( (uint64_t)milliseconds > (UINT64_MAX / UINT64_C(1000)) ) { return UINT64_MAX; }
    return (uint64_t)milliseconds * UINT64_C(1000);
}

static size_t xl6_ws_size(long long value)
{
    return value > 0 ? (size_t)value : 0u;
}

static uint64_t xl6_ws_u64(long long value)
{
    return value > 0 ? (uint64_t)value : 0u;
}

static uint32_t xl6_ws_u32(long long value)
{
    return value > 0 && (uint64_t)value <= UINT32_MAX ? (uint32_t)value : 0u;
}

static bool xl6_ws_replace(char** target, const char* value)
{
    char* copy;
    if ( target == 0 ) { return false; }
    copy = xl6_xrt_string_dup(value != 0 ? value : "", 0u);
    if ( copy == 0 ) { return false; }
    xrtFree(*target);
    *target = copy;
    return true;
}

static void xl6_ws_client_options_unit(XL6_WsClientOptions* options)
{
    if ( options == 0 ) { return; }
    xrtFree(options->Url);
    xrtFree(options->Origin);
    xrtFree(options->Protocols);
    xrtHttpHeadersDestroy(options->Headers);
    xrtNetProxyRelease(options->Proxy);
    memset(options, 0, sizeof(*options));
}

static bool xl6_ws_client_options_init(XL6_WsClientOptions* options, const char* url)
{
    if ( options == 0 ) { return false; }
    memset(options, 0, sizeof(*options));
    options->Headers = xrtHttpHeadersCreate(0);
    options->MessageLimit = 1048576u;
    options->FrameLimit = UINT64_C(1048576);
    options->SendLimit = 1048576u;
    options->HandshakeLimit = 65536u;
    options->ConnectTimeout = UINT64_C(30000000);
    options->HandshakeTimeout = UINT64_C(30000000);
    options->CloseTimeout = UINT64_C(5000000);
    options->CompressMin = 256u;
    options->CompressLevel = -1;
    options->VerifyPeer = true;
    if ( options->Headers == 0 ||
        !xl6_ws_replace(&options->Url, url) ||
        !xl6_ws_replace(&options->Origin, "") ||
        !xl6_ws_replace(&options->Protocols, "") ) {
        xl6_ws_client_options_unit(options);
        return false;
    }
    return true;
}

static bool xl6_ws_client_options_copy(XL6_WsClientOptions* target, const XL6_WsClientOptions* source)
{
    if ( target == 0 || source == 0 || !xl6_ws_client_options_init(target, source->Url) ) { return false; }
    if ( !xl6_ws_replace(&target->Origin, source->Origin) ||
        !xl6_ws_replace(&target->Protocols, source->Protocols) ) {
        xl6_ws_client_options_unit(target);
        return false;
    }
    xrtHttpHeadersDestroy(target->Headers);
    target->Headers = xrtHttpHeadersClone(source->Headers);
    if ( target->Headers == 0 ) { xl6_ws_client_options_unit(target); return false; }
    target->Proxy = xrtNetProxyRetain(source->Proxy);
    target->MessageLimit = source->MessageLimit;
    target->FrameLimit = source->FrameLimit;
    target->SendLimit = source->SendLimit;
    target->LowWater = source->LowWater;
    target->HighWater = source->HighWater;
    target->HandshakeLimit = source->HandshakeLimit;
    target->ConnectTimeout = source->ConnectTimeout;
    target->HandshakeTimeout = source->HandshakeTimeout;
    target->CloseTimeout = source->CloseTimeout;
    target->CompressMin = source->CompressMin;
    target->CompressLevel = source->CompressLevel;
    target->Compression = source->Compression;
    target->VerifyPeer = source->VerifyPeer;
    target->TlsConfig = source->TlsConfig;
    return true;
}

static void xl6_ws_server_options_unit(XL6_WsServerOptions* options)
{
    if ( options == 0 ) { return; }
    xrtFree(options->Host);
    xrtFree(options->Path);
    xrtFree(options->Origin);
    xrtFree(options->Protocols);
    memset(options, 0, sizeof(*options));
}

static bool xl6_ws_server_options_init(XL6_WsServerOptions* options, long long port, const char* host)
{
    if ( options == 0 || port < 0 || port > 65535 ) { return false; }
    memset(options, 0, sizeof(*options));
    options->Port = (uint16_t)port;
    options->Backlog = 128u;
    options->MessageLimit = 1048576u;
    options->FrameLimit = UINT64_C(1048576);
    options->SendLimit = 1048576u;
    options->HandshakeLimit = 65536u;
    options->HandshakeTimeout = UINT64_C(30000000);
    options->CloseTimeout = UINT64_C(5000000);
    options->CompressMin = 256u;
    options->CompressLevel = -1;
    if ( !xl6_ws_replace(&options->Host, host != 0 && host[0] != 0 ? host : "127.0.0.1") ||
        !xl6_ws_replace(&options->Path, "/") ||
        !xl6_ws_replace(&options->Origin, "") ||
        !xl6_ws_replace(&options->Protocols, "") ) {
        xl6_ws_server_options_unit(options);
        return false;
    }
    return true;
}

static bool xl6_ws_server_options_copy(XL6_WsServerOptions* target, const XL6_WsServerOptions* source)
{
    if ( target == 0 || source == 0 || !xl6_ws_server_options_init(target, source->Port, source->Host) ) { return false; }
    if ( !xl6_ws_replace(&target->Path, source->Path) ||
        !xl6_ws_replace(&target->Origin, source->Origin) ||
        !xl6_ws_replace(&target->Protocols, source->Protocols) ) {
        xl6_ws_server_options_unit(target);
        return false;
    }
    target->Backlog = source->Backlog;
    target->ListenerFlags = source->ListenerFlags;
    target->MessageLimit = source->MessageLimit;
    target->FrameLimit = source->FrameLimit;
    target->SendLimit = source->SendLimit;
    target->LowWater = source->LowWater;
    target->HighWater = source->HighWater;
    target->HandshakeLimit = source->HandshakeLimit;
    target->HandshakeTimeout = source->HandshakeTimeout;
    target->CloseTimeout = source->CloseTimeout;
    target->CompressMin = source->CompressMin;
    target->CompressLevel = source->CompressLevel;
    target->Compression = source->Compression;
    target->TlsConfig = source->TlsConfig;
    return true;
}

void* xl6_xrt_ws_client_options_create(const char* url)
{
    XL6_WsClientOptions* options = (XL6_WsClientOptions*)xrtCalloc(1u, sizeof(*options));
    if ( options == 0 || !xl6_ws_client_options_init(options, url) ) { xrtFree(options); return 0; }
    return options;
}

long long xl6_xrt_ws_client_options_destroy(void* raw)
{
    XL6_WsClientOptions* options = (XL6_WsClientOptions*)raw;
    if ( options != 0 ) { xl6_ws_client_options_unit(options); xrtFree(options); }
    return 0;
}

void* xl6_xrt_ws_client_options_native(void* raw) { return raw; }
int xl6_xrt_ws_client_options_url(void* raw, const char* value) { return raw != 0 && xl6_ws_replace(&((XL6_WsClientOptions*)raw)->Url, value); }
int xl6_xrt_ws_client_options_origin(void* raw, const char* value) { return raw != 0 && xl6_ws_replace(&((XL6_WsClientOptions*)raw)->Origin, value); }
int xl6_xrt_ws_client_options_protocols(void* raw, const char* value) { return raw != 0 && xl6_ws_replace(&((XL6_WsClientOptions*)raw)->Protocols, value); }
int xl6_xrt_ws_client_options_header(void* raw, const char* name, const char* value) { XL6_WsClientOptions* options = (XL6_WsClientOptions*)raw; return options != 0 && name != 0 && value != 0 && xrtHttpHeadersSet(options->Headers, xl6_ws_view(name), xl6_ws_view(value)); }
int xl6_xrt_ws_client_options_add_header(void* raw, const char* name, const char* value) { XL6_WsClientOptions* options = (XL6_WsClientOptions*)raw; return options != 0 && name != 0 && value != 0 && xrtHttpHeadersAdd(options->Headers, xl6_ws_view(name), xl6_ws_view(value)); }
long long xl6_xrt_ws_client_options_remove_header(void* raw, const char* name) { XL6_WsClientOptions* options = (XL6_WsClientOptions*)raw; return options != 0 && name != 0 ? (long long)xrtHttpHeadersRemove(options->Headers, xl6_ws_view(name)) : 0; }

void xl6_xrt_ws_client_options_limits(void* raw, long long receive, long long frame, long long queued)
{
    XL6_WsClientOptions* options = (XL6_WsClientOptions*)raw;
    if ( options == 0 ) { return; }
    if ( receive > 0 ) { options->MessageLimit = xl6_ws_size(receive); }
    if ( frame > 0 ) { options->FrameLimit = xl6_ws_u64(frame); }
    if ( queued > 0 ) { options->SendLimit = xl6_ws_size(queued); }
}

void xl6_xrt_ws_client_options_handshake_limit(void* raw, long long bytes) { XL6_WsClientOptions* options = (XL6_WsClientOptions*)raw; if ( options != 0 && bytes > 0 ) { options->HandshakeLimit = xl6_ws_size(bytes); } }
void xl6_xrt_ws_client_options_watermarks(void* raw, long long low, long long high) { XL6_WsClientOptions* options = (XL6_WsClientOptions*)raw; if ( options != 0 ) { options->LowWater = xl6_ws_size(low); options->HighWater = xl6_ws_size(high); } }
void xl6_xrt_ws_client_options_timeouts(void* raw, long long connect, long long handshake, long long close) { XL6_WsClientOptions* options = (XL6_WsClientOptions*)raw; if ( options != 0 ) { options->ConnectTimeout = xl6_ws_timeout(connect); options->HandshakeTimeout = xl6_ws_timeout(handshake); options->CloseTimeout = xl6_ws_timeout(close); } }
void xl6_xrt_ws_client_options_compression(void* raw, int enabled, long long minimum, long long level) { XL6_WsClientOptions* options = (XL6_WsClientOptions*)raw; if ( options != 0 ) { options->Compression = enabled != 0; options->CompressMin = xl6_ws_size(minimum); options->CompressLevel = (int)level; } }
void xl6_xrt_ws_client_options_verify_peer(void* raw, int enabled) { XL6_WsClientOptions* options = (XL6_WsClientOptions*)raw; if ( options != 0 ) { options->VerifyPeer = enabled != 0; } }
int xl6_xrt_ws_client_options_tls(void* raw, void* config) { XL6_WsClientOptions* options = (XL6_WsClientOptions*)raw; if ( options == 0 || config == 0 ) { return false; } options->TlsConfig = config; return true; }
void xl6_xrt_ws_client_options_proxy(void* raw, void* proxy) { XL6_WsClientOptions* options = (XL6_WsClientOptions*)raw; if ( options == 0 ) { return; } xrtNetProxyRelease(options->Proxy); options->Proxy = xrtNetProxyRetain((const xnetproxy*)proxy); }

void* xl6_xrt_ws_server_options_create(long long port, const char* host)
{
    XL6_WsServerOptions* options = (XL6_WsServerOptions*)xrtCalloc(1u, sizeof(*options));
    if ( options == 0 || !xl6_ws_server_options_init(options, port, host) ) { xrtFree(options); return 0; }
    return options;
}

long long xl6_xrt_ws_server_options_destroy(void* raw)
{
    XL6_WsServerOptions* options = (XL6_WsServerOptions*)raw;
    if ( options != 0 ) { xl6_ws_server_options_unit(options); xrtFree(options); }
    return 0;
}

void* xl6_xrt_ws_server_options_native(void* raw) { return raw; }
int xl6_xrt_ws_server_options_path(void* raw, const char* value) { return raw != 0 && value != 0 && value[0] == '/' && xl6_ws_replace(&((XL6_WsServerOptions*)raw)->Path, value); }
int xl6_xrt_ws_server_options_origin(void* raw, const char* value) { return raw != 0 && xl6_ws_replace(&((XL6_WsServerOptions*)raw)->Origin, value); }
int xl6_xrt_ws_server_options_protocol(void* raw, const char* value) { return raw != 0 && xl6_ws_replace(&((XL6_WsServerOptions*)raw)->Protocols, value); }

void xl6_xrt_ws_server_options_limits(void* raw, long long receive, long long frame, long long queued)
{
    XL6_WsServerOptions* options = (XL6_WsServerOptions*)raw;
    if ( options == 0 ) { return; }
    if ( receive > 0 ) { options->MessageLimit = xl6_ws_size(receive); }
    if ( frame > 0 ) { options->FrameLimit = xl6_ws_u64(frame); }
    if ( queued > 0 ) { options->SendLimit = xl6_ws_size(queued); }
}

void xl6_xrt_ws_server_options_handshake_limit(void* raw, long long bytes) { XL6_WsServerOptions* options = (XL6_WsServerOptions*)raw; if ( options != 0 && bytes > 0 ) { options->HandshakeLimit = xl6_ws_size(bytes); } }
void xl6_xrt_ws_server_options_listener(void* raw, long long backlog, long long flags) { XL6_WsServerOptions* options = (XL6_WsServerOptions*)raw; if ( options != 0 ) { options->Backlog = xl6_ws_u32(backlog); options->ListenerFlags = xl6_ws_u32(flags); } }
void xl6_xrt_ws_server_options_watermarks(void* raw, long long low, long long high) { XL6_WsServerOptions* options = (XL6_WsServerOptions*)raw; if ( options != 0 ) { options->LowWater = xl6_ws_size(low); options->HighWater = xl6_ws_size(high); } }
void xl6_xrt_ws_server_options_timeouts(void* raw, long long handshake, long long close) { XL6_WsServerOptions* options = (XL6_WsServerOptions*)raw; if ( options != 0 ) { options->HandshakeTimeout = xl6_ws_timeout(handshake); options->CloseTimeout = xl6_ws_timeout(close); } }
void xl6_xrt_ws_server_options_compression(void* raw, int enabled, long long minimum, long long level) { XL6_WsServerOptions* options = (XL6_WsServerOptions*)raw; if ( options != 0 ) { options->Compression = enabled != 0; options->CompressMin = xl6_ws_size(minimum); options->CompressLevel = (int)level; } }
int xl6_xrt_ws_server_options_tls(void* raw, void* config) { XL6_WsServerOptions* options = (XL6_WsServerOptions*)raw; if ( options == 0 || config == 0 ) { return false; } options->TlsConfig = config; return true; }
) "\n";



/* 对外值对象和 Connection 公共操作统一使用拥有型快照与异步发送入口。 */
