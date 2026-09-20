#define XL6_TLS_SOURCE(...) #__VA_ARGS__



/* TLS 运行时只暴露 xlang 所需的拥有型适配对象，协议状态机完全复用当前 XRT。 */
static const char xl6TlsTypesSource[] =
    "#ifndef XL6_TLS_RUNTIME\n"
    "#define XL6_TLS_RUNTIME\n"
    "#include <stdbool.h>\n"
    "#include <stddef.h>\n"
    "#include <stdint.h>\n"
    "#include <string.h>\n\n"
    XL6_TLS_SOURCE(
typedef xtlspolicy XL6_TlsPolicy;
typedef xtlslimits XL6_TlsLimits;
typedef xtlscontextconfig XL6_TlsContextConfig;
typedef xtlspeer XL6_TlsPeer;
typedef xtlsverifierconfig XL6_TlsVerifierConfig;
typedef xtlsclientconfig XL6_TlsClientConfig;
typedef xtlsserverrequest XL6_TlsServerRequest;
typedef xtlsserverchoice XL6_TlsServerChoice;
typedef xtlsserverresumerequest XL6_TlsServerResumeRequest;
typedef xtlsserverconfig XL6_TlsServerConfig;
typedef xtlsresumeinfo XL6_TlsResumeInfo;
typedef xnetspan XL6_NetSpan;

typedef struct XL6_TlsConfig {
    char* CertificateFile;
    char* KeyFile;
    char* CaFile;
    char* CrlFile;
    char* Host;
    char* Alpn;
    void* CertificateData;
    size_t CertificateSize;
    void* KeyData;
    size_t KeySize;
    void* CaData;
    size_t CaSize;
    void* CrlData;
    size_t CrlSize;
    bool VerifyPeer;
    bool AllowTls12Ed25519;
    uint16_t MaxVersion;
    XL6_Function VerifyCallback;
    bool HasVerifyCallback;
} XL6_TlsConfig;

typedef struct XL6_TlsLease {
    volatile int32_t References;
    char* Host;
    char* ProtocolText;
    xstrview* Protocols;
    size_t ProtocolCount;
    xtlscontext* Context;
    xtlsidentity* Identity;
    xtlsverifier* Verifier;
    xx509store* Store;
    bool VerifyPeer;
    bool Invalid;
    XL6_Function VerifyCallback;
    bool HasVerifyCallback;
} XL6_TlsLease;

typedef struct XL6_TlsSession {
    XL6_TlsLease* Lease;
    xtlssession* Native;
    xtlsidentity* Identity;
    xtlsresume* Resume;
    xtlsresume* IssuedResume;
    int LastStatus;
    bool Server;
    bool Failed;
    bool Closed;
    bool TicketIssued;
} XL6_TlsSession;

typedef struct XL6_TlsStream {
    xtlsstream* Native;
    XL6_TlsLease* Lease;
} XL6_TlsStream;

static void* xl6_xrt_buffer_from_owned_bytes(void*, size_t);
static inline char* xl6_xrt_string_dup(const char*, size_t);
) "\n";



