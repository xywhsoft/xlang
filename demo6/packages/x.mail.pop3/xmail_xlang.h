#ifndef XMAIL_XLANG_H
#define XMAIL_XLANG_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) || defined(_WIN64)
#define XMAIL_XLANG_API __declspec(dllexport)
#else
#define XMAIL_XLANG_API __attribute__((visibility("default")))
#endif

XMAIL_XLANG_API const char* xmail_mime_build_json_native(const char* sMessageJson);
XMAIL_XLANG_API const char* xmail_mime_parse_json_native(const char* sRawMessage);
XMAIL_XLANG_API const char* xmail_smtp_capability_json_native(const char* sConfigJson);
XMAIL_XLANG_API const char* xmail_smtp_send_json_native(const char* sRequestJson);
XMAIL_XLANG_API const char* xmail_pop3_capability_json_native(const char* sConfigJson);
XMAIL_XLANG_API const char* xmail_pop3_status_json_native(const char* sConfigJson);
XMAIL_XLANG_API const char* xmail_pop3_list_json_native(const char* sConfigJson);
XMAIL_XLANG_API const char* xmail_pop3_fetch_json_native(const char* sRequestJson);
XMAIL_XLANG_API const char* xmail_pop3_delete_json_native(const char* sRequestJson);
XMAIL_XLANG_API const char* xmail_imap_capability_json_native(const char* sRequestJson);
XMAIL_XLANG_API const char* xmail_imap_status_json_native(const char* sRequestJson);
XMAIL_XLANG_API const char* xmail_imap_list_json_native(const char* sRequestJson);
XMAIL_XLANG_API const char* xmail_imap_search_json_native(const char* sRequestJson);
XMAIL_XLANG_API const char* xmail_imap_fetch_json_native(const char* sRequestJson);
XMAIL_XLANG_API const char* xmail_imap_bodystructure_json_native(const char* sRequestJson);
XMAIL_XLANG_API const char* xmail_imap_fetch_flags_json_native(const char* sRequestJson);
XMAIL_XLANG_API const char* xmail_imap_store_flags_json_native(const char* sRequestJson);
XMAIL_XLANG_API const char* xmail_imap_expunge_json_native(const char* sRequestJson);
XMAIL_XLANG_API const char* xmail_imap_idle_probe_json_native(const char* sRequestJson);
XMAIL_XLANG_API const char* xmail_imap_idle_json_native(const char* sRequestJson);
XMAIL_XLANG_API const char* xmail_imap_watch_json_native(const char* sRequestJson);

#ifdef __cplusplus
}
#endif

#endif
