static const char xl6HttpServerStaticSource[] = XL6_HTTP_SERVER_SOURCE(
static bool xl6_http_static_indexes(XL6_Http_Static* value, const char* names)
{
    size_t size;
    size_t count;
    size_t start;
    size_t index;
    if ( value == 0 || names == 0 ) { return false; }
    size = strlen(names);
    value->IndexStorage = xl6_xrt_string_dup(names, size);
    if ( value->IndexStorage == 0 ) { return false; }
    if ( size == 0u ) { return true; }
    count = 1u;
    for ( index = 0u; index < size; index++ ) {
        if ( names[index] == ';' ) {
            if ( index == 0u || names[index - 1u] == ';' || count == SIZE_MAX ) { return false; }
            count++;
        }
    }
    if ( names[size - 1u] == ';' || count > SIZE_MAX / sizeof(xstrview) ) { return false; }
    value->Indexes = (xstrview*)xrtCalloc(count, sizeof(xstrview));
    if ( value->Indexes == 0 ) { return false; }
    start = 0u;
    count = 0u;
    for ( index = 0u; index <= size; index++ ) {
        if ( index == size || value->IndexStorage[index] == ';' ) {
            value->Indexes[count].Data = value->IndexStorage + start;
            value->Indexes[count].Size = index - start;
            if ( index != size ) { value->IndexStorage[index] = 0; }
            count++;
            start = index + 1u;
        }
    }
    value->IndexCount = count;
    return true;
}

static XL6_Http_Static* xl6_http_static_create(const char* mount, const char* root, const char* index, const char* cacheControl, uint32_t flags)
{
    XL6_Http_Static* value;
    if ( mount == 0 || root == 0 ) { return 0; }
    value = (XL6_Http_Static*)xrtCalloc(1u, sizeof(*value));
    if ( value == 0 ) { return 0; }
    value->Mount = xl6_xrt_string_dup(mount, 0u);
    value->CacheControl = xl6_xrt_string_dup(cacheControl != 0 ? cacheControl : "", 0u);
    value->Root = xrtRootOpen(root);
    if ( value->Mount == 0 || value->CacheControl == 0 || value->Root == 0 || !xl6_http_static_indexes(value, index != 0 ? index : "") ) {
        if ( value->Root != 0 ) { (void)xrtRootClose(value->Root); }
        xrtFree(value->Mount); xrtFree(value->IndexStorage); xrtFree(value->Indexes); xrtFree(value->CacheControl); xrtFree(value);
        return 0;
    }
    xrtHttpStaticServeConfigInit(&value->Config);
    value->Config.Path.Mount = xl6_http_view(value->Mount);
    value->Config.Path.Flags = flags;
    value->Config.Reply.CacheControl = xl6_http_view(value->CacheControl);
    value->Config.Indexes = value->Indexes;
    value->Config.IndexCount = value->IndexCount;
    return value;
}

static bool xl6_http_static_route(XL6_Http_Router* shared, const char* pattern, XL6_Http_Static* value)
{
    XL6_Http_Route* route = xl6_http_route_create(XL6_HTTP_ROUTE_STATIC);
    if ( route == 0 ) { return false; }
    route->Static = value;
    if ( !xl6_http_router_add(shared, "ANY", pattern, route) ) { xl6_http_route_release(route); return false; }
    return true;
}

static bool xl6_http_add_static(XL6_Http_Router* shared, const char* mount, const char* root, const char* index, const char* cacheControl, size_t chunkSize, uint32_t flags)
{
    XL6_Http_Static* exact;
    XL6_Http_Static* children;
    char* pattern;
    size_t mountSize;
    size_t suffixSize = strlen("/{path...}");
    (void)chunkSize;
    if ( shared == 0 || mount == 0 || root == 0 || mount[0] != '/' ) { return false; }
    mountSize = strlen(mount);
    if ( mountSize == 0u || (mountSize > 1u && mount[mountSize - 1u] == '/') || mountSize > SIZE_MAX - suffixSize - 1u ) { return false; }
    exact = xl6_http_static_create(mount, root, index, cacheControl, flags);
    if ( exact == 0 || !xl6_http_static_route(shared, mount, exact) ) { return false; }
    children = xl6_http_static_create(mount, root, index, cacheControl, flags);
    if ( children == 0 ) { return false; }
    pattern = (char*)xrtMalloc(mountSize + suffixSize + 1u);
    if ( pattern == 0 ) {
        if ( children->Root != 0 ) { (void)xrtRootClose(children->Root); }
        xrtFree(children->Mount); xrtFree(children->IndexStorage); xrtFree(children->Indexes); xrtFree(children->CacheControl); xrtFree(children);
        return false;
    }
    memcpy(pattern, mount, mountSize);
    if ( mountSize == 1u ) { memcpy(pattern + mountSize, "{path...}", suffixSize); pattern[mountSize + suffixSize - 1u] = 0; }
    else { memcpy(pattern + mountSize, "/{path...}", suffixSize + 1u); }
    if ( !xl6_http_static_route(shared, pattern, children) ) { xrtFree(pattern); return false; }
    xrtFree(pattern);
    return true;
}
    ) "\n";



