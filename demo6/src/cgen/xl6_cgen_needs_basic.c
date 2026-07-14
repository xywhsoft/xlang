#include "src/cgen/xl6_cgen_needs_basic.h"

#include <string.h>

static int xl6CgenNeedsBasicTextEquals(const char* a, const char* b)
{
    return (a != NULL) && (b != NULL) && (strcmp(a, b) == 0);
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
            xl6CgenNeedsBasicTextEquals(node->text, "TlsResume"));
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

static int xl6CgenNeedsBasicIsRegexClass(const XL6_AstNode* node)
{
    return node != NULL &&
           node->kind == XL6_AST_CLASS &&
           node->text != NULL &&
           (xl6CgenNeedsBasicTextEquals(node->text, "Regex") ||
            xl6CgenNeedsBasicTextEquals(node->text, "RegexMatch") ||
            xl6CgenNeedsBasicTextEquals(node->text, "RegexSet") ||
            xl6CgenNeedsBasicTextEquals(node->text, "RegexBuilder"));
}

static int xl6CgenNeedsBasicIsJsonClass(const XL6_AstNode* node)
{
    return node != NULL &&
           node->kind == XL6_AST_CLASS &&
           node->text != NULL &&
           (xl6CgenNeedsBasicTextEquals(node->text, "JsonWriter") ||
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
           xl6CgenNeedsBasicTextEquals(name, "NetProxy") ||
           xl6CgenNeedsBasicTextEquals(name, "NetEngine") ||
           xl6CgenNeedsBasicTextEquals(name, "TcpStream") ||
           xl6CgenNeedsBasicTextEquals(name, "TcpServer") ||
           xl6CgenNeedsBasicTextEquals(name, "TcpConnectConfig") ||
           xl6CgenNeedsBasicTextEquals(name, "TcpListenConfig") ||
           xl6CgenNeedsBasicTextEquals(name, "UdpSocket") ||
           xl6CgenNeedsBasicTextEquals(name, "UdpPacket") ||
           xl6CgenNeedsBasicTextEquals(name, "UdpConfig");
}

static int xl6CgenNeedsBasicIsLoggerClass(const XL6_AstNode* node)
{
    return node != NULL &&
           node->kind == XL6_AST_CLASS &&
           node->text != NULL &&
           (xl6CgenNeedsBasicTextEquals(node->text, "Logger") ||
            xl6CgenNeedsBasicTextEquals(node->text, "LogAppender"));
}

static int xl6CgenNeedsBasicIsHttpServerClass(const XL6_AstNode* node)
{
    return node != NULL &&
           node->kind == XL6_AST_CLASS &&
           node->text != NULL &&
           (xl6CgenNeedsBasicTextEquals(node->text, "HttpRequest") ||
            xl6CgenNeedsBasicTextEquals(node->text, "HttpResponse"));
}

static int xl6CgenNeedsBasicIsHttpClientClass(const XL6_AstNode* node)
{
    return node != NULL &&
           node->kind == XL6_AST_CLASS &&
           node->text != NULL &&
           (xl6CgenNeedsBasicTextEquals(node->text, "HttpClient") ||
            xl6CgenNeedsBasicTextEquals(node->text, "HttpClientRequest") ||
            xl6CgenNeedsBasicTextEquals(node->text, "HttpClientResponse") ||
            xl6CgenNeedsBasicTextEquals(node->text, "HttpDiagnostics"));
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

static int xl6CgenNeedsBasicIsUrlRuntimeCall(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }
    return xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_url_part") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_url_flag") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_url_port") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_url_default_port") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_url_is_default_port") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_url_host_header") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_url_build") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_url_resolve") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_url_normalize_path");
}

static int xl6CgenNeedsBasicIsQueryRuntimeCall(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }
    return xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_query_encode") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_query_decode") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_query_valid") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_query_pair") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_query_append") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_query_append_name") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_query_count") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_query_find") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_query_items");
}

static int xl6CgenNeedsBasicIsRegexRuntimeCall(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }
    return xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_regex_is_match") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_regex_full_match") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_regex_find") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_regex_match") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_regex_set_create") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_regex_valid_pattern") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_regex_error_message") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_regex_error_pos") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_regex_error_byte_pos") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_regex_set_matches");
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
    return xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_json_visit") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_xson_parse_options") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_xson_valid_options") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_xson_read_options") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_xson_stringify_options") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_xson_write_options");
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
    return xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_string_split") ||
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
    return xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_path_join");
}

static int xl6CgenNeedsBasicIsHttpServerRuntimeCall(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }
    return xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_server_create") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_server_create_tls") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_app_create") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_app_destroy") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_app_route") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_app_route_request") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_app_get") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_app_post") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_app_any") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_app_get_request") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_app_post_request") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_app_any_request") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_app_static") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_app_static_ex") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_app_error") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_server_set_app") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_server_reload_app") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_server_host") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_server_remove_host") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_server_error") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_server_route") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_server_route_request") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_server_get") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_server_post") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_server_any") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_server_get_request") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_server_post_request") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_server_any_request") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_server_static") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_server_static_ex") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_server_start") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_server_port") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_server_request_count") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_server_connection_count") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_server_stop") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_server_drain") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_server_destroy") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_mime_by_path");
}

