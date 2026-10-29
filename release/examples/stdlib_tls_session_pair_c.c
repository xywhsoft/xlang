#define XRT_MODULE_ALL
#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



/* 失败即报告源码行并终止范例，成功输出仍与 xlang 范例一致。 */
static void require_line(bool bCondition, int iLine)
{
    if ( !bCondition ) {
        fprintf(stderr, "tls_session_pair=fail line=%d\n", iLine);
        exit(1);
    }
}



#define require_ok(condition) require_line((condition), __LINE__)



/* OK 与 AGAIN 都表示会话仍可继续由调用方驱动。 */
static bool tls_result_ok(xtlsresult Result)
{
    return (Result == XTLS_OK) || (Result == XTLS_AGAIN);
}



/* 测试验证器接管信任决策，协议签名和 Finished 仍由 XRT 严格验证。 */
static xtlsverifydecision verify_accept(
    const xtlspeer* pPeer,
    ptr pContext
)
{
    int* pCount = (int*)pContext;

    if ( pCount != NULL ) {
        *pCount += 1;
    }
    return ((pPeer != NULL) && (pPeer->Role == XTLS_SERVER) &&
        (pPeer->CertificateCount != 0)) ?
        XTLS_VERIFY_ACCEPT : XTLS_VERIFY_REJECT;
}



/* 拒绝验证器用于确认客户端能够中止不可信证书握手。 */
static xtlsverifydecision verify_reject(
    const xtlspeer* pPeer,
    ptr pContext
)
{
    (void)pPeer;
    (void)pContext;
    return XTLS_VERIFY_REJECT;
}



/* 从一份 CERTIFICATE PEM 和 RSA PRIVATE KEY PEM 创建不可变服务端身份。 */
static xtlsidentity* load_identity(cstr sCertificate, cstr sPrivateKey)
{
    bytes pCertificateText = NULL;
    bytes pPrivateText = NULL;
    bytes pCertificate = NULL;
    bytes pPrivate = NULL;
    size_t iCertificateText = 0;
    size_t iPrivateText = 0;
    size_t iCertificate = 0;
    size_t iPrivate = 0;
    xpemblock CertificateBlock;
    xpemblock PrivateBlock;
    xbytesview Certificate;
    xtlsidentity* pIdentity = NULL;

    pCertificateText = xrtFileReadAll(sCertificate, &iCertificateText);
    pPrivateText = xrtFileReadAll(sPrivateKey, &iPrivateText);
    if ( (pCertificateText == NULL) || (pPrivateText == NULL) ||
        !xrtPemFind(
            (cstr)pCertificateText, iCertificateText,
            "CERTIFICATE", &CertificateBlock
        ) || !xrtPemFind(
            (cstr)pPrivateText, iPrivateText,
            "RSA PRIVATE KEY", &PrivateBlock
        ) ) {
        goto cleanup;
    }
    pCertificate = xrtPemDecodeNew(&CertificateBlock, &iCertificate);
    pPrivate = xrtPemDecodeNew(&PrivateBlock, &iPrivate);
    if ( (pCertificate == NULL) || (pPrivate == NULL) ) {
        goto cleanup;
    }
    Certificate = (xbytesview) { pCertificate, iCertificate };
    pIdentity = xrtTlsIdentityRsa(
        &Certificate, 1u, (xbytesview) { pPrivate, iPrivate }
    );

cleanup:
    if ( pPrivate != NULL ) {
        xrtSecureZero(pPrivate, iPrivate);
    }
    xrtFree(pPrivate);
    xrtFree(pCertificate);
    xrtFree(pPrivateText);
    xrtFree(pCertificateText);
    return pIdentity;
}