/* 配置快照、ALPN 与证书导入都在会话建立前完成。 */
static const char xl6TlsConfigSource[] = XL6_TLS_SOURCE(
static xstrview xl6_tls_view(const char* text)
{
    xstrview view;
    view.Data = text;
    view.Size = text != 0 ? strlen(text) : 0u;
    return view;
}

static xbytesview xl6_tls_bytes(const void* data, size_t size)
{
    xbytesview view;
    view.Data = (const unsigned char*)data;
    view.Size = size;
    return view;
}

static bool xl6_tls_text_equal(xstrview view, const char* text)
{
    size_t size = strlen(text);
    return view.Size == size && (size == 0u || memcmp(view.Data, text, size) == 0);
}

static bool xl6_tls_replace_string(char** target, const char* value)
{
    char* copy = xl6_xrt_string_dup(value != 0 ? value : "", 0u);
    if ( copy == 0 ) { return false; }
    xrtFree(*target);
    *target = copy;
    return true;
}

static void* xl6_tls_copy(const void* data, size_t size)
{
    void* copy;
    if ( size == 0u ) { return 0; }
    if ( data == 0 ) { return 0; }
    copy = xrtMalloc(size);
    if ( copy != 0 ) { memcpy(copy, data, size); }
    return copy;
}

static bool xl6_tls_replace_bytes(void** target, size_t* targetSize, const void* data, size_t size)
{
    void* copy = 0;
    if ( size > 0u ) {
        if ( data == 0 ) { return false; }
        copy = xl6_tls_copy(data, size);
        if ( copy == 0 ) { return false; }
    }
    xrtFree(*target);
    *target = copy;
    *targetSize = size;
    return true;
}

static bool xl6_tls_protocols_copy(XL6_TlsLease* lease, const char* source)
{
    size_t count = 0u;
    size_t index = 0u;
    char* cursor;
    char* begin;
    if ( source == 0 || source[0] == 0 ) { return true; }
    lease->ProtocolText = xl6_xrt_string_dup(source, 0u);
    if ( lease->ProtocolText == 0 ) { return false; }
    count = 1u;
    for ( cursor = lease->ProtocolText; *cursor != 0; cursor++ ) { if ( *cursor == ',' ) { count++; } }
    lease->Protocols = (xstrview*)xrtCalloc(count, sizeof(*lease->Protocols));
    if ( lease->Protocols == 0 ) { return false; }
    begin = lease->ProtocolText;
    for ( cursor = lease->ProtocolText;; cursor++ ) {
        if ( *cursor == ',' || *cursor == 0 ) {
            char* end = cursor;
            while ( begin < end && (*begin == ' ' || *begin == '\t') ) { begin++; }
            while ( end > begin && (end[-1] == ' ' || end[-1] == '\t') ) { end--; }
            if ( begin == end || (size_t)(end - begin) > 255u ) { return false; }
            lease->Protocols[index].Data = begin;
            lease->Protocols[index].Size = (size_t)(end - begin);
            index++;
            if ( *cursor == 0 ) { break; }
            *cursor = 0;
            begin = cursor + 1;
        }
    }
    lease->ProtocolCount = index;
    return true;
}

static xtlsidentity* xl6_tls_identity_data(const void* certificateData, size_t certificateSize, const void* keyData, size_t keySize)
{
    xpemcursor cursor;
    xpemblock block;
    xbytesview* certificates = 0;
    unsigned char** owned = 0;
    unsigned char* key = 0;
    size_t certificateCount = 0u;
    size_t index = 0u;
    size_t decodedKeySize = 0u;
    xtlsidentity* identity = 0;
    if ( certificateData == 0 || certificateSize == 0u || keyData == 0 || keySize == 0u ) { return 0; }
    if ( xrtPemInit(&cursor, (const char*)certificateData, certificateSize) ) {
        while ( xrtPemRead(&cursor, &block) == 1 ) { if ( xl6_tls_text_equal(block.Label, "CERTIFICATE") ) { certificateCount++; } }
    }
    if ( certificateCount == 0u ) {
        certificateCount = 1u;
        certificates = (xbytesview*)xrtCalloc(1u, sizeof(*certificates));
        if ( certificates == 0 ) { goto cleanup; }
        certificates[0] = xl6_tls_bytes(certificateData, certificateSize);
    } else {
        certificates = (xbytesview*)xrtCalloc(certificateCount, sizeof(*certificates));
        owned = (unsigned char**)xrtCalloc(certificateCount, sizeof(*owned));
        if ( certificates == 0 || owned == 0 || !xrtPemInit(&cursor, (const char*)certificateData, certificateSize) ) { goto cleanup; }
        while ( index < certificateCount && xrtPemRead(&cursor, &block) == 1 ) {
            if ( !xl6_tls_text_equal(block.Label, "CERTIFICATE") ) { continue; }
            owned[index] = xrtPemDecodeNew(&block, &certificates[index].Size);
            certificates[index].Data = owned[index];
            if ( owned[index] == 0 ) { goto cleanup; }
            index++;
        }
        if ( index != certificateCount ) { goto cleanup; }
    }
    if ( xrtPemInit(&cursor, (const char*)keyData, keySize) ) {
        while ( xrtPemRead(&cursor, &block) == 1 ) {
            if ( xl6_tls_text_equal(block.Label, "RSA PRIVATE KEY") || xl6_tls_text_equal(block.Label, "EC PRIVATE KEY") || xl6_tls_text_equal(block.Label, "PRIVATE KEY") ) {
                key = xrtPemDecodeNew(&block, &decodedKeySize);
                break;
            }
        }
    }
    if ( key == 0 ) {
        key = (unsigned char*)xl6_tls_copy(keyData, keySize);
        decodedKeySize = keySize;
    }
    if ( key == 0 ) { goto cleanup; }
    identity = xrtTlsIdentityRsa(certificates, certificateCount, xl6_tls_bytes(key, decodedKeySize));
    if ( identity == 0 ) { xrtClearError(); identity = xrtTlsIdentityP256(certificates, certificateCount, xl6_tls_bytes(key, decodedKeySize)); }
    if ( identity == 0 ) { xrtClearError(); identity = xrtTlsIdentityP384(certificates, certificateCount, xl6_tls_bytes(key, decodedKeySize)); }
    if ( identity == 0 ) { xrtClearError(); identity = xrtTlsIdentityEd25519(certificates, certificateCount, xl6_tls_bytes(key, decodedKeySize)); }
cleanup:
    if ( owned != 0 ) { for ( index = 0u; index < certificateCount; index++ ) { xrtFree(owned[index]); } }
    xrtFree(owned);
    xrtFree(certificates);
    xrtFree(key);
    return identity;
}

static xtlsidentity* xl6_tls_identity_file(const char* certificateFile, const char* keyFile)
{
    unsigned char* certificateData;
    unsigned char* keyData;
    size_t certificateSize = 0u;
    size_t keySize = 0u;
    xtlsidentity* identity;
    if ( certificateFile == 0 || keyFile == 0 ) { return 0; }
    certificateData = xrtFileReadAll(certificateFile, &certificateSize);
    keyData = xrtFileReadAll(keyFile, &keySize);
    if ( certificateData == 0 || keyData == 0 ) { xrtFree(certificateData); xrtFree(keyData); return 0; }
    identity = xl6_tls_identity_data(certificateData, certificateSize, keyData, keySize);
    xrtFree(certificateData);
    xrtFree(keyData);
    return identity;
}

static xtlsidentity* xl6_tls_config_identity(const XL6_TlsConfig* config)
{
    if ( config->CertificateData != 0 || config->KeyData != 0 ) {
        return xl6_tls_identity_data(config->CertificateData, config->CertificateSize, config->KeyData, config->KeySize);
    }
    if ( config->CertificateFile != 0 || config->KeyFile != 0 ) {
        return xl6_tls_identity_file(config->CertificateFile, config->KeyFile);
    }
    return 0;
}

static xtlscontext* xl6_tls_context_create(uint16_t maxVersion)
{
    XL6_TlsContextConfig config;
    XL6_TlsPolicy policy;
    xtlsversion versions[2];
    xrtTlsContextConfigInit(&config);
    if ( maxVersion == 0u || maxVersion >= 0x0304u ) { return xrtTlsContextCreate(&config); }
    if ( maxVersion < 0x0303u ) { return 0; }
    xrtTlsPolicyInit(&policy);
    versions[0] = 0x0303;
    policy.Versions = versions;
    policy.VersionCount = 1u;
    config.Policy = &policy;
    return xrtTlsContextCreate(&config);
}

static bool xl6_tls_store_data(xx509store* store, const void* data, size_t size)
{
    if ( store == 0 || data == 0 || size == 0u ) { return false; }
    if ( xrtX509StoreAddPem(store, (const char*)data, size, 0) ) { return true; }
    xrtClearError();
    return xrtX509StoreAdd(store, data, size) >= 0;
}
) "\n";