static int xl6CgenNeedsBasicIsHttpClientRuntimeCall(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }
    return xl6CgenNeedsBasicTextStartsWith(callee->text, "xl6_xrt_http_client_") ||
           xl6CgenNeedsBasicTextStartsWith(callee->text, "xl6_xrt_http_diagnostics_");
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
    return xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_xrt_file_read_text_strict");
}

int xl6CgenNodeNeedsRangeContains(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_BINARY && xl6CgenNeedsBasicTextEquals(node->text, "in") &&
         node->childCount > 1 && xl6CgenNeedsBasicIsRangeCall(node->children[1]) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsRangeContains(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsIntPow(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_BINARY && xl6CgenNeedsBasicTextEquals(node->text, "**") ) {
        return 1;
    }
    if ( node->kind == XL6_AST_ASSIGN && xl6CgenNeedsBasicTextEquals(node->text, "**=") ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsIntPow(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsFunctionRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_ANON_FUNC ) {
        return 1;
    }
    if ( node->kind == XL6_AST_TYPE && xl6CgenNeedsBasicTextEquals(node->text, "function") ) {
        return 1;
    }
    if ( node->kind == XL6_AST_CALL &&
         node->childCount > 0 &&
         node->children[0] != NULL &&
         node->children[0]->kind == XL6_AST_NAME &&
         xl6CgenNeedsBasicTextEquals(node->children[0]->text, "thread") ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsFunctionRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsApprox(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_BINARY && xl6CgenNeedsBasicTextEquals(node->text, "~=") ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsApprox(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsTimeRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_TIME ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsTimeRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsXValueRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsBuiltinObjectClass(node) ) {
        return 0;
    }
    if ( node->kind == XL6_AST_TYPE && xl6CgenNeedsBasicIsXValueType(node) ) {
        return 1;
    }
    if ( node->kind == XL6_AST_CALL && node->childCount > 0 &&
         node->children[0] != NULL && node->children[0]->kind == XL6_AST_NAME &&
         xl6CgenNeedsBasicTextEquals(node->children[0]->text, "type") ) {
        return 1;
    }
    if ( node->kind == XL6_AST_TYPE_IS || node->kind == XL6_AST_TYPE_AS ) {
        return 1;
    }
    if ( xl6CgenNeedsBasicIsToStringMemberCallSyntax(node) ) {
        return 1;
    }
    if ( xl6CgenNeedsBasicIsValueDerive(node) ||
         xl6CgenNeedsBasicIsPersistenceGenericCall(node) ) {
        return 1;
    }
    if ( node->kind == XL6_AST_CALL && node->childCount > 0 &&
         node->children[0] != NULL && node->children[0]->kind == XL6_AST_NAME &&
         xl6CgenNeedsBasicTextEquals(node->children[0]->text, "var") ) {
        return 1;
    }
    if ( xl6CgenNeedsBasicIsPrintIntrinsicCall(node) ) {
        return 1;
    }
    if ( xl6CgenNeedsBasicIsContainerMethodCall(node) ) {
        return 1;
    }
    if ( node->kind == XL6_AST_ARRAY_LITERAL || node->kind == XL6_AST_LIST_LITERAL ||
         node->kind == XL6_AST_SET_LITERAL || node->kind == XL6_AST_DICT_LITERAL ||
         node->kind == XL6_AST_TYPED_LITERAL ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsXValueRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsCryptoRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsCryptoClass(node) ) {
        return 1;
    }
    if ( xl6CgenNeedsBasicIsCryptoRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsCryptoRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsUrlRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsUrlRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsUrlRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsQueryRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsQueryRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsQueryRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsXidRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsXidClass(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsXidRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsRegexRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsRegexClass(node) ) {
        return 1;
    }
    if ( xl6CgenNeedsBasicIsRegexRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsRegexRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsJsonRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsJsonClass(node) || xl6CgenNeedsBasicIsJsonRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsJsonRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsProcessRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsProcessClass(node) ) {
        return 1;
    }
    if ( xl6CgenNeedsBasicIsProcessRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsProcessRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsNetRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsNetClass(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsNetRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsDirRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsDirRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsDirRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsStringEdgeRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsStringEdgeRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsStringEdgeRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsStdStringRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsStdStringRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsStdStringRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsPathRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsPathRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsPathRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsHttpServerRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsHttpServerClass(node) ) {
        return 1;
    }
    if ( xl6CgenNeedsBasicIsHttpServerRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsHttpServerRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsHttpClientRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsHttpClientClass(node) ) {
        return 1;
    }
    if ( xl6CgenNeedsBasicIsHttpClientRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsHttpClientRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsBufferRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsBufferClass(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsBufferRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsTlsRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsTlsClass(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsTlsRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

#include "src/cgen/xl6_cgen_needs_stdlib.c"