/* 创建只开放 TLS 1.3 AES-128-GCM 的共享测试上下文。 */
static xtlscontext* create_context(void)
{
    static const xtlsversion Versions[] = { XTLS_VERSION_13 };
    static const xtlscipher Ciphers[] = { XTLS_AES_128_GCM_SHA256 };
    static const uint16 Groups[] = { XTLS_GROUP_X25519 };
    static const xtlssignature Signatures[] = {
        XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256
    };
    xtlspolicy Policy;
    xtlscontextconfig Config;

    xrtTlsPolicyInit(&Policy);
    Policy.Versions = Versions;
    Policy.VersionCount = sizeof(Versions) / sizeof(Versions[0]);
    Policy.Ciphers = Ciphers;
    Policy.CipherCount = sizeof(Ciphers) / sizeof(Ciphers[0]);
    Policy.Groups = Groups;
    Policy.GroupCount = sizeof(Groups) / sizeof(Groups[0]);
    Policy.Signatures = Signatures;
    Policy.SignatureCount = sizeof(Signatures) / sizeof(Signatures[0]);
    xrtTlsContextConfigInit(&Config);
    Config.Policy = &Policy;
    return xrtTlsContextCreate(&Config);
}



/* 每次只搬运指定上限的密文，验证两端都支持任意传输分片。 */
static bool move_cipher(
    xtlssession* pSource,
    xtlssession* pTarget,
    size_t iMaximum
)
{
    xnetspan Span;
    bool bMoved = false;

    while ( xrtTlsSessionSendSize(pSource) != 0 ) {
        size_t iChunk;

        if ( !xrtTlsSessionSendFront(pSource, &Span) ||
            (Span.Size == 0) ) {
            return false;
        }
        iChunk = Span.Size < iMaximum ? Span.Size : iMaximum;
        if ( (xrtTlsSessionFeed(
            pTarget, Span.Data, iChunk
        ) != XTLS_OK) || !xrtTlsSessionSendConsume(
            pSource, iChunk
        ) ) {
            return false;
        }
        bMoved = true;
    }
    return bMoved;
}



/* 驱动双方角色状态机并交换密文，直到同时进入 READY。 */
static bool handshake_pair(
    xtlssession* pClient,
    xtlssession* pServer
)
{
    for ( size_t i = 0; i < 4096u; i++ ) {
        xtlsresult ClientResult = xrtTlsClientDrive(pClient);
        xtlsresult ServerResult = xrtTlsServerDrive(pServer);

        if ( ((ClientResult != XTLS_OK) &&
             (ClientResult != XTLS_AGAIN)) ||
            ((ServerResult != XTLS_OK) &&
             (ServerResult != XTLS_AGAIN)) ) {
            return false;
        }
        (void)move_cipher(pClient, pServer, 37u);
        (void)move_cipher(pServer, pClient, 53u);
        if ( (xrtTlsSessionState(pClient) == XTLS_STATE_READY) &&
            (xrtTlsSessionState(pServer) == XTLS_STATE_READY) ) {
            return true;
        }
    }
    return false;
}



/* 发送一段应用明文，并从对端完整读回相同内容。 */
static bool transfer_plain(
    xtlssession* pClient,
    xtlssession* pServer,
    cstr sText
)
{
    char Output[64];
    size_t iSize = strlen(sText);
    size_t iWritten = 0;
    size_t iRead = 0;
    xtlsresult Result;

    if ( (iSize > sizeof(Output)) || (xrtTlsSessionWrite(
        pClient, sText, iSize, &iWritten
    ) != XTLS_OK) || (iWritten != iSize) ||
        !move_cipher(pClient, pServer, 19u) ) {
        return false;
    }
    Result = xrtTlsServerDrive(pServer);
    if ( (Result != XTLS_OK) && (Result != XTLS_AGAIN) ) {
        return false;
    }
    return (xrtTlsSessionRead(
        pServer, Output, sizeof(Output), &iRead
    ) == XTLS_OK) && (iRead == iSize) &&
        (memcmp(Output, sText, iSize) == 0);
}



/* 把服务端排队的 NewSessionTicket 驱动到客户端恢复对象队列。 */
static xtlsresume* issue_ticket(
    xtlssession* pClient,
    xtlssession* pServer,
    xtlsresume** ppServerResume
)
{
    if ( xrtTlsServerTicketNew(
        pServer, ppServerResume
    ) != XTLS_OK ) {
        return NULL;
    }
    for ( size_t i = 0; i < 4096u; i++ ) {
        xtlsresult Result;

        (void)move_cipher(pServer, pClient, 31u);
        Result = xrtTlsClientDrive(pClient);
        if ( (Result != XTLS_OK) && (Result != XTLS_AGAIN) ) {
            return NULL;
        }
        if ( xrtTlsClientResumeCount(pClient) != 0 ) {
            return xrtTlsClientTakeResume(pClient);
        }
    }
    return NULL;
}