/* xlang 回调可以观察默认验证结果，再明确接受或拒绝。 */
static const char xl6TlsCallbackSource[] = XL6_TLS_SOURCE(
static xvalue* xl6_function_call_var(XL6_Function, int, ...);

static bool xl6_tls_event_set(xvalue* event, const char* key, xvalue* value)
{
    if ( event == 0 || value == 0 ) { if ( value != 0 ) { xrtValueRelease(value); } return false; }
    if ( !xl6_xrt_value_object_set(event, key, 0u, value, true) ) { xrtValueRelease(value); return false; }
    return true;
}

static bool xl6_tls_callback_call(XL6_TlsLease* lease, const XL6_TlsPeer* peer, bool defaultOk)
{
    xvalue* event;
    xvalue* result;
    xbytesview certificate = { 0, 0u };
    char* certificateHex = 0;
    bool accepted;
    if ( lease == 0 || !lease->HasVerifyCallback ) { return defaultOk; }
    if ( peer != 0 && peer->CertificateCount > 0u && peer->Certificates != 0 ) { certificate = *(const xbytesview*)peer->Certificates; }
    if ( certificate.Data != 0 && certificate.Size > 0u ) { certificateHex = (char*)xrtHexEncodeNew(certificate.Data, certificate.Size, 1u); }
    event = xrtValueObject();
    if ( event == 0 ||
         !xl6_tls_event_set(event, "defaultOk", xrtValueBool(defaultOk)) ||
         !xl6_tls_event_set(event, "certSize", xrtValueInt((long long)certificate.Size)) ||
         !xl6_tls_event_set(event, "certHex", xl6_xrt_value_text_create((void*)(certificateHex != 0 ? certificateHex : ""), 0u, false)) ) {
        xrtFree(certificateHex);
        if ( event != 0 ) { xrtValueRelease(event); }
        return false;
    }
    xrtFree(certificateHex);
    result = xl6_function_call_var(lease->VerifyCallback, 1, event);
    accepted = result != 0 ? xl6_xrt_value_bool(result) : false;
    if ( result != 0 ) { xrtValueRelease(result); }
    return accepted;
}

static xtlsverifydecision xl6_tls_verify_callback(const XL6_TlsPeer* peer, void* context)
{
    XL6_TlsLease* lease = (XL6_TlsLease*)context;
    bool defaultOk = lease != 0 && !lease->VerifyPeer;
    if ( lease != 0 && lease->VerifyPeer && lease->Store != 0 ) {
        defaultOk = xrtTlsPeerVerify(peer, lease->Store, 0, 0);
        if ( !defaultOk ) { xrtClearError(); }
    }
    return xl6_tls_callback_call(lease, peer, defaultOk) ? XTLS_VERIFY_ACCEPT : XTLS_VERIFY_REJECT;
}
) "\n";



