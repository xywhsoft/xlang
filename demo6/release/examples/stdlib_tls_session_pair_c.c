#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void require_ok(int condition)
{
    if ( !condition ) {
        fprintf(stderr, "tls_session_pair=fail\n");
        exit(1);
    }
}

static int tls_status_ok(xnet_result status)
{
    return status == XRT_NET_OK || status == XRT_NET_AGAIN;
}

static int verify_count = 0;
static int verify_default_ok = -1;
static size_t verify_cert_size = 0u;

static bool verify_accept(xtlssession* session, const void* cert_der, size_t cert_size, bool default_ok, void* user_data)
{
    int* counter = (int*)user_data;

    (void)session;
    if ( counter != NULL ) {
        *counter += 1;
    }
    verify_default_ok = default_ok ? 1 : 0;
    verify_cert_size = cert_size;
    return cert_der != NULL && cert_size > 0u;
}

static bool verify_reject(xtlssession* session, const void* cert_der, size_t cert_size, bool default_ok, void* user_data)
{
    (void)session;
    (void)cert_der;
    (void)cert_size;
    (void)default_ok;
    (void)user_data;
    return false;
}

static int move_cipher(xtlssession* source, xtlssession* target, size_t chunk_size)
{
    unsigned char buffer[64];
    int moved = 0;

    if ( chunk_size == 0u || chunk_size > sizeof(buffer) ) {
        chunk_size = sizeof(buffer);
    }
    while ( xrtNetTlsSessionPendingCipher(source) > 0u ) {
        size_t pending = xrtNetTlsSessionPendingCipher(source);
        size_t wanted = pending < chunk_size ? pending : chunk_size;
        size_t read_size = 0u;

        require_ok(xrtNetTlsSessionPeekCipher(source, buffer, wanted, &read_size) == XRT_NET_OK);
        require_ok(read_size > 0u);
        require_ok(tls_status_ok(xrtNetTlsSessionFeedCipher(target, buffer, read_size)));
        xrtNetTlsSessionConsumeCipher(source, read_size);
        moved = 1;
    }
    return moved;
}

static int handshake_pair_result(xtlssession* client, xtlssession* server)
{
    unsigned int step;

    for ( step = 0u; step < 4096u; ++step ) {
        xnet_result client_status = xrtNetTlsSessionDriveHandshake(client);
        xnet_result server_status = xrtNetTlsSessionDriveHandshake(server);
        if ( !tls_status_ok(client_status) || !tls_status_ok(server_status) ) {
            return -1;
        }
        (void)move_cipher(client, server, 37u);
        (void)move_cipher(server, client, 53u);
        if ( xrtNetTlsSessionIsReady(client) && xrtNetTlsSessionIsReady(server) ) {
            return 1;
        }
    }
    return 0;
}

static int handshake_pair(xtlssession* client, xtlssession* server)
{
    return handshake_pair_result(client, server) == 1;
}

