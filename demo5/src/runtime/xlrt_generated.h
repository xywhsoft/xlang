#ifndef XLRT_GENERATED_H
#define XLRT_GENERATED_H

#include "xlrt_profile.h"
#include "../../lib/xrt/xrt.h"

#ifndef XL_OBJECT_HEADER_MAGIC
#define XL_OBJECT_HEADER_MAGIC 0x584C4F42u
#endif

#ifndef XL_OBJECT_FLAG_DESTROYING
#define XL_OBJECT_FLAG_DESTROYING 0x00000001u
#endif

#ifndef XL_OBJECT_FLAG_DESTROYED
#define XL_OBJECT_FLAG_DESTROYED  0x00000002u
#endif

#ifndef XL_OBJECT_TYPE_INFO_DEFINED
#define XL_OBJECT_TYPE_INFO_DEFINED
typedef struct XL_ObjectMethodEntry {
	const char* sDispatchKey;
	void* pFunc;
} XL_ObjectMethodEntry;

typedef struct XL_ObjectWeakRef XL_ObjectWeakRef;
typedef int (*XL_ObjectGraphTraceVisitor)(xvalue value, void* pCtx);
typedef int (*XL_ObjectTraceStateFn)(const void* pState, XL_ObjectGraphTraceVisitor pVisit, void* pCtx);

typedef struct XL_ObjectTypeInfo {
	const char* sName;
	const struct XL_ObjectTypeInfo* pBaseType;
	void (*pDestroyFunc)(void* pBlob);
	const XL_ObjectMethodEntry* pMethods;
	size_t iMethodCount;
	int (*pVisibleExists)(void* pBlob, const char* sKey);
	xvalue (*pVisibleKeys)(void* pBlob);
} XL_ObjectTypeInfo;
#endif

#ifndef XL_OBJECT_HEADER_DEFINED
#define XL_OBJECT_HEADER_DEFINED
typedef struct XL_ObjectHeader {
	uint32_t uMagic;
	uint32_t uFlags;
	const XL_ObjectTypeInfo* pTypeInfo;
	void* pNativePayload;
	xvalue dynamicFields;
	XL_ObjectWeakRef* pWeakRefs;
	xvalue pOwner;
	struct XL_ObjectHeader* pTrackPrev;
	struct XL_ObjectHeader* pTrackNext;
} XL_ObjectHeader;
#endif

extern int xlrt_attach_generated_runtime(void);
extern void xlrt_detach_generated_runtime(int bAttachedHere);
extern const char* xlrt_host_name(void);
extern const char* xlrt_host_ip(void);
extern const char* xlrt_host_user(void);
extern const char* xlrt_host_os(void);
extern const char* xlrt_host_arch(void);
extern const char* xlrt_host_executable(void);
extern const char* xlrt_env_get(const char* sName);
extern int xlrt_env_exists(const char* sName);
extern int xlrt_env_set(const char* sName, const char* sValue);
extern const char* xlrt_env_cwd(void);
extern const char* xlrt_env_temp(void);
extern const char* xlrt_env_home(void);
extern void xlrt_native_error_clear(void);
extern void xlrt_native_error_raise(const char* sKind, const char* sMessage, int iCode, const char* sData);
extern int xlrt_native_error_has(void);
extern const char* xlrt_native_error_kind(void);
extern const char* xlrt_native_error_message(void);
extern int xlrt_native_error_code(void);
extern const char* xlrt_native_error_data(void);