static const char xl6TlsLeaseSource[] = XL6_TLS_SOURCE(
static void xl6_tls_lease_dispose(XL6_TlsLease* lease)
{
    if ( lease == 0 ) { return; }
    xrtTlsVerifierRelease(lease->Verifier);
    xrtX509StoreFree(lease->Store);
    xrtTlsIdentityRelease(lease->Identity);
    xrtTlsContextRelease(lease->Context);
    xrtFree(lease->Protocols);
    xrtFree(lease->ProtocolText);
    xrtFree(lease->Host);
    if ( lease->HasVerifyCallback ) { xl6_function_release_value(lease->VerifyCallback); }
    memset(lease, 0, sizeof(*lease));
    xrtFree(lease);
}

static XL6_TlsLease* xl6_tls_lease_create_mode(
    void* handle,
    bool client
)
{
    XL6_TlsConfig defaults;
    XL6_TlsConfig* source = (XL6_TlsConfig*)handle;
    XL6_TlsLease* lease;
    XL6_TlsVerifierConfig verifier;
    bool hasIdentity;
    memset(&defaults, 0, sizeof(defaults));
    defaults.VerifyPeer = true;
    defaults.AllowTls12Ed25519 = true;
    if ( source == 0 ) { source = &defaults; }
    lease = (XL6_TlsLease*)xrtCalloc(1u, sizeof(*lease));
    if ( lease == 0 ) { return 0; }
    lease->References = 1;
    lease->VerifyPeer = source->VerifyPeer;
    lease->Host = xl6_xrt_string_dup(source->Host != 0 ? source->Host : "", 0u);
    lease->Context = xl6_tls_context_create(source->MaxVersion);
    if ( lease->Host == 0 || lease->Context == 0 || !xl6_tls_protocols_copy(lease, source->Alpn) ) { lease->Invalid = true; }
    hasIdentity = source->CertificateData != 0 || source->KeyData != 0 || source->CertificateFile != 0 || source->KeyFile != 0;
    if ( hasIdentity ) {
        lease->Identity = xl6_tls_config_identity(source);
        if ( lease->Identity == 0 ) { lease->Invalid = true; xrtClearError(); }
    }
    if ( client && source->HasVerifyCallback ) {
        lease->VerifyCallback = xl6_function_copy_value(source->VerifyCallback);
        lease->HasVerifyCallback = true;
    }
    if ( client && source->VerifyPeer ) {
        if ( source->CaData != 0 ) {
            lease->Store = xrtX509StoreCreate();
            if ( lease->Store == 0 || !xl6_tls_store_data(lease->Store, source->CaData, source->CaSize) ) { lease->Invalid = true; }
        } else if ( source->CaFile != 0 && source->CaFile[0] != 0 ) {
            lease->Store = xrtX509StoreCreate();
            if ( lease->Store == 0 || !xrtX509StoreAddFile(lease->Store, source->CaFile, 0) ) { lease->Invalid = true; }
        } else {
            lease->Store = xrtX509StoreSystem();
            if ( lease->Store == 0 ) { lease->Invalid = true; }
        }
    }
    if ( client && ((source->CrlData != 0 && source->CrlSize != 0u) || (source->CrlFile != 0 && source->CrlFile[0] != 0)) ) {
        lease->Invalid = true;
    }
    if ( client && !lease->Invalid ) {
        xrtTlsVerifierConfigInit(&verifier);
        verifier.Verify = xl6_tls_verify_callback;
        verifier.Context = lease;
        lease->Verifier = xrtTlsVerifierCreate(&verifier);
        if ( lease->Verifier == 0 ) { lease->Invalid = true; }
    }
    return lease;
}

XL6_TlsLease* xl6_xrt_tls_lease_create(void* handle)
{
    return xl6_tls_lease_create_mode(handle, true);
}

XL6_TlsLease* xl6_xrt_tls_lease_retain(XL6_TlsLease* lease)
{
    return lease != 0 && xrtRefRetain(&lease->References) > 0 ? lease : 0;
}

void xl6_xrt_tls_lease_release(XL6_TlsLease* lease)
{
    if ( lease == 0 || xrtRefRelease(&lease->References) != 0 ) { return; }
    xl6_tls_lease_dispose(lease);
}

static void xl6_tls_shared_verifier_release(void* context)
{
    xl6_xrt_tls_lease_release((XL6_TlsLease*)context);
}

xtlsverifier* xl6_xrt_tls_lease_share_verifier(XL6_TlsLease* lease)
{
    XL6_TlsLease* retained;
    xtlsverifierconfig config;
    xtlsverifier* verifier;
    if ( lease == 0 || lease->Invalid || lease->Verifier == 0 ) { return 0; }
    retained = xl6_xrt_tls_lease_retain(lease);
    if ( retained == 0 ) { return 0; }
    xrtTlsVerifierConfigInit(&config);
    config.Verify = xl6_tls_verify_callback;
    config.Release = xl6_tls_shared_verifier_release;
    config.Context = retained;
    verifier = xrtTlsVerifierCreate(&config);
    if ( verifier == 0 ) { xl6_xrt_tls_lease_release(retained); }
    return verifier;
}

void* xl6_xrt_tls_lease_value(XL6_TlsLease* lease) { return lease; }
xtlscontext* xl6_xrt_tls_lease_context(XL6_TlsLease* lease) { return lease != 0 ? lease->Context : 0; }
xtlsverifier* xl6_xrt_tls_lease_verifier(XL6_TlsLease* lease) { return lease != 0 ? lease->Verifier : 0; }

XL6_TlsLease* xl6ExtlibTlsLeaseCreate(void* config)
{
    return xl6_xrt_tls_lease_create(config);
}

void xl6ExtlibTlsLeaseRelease(XL6_TlsLease* lease)
{
    xl6_xrt_tls_lease_release(lease);
}

xtlscontext* xl6ExtlibTlsLeaseContext(XL6_TlsLease* lease)
{
    return xl6_xrt_tls_lease_context(lease);
}

xtlsverifier* xl6ExtlibTlsLeaseShareVerifier(XL6_TlsLease* lease)
{
    return xl6_xrt_tls_lease_share_verifier(lease);
}

void* xl6ExtlibTlsLeaseCreateHandle(void* config)
{
    return xl6ExtlibTlsLeaseCreate(config);
}

void xl6ExtlibTlsLeaseReleaseHandle(void* lease)
{
    xl6ExtlibTlsLeaseRelease((XL6_TlsLease*)lease);
}

void* xl6ExtlibTlsLeaseContextHandle(void* lease)
{
    return xl6ExtlibTlsLeaseContext((XL6_TlsLease*)lease);
}

void* xl6ExtlibTlsLeaseShareVerifierHandle(void* lease)
{
    return xl6ExtlibTlsLeaseShareVerifier((XL6_TlsLease*)lease);
}
) "\n";



