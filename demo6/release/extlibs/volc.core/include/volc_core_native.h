#ifndef XVOLC_CORE_NATIVE_H
#define XVOLC_CORE_NATIVE_H

#ifdef __cplusplus
extern "C" {
#endif

/* 返回值与诊断位于当前线程的缓存，换线程或长期保存前必须复制。 */
int x_volc_core_available(void);
const char* x_volc_core_version(void);
const char* x_volc_core_last_error(void);
int x_volc_core_last_status(void);

const char* x_volc_core_sha256_hex(const char* text);
const char* x_volc_core_hmac_sha256_hex(const char* key, const char* text);
const char* x_volc_core_hmac_sha256_hex_key(const char* hex_key, const char* text);
const char* x_volc_core_canonical_query(const char* action, const char* version, const char* extra_pairs);
const char* x_volc_core_json_string(const char* text);

const char* x_volc_core_sign_preview(const char* method, const char* host, const char* path,
    const char* service, const char* region, const char* action, const char* version,
    const char* access_key, const char* secret_key, const char* session_token,
    const char* content_type, const char* extra_pairs, const char* body, const char* x_date);
const char* x_volc_core_sign_preview_acep(const char* action, const char* version,
    const char* access_key, const char* secret_key, const char* session_token,
    const char* body, const char* x_date);
const char* x_volc_core_sign_preview_sts(const char* access_key, const char* secret_key,
    const char* role_trn, const char* role_session_name, int duration_seconds, const char* x_date);
const char* x_volc_core_sign_preview_acep_spec(const char* spec);
const char* x_volc_core_sign_preview_sts_spec(const char* spec);

const char* x_volc_core_request(const char* method, const char* host, const char* path,
    const char* service, const char* region, const char* action, const char* version,
    const char* access_key, const char* secret_key, const char* session_token,
    const char* content_type, const char* extra_pairs, const char* body, int timeout_ms);
const char* x_volc_core_request_acep(const char* action, const char* version,
    const char* access_key, const char* secret_key, const char* session_token,
    const char* body, int timeout_ms);
const char* x_volc_core_request_sts(const char* access_key, const char* secret_key,
    const char* role_trn, const char* role_session_name, int duration_seconds, int timeout_ms);
const char* x_volc_core_request_acep_spec(const char* spec);
const char* x_volc_core_request_sts_spec(const char* spec);

#ifdef __cplusplus
}
#endif

#endif