extern void xl_core_print_int(int value);
extern void xl_core_print_float(double value);
extern void xl_core_print_string(const char* value);
extern void xl_core_print_bool(int value);
extern void xl_core_println_void(void);
extern void xl_core_println_int(int value);
extern void xl_core_println_float(double value);
extern void xl_core_println_string(const char* value);
extern void xl_core_println_bool(int value);
extern void xl_io_eprint_int(int value);
extern void xl_io_eprint_float(double value);
extern void xl_io_eprint_string(const char* value);
extern void xl_io_eprint_bool(int value);
extern void xl_io_eprintln_void(void);
extern void xl_io_eprintln_int(int value);
extern void xl_io_eprintln_float(double value);
extern void xl_io_eprintln_string(const char* value);
extern void xl_io_eprintln_bool(int value);
extern const char* xl_io_read_line(void);
extern void xl_io_flush(void);
extern int xl_core_len_string(const char* value);
extern int xl_core_len_array(xvalue value);
extern int xl_core_len_list(xvalue value);
extern int xl_core_len_set(xvalue value);
extern int xl_core_len_dict(xvalue value);
extern const char* xl_point_to_string(void* value);
extern const char* xl_value_to_string(xvalue value);
extern xvalue xl_value_clone_async_state(xvalue value);
extern const char* xl_debug_text(xvalue value);
extern void xl_debug_print(xvalue value);
extern void xl_debug_println(xvalue value);
extern void xl_log_debug(const char* sText);
extern void xl_log_info(const char* sText);
extern void xl_log_warn(const char* sText);
extern void xl_log_error(const char* sText);
extern void xl_trace_write(const char* sText);
extern int xl_regex_is_match(const char* sPattern, const char* sText);
extern const char* xl_regex_find(const char* sPattern, const char* sText);
extern xvalue xl_regex_captures(const char* sPattern, const char* sText);
extern int xl_regex_capture_count(const char* sPattern);
extern const char* xl_regex_replace_first(const char* sPattern, const char* sText, const char* sValue);
extern const char* xl_regex_replace_all(const char* sPattern, const char* sText, const char* sValue);
extern const char* xl_json_encode(xvalue value);
extern xvalue xl_json_decode(const char* sText);
extern void xl_array_append(xvalue value, xvalue item);
extern void xl_list_append(xvalue value, xvalue item);
extern const char* xl_array_join(xvalue value, const char* sSep);
extern const char* xl_list_join(xvalue value, const char* sSep);
extern void xl_array_clear(xvalue value);
extern void xl_list_clear(xvalue value);
extern void xl_set_clear(xvalue value);
extern void xl_dict_clear(xvalue value);
extern int xl_array_remove(xvalue value, int index);
extern int xl_list_remove(xvalue value, int index);
extern int xl_set_remove(xvalue value, xvalue item);
extern int xl_dict_remove(xvalue value, const char* key);
extern xvalue xl_array_take(xvalue value, int index);
extern xvalue xl_array_pop(xvalue value);
extern xvalue xl_list_take(xvalue value, int index);
extern xvalue xl_dict_take(xvalue value, const char* key);
extern int xl_set_has(xvalue value, xvalue item);
extern int xl_dict_has(xvalue value, const char* key);
extern xvalue xl_iter_keys(xvalue value);
extern xvalue xl_iter_values(xvalue value);
extern xvalue xl_iter_items(xvalue value);
extern xvalue xl_value_copy_preserve_refs(xvalue value);
extern xvalue xl_channel_create(int iCapacity);
extern int xl_channel_is_value(xvalue value);
extern int xl_channel_send(xvalue value, xvalue item);
extern xvalue xl_channel_recv(xvalue value);
extern int xl_channel_len(xvalue value);
extern int xl_channel_capacity(xvalue value);
extern int xl_channel_closed(xvalue value);
extern int xl_channel_close(xvalue value);
extern int xl_object_header_ready(const void* pBlob);
extern int xl_object_header_alive(const void* pBlob);
extern int xl_object_header_destroying(const void* pBlob);
extern int xl_object_header_destroyed(const void* pBlob);
extern void xl_object_header_init(void* pBlob);
extern void xl_object_header_bind(void* pBlob, const XL_ObjectTypeInfo* pTypeInfo);
extern void xl_object_header_clone(void* pDstBlob, const void* pSrcBlob, const XL_ObjectTypeInfo* pFallbackTypeInfo);
extern void xl_object_header_dispose(void* pBlob);
extern const XL_ObjectTypeInfo* xl_object_type(const void* pBlob);
extern void* xl_object_payload(const void* pBlob);
extern int xl_object_payload_set(void* pBlob, void* pPayload);
extern void* xl_object_payload_take(void* pBlob);
extern void* xl_object_dispatch(void* pBlob, const char* sDispatchKey);
extern int xl_object_delete_value(xvalue value);
extern void xl_object_destroy_if_last(xvalue value);
extern xvalue xl_object_bind_value(xvalue value);
extern xvalue xl_object_retain(xvalue value);
extern uint32_t xl_object_refcount(xvalue value);
extern void xl_object_release(xvalue value);
extern XL_ObjectWeakRef* xl_object_weak_new(xvalue value);
extern void xl_object_weak_delete(XL_ObjectWeakRef* pWeakRef);
extern int xl_object_weak_alive(const XL_ObjectWeakRef* pWeakRef);
extern xvalue xl_object_weak_lock(XL_ObjectWeakRef* pWeakRef);
extern xvalue xl_object_dynamic_fields(void* pBlob);
extern int xl_object_length(void* pBlob, int iDeclaredFieldCount);
extern int xl_object_exists(void* pBlob, const char* sKey);
extern xvalue xl_object_get(void* pBlob, const char* sKey, xvalue defaultValue, int bHasDefault);
extern int xl_object_set(void* pBlob, const char* sKey, xvalue value);
extern int xl_object_remove(void* pBlob, const char* sKey);
extern void xl_object_clear(void* pBlob);
extern xvalue xl_object_keys(void* pBlob);
extern xvalue xl_object_values(void* pBlob);
extern xvalue xl_object_items(void* pBlob);
extern int xl_object_meta_has_field(xvalue value, const char* sKey);
extern int xl_object_meta_has_method(xvalue value, const char* sKey);
extern xvalue xl_object_meta_field_names(xvalue value);
extern int xl_object_meta_field_count(xvalue value);
extern const char* xl_object_meta_field_name_at(xvalue value, int index, const char* sDefaultValue);
extern xvalue xl_object_meta_method_names(xvalue value);
extern int xl_object_meta_method_count(xvalue value);
extern const char* xl_object_meta_method_name_at(xvalue value, int index, const char* sDefaultValue);
extern int xl_object_collect_cycles(void);
extern int xl_object_trace_blob(const void* pBlob, XL_ObjectGraphTraceVisitor pVisit, void* pCtx);
extern int xl_object_future_bind_trace(xfuture* pFuture, const void* pState, XL_ObjectTraceStateFn pTraceState);
extern int xl_object_future_clear_trace(xfuture* pFuture);
extern int xl_object_future_clear_promise_trace(xpromise* pPromise);
extern int xl_object_future_trace_handle(const xfuture* pFuture, XL_ObjectGraphTraceVisitor pVisit, void* pCtx);
extern void* xl_sync_mutex(void);
extern int xl_sync_mutex_free(void* pHandle);
extern int xl_sync_mutex_lock(void* pHandle);
extern int xl_sync_mutex_try_lock(void* pHandle);
extern int xl_sync_mutex_unlock(void* pHandle);
extern void* xl_sync_cond(void);
extern int xl_sync_cond_free(void* pHandle);
extern int xl_sync_cond_wait(void* pHandle, void* pMutex);
extern int xl_sync_cond_wait_timeout(void* pHandle, void* pMutex, int iTimeoutMs);
extern int xl_sync_cond_signal(void* pHandle);
extern int xl_sync_cond_broadcast(void* pHandle);
extern void* xl_sync_rwlock(void);
extern int xl_sync_rwlock_free(void* pHandle);
extern int xl_sync_rwlock_read_lock(void* pHandle);
extern int xl_sync_rwlock_try_read_lock(void* pHandle);
extern int xl_sync_rwlock_read_unlock(void* pHandle);
extern int xl_sync_rwlock_write_lock(void* pHandle);
extern int xl_sync_rwlock_try_write_lock(void* pHandle);
extern int xl_sync_rwlock_write_unlock(void* pHandle);
extern int xl_sync_rwlock_downgrade(void* pHandle);
extern int xl_sync_rwlock_upgrade(void* pHandle);
extern void* xl_atomic_make(int iValue);
extern int xl_atomic_free(void* pHandle);
extern int xl_atomic_load(void* pHandle);
extern int xl_atomic_store(void* pHandle, int iValue);
extern int xl_atomic_exchange(void* pHandle, int iValue);
extern int xl_atomic_compare_exchange(void* pHandle, int iExpected, int iDesired);
extern int xl_atomic_add(void* pHandle, int iDelta);
extern int xl_thread_current_id(void);
extern void xl_thread_sleep(int iMs);
extern void xl_thread_yield_now(void);
extern xvalue xl_sys_loadlib(const char* sPath);
extern void* xl_sys_lib_proc(xvalue value, const char* sName);
extern int xl_sys_lib_close(xvalue value);
extern const char* xl_sys_lib_path(xvalue value);
extern int xl_sys_lib_loaded(xvalue value);

