#include "src/cgen/xl6_cgen_needs_basic.h"

#include <string.h>

static int xl6CgenNeedsBasicTextEquals(const char* a, const char* b)
{
    return (a != NULL) && (b != NULL) && (strcmp(a, b) == 0);
}

static int xl6CgenNeedsBasicExternTextEquals(const char* text, const char* symbol)
{
    size_t symbolLength;
    size_t textLength;

    if ( xl6CgenNeedsBasicTextEquals(text, symbol) ) {
        return 1;
    }
    if ( text == NULL || symbol == NULL ) {
        return 0;
    }
    symbolLength = strlen(symbol);
    textLength = strlen(text);
    if ( textLength != symbolLength + 2u ) {
        return 0;
    }
    return ((text[0] == '"' && text[symbolLength + 1u] == '"') ||
            (text[0] == '\'' && text[symbolLength + 1u] == '\'')) &&
           strncmp(text + 1, symbol, symbolLength) == 0;
}

static int xl6CgenNeedsBasicTextStartsWith(const char* text, const char* prefix)
{
    size_t prefixLen;

    if ( text == NULL || prefix == NULL ) {
        return 0;
    }
    prefixLen = strlen(prefix);
    return strncmp(text, prefix, prefixLen) == 0;
}

static int xl6CgenNeedsBasicIsRangeCall(const XL6_AstNode* node)
{
    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount < 2 ) {
        return 0;
    }
    return node->children[0] != NULL &&
           node->children[0]->kind == XL6_AST_NAME &&
           xl6CgenNeedsBasicTextEquals(node->children[0]->text, "range");
}

static int xl6CgenNeedsBasicIsPrintIntrinsicCall(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    return callee != NULL &&
           callee->kind == XL6_AST_NAME &&
           (xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_print") ||
            xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_println"));
}

static int xl6CgenNeedsBasicIsArrayXValueType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CgenNeedsBasicTextEquals(typeNode->text, "array") ||
            xl6CgenNeedsBasicTextEquals(typeNode->text, "array<var>"));
}

static int xl6CgenNeedsBasicIsListXValueType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CgenNeedsBasicTextEquals(typeNode->text, "list") ||
            xl6CgenNeedsBasicTextEquals(typeNode->text, "list<var>"));
}

static int xl6CgenNeedsBasicIsSetXValueType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CgenNeedsBasicTextEquals(typeNode->text, "set") ||
            xl6CgenNeedsBasicTextEquals(typeNode->text, "set<var>"));
}

static int xl6CgenNeedsBasicIsDictXValueType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CgenNeedsBasicTextEquals(typeNode->text, "dict") ||
            xl6CgenNeedsBasicTextEquals(typeNode->text, "dict<var>"));
}

static int xl6CgenNeedsBasicIsXValueType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CgenNeedsBasicTextEquals(typeNode->text, "var") ||
            xl6CgenNeedsBasicTextEquals(typeNode->text, "__class_handle") ||
            xl6CgenNeedsBasicIsArrayXValueType(typeNode) ||
            xl6CgenNeedsBasicIsListXValueType(typeNode) ||
            xl6CgenNeedsBasicIsSetXValueType(typeNode) ||
            xl6CgenNeedsBasicIsDictXValueType(typeNode));
}

int xl6CgenNodeNeedsClassHandleRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_CLASS ||
         (node->kind == XL6_AST_TYPE && node->text != NULL &&
          xl6CgenNeedsBasicTextEquals(node->text, "__class_handle")) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsClassHandleRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenNeedsBasicIsContainerMethodCall(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;
    const char* name;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_MEMBER || callee->text == NULL ) {
        return 0;
    }
    name = callee->text;
    return xl6CgenNeedsBasicTextEquals(name, "push") ||
           xl6CgenNeedsBasicTextEquals(name, "append") ||
           xl6CgenNeedsBasicTextEquals(name, "insert") ||
           xl6CgenNeedsBasicTextEquals(name, "remove") ||
           xl6CgenNeedsBasicTextEquals(name, "pop") ||
           xl6CgenNeedsBasicTextEquals(name, "take") ||
           xl6CgenNeedsBasicTextEquals(name, "clear") ||
           xl6CgenNeedsBasicTextEquals(name, "reserve") ||
           xl6CgenNeedsBasicTextEquals(name, "swap") ||
           xl6CgenNeedsBasicTextEquals(name, "add") ||
           xl6CgenNeedsBasicTextEquals(name, "contains");
}

static int xl6CgenNeedsBasicIsBuiltinObjectClass(const XL6_AstNode* node)
{
    return node != NULL &&
           node->kind == XL6_AST_CLASS &&
           node->text != NULL &&
           xl6CgenNeedsBasicTextEquals(node->text, "object");
}

