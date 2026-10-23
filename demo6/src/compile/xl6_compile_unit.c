#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/cgen/xl6_ir_cgen.h"

#include "src/compile/xl6_imports.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"

#include <stdio.h>
#include <string.h>

#define R(Name) XL6_ARTIFACT_##Name
#define D(...) (const size_t[]){__VA_ARGS__}, sizeof((const size_t[]){__VA_ARGS__})/sizeof(size_t)
static const XL6_ArtifactRuntimeComponent xl6ArtifactComponents[] = {
    {"src/runtime/xl6_slot.c", NULL, 0, {"XRT_MODULE_MEMORY"}},
    {"src/runtime/xl6_code_lease.c", NULL, 0, {"XRT_MODULE_MEMORY", "XRT_MODULE_ATOMIC", "XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_closure.c", D(R(CODE_LEASE),R(CELL),R(FOREIGN_ERROR)),
        {"XRT_MODULE_FUTURE", "XRUNTIME_MODULE_RUNTIME_CALL", "XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE"}},
    {"src/runtime/xl6_cell.c", NULL, 0, {"XRT_MODULE_MEMORY", "XRT_MODULE_ATOMIC", "XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_frame_storage.c", NULL, 0, {"XRT_MODULE_MEMORY"}},
    {"src/runtime/xl6_task.c", D(R(TASK_POOL),R(TASK_FRAME)), {"XRT_MODULE_TASK_POOL"}},
    {"src/runtime/xl6_continuation_queue.c", D(R(CODE_LEASE)),
        {"XRT_MODULE_MUTEX", "XRT_MODULE_COND", "XRT_MODULE_ERROR"}},
    {"src/runtime/xl6_continuation_executor.c", D(R(QUEUE)), {"XRT_MODULE_THREAD", "XRT_MODULE_ERROR"}},
    {"src/runtime/xl6_continuation_future.c", D(R(QUEUE)), {"XRT_MODULE_FUTURE"}},
    {"src/runtime/xl6_continuation_frame.c", D(R(CAPTURE_V2),R(CODE_LEASE),R(QUEUE),R(EXECUTOR),R(FOREIGN_ERROR),R(FUTURE_WAIT)),
        {"XRT_MODULE_THREAD", "XRT_MODULE_FUTURE"}},
    {"src/runtime/xl6_future_wait.c", D(R(CONTINUATION_FRAME),R(FUTURE_CALLBACK)), {"XRT_MODULE_FUTURE_COMBINE", "XRT_MODULE_TASK_GROUP"}},
    {"src/runtime/xl6_capture.c", D(R(CELL),R(CLOSURE),R(FOREIGN_ERROR),R(GENERATOR),R(VALUE_ITERATOR)),
        {"XRT_MODULE_MEMORY", "XRT_MODULE_VALUE", "XRT_MODULE_FUTURE"}},
    {"src/runtime/xl6_value_iterator.c", D(R(ARRAY_VALUE_ITERATOR),R(FOREIGN_ERROR)), {"XRT_MODULE_MEMORY", "XRT_MODULE_ATOMIC", "XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_future_callback.c", D(R(CAPTURE_V2),R(CODE_LEASE),R(OWNERSHIP_DOMAIN_NATIVE_FUTURE)),
        {"XRT_MODULE_FUTURE", "XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_generator.c", D(R(CAPTURE_V2),R(CODE_LEASE),R(FOREIGN_ERROR)), {"XRT_MODULE_ATOMIC"}},
    {"src/runtime/xl6_future_payload.c", D(R(CLOSURE),R(GENERATOR),R(FOREIGN_ERROR)),
        {"XRT_MODULE_MEMORY", "XRT_MODULE_VALUE", "XRT_MODULE_FUTURE", "XRUNTIME_MODULE_RUNTIME_VALUE_FUTURE"}},
    {"src/runtime/xl6_code_finalizer.c", D(R(CODE_LEASE),R(FOREIGN_ERROR)), {"XRT_MODULE_VALUE_GRAPH"}},
    {"src/runtime/xl6_ownership_plan.c", NULL, 0, {"XRT_MODULE_MEMORY"}},
    {"src/api/xl_protocol_registry.c", NULL, 0, {"XRT_MODULE_ONCE", "XRT_MODULE_MUTEX", "XRT_MODULE_MEMORY"}},
    {"src/api/xl_future_bridge.c", D(R(CODE_LEASE),R(FUTURE_PAYLOAD),R(FUTURE_METADATA)),
        {"XRT_MODULE_FUTURE_COMBINE", "XRT_MODULE_THREAD", "XRT_MODULE_MUTEX"}},
    {"src/api/xl_channel_bridge.c", NULL, 0, {"XRT_MODULE_CHANNEL_SELECT_CANCEL", "XRT_MODULE_CHANNEL_COROUTINE"}},
    {"src/api/xl_task_group_bridge.c", NULL, 0, {"XRT_MODULE_TASK_GROUP"}},
    {"src/api/xl_foreign_error.c", NULL, 0, {"XRT_MODULE_ONCE", "XRT_MODULE_ERROR"}},
    {"src/runtime/xl6_network_service.c", NULL, 0, {"XRT_MODULE_NET_ENGINE", "XRT_MODULE_NET_RESOLVER"}},
    {"src/runtime/xl6_task_file.c", D(R(TASK)), {"XRT_MODULE_FILE_ASYNC"}},
    {"src/runtime/xl6_log.c", NULL, 0, {"XRT_MODULE_LOGGER_CORE", "XRT_MODULE_STRING", "XRT_MODULE_ONCE"}},
    {"src/runtime/xl6_log_console.c", D(R(LOG_CORE)), {"XRT_MODULE_LOGGER_CONSOLE"}},
    {"src/runtime/xl6_log_record.c", D(R(LOG_CORE)), {"XRT_MODULE_VALUE", "XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_log_text.c", D(R(LOG_RECORD)), {"XRT_MODULE_LOGGER_FORMAT_TEXT_BUFFER"}},
    {"src/runtime/xl6_log_json.c", D(R(LOG_RECORD)), {"XRT_MODULE_LOGGER_FORMAT_JSON_BUFFER"}},
    {"src/runtime/xl6_log_custom.c", D(R(LOG_RECORD),R(FOREIGN_ERROR)), {"XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE", "XRT_MODULE_ONCE"}},
    {"src/runtime/xl6_log_file.c", D(R(LOG_CORE)), {"XRT_MODULE_LOGGER_FILE"}},
    {"src/runtime/xl6_log_file_text.c", D(R(LOG_FILE),R(LOG_TEXT)), {"XRT_MODULE_LOGGER_FILE_TEXT"}},
    {"src/runtime/xl6_log_file_json.c", D(R(LOG_FILE),R(LOG_JSON)), {"XRT_MODULE_LOGGER_FILE_JSON"}},
    {"src/runtime/xl6_log_async.c", D(R(LOG_CORE)), {"XRT_MODULE_LOGGER_ASYNC"}},
    {"src/runtime/xl6_log_ring.c", D(R(LOG_CORE)), {"XRT_MODULE_LOGGER_RING"}},
    {"src/runtime/xl6_log_printf.c", D(R(LOG_CORE)), {"XRT_MODULE_LOGGER_PRINTF"}},
    {"src/runtime/xl6_log_text_writer.c", D(R(LOG_TEXT)), {"XRT_MODULE_IO"}},
    {"src/runtime/xl6_log_json_writer.c", D(R(LOG_JSON)), {"XRT_MODULE_IO"}},
    {"src/runtime/xl6_text_value.c", NULL, 0, {"XRT_MODULE_STRING", "XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_text_query.c", NULL, 0, {"XRT_MODULE_STRING", "XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_text_binary.c", NULL, 0, {"XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_text_chars.c", D(R(ARRAY_PRODUCER),R(CHAR_VALUE)), {"XRT_MODULE_VALUE_CONTAINER", "XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_text_glob.c", NULL, 0, {"XRT_MODULE_STRING_GLOB", "XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_text_distance.c", NULL, 0, {"XRT_MODULE_UNICODE_DISTANCE", "XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_text_split.c", D(R(ARRAY_PRODUCER),R(STRING_VALUE)), {"XRT_MODULE_STRING_SPLIT", "XRT_MODULE_UNICODE_TEXT", "XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_text_join.c", D(R(ARRAY_INPUT)), {"XRT_MODULE_STRING", "XRT_MODULE_UNICODE_TEXT", "XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_text_iterator.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_STRING_SPLIT", "XRT_MODULE_UNICODE_TEXT", "XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_number_format.c", NULL, 0, {"XRT_MODULE_NUMBER_FORMAT"}},
    {"src/runtime/xl6_number_format_buffer.c", D(R(NUMBER_FORMAT)), {"XRT_MODULE_BUFFER"}},
    {"src/runtime/xl6_number_integer.c", NULL, 0, {"XRT_MODULE_NUMBER_INTEGER"}},
    {"src/runtime/xl6_number_float.c", NULL, 0, {"XRT_MODULE_NUMBER_FLOAT"}},
    {"src/runtime/xl6_number_integer_buffer.c", NULL, 0, {"XRT_MODULE_NUMBER_INTEGER", "XRT_MODULE_BUFFER"}},
    {"src/runtime/xl6_number_float_buffer.c", NULL, 0, {"XRT_MODULE_NUMBER_FLOAT", "XRT_MODULE_BUFFER"}},
    {"src/runtime/xl6_unicode.c", NULL, 0, {"XRT_MODULE_UNICODE", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_unicode_validator.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_UNICODE", "XRT_MODULE_VALUE", "XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_html.c", NULL, 0, {"XRT_MODULE_HTML_ESCAPE"}},
    {"src/runtime/xl6_encoding.c", NULL, 0, {"XRT_MODULE_CHARSET", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_encoding_meta.c", NULL, 0, {"XRT_MODULE_CHARSET"}},
    {"src/runtime/xl6_encoding_detect.c", NULL, 0, {"XRT_MODULE_CHARSET_DETECT", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_regex_core.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_REGEX_CORE", "XRT_MODULE_STRING", "XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_regex_match.c", D(R(NATIVE_HANDLE),R(REGEX_CORE)), {"XRT_MODULE_REGEX_MATCH", "XRT_MODULE_STRING", "XRT_MODULE_UNICODE_TEXT", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_regex_iterator.c", D(R(NATIVE_HANDLE),R(REGEX_MATCH)), {"XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_regex_replace.c", D(R(REGEX_CORE)), {"XRT_MODULE_REGEX_REPLACE", "XRT_MODULE_STRING", "XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_regex_replace_buffer.c", D(R(REGEX_CORE)), {"XRT_MODULE_REGEX_REPLACE", "XRT_MODULE_BUFFER", "XRT_MODULE_STRING", "XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_regex_split.c", D(R(NATIVE_HANDLE),R(REGEX_CORE)), {"XRT_MODULE_REGEX_SPLIT", "XRT_MODULE_STRING", "XRT_MODULE_UNICODE_TEXT", "XRT_MODULE_VALUE", "XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_regex_set.c", D(R(NATIVE_HANDLE),R(REGEX_CORE),R(ARRAY_INPUT),R(ARRAY_PRODUCER)), {"XRT_MODULE_REGEX_SET", "XRT_MODULE_STRING", "XRT_MODULE_UNICODE_TEXT", "XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_regex_replace_func.c", D(R(NATIVE_HANDLE),R(REGEX_MATCH),R(CLOSURE)), {"XRT_MODULE_REGEX_REPLACE"}},
    {"src/runtime/xl6_regex_replace_func_buffer.c", D(R(REGEX_REPLACE_FUNC)), {"XRT_MODULE_BUFFER"}},
    {"src/runtime/xl6_pattern_core.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_PATTERN", "XRT_MODULE_STRING", "XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_pattern_options.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_PATTERN", "XRT_MODULE_STRING", "XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_pattern_match.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_PATTERN", "XRT_MODULE_STRING", "XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_pattern_builder.c", D(R(NATIVE_HANDLE),R(ARRAY_INPUT)), {"XRT_MODULE_PATTERN", "XRT_MODULE_STRING", "XRT_MODULE_UNICODE_TEXT", "XRT_MODULE_VALUE_CONTAINER", "XRT_MODULE_THREAD", "XRT_MODULE_ATOMIC"}},
    {"src/runtime/xl6_time_core.c", D(), {"XRT_MODULE_TIME"}},
    {"src/runtime/xl6_time_datetime.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_TIME"}},
    {"src/runtime/xl6_time_local.c", D(R(TIME_DATETIME)), {"XRT_MODULE_TIME_LOCAL"}},
    {"src/runtime/xl6_time_text.c", D(), {"XRT_MODULE_TIME_TEXT", "XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_time_datetime_text.c", D(R(TIME_DATETIME)), {"XRT_MODULE_TIME_TEXT", "XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_environment.c", D(), {"XRT_MODULE_ENVIRONMENT", "XRT_MODULE_UNICODE_TEXT", "XRT_MODULE_STRING"}},
    {"src/runtime/xl6_signal_core.c", D(), {"XRT_MODULE_SIGNAL", "XRT_MODULE_STRING"}},
    {"src/runtime/xl6_signal_queue.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_SIGNAL", "XRT_MODULE_STRING", "XRT_MODULE_TIME", "XRT_MODULE_WAIT"}},
    {"src/runtime/xl6_signal_task_group.c", D(R(TASK_GROUP),R(FOREIGN_ERROR),R(NATIVE_HANDLE)), {"XRT_MODULE_ERROR", "XRUNTIME_MODULE_RUNTIME_VALUE_FUTURE"}},
    {"src/runtime/xl6_process_command.c", D(R(NATIVE_HANDLE),R(ARRAY_INPUT)), {"XRT_MODULE_PROCESS", "XRT_MODULE_UNICODE_TEXT", "XRT_MODULE_STRING", "XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_process_options.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_PROCESS_RUN"}},
    {"src/runtime/xl6_process_run.c", D(R(NATIVE_HANDLE),R(PROCESS_COMMAND)), {"XRT_MODULE_PROCESS_RUN", "XRT_MODULE_VALUE", "XRT_MODULE_UNICODE_TEXT", "XRT_MODULE_STRING"}},
    {"src/runtime/xl6_process_status.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_PROCESS"}},
    {"src/runtime/xl6_process_open.c", D(), {"XRT_MODULE_PROCESS_OPEN", "XRT_MODULE_UNICODE_TEXT", "XRT_MODULE_STRING"}},
    {"src/runtime/xl6_process_terminal.c", D(), {"XRT_MODULE_PROCESS_TERMINAL"}},
    {"src/runtime/xl6_process_live.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_PROCESS", "XRT_MODULE_ATOMIC", "XRT_MODULE_TIME"}},
    {"src/runtime/xl6_process_stream.c", D(R(PROCESS_LIVE)), {"XRT_MODULE_IO"}},
    {"src/runtime/xl6_process_future.c", D(R(PROCESS_LIVE),R(FUTURE_PAYLOAD),R(FUTURE_CALLBACK)),
        {"XRT_MODULE_PROCESS_FUTURE"}},
    {"src/runtime/xl6_process_pipeline_config.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_PROCESS", "XRT_MODULE_STRING"}},
    {"src/runtime/xl6_process_pipeline.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_PROCESS_PIPELINE", "XRT_MODULE_VALUE", "XRT_MODULE_STRING", "XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_process_file.c", D(), {"XRT_MODULE_PROCESS_FILE", "XRT_MODULE_ATOMIC"}},
    {"src/runtime/xl6_process_terminal_config.c", D(), {"XRT_MODULE_PROCESS_TERMINAL"}},
    {"src/runtime/xl6_process_terminal_live.c", D(), {"XRT_MODULE_PROCESS_TERMINAL"}},
    {"src/runtime/xl6_native_handle.c", D(R(FOREIGN_ERROR)), {"XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_buffer_value.c", D(), {"XRT_MODULE_BUFFER", "XRT_MODULE_VALUE", "XRT_MODULE_UNICODE_TEXT", "XRT_MODULE_STRING"}},
    {"src/runtime/xl6_buffer_iterator.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_BUFFER", "XRT_MODULE_MEMORY", "XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_buffer_hex.c", D(R(CODEC_HEX)), {"XRT_MODULE_BUFFER_HEX"}},
    {"src/runtime/xl6_buffer_base64.c", D(R(CODEC_BASE64)), {"XRT_MODULE_BUFFER_BASE64"}},
    {"src/runtime/xl6_buffer_core.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_BUFFER"}},
    {"src/runtime/xl6_buffer_query.c", D(), {"XRT_MODULE_BUFFER", "XRT_MODULE_STRING"}},
    {"src/runtime/xl6_crc32_text.c", D(), {"XRT_MODULE_CRC32"}},
    {"src/runtime/xl6_atomic_i32.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_ATOMIC", "XRT_MODULE_MEMORY"}},
    {"src/runtime/xl6_atomic_i32_domain.c", D(R(OWNERSHIP_CATALOG),R(ATOMIC_I32)), {NULL}},
    {"src/runtime/xl6_atomic_u32.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_ATOMIC", "XRT_MODULE_MEMORY"}},
    {"src/runtime/xl6_atomic_u32_domain.c", D(R(OWNERSHIP_CATALOG),R(ATOMIC_U32)), {NULL}},
    {"src/runtime/xl6_atomic_i64.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_ATOMIC", "XRT_MODULE_MEMORY"}},
    {"src/runtime/xl6_atomic_i64_domain.c", D(R(OWNERSHIP_CATALOG),R(ATOMIC_I64)), {NULL}},
    {"src/runtime/xl6_atomic_u64.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_ATOMIC", "XRT_MODULE_MEMORY"}},
    {"src/runtime/xl6_atomic_u64_domain.c", D(R(OWNERSHIP_CATALOG),R(ATOMIC_U64)), {NULL}},
    {"src/runtime/xl6_atomic_bool.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_ATOMIC", "XRT_MODULE_MEMORY"}},
    {"src/runtime/xl6_atomic_bool_domain.c", D(R(OWNERSHIP_CATALOG),R(ATOMIC_BOOL)), {NULL}},
    {"src/runtime/xl6_once.c", D(R(NATIVE_HANDLE),R(CLOSURE),R(FOREIGN_ERROR)), {"XRT_MODULE_ONCE", "XRT_MODULE_MEMORY"}},
    {"src/runtime/xl6_once_domain.c", D(R(OWNERSHIP_CATALOG),R(ONCE)), {NULL}},
    {"src/runtime/xl6_event.c", D(R(NATIVE_HANDLE),R(SYNC_CORE)), {"XRT_MODULE_EVENT", "XRUNTIME_MODULE_RUNTIME_VALUE_TYPE"}},
    {"src/runtime/xl6_event_domain.c", D(R(OWNERSHIP_CATALOG),R(EVENT),R(OWNERSHIP_DOMAIN_SYNC)), {NULL}},
    {"src/runtime/xl6_thread_exit.c", D(), {"XRT_MODULE_ATOMIC"}},
    {"src/runtime/xl6_thread_local.c", D(R(NATIVE_HANDLE),R(THREAD_EXIT),R(FOREIGN_ERROR)), {"XRT_MODULE_THREAD_KEY", "XRT_MODULE_INT_MAP", "XRT_MODULE_THREAD", "XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_thread_local_domain.c", D(R(OWNERSHIP_CATALOG),R(THREAD_LOCAL)), {NULL}},
    {"src/runtime/xl6_cancellation_source.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_CANCEL", "XRT_MODULE_VALUE_GRAPH"}},
    {"src/runtime/xl6_cancellation_source_domain.c", D(R(OWNERSHIP_CATALOG),R(CANCELLATION_SOURCE)), {NULL}},
    {"src/runtime/xl6_cancellation_registration.c", D(R(NATIVE_HANDLE),R(CLOSURE),R(FOREIGN_ERROR)), {"XRT_MODULE_CANCEL", "XRT_MODULE_THREAD", "XRT_MODULE_VALUE_GRAPH"}},
    {"src/runtime/xl6_cancellation_registration_domain.c", D(R(OWNERSHIP_CATALOG),R(CANCELLATION_REGISTRATION),R(OWNERSHIP_DOMAIN_CANCELLATION_SOURCE)), {NULL}},
    {"src/runtime/xl6_crypto_md5.c", D(), {"XRT_MODULE_CRYPTO_MD5", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_md5_stream.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_CRYPTO_MD5", "XRT_MODULE_VALUE", "XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_crypto_md5_domain.c", D(R(OWNERSHIP_CATALOG),R(CRYPTO_MD5_STREAM)), {NULL}},
    {"src/runtime/xl6_crypto_sha1.c", D(), {"XRT_MODULE_CRYPTO_SHA1", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_sha1_stream.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_CRYPTO_SHA1", "XRT_MODULE_VALUE", "XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_crypto_sha1_domain.c", D(R(OWNERSHIP_CATALOG),R(CRYPTO_SHA1_STREAM)), {NULL}},
    {"src/runtime/xl6_crypto_sha224.c", D(), {"XRT_MODULE_CRYPTO_SHA224", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_sha224_stream.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_CRYPTO_SHA224", "XRT_MODULE_VALUE", "XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_crypto_sha224_domain.c", D(R(OWNERSHIP_CATALOG),R(CRYPTO_SHA224_STREAM)), {NULL}},
    {"src/runtime/xl6_crypto_sha256.c", D(), {"XRT_MODULE_CRYPTO_SHA256", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_sha256_stream.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_CRYPTO_SHA256", "XRT_MODULE_VALUE", "XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_crypto_sha256_domain.c", D(R(OWNERSHIP_CATALOG),R(CRYPTO_SHA256_STREAM)), {NULL}},
    {"src/runtime/xl6_crypto_sha384.c", D(), {"XRT_MODULE_CRYPTO_SHA512", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_sha384_stream.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_CRYPTO_SHA512", "XRT_MODULE_VALUE", "XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_crypto_sha384_domain.c", D(R(OWNERSHIP_CATALOG),R(CRYPTO_SHA384_STREAM)), {NULL}},
    {"src/runtime/xl6_crypto_sha512.c", D(), {"XRT_MODULE_CRYPTO_SHA512", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_sha512_stream.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_CRYPTO_SHA512", "XRT_MODULE_VALUE", "XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_crypto_sha512_domain.c", D(R(OWNERSHIP_CATALOG),R(CRYPTO_SHA512_STREAM)), {NULL}},
    {"src/runtime/xl6_crypto_sha512_256.c", D(), {"XRT_MODULE_CRYPTO_SHA512_256", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_sha512_256_stream.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_CRYPTO_SHA512_256", "XRT_MODULE_VALUE", "XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_crypto_sha512_256_domain.c", D(R(OWNERSHIP_CATALOG),R(CRYPTO_SHA512_256_STREAM)), {NULL}},
    {"src/runtime/xl6_crypto_hmac_sha256.c", D(), {"XRT_MODULE_CRYPTO_HMAC_SHA256", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_hmac_sha256_stream.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_CRYPTO_HMAC_SHA256", "XRT_MODULE_VALUE", "XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_crypto_hmac_sha256_domain.c", D(R(OWNERSHIP_CATALOG),R(CRYPTO_HMAC_SHA256_STREAM)), {NULL}},
    {"src/runtime/xl6_crypto_hmac_sha384.c", D(), {"XRT_MODULE_CRYPTO_HMAC_SHA512", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_hmac_sha384_stream.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_CRYPTO_HMAC_SHA512", "XRT_MODULE_VALUE", "XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_crypto_hmac_sha384_domain.c", D(R(OWNERSHIP_CATALOG),R(CRYPTO_HMAC_SHA384_STREAM)), {NULL}},
    {"src/runtime/xl6_crypto_hmac_sha512.c", D(), {"XRT_MODULE_CRYPTO_HMAC_SHA512", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_hmac_sha512_stream.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_CRYPTO_HMAC_SHA512", "XRT_MODULE_VALUE", "XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_crypto_hmac_sha512_domain.c", D(R(OWNERSHIP_CATALOG),R(CRYPTO_HMAC_SHA512_STREAM)), {NULL}},
    {"src/runtime/xl6_crypto_pbkdf2_sha256.c", D(), {"XRT_MODULE_CRYPTO_PBKDF2_SHA256", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_hkdf_sha256.c", D(), {"XRT_MODULE_CRYPTO_HKDF_SHA256", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_pbkdf2_sha384.c", D(), {"XRT_MODULE_CRYPTO_PBKDF2_SHA512", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_hkdf_sha384.c", D(), {"XRT_MODULE_CRYPTO_HKDF_SHA512", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_pbkdf2_sha512.c", D(), {"XRT_MODULE_CRYPTO_PBKDF2_SHA512", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_hkdf_sha512.c", D(), {"XRT_MODULE_CRYPTO_HKDF_SHA512", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_equal.c", D(), {"XRT_MODULE_CRYPTO_CORE", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_aes_gcm.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_CRYPTO_AES_GCM", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_aes_gcm_domain.c", D(R(OWNERSHIP_CATALOG),R(CRYPTO_AES_GCM)), {NULL}},
    {"src/runtime/xl6_crypto_chacha_aead.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_CRYPTO_CHACHA20_POLY1305", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_chacha_aead_domain.c", D(R(OWNERSHIP_CATALOG),R(CRYPTO_CHACHA_AEAD)), {NULL}},
    {"src/runtime/xl6_crypto_x25519.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_CRYPTO_X25519", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_x25519_domain.c", D(R(OWNERSHIP_CATALOG),R(CRYPTO_X25519)), {NULL}},
    {"src/runtime/xl6_crypto_x25519_generate.c", D(R(CRYPTO_X25519)), {"XRT_MODULE_CRYPTO_X25519_KEYPAIR", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_x25519_raw.c", D(), {"XRT_MODULE_CRYPTO_X25519", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_x448.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_CRYPTO_X448", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_x448_domain.c", D(R(OWNERSHIP_CATALOG),R(CRYPTO_X448)), {NULL}},
    {"src/runtime/xl6_crypto_x448_generate.c", D(R(CRYPTO_X448)), {"XRT_MODULE_CRYPTO_X448_KEYPAIR", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_x448_raw.c", D(), {"XRT_MODULE_CRYPTO_X448", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_ed25519.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_CRYPTO_ED25519", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_ed25519_domain.c", D(R(OWNERSHIP_CATALOG),R(CRYPTO_ED25519)), {NULL}},
    {"src/runtime/xl6_crypto_ed25519_generate.c", D(R(CRYPTO_ED25519)), {"XRT_MODULE_RANDOM_SECURE"}},
    {"src/runtime/xl6_crypto_ed25519_sign.c", D(R(CRYPTO_ED25519)), {"XRT_MODULE_CRYPTO_ED25519_SIGN", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_ed25519_sign_buffer.c", D(R(CRYPTO_ED25519_SIGN),R(BUFFER_CORE)), {"XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_ed25519_seed_sign.c", D(), {"XRT_MODULE_CRYPTO_ED25519_SIGN", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_ed25519_seed_sign_buffer.c", D(R(CRYPTO_ED25519_SEED_SIGN),R(BUFFER_CORE)), {"XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_ed25519_public.c", D(), {"XRT_MODULE_CRYPTO_ED25519", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_ed25519_verify.c", D(), {"XRT_MODULE_CRYPTO_ED25519_VERIFY", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_ed25519_verify_buffer.c", D(R(CRYPTO_ED25519_VERIFY),R(BUFFER_CORE)), {"XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_p256.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_CRYPTO_P256", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_p256_domain.c", D(R(OWNERSHIP_CATALOG),R(CRYPTO_P256)), {NULL}},
    {"src/runtime/xl6_crypto_p256_generate.c", D(R(CRYPTO_P256)), {"XRT_MODULE_CRYPTO_P256_KEYPAIR"}},
    {"src/runtime/xl6_crypto_p256_shared.c", D(R(CRYPTO_P256)), {"XRT_MODULE_CRYPTO_P256", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_p256_sign.c", D(R(CRYPTO_P256)), {"XRT_MODULE_CRYPTO_ECDSA_P256_SIGN", "XRT_MODULE_CRYPTO_HMAC_SHA256", "XRT_MODULE_CRYPTO_HMAC_SHA512", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_p256_sign_der.c", D(R(CRYPTO_P256)), {"XRT_MODULE_CRYPTO_ECDSA_P256_SIGN_DER", "XRT_MODULE_CRYPTO_HMAC_SHA256", "XRT_MODULE_CRYPTO_HMAC_SHA512", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_p256_public.c", D(), {"XRT_MODULE_CRYPTO_P256", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_p256_verify.c", D(), {"XRT_MODULE_CRYPTO_ECDSA_P256", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_p256_verify_der.c", D(), {"XRT_MODULE_CRYPTO_ECDSA_P256_DER", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_p256_raw.c", D(), {"XRT_MODULE_CRYPTO_P256", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_p384.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_CRYPTO_P384", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_p384_domain.c", D(R(OWNERSHIP_CATALOG),R(CRYPTO_P384)), {NULL}},
    {"src/runtime/xl6_crypto_p384_generate.c", D(R(CRYPTO_P384)), {"XRT_MODULE_CRYPTO_P384_KEYPAIR"}},
    {"src/runtime/xl6_crypto_p384_shared.c", D(R(CRYPTO_P384)), {"XRT_MODULE_CRYPTO_P384", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_p384_sign.c", D(R(CRYPTO_P384)), {"XRT_MODULE_CRYPTO_ECDSA_P384_SIGN", "XRT_MODULE_CRYPTO_HMAC_SHA256", "XRT_MODULE_CRYPTO_HMAC_SHA512", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_p384_sign_der.c", D(R(CRYPTO_P384)), {"XRT_MODULE_CRYPTO_ECDSA_P384_SIGN_DER", "XRT_MODULE_CRYPTO_HMAC_SHA256", "XRT_MODULE_CRYPTO_HMAC_SHA512", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_p384_public.c", D(), {"XRT_MODULE_CRYPTO_P384", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_p384_verify.c", D(), {"XRT_MODULE_CRYPTO_ECDSA_P384", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_p384_verify_der.c", D(), {"XRT_MODULE_CRYPTO_ECDSA_P384_DER", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_p384_raw.c", D(), {"XRT_MODULE_CRYPTO_P384", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_nist_der.c", D(), {"XRT_MODULE_CRYPTO_ECDSA_DER", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_rsa_public.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_CRYPTO_RSA", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_rsa_public_domain.c", D(R(OWNERSHIP_CATALOG),R(CRYPTO_RSA_PUBLIC)), {NULL}},
    {"src/runtime/xl6_crypto_rsa_private.c", D(R(NATIVE_HANDLE),R(CRYPTO_RSA_PUBLIC)), {"XRT_MODULE_CRYPTO_RSA_PRIVATE", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_rsa_private_domain.c", D(R(OWNERSHIP_CATALOG),R(CRYPTO_RSA_PRIVATE)), {NULL}},
    {"src/runtime/xl6_crypto_rsa_public_raw.c", D(R(CRYPTO_RSA_PUBLIC)), {"XRT_MODULE_CRYPTO_RSA", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_rsa_private_raw.c", D(R(CRYPTO_RSA_PRIVATE)), {"XRT_MODULE_CRYPTO_RSA_PRIVATE", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_rsa_pss_verify.c", D(R(CRYPTO_RSA_PUBLIC)), {"XRT_MODULE_CRYPTO_RSA_PSS", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_rsa_pkcs1_verify.c", D(R(CRYPTO_RSA_PUBLIC)), {"XRT_MODULE_CRYPTO_RSA_PKCS1", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_rsa_pss_sign.c", D(R(CRYPTO_RSA_PRIVATE)), {"XRT_MODULE_CRYPTO_RSA_PSS_SIGN", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_rsa_pss_sign_salt.c", D(R(CRYPTO_RSA_PRIVATE)), {"XRT_MODULE_CRYPTO_RSA_PSS_SIGN", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crypto_rsa_pkcs1_sign.c", D(R(CRYPTO_RSA_PRIVATE)), {"XRT_MODULE_CRYPTO_RSA_PKCS1_SIGN", "XRT_MODULE_VALUE"}},
#include "xl6_key_components.inc"
#include "xl6_x509_components.inc"
#include "xl6_x509_records_components.inc"
#include "xl6_x509_profiles_components.inc"
#include "xl6_x509_crl_profiles_components.inc"
#include "xl6_x509_verify_components.inc"
#include "xl6_x509_crl_policy_components.inc"
    {"src/runtime/xl6_der_value.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_ASN1_DER", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_der_value_domain.c", D(R(OWNERSHIP_CATALOG),R(DER_VALUE)), {NULL}},
    {"src/runtime/xl6_der_reader.c", D(R(NATIVE_HANDLE),R(DER_VALUE)), {"XRT_MODULE_ASN1_DER", "XRT_MODULE_VALUE", "XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_der_reader_domain.c", D(R(OWNERSHIP_CATALOG),R(DER_READER)), {NULL}},
    {"src/runtime/xl6_der_builder.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_ASN1_DER", "XRT_MODULE_VALUE", "XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_der_builder_domain.c", D(R(OWNERSHIP_CATALOG),R(DER_BUILDER)), {NULL}},
    {"src/runtime/xl6_pem_block.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_PEM", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_pem_block_domain.c", D(R(OWNERSHIP_CATALOG),R(PEM_BLOCK)), {NULL}},
    {"src/runtime/xl6_pem_reader.c", D(R(NATIVE_HANDLE),R(PEM_BLOCK)), {"XRT_MODULE_PEM", "XRT_MODULE_VALUE", "XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_pem_reader_domain.c", D(R(OWNERSHIP_CATALOG),R(PEM_READER)), {NULL}},
    {"src/runtime/xl6_codec_hex.c", D(), {"XRT_MODULE_CODEC_HEX"}},
    {"src/runtime/xl6_codec_hex_value.c", D(R(CODEC_HEX)), {"XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_codec_base64.c", D(), {"XRT_MODULE_CODEC_BASE64"}},
    {"src/runtime/xl6_codec_base64_value.c", D(R(CODEC_BASE64)), {"XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_codec_percent.c", D(), {"XRT_MODULE_CODEC_PERCENT"}},
    {"src/runtime/xl6_codec_percent_value.c", D(R(CODEC_PERCENT)), {"XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_compress_deflate_all.c", D(), {"XRT_MODULE_DEFLATE", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_compress_inflate_all.c", D(), {"XRT_MODULE_INFLATE", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_compress_deflater.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_DEFLATE", "XRT_MODULE_BUFFER", "XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_compress_inflater.c", D(R(NATIVE_HANDLE)), {"XRT_MODULE_INFLATE", "XRT_MODULE_BUFFER", "XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_template_core.c",D(R(NATIVE_HANDLE)),{"XRT_MODULE_TEMPLATE_COMPOSE", "XRT_MODULE_THREAD", "XRT_MODULE_VALUE_CONTAINER", "XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_template_data.c",D(R(ARRAY_GRAPH),R(DICT_GRAPH),R(LIST_GRAPH),R(SET_GRAPH)),{"XRT_MODULE_VALUE_GRAPH"}},
    {"src/runtime/xl6_template_render.c",D(R(TEMPLATE_CORE),R(TEMPLATE_DATA),R(CLOSURE)),{"XRT_MODULE_ONCE", "XRT_MODULE_THREAD", "XRT_MODULE_ATOMIC"}},
    {"src/runtime/xl6_template_extension.c",D(R(TEMPLATE_CORE),R(TEMPLATE_RENDER),R(TEMPLATE_DATA)),{"XRT_MODULE_TEMPLATE_EXTENSION"}},
    {"src/runtime/xl6_template_buffer.c",D(R(TEMPLATE_RENDER)),{"XRT_MODULE_BUFFER"}},
    {"src/runtime/xl6_template_writer.c",D(R(TEMPLATE_RENDER)),{"XRT_MODULE_IO"}},
    {"src/runtime/xl6_template_file.c",D(R(TEMPLATE_CORE)),{"XRT_MODULE_FILE_TEXT"}},
    {"src/runtime/xl6_template_vfs.c",D(R(TEMPLATE_CORE)),{"XRT_MODULE_VFS", "XRT_MODULE_CHARSET", "XRT_MODULE_CHARSET_DETECT"}},
    {"src/runtime/xl6_crc32_bytes.c", D(), {"XRT_MODULE_CRC32", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_crc32_buffer.c", D(), {"XRT_MODULE_CRC32", "XRT_MODULE_BUFFER"}},
    {"src/runtime/xl6_sequence.c", D(), {"XRUNTIME_MODULE_TYPED_ARRAY", "XRUNTIME_MODULE_RUNTIME_VALUE_TYPE",
        "XRUNTIME_MODULE_RUNTIME_TYPE_STRING_VALUE", "XRT_MODULE_ATOMIC"}},
    {"src/runtime/xl6_array.c", D(R(SEQUENCE)), {"XRUNTIME_MODULE_TYPED_ARRAY"}},
    {"src/runtime/xl6_stack.c", D(R(SEQUENCE)), {"XRUNTIME_MODULE_TYPED_STACK"}},
    {"src/runtime/xl6_nullable_bool.c", D(), {"XRUNTIME_MODULE_RUNTIME_TYPE"}},
    {"src/runtime/xl6_nullable_i64.c", D(), {"XRUNTIME_MODULE_RUNTIME_TYPE"}},
    {"src/runtime/xl6_nullable_u64.c", D(), {"XRUNTIME_MODULE_RUNTIME_TYPE"}},
    {"src/runtime/xl6_nullable_f32.c", D(), {"XRUNTIME_MODULE_RUNTIME_TYPE"}},
    {"src/runtime/xl6_nullable_f64.c", D(), {"XRUNTIME_MODULE_RUNTIME_TYPE"}},
    {"src/runtime/xl6_nullable_time.c", D(), {"XRUNTIME_MODULE_RUNTIME_TYPE"}},
    {"src/runtime/xl6_nullable_char.c", D(), {"XRUNTIME_MODULE_RUNTIME_TYPE"}},
    {"src/runtime/xl6_nullable_string.c", D(), {"XRUNTIME_MODULE_RUNTIME_TYPE_STRING_VALUE"}},
    {"src/runtime/xl6_array_iterator.c", D(R(SEQUENCE)), {"XRUNTIME_MODULE_TYPED_ARRAY"}},
    {"src/runtime/xl6_array_value_iterator.c", D(R(ARRAY_ITERATOR)), {"XRUNTIME_MODULE_TYPED_ARRAY_VALUE"}},
    {"src/runtime/xl6_nullable_bool_value.c", D(R(NULLABLE_BOOL)), {"XRUNTIME_MODULE_RUNTIME_VALUE_TYPE"}},
    {"src/runtime/xl6_nullable_i64_value.c", D(R(NULLABLE_I64)), {"XRUNTIME_MODULE_RUNTIME_VALUE_TYPE"}},
    {"src/runtime/xl6_nullable_u64_value.c", D(R(NULLABLE_U64)), {"XRUNTIME_MODULE_RUNTIME_VALUE_TYPE"}},
    {"src/runtime/xl6_nullable_f32_value.c", D(R(NULLABLE_F32)), {"XRUNTIME_MODULE_RUNTIME_VALUE_TYPE"}},
    {"src/runtime/xl6_nullable_f64_value.c", D(R(NULLABLE_F64)), {"XRUNTIME_MODULE_RUNTIME_VALUE_TYPE"}},
    {"src/runtime/xl6_nullable_time_value.c", D(R(NULLABLE_TIME)), {"XRUNTIME_MODULE_RUNTIME_VALUE_TYPE"}},
    {"src/runtime/xl6_nullable_char_value.c", D(R(NULLABLE_CHAR)), {"XRUNTIME_MODULE_RUNTIME_VALUE_TYPE"}},
    {"src/runtime/xl6_string_type.c", D(R(FOREIGN_ERROR)), {"XRUNTIME_MODULE_RUNTIME_TYPE_STRING"}},
    {"src/runtime/xl6_string_value.c", D(R(STRING_TYPE),R(FOREIGN_ERROR)), {"XRUNTIME_MODULE_RUNTIME_TYPE_STRING_VALUE"}},
    {"src/runtime/xl6_array_value.c", D(R(SEQUENCE),R(ARRAY_REFERENCE_VALUE)), {"XRUNTIME_MODULE_TYPED_ARRAY_VALUE"}},
    {"src/runtime/xl6_char_type.c", D(), {"XRUNTIME_MODULE_RUNTIME_TYPE"}},
    {"src/runtime/xl6_char_value.c", D(R(CHAR_TYPE)), {"XRUNTIME_MODULE_TYPED_VALUE"}},
    {"src/runtime/xl6_array_reference_value.c", D(R(SEQUENCE)), {"XRUNTIME_MODULE_RUNTIME_VALUE_TYPE"}},
    {"src/runtime/xl6_array_erased.c", D(R(SEQUENCE),R(ARRAY_REFERENCE_VALUE),R(FOREIGN_ERROR)), {"XRUNTIME_MODULE_TYPED_ARRAY_VALUE"}},
    {"src/runtime/xl6_value_admission.c", D(), {"XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_nominal_admission.c", D(), {"XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_array_reference_admission.c", D(R(SEQUENCE),R(VALUE_ADMISSION)), {NULL}},
    {"src/runtime/xl6_array_nominal_admission.c", D(R(SEQUENCE),R(NOMINAL_ADMISSION)), {NULL}},
    {"src/runtime/xl6_array_default.c", D(R(SEQUENCE)),
        {"XRT_MODULE_VALUE_COLLECTION", "XRUNTIME_MODULE_TYPED_ARRAY"}},
    {"src/runtime/xl6_array_primitive_admission.c", D(R(SEQUENCE),R(VALUE_ADMISSION)), {NULL}},
    {"src/runtime/xl6_array_c_scalar_admission.c", D(R(SEQUENCE)), {"XRUNTIME_MODULE_RUNTIME_TYPE"}},
    {"src/runtime/xl6_stack_batch.c", D(R(SEQUENCE),R(FOREIGN_ERROR)), {"XRUNTIME_MODULE_TYPED_STACK"}},
    {"src/runtime/xl6_array_sequence_admission.c", D(R(SEQUENCE)), {NULL}},
    {"src/runtime/xl6_array_nullable_c_scalar_admission.c", D(R(SEQUENCE)), {"XRUNTIME_MODULE_RUNTIME_TYPE"}},
    {"src/runtime/xl6_nullable_native_i8.c", D(), {"XRUNTIME_MODULE_RUNTIME_TYPE"}},
    {"src/runtime/xl6_nullable_native_i8_value.c", D(R(NULLABLE_NATIVE_I8)), {"XRUNTIME_MODULE_RUNTIME_VALUE_TYPE"}},
    {"src/runtime/xl6_nullable_native_u8.c", D(), {"XRUNTIME_MODULE_RUNTIME_TYPE"}},
    {"src/runtime/xl6_nullable_native_u8_value.c", D(R(NULLABLE_NATIVE_U8)), {"XRUNTIME_MODULE_RUNTIME_VALUE_TYPE"}},
    {"src/runtime/xl6_nullable_native_i16.c", D(), {"XRUNTIME_MODULE_RUNTIME_TYPE"}},
    {"src/runtime/xl6_nullable_native_i16_value.c", D(R(NULLABLE_NATIVE_I16)), {"XRUNTIME_MODULE_RUNTIME_VALUE_TYPE"}},
    {"src/runtime/xl6_nullable_native_u16.c", D(), {"XRUNTIME_MODULE_RUNTIME_TYPE"}},
    {"src/runtime/xl6_nullable_native_u16_value.c", D(R(NULLABLE_NATIVE_U16)), {"XRUNTIME_MODULE_RUNTIME_VALUE_TYPE"}},
    {"src/runtime/xl6_nullable_native_i32.c", D(), {"XRUNTIME_MODULE_RUNTIME_TYPE"}},
    {"src/runtime/xl6_nullable_native_i32_value.c", D(R(NULLABLE_NATIVE_I32)), {"XRUNTIME_MODULE_RUNTIME_VALUE_TYPE"}},
    {"src/runtime/xl6_nullable_native_u32.c", D(), {"XRUNTIME_MODULE_RUNTIME_TYPE"}},
    {"src/runtime/xl6_nullable_native_u32_value.c", D(R(NULLABLE_NATIVE_U32)), {"XRUNTIME_MODULE_RUNTIME_VALUE_TYPE"}},
    {"src/runtime/xl6_array_numeric_enum_admission.c", D(R(SEQUENCE)), {"XRUNTIME_MODULE_RUNTIME_TYPE"}},
    {"src/runtime/xl6_array_callable_admission.c", D(R(SEQUENCE)),
        {"XRUNTIME_MODULE_RUNTIME_CALL", "XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE"}},
    {"src/runtime/xl6_native_scalar_value.c", D(), {"XRUNTIME_MODULE_TYPED_VALUE"}},
    {"src/runtime/xl6_array_input.c", D(R(SEQUENCE),R(FOREIGN_ERROR)), {"XRUNTIME_MODULE_TYPED_ARRAY_VALUE", "XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_future_aggregate.c", D(R(CODE_LEASE),R(FUTURE_PAYLOAD)), {"XRT_MODULE_FUTURE_COMBINE", "XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_future_aggregate_array.c", D(R(FUTURE_AGGREGATE),R(ARRAY_INPUT)), {"XRUNTIME_MODULE_RUNTIME_VALUE_FUTURE"}},
    {"src/runtime/xl6_future_metadata.c", D(), {"XRT_MODULE_FUTURE", "XRT_MODULE_UNICODE"}},
    {"src/runtime/xl6_array_future_admission.c", D(R(SEQUENCE)),
        {"XRUNTIME_MODULE_RUNTIME_TYPE_FUTURE", "XRUNTIME_MODULE_RUNTIME_VALUE_FUTURE"}},
    {"src/runtime/xl6_array_rng_shuffle.c", D(R(SEQUENCE)),
        {"XRT_MODULE_RANDOM", "XRT_MODULE_VALUE_COLLECTION"}},
    {"src/runtime/xl6_array_rand_shuffle.c", D(R(SEQUENCE)),
        {"XRT_MODULE_RANDOM_DEFAULT", "XRT_MODULE_VALUE_COLLECTION"}},
    {"src/runtime/xl6_collection_count.c", D(R(SEQUENCE)), {"XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_array_scalar_read.c", D(R(SEQUENCE)),
        {"XRT_MODULE_VALUE_CONTAINER", "XRUNTIME_MODULE_RUNTIME_TYPE"}},
    {"src/runtime/xl6_array_string_read.c", D(R(SEQUENCE),R(STRING_TYPE)),
        {"XRT_MODULE_VALUE_CONTAINER", "XRUNTIME_MODULE_RUNTIME_TYPE_STRING"}},
    {"src/runtime/xl6_array_value_read.c", D(R(SEQUENCE)),
        {"XRT_MODULE_VALUE_CONTAINER", "XRUNTIME_MODULE_TYPED_ARRAY_VALUE"}},
    {"src/runtime/xl6_array_import.c", D(R(SEQUENCE),R(ARRAY_INPUT),R(FOREIGN_ERROR)),
        {"XRT_MODULE_VALUE_CONTAINER", "XRUNTIME_MODULE_TYPED_ARRAY_VALUE"}},
    {"src/runtime/xl6_array_import_chain.c", D(R(SEQUENCE),R(ARRAY_INPUT),R(ARRAY_SEQUENCE_ADMISSION),R(ARRAY_REFERENCE_ADMISSION),R(FOREIGN_ERROR)),
        {"XRT_MODULE_VALUE_CONTAINER", "XRUNTIME_MODULE_TYPED_ARRAY_VALUE"}},
    {"src/runtime/xl6_list.c", D(R(SEQUENCE),R(ARRAY_REFERENCE_VALUE),R(ARRAY_FUTURE_ADMISSION),R(FOREIGN_ERROR)),
        {"XRT_MODULE_VALUE_CONTAINER", "XRUNTIME_MODULE_TYPED_LIST_VALUE"}},
    {"src/runtime/xl6_restore_class_closed.c", D(R(CODE_FINALIZER),R(CODE_LEASE),R(SEQUENCE)),
        {"XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_restore_class.c", D(R(RESTORE_CLASS),R(LIST_NATIVE)), {}},
    {"src/runtime/xl6_array_algebra.c", D(R(SEQUENCE),R(FOREIGN_ERROR),R(ARRAY_INPUT)),
        {"XRT_MODULE_VALUE_COLLECTION", "XRUNTIME_MODULE_TYPED_ARRAY"}},
    {"src/runtime/xl6_array_graph.c", D(R(SEQUENCE),R(FOREIGN_ERROR)),
        {"XRT_MODULE_VALUE_GRAPH", "XRUNTIME_MODULE_TYPED_ARRAY_VALUE"}},
    {"src/runtime/xl6_persistence_project.c", D(R(ARRAY_GRAPH),R(FOREIGN_ERROR)),
        {"XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_persistence_project_open.c", D(R(FOREIGN_ERROR),R(DYNAMIC_FIELDS)),
        {"XRT_MODULE_VALUE_GRAPH", "XRUNTIME_MODULE_RUNTIME_DYNAMIC_FIELD"}},
    {"src/runtime/xl6_array_methods.c", D(R(SEQUENCE)),
        {"XRT_MODULE_VALUE_CONTAINER", "XRUNTIME_MODULE_TYPED_ARRAY"}},
    {"src/runtime/xl6_array_method_insert.c", D(R(SEQUENCE)),
        {"XRT_MODULE_VALUE_CONTAINER", "XRUNTIME_MODULE_TYPED_ARRAY_VALUE"}},
    {"src/runtime/xl6_array_method_find.c", D(R(SEQUENCE),R(FOREIGN_ERROR)),
        {"XRT_MODULE_VALUE_CONTAINER", "XRUNTIME_MODULE_TYPED_ARRAY_VALUE"}},
    {"src/runtime/xl6_array_extract_scalar.c", D(R(SEQUENCE)),
        {"XRT_MODULE_VALUE_CONTAINER", "XRUNTIME_MODULE_TYPED_ARRAY", "XRUNTIME_MODULE_RUNTIME_TYPE"}},
    {"src/runtime/xl6_array_extract_string.c", D(R(SEQUENCE)),
        {"XRT_MODULE_VALUE_CONTAINER", "XRUNTIME_MODULE_TYPED_ARRAY"}},
    {"src/runtime/xl6_array_extract_value.c", D(R(SEQUENCE)),
        {"XRT_MODULE_VALUE_CONTAINER", "XRUNTIME_MODULE_TYPED_ARRAY_VALUE"}},
    {"src/runtime/xl6_collection_extract_value.c", D(), {"XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_array_extract_f32.c", D(R(SEQUENCE),R(VALUE_ADMISSION)),
        {"XRT_MODULE_VALUE_CONTAINER", "XRUNTIME_MODULE_TYPED_ARRAY", "XRUNTIME_MODULE_RUNTIME_TYPE"}},
    {"src/runtime/xl6_array_extract_char.c", D(R(SEQUENCE)),
        {"XRT_MODULE_VALUE_CONTAINER", "XRUNTIME_MODULE_TYPED_ARRAY"}},
    {"src/runtime/xl6_array_extract_trivial.c", D(R(SEQUENCE),R(VALUE_ADMISSION)),
        {"XRT_MODULE_VALUE_CONTAINER", "XRUNTIME_MODULE_TYPED_ARRAY_VALUE"}},
    {"src/runtime/xl6_error_value.c", D(), {"XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_value_text.c", D(R(ERROR_VALUE),R(ARRAY_GRAPH),R(FOREIGN_ERROR)),
        {"XRT_MODULE_NUMBER_FORMAT", "XRT_MODULE_XLON_WRITE", "XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_print_values.c", D(R(VALUE_TEXT),R(SEQUENCE),R(FOREIGN_ERROR)),
        {"XRT_MODULE_CONSOLE", "XRT_MODULE_VALUE_CONTAINER", "XRT_MODULE_STRING", "XRUNTIME_MODULE_TYPED_ARRAY_VALUE"}},
    {"src/runtime/xl6_class_text.c", D(R(VALUE_TEXT)),
        {"XRT_MODULE_VALUE_CONTAINER", "XRT_MODULE_STRING"}},
    {"src/runtime/xl6_error_text.c", D(), {"XRT_MODULE_STRING"}},
    {"src/runtime/xl6_array_call_frame.c", D(R(ARRAY_TYPED),R(ARRAY_ERASED),R(ARRAY_REFERENCE_ADMISSION),R(CALL_FRAME_EXPANSION),R(FOREIGN_ERROR)),
        {"XRT_MODULE_VALUE_CONTAINER", "XRUNTIME_MODULE_TYPED_ARRAY_VALUE"}},
    {"src/runtime/xl6_array_entry.c", D(R(ARRAY_TYPED),R(ARRAY_ERASED),R(ARRAY_REFERENCE_VALUE),R(ARRAY_REFERENCE_ADMISSION),R(ARRAY_IMPORT),R(FOREIGN_ERROR)),
        {"XRT_MODULE_VALUE_CONTAINER", "XRT_MODULE_STRING", "XRUNTIME_MODULE_TYPED_ARRAY_VALUE"}},
    {"src/runtime/xl6_array_method_remove.c", D(R(SEQUENCE),R(FOREIGN_ERROR)),
        {"XRT_MODULE_VALUE_CONTAINER", "XRUNTIME_MODULE_TYPED_ARRAY_VALUE"}},
    {"src/runtime/xl6_value_contains.c", D(R(ARRAY_METHODS_FIND),R(LIST_NATIVE),R(SET_NATIVE),R(DICT_NATIVE),R(VALUE_WIRE_CONTAINS),R(FOREIGN_ERROR)),
        {"XRT_MODULE_VALUE_CONTAINER", "XRT_MODULE_STRING"}},
    {"src/runtime/xl6_dynamic_collection.c", D(R(ARRAY_ERASED),R(STACK_ERASED),R(STACK),R(ARRAY_METHODS_CORE),R(ARRAY_METHODS_INSERT),R(ARRAY_METHODS_FIND),R(ARRAY_METHODS_REMOVE),R(VALUE_CONTAINS),R(LIST_METHODS),R(SET_NATIVE),R(FOREIGN_ERROR),R(STACK_ERASED_METHODS)),
        {"XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_array_os_argv.c", D(R(ARRAY_ENTRY),R(FOREIGN_ERROR)),
        {"XRT_MODULE_UNICODE", "XRT_MODULE_VALUE_CONTAINER", "XRUNTIME_MODULE_TYPED_ARRAY_VALUE"}},
    {"src/runtime/xl6_array_producer.c", D(R(ARRAY_PRIMITIVE_ADMISSION),R(ARRAY_ERASED),R(FOREIGN_ERROR)),
        {"XRT_MODULE_VALUE_CONTAINER", "XRUNTIME_MODULE_TYPED_ARRAY_VALUE"}},
    {"src/runtime/xl6_queue_spsc.c", D(R(SEQUENCE),R(FOREIGN_ERROR),R(ARRAY_REFERENCE_VALUE)), {"XRUNTIME_MODULE_TYPED_QUEUE_SPSC"}},
    {"src/runtime/xl6_queue_mpsc.c", D(R(SEQUENCE),R(FOREIGN_ERROR),R(ARRAY_REFERENCE_VALUE)), {"XRUNTIME_MODULE_TYPED_QUEUE_MPSC"}},
    {"src/runtime/xl6_queue_mpmc.c", D(R(SEQUENCE),R(FOREIGN_ERROR),R(ARRAY_REFERENCE_VALUE)), {"XRUNTIME_MODULE_TYPED_QUEUE_MPMC"}},
    {"src/runtime/xl6_channel.c", D(R(SEQUENCE),R(FOREIGN_ERROR)), {"XRT_MODULE_CHANNEL", "XRT_MODULE_SPIN"}},
    {"src/runtime/xl6_tree_projection.c", D(R(SEQUENCE),R(FOREIGN_ERROR)), {"XRUNTIME_MODULE_TYPED_TREE"}},
    {"src/runtime/xl6_tree.c", D(R(SEQUENCE),R(TREE_PROJECTION),R(FOREIGN_ERROR)), {"XRUNTIME_MODULE_TYPED_TREE"}},
    {"src/runtime/xl6_tree_bytes_key.c", D(R(ARRAY_REFERENCE_ADMISSION)), {"XRUNTIME_MODULE_RUNTIME_VALUE_TYPE"}},
    {"src/runtime/xl6_tree_admission.c", D(R(TREE),R(ARRAY_SEQUENCE_ADMISSION),R(FOREIGN_ERROR)), {NULL}},
    {"src/runtime/xl6_queue_spsc_admission.c", D(R(TYPED_QUEUE_SPSC),R(ARRAY_SEQUENCE_ADMISSION),R(FOREIGN_ERROR)), {NULL}},
    {"src/runtime/xl6_queue_mpsc_admission.c", D(R(TYPED_QUEUE_MPSC),R(ARRAY_SEQUENCE_ADMISSION),R(FOREIGN_ERROR)), {NULL}},
    {"src/runtime/xl6_queue_mpmc_admission.c", D(R(TYPED_QUEUE_MPMC),R(ARRAY_SEQUENCE_ADMISSION),R(FOREIGN_ERROR)), {NULL}},
    {"src/runtime/xl6_semaphore.c", D(R(FOREIGN_ERROR)), {"XRT_MODULE_SEM", "XRT_MODULE_ATOMIC", "XRUNTIME_MODULE_RUNTIME_VALUE_TYPE"}},
    {"src/runtime/xl6_array_semaphore_admission.c", D(R(SEQUENCE),R(SEMAPHORE)), {NULL}},
    {"src/runtime/xl6_sync.c", D(R(FOREIGN_ERROR)), {"XRT_MODULE_ONCE", "XRT_MODULE_THREAD", "XRUNTIME_MODULE_RUNTIME_VALUE_TYPE"}},
    {"src/runtime/xl6_mutex.c", D(R(SYNC_CORE)), {"XRT_MODULE_MUTEX", "XRUNTIME_MODULE_RUNTIME_VALUE_TYPE"}},
    {"src/runtime/xl6_cond.c", D(R(SYNC_CORE)), {"XRT_MODULE_COND", "XRT_MODULE_MUTEX", "XRUNTIME_MODULE_RUNTIME_VALUE_TYPE"}},
    {"src/runtime/xl6_rwlock.c", D(R(SYNC_CORE)), {"XRT_MODULE_RWLOCK", "XRUNTIME_MODULE_RUNTIME_VALUE_TYPE"}},
    {"src/runtime/xl6_array_sync_admission.c", D(R(SEQUENCE),R(SYNC_CORE)), {NULL}},
    {"src/runtime/xl6_thread.c", D(R(CLOSURE),R(FOREIGN_ERROR),R(THREAD_EXIT)), {"XRT_MODULE_THREAD", "XRUNTIME_MODULE_RUNTIME_VALUE_TYPE"}},
    {"src/runtime/xl6_array_thread_admission.c", D(R(SEQUENCE),R(THREAD)), {NULL}},
    {"src/runtime/xl6_dynamic_fields.c", D(R(FOREIGN_ERROR)),
        {"XRUNTIME_MODULE_RUNTIME_DYNAMIC_FIELD"}},
    {"src/runtime/xl6_dynamic_fields_copy.c", D(R(DYNAMIC_FIELDS),R(VALUE_COPY_GRAPH)), {NULL}},
    {"src/runtime/xl6_restore_open.c", D(R(RESTORE_CLASS_LIST),R(DYNAMIC_FIELDS),R(NATIVE_HANDLE),R(FOREIGN_ERROR)), {NULL}},
    {"src/runtime/xl6_list_methods.c", D(R(LIST_NATIVE),R(FOREIGN_ERROR)), {"XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_list_value_iterator.c", D(R(VALUE_ITERATOR),R(LIST_NATIVE)), {NULL}},
    {"src/runtime/xl6_list_value_read.c", D(R(LIST_NATIVE),R(FOREIGN_ERROR)), {"XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_value_index_read.c", D(R(ARRAY_VALUE_READ),R(LIST_VALUE_READ),R(VALUE_NAMED_READ),R(FOREIGN_ERROR)), {"XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_value_index_write.c", D(R(ARRAY_ERASED),R(LIST_NATIVE),R(VALUE_NAMED_WRITE),R(FOREIGN_ERROR)), {"XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_value_object_fields.c", D(R(CODE_FINALIZER),R(DYNAMIC_FIELDS),R(FOREIGN_ERROR)), {"XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_value_wire_contains.c", D(R(FOREIGN_ERROR)), {"XRT_MODULE_VALUE_CONTAINER","XRT_MODULE_STRING"}},
    {"src/runtime/xl6_list_scalar_read.c", D(R(LIST_NATIVE),R(FOREIGN_ERROR)), {"XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_list_string_read.c", D(R(LIST_NATIVE),R(FOREIGN_ERROR),R(STRING_TYPE)), {"XRT_MODULE_VALUE_CONTAINER","XRT_MODULE_STRING"}},
    {"src/runtime/xl6_list_value_write.c", D(R(LIST_NATIVE),R(FOREIGN_ERROR)), {"XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_list_algebra.c", D(R(LIST_NATIVE),R(FOREIGN_ERROR)), {"XRT_MODULE_VALUE_COLLECTION"}},
    {"src/runtime/xl6_value_copy.c", D(R(ARRAY_ERASED),R(LIST_NATIVE),R(SET_NATIVE),R(DICT_NATIVE),R(FOREIGN_ERROR)), {NULL}},
    {"src/runtime/xl6_list_graph.c", D(R(LIST_NATIVE),R(FOREIGN_ERROR)), {"XRT_MODULE_VALUE_GRAPH"}},
    {"src/runtime/xl6_value_copy_graph.c", D(R(ARRAY_GRAPH),R(LIST_GRAPH)), {NULL}},
    {"src/runtime/xl6_persistence_project_list.c", D(R(PERSISTENCE_PROJECT),R(LIST_GRAPH)), {NULL}},
    {"src/runtime/xl6_list_import_chain.c", D(R(LIST_NATIVE),R(ARRAY_INPUT),R(ARRAY_REFERENCE_ADMISSION)), {NULL}},
    {"src/runtime/xl6_list_schema.c", D(R(LIST_NATIVE),R(ARRAY_SEQUENCE_ADMISSION),R(FOREIGN_ERROR)), {NULL}},
    {"src/runtime/xl6_set_key_policy.c", D(R(FOREIGN_ERROR),R(NATIVE_LEAF_VALUE)), {"XRUNTIME_MODULE_TYPED_SET","XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_set.c", D(R(SEQUENCE),R(SET_KEYS),R(ARRAY_REFERENCE_VALUE),R(ARRAY_FUTURE_ADMISSION),R(FOREIGN_ERROR)),
        {"XRUNTIME_MODULE_TYPED_SET_VALUE"}},
    {"src/runtime/xl6_set_schema.c", D(R(SET_NATIVE),R(ARRAY_SEQUENCE_ADMISSION),R(FOREIGN_ERROR)), {NULL}},
    {"src/runtime/xl6_set_value_iterator.c", D(R(VALUE_ITERATOR),R(SET_NATIVE)), {NULL}},
    {"src/runtime/xl6_collection_value_iterator.c", D(R(LIST_VALUE_ITERATOR),R(SET_VALUE_ITERATOR),R(DICT_VALUE_ITERATOR)), {NULL}},
    {"src/runtime/xl6_set_graph.c", D(R(SET_NATIVE),R(FOREIGN_ERROR)), {"XRT_MODULE_VALUE_GRAPH"}},
    {"src/runtime/xl6_value_copy_collections.c", D(R(ARRAY_GRAPH),R(LIST_GRAPH),R(SET_GRAPH),R(DICT_GRAPH)), {NULL}},
    {"src/runtime/xl6_persistence_project_collections.c", D(R(PERSISTENCE_PROJECT),R(LIST_GRAPH),R(SET_GRAPH),R(DICT_GRAPH)), {NULL}},
    {"src/runtime/xl6_set_methods.c", D(R(SET_NATIVE),R(FOREIGN_ERROR)), {NULL}},
    {"src/runtime/xl6_set_import_chain.c", D(R(SET_NATIVE),R(ARRAY_INPUT),R(ARRAY_REFERENCE_ADMISSION)), {NULL}},
    {"src/runtime/xl6_collection_count_all.c", D(R(COLLECTION_COUNT),R(LIST_NATIVE),R(SET_NATIVE),R(DICT_NATIVE),R(FOREIGN_ERROR)), {NULL}},
    {"src/runtime/xl6_value_copy_set_graph.c", D(R(ARRAY_GRAPH),R(SET_GRAPH)), {NULL}},
    {"src/runtime/xl6_persistence_project_set.c", D(R(PERSISTENCE_PROJECT),R(SET_GRAPH)), {NULL}},
    {"src/runtime/xl6_restore_class_set.c", D(R(RESTORE_CLASS),R(SET_NATIVE)), {NULL}},
    {"src/runtime/xl6_restore_class_collections.c", D(R(RESTORE_CLASS_LIST),R(SET_NATIVE)), {NULL}},
    {"src/runtime/xl6_dict.c", D(R(SEQUENCE),R(ARRAY_FUTURE_ADMISSION),R(ARRAY_REFERENCE_VALUE),R(FOREIGN_ERROR)),
        {"XRUNTIME_MODULE_TYPED_DICT", "XRUNTIME_MODULE_TYPED_VALUE", "XRT_MODULE_STRING", "XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_dict_value_iterator.c", D(R(VALUE_ITERATOR),R(DICT_NATIVE)), {NULL}},
    {"src/runtime/xl6_dict_graph.c", D(R(DICT_NATIVE),R(FOREIGN_ERROR)), {"XRT_MODULE_VALUE_GRAPH"}},
    {"src/runtime/xl6_dict_schema.c", D(R(DICT_NATIVE),R(ARRAY_SEQUENCE_ADMISSION),R(FOREIGN_ERROR)), {NULL}},
    {"src/runtime/xl6_dict_value_index.c", D(R(DICT_NATIVE),R(FOREIGN_ERROR)), {"XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_dict_methods.c", D(R(DICT_NATIVE),R(FOREIGN_ERROR)), {"XRT_MODULE_VALUE_COLLECTION"}},
    {"src/runtime/xl6_dict_import_chain.c", D(R(DICT_NATIVE),R(ARRAY_INPUT),R(ARRAY_REFERENCE_ADMISSION)), {NULL}},
    {"src/runtime/xl6_slot_map.c", D(R(SEQUENCE),R(ARRAY_FUTURE_ADMISSION),R(FOREIGN_ERROR),R(SLOT_MAP_KEY_NATIVE)),
        {"XRT_MODULE_SLOT_MAP"}},
    {"src/runtime/xl6_slot_map_key.c", D(R(FOREIGN_ERROR)), {"XRUNTIME_MODULE_RUNTIME_TYPE","XRT_MODULE_ATOMIC"}},
    {"src/runtime/xl6_slot_map_key_value.c", D(R(SLOT_MAP_KEY_NATIVE),R(NATIVE_LEAF_VALUE)),
        {"XRT_MODULE_VALUE","XRT_MODULE_HASH64"}},
    {"src/runtime/xl6_array_native_value_admission.c", D(R(ARRAY_NATIVE_VALUE_POLICY),R(SLOT_MAP_KEY_VALUE)),
        {NULL}},
    {"src/runtime/xl6_native_leaf_value.c", D(R(FOREIGN_ERROR)),
        {"XRT_MODULE_VALUE","XRUNTIME_MODULE_RUNTIME_TYPE","XRT_MODULE_ATOMIC"}},
    {"src/runtime/xl6_array_extract_native_value.c", D(R(SEQUENCE),R(NATIVE_LEAF_VALUE),R(FOREIGN_ERROR)),
        {"XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_slot_map_keys.c", D(R(SLOT_MAP_NATIVE),R(ARRAY_NATIVE_VALUE_ADMISSION)),
        {NULL}},
    {"src/runtime/xl6_value_named_read.c",D(R(DICT_NATIVE),R(VALUE_OBJECT_FIELDS),R(FOREIGN_ERROR)),{"XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_value_named_write.c",D(R(DICT_NATIVE),R(VALUE_OBJECT_FIELDS),R(FOREIGN_ERROR)),{"XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_array_union_admission.c",D(R(SEQUENCE)),{"XRT_MODULE_VALUE","XRT_MODULE_ATOMIC"}},
    {"src/runtime/xl6_slot_map_pairs.c",D(R(SLOT_MAP_NATIVE),R(ARRAY_NATIVE_VALUE_ADMISSION),R(SLOT_MAP_KEY_VALUE),R(FOREIGN_ERROR)),{NULL}},
    {"src/runtime/xl6_slot_map_iterator.c",D(R(SEQUENCE),R(VALUE_ITERATOR),R(FOREIGN_ERROR)),{"XRUNTIME_MODULE_TYPED_ARRAY_VALUE"}},
    {"src/runtime/xl6_slot_map_iterator_key.c",D(R(SLOT_MAP_ITERATOR_CORE),R(SLOT_MAP_KEY_NATIVE),R(FOREIGN_ERROR)),{NULL}},
    {"src/runtime/xl6_slot_map_iterator_pair.c",D(R(SLOT_MAP_ITERATOR_KEY),R(SLOT_MAP_PAIRS)),{NULL}},
    {"src/runtime/xl6_slot_map_iterator_keys.c",D(R(SLOT_MAP_ITERATOR_KEY),R(SLOT_MAP_KEYS)),{NULL}},
    {"src/runtime/xl6_slot_map_iterator_values.c",D(R(SLOT_MAP_ITERATOR_CORE),R(SLOT_MAP_NATIVE)),{NULL}},
    {"src/runtime/xl6_slot_map_iterator_count.c",D(R(SLOT_MAP_ITERATOR_CORE),R(SLOT_MAP_NATIVE)),{NULL}},
    {"src/runtime/xl6_capture_v2.c",D(R(CAPTURE),R(FOREIGN_ERROR)),{"XRUNTIME_MODULE_RUNTIME_TYPE"}},
    {"src/runtime/xl6_slot_map_key_capture.c",D(R(CAPTURE_V2),R(SLOT_MAP_KEY_NATIVE)),{NULL}},
    {"src/runtime/xl6_native_future_payload.c",D(R(CAPTURE_V2),R(FOREIGN_ERROR)),
        {"XRT_MODULE_MEMORY","XRT_MODULE_FUTURE","XRUNTIME_MODULE_RUNTIME_TYPE"}},
    {"src/runtime/xl6_nullable_slot_map_key.c",D(R(SLOT_MAP_KEY_NATIVE),R(SLOT_MAP_KEY_VALUE),R(FOREIGN_ERROR)),
        {"XRUNTIME_MODULE_RUNTIME_TYPE"}},
    {"src/runtime/xl6_nullable_slot_map_key_capture.c",D(R(CAPTURE_V2),R(NULLABLE_SLOT_MAP_KEY)),{NULL}},
    {"src/runtime/xl6_closure_capture_v3.c",D(R(CLOSURE),R(CAPTURE_V2),R(FOREIGN_ERROR)),{NULL}},
    {"src/runtime/xl6_array_native_value_policy.c",D(R(SEQUENCE),R(FOREIGN_ERROR)),{"XRT_MODULE_ATOMIC"}},
    {"src/runtime/xl6_nullable_slot_map_key_collection.c",D(R(ARRAY_NATIVE_VALUE_POLICY),R(NULLABLE_SLOT_MAP_KEY)),{NULL}},
    {"src/runtime/xl6_cell_native.c",D(R(CELL),R(CAPTURE_V2),R(FOREIGN_ERROR)),{NULL}},
    {"src/runtime/xl6_stack_callable.c",D(R(SEQUENCE),R(CLOSURE),R(FOREIGN_ERROR)),
        {"XRUNTIME_MODULE_TYPED_STACK","XRUNTIME_MODULE_RUNTIME_CALL","XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE"}},
    {"src/runtime/xl6_wait_queue.c",D(R(SEQUENCE),R(FOREIGN_ERROR),R(MUTEX),R(COND)),
        {"XRUNTIME_MODULE_TYPED_QUEUE_MPMC","XRT_MODULE_COND","XRT_MODULE_WAIT"}},
    {"src/runtime/xl6_wait_queue_admission.c",D(R(WAIT_QUEUE_NATIVE),R(ARRAY_SEQUENCE_ADMISSION),R(FOREIGN_ERROR)),{NULL}},
    {"src/runtime/xl6_path_build.c",D(R(ARRAY_INPUT),R(FOREIGN_ERROR)),{"XRT_MODULE_PATH"}},
    {"src/runtime/xl6_stack_erased.c",D(R(SEQUENCE),R(ARRAY_REFERENCE_VALUE),R(FOREIGN_ERROR)),
        {"XRUNTIME_MODULE_TYPED_ARRAY_VALUE"}},
    {"src/runtime/xl6_char_checked.c",D(R(FOREIGN_ERROR)),{"XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_stack_erased_methods.c",D(R(STACK_ERASED),R(STACK_BATCH),R(STACK),R(FOREIGN_ERROR)),{NULL}},
    {"src/runtime/xl6_stack_erased_try.c",D(R(STACK_ERASED),R(FOREIGN_ERROR)),{"XRUNTIME_MODULE_RUNTIME_CALL"}},
    {"src/runtime/xl6_dynamic_product.c",D(R(FOREIGN_ERROR)),{"XRUNTIME_MODULE_RUNTIME_CALL","XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE"}},
    {"src/runtime/xl6_dynamic_collection_vector.c",D(R(STACK_ERASED_TRY),R(FOREIGN_ERROR)),{NULL}},
    {"src/runtime/xl6_float_convert.c",D(R(FOREIGN_ERROR)),
        {"XRT_MODULE_NUMBER_FLOAT", "XRUNTIME_MODULE_VALUE_CONVERT"}},
    {"src/runtime/xl6_sequence_projection_schema.c",D(R(SEQUENCE),R(FOREIGN_ERROR)),{NULL}},
    {"src/runtime/xl6_container_projection.c",D(R(FOREIGN_ERROR)),{NULL}},
    {"src/runtime/xl6_list_projection_cast.c",D(R(CONTAINER_PROJECTION),R(LIST_NATIVE)),{NULL}},
    {"src/runtime/xl6_set_projection_cast.c",D(R(CONTAINER_PROJECTION),R(SET_NATIVE)),{NULL}},
    {"src/runtime/xl6_dict_projection_cast.c",D(R(CONTAINER_PROJECTION),R(DICT_NATIVE)),{NULL}},
    {"src/runtime/xl6_tree_projection_cast.c",D(R(CONTAINER_PROJECTION),R(TREE)),{NULL}},
    {"src/runtime/xl6_value_domain_projection.c",D(R(CONTAINER_PROJECTION),R(SEQUENCE)),{NULL}},
    {"src/runtime/xl6_channel_admission.c",D(R(TYPED_CHANNEL),R(ARRAY_SEQUENCE_ADMISSION),R(FOREIGN_ERROR)),{NULL}},
    {"src/runtime/xl6_slot_map_admission.c",D(R(SLOT_MAP_NATIVE),R(ARRAY_SEQUENCE_ADMISSION),R(FOREIGN_ERROR)),{NULL}},
    {"src/runtime/xl6_call_frame_expansion.c",D(R(FOREIGN_ERROR)),{"XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_dict_call_frame.c",D(R(DICT_NATIVE),R(ARRAY_REFERENCE_ADMISSION),R(CALL_FRAME_EXPANSION),R(FOREIGN_ERROR)),
        {"XRT_MODULE_VALUE_CONTAINER", "XRT_MODULE_STRING"}},
    {"src/runtime/xl6_native_scalar_convert.c",D(R(FOREIGN_ERROR)),{"XRUNTIME_MODULE_RUNTIME_CONVERT"}},
    {"src/runtime/xl6_value_native_scalar_convert.c",D(R(NATIVE_SCALAR_CONVERT),R(FOREIGN_ERROR)),{"XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_integer_convert.c",D(R(FOREIGN_ERROR)),{"XRT_MODULE_NUMBER_INTEGER","XRUNTIME_MODULE_RUNTIME_CONVERT","XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_static_module.c",D(R(CODE_LEASE),R(OWNERSHIP_PLAN),R(FOREIGN_ERROR)),
        {"XRT_MODULE_MUTEX","XRT_MODULE_VALUE_GRAPH"}},
    {"src/runtime/xl6_ownership_catalog.c",D(R(FOREIGN_ERROR)),{"XRT_MODULE_VALUE_GRAPH","XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_ownership_domain_code_lease.c",D(R(OWNERSHIP_CATALOG),R(CODE_LEASE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_class.c",D(R(OWNERSHIP_CATALOG),R(CODE_FINALIZER)),{NULL}},
    {"src/runtime/xl6_ownership_domain_cell.c",D(R(OWNERSHIP_CATALOG),R(CELL)),{NULL}},
    {"src/runtime/xl6_ownership_domain_tree.c",D(R(OWNERSHIP_CATALOG),R(TREE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_semaphore.c",D(R(OWNERSHIP_CATALOG),R(SEMAPHORE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_sync.c",D(R(OWNERSHIP_CATALOG),R(SYNC_CORE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_thread.c",D(R(OWNERSHIP_CATALOG),R(THREAD)),{NULL}},
    {"src/runtime/xl6_ownership_domain_value_iterator.c",D(R(OWNERSHIP_CATALOG),R(VALUE_ITERATOR)),{NULL}},
    {"src/runtime/xl6_ownership_domain_error_value.c",D(R(OWNERSHIP_CATALOG),R(ERROR_VALUE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_slot_map_key.c",D(R(OWNERSHIP_CATALOG),R(SLOT_MAP_KEY_VALUE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_channel.c",D(R(OWNERSHIP_CATALOG),R(TYPED_CHANNEL)),{NULL}},
    {"src/runtime/xl6_ownership_domain_sequence.c",D(R(OWNERSHIP_CATALOG),R(SEQUENCE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_list.c",D(R(OWNERSHIP_CATALOG),R(LIST_NATIVE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_set.c",D(R(OWNERSHIP_CATALOG),R(SET_NATIVE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_dict.c",D(R(OWNERSHIP_CATALOG),R(DICT_NATIVE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_slot_map.c",D(R(OWNERSHIP_CATALOG),R(SLOT_MAP_NATIVE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_spsc_queue.c",D(R(OWNERSHIP_CATALOG),R(TYPED_QUEUE_SPSC)),{NULL}},
    {"src/runtime/xl6_ownership_domain_mpsc_queue.c",D(R(OWNERSHIP_CATALOG),R(TYPED_QUEUE_MPSC)),{NULL}},
    {"src/runtime/xl6_ownership_domain_mpmc_queue.c",D(R(OWNERSHIP_CATALOG),R(TYPED_QUEUE_MPMC)),{NULL}},
    {"src/runtime/xl6_ownership_domain_wait_queue.c",D(R(OWNERSHIP_CATALOG),R(WAIT_QUEUE_NATIVE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_process_command.c",D(R(OWNERSHIP_CATALOG),R(PROCESS_COMMAND)),{NULL}},
    {"src/runtime/xl6_ownership_domain_process_status.c",D(R(OWNERSHIP_CATALOG),R(PROCESS_STATUS)),{NULL}},
    {"src/runtime/xl6_ownership_domain_process_live.c",D(R(OWNERSHIP_CATALOG),R(PROCESS_LIVE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_process_future.c",D(R(OWNERSHIP_CATALOG)),{"XRT_MODULE_PROCESS_FUTURE"}},
    {"src/runtime/xl6_ownership_domain_buffer_iterator.c",D(R(OWNERSHIP_CATALOG),R(BUFFER_ITERATOR)),{NULL}},
    {"src/runtime/xl6_ownership_domain_sequence_future.c",D(R(OWNERSHIP_CATALOG),R(ARRAY_FUTURE_ADMISSION)),{NULL}},
    {"src/runtime/xl6_ownership_domain_sequence_semaphore.c",D(R(OWNERSHIP_CATALOG),R(ARRAY_SEMAPHORE_ADMISSION)),{NULL}},
    {"src/runtime/xl6_ownership_domain_sequence_sync.c",D(R(OWNERSHIP_CATALOG),R(ARRAY_SYNC_ADMISSION)),{NULL}},
    {"src/runtime/xl6_ownership_domain_sequence_thread.c",D(R(OWNERSHIP_CATALOG),R(ARRAY_THREAD_ADMISSION)),{NULL}},
    {"src/runtime/xl6_ownership_domain_sequence_slot_map_key.c",D(R(OWNERSHIP_CATALOG),R(ARRAY_NATIVE_VALUE_ADMISSION)),{NULL}},
    {"src/runtime/xl6_ownership_domain_closure.c",D(R(OWNERSHIP_CATALOG),R(CLOSURE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_dynamic_fields.c",D(R(OWNERSHIP_CATALOG),R(DYNAMIC_FIELDS)),{NULL}},
    {"src/runtime/xl6_ownership_domain_executor.c",D(R(OWNERSHIP_CATALOG),R(EXECUTOR)),{NULL}},
    {"src/runtime/xl6_ownership_domain_generator.c",D(R(OWNERSHIP_CATALOG),R(GENERATOR)),{NULL}},
    {"src/runtime/xl6_ownership_domain_queue.c",D(R(OWNERSHIP_CATALOG),R(QUEUE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_frame.c",D(R(OWNERSHIP_CATALOG),R(CONTINUATION_FRAME)),{NULL}},
    {"src/runtime/xl6_ownership_domain_future_callback.c",D(R(OWNERSHIP_CATALOG),R(FUTURE_CALLBACK)),{NULL}},
    {"src/runtime/xl6_ownership_domain_task.c",D(R(OWNERSHIP_CATALOG),R(TASK_FRAME)),{NULL}},
    {"src/runtime/xl6_ownership_domain_future_payload.c",D(R(OWNERSHIP_CATALOG),R(FUTURE_PAYLOAD)),{NULL}},
    {"src/runtime/xl6_ownership_domain_native_future.c",D(R(OWNERSHIP_CATALOG)),{"XRT_MODULE_FUTURE"}},
    {"src/runtime/xl6_ownership_domain_future_aggregate.c",D(R(OWNERSHIP_CATALOG),R(FUTURE_AGGREGATE)),{"XRT_MODULE_FUTURE_COMBINE"}},
    {"src/runtime/xl6_ownership_domain_network.c",D(R(OWNERSHIP_CATALOG),R(NETWORK_SERVICE)),{"XRT_MODULE_NET_ENGINE","XRT_MODULE_NET_RESOLVER"}},
    {"src/runtime/xl6_xlang_context_policy.c",D(R(NATIVE_HANDLE)),{NULL},XL6_ARTIFACT_HOST_XLANG_RUNTIME_V1},
    {"src/runtime/xl6_ownership_domain_xlang_context.c",D(R(OWNERSHIP_CATALOG),R(XLANG_CONTEXT_POLICY)),{NULL}},
    {"src/runtime/xl6_ownership_domain_buffer.c",D(R(OWNERSHIP_CATALOG),R(BUFFER_CORE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_resource_time_date_time.c",D(R(OWNERSHIP_CATALOG),R(TIME_DATETIME)),{NULL}},
    {"src/runtime/xl6_ownership_domain_resource_unicode_validator.c",D(R(OWNERSHIP_CATALOG),R(UNICODE_VALIDATOR)),{NULL}},
    {"src/runtime/xl6_ownership_domain_resource_text_iterator.c",D(R(OWNERSHIP_CATALOG),R(TEXT_ITERATOR)),{NULL}},
    {"src/runtime/xl6_ownership_domain_resource_pattern_core.c",D(R(OWNERSHIP_CATALOG),R(PATTERN_CORE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_resource_pattern_options.c",D(R(OWNERSHIP_CATALOG),R(PATTERN_OPTIONS)),{NULL}},
    {"src/runtime/xl6_ownership_domain_resource_deflater.c",D(R(OWNERSHIP_CATALOG),R(COMPRESS_DEFLATER)),{NULL}},
    {"src/runtime/xl6_ownership_domain_resource_inflater.c",D(R(OWNERSHIP_CATALOG),R(COMPRESS_INFLATER)),{NULL}},
    {"src/runtime/xl6_ownership_domain_resource_template.c",D(R(OWNERSHIP_CATALOG),R(TEMPLATE_CORE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_resource_template_call.c",D(R(OWNERSHIP_CATALOG),R(TEMPLATE_RENDER)),{NULL}},
    {"src/runtime/xl6_ownership_domain_resource_template_engine.c",D(R(OWNERSHIP_CATALOG),R(TEMPLATE_EXTENSION)),{NULL}},
    {"src/runtime/xl6_ownership_domain_resource_pattern_match.c",D(R(OWNERSHIP_CATALOG),R(PATTERN_MATCH)),{NULL}},
    {"src/runtime/xl6_ownership_domain_resource_pattern_builder.c",D(R(OWNERSHIP_CATALOG),R(PATTERN_BUILDER)),{NULL}},
    {"src/runtime/xl6_ownership_domain_resource_process_pipeline_config.c",D(R(OWNERSHIP_CATALOG),R(PROCESS_PIPELINE_CONFIG)),{NULL}},
    {"src/runtime/xl6_ownership_domain_resource_process_pipeline.c",D(R(OWNERSHIP_CATALOG),R(PROCESS_PIPELINE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_resource_process_run.c",D(R(OWNERSHIP_CATALOG),R(PROCESS_RUN)),{NULL}},
    {"src/runtime/xl6_ownership_domain_resource_regex_core.c",D(R(OWNERSHIP_CATALOG),R(REGEX_CORE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_resource_regex_match.c",D(R(OWNERSHIP_CATALOG),R(REGEX_MATCH)),{NULL}},
    {"src/runtime/xl6_ownership_domain_resource_regex_iterator.c",D(R(OWNERSHIP_CATALOG),R(REGEX_ITERATOR)),{NULL}},
    {"src/runtime/xl6_ownership_domain_resource_regex_split.c",D(R(OWNERSHIP_CATALOG),R(REGEX_SPLIT)),{NULL}},
    {"src/runtime/xl6_ownership_domain_resource_regex_set.c",D(R(OWNERSHIP_CATALOG),R(REGEX_SET)),{NULL}},
    {"src/runtime/xl6_ownership_domain_resource_regex_replace_func.c",D(R(OWNERSHIP_CATALOG),R(REGEX_REPLACE_FUNC)),{NULL}},
    {"src/runtime/xl6_ownership_domain_resource_signal_queue.c",D(R(OWNERSHIP_CATALOG),R(SIGNAL_QUEUE)),{NULL}},
    {"src/runtime/xl6_random_state.c",D(R(NATIVE_HANDLE)),{"XRT_MODULE_RANDOM","XRT_MODULE_THREAD","XRT_MODULE_MEMORY"}},
    {"src/runtime/xl6_fs_file_owner.c",D(R(NATIVE_HANDLE)),{"XRT_MODULE_FILE","XRT_MODULE_ATOMIC"}},
    {"src/runtime/xl6_ownership_domain_resource_process_options.c",D(R(OWNERSHIP_CATALOG),R(PROCESS_OPTIONS)),{"XRT_MODULE_CANCEL"}},
    {"src/runtime/xl6_ownership_domain_resource_random_state.c",D(R(OWNERSHIP_CATALOG),R(RANDOM_STATE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_resource_fs_file.c",D(R(OWNERSHIP_CATALOG),R(FS_FILE_OWNER)),{NULL}},
    {"src/runtime/xl6_vfs_core.c",D(R(NATIVE_HANDLE)),{"XRT_MODULE_VFS"}},
    {"src/runtime/xl6_vfs_memory.c",D(R(VFS_CORE),R(NATIVE_HANDLE)),{"XRT_MODULE_VFS_MEMORY"}},
    {"src/runtime/xl6_vfs_disk.c",D(R(VFS_CORE),R(NATIVE_HANDLE)),{"XRT_MODULE_VFS_DISK"}},
    {"src/runtime/xl6_vfs_pack.c",D(R(VFS_CORE),R(NATIVE_HANDLE)),{"XRT_MODULE_VFS_PACK","XRT_MODULE_FILE","XRT_MODULE_ATOMIC"}},
    {"src/runtime/xl6_vfs_file.c",D(R(VFS_CORE),R(FS_FILE_OWNER)),{"XRT_MODULE_VFS","XRT_MODULE_FILE","XRT_MODULE_ATOMIC"}},
    {"src/runtime/xl6_vfs_info.c",D(R(VFS_CORE),R(FS_INFO_OWNER)),{"XRT_MODULE_VFS","XRT_MODULE_FILE"}},
    {"src/runtime/xl6_vfs_dir.c",D(R(VFS_CORE),R(FS_DIR_OWNER)),{"XRT_MODULE_VFS","XRT_MODULE_DIR"}},
    {"src/runtime/xl6_fs_info_owner.c",D(R(NATIVE_HANDLE)),{"XRT_MODULE_FILE"}},
    {"src/runtime/xl6_fs_dir_owner.c",D(R(NATIVE_HANDLE)),{"XRT_MODULE_DIR"}},
    {"src/runtime/xl6_ownership_domain_resource_vfs_core.c",D(R(OWNERSHIP_CATALOG),R(VFS_CORE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_resource_vfs_memory.c",D(R(OWNERSHIP_CATALOG),R(VFS_MEMORY)),{NULL}},
    {"src/runtime/xl6_ownership_domain_resource_vfs_disk.c",D(R(OWNERSHIP_CATALOG),R(VFS_DISK)),{NULL}},
    {"src/runtime/xl6_ownership_domain_resource_vfs_pack.c",D(R(OWNERSHIP_CATALOG),R(VFS_PACK)),{NULL}},
    {"src/runtime/xl6_ownership_domain_resource_fs_info_owner.c",D(R(OWNERSHIP_CATALOG),R(FS_INFO_OWNER)),{NULL}},
    {"src/runtime/xl6_ownership_domain_resource_fs_dir_owner.c",D(R(OWNERSHIP_CATALOG),R(FS_DIR_OWNER)),{NULL}},
    {"src/runtime/xl6_io_read_file.c",NULL,0,{"XRT_MODULE_IO_FILE","XRT_MODULE_IO_BUFFER","XRT_MODULE_BUFFER","XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_io_write_file.c",NULL,0,{"XRT_MODULE_IO_FILE","XRT_MODULE_FILE","XRT_MODULE_BUFFER","XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_io_reader.c",D(R(NATIVE_HANDLE)),{"XRT_MODULE_IO"}},
    {"src/runtime/xl6_io_reader_data.c",NULL,0,{"XRT_MODULE_IO_BUFFER","XRT_MODULE_BUFFER","XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_io_reader_position.c",NULL,0,{"XRT_MODULE_IO"}},
    {"src/runtime/xl6_io_writer.c",D(R(NATIVE_HANDLE)),{"XRT_MODULE_IO"}},
    {"src/runtime/xl6_io_writer_data.c",NULL,0,{"XRT_MODULE_IO","XRT_MODULE_BUFFER","XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_io_writer_position.c",NULL,0,{"XRT_MODULE_IO"}},
    {"src/runtime/xl6_io_line.c",D(R(NATIVE_HANDLE)),{"XRT_MODULE_IO_LINE"}},
    {"src/runtime/xl6_io_copy.c",NULL,0,{"XRT_MODULE_IO"}},
    {"src/runtime/xl6_io_file.c",D(R(FS_FILE_OWNER)),{"XRT_MODULE_IO","XRT_MODULE_FILE","XRT_MODULE_ATOMIC"}},
    {"src/runtime/xl6_io_reader_open.c",NULL,0,{"XRT_MODULE_IO_FILE","XRT_MODULE_FILE"}},
    {"src/runtime/xl6_io_reader_memory.c",NULL,0,{"XRT_MODULE_IO_BUFFER","XRT_MODULE_BUFFER","XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_io_reader_standard.c",NULL,0,{"XRT_MODULE_IO_STANDARD"}},
    {"src/runtime/xl6_io_writer_open.c",NULL,0,{"XRT_MODULE_IO_FILE","XRT_MODULE_FILE"}},
    {"src/runtime/xl6_io_writer_buffer.c",NULL,0,{"XRT_MODULE_IO_BUFFER","XRT_MODULE_BUFFER","XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_io_writer_standard.c",NULL,0,{"XRT_MODULE_IO_STANDARD"}},
    {"src/runtime/xl6_ownership_domain_resource_io_reader.c",D(R(OWNERSHIP_CATALOG),R(IO_READER)),{NULL}},
    {"src/runtime/xl6_ownership_domain_resource_io_writer.c",D(R(OWNERSHIP_CATALOG),R(IO_WRITER)),{NULL}},
    {"src/runtime/xl6_ownership_domain_resource_io_line.c",D(R(OWNERSHIP_CATALOG),R(IO_LINE)),{NULL}},
    {"src/runtime/xl6_fs_map_owner.c",D(R(NATIVE_HANDLE),R(FS_FILE_OWNER)),{"XRT_MODULE_FILE_MAP","XRT_MODULE_FILE","XRT_MODULE_ATOMIC"}},
    {"src/runtime/xl6_fs_lock_owner.c",D(R(NATIVE_HANDLE),R(FS_FILE_OWNER)),{"XRT_MODULE_FILE_LOCK","XRT_MODULE_FILE","XRT_MODULE_ATOMIC"}},
    {"src/runtime/xl6_fs_root_owner.c",D(R(NATIVE_HANDLE)),{"XRT_MODULE_FILE_ROOT"}},
    {"src/runtime/xl6_ownership_domain_resource_fs_map.c",D(R(OWNERSHIP_CATALOG),R(FS_MAP_OWNER)),{NULL}},
    {"src/runtime/xl6_ownership_domain_resource_fs_lock.c",D(R(OWNERSHIP_CATALOG),R(FS_LOCK_OWNER)),{NULL}},
    {"src/runtime/xl6_ownership_domain_resource_fs_root.c",D(R(OWNERSHIP_CATALOG),R(FS_ROOT_OWNER)),{NULL}},
    {"src/runtime/xl6_task_group_future.c",D(R(TASK_GROUP),R(FUTURE_CALLBACK)),{"XRT_MODULE_FUTURE"}},
    {"src/runtime/xl6_ownership_domain_resource_task_group.c",D(R(OWNERSHIP_CATALOG),R(SIGNAL_TASK_GROUP)),{"XRT_MODULE_TASK_GROUP"}},
    {"src/runtime/xl6_ownership_domain_native_future_payload.c",D(R(OWNERSHIP_CATALOG),R(NATIVE_FUTURE_PAYLOAD)),{NULL}},
    {"src/runtime/xl6_future_value.c",D(R(FUTURE_PAYLOAD),R(NATIVE_FUTURE_PAYLOAD)),{"XRT_MODULE_FUTURE","XRUNTIME_MODULE_RUNTIME_TYPE"}},
    {"src/runtime/xl6_promise.c",D(R(NATIVE_HANDLE),R(FUTURE_VALUE),R(FOREIGN_ERROR)),{"XRT_MODULE_FUTURE","XRT_MODULE_ATOMIC","XRT_MODULE_VALUE_GRAPH"}},
    {"src/runtime/xl6_promise_domain.c",D(R(OWNERSHIP_CATALOG),R(PROMISE),R(OWNERSHIP_DOMAIN_NATIVE_FUTURE),R(OWNERSHIP_DOMAIN_FUTURE_PAYLOAD),R(OWNERSHIP_DOMAIN_NATIVE_FUTURE_PAYLOAD)),{NULL}},
    {"src/runtime/xl6_channel_wait.c",D(R(TYPED_CHANNEL),R(FOREIGN_ERROR)),{"XRT_MODULE_CHANNEL"}},
    {"src/runtime/xl6_channel_cancel.c",D(R(TYPED_CHANNEL),R(FOREIGN_ERROR)),{"XRT_MODULE_CHANNEL_CANCEL"}},
    {"src/runtime/xl6_channel_select.c",D(R(TYPED_CHANNEL),R(FOREIGN_ERROR)),{"XRT_MODULE_CHANNEL_SELECT"}},
    {"src/runtime/xl6_channel_select_cancel.c",D(R(TYPED_CHANNEL_SELECT),R(FOREIGN_ERROR)),{"XRT_MODULE_CHANNEL_SELECT_CANCEL"}},
    {"src/runtime/xl6_channel_case.c",D(R(TYPED_CHANNEL),R(NATIVE_HANDLE),R(FOREIGN_ERROR)),{"XRT_MODULE_ATOMIC","XRT_MODULE_THREAD","XRT_MODULE_VALUE_GRAPH"}},
    {"src/runtime/xl6_channel_cases_select.c",D(R(CHANNEL_CASE),R(TYPED_CHANNEL_SELECT)),{NULL}},
    {"src/runtime/xl6_channel_cases_select_cancel.c",D(R(CHANNEL_CASE),R(TYPED_CHANNEL_SELECT_CANCEL)),{NULL}},
    {"src/runtime/xl6_ownership_domain_channel_case.c",D(R(OWNERSHIP_CATALOG),R(CHANNEL_CASE)),{NULL}},
    {"src/runtime/xl6_channel_case_language.c",D(R(CHANNEL_CASE)),{NULL}},
    {"src/runtime/xl6_channel_case_array.c",D(R(CHANNEL_CASE),R(SEQUENCE)),{NULL}},
    {"src/runtime/xl6_channel_select_language.c",D(R(CHANNEL_CASE_ARRAY),R(CHANNEL_CASE_SELECT)),{NULL}},
    {"src/runtime/xl6_channel_select_cancel_language.c",D(R(CHANNEL_CASE_ARRAY),R(CHANNEL_CASE_SELECT_CANCEL)),{NULL}},
    {"src/runtime/xl6_future_callback_queue.c",D(R(FUTURE_CALLBACK),R(EXECUTOR),R(CONTINUATION_FUTURE)),{"XRT_MODULE_FUTURE"}},
    {"src/runtime/xl6_ownership_domain_future_callback_queue.c",D(R(OWNERSHIP_CATALOG),R(FUTURE_CALLBACK_QUEUE)),{NULL}},
    {"src/runtime/xl6_channel_future.c",D(R(CHANNEL_CASE),R(TYPED_CHANNEL_SELECT),R(CHANNEL_FUTURE_CORE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_channel_future.c",D(R(OWNERSHIP_CATALOG),R(CHANNEL_FUTURE)),{NULL}},
    {"src/runtime/xl6_channel_future_language.c",D(R(CHANNEL_FUTURE),R(CHANNEL_CASE_ARRAY)),{NULL}},
    {"src/runtime/xl6_channel_future_core.c",D(R(EXECUTOR),R(FUTURE_PAYLOAD)),{"XRT_MODULE_CHANNEL_SUBSCRIBE","XRT_MODULE_FUTURE"}},
    {"src/runtime/xl6_channel_direct_future.c",D(R(CHANNEL_FUTURE_CORE),R(TYPED_CHANNEL),R(NATIVE_HANDLE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_channel_direct_future.c",D(R(OWNERSHIP_CATALOG),R(CHANNEL_DIRECT_FUTURE)),{NULL}},
    {"src/runtime/xl6_channel_direct_language.c",D(R(CHANNEL_DIRECT_FUTURE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_channel_direct_language.c",D(R(OWNERSHIP_CATALOG),R(CHANNEL_DIRECT_LANGUAGE)),{NULL}},
    {"src/runtime/xl6_deadline_service.c",D(R(FOREIGN_ERROR)),{"XRT_MODULE_MEMORY","XRT_MODULE_ERROR","XRT_MODULE_THREAD","XRT_MODULE_COND","XRT_MODULE_ATOMIC","XRT_MODULE_TIME"}},
    {"src/runtime/xl6_ownership_domain_deadline_service.c",D(R(OWNERSHIP_CATALOG),R(DEADLINE_SERVICE)),{NULL}},
    {"src/runtime/xl6_channel_future_deadline.c",D(R(CHANNEL_FUTURE_CORE),R(DEADLINE_SERVICE)),{NULL}},
    {"src/runtime/xl6_channel_direct_deadline.c",D(R(CHANNEL_DIRECT_FUTURE),R(CHANNEL_FUTURE_DEADLINE)),{NULL}},
    {"src/runtime/xl6_channel_select_deadline.c",D(R(CHANNEL_FUTURE),R(CHANNEL_FUTURE_DEADLINE)),{NULL}},
    {"src/runtime/xl6_channel_direct_deadline_language.c",D(R(CHANNEL_DIRECT_DEADLINE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_channel_direct_deadline_language.c",D(R(OWNERSHIP_CATALOG),R(CHANNEL_DIRECT_DEADLINE_LANGUAGE)),{NULL}},
    {"src/runtime/xl6_channel_select_deadline_language.c",D(R(CHANNEL_SELECT_DEADLINE),R(CHANNEL_CASE_ARRAY)),{NULL}},
    {"src/runtime/xl6_ownership_domain_channel_select_deadline_language.c",D(R(OWNERSHIP_CATALOG),R(CHANNEL_SELECT_DEADLINE_LANGUAGE)),{NULL}},
    {"src/runtime/xl6_task_group_start.c",D(R(TASK_GROUP),R(CLOSURE),R(FUTURE_VALUE),R(FOREIGN_ERROR)),{"XRT_MODULE_TASK_GROUP"}},
    {"src/runtime/xl6_ownership_domain_native_executor.c",D(R(OWNERSHIP_CATALOG)),{"XRT_MODULE_EXECUTOR"}},
    {"src/runtime/xl6_task_pool.c",NULL,0,{"XRT_MODULE_TASK_POOL","XRT_MODULE_ATOMIC"}},
    {"src/runtime/xl6_ownership_domain_task_pool.c",D(R(OWNERSHIP_CATALOG),R(TASK_POOL)),{NULL}},
    {"src/runtime/xl6_executor_jobs.c",D(R(DETACHED_WORK),R(FOREIGN_ERROR),R(EXECUTION_WORKER_EXIT)),{"XRT_MODULE_EXECUTOR"}},
    {"src/runtime/xl6_ownership_domain_executor_jobs.c",D(R(OWNERSHIP_CATALOG),R(EXECUTOR_JOBS),R(OWNERSHIP_DOMAIN_NATIVE_EXECUTOR)),{NULL}},
    {"src/runtime/xl6_execution_resource.c",D(R(FOREIGN_ERROR)),{"XRT_MODULE_EVENT","XRT_MODULE_ATOMIC","XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_execution_executor.c",D(R(EXECUTION_SOURCE),R(EXECUTOR_JOBS)),{NULL}},
    {"src/runtime/xl6_execution_pool.c",D(R(EXECUTION_SOURCE),R(TASK_POOL_EXIT)),{NULL}},
    {"src/runtime/xl6_ownership_domain_execution_source.c",D(R(OWNERSHIP_CATALOG),R(EXECUTION_SOURCE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_execution_executor.c",D(R(OWNERSHIP_CATALOG),R(EXECUTION_EXECUTOR)),{NULL}},
    {"src/runtime/xl6_ownership_domain_execution_pool.c",D(R(OWNERSHIP_CATALOG),R(EXECUTION_POOL)),{NULL}},
    {"src/runtime/xl6_execution_module.c",D(R(EXECUTION_SOURCE)),{NULL}},
    {"src/runtime/xl6_executor_language.c",D(R(EXECUTION_EXECUTOR),R(NATIVE_HANDLE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_executor_language.c",D(R(OWNERSHIP_CATALOG),R(EXECUTOR_LANGUAGE),R(OWNERSHIP_DOMAIN_EXECUTION_EXECUTOR)),{NULL}},
    {"src/runtime/xl6_executor_batch.c",D(R(EXECUTION_EXECUTOR),R(ARRAY_CALLABLE_ADMISSION)),{NULL}},
    {"src/runtime/xl6_task_submit_closure.c",D(R(TASK),R(TASK_RESULT_CLOSURE)),{NULL}},
    {"src/runtime/xl6_task_pool_language.c",D(R(EXECUTION_POOL),R(NATIVE_HANDLE)),{NULL}},
    {"src/runtime/xl6_task_pool_submit_language.c",D(R(EXECUTION_POOL),R(TASK_SUBMIT_CLOSURE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_task_pool_language.c",D(R(OWNERSHIP_CATALOG),R(TASK_POOL_LANGUAGE),R(OWNERSHIP_DOMAIN_EXECUTION_POOL)),{NULL}},
    {"src/runtime/xl6_execution_failures.c",D(R(FOREIGN_ERROR)),{"XRT_MODULE_MEMORY","XRT_MODULE_ATOMIC","XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_execution_worker_exit.c",D(R(EXECUTION_FAILURES),R(THREAD_EXIT),R(FOREIGN_ERROR)),{NULL}},
    {"src/runtime/xl6_task_pool_exit.c",D(R(TASK_POOL),R(EXECUTION_WORKER_EXIT)),{NULL}},
    {"src/runtime/xl6_ownership_domain_task_pool_exit.c",D(R(OWNERSHIP_CATALOG),R(TASK_POOL_EXIT)),{NULL}},
    {"src/runtime/xl6_ownership_domain_execution_failures.c",D(R(OWNERSHIP_CATALOG),R(EXECUTION_FAILURES)),{NULL}},
    {"src/runtime/xl6_net_address.c",D(R(NATIVE_HANDLE)),{"XRT_MODULE_NET","XRT_MODULE_STRING","XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_net_resolve_address.c",D(R(NET_ADDRESS)),{"XRT_MODULE_NET_DNS","XRT_MODULE_STRING","XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_ownership_domain_net_address.c",D(R(OWNERSHIP_CATALOG),R(NET_ADDRESS)),{NULL}},
    {"src/runtime/xl6_net_interfaces.c",D(R(NATIVE_HANDLE),R(NET_ADDRESS)),{"XRT_MODULE_NET_INTERFACE","XRT_MODULE_STRING","XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_net_interfaces_text.c",D(R(NET_INTERFACES)),{"XRT_MODULE_UNICODE_TEXT","XRT_MODULE_STRING"}},
    {"src/runtime/xl6_net_interface_query.c",NULL,0,{"XRT_MODULE_NET_INTERFACE","XRT_MODULE_UNICODE_TEXT","XRT_MODULE_STRING"}},
    {"src/runtime/xl6_net_interface_local.c",D(R(NET_ADDRESS)),{"XRT_MODULE_NET_INTERFACE_TEXT","XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_ownership_domain_net_interfaces.c",D(R(OWNERSHIP_CATALOG),R(NET_INTERFACES)),{NULL}},
    {"src/runtime/xl6_ownership_domain_net_interface.c",D(R(OWNERSHIP_CATALOG),R(NET_INTERFACES)),{NULL}},
    {"src/runtime/xl6_ownership_domain_net_interface_address.c",D(R(OWNERSHIP_CATALOG),R(NET_INTERFACES)),{NULL}},
    {"src/runtime/xl6_net_backends.c",NULL,0,{"XRT_MODULE_NET_PORT_SELECT","XRT_MODULE_NET_PORT_IOCP",
        "XRT_MODULE_NET_PORT_EPOLL","XRT_MODULE_NET_PORT_URING","XRT_MODULE_NET_PORT_KQUEUE"}},
    {"src/runtime/xl6_net_backend_text.c",D(R(NET_BACKENDS)),{"XRT_MODULE_STRING"}},
    {"src/runtime/xl6_detached_work.c",D(R(CLOSURE),R(FOREIGN_ERROR),R(EXECUTION_FAILURES)),
        {"XRT_MODULE_MEMORY","XRT_MODULE_ATOMIC","XRT_MODULE_THREAD","XRUNTIME_MODULE_RUNTIME_CALL","XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE"}},
    {"src/runtime/xl6_ownership_domain_detached_work.c",D(R(OWNERSHIP_CATALOG),R(DETACHED_WORK)),{NULL}},
    {"src/runtime/xl6_execution_net_engine.c",D(R(EXECUTION_SOURCE),R(DETACHED_WORK),R(EXECUTION_WORKER_EXIT),R(NET_BACKENDS)),{"XRT_MODULE_NET_ENGINE"}},
    {"src/runtime/xl6_ownership_domain_execution_net_engine.c",D(R(OWNERSHIP_CATALOG),R(EXECUTION_NET_ENGINE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_net_engine_native.c",D(R(OWNERSHIP_CATALOG),R(EXECUTION_NET_ENGINE),R(OWNERSHIP_DOMAIN_NET_BUFFER_NATIVE)),{NULL}},
    {"src/runtime/xl6_execution_net_file.c",D(R(EXECUTION_SOURCE),R(EXECUTION_NET_ENGINE),R(OWNERSHIP_DOMAIN_NET_FILE_NATIVE)),{"XRT_MODULE_NET_FILE_FUTURE"}},
    {"src/runtime/xl6_ownership_domain_execution_net_file.c",D(R(OWNERSHIP_CATALOG),R(EXECUTION_NET_FILE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_net_file_native.c",D(R(OWNERSHIP_CATALOG),R(OWNERSHIP_DOMAIN_NATIVE_FUTURE),R(OWNERSHIP_DOMAIN_NET_ENGINE_NATIVE)),{"XRT_MODULE_NET_FILE_FUTURE"}},
    {"src/runtime/xl6_net_file_language.c",D(R(EXECUTION_NET_FILE),R(NATIVE_HANDLE)),{"XRT_MODULE_STRING"}},
    {"src/runtime/xl6_net_file_future.c",D(R(NET_FILE_LANGUAGE),R(FUTURE_CALLBACK_QUEUE),R(FUTURE_PAYLOAD)),{"XRT_MODULE_VALUE","XRT_MODULE_BUFFER"}},
    {"src/runtime/xl6_net_file_write.c",D(R(NET_FILE_FUTURE)),{NULL}},
    {"src/runtime/xl6_net_file_bytes.c",D(R(NET_FILE_FUTURE)),{"XRT_MODULE_ONCE"}},
    {"src/runtime/xl6_ownership_domain_net_file_language.c",D(R(OWNERSHIP_CATALOG),R(NET_FILE_LANGUAGE),R(OWNERSHIP_DOMAIN_EXECUTION_NET_FILE),R(OWNERSHIP_DOMAIN_NET_FILE_NATIVE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_net_file_bytes.c",D(R(OWNERSHIP_CATALOG),R(NET_FILE_BYTES)),{NULL}},
    {"src/runtime/xl6_net_engine_config.c",D(R(NATIVE_HANDLE),R(FOREIGN_ERROR)),{"XRT_MODULE_NET_ENGINE"}},
    {"src/runtime/xl6_net_engine_language.c",D(R(EXECUTION_NET_ENGINE),R(NATIVE_HANDLE)),{NULL}},
    {"src/runtime/xl6_net_engine_stats.c",D(R(NATIVE_HANDLE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_net_engine_config.c",D(R(OWNERSHIP_CATALOG),R(NET_ENGINE_CONFIG)),{NULL}},
    {"src/runtime/xl6_ownership_domain_net_engine_language.c",D(R(OWNERSHIP_CATALOG),R(NET_ENGINE_LANGUAGE),R(OWNERSHIP_DOMAIN_EXECUTION_NET_ENGINE),R(OWNERSHIP_DOMAIN_NET_ENGINE_NATIVE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_net_engine_stats.c",D(R(OWNERSHIP_CATALOG),R(NET_ENGINE_STATS)),{NULL}},
    {"src/runtime/xl6_net_socket_owner.c",D(R(FOREIGN_ERROR)),{"XRT_MODULE_NET_SOCKET","XRT_MODULE_MEMORY","XRT_MODULE_ATOMIC","XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_net_socket_language.c",D(R(NET_SOCKET_OWNER),R(NATIVE_HANDLE),R(NET_ADDRESS)),{NULL}},
    {"src/runtime/xl6_net_socket_io.c",D(R(NET_SOCKET_LANGUAGE)),{"XRT_MODULE_BUFFER","XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_ownership_domain_net_socket_owner.c",D(R(OWNERSHIP_CATALOG),R(NET_SOCKET_OWNER)),{NULL}},
    {"src/runtime/xl6_ownership_domain_net_socket.c",D(R(OWNERSHIP_CATALOG),R(NET_SOCKET_LANGUAGE)),{NULL}},
    {"src/runtime/xl6_net_datagram_values.c",D(R(NATIVE_HANDLE),R(FOREIGN_ERROR),R(NET_ADDRESS)),{"XRT_MODULE_NET","XRT_MODULE_MEMORY"}},
    {"src/runtime/xl6_net_socket_datagram_control.c",D(R(NET_SOCKET_LANGUAGE)),{NULL}},
    {"src/runtime/xl6_net_socket_datagram_io.c",D(R(NET_SOCKET_LANGUAGE),R(NET_DATAGRAM_VALUES)),{"XRT_MODULE_BUFFER","XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_ownership_domain_net_datagram_info.c",D(R(OWNERSHIP_CATALOG),R(NET_DATAGRAM_VALUES)),{NULL}},
    {"src/runtime/xl6_ownership_domain_net_datagram_control.c",D(R(OWNERSHIP_CATALOG),R(NET_DATAGRAM_VALUES)),{NULL}},
    {"src/runtime/xl6_net_datagram_error_values.c",D(R(NATIVE_HANDLE),R(FOREIGN_ERROR),R(NET_ADDRESS)),{"XRT_MODULE_NET","XRT_MODULE_MEMORY"}},
    {"src/runtime/xl6_net_socket_datagram_error_io.c",D(R(NET_SOCKET_LANGUAGE),R(NET_DATAGRAM_ERROR_VALUES)),{"XRT_MODULE_BUFFER"}},
    {"src/runtime/xl6_ownership_domain_net_datagram_error_info.c",D(R(OWNERSHIP_CATALOG),R(NET_DATAGRAM_ERROR_VALUES)),{NULL}},
    {"src/runtime/xl6_value_numeric.c",D(R(ERROR_TEXT)),{"XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_value_add.c",D(R(VALUE_NUMERIC),R(ARRAY_ALGEBRA),R(STRING_VALUE)),{NULL}},
    {"src/runtime/xl6_net_socket_vector.c",D(R(NET_SOCKET_LANGUAGE),R(NET_VECTOR_INPUT)),{NULL}},
    {"src/runtime/xl6_net_socket_vector_text.c",D(R(NET_VECTOR_CORE),R(ARRAY_PRIMITIVE_ADMISSION)),{NULL}},
    {"src/runtime/xl6_net_socket_vector_bytes.c",D(R(NET_VECTOR_CORE),R(ARRAY_REFERENCE_ADMISSION)),{NULL}},
    {"src/runtime/xl6_net_socket_vector_buffer.c",D(R(NET_VECTOR_CORE),R(NET_VECTOR_BUFFER_INPUT)),{NULL}},
    {"src/runtime/xl6_net_socket_vector_receive.c",D(R(NET_VECTOR_BUFFER),R(NET_DATAGRAM_VALUES)),{NULL}},
    {"src/runtime/xl6_net_datagram_packet.c",D(R(BUFFER_CORE),R(NATIVE_HANDLE),R(NET_ADDRESS),R(FOREIGN_ERROR)),{NULL}},
    {"src/runtime/xl6_net_datagram_packet_info.c",D(R(NET_DATAGRAM_PACKET),R(NET_DATAGRAM_VALUES)),{NULL}},
    {"src/runtime/xl6_net_batch_progress.c",D(R(NATIVE_HANDLE),R(FOREIGN_ERROR)),{"XRT_MODULE_MEMORY"}},
    {"src/runtime/xl6_net_socket_batch.c",D(R(NET_SOCKET_LANGUAGE),R(NET_DATAGRAM_PACKET),R(NET_BATCH_PROGRESS),R(SEQUENCE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_net_datagram_packet.c",D(R(OWNERSHIP_CATALOG),R(NET_DATAGRAM_PACKET)),{NULL}},
    {"src/runtime/xl6_ownership_domain_net_batch_progress.c",D(R(OWNERSHIP_CATALOG),R(NET_BATCH_PROGRESS)),{NULL}},
    {"src/runtime/xl6_net_address_list.c",D(R(NATIVE_HANDLE),R(NET_ADDRESS),R(FOREIGN_ERROR)),{"XRT_MODULE_NET_DNS"}},
    {"src/runtime/xl6_net_address_list_array.c",D(R(NET_ADDRESS_LIST),R(SEQUENCE)),{NULL}},
    {"src/runtime/xl6_net_address_list_resolve.c",D(R(NET_ADDRESS_LIST)),{"XRT_MODULE_STRING","XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_net_resolver_config.c",D(R(NATIVE_HANDLE),R(FOREIGN_ERROR)),{"XRT_MODULE_NET_RESOLVER"}},
    {"src/runtime/xl6_net_resolver_stats.c",D(R(NATIVE_HANDLE),R(FOREIGN_ERROR)),{"XRT_MODULE_MEMORY"}},
    {"src/runtime/xl6_execution_net_resolver.c",D(R(EXECUTION_SOURCE),R(OWNERSHIP_DOMAIN_NET_RESOLVER_NATIVE)),{"XRT_MODULE_NET_RESOLVER_FUTURE"}},
    {"src/runtime/xl6_net_resolver_language.c",D(R(EXECUTION_NET_RESOLVER),R(NATIVE_HANDLE),R(FOREIGN_ERROR)),{"XRT_MODULE_STRING","XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_net_resolve_operation.c",D(R(NATIVE_HANDLE),R(FUTURE_CALLBACK),R(FUTURE_PAYLOAD),R(OWNERSHIP_DOMAIN_NET_RESOLVER_NATIVE)),{"XRT_MODULE_NET_RESOLVER_FUTURE"}},
    {"src/runtime/xl6_ownership_domain_net_address_list.c",D(R(OWNERSHIP_CATALOG),R(NET_ADDRESS_LIST)),{NULL}},
    {"src/runtime/xl6_ownership_domain_net_resolver_config.c",D(R(OWNERSHIP_CATALOG),R(NET_RESOLVER_CONFIG)),{NULL}},
    {"src/runtime/xl6_ownership_domain_net_resolver_stats.c",D(R(OWNERSHIP_CATALOG),R(NET_RESOLVER_STATS)),{NULL}},
    {"src/runtime/xl6_ownership_domain_net_resolver_language.c",D(R(OWNERSHIP_CATALOG),R(NET_RESOLVER_LANGUAGE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_net_resolve_operation.c",D(R(OWNERSHIP_CATALOG),R(NET_RESOLVE_OPERATION)),{NULL}},
    {"src/runtime/xl6_ownership_domain_execution_net_resolver.c",D(R(OWNERSHIP_CATALOG),R(EXECUTION_NET_RESOLVER)),{NULL}},
    {"src/runtime/xl6_ownership_domain_net_resolver_native.c",D(R(OWNERSHIP_CATALOG)),{"XRT_MODULE_NET_RESOLVER_FUTURE"}},
    {"src/runtime/xl6_net_buffer.c",D(R(NATIVE_HANDLE),R(FOREIGN_ERROR)),{"XRT_MODULE_NET_BUFFER","XRT_MODULE_ATOMIC"}},
    {"src/runtime/xl6_net_buffer_value.c",D(R(NET_BUFFER_CORE)),{"XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_net_buffer_buffer.c",D(R(NET_BUFFER_CORE),R(BUFFER_CORE)),{NULL}},
    {"src/runtime/xl6_net_frame_result.c",D(R(NATIVE_HANDLE),R(FOREIGN_ERROR)),{"XRT_MODULE_NET_FRAME","XRT_MODULE_ATOMIC"}},
    {"src/runtime/xl6_net_frame_value.c",D(R(NET_FRAME_RESULT)),{"XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_net_frame_buffer.c",D(R(NET_FRAME_RESULT),R(BUFFER_CORE)),{NULL}},
    {"src/runtime/xl6_net_frame_text.c",D(R(NET_FRAME_RESULT)),{"XRT_MODULE_STRING","XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_net_frame_line.c",D(R(NET_BUFFER_CORE),R(NET_FRAME_RESULT)),{"XRT_MODULE_NET_FRAME_LINE","XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_net_frame_line_value.c",D(R(NET_FRAME_LINE),R(NET_BUFFER_VALUE)),{NULL}},
    {"src/runtime/xl6_net_frame_line_buffer.c",D(R(NET_FRAME_LINE),R(NET_BUFFER_BUFFER)),{NULL}},
    {"src/runtime/xl6_net_frame_length.c",D(R(NET_BUFFER_CORE),R(NET_FRAME_RESULT)),{"XRT_MODULE_NET_FRAME_LENGTH"}},
    {"src/runtime/xl6_net_frame_length_value.c",D(R(NET_FRAME_LENGTH),R(NET_BUFFER_VALUE)),{NULL}},
    {"src/runtime/xl6_net_frame_length_buffer.c",D(R(NET_FRAME_LENGTH),R(NET_BUFFER_BUFFER)),{NULL}},
    {"src/runtime/xl6_ownership_domain_net_buffer.c",D(R(OWNERSHIP_CATALOG),R(NET_BUFFER_CORE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_net_frame_result.c",D(R(OWNERSHIP_CATALOG),R(NET_FRAME_RESULT)),{NULL}},
    {"src/runtime/xl6_ownership_domain_net_line_framer.c",D(R(OWNERSHIP_CATALOG),R(NET_FRAME_LINE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_net_length_framer.c",D(R(OWNERSHIP_CATALOG),R(NET_FRAME_LENGTH)),{NULL}},
    {"src/runtime/xl6_net_proxy.c",D(R(NATIVE_HANDLE),R(FOREIGN_ERROR)),{"XRT_MODULE_NET_PROXY","XRT_MODULE_VALUE","XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_net_proxy_value.c",D(R(NET_PROXY)),{NULL}},
    {"src/runtime/xl6_net_proxy_text.c",D(R(NET_PROXY)),{"XRT_MODULE_STRING"}},
    {"src/runtime/xl6_net_proxy_handshake.c",D(R(NET_PROXY),R(NET_BUFFER_CORE)),{"XRT_MODULE_NET_PROXY_HANDSHAKE","XRT_MODULE_NET_PROXY_SOCKS5","XRT_MODULE_NET_PROXY_HTTP_CONNECT"}},
    {"src/runtime/xl6_net_proxy_handshake_value.c",D(R(NET_PROXY_HANDSHAKE),R(NET_BUFFER_VALUE)),{NULL}},
    {"src/runtime/xl6_net_proxy_handshake_buffer.c",D(R(NET_PROXY_HANDSHAKE),R(NET_BUFFER_BUFFER)),{NULL}},
    {"src/runtime/xl6_net_proxy_handshake_text.c",D(R(NET_PROXY_HANDSHAKE)),{"XRT_MODULE_STRING"}},
    {"src/runtime/xl6_net_proxy_handshake_address.c",D(R(NET_PROXY_HANDSHAKE),R(NET_ADDRESS)),{NULL}},
    {"src/runtime/xl6_ownership_domain_net_proxy.c",D(R(OWNERSHIP_CATALOG),R(NET_PROXY)),{NULL}},
    {"src/runtime/xl6_ownership_domain_net_proxy_handshake.c",D(R(OWNERSHIP_CATALOG),R(NET_PROXY_HANDSHAKE)),{NULL}},
    {"src/runtime/xl6_tcp_stream_options.c",D(R(NATIVE_HANDLE),R(FOREIGN_ERROR)),{"XRT_MODULE_NET_TCP"}},
    {"src/runtime/xl6_tcp_dial_options.c",D(R(TCP_STREAM_OPTIONS)),{"XRT_MODULE_NET_TCP_DIAL"}},
    {"src/runtime/xl6_tcp_listen_options.c",D(R(TCP_STREAM_OPTIONS)),{"XRT_MODULE_NET_TCP"}},
    {"src/runtime/xl6_tcp_listen_options_address.c",D(R(TCP_LISTEN_OPTIONS),R(NET_ADDRESS)),{NULL}},
    {"src/runtime/xl6_ownership_domain_tcp_stream_options.c",D(R(OWNERSHIP_CATALOG),R(TCP_STREAM_OPTIONS)),{NULL}},
    {"src/runtime/xl6_ownership_domain_tcp_dial_options.c",D(R(OWNERSHIP_CATALOG),R(TCP_DIAL_OPTIONS)),{NULL}},
    {"src/runtime/xl6_ownership_domain_tcp_listen_options.c",D(R(OWNERSHIP_CATALOG),R(TCP_LISTEN_OPTIONS)),{NULL}},
    {"src/runtime/xl6_ownership_domain_net_buffer_native.c",D(R(OWNERSHIP_CATALOG)),{"XRT_MODULE_NET_BUFFER"}},
    {"src/runtime/xl6_ownership_domain_net_tcp_native.c",D(R(OWNERSHIP_CATALOG),R(OWNERSHIP_DOMAIN_NET_TCP_BASE_NATIVE),R(OWNERSHIP_DOMAIN_NATIVE_FUTURE)),{"XRT_MODULE_NET_TCP_FUTURE"}},
    {"src/runtime/xl6_execution_tcp.c",D(R(EXECUTION_SOURCE),R(EXECUTION_NET_ENGINE),R(OWNERSHIP_DOMAIN_NET_TCP_NATIVE)),{"XRT_MODULE_NET_TCP"}},
    {"src/runtime/xl6_ownership_domain_execution_tcp.c",D(R(OWNERSHIP_CATALOG),R(EXECUTION_TCP),R(OWNERSHIP_DOMAIN_NET_TCP_NATIVE)),{NULL}},
    {"src/runtime/xl6_tcp_language.c",D(R(EXECUTION_TCP),R(NATIVE_HANDLE),R(FOREIGN_ERROR),R(NET_ADDRESS)),{"XRT_MODULE_VALUE","XRT_MODULE_BUFFER"}},
    {"src/runtime/xl6_tcp_future.c",D(R(TCP_LANGUAGE),R(FUTURE_CALLBACK_QUEUE),R(FUTURE_PAYLOAD)),{"XRT_MODULE_NET_TCP_FUTURE"}},
    {"src/runtime/xl6_tcp_stats.c",D(R(EXECUTION_TCP),R(NATIVE_HANDLE),R(FOREIGN_ERROR)),{NULL}},
    {"src/runtime/xl6_tcp_sync.c",D(R(TCP_LANGUAGE)),{"XRT_MODULE_NET_TCP_SYNC"}},
    {"src/runtime/xl6_ownership_domain_tcp_language.c",D(R(OWNERSHIP_CATALOG),R(TCP_LANGUAGE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_tcp_stats.c",D(R(OWNERSHIP_CATALOG),R(TCP_STATS)),{NULL}},
    {"src/runtime/xl6_ownership_domain_net_dial_native.c",D(R(OWNERSHIP_CATALOG),R(OWNERSHIP_DOMAIN_NET_TCP_NATIVE),R(OWNERSHIP_DOMAIN_NET_RESOLVER_NATIVE)),{"XRT_MODULE_NET_TCP_DIAL_FUTURE"}},
    {"src/runtime/xl6_execution_tcp_dial.c",D(R(EXECUTION_TCP_DIAL_COMMON),R(OWNERSHIP_DOMAIN_NET_DIAL_NATIVE)),{"XRT_MODULE_NET_TCP_DIAL_FUTURE"}},
    {"src/runtime/xl6_tcp_dial.c",D(R(EXECUTION_TCP_DIAL_COMMON),R(TCP_LANGUAGE)),{NULL}},
    {"src/runtime/xl6_tcp_dial_future.c",D(R(TCP_DIAL),R(EXECUTION_TCP_DIAL),R(TCP_FUTURE)),{"XRT_MODULE_STRING","XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_tcp_dial_sync.c",D(R(TCP_DIAL),R(EXECUTION_TCP_DIAL),R(TCP_DIAL_SYNC_COMMON)),{"XRT_MODULE_STRING","XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_future_callback_interrupt.c",D(R(FUTURE_CALLBACK_QUEUE),R(DEADLINE_SERVICE)),{"XRT_MODULE_TIME"}},
    {"src/runtime/xl6_ownership_domain_future_callback_interrupt.c",D(R(OWNERSHIP_CATALOG),R(FUTURE_CALLBACK_INTERRUPT)),{NULL}},
    {"src/runtime/xl6_tcp_dial_interrupt_future.c",D(R(TCP_DIAL_FUTURE),R(FUTURE_CALLBACK_INTERRUPT)),{NULL}},
    {"src/runtime/xl6_tcp_dial_interrupt_sync.c",D(R(TCP_DIAL_SYNC)),{NULL}},
    {"src/runtime/xl6_execution_tcp_dial_common.c",D(R(EXECUTION_TCP),R(EXECUTION_NET_RESOLVER)),{NULL}},
    {"src/runtime/xl6_tcp_dial_sync_common.c",D(R(EXECUTION_TCP_DIAL_COMMON),R(TCP_LANGUAGE),R(FUTURE_PAYLOAD)),{NULL}},
    /* NetProxy chooses the protocol at runtime. Dial needs both approved
     * implementations, unlike the immutable configuration-only component. */
    {"src/runtime/xl6_ownership_domain_net_proxy_dial_native.c",D(R(OWNERSHIP_CATALOG),R(OWNERSHIP_DOMAIN_NET_PROXY),R(OWNERSHIP_DOMAIN_NET_TCP_NATIVE),R(OWNERSHIP_DOMAIN_NET_RESOLVER_NATIVE)),{"XRT_MODULE_NET_PROXY_DIAL_FUTURE","XRT_MODULE_NET_PROXY_SOCKS5","XRT_MODULE_NET_PROXY_HTTP_CONNECT"}},
    {"src/runtime/xl6_execution_tcp_proxy_dial.c",D(R(EXECUTION_TCP_DIAL_COMMON),R(OWNERSHIP_DOMAIN_NET_PROXY_DIAL_NATIVE)),{"XRT_MODULE_NET_PROXY_DIAL_FUTURE"}},
    {"src/runtime/xl6_tcp_proxy_dial_options.c",D(R(TCP_DIAL_OPTIONS)),{"XRT_MODULE_NET_PROXY_DIAL"}},
    {"src/runtime/xl6_ownership_domain_tcp_proxy_dial_options.c",D(R(OWNERSHIP_CATALOG),R(TCP_PROXY_DIAL_OPTIONS)),{NULL}},
    {"src/runtime/xl6_tcp_proxy_dial_sync.c",D(R(EXECUTION_TCP_PROXY_DIAL),R(TCP_DIAL_SYNC_COMMON)),{"XRT_MODULE_STRING","XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_tcp_proxy_dial_future.c",D(R(EXECUTION_TCP_PROXY_DIAL),R(TCP_FUTURE)),{"XRT_MODULE_STRING","XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_tcp_proxy_dial_interrupt_future.c",D(R(TCP_PROXY_DIAL_FUTURE),R(FUTURE_CALLBACK_INTERRUPT)),{NULL}},
    {"src/runtime/xl6_net_vector_input.c",D(R(SEQUENCE)),{NULL}},
    {"src/runtime/xl6_tcp_vector.c",D(R(TCP_LANGUAGE),R(NET_VECTOR_INPUT)),{NULL}},
    {"src/runtime/xl6_tcp_vector_text.c",D(R(TCP_VECTOR),R(ARRAY_PRIMITIVE_ADMISSION)),{NULL}},
    {"src/runtime/xl6_tcp_vector_bytes.c",D(R(TCP_VECTOR),R(ARRAY_REFERENCE_ADMISSION)),{NULL}},
    {"src/runtime/xl6_tcp_vector_buffer.c",D(R(TCP_VECTOR),R(NET_VECTOR_BUFFER_INPUT)),{NULL}},
    {"src/runtime/xl6_tcp_net_buffer.c",D(R(TCP_LANGUAGE),R(NET_BUFFER_CORE)),{NULL}},
    {"src/runtime/xl6_tcp_file.c",D(R(TCP_LANGUAGE),R(FS_FILE_OWNER)),{"XRT_MODULE_NET_TCP_FILE"}},
    {"src/runtime/xl6_tcp_receive_into.c",D(R(TCP_LANGUAGE),R(NET_BUFFER_CORE)),{"XRT_MODULE_NET_TCP_SYNC"}},
    {"src/runtime/xl6_tcp_receive_into_future.c",D(R(TCP_RECEIVE_INTO),R(FUTURE_CALLBACK_QUEUE),R(FUTURE_PAYLOAD)),{NULL}},
    {"src/runtime/xl6_net_vector_buffer_input.c",D(R(NET_VECTOR_INPUT),R(BUFFER_CORE)),{NULL}},
    {"src/runtime/xl6_tcp_receive_into_net_buffer_future.c",D(R(TCP_RECEIVE_INTO_FUTURE)),{NULL}},
    {"src/runtime/xl6_tcp_receive_into_line_future.c",D(R(TCP_RECEIVE_INTO_FUTURE),R(NET_FRAME_LINE)),{NULL}},
    {"src/runtime/xl6_tcp_receive_into_length_future.c",D(R(TCP_RECEIVE_INTO_FUTURE),R(NET_FRAME_LENGTH)),{NULL}},
    {"src/runtime/xl6_ownership_domain_net_tcp_base_native.c",D(R(OWNERSHIP_CATALOG),R(OWNERSHIP_DOMAIN_NET_ENGINE_NATIVE)),{"XRT_MODULE_NET_TCP"}},
    {"src/runtime/xl6_ownership_domain_net_server_native.c",D(R(OWNERSHIP_CATALOG),R(OWNERSHIP_DOMAIN_NET_TCP_BASE_NATIVE)),{"XRT_MODULE_NET_TCP_SERVER"}},
    {"src/runtime/xl6_ownership_domain_net_server_future_native.c",D(R(OWNERSHIP_CATALOG),R(OWNERSHIP_DOMAIN_NET_SERVER_NATIVE),R(OWNERSHIP_DOMAIN_NATIVE_FUTURE)),{"XRT_MODULE_NET_TCP_SERVER_FUTURE"}},
    {"src/runtime/xl6_execution_tcp_server.c",D(R(EXECUTION_SOURCE),R(EXECUTION_NET_ENGINE),R(OWNERSHIP_DOMAIN_NET_SERVER_NATIVE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_execution_tcp_server.c",D(R(OWNERSHIP_CATALOG),R(EXECUTION_TCP_SERVER)),{NULL}},
    {"src/runtime/xl6_execution_tcp_server_accept.c",D(R(EXECUTION_TCP_SERVER),R(EXECUTION_TCP),R(OWNERSHIP_DOMAIN_NET_SERVER_FUTURE_NATIVE)),{NULL}},
    {"src/runtime/xl6_tcp_server_options.c",D(R(NATIVE_HANDLE),R(FOREIGN_ERROR),R(TCP_LISTEN_OPTIONS)),{"XRT_MODULE_NET_TCP_SERVER"}},
    {"src/runtime/xl6_ownership_domain_tcp_server_options.c",D(R(OWNERSHIP_CATALOG),R(TCP_SERVER_OPTIONS)),{NULL}},
    {"src/runtime/xl6_tcp_server_language.c",D(R(EXECUTION_TCP_SERVER),R(NATIVE_HANDLE),R(FOREIGN_ERROR),R(NET_ADDRESS)),{NULL}},
    {"src/runtime/xl6_ownership_domain_tcp_server.c",D(R(OWNERSHIP_CATALOG),R(TCP_SERVER_LANGUAGE)),{NULL}},
    {"src/runtime/xl6_tcp_server_accept.c",D(R(TCP_SERVER_LANGUAGE),R(EXECUTION_TCP_SERVER_ACCEPT)),{NULL}},
    {"src/runtime/xl6_tcp_server_future.c",D(R(TCP_SERVER_LANGUAGE),R(EXECUTION_TCP_SERVER_ACCEPT),R(TCP_FUTURE)),{NULL}},
    {"src/runtime/xl6_tcp_server_interrupt_future.c",D(R(TCP_SERVER_FUTURE),R(FUTURE_CALLBACK_INTERRUPT)),{NULL}},
    {"src/runtime/xl6_tcp_server_sync.c",D(R(TCP_SERVER_LANGUAGE),R(EXECUTION_TCP),R(OWNERSHIP_DOMAIN_NET_SERVER_FUTURE_NATIVE)),{"XRT_MODULE_NET_TCP_SERVER_SYNC"}},
    {"src/runtime/xl6_tcp_server_stats.c",D(R(TCP_SERVER_LANGUAGE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_tcp_server_stats.c",D(R(OWNERSHIP_CATALOG),R(TCP_SERVER_STATS)),{NULL}},
    {"src/runtime/xl6_tcp_server_poll.c",D(R(TCP_SERVER_LANGUAGE),R(EXECUTION_TCP)),{NULL}},
    {"src/runtime/xl6_net_bytes.c",D(R(NATIVE_HANDLE),R(FOREIGN_ERROR),R(OWNERSHIP_DOMAIN_NET_BUFFER_NATIVE)),{"XRT_MODULE_NET_BUFFER","XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_net_bytes_buffer.c",D(R(NET_BYTES),R(BUFFER_CORE)),{"XRT_MODULE_BUFFER"}},
    {"src/runtime/xl6_ownership_domain_net_bytes.c",D(R(OWNERSHIP_CATALOG),R(NET_BYTES)),{NULL}},
    {"src/runtime/xl6_tcp_send_ref.c",D(R(TCP_LANGUAGE),R(NET_BYTES)),{NULL}},
    {"src/runtime/xl6_tcp_send_move.c",D(R(TCP_LANGUAGE),R(BUFFER_CORE)),{NULL}},
    {"src/runtime/xl6_tcp_recv_owned_sync.c",D(R(TCP_LANGUAGE),R(NET_BYTES)),{"XRT_MODULE_NET_TCP_SYNC"}},
    {"src/runtime/xl6_tcp_recv_owned_future.c",D(R(TCP_FUTURE),R(NET_BYTES)),{NULL}},
    {"src/runtime/xl6_tcp_recv_owned_interrupt.c",D(R(TCP_RECV_OWNED_FUTURE),R(FUTURE_CALLBACK_INTERRUPT)),{NULL}},
    {"src/runtime/xl6_tcp_send_refs.c",D(R(TCP_LANGUAGE),R(NET_BYTES),R(SEQUENCE)),{NULL}},
    {"src/runtime/xl6_net_buffer_ref.c",D(R(NET_BUFFER_CORE),R(NET_BYTES)),{NULL}},
    {"src/runtime/xl6_tcp_send_chain_move.c",D(R(TCP_LANGUAGE),R(NET_BUFFER_CORE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_net_udp_base_native.c",D(R(OWNERSHIP_CATALOG),R(OWNERSHIP_DOMAIN_NET_ENGINE_NATIVE)),{"XRT_MODULE_NET_UDP"}},
    {"src/runtime/xl6_ownership_domain_net_udp_native.c",D(R(OWNERSHIP_CATALOG),R(OWNERSHIP_DOMAIN_NET_UDP_BASE_NATIVE),R(OWNERSHIP_DOMAIN_NATIVE_FUTURE)),{"XRT_MODULE_NET_UDP_FUTURE"}},
    {"src/runtime/xl6_execution_udp.c",D(R(EXECUTION_SOURCE),R(EXECUTION_NET_ENGINE),R(OWNERSHIP_DOMAIN_NET_UDP_BASE_NATIVE)),{"XRT_MODULE_NET_UDP"}},
    {"src/runtime/xl6_ownership_domain_execution_udp.c",D(R(OWNERSHIP_CATALOG),R(EXECUTION_UDP)),{NULL}},
    {"src/runtime/xl6_execution_udp_data.c",D(R(EXECUTION_UDP),R(OWNERSHIP_DOMAIN_NET_BUFFER_NATIVE)),{NULL}},
    {"src/runtime/xl6_execution_udp_future.c",D(R(EXECUTION_UDP),R(OWNERSHIP_DOMAIN_NET_UDP_NATIVE)),{NULL}},
    {"src/runtime/xl6_udp_config.c",D(R(NATIVE_HANDLE),R(FOREIGN_ERROR)),{"XRT_MODULE_NET_UDP"}},
    {"src/runtime/xl6_ownership_domain_udp_config.c",D(R(OWNERSHIP_CATALOG),R(UDP_CONFIG)),{NULL}},
    {"src/runtime/xl6_udp_values.c",D(R(NATIVE_HANDLE),R(FOREIGN_ERROR)),{"XRT_MODULE_NET_UDP","XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_udp_packet_info.c",D(R(UDP_VALUES),R(NET_ADDRESS),R(NET_DATAGRAM_VALUES),R(NET_DATAGRAM_ERROR_VALUES)),{NULL}},
    {"src/runtime/xl6_udp_batch.c",D(R(UDP_VALUES)),{"XRT_MODULE_NET_UDP_FUTURE"}},
    {"src/runtime/xl6_udp_language.c",D(R(EXECUTION_UDP),R(NATIVE_HANDLE),R(FOREIGN_ERROR)),{NULL}},
    {"src/runtime/xl6_udp_address.c",D(R(UDP_LANGUAGE),R(NET_ADDRESS)),{NULL}},
    {"src/runtime/xl6_udp_stats.c",D(R(EXECUTION_UDP),R(NATIVE_HANDLE),R(FOREIGN_ERROR)),{NULL}},
    {"src/runtime/xl6_udp_send.c",D(R(UDP_LANGUAGE),R(EXECUTION_UDP_DATA)),{"XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_udp_send_buffer.c",D(R(UDP_SEND),R(BUFFER_CORE)),{NULL}},
    {"src/runtime/xl6_udp_send_ref.c",D(R(UDP_LANGUAGE),R(EXECUTION_UDP_DATA),R(NET_BYTES),R(UDP_VALUES)),{NULL}},
    {"src/runtime/xl6_udp_poll.c",D(R(UDP_LANGUAGE),R(EXECUTION_UDP_DATA),R(UDP_VALUES)),{NULL}},
    {"src/runtime/xl6_udp_sync.c",D(R(UDP_LANGUAGE),R(UDP_BATCH),R(OWNERSHIP_DOMAIN_NET_UDP_NATIVE)),{"XRT_MODULE_NET_UDP_SYNC"}},
    {"src/runtime/xl6_ownership_domain_udp_values.c",D(R(OWNERSHIP_CATALOG),R(UDP_VALUES)),{NULL}},
    {"src/runtime/xl6_ownership_domain_udp_batch.c",D(R(OWNERSHIP_CATALOG),R(UDP_BATCH)),{NULL}},
    {"src/runtime/xl6_ownership_domain_udp_language.c",D(R(OWNERSHIP_CATALOG),R(UDP_LANGUAGE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_udp_stats.c",D(R(OWNERSHIP_CATALOG),R(UDP_STATS)),{NULL}},
    {"src/runtime/xl6_native_class_result.c",D(R(NATIVE_HANDLE),R(CODE_FINALIZER),R(OWNERSHIP_DOMAIN_CLASS)),{NULL}},
    {"src/runtime/xl6_udp_future.c",D(R(UDP_LANGUAGE),R(EXECUTION_UDP_FUTURE),R(UDP_BATCH),R(NATIVE_CLASS_RESULT),R(FUTURE_CALLBACK_QUEUE),R(FUTURE_PAYLOAD)),{NULL}},
    {"src/runtime/xl6_udp_future_interrupt.c",D(R(UDP_FUTURE),R(FUTURE_CALLBACK_INTERRUPT)),{NULL}},
    {"src/runtime/xl6_udp_message.c",D(R(NET_BYTES),R(UDP_VALUES),R(NATIVE_HANDLE),R(FOREIGN_ERROR)),{NULL}},
    {"src/runtime/xl6_udp_message_buffer.c",D(R(UDP_MESSAGE),R(BUFFER_CORE)),{NULL}},
    {"src/runtime/xl6_udp_message_array.c",D(R(UDP_MESSAGE),R(SEQUENCE)),{NULL}},
    {"src/runtime/xl6_ownership_domain_udp_message.c",D(R(OWNERSHIP_CATALOG),R(UDP_MESSAGE)),{NULL}},
    {"src/runtime/xl6_udp_vector.c",D(R(UDP_LANGUAGE),R(NET_VECTOR_INPUT)),{NULL}},
    {"src/runtime/xl6_udp_vector_text.c",D(R(UDP_VECTOR),R(ARRAY_PRIMITIVE_ADMISSION)),{NULL}},
    {"src/runtime/xl6_udp_vector_bytes.c",D(R(UDP_VECTOR),R(ARRAY_REFERENCE_ADMISSION)),{NULL}},
    {"src/runtime/xl6_udp_vector_buffer.c",D(R(UDP_VECTOR),R(NET_VECTOR_BUFFER_INPUT)),{NULL}},
    {"src/runtime/xl6_udp_refs.c",D(R(UDP_LANGUAGE),R(NET_BYTES),R(SEQUENCE)),{NULL}},
    {"src/runtime/xl6_udp_transfer.c",D(R(UDP_LANGUAGE),R(NET_BATCH_PROGRESS),R(SEQUENCE)),{NULL}},
    {"src/runtime/xl6_udp_transfer_owned.c",D(R(UDP_TRANSFER),R(UDP_MESSAGE)),{NULL}},
    {"src/runtime/xl6_udp_transfer_copy.c",D(R(UDP_TRANSFER),R(NET_DATAGRAM_PACKET)),{NULL}},
    {"src/runtime/xl6_udp_message_send.c",D(R(UDP_LANGUAGE),R(UDP_MESSAGE),R(EXECUTION_UDP_DATA)),{NULL}},
    {"src/runtime/xl6_udp_multicast.c",D(R(UDP_LANGUAGE),R(FUTURE_PAYLOAD),R(FUTURE_CALLBACK),R(CLOSURE),R(OWNERSHIP_DOMAIN_NATIVE_FUTURE)),{NULL}},
    {"src/runtime/xl6_task_frame.c",D(R(CAPTURE_V2),R(CODE_LEASE),R(FUTURE_PAYLOAD)),{"XRT_MODULE_TASK","XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_task_net.c",D(R(TASK_FRAME),R(OWNERSHIP_DOMAIN_NET_ENGINE_NATIVE)),{"XRT_MODULE_TASK_NET"}},
    {"src/runtime/xl6_ownership_domain_task_net.c",D(R(OWNERSHIP_CATALOG),R(TASK_NET),R(OWNERSHIP_DOMAIN_TASK),R(OWNERSHIP_DOMAIN_NET_ENGINE_NATIVE)),{NULL}},
    {"src/runtime/xl6_task_result_closure.c",D(R(TASK_FRAME),R(FUTURE_VALUE),R(CLOSURE)),{NULL}},
    {"src/runtime/xl6_task_net_language.c",D(R(EXECUTION_NET_ENGINE),R(TASK_NET),R(TASK_RESULT_CLOSURE)),{NULL}}
};
_Static_assert(sizeof(xl6ArtifactComponents)/sizeof(xl6ArtifactComponents[0]) == XL6_ARTIFACT_RUNTIME_COUNT,
    "Every resident component needs one dependency descriptor");
#undef D
_Static_assert(XL6_ARTIFACT_RUNTIME_COUNT > 0, "Resident graph needs a population");

const XL6_ArtifactRuntimeComponent* xl6ArtifactRuntimeComponentGet(unsigned id)
{
    return id < XL6_ARTIFACT_RUNTIME_COUNT ? &xl6ArtifactComponents[id] : NULL;
}

XL6_ArtifactRuntimeId xl6ArtifactRuntimeSymbolOwner(const char* symbol)
{
    static const struct { const char* name; XL6_ArtifactRuntimeId owner; } exports[] = {
#define XL6_ARTIFACT_SYMBOL(Name, Component) {Name, R(Component)},
#include "src/compile/xl6_artifact_runtime_symbols.inc"
#undef XL6_ARTIFACT_SYMBOL
    };
    if (symbol != NULL)
        for (size_t i=0;i<sizeof(exports)/sizeof(exports[0]);++i)
            if (!strcmp(symbol,exports[i].name)) return exports[i].owner;
    return XL6_ARTIFACT_RUNTIME_COUNT;
}

const XL6_ArtifactNativeSymbol* xl6ArtifactNativeSymbolLookup(const char* symbol, size_t* count)
{
    static const XL6_ArtifactNativeSymbol exports[] = {
#define XL6_ARTIFACT_NATIVE_SYMBOL(Symbol, Module) {Symbol, Module},
#include "src/compile/xl6_artifact_native_symbols.inc"
#undef XL6_ARTIFACT_NATIVE_SYMBOL
    };
    size_t first=0, last=sizeof(exports)/sizeof(exports[0]), begin, end;
    if (!count) return NULL;
    *count=0;
    if (!symbol) return NULL;
    /* Lower bound: shared providers occupy adjacent rows in the catalog. */
    while (first<last) {
        size_t middle=first+(last-first)/2;
        if (strcmp(exports[middle].symbol,symbol)<0) first=middle+1;
        else last=middle;
    }
    begin=first;
    end=sizeof(exports)/sizeof(exports[0]);
    while (first<end && !strcmp(exports[first].symbol,symbol)) ++first;
    *count=first-begin;
    return *count ? &exports[begin] : NULL;
}

static int xl6ArtifactNativeModuleRequire(XL6_ArtifactRuntimePlan* plan, const char* module)
{
    size_t i=0;
    if (!module) return 1;
    while (i<plan->nativeModuleCount && strcmp(plan->nativeModules[i],module)) ++i;
    if (i<plan->nativeModuleCount) return 1;
    if (i==XL6_ARTIFACT_NATIVE_MODULE_LIMIT) return 0;
    plan->nativeModules[plan->nativeModuleCount++]=module;
    return 1;
}

static int xl6ArtifactVisitNativeModule(void* context, const char* module)
{
    return xl6ArtifactNativeModuleRequire(context,module);
}

static int xl6ArtifactDependencies(void* context, size_t id,
    const size_t** dependencies, size_t* count)
{
    const XL6_ArtifactRuntimeComponent* component;
    (void)context;
    if (id >= XL6_ARTIFACT_RUNTIME_COUNT) return 0;
    component = &xl6ArtifactComponents[id];
    *dependencies = component->dependencies; *count = component->dependencyCount;
    return 1;
}

int xl6ArtifactRuntimeClosePlan(const XL6_ArtifactRuntimeSet* roots, XL6_ArtifactRuntimePlan* plan)
{
    XL6_ArtifactRuntimePlan result;
    XL6_ArtifactRuntimeSet copy = {{0}};
    if (!plan) return 0;
    /* Snapshot before zeroing: roots may alias the previous output plan. */
    if (roots != NULL) copy = *roots;
    memset(plan,0,sizeof(*plan));
    if (roots == NULL || !xl6ComponentSetValid(copy.words,XL6_ARTIFACT_RUNTIME_COUNT)) return 0;
    memset(&result,0,sizeof(result));
    result.roots=copy; result.components=copy;
    if (!xl6ComponentSetClose(result.components.words,XL6_ARTIFACT_RUNTIME_COUNT,
            xl6ArtifactDependencies,NULL)) return 0;
    for (unsigned i=0;i<XL6_ARTIFACT_RUNTIME_COUNT;++i) {
        const XL6_ArtifactRuntimeComponent* item=&xl6ArtifactComponents[i];
        if (!xl6ArtifactRuntimeSetHas(&result.components,i)) continue;
        if(item->hostAbi & ~(unsigned)XL6_ARTIFACT_HOST_ALL)return 0;
        result.hostAbi|=item->hostAbi;
        result.sources[result.sourceCount++]=item->source;
        for (size_t j=0;j<sizeof(item->nativeModules)/sizeof(item->nativeModules[0]) && item->nativeModules[j];++j) {
            size_t k=0;
            while (k<result.nativeModuleCount && strcmp(result.nativeModules[k],item->nativeModules[j])) ++k;
            if (k<result.nativeModuleCount) continue;
            if (k==XL6_ARTIFACT_NATIVE_MODULE_LIMIT) return 0;
            result.nativeModules[result.nativeModuleCount++]=item->nativeModules[j];
        }
    }
    *plan=result;
    return 1;
}

/* Each physical type is visited once, including the nested payload of a
 * nullable/product/callable. This is not a scan of unused source declarations. */
typedef struct XL6_ArtifactTypeWork {
    const XL6_IrModule* module;
    unsigned char* seen;
    XL6_IrTypeId* queue;
    size_t count;
} XL6_ArtifactTypeWork;

static void xl6ArtifactTypeRequire(XL6_ArtifactTypeWork* work, XL6_IrTypeId id)
{
    const XL6_IrType* type=xl6IrTypeGet(&work->module->types,id);
    size_t index;
    if (!type) return; /* INVALID denotes no payload, already checked by XLIR. */
    index=(size_t)(type-work->module->types.items);
    if (!work->seen[index]) { work->seen[index]=1; work->queue[work->count++]=id; }
}

static int xl6ArtifactInstructionReachable(const XL6_IrModule* module, size_t index)
{
    for (size_t i=0;i<module->functionCount;++i) {
        const XL6_IrFunction* function=&module->functions[i];
        if (index>=function->firstInstruction &&
            index-function->firstInstruction<function->instructionCount)
            return function->reachability != XL6_IR_REACHABILITY_UNREACHABLE;
    }
    return 0;
}

int xl6ArtifactOwnershipPlanBuild(const XL6_ArtifactRuntimePlan* base,XL6_ArtifactOwnershipPlan* output)
{
    if(!output)return 0;
    memset(output,0,sizeof(*output));
    if(!base || base->entry<XL6_ARTIFACT_ENTRY_FRAGMENT || base->entry>XL6_ARTIFACT_ENTRY_EXECUTABLE_V1 ||
        base->sourceCount>XL6_ARTIFACT_RUNTIME_COUNT || base->nativeModuleCount>XL6_ARTIFACT_NATIVE_MODULE_LIMIT)return 0;
    XL6_ArtifactOwnershipPlan result={0};
    XL6_ArtifactRuntimePlan closed={0};
    if(!xl6ArtifactRuntimeClosePlan(&base->roots,&closed) ||
        !xl6ArtifactRuntimeSetEqual(&base->components,&closed.components) || base->sourceCount!=closed.sourceCount ||
        base->hostAbi!=closed.hostAbi)return 0;
    for(size_t i=0;i<closed.sourceCount;++i)
        if(!base->sources[i] || strcmp(base->sources[i],closed.sources[i]))return 0;
    XL6_ArtifactRuntimeSet roots=base->roots;
    if(!xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,R(OWNERSHIP_CATALOG)))return 0;
#define XL6_OWNERSHIP_DOMAIN_ENTRY(Producer, Component, Getter) \
    if(xl6ArtifactRuntimeSetHas(&base->components,R(Producer)) || xl6ArtifactRuntimeSetHas(&base->components,R(Component))){ \
        if(!xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,R(Component)))return 0; \
        result.domains[result.domainCount++]=#Getter; \
    }
#include "src/runtime/xl6_ownership_domains.inc"
#undef XL6_OWNERSHIP_DOMAIN_ENTRY
    if(!xl6ArtifactRuntimeClosePlan(&roots,&result.runtime))return 0;
    for(size_t i=0;i<base->nativeModuleCount;++i)
        if(!xl6ArtifactNativeModuleRequire(&result.runtime,base->nativeModules[i]))return 0;
    result.runtime.entry=base->entry;result.runtime.entryOwnershipAbi=base->entryOwnershipAbi;*output=result;return 1;
}

static int xl6ArtifactVisitResidentSymbol(void* opaque, const char* symbol)
{
    XL6_ArtifactRuntimeSet* roots = opaque;
    XL6_ArtifactRuntimeId owner = xl6ArtifactRuntimeSymbolOwner(symbol);
    return owner != XL6_ARTIFACT_RUNTIME_COUNT &&
        xl6ComponentSetAdd(roots->words,XL6_ARTIFACT_RUNTIME_COUNT,owner);
}

int xl6ArtifactRuntimePlanBuild(const XL6_IrModule* module, XL6_ArtifactEntryAbi entry,
    XL6_ArtifactRuntimePlan* plan, XL6_DiagList* diags)
{
    XL6_ArtifactTypeWork work;
    XL6_IrCgenOptions cOptions={0};
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(R(SLOT));
#define REQUIRE(...) do { const size_t ids[] = {__VA_ARGS__}; \
    if (!xl6ComponentSetAddMany(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,ids,sizeof(ids)/sizeof(ids[0]))) { \
        xrtFree(work.seen); xrtFree(work.queue); \
        xl6DiagAdd(diags,0,0,"Resident root identity is outside the component population"); return 0; \
    } } while (0)
    if (!plan) return 0;
    memset(plan,0,sizeof(*plan));
    if (entry < XL6_ARTIFACT_ENTRY_FRAGMENT || entry > XL6_ARTIFACT_ENTRY_EXECUTABLE_V1) {
        xl6DiagAdd(diags,0,0,"Artifact entry ABI is unresolved");
        return 0;
    }
    if (!module || module->stage != XL6_IR_STAGE_LOWERED) {
        xl6DiagAdd(diags,0,0,"Artifact runtime plan requires Lowered XLIR");
        return 0;
    }
    if (!xl6IrVerifyModule(module,diags)) return 0;
    memset(&work,0,sizeof(work)); work.module=module;
    if (module->types.count > SIZE_MAX/sizeof(*work.queue)) return 0;
    work.seen=xrtMalloc(module->types.count ? module->types.count : 1);
    work.queue=xrtMalloc(module->types.count ? module->types.count*sizeof(*work.queue) : 1);
    if (!work.seen || !work.queue) {
        xrtFree(work.seen); xrtFree(work.queue);
        xl6DiagAdd(diags,0,0,"Cannot allocate artifact type dependency worklist");
        return 0;
    }
    memset(work.seen,0,module->types.count);
    /* The generated checked init/run/fini boundary owns an error scope even
     * when the language body contains no fallible instruction. Its deployment
     * ABI is explicit, independent of the module's source function effects. */
    if (entry != XL6_ARTIFACT_ENTRY_FRAGMENT) REQUIRE(R(FOREIGN_ERROR));
    if (module->abi.codeOwnerAbi != XL6_IR_MODULE_CODE_OWNER_NONE) REQUIRE(R(CODE_LEASE));
    if (module->abi.continuationAbi != XL6_IR_MODULE_CONTINUATION_NONE)
        REQUIRE(R(EXECUTOR),R(CONTINUATION_FUTURE),R(CONTINUATION_FRAME));
    if(module->abi.deadlineOwnershipAbi!=XL6_IR_MODULE_DEADLINE_OWNERSHIP_NONE)
        REQUIRE(R(DEADLINE_SERVICE),R(OWNERSHIP_DOMAIN_DEADLINE_SERVICE));
    if(module->abi.executionOwnershipAbi!=XL6_IR_MODULE_EXECUTION_OWNERSHIP_NONE)
        REQUIRE(R(EXECUTION_MODULE),R(OWNERSHIP_DOMAIN_EXECUTION_SOURCE));
    if (module->abi.taskPoolAbi != XL6_IR_MODULE_TASK_POOL_NONE) REQUIRE(R(TASK));
    for (size_t i=0;i<module->callTargetCount;++i)
        if (module->callTargets[i].reachability != XL6_IR_REACHABILITY_UNREACHABLE &&
            xl6IrRuntimeFsBridgeUsesOwner(module->callTargets[i].runtimeBridge,"xl6_xrt_fs_async_destroy"))
            REQUIRE(R(FILE_ASYNC));
    if (module->abi.futureAggregateAbi != XL6_IR_FUTURE_AGGREGATE_NONE) REQUIRE(R(FUTURE_AGGREGATE));
    if (module->abi.nativeFutureMapAbi != XL6_IR_NATIVE_FUTURE_MAP_NONE) REQUIRE(R(FUTURE_CALLBACK),R(FUTURE_PAYLOAD));
    if (module->abi.networkServiceAbi != XL6_IR_MODULE_NETWORK_SERVICE_NONE) REQUIRE(R(NETWORK_SERVICE));
    if (module->asyncMachineCount) REQUIRE(R(CONTINUATION_FRAME),R(FUTURE_PAYLOAD));
    if (module->generatorMachineCount) REQUIRE(R(GENERATOR));
    if (module->classLayoutCount) REQUIRE(R(CODE_FINALIZER));
    if (module->exportCount) REQUIRE(R(FOREIGN_ERROR));
    for (size_t i=0;i<module->storageCount;++i) {
        xl6ArtifactTypeRequire(&work,module->storages[i].type);
        xl6ArtifactTypeRequire(&work,module->storages[i].cellType);
    }
    for (size_t i=0;i<module->instructionCount;++i) {
        const XL6_IrInstruction* instruction=&module->instructions[i];
        /* Cleanup scopes are also emitted in unreachable structural bodies. */
        switch (instruction->kind) {
        case XL6_IR_INSTR_ERROR_UNWIND_BEGIN: case XL6_IR_INSTR_CLEANUP_BEGIN:
        case XL6_IR_INSTR_CLEANUP_INHERIT_BEGIN: REQUIRE(R(FOREIGN_ERROR)); break;
        default: break;
        }
        if (!xl6ArtifactInstructionReachable(module,i)) continue;
        xl6ArtifactTypeRequire(&work,instruction->type);
        xl6ArtifactTypeRequire(&work,instruction->calleeType);
        xl6ArtifactTypeRequire(&work,instruction->payloadType);
        if (instruction->kind==XL6_IR_INSTR_AWAIT) REQUIRE(R(FUTURE_PAYLOAD));
        if (instruction->kind!=XL6_IR_INSTR_RUNTIME_CALL) continue;
        if (instruction->runtimeOp == XL6_IR_RUNTIME_STACK_CREATE ||
            instruction->runtimeOp == XL6_IR_RUNTIME_ARRAY_CREATE ||
            instruction->runtimeOp == XL6_IR_RUNTIME_LIST_CREATE ||
            instruction->runtimeOp == XL6_IR_RUNTIME_LIST_IMPORT_CHAIN ||
            instruction->runtimeOp == XL6_IR_RUNTIME_ARRAY_IMPORT_CHAIN) {
            const XL6_IrType* recipe=xl6IrTypeGet(&module->types,instruction->type);
            if (recipe && recipe->collectionAbi==XL6_IR_COLLECTION_LIST)
                recipe=xl6IrTypeGet(&module->types,recipe->collectionValueSchemaType);
            for (size_t depth=0;recipe && depth<=64;++depth) {
                switch (recipe->sequenceLayout.abi) {
                case XL6_IR_SEQUENCE_LAYOUT_PRIMITIVE_V2: REQUIRE(R(ARRAY_PRIMITIVE_ADMISSION)); break;
                case XL6_IR_SEQUENCE_LAYOUT_DYNAMIC_VALUE: REQUIRE(R(ARRAY_REFERENCE_ADMISSION)); break;
                case XL6_IR_SEQUENCE_LAYOUT_REFERENCE_V2: REQUIRE(R(ARRAY_REFERENCE_ADMISSION)); break;
                case XL6_IR_SEQUENCE_LAYOUT_NOMINAL_V2: REQUIRE(R(ARRAY_NOMINAL_ADMISSION)); break;
                case XL6_IR_SEQUENCE_LAYOUT_UNION_VALUE_V1: REQUIRE(R(ARRAY_UNION_ADMISSION)); break;
                case XL6_IR_SEQUENCE_LAYOUT_C_SCALAR_V1: REQUIRE(R(ARRAY_C_SCALAR_ADMISSION)); break;
                case XL6_IR_SEQUENCE_LAYOUT_NULLABLE_C_SCALAR_V1: REQUIRE(R(ARRAY_NULLABLE_C_SCALAR_ADMISSION)); break;
                case XL6_IR_SEQUENCE_LAYOUT_NUMERIC_ENUM_V1: REQUIRE(R(ARRAY_NUMERIC_ENUM_ADMISSION)); break;
                case XL6_IR_SEQUENCE_LAYOUT_NATIVE_VALUE_V1:
                    /* Shared resident-symbol visitor selects the exact profile. */
                    break;
                case XL6_IR_SEQUENCE_LAYOUT_SEQUENCE_REFERENCE_V1: REQUIRE(R(ARRAY_SEQUENCE_ADMISSION)); break;
                case XL6_IR_SEQUENCE_LAYOUT_LIST_REFERENCE_V1: REQUIRE(R(LIST_SCHEMA)); break;
                case XL6_IR_SEQUENCE_LAYOUT_SET_REFERENCE_V1: REQUIRE(R(SET_SCHEMA)); break;
                case XL6_IR_SEQUENCE_LAYOUT_DICT_REFERENCE_V1: REQUIRE(R(DICT_SCHEMA)); break;
                case XL6_IR_SEQUENCE_LAYOUT_TREE_REFERENCE_V1: REQUIRE(R(TREE_ADMISSION)); break;
                case XL6_IR_SEQUENCE_LAYOUT_QUEUE_REFERENCE_V1:
                case XL6_IR_SEQUENCE_LAYOUT_CHANNEL_REFERENCE_V1:
                case XL6_IR_SEQUENCE_LAYOUT_SLOT_MAP_REFERENCE_V1:
                    /* The shared resident-symbol visitor selects the exact
                     * family and all descendants from this closed recipe. */
                    break;
                case XL6_IR_SEQUENCE_LAYOUT_CALLABLE_REFERENCE_V1: REQUIRE(R(ARRAY_CALLABLE_ADMISSION)); break;
                case XL6_IR_SEQUENCE_LAYOUT_FUTURE_REFERENCE_V1: REQUIRE(R(ARRAY_FUTURE_ADMISSION)); break;
                case XL6_IR_SEQUENCE_LAYOUT_SEMAPHORE_REFERENCE_V1: REQUIRE(R(ARRAY_SEMAPHORE_ADMISSION)); break;
                case XL6_IR_SEQUENCE_LAYOUT_THREAD_REFERENCE_V1: REQUIRE(R(ARRAY_THREAD_ADMISSION)); break;
                case XL6_IR_SEQUENCE_LAYOUT_MUTEX_REFERENCE_V1:
                case XL6_IR_SEQUENCE_LAYOUT_COND_REFERENCE_V1:
                case XL6_IR_SEQUENCE_LAYOUT_RWLOCK_REFERENCE_V1: REQUIRE(R(ARRAY_SYNC_ADMISSION)); break;
                default: /* Verifier must reject unsupported producers. */
                    xl6DiagAdd(diags,0,0,"Artifact sequence recipe has no registered resident layout provider");
                    xrtFree(work.seen); xrtFree(work.queue); return 0;
                }
                if (recipe->sequenceLayout.abi!=XL6_IR_SEQUENCE_LAYOUT_SEQUENCE_REFERENCE_V1 &&
                    recipe->sequenceLayout.abi!=XL6_IR_SEQUENCE_LAYOUT_LIST_REFERENCE_V1 &&
                    recipe->sequenceLayout.abi!=XL6_IR_SEQUENCE_LAYOUT_SET_REFERENCE_V1 &&
                    recipe->sequenceLayout.abi!=XL6_IR_SEQUENCE_LAYOUT_DICT_REFERENCE_V1) break;
                recipe=xl6IrTypeGet(&module->types,recipe->sequenceLayout.baseElementType);
                if (recipe && (recipe->collectionAbi==XL6_IR_COLLECTION_LIST || recipe->collectionAbi==XL6_IR_COLLECTION_SET || recipe->collectionAbi==XL6_IR_COLLECTION_DICT))
                    recipe=xl6IrTypeGet(&module->types,recipe->collectionValueSchemaType);
                if (!recipe || depth==64) { xrtFree(work.seen); xrtFree(work.queue); return 0; }
            }
        } else if (instruction->runtimeOp>=XL6_IR_RUNTIME_STACK_PUSH_BATCH &&
            instruction->runtimeOp<=XL6_IR_RUNTIME_STACK_PEEK_BATCH_AT) REQUIRE(R(STACK_BATCH));
        else if (instruction->runtimeOp > XL6_IR_RUNTIME_STACK_CREATE &&
            instruction->runtimeOp <= XL6_IR_RUNTIME_STACK_CAPACITY) {
            const XL6_IrType* payload=xl6IrTypeGet(&module->types,instruction->payloadType);
            bool read=instruction->runtimeOp==XL6_IR_RUNTIME_STACK_POP || instruction->runtimeOp==XL6_IR_RUNTIME_STACK_PEEK ||
                instruction->runtimeOp==XL6_IR_RUNTIME_STACK_PEEK_AT || instruction->runtimeOp==XL6_IR_RUNTIME_STACK_TRY_POP ||
                instruction->runtimeOp==XL6_IR_RUNTIME_STACK_TRY_PEEK || instruction->runtimeOp==XL6_IR_RUNTIME_STACK_TRY_PEEK_AT;
            REQUIRE(read && payload && payload->cAbi==XL6_IR_C_ABI_CLOSURE_PTR ? R(STACK_CALLABLE) : R(STACK));
        }
        if (instruction->runtimeOp == XL6_IR_RUNTIME_NULLABLE_STRING_EQUAL) REQUIRE(R(NULLABLE_STRING));
        /* Only operations which physically consume an xrttype descriptor
         * select its immutable resident profile. Raw scalar queue/channel
         * transports and ordinary optional values need no descriptor code. */
        bool trivialExtract=instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_TYPED_COLLECTION_METHOD &&
            (instruction->typedCollectionOp==XL6_IR_TYPED_COLLECTION_POP ||
             instruction->typedCollectionOp==XL6_IR_TYPED_COLLECTION_TAKE) &&
            xl6IrArrayTrivialExtractResult(xl6IrTypeGet(&module->types,instruction->type));
        if (trivialExtract || instruction->runtimeOp==XL6_IR_RUNTIME_ARRAY_IMPORT_CHAIN ||
            instruction->runtimeOp==XL6_IR_RUNTIME_ARRAY_CREATE ||
            instruction->runtimeOp==XL6_IR_RUNTIME_LIST_CREATE ||
            instruction->runtimeOp==XL6_IR_RUNTIME_LIST_IMPORT_CHAIN ||
            (instruction->runtimeOp >= XL6_IR_RUNTIME_STACK_CREATE &&
             instruction->runtimeOp <= XL6_IR_RUNTIME_STACK_CAPACITY) ||
            (instruction->runtimeOp >= XL6_IR_RUNTIME_AVL_TREE_CREATE &&
             instruction->runtimeOp <= XL6_IR_RUNTIME_AVL_TREE_CLONE) ||
            instruction->runtimeOp == XL6_IR_RUNTIME_SPECIAL_QUEUE_PUSH_BATCH ||
            instruction->runtimeOp == XL6_IR_RUNTIME_SPECIAL_QUEUE_POP_BATCH_INTO) {
            const XL6_IrType* created=(instruction->runtimeOp==XL6_IR_RUNTIME_STACK_CREATE ||
                instruction->runtimeOp==XL6_IR_RUNTIME_ARRAY_CREATE ||
                instruction->runtimeOp==XL6_IR_RUNTIME_LIST_CREATE ||
                instruction->runtimeOp==XL6_IR_RUNTIME_LIST_IMPORT_CHAIN ||
                instruction->runtimeOp==XL6_IR_RUNTIME_ARRAY_IMPORT_CHAIN)
                ? xl6IrTypeGet(&module->types,instruction->type) : NULL;
            if (created && created->collectionAbi==XL6_IR_COLLECTION_LIST)
                created=xl6IrTypeGet(&module->types,created->collectionValueSchemaType);
            const XL6_IrType* extracted=trivialExtract ? xl6IrTypeGet(&module->types,instruction->type) : NULL;
            /* Extraction consumes the result descriptor AND the optional's
             * base descriptor. Only the result consumes a Value converter. */
            XL6_IrTypeId base=extracted && extracted->kind==XL6_IR_TYPE_NULLABLE &&
                extracted->paramCount==1 && extracted->params ? extracted->params[0] : XL6_IR_TYPE_INVALID;
            XL6_IrTypeId physicalIds[66]; size_t physicalCount=0;
            physicalIds[physicalCount++]=trivialExtract ? instruction->type : instruction->payloadType;
            if (trivialExtract) { if (base) physicalIds[physicalCount++]=base; }
            else {
                physicalIds[physicalCount++]=instruction->runtimeResourceKeyType;
                for (const XL6_IrType* recipe=created;recipe &&
                    (recipe->sequenceLayout.abi==XL6_IR_SEQUENCE_LAYOUT_SEQUENCE_REFERENCE_V1 ||
                     recipe->sequenceLayout.abi==XL6_IR_SEQUENCE_LAYOUT_LIST_REFERENCE_V1 ||
                     recipe->sequenceLayout.abi==XL6_IR_SEQUENCE_LAYOUT_SET_REFERENCE_V1 ||
                     recipe->sequenceLayout.abi==XL6_IR_SEQUENCE_LAYOUT_DICT_REFERENCE_V1);) {
                    if (physicalCount>=sizeof(physicalIds)/sizeof(*physicalIds)) {
                        xrtFree(work.seen); xrtFree(work.queue); return 0;
                    }
                    recipe=xl6IrTypeGet(&module->types,recipe->sequenceLayout.baseElementType);
                    if (recipe && (recipe->collectionAbi==XL6_IR_COLLECTION_LIST || recipe->collectionAbi==XL6_IR_COLLECTION_SET || recipe->collectionAbi==XL6_IR_COLLECTION_DICT))
                        recipe=xl6IrTypeGet(&module->types,recipe->collectionValueSchemaType);
                    physicalIds[physicalCount++]=recipe ? recipe->sequenceLayout.elementType : XL6_IR_TYPE_INVALID;
                }
            }
            for (size_t slot=0;slot<physicalCount;++slot) {
                XL6_IrTypeId id=physicalIds[slot];
                const XL6_IrType* physical=xl6IrTypeGet(&module->types,id);
                if (!physical) continue;
                switch (physical->nativeSlotAbi) {
                case XL6_IR_NATIVE_SLOT_CHAR: REQUIRE(R(CHAR_TYPE)); break;
                case XL6_IR_NATIVE_SLOT_STRING: REQUIRE(R(STRING_TYPE)); break;
                case XL6_IR_NATIVE_SLOT_NULLABLE_STRING: REQUIRE(R(NULLABLE_STRING)); break;
                case XL6_IR_NATIVE_SLOT_NULLABLE_BOOL: REQUIRE(R(NULLABLE_BOOL)); break;
                case XL6_IR_NATIVE_SLOT_NULLABLE_I64: REQUIRE(R(NULLABLE_I64)); break;
                case XL6_IR_NATIVE_SLOT_NULLABLE_U64: REQUIRE(R(NULLABLE_U64)); break;
                case XL6_IR_NATIVE_SLOT_NULLABLE_TYPE_ID: REQUIRE(R(NULLABLE_U64)); break;
                case XL6_IR_NATIVE_SLOT_NULLABLE_NATIVE_I8: REQUIRE(R(NULLABLE_NATIVE_I8)); break;
                case XL6_IR_NATIVE_SLOT_NULLABLE_NATIVE_U8: REQUIRE(R(NULLABLE_NATIVE_U8)); break;
                case XL6_IR_NATIVE_SLOT_NULLABLE_NATIVE_I16: REQUIRE(R(NULLABLE_NATIVE_I16)); break;
                case XL6_IR_NATIVE_SLOT_NULLABLE_NATIVE_U16: REQUIRE(R(NULLABLE_NATIVE_U16)); break;
                case XL6_IR_NATIVE_SLOT_NULLABLE_NATIVE_I32: REQUIRE(R(NULLABLE_NATIVE_I32)); break;
                case XL6_IR_NATIVE_SLOT_NULLABLE_NATIVE_U32: REQUIRE(R(NULLABLE_NATIVE_U32)); break;
                case XL6_IR_NATIVE_SLOT_NULLABLE_F32: REQUIRE(R(NULLABLE_F32)); break;
                case XL6_IR_NATIVE_SLOT_NULLABLE_F64: REQUIRE(R(NULLABLE_F64)); break;
                case XL6_IR_NATIVE_SLOT_NULLABLE_TIME: REQUIRE(R(NULLABLE_TIME)); break;
                case XL6_IR_NATIVE_SLOT_NULLABLE_CHAR: REQUIRE(R(NULLABLE_CHAR)); break;
                default: break;
                }
                /* A descriptor and its dynamic projection have separate
                 * dependencies. Stack-only storage never pulls a converter. */
                bool projection=instruction->runtimeOp==XL6_IR_RUNTIME_ARRAY_IMPORT_CHAIN ||
                    instruction->runtimeOp==XL6_IR_RUNTIME_ARRAY_CREATE ||
                    instruction->runtimeOp==XL6_IR_RUNTIME_LIST_CREATE ||
                    instruction->runtimeOp==XL6_IR_RUNTIME_LIST_IMPORT_CHAIN ||
                    (trivialExtract ? slot==0 : (slot ? instruction->runtimeOp == XL6_IR_RUNTIME_AVL_TREE_KEYS :
                    instruction->runtimeOp == XL6_IR_RUNTIME_AVL_TREE_VALUES ||
                    instruction->runtimeOp == XL6_IR_RUNTIME_SPECIAL_QUEUE_PUSH_BATCH ||
                    instruction->runtimeOp == XL6_IR_RUNTIME_SPECIAL_QUEUE_POP_BATCH_INTO));
                if (!projection) continue;
                switch (physical->nativeSlotAbi) {
                case XL6_IR_NATIVE_SLOT_CHAR: REQUIRE(R(CHAR_VALUE)); break;
                case XL6_IR_NATIVE_SLOT_STRING: REQUIRE(R(STRING_VALUE)); break;
                case XL6_IR_NATIVE_SLOT_NULLABLE_BOOL: REQUIRE(R(NULLABLE_BOOL_VALUE)); break;
                case XL6_IR_NATIVE_SLOT_NULLABLE_I64: REQUIRE(R(NULLABLE_I64_VALUE)); break;
                case XL6_IR_NATIVE_SLOT_NULLABLE_U64: REQUIRE(R(NULLABLE_U64_VALUE)); break;
                case XL6_IR_NATIVE_SLOT_NULLABLE_TYPE_ID: REQUIRE(R(NULLABLE_U64_VALUE)); break;
                case XL6_IR_NATIVE_SLOT_NULLABLE_NATIVE_I8: REQUIRE(R(NULLABLE_NATIVE_I8_VALUE)); break;
                case XL6_IR_NATIVE_SLOT_NULLABLE_NATIVE_U8: REQUIRE(R(NULLABLE_NATIVE_U8_VALUE)); break;
                case XL6_IR_NATIVE_SLOT_NULLABLE_NATIVE_I16: REQUIRE(R(NULLABLE_NATIVE_I16_VALUE)); break;
                case XL6_IR_NATIVE_SLOT_NULLABLE_NATIVE_U16: REQUIRE(R(NULLABLE_NATIVE_U16_VALUE)); break;
                case XL6_IR_NATIVE_SLOT_NULLABLE_NATIVE_I32: REQUIRE(R(NULLABLE_NATIVE_I32_VALUE)); break;
                case XL6_IR_NATIVE_SLOT_NULLABLE_NATIVE_U32: REQUIRE(R(NULLABLE_NATIVE_U32_VALUE)); break;
                case XL6_IR_NATIVE_SLOT_NULLABLE_F32: REQUIRE(R(NULLABLE_F32_VALUE)); break;
                case XL6_IR_NATIVE_SLOT_NULLABLE_F64: REQUIRE(R(NULLABLE_F64_VALUE)); break;
                case XL6_IR_NATIVE_SLOT_NULLABLE_TIME: REQUIRE(R(NULLABLE_TIME_VALUE)); break;
                case XL6_IR_NATIVE_SLOT_NULLABLE_CHAR: REQUIRE(R(NULLABLE_CHAR_VALUE)); break;
                default: break;
                }
            }
        }
        if (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_NATIVE_HANDLE_TAKE) {
            const XL6_IrCallTarget* destroy = xl6IrCallTargetGet(module, instruction->targetSymbol);
            if (destroy && destroy->nativeHandlePolicyAbi == XL6_IR_NATIVE_HANDLE_POLICY_RESIDENT_V1) {
                XL6_ArtifactRuntimeId owner = xl6ArtifactRuntimeSymbolOwner(destroy->nativeHandlePolicyCName);
                if (owner == XL6_ARTIFACT_RUNTIME_COUNT) {
                    xrtFree(work.seen); xrtFree(work.queue); return 0;
                }
                REQUIRE(R(NATIVE_HANDLE), owner);
            }
        }
        switch (instruction->runtimeOp) {
        case XL6_IR_RUNTIME_XVALUE_CAST_CHECKED: {
            const XL6_IrType* target = xl6IrTypeGet(&module->types, instruction->type);
            if (target && target->kind == XL6_IR_TYPE_BUILTIN &&
                (target->cAbi == XL6_IR_C_ABI_I64 || target->cAbi == XL6_IR_C_ABI_U64))
                REQUIRE(R(VALUE_ADMISSION));
            break;
        }
        case XL6_IR_RUNTIME_XVALUE_CAST_NULLABLE_BYTES:
            REQUIRE(R(VALUE_ADMISSION)); break;
        case XL6_IR_RUNTIME_XVALUE_ENUM_BIND:
            REQUIRE(R(VALUE_ADMISSION)); break;
        case XL6_IR_RUNTIME_XVALUE_CAST_NULLABLE_TAGGED: {
            const XL6_IrType* element = xl6IrTypeGet(&module->types, instruction->payloadType);
            if (element && element->kind == XL6_IR_TYPE_BUILTIN &&
                (element->cAbi == XL6_IR_C_ABI_I64 || element->cAbi == XL6_IR_C_ABI_U64))
                REQUIRE(R(VALUE_ADMISSION));
            break;
        }
        case XL6_IR_RUNTIME_XVALUE_PROJECT_XVALUE_RETAIN:
            if (xl6IrTypeGet(&module->types,instruction->type)->cAbi==XL6_IR_C_ABI_MANAGED_BYTES) REQUIRE(R(VALUE_ADMISSION));
            break;
        case XL6_IR_RUNTIME_XVALUE_PROJECT_SCALAR_COPY:
            if (xl6IrTypeGet(&module->types,instruction->type)->cAbi==XL6_IR_C_ABI_F32) REQUIRE(R(VALUE_ADMISSION));
            break;
        case XL6_IR_RUNTIME_XVALUE_COLLECTION_CHECKED_CAST:
            REQUIRE(R(VALUE_ADMISSION)); break;
        case XL6_IR_RUNTIME_XVALUE_COLLECTION_CHECKED_REBIND:
            if (instruction->collectionAdmissionAbi==XL6_IR_COLLECTION_ADMISSION_NOMINAL) REQUIRE(R(NOMINAL_ADMISSION));
            else REQUIRE(R(VALUE_ADMISSION));
            break;
        case XL6_IR_RUNTIME_XVALUE_NOMINAL_TEST:
        case XL6_IR_RUNTIME_XVALUE_NOMINAL_CAST_CHECKED:
        case XL6_IR_RUNTIME_XVALUE_NOMINAL_CAST_NULLABLE: REQUIRE(R(NOMINAL_ADMISSION)); break;
        case XL6_IR_RUNTIME_FUTURE_ALL_XVALUES_ARRAY: case XL6_IR_RUNTIME_FUTURE_RACE_ARRAY:
        case XL6_IR_RUNTIME_FUTURE_WHEN_ANY_INDEX_ARRAY:
            REQUIRE(R(FUTURE_AGGREGATE_ARRAY)); break;
        case XL6_IR_RUNTIME_FUTURE_CONTINUE_CLOSURE: REQUIRE(R(FUTURE_CALLBACK_QUEUE),R(FUTURE_PAYLOAD)); break;
        case XL6_IR_RUNTIME_TASK_SPAWN_CLOSURE: REQUIRE(R(TASK)); break;
        case XL6_IR_RUNTIME_FUTURE_SET_DEBUG_NAME: case XL6_IR_RUNTIME_FUTURE_DEBUG_NAME_COPY:
        case XL6_IR_RUNTIME_FUTURE_CREATE_TIME_MS: case XL6_IR_RUNTIME_FUTURE_COMPLETE_TIME_MS:
        case XL6_IR_RUNTIME_FUTURE_PENDING_CONTINUATIONS: REQUIRE(R(FUTURE_METADATA)); break;
        default: break;
        }
    }
    for (size_t i=0;i<module->callTargetCount;++i) {
        const XL6_IrCallTarget* target=&module->callTargets[i];
        if (target->reachability == XL6_IR_REACHABILITY_UNREACHABLE) continue;
        /* Exact linkage ownership is independent of source spelling/semantics. */
        XL6_ArtifactRuntimeId owner = xl6ArtifactRuntimeSymbolOwner(target->cName);
        if (owner != XL6_ARTIFACT_RUNTIME_COUNT) REQUIRE(owner);
        if (target->errorPhysicalAbi==XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE) REQUIRE(R(FOREIGN_ERROR));
        if (target->kind==XL6_IR_CALL_TARGET_INTERNAL_ASYNC) REQUIRE(R(FUTURE_PAYLOAD),R(FOREIGN_ERROR));
        xl6ArtifactTypeRequire(&work,target->returnType);
        for (size_t p=0;p<target->parameterCount;++p) xl6ArtifactTypeRequire(&work,target->parameterTypes[p]);
    }
    for (size_t i=0;i<module->classFieldCount;++i) xl6ArtifactTypeRequire(&work,module->classFields[i].type);
    for (size_t i=0;i<work.count;++i) {
        const XL6_IrType* type=xl6IrTypeGet(&module->types,work.queue[i]);
        if(type->runtimeResourceAbi==XL6_IR_RUNTIME_RESOURCE_AVL_TREE &&
            xl6IrAvlKeyOrderAbi(module,work.queue[i])==XL6_IR_RUNTIME_RESOURCE_KEY_ORDER_BYTES)
            REQUIRE(R(TREE_BYTES_KEY));
        switch (type->cAbi) {
        case XL6_IR_C_ABI_CLOSURE_PTR: REQUIRE(R(CLOSURE)); break;
        case XL6_IR_C_ABI_RUNTIME_CELL_PTR: REQUIRE(R(CELL)); break;
        case XL6_IR_C_ABI_RUNTIME_GENERATOR_PTR: REQUIRE(R(GENERATOR)); break;
        case XL6_IR_C_ABI_RUNTIME_VALUE_ITERATOR_PTR: REQUIRE(R(VALUE_ITERATOR)); break;
        case XL6_IR_C_ABI_RUNTIME_FUTURE_PTR: REQUIRE(R(FUTURE_PAYLOAD)); break;
        default: break;
        }
        for (size_t p=0;p<type->paramCount;++p) xl6ArtifactTypeRequire(&work,type->params[p]);
        xl6ArtifactTypeRequire(&work,type->callableResultType);
    }
    xrtFree(work.seen); xrtFree(work.queue);
    cOptions.emitScriptEntry=entry!=XL6_ARTIFACT_ENTRY_FRAGMENT;
    cOptions.emitMainWrapper=entry==XL6_ARTIFACT_ENTRY_EXECUTABLE_V1;
    if (!xl6IrCgenVisitResidentSymbols(module,diags,&cOptions,xl6ArtifactVisitResidentSymbol,&roots)) return 0;
    if (!xl6ArtifactRuntimeClosePlan(&roots,plan)) return 0;
    {
        /* The public xlValue ABI includes graph cloning, native/callable/type
         * values and their conversions even in scalar-only artifacts. This
         * fixed deployment contract is not inferred from source or C text. */
        static const char* const baseModules[]={
            "XRT_MODULE_TEMP_MEMORY", "XRT_MODULE_THREAD_KEY", "XRT_MODULE_ONCE",
            "XRT_MODULE_VALUE_GRAPH", "XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE",
            "XRUNTIME_MODULE_RUNTIME_VALUE_TYPE", "XRUNTIME_MODULE_RUNTIME_TYPE_STRING_VALUE",
            "XRUNTIME_MODULE_RUNTIME_CONVERT_STRING", "XRT_MODULE_XLON_WRITE", "XRT_MODULE_TIME_TEXT"
        };
        for (size_t i=0;i<sizeof(baseModules)/sizeof(baseModules[0]);++i)
            if (!xl6ArtifactNativeModuleRequire(plan,baseModules[i])) goto native_capacity_failure;
    }
    cOptions.emitScriptEntry=entry!=XL6_ARTIFACT_ENTRY_FRAGMENT;
    cOptions.emitMainWrapper=entry==XL6_ARTIFACT_ENTRY_EXECUTABLE_V1;
    if (!xl6IrCgenVisitNativeModules(module,diags,&cOptions,xl6ArtifactVisitNativeModule,plan)) {
        memset(plan,0,sizeof(*plan));
        xl6DiagAdd(diags,0,0,"Cannot build verified C implementation dependencies");
        return 0;
    }
    for (size_t i=0;i<module->callTargetCount;++i) {
        const XL6_IrCallTarget* target=&module->callTargets[i];
        const XL6_ArtifactNativeSymbol* symbols;
        size_t count;
        if (target->kind!=XL6_IR_CALL_TARGET_EXTERN_C ||
            target->reachability==XL6_IR_REACHABILITY_UNREACHABLE) continue;
        symbols=xl6ArtifactNativeSymbolLookup(target->cName,&count);
        for (size_t j=0;j<count;++j) {
            if (!xl6ArtifactNativeModuleRequire(plan,symbols[j].module))
                goto native_capacity_failure;
        }
    }
    plan->entry=entry;
    if(entry==XL6_ARTIFACT_ENTRY_EXECUTABLE_V1){
        bool graph=module->exportCount!=0 || xl6ArtifactRuntimeSetHas(&plan->components,R(CODE_FINALIZER)) ||
            xl6ArtifactRuntimeSetHas(&plan->components,R(CLOSURE)) ||
            xl6ArtifactRuntimeSetHas(&plan->components,R(CELL)) ||
            module->abi.readBoundaryAbi!=XL6_IR_MODULE_READ_BOUNDARY_NONE;
        for(size_t i=0;i<module->storageCount;++i){
            const XL6_IrStorage* storage=&module->storages[i];
            if(storage->kind==XL6_IR_STORAGE_GLOBAL && storage->moduleTraceAbi!=XL6_IR_FRAME_TRACE_NONE &&
                storage->moduleTraceAbi!=XL6_IR_FRAME_TRACE_INVALID && storage->moduleTraceAbi!=XL6_IR_FRAME_TRACE_OPAQUE)graph=true;
        }
        plan->entryOwnershipAbi=graph ? XL6_ARTIFACT_ENTRY_OWNERSHIP_GRAPH : XL6_ARTIFACT_ENTRY_OWNERSHIP_DIRECT;
        if(graph){
            XL6_ArtifactRuntimeSet entryRoots=plan->roots;
            XL6_ArtifactRuntimePlan entryPlan={0};XL6_ArtifactOwnershipPlan ownership={0};
            if(!xl6ComponentSetAdd(entryRoots.words,XL6_ARTIFACT_RUNTIME_COUNT,R(STATIC_MODULE)) ||
                !xl6ArtifactRuntimeClosePlan(&entryRoots,&entryPlan))goto native_capacity_failure;
            for(size_t i=0;i<plan->nativeModuleCount;++i)
                if(!xl6ArtifactNativeModuleRequire(&entryPlan,plan->nativeModules[i]))goto native_capacity_failure;
            entryPlan.entry=entry;entryPlan.entryOwnershipAbi=XL6_ARTIFACT_ENTRY_OWNERSHIP_GRAPH;
            if(!xl6ArtifactOwnershipPlanBuild(&entryPlan,&ownership))goto native_capacity_failure;
            *plan=ownership.runtime;
        }
    }
    return 1;
native_capacity_failure:
    memset(plan,0,sizeof(*plan));
    xl6DiagAdd(diags,0,0,"Artifact native module capacity exceeded");
    return 0;
}
#undef R
#undef REQUIRE

static void xl6CompileUnitBaseNameNoExt(char* out, size_t outSize, const char* path)
{
    const char* base;
    const char* p;
    const char* dot;
    size_t len;

    if ( out == NULL || outSize == 0 ) {
        return;
    }

    base = path != NULL ? path : "module";
    for ( p = base; *p != '\0'; ++p ) {
        if ( *p == '\\' || *p == '/' || *p == ':' ) {
            base = p + 1;
        }
    }

    dot = NULL;
    for ( p = base; *p != '\0'; ++p ) {
        if ( *p == '.' ) {
            dot = p;
        }
    }

    len = dot != NULL ? (size_t)(dot - base) : strlen(base);
    if ( len == 0 ) {
        base = "module";
        len = strlen(base);
    }
    if ( len >= outSize ) {
        len = outSize - 1;
    }
    memcpy(out, base, len);
    out[len] = '\0';
}

void xl6CompileUnitInit(XL6_CompileUnit* unit)
{
    if ( unit == NULL ) {
        return;
    }
    memset(unit, 0, sizeof(*unit));
    xl6ParseResultInit(&unit->parse);
    xl6ImportDepsInit(&unit->importDeps);
    xl6IrModuleInit(&unit->ir);
}

void xl6CompileUnitFree(XL6_CompileUnit* unit)
{
    if ( unit == NULL ) {
        return;
    }
    xl6ParseResultFree(&unit->parse);
    xl6ImportDepsFree(&unit->importDeps);
    xl6IrModuleFree(&unit->ir);
    memset(unit, 0, sizeof(*unit));
}
int xl6CompileUnitLoadFile(XL6_CompileUnit* unit, const char* path, int runSem)
{
    int ok;

    if ( unit == NULL || path == NULL ) {
        return 0;
    }

    unit->path = path;
    unit->phase = "parse";
    xl6CompileUnitBaseNameNoExt(unit->moduleName, sizeof(unit->moduleName), path);

    ok = xl6SourceOverlayParse(unit->importDeps.sourceOverlay, path, &unit->parse);
    if ( ok ) {
        unit->phase = "import";
        ok = xl6ImportsExpandModule(unit->parse.root, path, &unit->parse.diags, &unit->importDeps);
    }
    if ( ok ) {
        unit->phase = "stdlib";
        ok = xl6StdlibExpandModule(unit->parse.root, &unit->parse.diags);
    }
    if ( ok && runSem ) {
        unit->phase = "sem";
        ok = xl6SemCheckModuleWithQuery(unit->parse.root, &unit->parse.diags, unit->moduleName,
            unit->importDeps.sourceOverlay && unit->importDeps.sourceOverlay->semanticIndex,
            unit->importDeps.sourceOverlay ? unit->importDeps.sourceOverlay->completionQuery : NULL);
        unit->semChecked = 1;
    }
    if ( ok && runSem && !(unit->importDeps.sourceOverlay && unit->importDeps.sourceOverlay->completionQuery) ) {
        unit->phase = "ir";
        ok = xl6IrBuildModuleFromAst(&unit->ir, unit->parse.root, &unit->parse.diags);
        unit->irReady = ok;
    }
    unit->success = ok && unit->parse.diags.count == 0;
    if ( unit->success ) {
        unit->phase = "checked";
    }
    return unit->success;
}

int xl6CompileUnitLoadSource(XL6_CompileUnit* unit, const char* source, const char* virtualPath, int runSem)
{
    const char* path;
    int ok;

    if ( unit == NULL || source == NULL ) {
        return 0;
    }

    path = virtualPath != NULL && virtualPath[0] != '\0' ? virtualPath : "<memory>";
    unit->path = path;
    unit->phase = "parse";
    xl6CompileUnitBaseNameNoExt(unit->moduleName, sizeof(unit->moduleName), path);

    ok = xl6ParseSourceWithCompletion(source, path, &unit->parse, unit->importDeps.sourceOverlay ? unit->importDeps.sourceOverlay->completionQuery : NULL);
    if ( ok ) {
        unit->phase = "import";
        ok = xl6ImportsExpandModule(unit->parse.root, path, &unit->parse.diags, &unit->importDeps);
    }
    if ( ok ) {
        unit->phase = "stdlib";
        ok = xl6StdlibExpandModule(unit->parse.root, &unit->parse.diags);
    }
    if ( ok && runSem ) {
        unit->phase = "sem";
        ok = xl6SemCheckModuleWithQuery(unit->parse.root, &unit->parse.diags, unit->moduleName,
            unit->importDeps.sourceOverlay && unit->importDeps.sourceOverlay->semanticIndex,
            unit->importDeps.sourceOverlay ? unit->importDeps.sourceOverlay->completionQuery : NULL);
        unit->semChecked = 1;
    }
    if ( ok && runSem && !(unit->importDeps.sourceOverlay && unit->importDeps.sourceOverlay->completionQuery) ) {
        unit->phase = "ir";
        ok = xl6IrBuildModuleFromAst(&unit->ir, unit->parse.root, &unit->parse.diags);
        unit->irReady = ok;
    }
    unit->success = ok && unit->parse.diags.count == 0;
    if ( unit->success ) {
        unit->phase = "checked";
    }
    return unit->success;
}