int main(void)
{
    const char* cert_file = "release/examples/fixtures/tls_cert.pem";
    const char* key_file = "release/examples/fixtures/tls_key.pem";
    const char payload[] = "hello tls";
    const char* alpn;
    xtlsconfig server_config;
    xtlsconfig client_config;
    xtlsconfig reject_client_config;
    xtlsconfig bad_config;
    xtlssession* client = NULL;
    xtlssession* server = NULL;
    xtlssession* resumed_client = NULL;
    xtlssession* resumed_server = NULL;
    xtlssession* partial_client = NULL;
    xtlssession* partial_server = NULL;
    xtlssession* reject_client = NULL;
    xtlssession* reject_server = NULL;
    xtlssession* bad_server = NULL;
    xtlsresume* resume = NULL;
    unsigned char one_byte = 0u;
    char plain[32];
    const char* sni;
    size_t size = 0u;
    size_t close_bytes;

    memset(&server_config, 0, sizeof(server_config));
    memset(&client_config, 0, sizeof(client_config));
    memset(&reject_client_config, 0, sizeof(reject_client_config));
    memset(&bad_config, 0, sizeof(bad_config));
    memset(plain, 0, sizeof(plain));
    server_config.sCertFile = cert_file;
    server_config.sKeyFile = key_file;
    server_config.bVerifyPeer = false;
    server_config.bAllowTLS12Ed25519 = true;
    server_config.iMaxVersion = 0x0303u;
    server_config.sAlpnProtocols = "http/1.1";
    client_config.sHostName = "localhost";
    client_config.sAlpnProtocols = "h2,http/1.1";
    client_config.bVerifyPeer = false;
    client_config.bAllowTLS12Ed25519 = true;
    client_config.iMaxVersion = 0x0303u;
    client_config.OnVerify = verify_accept;
    client_config.pVerifyUserData = &verify_count;
    reject_client_config = client_config;
    reject_client_config.OnVerify = verify_reject;
    reject_client_config.pVerifyUserData = NULL;

    xrtInit();
    client = xrtNetTlsSessionCreate(&client_config, false);
    server = xrtNetTlsSessionCreate(&server_config, true);
    require_ok(client != NULL && server != NULL);
    require_ok(xrtNetTlsSessionSetCert(server, cert_file, key_file) == XRT_NET_OK);
    require_ok(handshake_pair(client, server));
    sni = xrtNetTlsSessionGetSNI(server);
    require_ok(sni != NULL && strcmp(sni, "localhost") == 0);
    alpn = xrtNetTlsSessionGetALPN(client);
    require_ok(alpn != NULL && strcmp(alpn, "http/1.1") == 0);
    alpn = xrtNetTlsSessionGetALPN(server);
    require_ok(alpn != NULL && strcmp(alpn, "http/1.1") == 0);
    require_ok(verify_count >= 1);
    require_ok(verify_default_ok == 1);
    require_ok(verify_cert_size > 0u);

    require_ok(xrtNetTlsSessionWritePlain(client, payload, sizeof(payload) - 1u, &size) == XRT_NET_OK);
    require_ok(size == sizeof(payload) - 1u);
    require_ok(move_cipher(client, server, 19u));
    require_ok(xrtNetTlsSessionReadPlain(server, plain, sizeof(plain), &size) == XRT_NET_OK);
    require_ok(size == sizeof(payload) - 1u && memcmp(plain, payload, size) == 0);

    resume = xrtNetTlsSessionExportResume(client);
    require_ok(resume != NULL);
    require_ok(tls_status_ok(xrtNetTlsSessionQueueClose(client)));
    close_bytes = xrtNetTlsSessionPendingCipher(client);
    require_ok(tls_status_ok(xrtNetTlsSessionQueueClose(client)));
    require_ok(xrtNetTlsSessionPendingCipher(client) == close_bytes);

    client_config.pResume = resume;
    resumed_client = xrtNetTlsSessionCreate(&client_config, false);
    resumed_server = xrtNetTlsSessionCreate(&server_config, true);
    require_ok(resumed_client != NULL && resumed_server != NULL);
    require_ok(handshake_pair(resumed_client, resumed_server));
    require_ok(xrtNetTlsSessionWasResumed(resumed_client));
    require_ok(xrtNetTlsSessionWasResumed(resumed_server));
    alpn = xrtNetTlsSessionGetALPN(resumed_client);
    require_ok(alpn != NULL && strcmp(alpn, "http/1.1") == 0);
    alpn = xrtNetTlsSessionGetALPN(resumed_server);
    require_ok(alpn != NULL && strcmp(alpn, "http/1.1") == 0);

    bad_config.sCertFile = "missing-cert.pem";
    bad_config.sKeyFile = "missing-key.pem";
    bad_server = xrtNetTlsSessionCreate(&bad_config, true);
    require_ok(bad_server == NULL);

    client_config.pResume = NULL;
    partial_client = xrtNetTlsSessionCreate(&client_config, false);
    partial_server = xrtNetTlsSessionCreate(&server_config, true);
    require_ok(partial_client != NULL && partial_server != NULL);
    require_ok(tls_status_ok(xrtNetTlsSessionDriveHandshake(partial_client)));
    require_ok(xrtNetTlsSessionPendingCipher(partial_client) > 1u);
    require_ok(xrtNetTlsSessionPeekCipher(partial_client, &one_byte, 1u, &size) == XRT_NET_OK && size == 1u);
    require_ok(tls_status_ok(xrtNetTlsSessionFeedCipher(partial_server, &one_byte, 1u)));
    require_ok(xrtNetTlsSessionDriveHandshake(partial_server) == XRT_NET_AGAIN);
    require_ok(!xrtNetTlsSessionIsReady(partial_server));

    reject_client = xrtNetTlsSessionCreate(&reject_client_config, false);
    reject_server = xrtNetTlsSessionCreate(&server_config, true);
    require_ok(reject_client != NULL && reject_server != NULL);
    require_ok(handshake_pair_result(reject_client, reject_server) == -1);

    xrtNetTlsSessionDestroy(reject_server);
    xrtNetTlsSessionDestroy(reject_client);
    xrtNetTlsSessionDestroy(partial_server);
    xrtNetTlsSessionDestroy(partial_client);
    xrtNetTlsSessionDestroy(resumed_server);
    xrtNetTlsSessionDestroy(resumed_client);
    xrtNetTlsResumeDestroy(resume);
    xrtNetTlsSessionDestroy(server);
    xrtNetTlsSessionDestroy(client);
    xrtUnit();
    printf("tls_session_pair=ok\n");
    return 0;
}