static int xl6CgenNeedsBasicIsBufferClass(const XL6_AstNode* node)
{
    return node != NULL &&
           node->kind == XL6_AST_CLASS &&
           node->text != NULL &&
           xl6CgenNeedsBasicTextEquals(node->text, "Buffer");
}

static int xl6CgenNeedsBasicIsTlsClass(const XL6_AstNode* node)
{
    return node != NULL &&
           node->kind == XL6_AST_CLASS &&
           node->text != NULL &&
           (xl6CgenNeedsBasicTextEquals(node->text, "TlsConfig") ||
            xl6CgenNeedsBasicTextEquals(node->text, "TlsSession") ||
            xl6CgenNeedsBasicTextEquals(node->text, "TlsResume") ||
            xl6CgenNeedsBasicTextEquals(node->text, "TlsStream") ||
            xl6CgenNeedsBasicTextEquals(node->text, "TlsServer") ||
            xl6CgenNeedsBasicTextEquals(node->text, "TlsServerStats"));
}

static int xl6CgenNeedsBasicIsXidClass(const XL6_AstNode* node)
{
    return node != NULL &&
           node->kind == XL6_AST_CLASS &&
           node->text != NULL &&
           xl6CgenNeedsBasicTextEquals(node->text, "Xid");
}

static int xl6CgenNeedsBasicIsAsyncFileClass(const XL6_AstNode* node)
{
    return node != NULL &&
           node->kind == XL6_AST_CLASS &&
           node->text != NULL &&
           xl6CgenNeedsBasicTextEquals(node->text, "AsyncFile");
}

static int xl6CgenNeedsBasicIsJsonClass(const XL6_AstNode* node)
{
    return node != NULL &&
           node->kind == XL6_AST_CLASS &&
           node->text != NULL &&
           (xl6CgenNeedsBasicTextEquals(node->text, "JsonWriter") ||
            xl6CgenNeedsBasicTextEquals(node->text, "XsonWriter") ||
            xl6CgenNeedsBasicTextEquals(node->text, "XsonOptions"));
}

static int xl6CgenNeedsBasicIsProcessClass(const XL6_AstNode* node)
{
    return node != NULL &&
           node->kind == XL6_AST_CLASS &&
           node->text != NULL &&
           (xl6CgenNeedsBasicTextEquals(node->text, "Process") ||
            xl6CgenNeedsBasicTextEquals(node->text, "ProcessCommand") ||
            xl6CgenNeedsBasicTextEquals(node->text, "ProcessResult") ||
            xl6CgenNeedsBasicTextEquals(node->text, "PipelineResult"));
}

static int xl6CgenNeedsBasicIsNetClass(const XL6_AstNode* node)
{
    const char* name;

    if ( node == NULL || node->kind != XL6_AST_CLASS || node->text == NULL ) {
        return 0;
    }
    name = node->text;
    return xl6CgenNeedsBasicTextEquals(name, "NetAddress") ||
           xl6CgenNeedsBasicTextEquals(name, "NetAddressList") ||
           xl6CgenNeedsBasicTextEquals(name, "NetInterfaceAddress") ||
           xl6CgenNeedsBasicTextEquals(name, "NetInterface") ||
           xl6CgenNeedsBasicTextEquals(name, "NetInterfaceList") ||
           xl6CgenNeedsBasicTextEquals(name, "NetBytes") ||
           xl6CgenNeedsBasicTextEquals(name, "NetEngineStats") ||
           xl6CgenNeedsBasicTextEquals(name, "NetResolverStats") ||
           xl6CgenNeedsBasicTextEquals(name, "NetProxy") ||
           xl6CgenNeedsBasicTextEquals(name, "NetEngine") ||
           xl6CgenNeedsBasicTextEquals(name, "TcpStream") ||
           xl6CgenNeedsBasicTextEquals(name, "TcpStreamStats") ||
           xl6CgenNeedsBasicTextEquals(name, "TcpServer") ||
           xl6CgenNeedsBasicTextEquals(name, "TcpServerStats") ||
           xl6CgenNeedsBasicTextEquals(name, "TcpConnectConfig") ||
           xl6CgenNeedsBasicTextEquals(name, "TcpListenConfig") ||
           xl6CgenNeedsBasicTextEquals(name, "UdpSocket") ||
           xl6CgenNeedsBasicTextEquals(name, "UdpPacket") ||
           xl6CgenNeedsBasicTextEquals(name, "UdpBatch") ||
           xl6CgenNeedsBasicTextEquals(name, "UdpStats") ||
           xl6CgenNeedsBasicTextEquals(name, "UdpConfig") ||
           xl6CgenNeedsBasicTextEquals(name, "TlsStream") ||
           xl6CgenNeedsBasicTextEquals(name, "TlsServer") ||
           xl6CgenNeedsBasicTextEquals(name, "TlsServerStats");
}

