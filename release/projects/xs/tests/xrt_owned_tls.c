/* C 脚本需要完整 XRT 公共声明；实现仍由 xl 运行宿主注入。 */
#define XRT_MODULE_ALL
#include "xrt.h"

#include <stdio.h>

/*
    该用例直接使用当前 XRT 的公开 HTTP/TLS 分层 API。
    不经过 xlang 的服务封装，确保 C 环境也能独立完成完整生命周期。
*/
static xtlsidentity* xs2_load_tls_identity(const char* certificatePath,
    const char* keyPath)
{
    bytes certificateText = NULL;
    bytes keyText = NULL;
    bytes certificateData = NULL;
    bytes keyData = NULL;
    size_t certificateTextSize = 0u;
    size_t keyTextSize = 0u;
    size_t certificateSize = 0u;
    size_t keySize = 0u;
    xpemblock certificateBlock;
    xpemblock keyBlock;
    xbytesview certificate;
    xtlsidentity* identity = NULL;

    certificateText = xrtFileReadAll(certificatePath, &certificateTextSize);
    keyText = xrtFileReadAll(keyPath, &keyTextSize);
    if ( certificateText == NULL || keyText == NULL ||
         !xrtPemFind((const char*)certificateText, certificateTextSize,
             "CERTIFICATE", &certificateBlock) ||
         (!xrtPemFind((const char*)keyText, keyTextSize,
              "RSA PRIVATE KEY", &keyBlock) &&
          !xrtPemFind((const char*)keyText, keyTextSize,
              "EC PRIVATE KEY", &keyBlock) &&
          !xrtPemFind((const char*)keyText, keyTextSize,
              "PRIVATE KEY", &keyBlock)) ) {
        goto cleanup;
    }

    certificateData = xrtPemDecodeNew(&certificateBlock, &certificateSize);
    keyData = xrtPemDecodeNew(&keyBlock, &keySize);
    if ( certificateData == NULL || keyData == NULL ) {
        goto cleanup;
    }

    certificate.Data = certificateData;
    certificate.Size = certificateSize;
    identity = xrtTlsIdentityRsa(&certificate, 1u,
        (xbytesview){ keyData, keySize });
    if ( identity == NULL ) {
        identity = xrtTlsIdentityP256(&certificate, 1u,
            (xbytesview){ keyData, keySize });
    }
    if ( identity == NULL ) {
        identity = xrtTlsIdentityP384(&certificate, 1u,
            (xbytesview){ keyData, keySize });
    }
    if ( identity == NULL ) {
        identity = xrtTlsIdentityEd25519(&certificate, 1u,
            (xbytesview){ keyData, keySize });
    }

cleanup:
    xrtFree(certificateData);
    xrtFree(keyData);
    xrtFree(certificateText);
    xrtFree(keyText);
    return identity;
}

int main(void)
{
    xnetengineconfig engineConfig;
    xhttpserverconfig serverConfig;
    xhttpservertlsconfig tlsConfig;
    xnetengine* engine = NULL;
    xhttpserver* server = NULL;
    xtlsidentity* identity = NULL;
    xnetaddr address;
    int result = 0;

    identity = xs2_load_tls_identity(
        "examples/fixtures/tls_cert.pem",
        "examples/fixtures/tls_key.pem");
    if ( identity == NULL ) {
        fputs("XRT TLS identity create failed\n", stderr);
        return 1;
    }

    xrtNetEngineConfigInit(&engineConfig);
    engineConfig.Workers = 2u;
    engine = xrtNetEngineCreate(&engineConfig);
    if ( engine == NULL || !xrtNetEngineStart(engine) ) {
        fputs("XRT network engine start failed\n", stderr);
        result = 2;
        goto cleanup;
    }

    xrtHttpServerConfigInit(&serverConfig);
    if ( !xrtNetAddrLoopback(&serverConfig.Network.Listen.Address,
             XNET_FAMILY_IPV4, 0u) ) {
        fputs("XRT loopback address setup failed\n", stderr);
        result = 3;
        goto cleanup;
    }
    serverConfig.Network.Listen.Backlog = 128;

    xrtHttpServerTlsConfigInit(&tlsConfig);
    tlsConfig.Handshake.Identity = identity;
    server = xrtHttpServerStartTls(engine, &serverConfig, &tlsConfig, NULL);
    if ( server == NULL || !xrtHttpServerSecure(server) ||
         !xrtHttpServerLocal(server, 0u, &address) || address.Port == 0u ) {
        fputs("XRT TLS HTTP server start failed\n", stderr);
        result = 4;
        goto cleanup;
    }

cleanup:
    if ( server != NULL ) {
        (void)xrtHttpServerAbort(server);
        xrtHttpServerDestroy(server);
    }
    xrtTlsIdentityRelease(identity);
    if ( engine != NULL ) {
        (void)xrtNetEngineStop(engine);
        (void)xrtNetEngineDestroy(engine);
    }
    if ( result == 0 ) {
        puts("xs2 XRT owned TLS test ok");
    }
    return result;
}
