static const char xl6HttpServerPublicSource[] = XL6_HTTP_SERVER_SOURCE(
static void xl6_http_server_configure(XL6_Http_Server* server, const char* host, int port, uint32_t backlog, uint32_t recvLimit, uint32_t bodyLimit, uint32_t headerLimit, uint32_t headerBytesLimit, uint32_t headerTimeoutMs, uint32_t bodyTimeoutMs, uint32_t idleTimeoutMs, uint32_t writeTimeoutMs)
{
    xrtHttpServerConfigInit(&server->Config);
    (void)xl6_net_address_parse(&server->Config.Network.Listen.Address, host != 0 && host[0] != 0 ? host : "127.0.0.1", (uint16_t)port, true);
    server->Config.Network.Listen.Backlog = (int)backlog;
    if ( recvLimit != 0u ) {
        server->Config.Network.Listen.Stream.ReadLimit = recvLimit;
        if ( server->Config.Network.Listen.Stream.ReadSize > recvLimit ) {
            server->Config.Network.Listen.Stream.ReadSize = recvLimit;
        }
    }
    server->Config.Http1.Body.MaxBody = bodyLimit;
    server->Config.Http1.Head.MaxFields = headerLimit;
    server->Config.Http1.Head.MaxHead = headerBytesLimit;
    server->Config.HeaderTimeout = xl6_http_timeout_us(headerTimeoutMs);
    server->Config.BodyTimeout = xl6_http_timeout_us(bodyTimeoutMs);
    server->Config.IdleTimeout = xl6_http_timeout_us(idleTimeoutMs);
    server->Config.WriteTimeout = xl6_http_timeout_us(writeTimeoutMs);
}

static void xl6_http_server_dispose(XL6_Http_Server* server)
{
    XL6_Http_Host* host;
    XL6_Http_Tls_Host* tlsHost;
    if ( server == 0 ) { return; }
    host = server->Hosts;
    while ( host != 0 ) { XL6_Http_Host* next = host->Next; xl6_http_router_release(host->Shared); xrtFree(host->Host); xrtFree(host); host = next; }
    tlsHost = server->TlsHosts;
    while ( tlsHost != 0 ) { XL6_Http_Tls_Host* next = tlsHost->Next; xrtTlsIdentityRelease(tlsHost->Identity); xrtFree(tlsHost->Host); xrtFree(tlsHost); tlsHost = next; }
    xl6_http_router_release(server->Default);
    xrtTlsIdentityRelease(server->Identity);
    if ( server->Pool != 0 ) { (void)xrtTaskPoolDestroy(server->Pool); }
    if ( server->OwnEngine && server->Engine != 0 ) { (void)xrtNetEngineStop(server->Engine); (void)xrtNetEngineDestroy(server->Engine); }
    xrtFree(server);
}

static XL6_Http_Server* xl6_http_server_create_core(xnetengine* engine, bool ownEngine, const char* host, int port, uint32_t backlog, uint32_t recvLimit, uint32_t bodyLimit, uint32_t headerLimit, uint32_t headerBytesLimit, uint32_t workerCount, uint32_t headerTimeoutMs, uint32_t bodyTimeoutMs, uint32_t idleTimeoutMs, uint32_t writeTimeoutMs, uint32_t multipartPartLimit, uint32_t multipartPartSizeLimit)
{
    XL6_Http_Server* server;
    xtaskpoolconfig poolConfig;
    if ( port < 0 || port > 65535 || backlog > INT_MAX ) { return 0; }
    server = (XL6_Http_Server*)xrtCalloc(1u, sizeof(*server));
    if ( server == 0 ) { return 0; }
    server->Default = xl6_http_router_create();
    if ( ownEngine ) {
        xnetengineconfig engineConfig;
        xrtNetEngineConfigInit(&engineConfig);
        engineConfig.Workers = workerCount;
        server->Engine = xrtNetEngineCreate(&engineConfig);
        server->OwnEngine = true;
        if ( server->Engine == 0 || !xrtNetEngineStart(server->Engine) ) { xl6_http_server_dispose(server); return 0; }
    } else { server->Engine = engine; }
    poolConfig.Threads = workerCount != 0u ? workerCount : 1u;
    poolConfig.QueueLimit = 1024u;
    poolConfig.StackSize = 0u;
    server->Pool = xrtTaskPoolCreate(&poolConfig);
    server->MultipartParts = multipartPartLimit;
    server->MultipartPartBytes = multipartPartSizeLimit;
    xl6_http_server_configure(server, host, port, backlog, recvLimit, bodyLimit, headerLimit, headerBytesLimit, headerTimeoutMs, bodyTimeoutMs, idleTimeoutMs, writeTimeoutMs);
    if ( server->Default == 0 || server->Engine == 0 || server->Pool == 0 ) { xl6_http_server_dispose(server); return 0; }
    return server;
}

void* xl6_xrt_http_server_create(const char* host, int port, uint32_t backlog, uint32_t recvLimit, uint32_t bodyLimit, uint32_t headerLimit, uint32_t headerBytesLimit, uint32_t workerCount, uint32_t headerTimeoutMs, uint32_t bodyTimeoutMs, uint32_t idleTimeoutMs, uint32_t writeTimeoutMs, uint32_t multipartPartLimit, uint32_t multipartPartSizeLimit)
{
    return xl6_http_server_create_core(0, true, host, port, backlog, recvLimit, bodyLimit, headerLimit, headerBytesLimit, workerCount, headerTimeoutMs, bodyTimeoutMs, idleTimeoutMs, writeTimeoutMs, multipartPartLimit, multipartPartSizeLimit);
}

void* xl6_xrt_http_server_create_engine(void* engine, const char* host, int port, uint32_t backlog, uint32_t recvLimit, uint32_t bodyLimit, uint32_t headerLimit, uint32_t headerBytesLimit, uint32_t workerCount, uint32_t headerTimeoutMs, uint32_t bodyTimeoutMs, uint32_t idleTimeoutMs, uint32_t writeTimeoutMs, uint32_t multipartPartLimit, uint32_t multipartPartSizeLimit)
{
    return engine != 0 ? xl6_http_server_create_core((xnetengine*)engine, false, host, port, backlog, recvLimit, bodyLimit, headerLimit, headerBytesLimit, workerCount, headerTimeoutMs, bodyTimeoutMs, idleTimeoutMs, writeTimeoutMs, multipartPartLimit, multipartPartSizeLimit) : 0;
}

static void* xl6_http_server_create_tls_core(xnetengine* engine, bool ownEngine, const char* certificatePath, const char* keyPath, const char* host, int port, uint32_t backlog, uint32_t recvLimit, uint32_t bodyLimit, uint32_t headerLimit, uint32_t headerBytesLimit, uint32_t workerCount, uint32_t headerTimeoutMs, uint32_t bodyTimeoutMs, uint32_t idleTimeoutMs, uint32_t writeTimeoutMs, uint32_t multipartPartLimit, uint32_t multipartPartSizeLimit)
{
    XL6_Http_Server* server = xl6_http_server_create_core(engine, ownEngine, host, port, backlog, recvLimit, bodyLimit, headerLimit, headerBytesLimit, workerCount, headerTimeoutMs, bodyTimeoutMs, idleTimeoutMs, writeTimeoutMs, multipartPartLimit, multipartPartSizeLimit);
    if ( server == 0 ) { return 0; }
    server->Identity = xl6_http_identity_load(certificatePath, keyPath);
    if ( server->Identity == 0 ) { xl6_http_server_dispose(server); return 0; }
    server->Secure = true;
    return server;
}

void* xl6_xrt_http_server_create_tls(const char* certificatePath, const char* keyPath, const char* host, int port, uint32_t backlog, uint32_t recvLimit, uint32_t bodyLimit, uint32_t headerLimit, uint32_t headerBytesLimit, uint32_t workerCount, uint32_t headerTimeoutMs, uint32_t bodyTimeoutMs, uint32_t idleTimeoutMs, uint32_t writeTimeoutMs, uint32_t multipartPartLimit, uint32_t multipartPartSizeLimit)
{
    return xl6_http_server_create_tls_core(0, true, certificatePath, keyPath, host, port, backlog, recvLimit, bodyLimit, headerLimit, headerBytesLimit, workerCount, headerTimeoutMs, bodyTimeoutMs, idleTimeoutMs, writeTimeoutMs, multipartPartLimit, multipartPartSizeLimit);
}

void* xl6_xrt_http_server_create_tls_engine(void* engine, const char* certificatePath, const char* keyPath, const char* host, int port, uint32_t backlog, uint32_t recvLimit, uint32_t bodyLimit, uint32_t headerLimit, uint32_t headerBytesLimit, uint32_t workerCount, uint32_t headerTimeoutMs, uint32_t bodyTimeoutMs, uint32_t idleTimeoutMs, uint32_t writeTimeoutMs, uint32_t multipartPartLimit, uint32_t multipartPartSizeLimit)
{
    return engine != 0 ? xl6_http_server_create_tls_core((xnetengine*)engine, false, certificatePath, keyPath, host, port, backlog, recvLimit, bodyLimit, headerLimit, headerBytesLimit, workerCount, headerTimeoutMs, bodyTimeoutMs, idleTimeoutMs, writeTimeoutMs, multipartPartLimit, multipartPartSizeLimit) : 0;
}

void* xl6_xrt_http_app_create(void)
{
    XL6_Http_App* app = (XL6_Http_App*)xrtCalloc(1u, sizeof(*app));
    if ( app == 0 ) { return 0; }
    app->Shared = xl6_http_router_create();
    if ( app->Shared == 0 ) { xrtFree(app); return 0; }
    return app;
}

void xl6_xrt_http_app_destroy(void* raw)
{
    XL6_Http_App* app = (XL6_Http_App*)raw;
    if ( app == 0 ) { return; }
    xl6_http_router_release(app->Shared);
    xrtFree(app);
}

bool xl6_xrt_http_app_route(void* raw, const char* method, const char* pattern, XL6_Function handler) { XL6_Http_App* app = (XL6_Http_App*)raw; return app != 0 && xl6_http_add_handler(app->Shared, method, pattern, handler, XL6_HTTP_ROUTE_TEXT); }
bool xl6_xrt_http_app_route_request(void* raw, const char* method, const char* pattern, XL6_Function handler) { XL6_Http_App* app = (XL6_Http_App*)raw; return app != 0 && xl6_http_add_handler(app->Shared, method, pattern, handler, XL6_HTTP_ROUTE_OBJECT); }
bool xl6_xrt_http_app_get(void* raw, const char* pattern, XL6_Function handler) { return xl6_xrt_http_app_route(raw, "GET", pattern, handler); }
bool xl6_xrt_http_app_post(void* raw, const char* pattern, XL6_Function handler) { return xl6_xrt_http_app_route(raw, "POST", pattern, handler); }
bool xl6_xrt_http_app_any(void* raw, const char* pattern, XL6_Function handler) { return xl6_xrt_http_app_route(raw, "ANY", pattern, handler); }
bool xl6_xrt_http_app_get_request(void* raw, const char* pattern, XL6_Function handler) { return xl6_xrt_http_app_route_request(raw, "GET", pattern, handler); }
bool xl6_xrt_http_app_post_request(void* raw, const char* pattern, XL6_Function handler) { return xl6_xrt_http_app_route_request(raw, "POST", pattern, handler); }
bool xl6_xrt_http_app_any_request(void* raw, const char* pattern, XL6_Function handler) { return xl6_xrt_http_app_route_request(raw, "ANY", pattern, handler); }
bool xl6_xrt_http_app_route_stream_request(void* raw, const char* method, const char* pattern, XL6_Function beginHandler, XL6_Function chunkHandler, XL6_Function endHandler) { XL6_Http_App* app = (XL6_Http_App*)raw; return app != 0 && xl6_http_add_stream(app->Shared, method, pattern, beginHandler, chunkHandler, endHandler); }
bool xl6_xrt_http_app_post_stream_request(void* raw, const char* pattern, XL6_Function beginHandler, XL6_Function chunkHandler, XL6_Function endHandler) { return xl6_xrt_http_app_route_stream_request(raw, "POST", pattern, beginHandler, chunkHandler, endHandler); }
bool xl6_xrt_http_app_any_stream_request(void* raw, const char* pattern, XL6_Function beginHandler, XL6_Function chunkHandler, XL6_Function endHandler) { return xl6_xrt_http_app_route_stream_request(raw, "ANY", pattern, beginHandler, chunkHandler, endHandler); }
bool xl6_xrt_http_app_static(void* raw, const char* mount, const char* root) { XL6_Http_App* app = (XL6_Http_App*)raw; return app != 0 && xl6_http_add_static(app->Shared, mount, root, "index.html", "", 65536u, 1u); }
bool xl6_xrt_http_app_static_ex(void* raw, const char* mount, const char* root, const char* index, const char* cacheControl, size_t chunkSize, uint32_t flags) { XL6_Http_App* app = (XL6_Http_App*)raw; return app != 0 && xl6_http_add_static(app->Shared, mount, root, index, cacheControl, chunkSize, flags); }
bool xl6_xrt_http_app_error(void* raw, XL6_Function handler) { XL6_Http_App* app = (XL6_Http_App*)raw; return app != 0 && xl6_http_set_error(app->Shared, handler); }

static bool xl6_http_server_set_default(XL6_Http_Server* server, XL6_Http_Router* shared)
{
    XL6_Http_Router* old;
    if ( server == 0 || shared == 0 ) { return false; }
    if ( server->Native != 0 ) {
        xl6_http_router_bind(shared, server);
        if ( !xrtHttpServerRouterFreeze(shared->Router) || !xrtHttpServerMuxDefault(server->Mux, shared->Router) ) { return false; }
    }
    old = server->Default;
    server->Default = xl6_http_router_retain(shared);
    xl6_http_router_release(old);
    return true;
}

bool xl6_xrt_http_server_set_app(void* serverRaw, void* appRaw)
{
    XL6_Http_Server* server = (XL6_Http_Server*)serverRaw;
    XL6_Http_App* app = (XL6_Http_App*)appRaw;
    return app != 0 && xl6_http_server_set_default(server, app->Shared);
}

bool xl6_xrt_http_server_reload_app(void* serverRaw, void* appRaw) { return xl6_xrt_http_server_set_app(serverRaw, appRaw); }

bool xl6_xrt_http_server_host(void* serverRaw, const char* name, void* appRaw)
{
    XL6_Http_Server* server = (XL6_Http_Server*)serverRaw;
    XL6_Http_App* app = (XL6_Http_App*)appRaw;
    XL6_Http_Host* host;
    if ( server == 0 || app == 0 || name == 0 || name[0] == 0 ) { return false; }
    host = server->Hosts;
    while ( host != 0 ) {
        if ( xl6_http_ascii_equal(host->Host, (const unsigned char*)name, strlen(name)) ) {
            if ( server->Native != 0 ) { xl6_http_router_bind(app->Shared, server); if ( !xrtHttpServerRouterFreeze(app->Shared->Router) || !xrtHttpServerMuxHost(server->Mux, xl6_http_view(name), app->Shared->Router) ) { return false; } }
            xl6_http_router_release(host->Shared); host->Shared = xl6_http_router_retain(app->Shared); return true;
        }
        host = host->Next;
    }
    host = (XL6_Http_Host*)xrtCalloc(1u, sizeof(*host));
    if ( host == 0 ) { return false; }
    host->Host = xl6_xrt_string_dup(name, 0u);
    host->Shared = xl6_http_router_retain(app->Shared);
    if ( host->Host == 0 ) { xl6_http_router_release(host->Shared); xrtFree(host); return false; }
    if ( server->Native != 0 ) { xl6_http_router_bind(app->Shared, server); if ( !xrtHttpServerRouterFreeze(app->Shared->Router) || !xrtHttpServerMuxHost(server->Mux, xl6_http_view(name), app->Shared->Router) ) { xl6_http_router_release(host->Shared); xrtFree(host->Host); xrtFree(host); return false; } }
    host->Next = server->Hosts; server->Hosts = host;
    return true;
}

bool xl6_xrt_http_server_remove_host(void* raw, const char* name)
{
    XL6_Http_Server* server = (XL6_Http_Server*)raw;
    XL6_Http_Host** link;
    if ( server == 0 || name == 0 ) { return false; }
    link = &server->Hosts;
    while ( *link != 0 ) {
        XL6_Http_Host* host = *link;
        if ( xl6_http_ascii_equal(host->Host, (const unsigned char*)name, strlen(name)) ) {
            if ( server->Native != 0 && xrtHttpServerMuxRemove(server->Mux, xl6_http_view(name)) < 0 ) { return false; }
            *link = host->Next; xl6_http_router_release(host->Shared); xrtFree(host->Host); xrtFree(host); return true;
        }
        link = &host->Next;
    }
    return false;
}

bool xl6_xrt_http_server_tls_host(void* raw, const char* name, const char* certificatePath, const char* keyPath)
{
    XL6_Http_Server* server = (XL6_Http_Server*)raw;
    XL6_Http_Tls_Host* host;
    xtlsidentity* identity;
    if ( server == 0 || !server->Secure || server->Native != 0 || name == 0 ) { return false; }
    identity = xl6_http_identity_load(certificatePath, keyPath);
    if ( identity == 0 ) { return false; }
    host = server->TlsHosts;
    while ( host != 0 ) {
        if ( xl6_http_ascii_equal(host->Host, (const unsigned char*)name, strlen(name)) ) { xrtTlsIdentityRelease(host->Identity); host->Identity = identity; return true; }
        host = host->Next;
    }
    host = (XL6_Http_Tls_Host*)xrtCalloc(1u, sizeof(*host));
    if ( host == 0 ) { xrtTlsIdentityRelease(identity); return false; }
    host->Host = xl6_xrt_string_dup(name, 0u); host->Identity = identity;
    if ( host->Host == 0 ) { xrtTlsIdentityRelease(identity); xrtFree(host); return false; }
    host->Next = server->TlsHosts; server->TlsHosts = host;
    return true;
}

bool xl6_xrt_http_server_remove_tls_host(void* raw, const char* name)
{
    XL6_Http_Server* server = (XL6_Http_Server*)raw;
    XL6_Http_Tls_Host** link;
    if ( server == 0 || server->Native != 0 || name == 0 ) { return false; }
    link = &server->TlsHosts;
    while ( *link != 0 ) {
        XL6_Http_Tls_Host* host = *link;
        if ( xl6_http_ascii_equal(host->Host, (const unsigned char*)name, strlen(name)) ) { *link = host->Next; xrtTlsIdentityRelease(host->Identity); xrtFree(host->Host); xrtFree(host); return true; }
        link = &host->Next;
    }
    return false;
}

bool xl6_xrt_http_server_error(void* raw, XL6_Function handler) { XL6_Http_Server* server = (XL6_Http_Server*)raw; return server != 0 && xl6_http_set_error(server->Default, handler); }
bool xl6_xrt_http_server_route(void* raw, const char* method, const char* pattern, XL6_Function handler) { XL6_Http_Server* server = (XL6_Http_Server*)raw; return server != 0 && xl6_http_add_handler(server->Default, method, pattern, handler, XL6_HTTP_ROUTE_TEXT); }
bool xl6_xrt_http_server_route_request(void* raw, const char* method, const char* pattern, XL6_Function handler) { XL6_Http_Server* server = (XL6_Http_Server*)raw; return server != 0 && xl6_http_add_handler(server->Default, method, pattern, handler, XL6_HTTP_ROUTE_OBJECT); }
bool xl6_xrt_http_server_get(void* raw, const char* pattern, XL6_Function handler) { return xl6_xrt_http_server_route(raw, "GET", pattern, handler); }
bool xl6_xrt_http_server_post(void* raw, const char* pattern, XL6_Function handler) { return xl6_xrt_http_server_route(raw, "POST", pattern, handler); }
bool xl6_xrt_http_server_any(void* raw, const char* pattern, XL6_Function handler) { return xl6_xrt_http_server_route(raw, "ANY", pattern, handler); }
bool xl6_xrt_http_server_get_request(void* raw, const char* pattern, XL6_Function handler) { return xl6_xrt_http_server_route_request(raw, "GET", pattern, handler); }
bool xl6_xrt_http_server_post_request(void* raw, const char* pattern, XL6_Function handler) { return xl6_xrt_http_server_route_request(raw, "POST", pattern, handler); }
bool xl6_xrt_http_server_any_request(void* raw, const char* pattern, XL6_Function handler) { return xl6_xrt_http_server_route_request(raw, "ANY", pattern, handler); }
bool xl6_xrt_http_server_route_stream_request(void* raw, const char* method, const char* pattern, XL6_Function beginHandler, XL6_Function chunkHandler, XL6_Function endHandler) { XL6_Http_Server* server = (XL6_Http_Server*)raw; return server != 0 && xl6_http_add_stream(server->Default, method, pattern, beginHandler, chunkHandler, endHandler); }
bool xl6_xrt_http_server_post_stream_request(void* raw, const char* pattern, XL6_Function beginHandler, XL6_Function chunkHandler, XL6_Function endHandler) { return xl6_xrt_http_server_route_stream_request(raw, "POST", pattern, beginHandler, chunkHandler, endHandler); }
bool xl6_xrt_http_server_any_stream_request(void* raw, const char* pattern, XL6_Function beginHandler, XL6_Function chunkHandler, XL6_Function endHandler) { return xl6_xrt_http_server_route_stream_request(raw, "ANY", pattern, beginHandler, chunkHandler, endHandler); }
bool xl6_xrt_http_server_static(void* raw, const char* mount, const char* root) { XL6_Http_Server* server = (XL6_Http_Server*)raw; return server != 0 && xl6_http_add_static(server->Default, mount, root, "index.html", "", 65536u, 1u); }
bool xl6_xrt_http_server_static_ex(void* raw, const char* mount, const char* root, const char* index, const char* cacheControl, size_t chunkSize, uint32_t flags) { XL6_Http_Server* server = (XL6_Http_Server*)raw; return server != 0 && xl6_http_add_static(server->Default, mount, root, index, cacheControl, chunkSize, flags); }

bool xl6_xrt_http_server_start(void* raw)
{
    XL6_Http_Server* server = (XL6_Http_Server*)raw;
    XL6_Http_Host* host;
    if ( server == 0 || server->Native != 0 || server->Default == 0 ) { return false; }
    server->Mux = xrtHttpServerMuxCreate(0);
    if ( server->Mux == 0 ) { return false; }
    xl6_http_router_bind(server->Default, server);
    if ( !xrtHttpServerRouterFreeze(server->Default->Router) || !xrtHttpServerMuxDefault(server->Mux, server->Default->Router) ) { xrtHttpServerMuxDestroy(server->Mux); server->Mux = 0; return false; }
    host = server->Hosts;
    while ( host != 0 ) {
        xl6_http_router_bind(host->Shared, server);
        if ( !xrtHttpServerRouterFreeze(host->Shared->Router) || !xrtHttpServerMuxHost(server->Mux, xl6_http_view(host->Host), host->Shared->Router) ) { xrtHttpServerMuxDestroy(server->Mux); server->Mux = 0; return false; }
        host = host->Next;
    }
    if ( server->Secure ) {
        xhttpservertlsconfig tls;
        if ( server->HasTlsConfig ) { tls = server->Tls; }
        else { xrtHttpServerTlsConfigInit(&tls); tls.Handshake.Identity = server->Identity; }
        if ( server->TlsHosts != 0 ) { tls.Handshake.Select = xl6_http_tls_select; tls.Handshake.SelectContext = server; }
        server->Native = xrtHttpServerMuxStartTls(server->Engine, &server->Config, &tls, server->Mux, 0);
    } else { server->Native = xrtHttpServerMuxStart(server->Engine, &server->Config, server->Mux, 0); }
    if ( server->Native == 0 ) { xrtHttpServerMuxDestroy(server->Mux); server->Mux = 0; return false; }
    return true;
}

int xl6_xrt_http_server_port(void* raw)
{
    XL6_Http_Server* server = (XL6_Http_Server*)raw;
    xnetaddr address;
    return server != 0 && server->Native != 0 && xrtHttpServerLocal(server->Native, 0u, &address) ? (int)address.Port : 0;
}

int xl6_xrt_http_server_request_count(void* raw)
{
    XL6_Http_Server* server = (XL6_Http_Server*)raw;
    xhttpserverstats stats;
    if ( server == 0 || server->Native == 0 || !xrtHttpServerStats(server->Native, &stats) ) { return 0; }
    return stats.Requests > INT_MAX ? INT_MAX : (int)stats.Requests;
}

int xl6_xrt_http_server_connection_count(void* raw)
{
    XL6_Http_Server* server = (XL6_Http_Server*)raw;
    xhttpserverstats stats;
    if ( server == 0 || server->Native == 0 || !xrtHttpServerStats(server->Native, &stats) ) { return 0; }
    return stats.Connections > INT_MAX ? INT_MAX : (int)stats.Connections;
}

static bool xl6_http_server_wait(XL6_Http_Server* server, long long timeoutMs)
{
    uint64_t waited = 0u;
    if ( server == 0 || server->Native == 0 ) { return true; }
    while ( xrtHttpServerState(server->Native) != 3 ) {
        if ( timeoutMs >= 0 && waited >= (uint64_t)timeoutMs ) { return false; }
        xrtSleep(1u);
        waited++;
    }
    return true;
}

bool xl6_xrt_http_server_drain(void* raw, long long timeoutMs)
{
    XL6_Http_Server* server = (XL6_Http_Server*)raw;
    if ( server == 0 || server->Native == 0 ) { return true; }
    if ( !xrtHttpServerDrain(server->Native) ) { return false; }
    if ( xl6_http_server_wait(server, timeoutMs) ) { return true; }
    (void)xrtHttpServerAbort(server->Native);
    return false;
}

void xl6_xrt_http_server_stop(void* raw)
{
    XL6_Http_Server* server = (XL6_Http_Server*)raw;
    if ( server == 0 || server->Native == 0 ) { return; }
    (void)xrtHttpServerAbort(server->Native);
    (void)xl6_http_server_wait(server, 5000);
}

void xl6_xrt_http_server_destroy(void* raw)
{
    XL6_Http_Server* server = (XL6_Http_Server*)raw;
    if ( server == 0 ) { return; }
    if ( server->Native != 0 ) {
        if ( xrtHttpServerState(server->Native) != 3 ) { (void)xrtHttpServerAbort(server->Native); (void)xl6_http_server_wait(server, 5000); }
        xrtHttpServerDestroy(server->Native);
        server->Native = 0;
    }
    xrtHttpServerMuxDestroy(server->Mux);
    server->Mux = 0;
    xl6_http_server_dispose(server);
}
    ) "\n";