static int xl6CgenNeedsBasicIsLoggerClass(const XL6_AstNode* node)
{
    return node != NULL &&
           node->kind == XL6_AST_CLASS &&
           node->text != NULL &&
           (xl6CgenNeedsBasicTextEquals(node->text, "Logger") ||
            xl6CgenNeedsBasicTextEquals(node->text, "LogSink"));
}

static int xl6CgenNeedsBasicIsSignalClass(const XL6_AstNode* node)
{
    return node != NULL &&
           node->kind == XL6_AST_CLASS &&
           node->text != NULL &&
           xl6CgenNeedsBasicTextEquals(node->text, "SignalHandle");
}

static int xl6CgenNeedsBasicIsTemplateClass(const XL6_AstNode* node)
{
    return node != NULL &&
           node->kind == XL6_AST_CLASS &&
           node->text != NULL &&
           (xl6CgenNeedsBasicTextEquals(node->text, "Template") ||
            xl6CgenNeedsBasicTextEquals(node->text, "TemplateEngine"));
}

static int xl6CgenNeedsBasicIsFileClass(const XL6_AstNode* node)
{
    return node != NULL &&
           node->kind == XL6_AST_CLASS &&
           node->text != NULL &&
           xl6CgenNeedsBasicTextEquals(node->text, "File");
}

static int xl6CgenNeedsBasicIsCryptoClass(const XL6_AstNode* node)
{
    return node != NULL &&
           node->kind == XL6_AST_CLASS &&
           node->text != NULL &&
           (xl6CgenNeedsBasicTextEquals(node->text, "Sha1") ||
            xl6CgenNeedsBasicTextEquals(node->text, "Sha256") ||
            xl6CgenNeedsBasicTextEquals(node->text, "Sha384") ||
            xl6CgenNeedsBasicTextEquals(node->text, "Sha512"));
}

static int xl6CgenNeedsBasicIsToStringMemberCallSyntax(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* args;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }

    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_MEMBER ||
         !xl6CgenNeedsBasicTextEquals(callee->text, "toString") ) {
        return 0;
    }

    args = node->childCount > 1 ? node->children[1] : NULL;
    return args == NULL || args->childCount == 0;
}

static int xl6CgenNeedsBasicIsValueDerive(const XL6_AstNode* node)
{
    return node != NULL &&
           node->kind == XL6_AST_DERIVE &&
           (xl6CgenNeedsBasicTextEquals(node->text, "value") ||
            xl6CgenNeedsBasicTextEquals(node->text, "json") ||
            xl6CgenNeedsBasicTextEquals(node->text, "xson"));
}

static int xl6CgenNeedsBasicIsPersistenceGenericCall(const XL6_AstNode* node)
{
    const XL6_AstNode* instant;
    const XL6_AstNode* nameNode;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    instant = node->children[0];
    if ( instant == NULL || instant->kind != XL6_AST_GENERIC_INSTANT || instant->childCount == 0 ) {
        return 0;
    }
    nameNode = instant->children[0];
    return nameNode != NULL &&
           nameNode->kind == XL6_AST_NAME &&
           (xl6CgenNeedsBasicTextEquals(nameNode->text, "__xl_std_json_parse") ||
            xl6CgenNeedsBasicTextEquals(nameNode->text, "__xl_std_json_read") ||
            xl6CgenNeedsBasicTextEquals(nameNode->text, "__xl_std_xson_parse") ||
            xl6CgenNeedsBasicTextEquals(nameNode->text, "__xl_std_xson_read"));
}

