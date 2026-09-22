static const char xl6WebSocketServerSource[] = XL6_WS_SOURCE(
static void xl6_ws_server_config(const XL6_WsServerOptions* options, xwsserverconfig* config)
{
    xrtWsServerConfigInit(config);
    config->Connection.MessageLimit = options->MessageLimit;
    config->Connection.FrameLimit = options->FrameLimit;
    config->Connection.SendLimit = options->HighWater != 0u && options->HighWater < options->SendLimit ? options->HighWater : options->SendLimit;
    config->Connection.CloseTimeout = options->CloseTimeout;
    config->Connection.AsyncBytesLimit = options->SendLimit;
    config->Connection.Deflater.Level = options->CompressLevel;
    config->Protocols = xl6_ws_view(options->Protocols);
    config->EnableDeflate = options->Compression;
}

static bool xl6_ws_origin_allowed(const XL6_WsServer* server, const xhttpserverrequest* request)
{
    const xhttpfield* field;
    size_t size;
    if ( server == 0 || server->Options.Origin == 0 || server->Options.Origin[0] == 0 ) { return true; }
    field = xrtHttpServerRequestHeader(request, xl6_ws_view("Origin"));
    size = strlen(server->Options.Origin);
    return field != 0 && field->Value.Size == size && memcmp(field->Value.Data, server->Options.Origin, size) == 0;
}

static void xl6_ws_handshake_response_unit(XL6_WsHandshakeResponse* response)
{
    if ( response == 0 ) { return; }
    xrtFree(response->Reason);
    xrtFree(response->Protocol);
    xrtHttpHeadersDestroy(response->Headers);
    memset(response, 0, sizeof(*response));
}

static bool xl6_ws_handshake_response_init(XL6_WsHandshakeResponse* response, const xwsserverhandshake* handshake)
{
    if ( response == 0 || handshake == 0 ) { return false; }
    memset(response, 0, sizeof(*response));
    response->Status = 403;
    response->Compression = handshake->DeflateEnabled;
    response->Headers = xrtHttpHeadersCreate(0);
    response->Reason = xl6_xrt_string_dup("Forbidden", 0u);
    response->Protocol = xl6_ws_copy_view(handshake->Protocol);
    if ( response->Headers == 0 || response->Reason == 0 || response->Protocol == 0 ) { xl6_ws_handshake_response_unit(response); return false; }
    return true;
}

static int xl6_ws_server_handshake(XL6_WsServer* server, XL6_WsPeer* peer, const xhttpserverrequest* request, XL6_WsHandshakeResponse* response)
{
    XL6_WsHandshakeMessage message;
    xvalue* result;
    int action;
    if ( server == 0 || !xl6_ws_function_valid(&server->Callbacks.Handshake) ) { return 0; }
    memset(&message, 0, sizeof(message));
    message.Kind = 1;
    message.Request = request;
    result = xl6_function_call_var(server->Callbacks.Handshake, 3, xrtValuePointer(peer), xrtValuePointer(&message), xrtValuePointer(response));
    action = result != 0 ? (int)xl6_xrt_value_int(result) : 2;
    xl6_ws_result_release(result);
    if ( action == 1 ) { response->Action = 1; }
    else if ( action != 0 ) { response->Action = 2; }
    return response->Action;
}

static bool xl6_ws_server_reject_response(xhttpconn* connection, const XL6_WsHandshakeResponse* response)
{
    xhttpreply* reply;
    size_t index;
    bool ok;
    uint16_t status = response != 0 && response->Status >= 400 && response->Status <= 599 ? (uint16_t)response->Status : 403u;
    reply = xrtHttpReplyCreate(status);
    if ( reply == 0 ) { return false; }
    if ( response != 0 && response->Reason != 0 && response->Reason[0] != 0 ) { (void)xrtHttpReplySetReason(reply, xl6_ws_view(response->Reason)); }
    if ( response != 0 ) {
        for ( index = 0u; index < xrtHttpHeadersCount(response->Headers); ++index ) {
            const xhttpfield* field = xrtHttpHeadersAt(response->Headers, index);
            if ( field == 0 || !xrtHttpReplyAddHeader(reply, field->Name, field->Value) ) { xrtHttpReplyDestroy(reply); return false; }
        }
    }
    ok = xrtHttpConnRespond(connection, reply) == 0;
    xrtHttpReplyDestroy(reply);
    return ok;
}

static void xl6_ws_server_upgrade_complete(xhttpconn* http, int result, xwsconn* connection, const xerror* error, void* data)
{
    XL6_WsPeer* peer = (XL6_WsPeer*)data;
    xvalue* callbackResult;
    (void)http;
    if ( peer == 0 ) { xrtWsConnDestroy(connection); return; }
    if ( result == 0 && connection != 0 ) {
        peer->Connection = connection;
        if ( peer->Server != 0 && xl6_ws_function_valid(&peer->Server->Callbacks.Open) ) {
            callbackResult = xl6_function_call_var(peer->Server->Callbacks.Open, 1, xrtValuePointer(peer));
            xl6_ws_result_release(callbackResult);
        }
        return;
    }
    xl6_ws_error_set(&peer->LastError, result != 0 ? result : -1, 0, 0u, error != 0 ? error : xrtGetError());
    if ( peer->Server != 0 ) { xl6_ws_error_set(&peer->Server->LastError, peer->LastError.Result, 0, 0u, peer->LastError.Error); }
    xl6_ws_call_peer_error(peer);
    xrtWsConnDestroy(connection);
    xl6_ws_peer_release(peer);
}

static bool xl6_ws_server_upgrade(XL6_WsServer* server, xhttpconn* connection, const xhttpserverrequest* request)
{
    XL6_WsPeer* peer;
    XL6_WsHandshakeResponse response;
    xwsserverconfig config;
    xwsserverhandshake handshake;
    xwsconnevents events;
    int action;
    int result;
    if ( server == 0 || connection == 0 || request == 0 ) { return false; }
    if ( !xl6_ws_origin_allowed(server, request) ) { return xrtHttpConnReply(connection, 403u, xl6_ws_view("text/plain; charset=utf-8"), xl6_ws_bytes("Forbidden", 9u)) == 0; }
    xl6_ws_server_config(&server->Options, &config);
    if ( !xrtWsServerCheck(request, &config, &handshake) ) { return xrtWsServerReject(connection, xrtGetError()) == 0; }
    peer = xl6_ws_peer_create(server);
    if ( peer == 0 || !xl6_ws_handshake_response_init(&response, &handshake) ) { xl6_ws_peer_release(peer); return xrtHttpConnReply(connection, 500u, xl6_ws_view("text/plain; charset=utf-8"), xl6_ws_bytes("Internal Server Error", 21u)) == 0; }
    action = xl6_ws_server_handshake(server, peer, request, &response);
    peer->UserData = response.ConnectionData;
    if ( action == 1 ) {
        result = xl6_ws_server_reject_response(connection, &response);
        xl6_ws_handshake_response_unit(&response);
        xl6_ws_peer_release(peer);
        return result;
    }
    if ( action != 0 ) {
        xl6_ws_handshake_response_unit(&response);
        xl6_ws_peer_release(peer);
        return xrtHttpConnReply(connection, 500u, xl6_ws_view("text/plain; charset=utf-8"), xl6_ws_bytes("Internal Server Error", 21u)) == 0;
    }
    config.Protocols = xl6_ws_view(response.Protocol);
    config.EnableDeflate = response.Compression && server->Options.Compression;
    if ( !xrtWsServerCheck(request, &config, &handshake) ) {
        xl6_ws_handshake_response_unit(&response);
        xl6_ws_peer_release(peer);
        return xrtWsServerReject(connection, xrtGetError()) == 0;
    }
    xl6_ws_peer_events(&events);
    result = xrtWsUpgradeAccept(connection, &config, &handshake, response.Headers, &events, peer, xl6_ws_server_upgrade_complete, peer);
    xl6_ws_handshake_response_unit(&response);
    if ( result != 0 ) {
        xl6_ws_error_set(&server->LastError, result, 0, 0u, xrtGetError());
        xl6_ws_peer_release(peer);
        return false;
    }
    server->Started = true;
    return true;
}

static void xl6_ws_server_route(xhttpserver* nativeServer, xhttpconn* connection, const xhttpserverrequest* request, const xhttprouteparam* params, size_t paramCount, void* data)
{
    (void)nativeServer;
    (void)params;
    (void)paramCount;
    if ( !xl6_ws_server_upgrade((XL6_WsServer*)data, connection, request) ) {
        XL6_WsServer* server = (XL6_WsServer*)data;
        xl6_ws_error_set(server != 0 ? &server->LastError : 0, -1, 0, 0u, xrtGetError());
    }
}

static XL6_WsServer* xl6_ws_server_create(const XL6_WsServerOptions* options, xnetengine* engine)
{
    XL6_WsServer* server;
    if ( options == 0 ) { return 0; }
    server = (XL6_WsServer*)xrtCalloc(1u, sizeof(*server));
    if ( server == 0 ) { return 0; }
    server->References = 1;
    if ( !xl6_ws_server_options_copy(&server->Options, options) ) { xl6_ws_server_release(server); return 0; }
    server->Engine = engine;
    if ( server->Engine == 0 ) {
        server->Engine = (xnetengine*)xl6_xrt_net_engine_create(1);
        server->OwnEngine = server->Engine != 0;
        if ( server->Engine == 0 || !xl6_xrt_net_engine_start(server->Engine) ) { xl6_ws_server_release(server); return 0; }
    }
    if ( server->Options.TlsConfig != 0 ) {
        server->TlsLease = xl6_xrt_tls_lease_create(server->Options.TlsConfig);
        if ( server->TlsLease == 0 || server->TlsLease->Invalid || server->TlsLease->Identity == 0 ) { xl6_ws_server_release(server); return 0; }
    }
    return server;
}

static void xl6_ws_server_release(XL6_WsServer* server)
{
    if ( server == 0 || xrtRefRelease(&server->References) != 0 ) { return; }
    if ( server->Http != 0 ) { xl6_xrt_http_server_stop(server->Http); xl6_xrt_http_server_destroy(server->Http); }
    xl6_xrt_tls_lease_release(server->TlsLease);
    xl6_ws_server_options_unit(&server->Options);
    xl6_ws_callbacks_unit(&server->Callbacks);
    xl6_ws_error_clear(&server->LastError);
    if ( server->OwnEngine ) { (void)xl6_xrt_net_engine_destroy(server->Engine); }
    xrtFree(server);
}

static XL6_WsServer* xl6_ws_server_create_host(long long port, const char* host, xnetengine* engine)
{
    XL6_WsServerOptions options;
    XL6_WsServer* server;
    if ( !xl6_ws_server_options_init(&options, port, host) ) { return 0; }
    server = xl6_ws_server_create(&options, engine);
    xl6_ws_server_options_unit(&options);
    return server;
}

void* xl6_xrt_ws_server_create(void* options) { return xl6_ws_server_create((const XL6_WsServerOptions*)options, 0); }
void* xl6_xrt_ws_server_create_engine(void* options, void* engine) { return engine != 0 ? xl6_ws_server_create((const XL6_WsServerOptions*)options, (xnetengine*)engine) : 0; }
void* xl6_xrt_ws_server_create_host(long long port, const char* host) { return xl6_ws_server_create_host(port, host, 0); }
void* xl6_xrt_ws_server_create_host_engine(long long port, const char* host, void* engine) { return engine != 0 ? xl6_ws_server_create_host(port, host, (xnetengine*)engine) : 0; }
long long xl6_xrt_ws_server_destroy(void* raw) { XL6_WsServer* server = (XL6_WsServer*)raw; if ( server != 0 ) { if ( server->Http != 0 ) { xl6_xrt_http_server_stop(server->Http); xl6_xrt_http_server_destroy(server->Http); server->Http = 0; } xl6_ws_server_release(server); } return 0; }
void* xl6_xrt_ws_server_native(void* raw) { return raw; }

long long xl6_xrt_ws_server_start(void* raw)
{
    XL6_WsServer* server = (XL6_WsServer*)raw;
    uint32_t timeout;
    if ( server == 0 || server->Started || server->Engine == 0 ) { return -1; }
    timeout = server->Options.HandshakeTimeout >= UINT64_C(1000) && server->Options.HandshakeTimeout / UINT64_C(1000) <= UINT32_MAX ? (uint32_t)(server->Options.HandshakeTimeout / UINT64_C(1000)) : 30000u;
    server->Http = xl6_http_server_create_core(server->Engine, false, server->Options.Host, server->Options.Port, server->Options.Backlog, (uint32_t)(server->Options.HandshakeLimit > UINT32_MAX ? UINT32_MAX : server->Options.HandshakeLimit), 1u, 128u, (uint32_t)(server->Options.HandshakeLimit > UINT32_MAX ? UINT32_MAX : server->Options.HandshakeLimit), 1u, timeout, timeout, timeout, timeout, 16u, 1048576u);
    if ( server->Http != 0 && server->TlsLease != 0 ) {
        server->Http->Secure = true;
        xrtHttpServerTlsConfigInit(&server->Http->Tls);
        server->Http->Tls.Handshake.Context = server->TlsLease->Context;
        server->Http->Tls.Handshake.Identity = server->TlsLease->Identity;
        server->Http->Tls.Handshake.Protocols = server->TlsLease->Protocols;
        server->Http->Tls.Handshake.ProtocolCount = server->TlsLease->ProtocolCount;
        server->Http->HasTlsConfig = true;
    }
    if ( server->Http == 0 || server->Http->Default == 0 || !xrtHttpServerRoute(server->Http->Default->Router, xl6_ws_view("GET"), xl6_ws_view(server->Options.Path), xl6_ws_server_route, server) || !xl6_xrt_http_server_start(server->Http) ) {
        xl6_ws_error_set(&server->LastError, -1, 0, 0u, xrtGetError());
        if ( server->Http != 0 ) { xl6_xrt_http_server_destroy(server->Http); server->Http = 0; }
        return -1;
    }
    server->Started = true;
    return 0;
}

void xl6_xrt_ws_server_stop(void* raw) { XL6_WsServer* server = (XL6_WsServer*)raw; if ( server != 0 && server->Http != 0 ) { xl6_xrt_http_server_stop(server->Http); xl6_xrt_http_server_destroy(server->Http); server->Http = 0; } }
long long xl6_xrt_ws_server_port(void* raw) { XL6_WsServer* server = (XL6_WsServer*)raw; return server != 0 && server->Http != 0 ? (long long)xl6_xrt_http_server_port(server->Http) : 0; }
void* xl6_xrt_ws_server_error(void* raw) { XL6_WsServer* server = (XL6_WsServer*)raw; return server != 0 ? xl6_ws_error_clone(&server->LastError) : 0; }

int xl6_xrt_ws_server_on_open(void* raw, xvalue* callback) { XL6_WsServer* server = (XL6_WsServer*)raw; return server != 0 && xl6_ws_function_set_value(&server->Callbacks.Open, callback, server->Started); }
int xl6_xrt_ws_server_on_text(void* raw, xvalue* callback) { XL6_WsServer* server = (XL6_WsServer*)raw; return server != 0 && xl6_ws_function_set_value(&server->Callbacks.Text, callback, server->Started); }
int xl6_xrt_ws_server_on_binary(void* raw, xvalue* callback) { XL6_WsServer* server = (XL6_WsServer*)raw; return server != 0 && xl6_ws_function_set_value(&server->Callbacks.Binary, callback, server->Started); }
int xl6_xrt_ws_server_on_close(void* raw, xvalue* callback) { XL6_WsServer* server = (XL6_WsServer*)raw; return server != 0 && xl6_ws_function_set_value(&server->Callbacks.Close, callback, server->Started); }
int xl6_xrt_ws_server_on_error(void* raw, xvalue* callback) { XL6_WsServer* server = (XL6_WsServer*)raw; return server != 0 && xl6_ws_function_set_value(&server->Callbacks.Error, callback, server->Started); }
int xl6_xrt_ws_server_on_ping(void* raw, xvalue* callback) { XL6_WsServer* server = (XL6_WsServer*)raw; return server != 0 && xl6_ws_function_set_value(&server->Callbacks.Ping, callback, server->Started); }
int xl6_xrt_ws_server_on_pong(void* raw, xvalue* callback) { XL6_WsServer* server = (XL6_WsServer*)raw; return server != 0 && xl6_ws_function_set_value(&server->Callbacks.Pong, callback, server->Started); }
int xl6_xrt_ws_server_on_backpressure(void* raw, xvalue* callback) { XL6_WsServer* server = (XL6_WsServer*)raw; return server != 0 && xl6_ws_function_set_value(&server->Callbacks.Backpressure, callback, server->Started); }
int xl6_xrt_ws_server_on_writable(void* raw, xvalue* callback) { XL6_WsServer* server = (XL6_WsServer*)raw; return server != 0 && xl6_ws_function_set_value(&server->Callbacks.Writable, callback, server->Started); }
int xl6_xrt_ws_server_on_drain(void* raw, xvalue* callback) { XL6_WsServer* server = (XL6_WsServer*)raw; return server != 0 && xl6_ws_function_set_value(&server->Callbacks.Drain, callback, server->Started); }
int xl6_xrt_ws_server_on_handshake(void* raw, xvalue* callback) { XL6_WsServer* server = (XL6_WsServer*)raw; return server != 0 && xl6_ws_function_set_value(&server->Callbacks.Handshake, callback, server->Started); }

void* xl6_xrt_ws_conn_retain(void* raw) { return xl6_ws_peer_retain((XL6_WsPeer*)raw); }
long long xl6_xrt_ws_conn_release(void* raw) { xl6_ws_peer_release((XL6_WsPeer*)raw); return 0; }
void* xl6_xrt_ws_conn_native(void* raw) { XL6_WsPeer* peer = (XL6_WsPeer*)raw; return peer != 0 ? peer->Connection : 0; }
void* xl6_xrt_ws_conn_data(void* raw) { XL6_WsPeer* peer = (XL6_WsPeer*)raw; return peer != 0 ? peer->UserData : 0; }
void xl6_xrt_ws_conn_set_data(void* raw, void* value) { XL6_WsPeer* peer = (XL6_WsPeer*)raw; if ( peer != 0 ) { peer->UserData = value; } }
int xl6_xrt_ws_conn_open(void* raw) { XL6_WsPeer* peer = (XL6_WsPeer*)raw; return peer != 0 && peer->Connection != 0 && xrtWsConnState(peer->Connection) == 0; }
void* xl6_xrt_ws_conn_protocol(void* raw) { XL6_WsPeer* peer = (XL6_WsPeer*)raw; return xl6_ws_copy_view(peer != 0 && peer->Connection != 0 ? xrtWsConnProtocol(peer->Connection) : xl6_ws_view("")); }
int xl6_xrt_ws_conn_compressed(void* raw) { XL6_WsPeer* peer = (XL6_WsPeer*)raw; return peer != 0 && xl6_ws_deflate_enabled(peer->Connection); }
long long xl6_xrt_ws_conn_pending(void* raw) { XL6_WsPeer* peer = (XL6_WsPeer*)raw; return peer != 0 && peer->Connection != 0 ? (long long)xrtWsConnPending(peer->Connection) : 0; }
long long xl6_xrt_ws_conn_send_text(void* raw, const char* data) { XL6_WsPeer* peer = (XL6_WsPeer*)raw; return peer != 0 && peer->Server != 0 ? xl6_ws_send_text(peer->Connection, data, peer->Server->Options.CompressMin, peer->Server->Options.Compression) : -1; }
long long xl6_xrt_ws_conn_send_bytes(void* raw, void* data, size_t size) { XL6_WsPeer* peer = (XL6_WsPeer*)raw; return peer != 0 && peer->Server != 0 ? xl6_ws_send_binary(peer->Connection, data, size, peer->Server->Options.CompressMin, peer->Server->Options.Compression) : -1; }
long long xl6_xrt_ws_conn_ping(void* raw, void* data, size_t size) { XL6_WsPeer* peer = (XL6_WsPeer*)raw; return peer != 0 ? xl6_ws_ping(peer->Connection, data, size) : -1; }
long long xl6_xrt_ws_conn_close(void* raw, long long code, const char* reason) { XL6_WsPeer* peer = (XL6_WsPeer*)raw; return peer != 0 ? xl6_ws_close(peer->Connection, code, reason) : -1; }
long long xl6_xrt_ws_conn_wait(void* raw, long long kind, long long timeout) { XL6_WsPeer* peer = (XL6_WsPeer*)raw; return peer != 0 ? xl6_ws_wait(peer->Connection, kind, timeout) : -1; }
xfuture* xl6_xrt_ws_conn_writable_async(void* raw) { XL6_WsPeer* peer = (XL6_WsPeer*)raw; return peer != 0 ? xl6_ws_future_bool(xrtWsConnWaitAsync(peer->Connection, 1)) : 0; }
xfuture* xl6_xrt_ws_conn_drain_async(void* raw) { XL6_WsPeer* peer = (XL6_WsPeer*)raw; return peer != 0 ? xl6_ws_future_bool(xrtWsConnWaitAsync(peer->Connection, 2)) : 0; }
xfuture* xl6_xrt_ws_conn_close_async(void* raw) { XL6_WsPeer* peer = (XL6_WsPeer*)raw; return peer != 0 ? xl6_ws_future_bool(xrtWsConnWaitAsync(peer->Connection, 3)) : 0; }
void* xl6_xrt_ws_conn_begin_text(void* raw) { XL6_WsPeer* peer = (XL6_WsPeer*)raw; return peer != 0 && peer->Server != 0 ? xl6_ws_writer_create(peer->Connection, 1u, peer->Server->Options.CompressMin, peer->Server->Options.Compression) : 0; }
void* xl6_xrt_ws_conn_begin_binary(void* raw) { XL6_WsPeer* peer = (XL6_WsPeer*)raw; return peer != 0 && peer->Server != 0 ? xl6_ws_writer_create(peer->Connection, 2u, peer->Server->Options.CompressMin, peer->Server->Options.Compression) : 0; }

void* xl6_xrt_ws_conn_close_info(void* raw)
{
    XL6_WsPeer* peer = (XL6_WsPeer*)raw;
    xwsconnclose closeInfo;
    if ( peer == 0 ) { return 0; }
    if ( peer->LastClose.Reason != 0 ) { return xl6_ws_close_clone(&peer->LastClose); }
    if ( peer->Connection == 0 || !xrtWsConnCloseInfo(peer->Connection, &closeInfo) ) { return 0; }
    {
        XL6_WsCloseSnapshot snapshot;
        memset(&snapshot, 0, sizeof(snapshot));
        if ( !xl6_ws_close_set(&snapshot, &closeInfo) ) { return 0; }
        {
            XL6_WsCloseSnapshot* result = xl6_ws_close_clone(&snapshot);
            xl6_ws_close_clear(&snapshot);
            return result;
        }
    }
}

void* xl6_xrt_ws_conn_error(void* raw)
{
    XL6_WsPeer* peer = (XL6_WsPeer*)raw;
    XL6_WsErrorSnapshot snapshot;
    if ( peer == 0 ) { return 0; }
    if ( peer->LastError.Error != 0 ) { return xl6_ws_error_clone(&peer->LastError); }
    memset(&snapshot, 0, sizeof(snapshot));
    xl6_ws_error_set(&snapshot, -1, 0, 0u, peer->Connection != 0 ? xrtWsConnError(peer->Connection) : 0);
    {
        XL6_WsErrorSnapshot* result = xl6_ws_error_clone(&snapshot);
        xl6_ws_error_clear(&snapshot);
        return result;
    }
}
) "\n";



/* HttpResponse 的升级入口复用同一服务端配置，不创建中间响应对象。 */