extern xtime xl_time_now(void);
extern const char* xl_time_to_string(xtime value);
extern const char* xl_time_now_text(void);
extern const char* xl_time_date_text(void);
extern const char* xl_time_time_text(void);
extern const char* xl_time_relative(xtime value, xtime base);
extern xtime xl_time_parse(const char* sText);
extern const char* xl_time_format(xtime value, const char* sPattern);
extern xtime xl_time_parse_format(const char* sText, const char* sPattern);
extern void xl_time_sleep(int iMs);
extern int xl_time_is_leap_year(int iYear);

extern const char* xl_path_join(const char* sLeft, const char* sRight);
extern const char* xl_path_name_ext(const char* sPath);
extern const char* xl_path_name(const char* sPath);
extern const char* xl_path_ext(const char* sPath);
extern const char* xl_path_dir(const char* sPath);
extern int xl_path_exists(const char* sPath);
extern int xl_path_is_abs(const char* sPath);
extern const char* xl_path_random(const char* sHead, const char* sFoot, int iLen);

extern int xl_fs_exists(const char* sPath);
extern int xl_fs_is_file(const char* sPath);
extern int xl_fs_is_dir(const char* sPath);
extern int xl_fs_size(const char* sPath);
extern int xl_fs_set_size(const char* sPath, int iSize);
extern xtime xl_fs_access_time(const char* sPath);
extern xtime xl_fs_change_time(const char* sPath);
extern const char* xl_fs_read_all(const char* sPath);
extern int xl_fs_write_all(const char* sPath, const char* sText);
extern int xl_fs_append_all(const char* sPath, const char* sText);
extern int xl_fs_mkdir_all(const char* sPath);
extern int xl_fs_copy(const char* sSrc, const char* sDst);
extern int xl_fs_move(const char* sSrc, const char* sDst);
extern int xl_fs_delete(const char* sPath);