/* TlsConfig 保持轻量可变；跨异步边界时统一转换为不可变 lease。 */
static const char xl6TlsConfigApiSource[] = XL6_TLS_SOURCE(
void* xl6_xrt_tls_config_create(void)
{
    XL6_TlsConfig* config = (XL6_TlsConfig*)xrtCalloc(1u, sizeof(*config));
    if ( config != 0 ) { config->VerifyPeer = true; config->AllowTls12Ed25519 = true; }
    return config;
}

long long xl6_xrt_tls_config_destroy(void* handle)
{
    XL6_TlsConfig* config = (XL6_TlsConfig*)handle;
    if ( config == 0 ) { return 0; }
    if ( config->HasVerifyCallback ) { xl6_function_release_value(config->VerifyCallback); }
    xrtFree(config->CertificateFile); xrtFree(config->KeyFile); xrtFree(config->CaFile); xrtFree(config->CrlFile);
    xrtFree(config->Host); xrtFree(config->Alpn); xrtFree(config->CertificateData); xrtFree(config->KeyData);
    xrtFree(config->CaData); xrtFree(config->CrlData);
    memset(config, 0, sizeof(*config)); xrtFree(config); return 0;
}

void* xl6_xrt_tls_config_native(void* handle) { return handle; }
int xl6_xrt_tls_config_host(void* handle, const char* host) { XL6_TlsConfig* config = (XL6_TlsConfig*)handle; return config != 0 && xl6_tls_replace_string(&config->Host, host); }
int xl6_xrt_tls_config_alpn(void* handle, const char* protocols) { XL6_TlsConfig* config = (XL6_TlsConfig*)handle; return config != 0 && xl6_tls_replace_string(&config->Alpn, protocols); }

int xl6_xrt_tls_config_cert(void* handle, const char* certificate, const char* key)
{
    XL6_TlsConfig* config = (XL6_TlsConfig*)handle;
    char* certificateCopy;
    char* keyCopy;
    if ( config == 0 ) { return false; }
    certificateCopy = xl6_xrt_string_dup(certificate != 0 ? certificate : "", 0u);
    keyCopy = xl6_xrt_string_dup(key != 0 ? key : "", 0u);
    if ( certificateCopy == 0 || keyCopy == 0 ) { xrtFree(certificateCopy); xrtFree(keyCopy); return false; }
    xrtFree(config->CertificateFile); xrtFree(config->KeyFile);
    config->CertificateFile = certificateCopy; config->KeyFile = keyCopy;
    return true;
}

int xl6_xrt_tls_config_ca(void* handle, const char* path) { XL6_TlsConfig* config = (XL6_TlsConfig*)handle; return config != 0 && xl6_tls_replace_string(&config->CaFile, path); }
int xl6_xrt_tls_config_crl(void* handle, const char* path) { XL6_TlsConfig* config = (XL6_TlsConfig*)handle; return config != 0 && xl6_tls_replace_string(&config->CrlFile, path); }
void xl6_xrt_tls_config_verify(void* handle, int enabled) { XL6_TlsConfig* config = (XL6_TlsConfig*)handle; if ( config != 0 ) { config->VerifyPeer = enabled != 0; } }
void xl6_xrt_tls_config_allow_ed25519(void* handle, int enabled) { XL6_TlsConfig* config = (XL6_TlsConfig*)handle; if ( config != 0 ) { config->AllowTls12Ed25519 = enabled != 0; } }
void xl6_xrt_tls_config_max_version(void* handle, long long version) { XL6_TlsConfig* config = (XL6_TlsConfig*)handle; if ( config != 0 ) { config->MaxVersion = version >= 0 && version <= 65535 ? (uint16_t)version : 0u; } }

bool xl6_xrt_tls_config_verify_callback(void* handle, XL6_Function callback)
{
    XL6_TlsConfig* config = (XL6_TlsConfig*)handle;
    if ( config == 0 || (callback.invoke_var == 0 && callback.invoke_xcall == 0) ) { return false; }
    if ( config->HasVerifyCallback ) { xl6_function_release_value(config->VerifyCallback); }
    config->VerifyCallback = xl6_function_copy_value(callback);
    config->HasVerifyCallback = true;
    return true;
}

int xl6_xrt_tls_config_cert_data(void* handle, void* certificate, size_t certificateSize, void* key, size_t keySize)
{
    XL6_TlsConfig* config = (XL6_TlsConfig*)handle;
    void* certificateCopy = 0;
    void* keyCopy = 0;
    if ( config == 0 || (certificateSize != 0u && certificate == 0) || (keySize != 0u && key == 0) ) { return false; }
    if ( certificateSize != 0u ) { certificateCopy = xl6_tls_copy(certificate, certificateSize); }
    if ( keySize != 0u ) { keyCopy = xl6_tls_copy(key, keySize); }
    if ( (certificateSize != 0u && certificateCopy == 0) || (keySize != 0u && keyCopy == 0) ) { xrtFree(certificateCopy); xrtFree(keyCopy); return false; }
    xrtFree(config->CertificateData); xrtFree(config->KeyData);
    config->CertificateData = certificateCopy; config->CertificateSize = certificateSize;
    config->KeyData = keyCopy; config->KeySize = keySize;
    return true;
}

int xl6_xrt_tls_config_ca_data(void* handle, void* data, size_t size) { XL6_TlsConfig* config = (XL6_TlsConfig*)handle; return config != 0 && xl6_tls_replace_bytes(&config->CaData, &config->CaSize, data, size); }
int xl6_xrt_tls_config_crl_data(void* handle, void* data, size_t size) { XL6_TlsConfig* config = (XL6_TlsConfig*)handle; return config != 0 && xl6_tls_replace_bytes(&config->CrlData, &config->CrlSize, data, size); }
) "\n";