/* 服务端恢复回调只返回票据字节完全匹配的不可变恢复对象。 */
static const xtlsresume* resume_lookup(
    ptr pContext,
    const xtlsserverresumerequest* pRequest
)
{
    const xtlsresume* pResume = (const xtlsresume*)pContext;
    xtlsresumeinfo Info;

    if ( (pRequest == NULL) ||
        !xrtTlsResumeInfo(pResume, &Info) ||
        (pRequest->Ticket.Size != Info.Ticket.Size) ||
        ((Info.Ticket.Size != 0) && (memcmp(
            pRequest->Ticket.Data, Info.Ticket.Data, Info.Ticket.Size
        ) != 0)) ) {
        return NULL;
    }
    return pResume;
}



/* 演示当前 XRT 传输无关 TLS 端点、票据恢复和严格失败边界。 */
int main(void)
{
    static const xstrview ClientProtocols[] = {
        XRT_STR_INIT("h2"),
        XRT_STR_INIT("http/1.1")
    };
    static const xstrview ServerProtocols[] = {
        XRT_STR_INIT("http/1.1")
    };
    static const char Payload[] = "hello tls";
    xtlscontext* pContext = create_context();
    xtlsidentity* pIdentity = load_identity(
        "release/examples/fixtures/tls_cert.pem",
        "release/examples/fixtures/tls_key.pem"
    );
    xtlsverifierconfig VerifierConfig;
    xtlsverifier* pVerifier;
    xtlsverifier* pRejectVerifier;
    xtlsclientconfig ClientConfig;
    xtlsserverconfig ServerConfig;
    xtlssession* pClient;
    xtlssession* pServer;
    xtlssession* pResumedClient;
    xtlssession* pResumedServer;
    xtlssession* pPartialClient;
    xtlssession* pPartialServer;
    xtlssession* pRejectClient;
    xtlssession* pRejectServer;
    xtlsresume* pServerResume = NULL;
    xtlsresume* pClientResume;
    xbytesview ServerName;
    xbytesview ClientProtocol;
    xbytesview ServerProtocol;
    xnetspan Span;
    size_t iCloseSize;
    int iVerifyCount = 0;

    require_ok((pContext != NULL) && (pIdentity != NULL));
    xrtTlsVerifierConfigInit(&VerifierConfig);
    VerifierConfig.Verify = verify_accept;
    VerifierConfig.Context = &iVerifyCount;
    pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
    VerifierConfig.Verify = verify_reject;
    VerifierConfig.Context = NULL;
    pRejectVerifier = xrtTlsVerifierCreate(&VerifierConfig);
    require_ok((pVerifier != NULL) && (pRejectVerifier != NULL));

    xrtTlsClientConfigInit(&ClientConfig);
    ClientConfig.Context = pContext;
    ClientConfig.ServerName = XRT_STR_LITERAL("localhost");
    ClientConfig.Protocols = ClientProtocols;
    ClientConfig.ProtocolCount =
        sizeof(ClientProtocols) / sizeof(ClientProtocols[0]);
    ClientConfig.Verifier = pVerifier;
    xrtTlsServerConfigInit(&ServerConfig);
    ServerConfig.Context = pContext;
    ServerConfig.Identity = pIdentity;
    ServerConfig.Protocols = ServerProtocols;
    ServerConfig.ProtocolCount =
        sizeof(ServerProtocols) / sizeof(ServerProtocols[0]);
    ServerConfig.RequireProtocol = true;

    pClient = xrtTlsClientCreate(&ClientConfig, NULL);
    pServer = xrtTlsServerCreate(&ServerConfig, NULL);
    require_ok((pClient != NULL) && (pServer != NULL) &&
        handshake_pair(pClient, pServer));
    require_ok((xrtTlsSessionVersion(pClient) == XTLS_VERSION_13) &&
        (xrtTlsSessionVersion(pServer) == XTLS_VERSION_13) &&
        (xrtTlsSessionCipher(pClient) == XTLS_AES_128_GCM_SHA256) &&
        (xrtTlsSessionCipher(pServer) == XTLS_AES_128_GCM_SHA256));
    require_ok(xrtTlsServerName(pServer, &ServerName) &&
        (ServerName.Size == 9u) &&
        (memcmp(ServerName.Data, "localhost", 9u) == 0));
    require_ok(xrtTlsSessionProtocol(pClient, &ClientProtocol) &&
        xrtTlsSessionProtocol(pServer, &ServerProtocol) &&
        (ClientProtocol.Size == 8u) &&
        (ClientProtocol.Size == ServerProtocol.Size) &&
        (memcmp(ClientProtocol.Data, "http/1.1", 8u) == 0) &&
        (memcmp(
            ClientProtocol.Data, ServerProtocol.Data, ClientProtocol.Size
        ) == 0));
    require_ok((iVerifyCount != 0) &&
        (xrtTlsClientCertificateCount(pClient) != 0) &&
        transfer_plain(pClient, pServer, Payload));

    pClientResume = issue_ticket(
        pClient, pServer, &pServerResume
    );
    require_ok((pClientResume != NULL) && (pServerResume != NULL));
    require_ok(tls_result_ok(xrtTlsSessionClose(pClient)));
    iCloseSize = xrtTlsSessionSendSize(pClient);
    require_ok(tls_result_ok(xrtTlsSessionClose(pClient)) &&
        (xrtTlsSessionSendSize(pClient) == iCloseSize));

    ClientConfig.Resume = pClientResume;
    ServerConfig.Resume = resume_lookup;
    ServerConfig.ResumeContext = pServerResume;
    pResumedClient = xrtTlsClientCreate(&ClientConfig, NULL);
    pResumedServer = xrtTlsServerCreate(&ServerConfig, NULL);
    require_ok((pResumedClient != NULL) && (pResumedServer != NULL) &&
        handshake_pair(pResumedClient, pResumedServer) &&
        xrtTlsClientResumed(pResumedClient) &&
        xrtTlsServerResumed(pResumedServer));

    ClientConfig.Resume = NULL;
    ServerConfig.Resume = NULL;
    ServerConfig.ResumeContext = NULL;
    pPartialClient = xrtTlsClientCreate(&ClientConfig, NULL);
    pPartialServer = xrtTlsServerCreate(&ServerConfig, NULL);
    require_ok((pPartialClient != NULL) && (pPartialServer != NULL) &&
        xrtTlsSessionSendFront(pPartialClient, &Span) && (Span.Size != 0));
    require_ok((xrtTlsSessionFeed(
        pPartialServer, Span.Data, 1u
    ) == XTLS_OK) && xrtTlsSessionSendConsume(pPartialClient, 1u) &&
        (xrtTlsServerDrive(pPartialServer) == XTLS_AGAIN) &&
        (xrtTlsSessionState(pPartialServer) != XTLS_STATE_READY));

    ClientConfig.Verifier = pRejectVerifier;
    pRejectClient = xrtTlsClientCreate(&ClientConfig, NULL);
    pRejectServer = xrtTlsServerCreate(&ServerConfig, NULL);
    require_ok((pRejectClient != NULL) && (pRejectServer != NULL) &&
        !handshake_pair(pRejectClient, pRejectServer));

    ServerConfig.Identity = NULL;
    require_ok(xrtTlsServerCreate(&ServerConfig, NULL) == NULL);

    xrtTlsSessionDestroy(pRejectServer);
    xrtTlsSessionDestroy(pRejectClient);
    xrtTlsSessionDestroy(pPartialServer);
    xrtTlsSessionDestroy(pPartialClient);
    xrtTlsSessionDestroy(pResumedServer);
    xrtTlsSessionDestroy(pResumedClient);
    xrtTlsSessionDestroy(pServer);
    xrtTlsSessionDestroy(pClient);
    xrtTlsResumeRelease(pClientResume);
    xrtTlsResumeRelease(pServerResume);
    xrtTlsVerifierRelease(pRejectVerifier);
    xrtTlsVerifierRelease(pVerifier);
    xrtTlsIdentityRelease(pIdentity);
    xrtTlsContextRelease(pContext);
    printf("tls_session_pair=ok\n");
    return 0;
}