/* PEM 身份导入与 SNI 选择保持在 xlang 便捷层，TLS 协议仍完全由 XRT 驱动。 */
static const char xl6HttpServerTlsSource[] = XL6_HTTP_SERVER_SOURCE(
static xtlsidentity* xl6_http_identity_load(const char* certificatePath, const char* keyPath)
{
    unsigned char* certificateText = 0;
    unsigned char* keyText = 0;
    size_t certificateTextSize = 0u;
    size_t keyTextSize = 0u;
    xpemcursor cursor;
    xpemblock block;
    xbytesview* certificates = 0;
    unsigned char** owned = 0;
    size_t count = 0u;
    size_t index = 0u;
    unsigned char* key = 0;
    size_t keySize = 0u;
    xtlsidentity* identity = 0;
    if ( certificatePath == 0 || keyPath == 0 ) { return 0; }
    certificateText = xrtFileReadAll(certificatePath, &certificateTextSize);
    keyText = xrtFileReadAll(keyPath, &keyTextSize);
    if ( certificateText == 0 || keyText == 0 || !xrtPemInit(&cursor, (const char*)certificateText, certificateTextSize) ) { goto cleanup; }
    while ( xrtPemRead(&cursor, &block) == 1 ) { if ( xl6_http_view_equal_text(block.Label, "CERTIFICATE") ) { count++; } }
    if ( count == 0u || count > SIZE_MAX / sizeof(xbytesview) ) { goto cleanup; }
    certificates = (xbytesview*)xrtCalloc(count, sizeof(*certificates));
    owned = (unsigned char**)xrtCalloc(count, sizeof(*owned));
    if ( certificates == 0 || owned == 0 || !xrtPemInit(&cursor, (const char*)certificateText, certificateTextSize) ) { goto cleanup; }
    while ( index < count && xrtPemRead(&cursor, &block) == 1 ) {
        if ( !xl6_http_view_equal_text(block.Label, "CERTIFICATE") ) { continue; }
        owned[index] = xrtPemDecodeNew(&block, &certificates[index].Size);
        certificates[index].Data = owned[index];
        if ( owned[index] == 0 ) { goto cleanup; }
        index++;
    }
    if ( index != count || !xrtPemInit(&cursor, (const char*)keyText, keyTextSize) ) { goto cleanup; }
    while ( xrtPemRead(&cursor, &block) == 1 ) {
        if ( xl6_http_view_equal_text(block.Label, "RSA PRIVATE KEY") || xl6_http_view_equal_text(block.Label, "EC PRIVATE KEY") || xl6_http_view_equal_text(block.Label, "PRIVATE KEY") ) { key = xrtPemDecodeNew(&block, &keySize); break; }
    }
    if ( key == 0 ) { goto cleanup; }
    identity = xrtTlsIdentityRsa(certificates, count, xl6_http_bytes(key, keySize));
    if ( identity == 0 ) { identity = xrtTlsIdentityP256(certificates, count, xl6_http_bytes(key, keySize)); }
    if ( identity == 0 ) { identity = xrtTlsIdentityP384(certificates, count, xl6_http_bytes(key, keySize)); }
    if ( identity == 0 ) { identity = xrtTlsIdentityEd25519(certificates, count, xl6_http_bytes(key, keySize)); }
cleanup:
    if ( owned != 0 ) { for ( index = 0u; index < count; index++ ) { xrtFree(owned[index]); } }
    xrtFree(owned); xrtFree(certificates); xrtFree(key); xrtFree(certificateText); xrtFree(keyText);
    return identity;
}

static bool xl6_http_tls_select(void* context, const xtlsserverrequest* request, xtlsserverchoice* choice)
{
    XL6_Http_Server* server = (XL6_Http_Server*)context;
    XL6_Http_Tls_Host* host;
    if ( server == 0 || request == 0 || choice == 0 ) { return false; }
    host = server->TlsHosts;
    while ( host != 0 ) {
        if ( xl6_http_ascii_equal(host->Host, request->ServerName.Data, request->ServerName.Size) ) { choice->Identity = host->Identity; return true; }
        host = host->Next;
    }
    return choice->Identity != 0;
}
    ) "\n";



/* Server/App 公开入口保留简洁手感，同时采用 Router 冻结和 Mux 热替换契约。 */