/* 原始会话按 XRT 的输入、密文输出和明文输出三条有界队列进行适配。 */
static const char xl6TlsSessionSource[] = XL6_TLS_SOURCE(
static int xl6_tls_result(int result)
{
    if ( result == 0 ) { return 0; }
    if ( result == 1 ) { return -2; }
    if ( result == 2 ) { return 1; }
    return -1;
}

static const xtlsresume* xl6_tls_server_resume(void* context, const XL6_TlsServerResumeRequest* request)
{
    XL6_TlsSession* session = (XL6_TlsSession*)context;
    (void)request;
    return session != 0 ? session->Resume : 0;
}

static bool xl6_tls_session_ensure(XL6_TlsSession* session)
{
    if ( session == 0 || session->Lease == 0 || session->Lease->Invalid || session->Failed || session->Closed ) { return false; }
    if ( session->Native != 0 ) { return true; }
    if ( session->Server ) {
        XL6_TlsServerConfig config;
        const xtlsidentity* identity = session->Identity != 0 ? session->Identity : session->Lease->Identity;
        if ( identity == 0 ) { session->LastStatus = -1; return false; }
        xrtTlsServerConfigInit(&config);
        config.Context = session->Lease->Context;
        config.Identity = identity;
        config.Protocols = session->Lease->Protocols;
        config.ProtocolCount = session->Lease->ProtocolCount;
        config.Resume = session->Resume != 0 ? xl6_tls_server_resume : 0;
        config.ResumeContext = session;
        session->Native = xrtTlsServerCreate(&config, 0);
    } else {
        XL6_TlsClientConfig config;
        xrtTlsClientConfigInit(&config);
        config.Context = session->Lease->Context;
        config.ServerName = xl6_tls_view(session->Lease->Host);
        config.VerifyName = xl6_tls_view(session->Lease->Host);
        config.Protocols = session->Lease->Protocols;
        config.ProtocolCount = session->Lease->ProtocolCount;
        config.Verifier = session->Lease->Verifier;
        config.Resume = session->Resume;
        session->Native = xrtTlsClientCreate(&config, 0);
    }
    if ( session->Native == 0 ) { session->LastStatus = -1; return false; }
    return true;
}

static void xl6_tls_server_ticket(XL6_TlsSession* session)
{
    int result;
    if ( session == 0 || !session->Server || session->Native == 0 || session->TicketIssued ||
         xrtTlsSessionState(session->Native) != 2 || xrtTlsSessionVersion(session->Native) != 0x0304 ) { return; }
    result = xrtTlsServerTicketNew(session->Native, &session->IssuedResume);
    if ( result == 0 ) { session->TicketIssued = true; }
}

void* xl6_xrt_tls_session_create(void* configHandle, int server, void* resumeHandle)
{
    XL6_TlsSession* session = (XL6_TlsSession*)xrtCalloc(1u, sizeof(*session));
    if ( session == 0 ) { return 0; }
    session->Lease = xl6_tls_lease_create_mode(
        configHandle,
        server == 0
    );
    session->Server = server != 0;
    session->Resume = xrtTlsResumeRetain((const xtlsresume*)resumeHandle);
    if ( session->Lease == 0 ) { xrtFree(session); return 0; }
    if ( !session->Server ) { (void)xl6_tls_session_ensure(session); }
    return session;
}

long long xl6_xrt_tls_session_destroy(void* handle)
{
    XL6_TlsSession* session = (XL6_TlsSession*)handle;
    if ( session == 0 ) { return 0; }
    xrtTlsSessionDestroy(session->Native);
    xrtTlsIdentityRelease(session->Identity);
    xrtTlsResumeRelease(session->IssuedResume);
    xrtTlsResumeRelease(session->Resume);
    xl6_xrt_tls_lease_release(session->Lease);
    memset(session, 0, sizeof(*session)); xrtFree(session); return 0;
}

int xl6_xrt_tls_session_ready(void* handle) { XL6_TlsSession* session = (XL6_TlsSession*)handle; return session != 0 && session->Native != 0 && xrtTlsSessionState(session->Native) == 2; }
long long xl6_xrt_tls_session_last_status(void* handle) { XL6_TlsSession* session = (XL6_TlsSession*)handle; return session != 0 ? session->LastStatus : -1; }
long long xl6_xrt_tls_session_version(void* handle) { XL6_TlsSession* session = (XL6_TlsSession*)handle; return session != 0 && session->Native != 0 ? (long long)xrtTlsSessionVersion(session->Native) : 0; }
long long xl6_xrt_tls_session_cipher(void* handle) { XL6_TlsSession* session = (XL6_TlsSession*)handle; return session != 0 && session->Native != 0 ? (long long)xrtTlsSessionCipher(session->Native) : 0; }

long long xl6_xrt_tls_session_drive(void* handle)
{
    XL6_TlsSession* session = (XL6_TlsSession*)handle;
    int result;
    if ( !xl6_tls_session_ensure(session) ) { return -1; }
    result = session->Server ? xrtTlsServerDrive(session->Native) : xrtTlsClientDrive(session->Native);
    session->LastStatus = xl6_tls_result(result);
    if ( result < 0 ) { session->Failed = true; }
    xl6_tls_server_ticket(session);
    return session->LastStatus;
}

long long xl6_xrt_tls_session_feed(void* handle, void* data, size_t size)
{
    XL6_TlsSession* session = (XL6_TlsSession*)handle;
    bool ready;
    int result;
    if ( !xl6_tls_session_ensure(session) ) { return -1; }
    ready = xrtTlsSessionState(session->Native) == 2;
    result = xrtTlsSessionFeed(session->Native, data, size);
    if ( result == 0 && ready ) { result = session->Server ? xrtTlsServerDrive(session->Native) : xrtTlsClientDrive(session->Native); }
    session->LastStatus = xl6_tls_result(result);
    if ( result < 0 ) { session->Failed = true; }
    xl6_tls_server_ticket(session);
    return session->LastStatus;
}

long long xl6_xrt_tls_session_pending_cipher(void* handle) { XL6_TlsSession* session = (XL6_TlsSession*)handle; return session != 0 && session->Native != 0 ? (long long)xrtTlsSessionSendSize(session->Native) : 0; }
long long xl6_xrt_tls_session_pending_plain(void* handle) { XL6_TlsSession* session = (XL6_TlsSession*)handle; return session != 0 && session->Native != 0 ? (long long)xrtTlsSessionPlainSize(session->Native) : 0; }

static void* xl6_tls_cipher_buffer(XL6_TlsSession* session, long long maximum)
{
    size_t available;
    size_t wanted;
    size_t spanCount;
    size_t copied = 0u;
    XL6_NetSpan* spans;
    unsigned char* data;
    if ( session == 0 || session->Native == 0 ) { return xrtBufferCreate(); }
    available = xrtTlsSessionSendSize(session->Native);
    wanted = maximum > 0 && (uint64_t)maximum < available ? (size_t)maximum : available;
    if ( wanted == 0u ) { return xrtBufferCreate(); }
    spanCount = xrtTlsSessionSendSpanCount(session->Native);
    spans = (XL6_NetSpan*)xrtCalloc(spanCount, sizeof(*spans));
    data = (unsigned char*)xrtMalloc(wanted);
    if ( spans == 0 || data == 0 ) { xrtFree(spans); xrtFree(data); return 0; }
    spanCount = xrtTlsSessionSendSpans(session->Native, spans, spanCount);
    for ( size_t index = 0u; index < spanCount && copied < wanted; index++ ) {
        size_t take = spans[index].Size < (wanted - copied) ? spans[index].Size : (wanted - copied);
        memcpy(data + copied, spans[index].Data, take); copied += take;
    }
    xrtFree(spans);
    if ( copied != wanted ) { xrtFree(data); return xrtBufferCreate(); }
    return xl6_xrt_buffer_from_owned_bytes(data, copied);
}

void* xl6_xrt_tls_session_peek_cipher(void* handle, long long maximum) { return xl6_tls_cipher_buffer((XL6_TlsSession*)handle, maximum); }
void xl6_xrt_tls_session_consume_cipher(void* handle, long long size) { XL6_TlsSession* session = (XL6_TlsSession*)handle; if ( session != 0 && session->Native != 0 && size > 0 ) { (void)xrtTlsSessionSendConsume(session->Native, (size_t)size); } }

long long xl6_xrt_tls_session_write_plain(void* handle, void* data, size_t size)
{
    XL6_TlsSession* session = (XL6_TlsSession*)handle;
    size_t written = 0u;
    int result;
    if ( !xl6_tls_session_ensure(session) ) { return -1; }
    result = xrtTlsSessionWrite(session->Native, data, size, &written);
    session->LastStatus = xl6_tls_result(result);
    return written != 0u || size == 0u ? (long long)written : session->LastStatus;
}

void* xl6_xrt_tls_session_read_plain(void* handle, long long maximum)
{
    XL6_TlsSession* session = (XL6_TlsSession*)handle;
    size_t available;
    size_t wanted;
    size_t readSize = 0u;
    unsigned char* data;
    int result;
    if ( session == 0 || session->Native == 0 ) { return xrtBufferCreate(); }
    available = xrtTlsSessionPlainSize(session->Native);
    wanted = maximum > 0 && (uint64_t)maximum < available ? (size_t)maximum : available;
    if ( wanted == 0u ) { return xrtBufferCreate(); }
    data = (unsigned char*)xrtMalloc(wanted);
    if ( data == 0 ) { return 0; }
    result = xrtTlsSessionRead(session->Native, data, wanted, &readSize);
    session->LastStatus = xl6_tls_result(result);
    if ( result < 0 ) { xrtFree(data); return xrtBufferCreate(); }
    return xl6_xrt_buffer_from_owned_bytes(data, readSize);
}

long long xl6_xrt_tls_session_close(void* handle)
{
    XL6_TlsSession* session = (XL6_TlsSession*)handle;
    if ( session != 0 && session->Closed ) { return 0; }
    if ( !xl6_tls_session_ensure(session) ) { return -1; }
    if ( xrtTlsSessionState(session->Native) < 2 ) {
        xrtTlsSessionDestroy(session->Native);
        session->Native = 0;
        session->Closed = true;
        session->LastStatus = 0;
        return 0;
    }
    session->LastStatus = xl6_tls_result(xrtTlsSessionClose(session->Native));
    return session->LastStatus;
}

int xl6_xrt_tls_session_resumed(void* handle)
{
    XL6_TlsSession* session = (XL6_TlsSession*)handle;
    if ( session == 0 || session->Native == 0 || xrtTlsSessionVersion(session->Native) != 0x0304 ) { return false; }
    return session->Server ? xrtTlsServerResumed(session->Native) : xrtTlsClientResumed(session->Native);
}

void* xl6_xrt_tls_session_sni(void* handle)
{
    XL6_TlsSession* session = (XL6_TlsSession*)handle;
    xbytesview name = { 0, 0u };
    if ( session != 0 && session->Server && session->Native != 0 ) { (void)xrtTlsServerName(session->Native, &name); }
    return xl6_xrt_string_dup((const char*)(name.Data != 0 ? name.Data : (const unsigned char*)""), name.Size);
}

void* xl6_xrt_tls_session_alpn(void* handle)
{
    XL6_TlsSession* session = (XL6_TlsSession*)handle;
    xbytesview protocol = { 0, 0u };
    if ( session != 0 && session->Native != 0 ) { (void)xrtTlsSessionProtocol(session->Native, &protocol); }
    return xl6_xrt_string_dup((const char*)(protocol.Data != 0 ? protocol.Data : (const unsigned char*)""), protocol.Size);
}

void* xl6_xrt_tls_session_peer_cert(void* handle)
{
    XL6_TlsSession* session = (XL6_TlsSession*)handle;
    const void* certificate;
    xbytesview raw;
    void* copy;
    if ( session == 0 || session->Server || session->Native == 0 || xrtTlsClientCertificateCount(session->Native) == 0u ) { return xrtBufferCreate(); }
    certificate = xrtTlsClientCertificate(session->Native, 0u);
    if ( certificate == 0 ) { return xrtBufferCreate(); }
    raw = *(const xbytesview*)certificate;
    copy = raw.Size != 0u ? xl6_tls_copy(raw.Data, raw.Size) : 0;
    if ( raw.Size != 0u && copy == 0 ) { return 0; }
    return raw.Size != 0u ? xl6_xrt_buffer_from_owned_bytes(copy, raw.Size) : xrtBufferCreate();
}

void* xl6_xrt_tls_session_export_resume(void* handle)
{
    XL6_TlsSession* session = (XL6_TlsSession*)handle;
    if ( session == 0 || session->Server || session->Native == 0 ) { return 0; }
    for ( size_t index = 0u; index < 8u && xrtTlsSessionFeedSize(session->Native) != 0u; index++ ) {
        int result = xrtTlsClientDrive(session->Native);
        if ( result < 0 ) { return 0; }
        if ( result == 1 && xrtTlsSessionFeedSize(session->Native) != 0u ) { continue; }
    }
    return xrtTlsClientTakeResume(session->Native);
}

int xl6_xrt_tls_session_set_cert(void* handle, const char* certificate, const char* key)
{
    XL6_TlsSession* session = (XL6_TlsSession*)handle;
    xtlsidentity* identity;
    if ( session == 0 || !session->Server || session->Native != 0 ) { return false; }
    identity = xl6_tls_identity_file(certificate, key);
    if ( identity == 0 ) { return false; }
    xrtTlsIdentityRelease(session->Identity); session->Identity = identity; session->Failed = false; return true;
}

int xl6_xrt_tls_session_set_cert_data(void* handle, void* certificate, size_t certificateSize, void* key, size_t keySize)
{
    XL6_TlsSession* session = (XL6_TlsSession*)handle;
    xtlsidentity* identity;
    if ( session == 0 || !session->Server || session->Native != 0 ) { return false; }
    identity = xl6_tls_identity_data(certificate, certificateSize, key, keySize);
    if ( identity == 0 ) { return false; }
    xrtTlsIdentityRelease(session->Identity); session->Identity = identity; session->Failed = false; return true;
}

void xl6_xrt_tls_session_allow_ed25519(void* handle, int enabled) { (void)handle; (void)enabled; }
void* xl6_xrt_tls_resume_wrap(void* handle) { return handle; }
long long xl6_xrt_tls_resume_destroy(void* handle) { xrtTlsResumeRelease((xtlsresume*)handle); return 0; }
void* xl6_xrt_tls_resume_native(void* handle) { return handle; }
) "\n#endif\n\n";



#include "xl6_cgen_tls_stream.c"