static int xl6CgenNeedsBasicIsCryptoRuntimeCall(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }
    return xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_sha1_hex") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_sha256_hex") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_sha384_hex") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_sha512_hex") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_hmac_sha256_hex") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_hmac_sha384_hex") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_hmac_sha512_hex") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_chacha20_encrypt_hex") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_chacha20_decrypt_text") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_chacha20_poly1305_encrypt_hex") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_chacha20_poly1305_decrypt_text") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_aes128_gcm_encrypt_hex") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_aes128_gcm_decrypt_text") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_aes256_gcm_encrypt_hex") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_aes256_gcm_decrypt_text") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_hkdf_sha256_hex") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_hkdf_sha384_hex") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_random_hex") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_sha1_bytes") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_sha256_bytes") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_sha384_bytes") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_sha512_bytes") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_hmac_sha256_bytes") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_hmac_sha384_bytes") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_hmac_sha512_bytes") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_chacha20_bytes") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_chacha20_poly1305_encrypt_bytes") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_chacha20_poly1305_decrypt_bytes") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_aes128_gcm_encrypt_bytes") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_aes128_gcm_decrypt_bytes") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_aes256_gcm_encrypt_bytes") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_aes256_gcm_decrypt_bytes") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_hkdf_sha256_bytes") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_hkdf_sha384_bytes") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_random_bytes") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_ed25519_keypair") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_ed25519_public_key_hex") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_ed25519_sign_hex") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_ed25519_verify") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_x25519_keypair") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_x25519_shared_secret_hex") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_x448_keypair") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_x448_shared_secret_hex") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_p256_keypair") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_p256_shared_secret_hex") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_p384_keypair") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_p384_shared_secret_hex") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_ecdsa_verify") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_rsa_pss_verify") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_rsa_pkcs1_verify");
}

static int xl6CgenNeedsBasicIsJsonRuntimeCall(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }
    return xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_xrt_json_parse") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_xrt_json_valid") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_xrt_json_parse_file") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_xrt_json_stringify") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_xrt_json_write_file") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_json_visit") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_xson_parse_options") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_xson_valid_options") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_xson_read_options") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_xson_stringify_options") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_xson_write_options") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_xson_visit_options");
}

static int xl6CgenNeedsBasicIsProcessRuntimeCall(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }
    return xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_process_exec") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_process_success") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_process_exit_code") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_process_run") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_process_run_file") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_process_pipeline");
}

static int xl6CgenNeedsBasicIsLogRuntimeCall(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }
    return xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_log_create") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_log_default") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_log_default_set_level") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_log_default_get_level") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_log_default_add_console") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_log_default_add_file") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_log_default_add_rolling_file") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_log_default_write") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_log_default_flush") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_log_level_name");
}

static int xl6CgenNeedsBasicIsSignalRuntimeCall(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }
    return xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_signal_on0") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_signal_on1") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_signal_supported") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_signal_name") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_signal_restore") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_signal_reset") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_signal_send_to") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_signal_raise") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_signal_count") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_signal_received") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_signal_clear");
}

static int xl6CgenNeedsBasicIsEncodingRuntimeCall(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }
    return xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_encoding_char_size") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_encoding_is_utf8_text") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_encoding_is_utf8_bytes") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_encoding_detect_text") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_encoding_detect_bytes") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_encoding_to_bytes") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_encoding_from_bytes") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_encoding_convert_bytes") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_encoding_name") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_encoding_swap_utf16") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_encoding_swap_utf32");
}

static int xl6CgenNeedsBasicIsDirRuntimeCall(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }
    return xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_dir_list") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_dir_glob") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_dir_entries");
}

static int xl6CgenNeedsBasicIsStringEdgeRuntimeCall(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }
    return xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_string_starts_with") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_string_ends_with");
}

static int xl6CgenNeedsBasicIsStdStringRuntimeCall(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }
    return xl6CgenNeedsBasicTextStartsWith(callee->text, "__xl_std_xrt_string_") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_string_split") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_string_chars") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_string_bytes") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_string_join") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_string_split_lines");
}

static int xl6CgenNeedsBasicIsPathRuntimeCall(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }
    return xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_path_join") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_xrt_path_random") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_xrt_path_sep") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_xrt_path_normalize");
}

static int xl6CgenNeedsBasicIsTemplateRuntimeCall(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }
    return xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_template_render");
}

static int xl6CgenNeedsBasicIsAsyncFileRuntimeCall(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }
    return xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_file_read_text_async") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_file_read_bytes_async_native") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_file_write_text_async") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_file_write_bytes_async_native") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_file_append_text_async") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_file_copy_async") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_file_move_async") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_file_delete_async") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_dir_create_async") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_dir_create_all_async") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_dir_copy_async") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_dir_move_async") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_dir_delete_async");
}

static int xl6CgenNeedsBasicIsFileRuntimeCall(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }
    /* Buffer 文件桥仍调用同步文件底座，不能依赖其他 file API 间接启用。 */
    return xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_file_read_bytes") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_file_write_bytes") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_file_append_bytes") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_xrt_file_delete") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_xrt_file_get_size") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_xrt_file_get_attr") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_xrt_file_set_attr") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_xrt_file_get_access_time") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_xrt_file_get_change_time") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_xrt_file_write_all") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_xrt_file_append") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_xrt_file_read_all") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_xrt_file_read_text_strict") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_xrt_file_rename") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_xrt_file_replace") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_xrt_file_write_all_atomic");
}