extern int xl_str_contains(const char* sText, const char* sPart);
extern int xl_str_empty(const char* sText);
extern int xl_str_byte_len(const char* sText);
extern int xl_str_index_of(const char* sText, const char* sPart);
extern int xl_str_last_index_of(const char* sText, const char* sPart);
extern const char* xl_str_slice(const char* sText, int iStart, int iCount);
extern const char* xl_str_repeat(const char* sText, int iCount);
extern xvalue xl_str_split(const char* sText, const char* sPart);
extern xvalue xl_str_split_lines(const char* sText);
extern int xl_str_starts_with(const char* sText, const char* sPart);
extern int xl_str_ends_with(const char* sText, const char* sPart);
extern const char* xl_str_concat(const char* sLeft, const char* sRight);
extern const char* xl_str_trim(const char* sText);
extern const char* xl_str_trim_left(const char* sText);
extern const char* xl_str_trim_right(const char* sText);
extern const char* xl_str_lower(const char* sText);
extern const char* xl_str_upper(const char* sText);
extern const char* xl_str_replace(const char* sText, const char* sPart, const char* sValue);
extern const char* xl_str_reverse(const char* sText);
extern const char* xl_str_pad_left(const char* sText, int iWidth, const char* sFill);
extern const char* xl_str_pad_right(const char* sText, int iWidth, const char* sFill);

extern int xl_rand_int32(void);
extern int xl_rand_bool(void);
extern int xl_rand_range(int iMin, int iMax);
extern const char* xl_rand_text(int iLen);
extern const char* xl_rand_text_from(const char* sTemplate, int iLen);

extern const char* xl_host_name(void);
extern const char* xl_host_ip(void);
extern const char* xl_host_user(void);
extern const char* xl_host_os(void);
extern const char* xl_host_arch(void);
extern const char* xl_host_executable(void);
extern const char* xl_env_get(const char* sName);
extern int xl_env_exists(const char* sName);
extern int xl_env_set(const char* sName, const char* sValue);
extern const char* xl_env_cwd(void);
extern const char* xl_env_temp(void);
extern const char* xl_env_home(void);

extern int xl_dir_create(const char* sPath);
extern const char* xl_dir_list(const char* sPath);
extern const char* xl_dir_files(const char* sPath);
extern const char* xl_dir_dirs(const char* sPath);
extern const char* xl_dir_tree(const char* sPath);
extern int xl_dir_copy(const char* sSrc, const char* sDst);
extern int xl_dir_move(const char* sSrc, const char* sDst);
extern int xl_dir_delete(const char* sPath);

extern const char* xl_fmt_int_text(int value, const char* sPattern);
extern const char* xl_fmt_float_text(double value, const char* sPattern);
extern const char* xl_fmt_bool_text(int value);

extern const char* xl_crypto_hmac_sha256(const char* sKey, const char* sText);
extern const char* xl_crypto_hmac_sha512(const char* sKey, const char* sText);

extern const char* xl_encode_hex_encode(const char* sText);
extern const char* xl_encode_hex_decode(const char* sText);
extern const char* xl_encode_base64_encode(const char* sText);
extern const char* xl_encode_base64_decode(const char* sText);

extern int xl_hash_hash32(const char* sText);
extern const char* xl_hash_hash64(const char* sText);
extern const char* xl_hash_hash64_micro(const char* sText);
extern const char* xl_hash_hash64_nano(const char* sText);
extern const char* xl_hash_sha256(const char* sText);
extern const char* xl_hash_sha512(const char* sText);

extern int xl_math_abs_int(int value);
extern double xl_math_abs_float(double value);
extern int xl_math_min_int(int left, int right);
extern double xl_math_min_float(double left, double right);
extern int xl_math_max_int(int left, int right);
extern double xl_math_max_float(double left, double right);
extern int xl_math_clamp_int(int value, int low, int high);
extern double xl_math_clamp_float(double value, double low, double high);
extern double xl_math_floor(double value);
extern double xl_math_ceil(double value);
extern double xl_math_round(double value);
extern double xl_math_trunc(double value);
extern int xl_math_sqrt_int(int value);
extern double xl_math_sqrt(double value);
extern int xl_math_pow_int(int value, int exp);
extern double xl_math_pow(double value, double exp);
extern double xl_math_sin(double value);
extern double xl_math_cos(double value);
extern double xl_math_tan(double value);
extern double xl_math_exp(double value);
extern double xl_math_log(double value);
extern double xl_math_log10(double value);

extern const char* xl_pack_write_u8(int value);
extern const char* xl_pack_write_u16_le(int value);
extern const char* xl_pack_write_u16_be(int value);
extern const char* xl_pack_write_u32_le(int value);
extern const char* xl_pack_write_u32_be(int value);
extern int xl_pack_read_u8(const char* sData, int iOffset);
extern int xl_pack_read_u16_le(const char* sData, int iOffset);
extern int xl_pack_read_u16_be(const char* sData, int iOffset);
extern int xl_pack_read_u32_le(const char* sData, int iOffset);
extern int xl_pack_read_u32_be(const char* sData, int iOffset);
extern const char* xl_query_encode(xvalue value);
extern xvalue xl_query_decode(const char* sText);
extern xvalue xl_url_parse(const char* sText);
extern const char* xl_url_build(xvalue value);
extern const char* xl_http_headers(xvalue value);
extern const char* xl_http_request(const char* sMethod, const char* sTarget, xvalue headers, const char* sBody);
extern const char* xl_http_response(int iStatus, const char* sReason, xvalue headers, const char* sBody);
extern xvalue xl_http_parse_headers(const char* sText);
extern const char* xl_mime_by_ext(const char* sExt);
extern const char* xl_mime_by_path(const char* sPath);
extern int xl_mime_is_text(const char* sMime);
extern xvalue xl_mime_parse(const char* sText);
extern const char* xl_mime_build(xvalue value);
extern const char* xl_multipart_boundary(const char* sContentType);
extern xvalue xl_multipart_parse(const char* sText, const char* sBoundary);
extern const char* xl_multipart_build(xvalue parts, const char* sBoundary);
extern xvalue xl_httpd_request(const char* sText);
extern xvalue xl_httpd_match(const char* sPattern, const char* sPath);
extern const char* xl_httpd_response(int iStatus, xvalue headers, const char* sBody);
extern const char* xl_httpd_text(int iStatus, const char* sBody);
extern xvalue xl_net_ip_parse(const char* sText);
extern const char* xl_net_ip_build(xvalue value);
extern const char* xl_net_ip_normalize(const char* sText);
extern int xl_net_ip_is_ip(const char* sText);
extern int xl_net_ip_is_ipv4(const char* sText);
extern int xl_net_ip_is_ipv6(const char* sText);
extern const char* xl_net_dns_resolve(const char* sHost);
extern xvalue xl_net_dns_resolve_all(const char* sHost);
extern const char* xl_net_dns_reverse(const char* sIp);
extern void* xl_net_tcp_listen(const char* sHost, int iPort);
extern void* xl_net_tcp_accept(void* pListener, int iTimeoutMs);
extern void* xl_net_tcp_connect(const char* sHost, int iPort);
extern int xl_net_tcp_send(void* pSocket, const char* sText);
extern const char* xl_net_tcp_recv(void* pSocket, int iSize, int iTimeoutMs);
extern int xl_net_tcp_local_port(void* pSocket);
extern int xl_net_tcp_close(void* pSocket);
extern void* xl_net_udp_bind(const char* sHost, int iPort);
extern int xl_net_udp_send_to(void* pSocket, const char* sHost, int iPort, const char* sText);
extern xvalue xl_net_udp_recv_from(void* pSocket, int iSize, int iTimeoutMs);
extern int xl_net_udp_local_port(void* pSocket);
extern int xl_net_udp_close(void* pSocket);
extern const char* xl_net_ws_key(void);
extern const char* xl_net_ws_accept(const char* sKey);
extern const char* xl_net_ws_request(const char* sHost, const char* sPath, const char* sKey);
extern const char* xl_net_ws_response(const char* sKey);
extern const char* xl_net_ws_frame_text(const char* sText, int bMask);
extern xvalue xl_net_ws_unframe(const char* sFrame);
extern xvalue xl_net_proxy_parse(const char* sUrl);
extern const char* xl_net_proxy_basic_auth(const char* sUser, const char* sPass);
extern const char* xl_net_proxy_http_connect(const char* sHost, int iPort, const char* sAuth);
extern const char* xl_net_proxy_socks5_greeting_hex(int bAuth);
extern const char* xl_net_proxy_socks5_connect_hex(const char* sHost, int iPort);
extern xvalue xl_net_tls_versions(void);
extern int xl_net_tls_is_tls_url(const char* sUrl);
extern int xl_net_tls_default_port(const char* sScheme);
extern const char* xl_net_tls_sni_host(const char* sUrl);
extern const char* xl_net_tls_fingerprint_sha256(const char* sData);
extern xvalue xl_net_tls_client_config(const char* sHost, int bVerify);
extern xvalue xl_range_collect1(int stop);
extern xvalue xl_range_collect2(int start, int stop);
extern xvalue xl_range_collect3(int start, int stop, int step);
extern const char* xl_template_render(const char* sTemplate, xvalue value);

extern int xl_process_exec(const char* sCommand);
extern const char* xl_process_capture(const char* sCommand);
extern int xl_process_exec_in(const char* sCommand, const char* sWorkDir);
extern const char* xl_process_capture_in(const char* sCommand, const char* sWorkDir);
extern int xl_process_terminal_supported(void);
extern const char* xl_net_resolve(const char* sHost);
extern void* xl_mem_alloc(int iSize);
extern int xl_mem_free(void* pMem);
extern int xl_mem_is_null(void* pMem);
extern int xl_mem_fill(void* pMem, int iByteValue, int iSize);
extern int xl_mem_copy(void* pDst, void* pSrc, int iSize);
extern int xl_mem_write_string(void* pMem, const char* sText);
extern const char* xl_mem_read_string(void* pMem, int iSize);

#endif
