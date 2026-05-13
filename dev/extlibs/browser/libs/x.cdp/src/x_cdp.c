#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "xrt.h"

#define XCDP_VERSION "0.1.0"
#define XCDP_MAGIC UINT32_C(0x58434450)
#define XCDP_DEFAULT_CONNECT_TIMEOUT_MS 5000u
#define XCDP_DEFAULT_SEND_TIMEOUT_MS 30000u
#define XCDP_DEFAULT_HTTP_TIMEOUT_MS 5000u
#define XCDP_QUEUE_CAPACITY 1024u
#define XCDP_RECV_LIMIT (16u * 1024u * 1024u)

typedef struct xcdp_msg {
	char* text;
	size_t len;
	struct xcdp_msg* next;
} xcdp_msg;

typedef struct xcdp_conn {
	uint32_t magic;
	xnetengine* engine;
	xwsclient* client;
	xmpscqwait incoming;
	xmpscqwait events;
	xmutex lock;
	volatile int open;
	volatile int closed;
	volatile int closing;
	int next_id;
	char* last_return;
	char last_error[512];
	xcdp_msg* pending_head;
	xcdp_msg* pending_tail;
} xcdp_conn;

int x_cdp_dispose(void* handle);
const char* x_cdp_runtime_evaluate(void* handle, const char* session_id, const char* expression, int timeout_ms);
const char* x_cdp_wait_event(void* handle, const char* session_id, const char* method, int timeout_ms);
int x_cdp_page_failure_artifacts(void* handle, const char* session_id, const char* screenshot_path, const char* snapshot_path, int timeout_ms);
void* x_cdp_connect_http(const char* endpoint, int timeout_ms);
int x_cdp_input_dispatch_key_event(void* handle, const char* session_id, const char* type, const char* key, const char* text, int timeout_ms);
int x_cdp_input_insert_text(void* handle, const char* session_id, const char* text, int timeout_ms);

static xmutex g_xcdp_lock = NULL;
static char* g_xcdp_return = NULL;
static char g_xcdp_error[512] = "";

static void xcdp_global_init(void)
{
	if ( g_xcdp_lock == NULL ) {
		g_xcdp_lock = xrtMutexCreate();
	}
}

static uint32_t xcdp_timeout_or_default(int timeout_ms, uint32_t fallback)
{
	if ( timeout_ms <= 0 ) {
		return fallback;
	}
	return (uint32_t)timeout_ms;
}

static char* xcdp_strndup_xrt(const char* text, size_t len)
{
	char* copy;
	if ( text == NULL ) {
		len = 0u;
	}
	copy = (char*)xrtMalloc(len + 1u);
	if ( copy == NULL ) {
		return NULL;
	}
	if ( len > 0u ) {
		memcpy(copy, text, len);
	}
	copy[len] = '\0';
	return copy;
}

static void xcdp_set_global_error_text(const char* text)
{
	xcdp_global_init();
	if ( g_xcdp_lock ) {
		xrtMutexLock(g_xcdp_lock);
	}
	snprintf(g_xcdp_error, sizeof(g_xcdp_error), "%s", text ? text : "");
	if ( g_xcdp_lock ) {
		xrtMutexUnlock(g_xcdp_lock);
	}
}

static void xcdp_set_global_errorf(const char* fmt, ...)
{
	va_list ap;
	char buf[512];
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	xcdp_set_global_error_text(buf);
}

static const char* xcdp_set_global_return_copy(const char* text, size_t len)
{
	char* copy = xcdp_strndup_xrt(text, len);
	xcdp_global_init();
	if ( copy == NULL ) {
		xcdp_set_global_error_text("out of memory");
		return "";
	}
	if ( g_xcdp_lock ) {
		xrtMutexLock(g_xcdp_lock);
	}
	if ( g_xcdp_return ) {
		xrtFree(g_xcdp_return);
	}
	g_xcdp_return = copy;
	if ( g_xcdp_lock ) {
		xrtMutexUnlock(g_xcdp_lock);
	}
	return g_xcdp_return;
}

static void xcdp_set_conn_error_text(xcdp_conn* conn, const char* text)
{
	if ( conn && conn->lock ) {
		xrtMutexLock(conn->lock);
		snprintf(conn->last_error, sizeof(conn->last_error), "%s", text ? text : "");
		xrtMutexUnlock(conn->lock);
	}
	xcdp_set_global_error_text(text);
}

static void xcdp_set_conn_errorf(xcdp_conn* conn, const char* fmt, ...)
{
	va_list ap;
	char buf[512];
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	xcdp_set_conn_error_text(conn, buf);
}

static const char* xcdp_set_conn_return_owned(xcdp_conn* conn, char* text)
{
	if ( conn == NULL || text == NULL ) {
		return "";
	}
	if ( conn->lock ) {
		xrtMutexLock(conn->lock);
	}
	if ( conn->last_return ) {
		xrtFree(conn->last_return);
	}
	conn->last_return = text;
	if ( conn->lock ) {
		xrtMutexUnlock(conn->lock);
	}
	return conn->last_return;
}

static xcdp_conn* xcdp_checked_conn(void* handle)
{
	xcdp_conn* conn = (xcdp_conn*)handle;
	if ( conn == NULL || conn->magic != XCDP_MAGIC ) {
		xcdp_set_global_error_text("invalid x.cdp connection");
		return NULL;
	}
	return conn;
}

static xcdp_msg* xcdp_msg_create(const char* text, size_t len)
{
	xcdp_msg* msg;
	if ( text == NULL ) {
		return NULL;
	}
	msg = (xcdp_msg*)xrtMalloc(sizeof(xcdp_msg));
	if ( msg == NULL ) {
		return NULL;
	}
	memset(msg, 0, sizeof(*msg));
	msg->text = xcdp_strndup_xrt(text, len);
	if ( msg->text == NULL ) {
		xrtFree(msg);
		return NULL;
	}
	msg->len = len;
	return msg;
}

static void xcdp_msg_destroy(xcdp_msg* msg)
{
	if ( msg == NULL ) {
		return;
	}
	if ( msg->text ) {
		xrtFree(msg->text);
	}
	xrtFree(msg);
}

static int xcdp_queue_push_msg(xmpscqwait queue, xcdp_msg* msg)
{
	if ( queue == NULL || msg == NULL ) {
		xcdp_msg_destroy(msg);
		return 0;
	}
	msg->next = NULL;
	if ( xrtMPSCQWaitTryPush(queue, msg) != XQUEUE_OK ) {
		xcdp_msg_destroy(msg);
		return 0;
	}
	return 1;
}

static int xcdp_queue_pop_msg(xmpscqwait queue, xcdp_msg** out, uint32_t timeout_ms)
{
	ptr item = NULL;
	xqueue_result result;
	double deadline;
	if ( out == NULL ) {
		return 0;
	}
	*out = NULL;
	if ( queue == NULL ) {
		return 0;
	}
	if ( timeout_ms == 0u ) {
		result = xrtMPSCQWaitTryPop(queue, &item);
		if ( result != XQUEUE_OK || item == NULL ) {
			return 0;
		}
		*out = (xcdp_msg*)item;
		return 1;
	}
	deadline = xrtTimer() + ((double)timeout_ms / 1000.0);
	while ( xrtTimer() < deadline ) {
		result = xrtMPSCQWaitTryPop(queue, &item);
		if ( result == XQUEUE_OK && item != NULL ) {
			*out = (xcdp_msg*)item;
			return 1;
		}
		if ( result == XQUEUE_CLOSED || result == XQUEUE_ERROR ) {
			return 0;
		}
		xrtSleep(5u);
	}
	return 0;
}

static void xcdp_queue_drain(xmpscqwait queue)
{
	xcdp_msg* msg = NULL;
	if ( queue == NULL ) {
		return;
	}
	while ( xcdp_queue_pop_msg(queue, &msg, 0u) ) {
		xcdp_msg_destroy(msg);
	}
}

static void xcdp_pending_push(xcdp_conn* conn, xcdp_msg* msg)
{
	if ( conn == NULL || msg == NULL ) {
		xcdp_msg_destroy(msg);
		return;
	}
	msg->next = NULL;
	if ( conn->lock ) {
		xrtMutexLock(conn->lock);
	}
	if ( conn->pending_tail ) {
		conn->pending_tail->next = msg;
	} else {
		conn->pending_head = msg;
	}
	conn->pending_tail = msg;
	if ( conn->lock ) {
		xrtMutexUnlock(conn->lock);
	}
}

static xcdp_msg* xcdp_pending_take_id(xcdp_conn* conn, int id)
{
	xcdp_msg* prev = NULL;
	xcdp_msg* cur = NULL;
	if ( conn == NULL ) {
		return NULL;
	}
	if ( conn->lock ) {
		xrtMutexLock(conn->lock);
	}
	cur = conn->pending_head;
	while ( cur ) {
		int msg_id = 0;
		int has_id = 0;
		xvalue root = xrtParseJSON(cur->text, cur->len);
		if ( xvoIsTable(root) ) {
			xvalue id_val = xvoTableGetValue(root, "id", 2u);
			if ( xvoIsInt(id_val) ) {
				msg_id = (int)xvoGetInt(id_val);
				has_id = 1;
			}
		}
		xvoUnref(root);
		if ( has_id && msg_id == id ) {
			if ( prev ) {
				prev->next = cur->next;
			} else {
				conn->pending_head = cur->next;
			}
			if ( conn->pending_tail == cur ) {
				conn->pending_tail = prev;
			}
			cur->next = NULL;
			if ( conn->lock ) {
				xrtMutexUnlock(conn->lock);
			}
			return cur;
		}
		prev = cur;
		cur = cur->next;
	}
	if ( conn->lock ) {
		xrtMutexUnlock(conn->lock);
	}
	return NULL;
}

static void xcdp_pending_drain(xcdp_conn* conn)
{
	xcdp_msg* cur;
	if ( conn == NULL ) {
		return;
	}
	if ( conn->lock ) {
		xrtMutexLock(conn->lock);
	}
	cur = conn->pending_head;
	conn->pending_head = NULL;
	conn->pending_tail = NULL;
	if ( conn->lock ) {
		xrtMutexUnlock(conn->lock);
	}
	while ( cur ) {
		xcdp_msg* next = cur->next;
		cur->next = NULL;
		xcdp_msg_destroy(cur);
		cur = next;
	}
}

static int xcdp_message_id(const char* text, size_t len, int* out_id)
{
	int ok = 0;
	xvalue root;
	if ( out_id ) {
		*out_id = 0;
	}
	if ( text == NULL ) {
		return 0;
	}
	root = xrtParseJSON((str)text, len);
	if ( xvoIsTable(root) ) {
		xvalue id_val = xvoTableGetValue(root, "id", 2u);
		if ( xvoIsInt(id_val) ) {
			if ( out_id ) {
				*out_id = (int)xvoGetInt(id_val);
			}
			ok = 1;
		}
	}
	xvoUnref(root);
	return ok;
}

static int xcdp_message_text_field_equals(const char* text, size_t len, const char* field, uint32_t field_len, const char* expected)
{
	xvalue root = NULL;
	xvalue value = NULL;
	int ok = 0;
	if ( expected == NULL || expected[0] == '\0' ) {
		return 1;
	}
	if ( text == NULL || len == 0u || field == NULL || field_len == 0u ) {
		return 0;
	}
	root = xrtParseJSON((str)text, len);
	if ( !xvoIsTable(root) ) {
		xvoUnref(root);
		return 0;
	}
	value = xvoTableGetValue(root, field, field_len);
	if ( xvoIsText(value) && xvoGetSize(value) == strlen(expected) && memcmp(xvoGetText(value), expected, xvoGetSize(value)) == 0 ) {
		ok = 1;
	}
	xvoUnref(root);
	return ok;
}

static int xcdp_text_contains(const char* text, size_t text_len, const char* needle)
{
	size_t needle_len;
	size_t i;
	if ( needle == NULL || needle[0] == '\0' ) {
		return 1;
	}
	if ( text == NULL ) {
		return 0;
	}
	needle_len = strlen(needle);
	if ( needle_len == 0u ) {
		return 1;
	}
	if ( text_len < needle_len ) {
		return 0;
	}
	for ( i = 0u; i <= text_len - needle_len; ++i ) {
		if ( memcmp(text + i, needle, needle_len) == 0 ) {
			return 1;
		}
	}
	return 0;
}

static char* xcdp_js_string_literal(xcdp_conn* conn, const char* value)
{
	const unsigned char* src = (const unsigned char*)(value ? value : "");
	size_t len = strlen((const char*)src);
	char* out = (char*)xrtMalloc((len * 6u) + 3u);
	size_t i;
	size_t j = 0u;
	if ( out == NULL ) {
		xcdp_set_conn_error_text(conn, "out of memory while building JS literal");
		return NULL;
	}
	out[j++] = '"';
	for ( i = 0u; i < len; ++i ) {
		unsigned char ch = src[i];
		switch ( ch ) {
		case '\\': out[j++] = '\\'; out[j++] = '\\'; break;
		case '"': out[j++] = '\\'; out[j++] = '"'; break;
		case '\n': out[j++] = '\\'; out[j++] = 'n'; break;
		case '\r': out[j++] = '\\'; out[j++] = 'r'; break;
		case '\t': out[j++] = '\\'; out[j++] = 't'; break;
		default:
			if ( ch < 0x20u ) {
				static const char* hex = "0123456789abcdef";
				out[j++] = '\\';
				out[j++] = 'u';
				out[j++] = '0';
				out[j++] = '0';
				out[j++] = hex[(ch >> 4) & 0x0Fu];
				out[j++] = hex[ch & 0x0Fu];
			} else {
				out[j++] = (char)ch;
			}
			break;
		}
	}
	out[j++] = '"';
	out[j] = '\0';
	return out;
}

static int xcdp_eval_bool(const char* value)
{
	return value && (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
}

static int xcdp_poll_bool_expression(xcdp_conn* conn, const char* session_id, const char* expression, int timeout_ms)
{
	uint32_t timeout = timeout_ms < 0 ? XCDP_DEFAULT_SEND_TIMEOUT_MS : (uint32_t)timeout_ms;
	double deadline = xrtTimer() + ((double)timeout / 1000.0);
	do {
		int eval_timeout = 100;
		const char* value;
		double now = xrtTimer();
		if ( timeout > 0u && now >= deadline ) {
			break;
		}
		if ( timeout > 0u ) {
			double remain_ms = (deadline - now) * 1000.0;
			if ( remain_ms < 100.0 ) {
				eval_timeout = (int)remain_ms;
				if ( eval_timeout < 1 ) {
					eval_timeout = 1;
				}
			}
		}
		value = x_cdp_runtime_evaluate(conn, session_id, expression, eval_timeout);
		if ( xcdp_eval_bool(value) ) {
			return 1;
		}
		if ( timeout == 0u ) {
			break;
		}
		xrtSleep(25u);
	} while ( timeout > 0u );
	return 0;
}

static int xcdp_message_event_matches(const char* text, size_t len, const char* session_id, const char* method)
{
	if ( method != NULL && method[0] != '\0' && !xcdp_message_text_field_equals(text, len, "method", 6u, method) ) {
		return 0;
	}
	if ( session_id != NULL && session_id[0] != '\0' && !xcdp_message_text_field_equals(text, len, "sessionId", 9u, session_id) ) {
		return 0;
	}
	return 1;
}

static int xcdp_message_event_method_starts_with(const char* text, size_t len, const char* session_id, const char* prefix)
{
	xvalue root = NULL;
	xvalue value = NULL;
	size_t prefix_len;
	int ok = 0;
	if ( prefix == NULL || prefix[0] == '\0' ) {
		return xcdp_message_event_matches(text, len, session_id, NULL);
	}
	if ( session_id != NULL && session_id[0] != '\0' && !xcdp_message_text_field_equals(text, len, "sessionId", 9u, session_id) ) {
		return 0;
	}
	root = xrtParseJSON((str)text, len);
	if ( !xvoIsTable(root) ) {
		xvoUnref(root);
		return 0;
	}
	value = xvoTableGetValue(root, "method", 6u);
	prefix_len = strlen(prefix);
	if ( xvoIsText(value) && xvoGetSize(value) >= prefix_len && memcmp(xvoGetText(value), prefix, prefix_len) == 0 ) {
		ok = 1;
	}
	xvoUnref(root);
	return ok;
}

static int xcdp_message_network_url_matches(const char* text, size_t len, const char* session_id, const char* method, const char* url_part)
{
	xvalue root = NULL;
	xvalue params = NULL;
	xvalue holder = NULL;
	xvalue url = NULL;
	int ok = 0;
	if ( !xcdp_message_event_matches(text, len, session_id, method) ) {
		return 0;
	}
	if ( url_part == NULL || url_part[0] == '\0' ) {
		return 1;
	}
	root = xrtParseJSON((str)text, len);
	if ( !xvoIsTable(root) ) {
		xvoUnref(root);
		return 0;
	}
	params = xvoTableGetValue(root, "params", 6u);
	if ( xvoIsTable(params) ) {
		holder = xvoTableGetValue(params, "request", 7u);
		if ( !xvoIsTable(holder) ) {
			holder = xvoTableGetValue(params, "response", 8u);
		}
		if ( xvoIsTable(holder) ) {
			url = xvoTableGetValue(holder, "url", 3u);
		}
	}
	if ( xvoIsText(url) ) {
		ok = xcdp_text_contains(xvoGetText(url), xvoGetSize(url), url_part);
	}
	xvoUnref(root);
	return ok;
}

static int xcdp_append_jsonl_object(xcdp_conn* conn, const char* path, const char* json_object)
{
	xvalue root = NULL;
	char* line = NULL;
	size_t line_len = 0u;
	FILE* fp = NULL;
	int ok = 0;
	if ( path == NULL || path[0] == '\0' ) {
		xcdp_set_conn_error_text(conn, "JSONL output path is empty");
		return 0;
	}
	if ( json_object == NULL || json_object[0] == '\0' ) {
		xcdp_set_conn_error_text(conn, "JSONL object is empty");
		return 0;
	}
	root = xrtParseJSON((str)json_object, strlen(json_object));
	if ( !xvoIsTable(root) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "JSONL object must be a JSON object");
		return 0;
	}
	line = xrtStringifyJSON(root, 0, &line_len);
	xvoUnref(root);
	if ( line == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to stringify JSONL object");
		return 0;
	}
	fp = fopen(path, "ab");
	if ( fp == NULL ) {
		xrtFree(line);
		xcdp_set_conn_error_text(conn, "failed to open JSONL output path");
		return 0;
	}
	ok = (fwrite(line, 1u, line_len, fp) == line_len && fwrite("\n", 1u, 1u, fp) == 1u) ? 1 : 0;
	fclose(fp);
	xrtFree(line);
	if ( !ok ) {
		xcdp_set_conn_error_text(conn, "failed to append JSONL object");
	}
	return ok;
}

static int xcdp_write_json_object(xcdp_conn* conn, const char* path, const char* json_object)
{
	xvalue root = NULL;
	char* text = NULL;
	size_t text_len = 0u;
	int ok;
	if ( path == NULL || path[0] == '\0' ) {
		xcdp_set_conn_error_text(conn, "JSON output path is empty");
		return 0;
	}
	if ( json_object == NULL || json_object[0] == '\0' ) {
		xcdp_set_conn_error_text(conn, "JSON object is empty");
		return 0;
	}
	root = xrtParseJSON((str)json_object, strlen(json_object));
	if ( !xvoIsTable(root) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "JSON value must be an object");
		return 0;
	}
	text = xrtStringifyJSON(root, 1, &text_len);
	xvoUnref(root);
	if ( text == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to stringify JSON object");
		return 0;
	}
	ok = xrtFilePutAll((str)path, (ptr)text, text_len) ? 1 : 0;
	xrtFree(text);
	if ( !ok ) {
		xcdp_set_conn_error_text(conn, "failed to write JSON output path");
	}
	return ok;
}

static int xcdp_endpoint_has_suffix(const char* endpoint, const char* suffix)
{
	size_t endpoint_len;
	size_t suffix_len;
	if ( endpoint == NULL || suffix == NULL ) {
		return 0;
	}
	endpoint_len = strlen(endpoint);
	suffix_len = strlen(suffix);
	if ( endpoint_len < suffix_len ) {
		return 0;
	}
	return strcmp(endpoint + endpoint_len - suffix_len, suffix) == 0;
}

static const char* xcdp_make_http_url(const char* endpoint, const char* suffix)
{
	size_t endpoint_len;
	size_t suffix_len;
	size_t extra_slash;
	char* out;
	if ( endpoint == NULL || endpoint[0] == '\0' ) {
		xcdp_set_global_error_text("empty browser endpoint");
		return "";
	}
	if ( strncmp(endpoint, "ws://", 5u) == 0 || strncmp(endpoint, "wss://", 6u) == 0 ) {
		xcdp_set_global_error_text("HTTP bootstrap URL requires http:// or https:// endpoint");
		return "";
	}
	if ( xcdp_endpoint_has_suffix(endpoint, suffix) ) {
		return xcdp_set_global_return_copy(endpoint, strlen(endpoint));
	}
	endpoint_len = strlen(endpoint);
	suffix_len = strlen(suffix);
	extra_slash = (endpoint_len > 0u && endpoint[endpoint_len - 1u] == '/') ? 0u : 1u;
	out = (char*)xrtMalloc(endpoint_len + extra_slash + suffix_len + 1u);
	if ( out == NULL ) {
		xcdp_set_global_error_text("out of memory");
		return "";
	}
	memcpy(out, endpoint, endpoint_len);
	if ( extra_slash ) {
		out[endpoint_len] = '/';
	}
	memcpy(out + endpoint_len + extra_slash, suffix, suffix_len);
	out[endpoint_len + extra_slash + suffix_len] = '\0';
	if ( g_xcdp_lock ) {
		xrtMutexLock(g_xcdp_lock);
	}
	if ( g_xcdp_return ) {
		xrtFree(g_xcdp_return);
	}
	g_xcdp_return = out;
	if ( g_xcdp_lock ) {
		xrtMutexUnlock(g_xcdp_lock);
	}
	return g_xcdp_return;
}

static void xcdp_ws_on_open(ptr owner, xwsclient* client)
{
	xcdp_conn* conn = (xcdp_conn*)owner;
	(void)client;
	if ( conn == NULL || conn->magic != XCDP_MAGIC ) {
		return;
	}
	conn->open = 1;
	conn->closed = 0;
	if ( conn->lock ) {
		xrtMutexLock(conn->lock);
		conn->last_error[0] = '\0';
		xrtMutexUnlock(conn->lock);
	}
}

static void xcdp_ws_on_text(ptr owner, xwsclient* client, const char* data, size_t len)
{
	xcdp_conn* conn = (xcdp_conn*)owner;
	xcdp_msg* msg;
	(void)client;
	if ( conn == NULL || conn->magic != XCDP_MAGIC || conn->closing ) {
		return;
	}
	msg = xcdp_msg_create(data, len);
	if ( msg == NULL ) {
		xcdp_set_conn_error_text(conn, "out of memory while receiving CDP message");
		return;
	}
	xcdp_queue_push_msg(conn->incoming, msg);
}

static void xcdp_ws_on_close(ptr owner, xwsclient* client, xnet_result reason)
{
	xcdp_conn* conn = (xcdp_conn*)owner;
	(void)client;
	if ( conn == NULL || conn->magic != XCDP_MAGIC ) {
		return;
	}
	conn->open = 0;
	conn->closed = 1;
	if ( !conn->closing ) {
		xcdp_set_conn_errorf(conn, "WebSocket closed: %d", (int)reason);
	}
}

static void xcdp_ws_on_error(ptr owner, xwsclient* client, int sys_err)
{
	xcdp_conn* conn = (xcdp_conn*)owner;
	(void)client;
	if ( conn == NULL || conn->magic != XCDP_MAGIC ) {
		return;
	}
	conn->open = 0;
	xcdp_set_conn_errorf(conn, "WebSocket error: %d", sys_err);
}

static char* xcdp_build_command_json(xcdp_conn* conn, int id, const char* session_id, const char* method, const char* params_json, size_t* out_len)
{
	xvalue root = NULL;
	xvalue params = NULL;
	char* json_text = NULL;
	size_t json_len = 0u;
	if ( out_len ) {
		*out_len = 0u;
	}
	if ( method == NULL || method[0] == '\0' ) {
		xcdp_set_conn_error_text(conn, "CDP method is empty");
		return NULL;
	}
	if ( params_json != NULL && params_json[0] != '\0' ) {
		params = xrtParseJSON((str)params_json, strlen(params_json));
		if ( !xvoIsTable(params) ) {
			xvoUnref(params);
			xcdp_set_conn_error_text(conn, "CDP params_json must be a JSON object");
			return NULL;
		}
	} else {
		params = xvoCreateTable();
	}
	root = xvoCreateTable();
	if ( root == NULL || params == NULL ) {
		xvoUnref(root);
		xvoUnref(params);
		xcdp_set_conn_error_text(conn, "out of memory while building CDP command");
		return NULL;
	}
	if ( !xvoTableSetInt(root, "id", 2u, id) ||
		!xvoTableSetText(root, "method", 6u, (ptr)method, (uint32)strlen(method), FALSE) ||
		!xvoTableSetValue(root, "params", 6u, params, TRUE) ) {
		xvoUnref(root);
		xvoUnref(params);
		xcdp_set_conn_error_text(conn, "failed to build CDP command object");
		return NULL;
	}
	params = NULL;
	if ( session_id != NULL && session_id[0] != '\0' ) {
		if ( !xvoTableSetText(root, "sessionId", 9u, (ptr)session_id, (uint32)strlen(session_id), FALSE) ) {
			xvoUnref(root);
			xcdp_set_conn_error_text(conn, "failed to set CDP sessionId");
			return NULL;
		}
	}
	json_text = xrtStringifyJSON(root, 0, &json_len);
	xvoUnref(root);
	if ( json_text == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to stringify CDP command JSON");
		return NULL;
	}
	if ( out_len ) {
		*out_len = json_len;
	}
	return json_text;
}

static char* xcdp_build_target_params(xcdp_conn* conn, const char* key, uint32_t key_len, const char* value)
{
	xvalue root = NULL;
	char* json_text = NULL;
	size_t json_len = 0u;
	if ( key == NULL || key_len == 0u || value == NULL || value[0] == '\0' ) {
		xcdp_set_conn_error_text(conn, "CDP target parameter is empty");
		return NULL;
	}
	root = xvoCreateTable();
	if ( root == NULL ) {
		xcdp_set_conn_error_text(conn, "out of memory while building CDP target params");
		return NULL;
	}
	if ( !xvoTableSetText(root, key, key_len, (ptr)value, (uint32)strlen(value), FALSE) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "failed to build CDP target params");
		return NULL;
	}
	json_text = xrtStringifyJSON(root, 0, &json_len);
	xvoUnref(root);
	(void)json_len;
	if ( json_text == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to stringify CDP target params");
	}
	return json_text;
}

static char* xcdp_build_create_target_params(xcdp_conn* conn, const char* url, const char* browser_context_id)
{
	xvalue root = NULL;
	char* json_text = NULL;
	size_t json_len = 0u;
	const char* target_url = (url && url[0]) ? url : "about:blank";
	root = xvoCreateTable();
	if ( root == NULL ) {
		xcdp_set_conn_error_text(conn, "out of memory while building CDP create target params");
		return NULL;
	}
	if ( !xvoTableSetText(root, "url", 3u, (ptr)target_url, (uint32)strlen(target_url), FALSE) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "failed to build CDP create target params");
		return NULL;
	}
	if ( browser_context_id != NULL && browser_context_id[0] != '\0' ) {
		if ( !xvoTableSetText(root, "browserContextId", 16u, (ptr)browser_context_id, (uint32)strlen(browser_context_id), FALSE) ) {
			xvoUnref(root);
			xcdp_set_conn_error_text(conn, "failed to set CDP browserContextId");
			return NULL;
		}
	}
	json_text = xrtStringifyJSON(root, 0, &json_len);
	xvoUnref(root);
	(void)json_len;
	if ( json_text == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to stringify CDP create target params");
	}
	return json_text;
}

static char* xcdp_build_attach_target_params(xcdp_conn* conn, const char* target_id, int flatten)
{
	xvalue root = NULL;
	char* json_text = NULL;
	size_t json_len = 0u;
	if ( target_id == NULL || target_id[0] == '\0' ) {
		xcdp_set_conn_error_text(conn, "CDP targetId is empty");
		return NULL;
	}
	root = xvoCreateTable();
	if ( root == NULL ) {
		xcdp_set_conn_error_text(conn, "out of memory while building CDP attach params");
		return NULL;
	}
	if ( !xvoTableSetText(root, "targetId", 8u, (ptr)target_id, (uint32)strlen(target_id), FALSE) ||
		!xvoTableSetBool(root, "flatten", 7u, flatten ? TRUE : FALSE) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "failed to build CDP attach params");
		return NULL;
	}
	json_text = xrtStringifyJSON(root, 0, &json_len);
	xvoUnref(root);
	(void)json_len;
	if ( json_text == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to stringify CDP attach params");
	}
	return json_text;
}

static char* xcdp_build_target_discover_params(xcdp_conn* conn, int discover)
{
	xvalue root = NULL;
	char* json_text = NULL;
	size_t json_len = 0u;
	root = xvoCreateTable();
	if ( root == NULL ) {
		xcdp_set_conn_error_text(conn, "out of memory while building Target.setDiscoverTargets params");
		return NULL;
	}
	if ( !xvoTableSetBool(root, "discover", 8u, discover ? TRUE : FALSE) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "failed to build Target.setDiscoverTargets params");
		return NULL;
	}
	json_text = xrtStringifyJSON(root, 0, &json_len);
	xvoUnref(root);
	(void)json_len;
	if ( json_text == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to stringify Target.setDiscoverTargets params");
	}
	return json_text;
}

static char* xcdp_build_optional_browser_context_params(xcdp_conn* conn, const char* browser_context_id)
{
	xvalue root = NULL;
	char* json_text = NULL;
	size_t json_len = 0u;
	root = xvoCreateTable();
	if ( root == NULL ) {
		xcdp_set_conn_error_text(conn, "out of memory while building browserContextId params");
		return NULL;
	}
	if ( browser_context_id != NULL && browser_context_id[0] != '\0' ) {
		if ( !xvoTableSetText(root, "browserContextId", 16u, (ptr)browser_context_id, (uint32)strlen(browser_context_id), FALSE) ) {
			xvoUnref(root);
			xcdp_set_conn_error_text(conn, "failed to set browserContextId params");
			return NULL;
		}
	}
	json_text = xrtStringifyJSON(root, 0, &json_len);
	xvoUnref(root);
	(void)json_len;
	if ( json_text == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to stringify browserContextId params");
	}
	return json_text;
}

static char* xcdp_build_grant_permissions_params(xcdp_conn* conn, const char* browser_context_id, const char* permissions_json, const char* origin)
{
	xvalue root = NULL;
	xvalue permissions = NULL;
	char* json_text = NULL;
	size_t json_len = 0u;
	if ( permissions_json == NULL || permissions_json[0] == '\0' ) {
		xcdp_set_conn_error_text(conn, "permissions JSON is empty");
		return NULL;
	}
	permissions = xrtParseJSON((str)permissions_json, strlen(permissions_json));
	if ( !xvoIsArray(permissions) ) {
		xvoUnref(permissions);
		xcdp_set_conn_error_text(conn, "permissions JSON must be an array");
		return NULL;
	}
	root = xvoCreateTable();
	if ( root == NULL ) {
		xvoUnref(permissions);
		xcdp_set_conn_error_text(conn, "out of memory while building Browser.grantPermissions params");
		return NULL;
	}
	if ( !xvoTableSetValue(root, "permissions", 11u, permissions, TRUE) ) {
		xvoUnref(root);
		xvoUnref(permissions);
		xcdp_set_conn_error_text(conn, "failed to set Browser.grantPermissions permissions");
		return NULL;
	}
	permissions = NULL;
	if ( browser_context_id != NULL && browser_context_id[0] != '\0' ) {
		if ( !xvoTableSetText(root, "browserContextId", 16u, (ptr)browser_context_id, (uint32)strlen(browser_context_id), FALSE) ) {
			xvoUnref(root);
			xcdp_set_conn_error_text(conn, "failed to set Browser.grantPermissions browserContextId");
			return NULL;
		}
	}
	if ( origin != NULL && origin[0] != '\0' ) {
		if ( !xvoTableSetText(root, "origin", 6u, (ptr)origin, (uint32)strlen(origin), FALSE) ) {
			xvoUnref(root);
			xcdp_set_conn_error_text(conn, "failed to set Browser.grantPermissions origin");
			return NULL;
		}
	}
	json_text = xrtStringifyJSON(root, 0, &json_len);
	xvoUnref(root);
	(void)json_len;
	if ( json_text == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to stringify Browser.grantPermissions params");
	}
	return json_text;
}

static char* xcdp_build_set_cookies_params(xcdp_conn* conn, const char* browser_context_id, const char* cookies_json)
{
	xvalue root = NULL;
	xvalue cookies = NULL;
	char* json_text = NULL;
	size_t json_len = 0u;
	if ( cookies_json == NULL || cookies_json[0] == '\0' ) {
		xcdp_set_conn_error_text(conn, "cookies JSON is empty");
		return NULL;
	}
	cookies = xrtParseJSON((str)cookies_json, strlen(cookies_json));
	if ( !xvoIsArray(cookies) ) {
		xvoUnref(cookies);
		xcdp_set_conn_error_text(conn, "cookies JSON must be an array");
		return NULL;
	}
	root = xvoCreateTable();
	if ( root == NULL ) {
		xvoUnref(cookies);
		xcdp_set_conn_error_text(conn, "out of memory while building Storage.setCookies params");
		return NULL;
	}
	if ( !xvoTableSetValue(root, "cookies", 7u, cookies, TRUE) ) {
		xvoUnref(root);
		xvoUnref(cookies);
		xcdp_set_conn_error_text(conn, "failed to set Storage.setCookies cookies");
		return NULL;
	}
	cookies = NULL;
	if ( browser_context_id != NULL && browser_context_id[0] != '\0' ) {
		if ( !xvoTableSetText(root, "browserContextId", 16u, (ptr)browser_context_id, (uint32)strlen(browser_context_id), FALSE) ) {
			xvoUnref(root);
			xcdp_set_conn_error_text(conn, "failed to set Storage.setCookies browserContextId");
			return NULL;
		}
	}
	json_text = xrtStringifyJSON(root, 0, &json_len);
	xvoUnref(root);
	(void)json_len;
	if ( json_text == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to stringify Storage.setCookies params");
	}
	return json_text;
}

static char* xcdp_build_download_behavior_params(xcdp_conn* conn, const char* behavior, const char* download_path)
{
	xvalue root = NULL;
	char* json_text = NULL;
	size_t json_len = 0u;
	if ( behavior == NULL || behavior[0] == '\0' ) {
		xcdp_set_conn_error_text(conn, "download behavior is empty");
		return NULL;
	}
	root = xvoCreateTable();
	if ( root == NULL ) {
		xcdp_set_conn_error_text(conn, "out of memory while building Browser.setDownloadBehavior params");
		return NULL;
	}
	if ( !xvoTableSetText(root, "behavior", 8u, (ptr)behavior, (uint32)strlen(behavior), FALSE) ||
		!xvoTableSetBool(root, "eventsEnabled", 13u, TRUE) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "failed to build Browser.setDownloadBehavior params");
		return NULL;
	}
	if ( download_path != NULL && download_path[0] != '\0' ) {
		if ( !xvoTableSetText(root, "downloadPath", 12u, (ptr)download_path, (uint32)strlen(download_path), FALSE) ) {
			xvoUnref(root);
			xcdp_set_conn_error_text(conn, "failed to set Browser.setDownloadBehavior downloadPath");
			return NULL;
		}
	}
	json_text = xrtStringifyJSON(root, 0, &json_len);
	xvoUnref(root);
	(void)json_len;
	if ( json_text == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to stringify Browser.setDownloadBehavior params");
	}
	return json_text;
}

static char* xcdp_build_storage_set_expression(xcdp_conn* conn, const char* storage_name, const char* items_json)
{
	xvalue items = NULL;
	char* json_text = NULL;
	char* expr = NULL;
	size_t json_len = 0u;
	size_t expr_len;
	if ( storage_name == NULL || storage_name[0] == '\0' ) {
		xcdp_set_conn_error_text(conn, "storage name is empty");
		return NULL;
	}
	if ( items_json == NULL || items_json[0] == '\0' ) {
		xcdp_set_conn_error_text(conn, "storage items JSON is empty");
		return NULL;
	}
	items = xrtParseJSON((str)items_json, strlen(items_json));
	if ( !xvoIsTable(items) ) {
		xvoUnref(items);
		xcdp_set_conn_error_text(conn, "storage items JSON must be an object");
		return NULL;
	}
	json_text = xrtStringifyJSON(items, 0, &json_len);
	xvoUnref(items);
	if ( json_text == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to stringify storage items JSON");
		return NULL;
	}
	expr_len = strlen(storage_name) + json_len + 220u;
	expr = (char*)xrtMalloc(expr_len);
	if ( expr == NULL ) {
		xrtFree(json_text);
		xcdp_set_conn_error_text(conn, "out of memory while building storage set expression");
		return NULL;
	}
	snprintf(expr, expr_len, "(function(){var data=%s;var s=%s;s.clear();Object.keys(data).forEach(function(k){s.setItem(k,String(data[k]));});return true;})()", json_text, storage_name);
	xrtFree(json_text);
	return expr;
}

static const char* xcdp_response_result_text(xcdp_conn* conn, const char* response_json, const char* field, uint32_t field_len)
{
	xvalue root = NULL;
	xvalue result = NULL;
	xvalue value = NULL;
	char* copy = NULL;
	if ( response_json == NULL || response_json[0] == '\0' ) {
		return "";
	}
	root = xrtParseJSON((str)response_json, strlen(response_json));
	if ( !xvoIsTable(root) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "invalid CDP response JSON");
		return "";
	}
	result = xvoTableGetValue(root, "result", 6u);
	if ( !xvoIsTable(result) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "CDP response does not contain result object");
		return "";
	}
	value = xvoTableGetValue(result, field, field_len);
	if ( !xvoIsText(value) ) {
		xvoUnref(root);
		xcdp_set_conn_errorf(conn, "CDP response result does not contain %s", field ? field : "");
		return "";
	}
	copy = xcdp_strndup_xrt(xvoGetText(value), xvoGetSize(value));
	xvoUnref(root);
	if ( copy == NULL ) {
		xcdp_set_conn_error_text(conn, "out of memory");
		return "";
	}
	return xcdp_set_conn_return_owned(conn, copy);
}

static int xcdp_response_result_bool(xcdp_conn* conn, const char* response_json, const char* field, uint32_t field_len)
{
	xvalue root = NULL;
	xvalue result = NULL;
	xvalue value = NULL;
	int ok;
	if ( response_json == NULL || response_json[0] == '\0' ) {
		return 0;
	}
	root = xrtParseJSON((str)response_json, strlen(response_json));
	if ( !xvoIsTable(root) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "invalid CDP response JSON");
		return 0;
	}
	result = xvoTableGetValue(root, "result", 6u);
	if ( !xvoIsTable(result) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "CDP response does not contain result object");
		return 0;
	}
	value = xvoTableGetValue(result, field, field_len);
	ok = xvoGetBool(value) ? 1 : 0;
	xvoUnref(root);
	return ok;
}

static int xcdp_response_has_result(xcdp_conn* conn, const char* response_json)
{
	xvalue root = NULL;
	xvalue result = NULL;
	if ( response_json == NULL || response_json[0] == '\0' ) {
		return 0;
	}
	root = xrtParseJSON((str)response_json, strlen(response_json));
	if ( !xvoIsTable(root) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "invalid CDP response JSON");
		return 0;
	}
	result = xvoTableGetValue(root, "result", 6u);
	if ( !xvoIsTable(result) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "CDP response does not contain result object");
		return 0;
	}
	xvoUnref(root);
	return 1;
}

static const char* xcdp_response_result_json(xcdp_conn* conn, const char* response_json)
{
	xvalue root = NULL;
	xvalue result = NULL;
	char* json_text = NULL;
	char* copy = NULL;
	size_t json_len = 0u;
	if ( response_json == NULL || response_json[0] == '\0' ) {
		return "";
	}
	root = xrtParseJSON((str)response_json, strlen(response_json));
	if ( !xvoIsTable(root) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "invalid CDP response JSON");
		return "";
	}
	result = xvoTableGetValue(root, "result", 6u);
	if ( !xvoIsTable(result) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "CDP response does not contain result object");
		return "";
	}
	json_text = xrtStringifyJSON(result, 0, &json_len);
	xvoUnref(root);
	if ( json_text == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to stringify CDP response result");
		return "";
	}
	copy = xcdp_strndup_xrt(json_text, json_len);
	xrtFree(json_text);
	if ( copy == NULL ) {
		xcdp_set_conn_error_text(conn, "out of memory");
		return "";
	}
	return xcdp_set_conn_return_owned(conn, copy);
}

static char* xcdp_build_runtime_evaluate_params(xcdp_conn* conn, const char* expression)
{
	xvalue root = NULL;
	char* json_text = NULL;
	size_t json_len = 0u;
	if ( expression == NULL || expression[0] == '\0' ) {
		xcdp_set_conn_error_text(conn, "Runtime.evaluate expression is empty");
		return NULL;
	}
	root = xvoCreateTable();
	if ( root == NULL ) {
		xcdp_set_conn_error_text(conn, "out of memory while building Runtime.evaluate params");
		return NULL;
	}
	if ( !xvoTableSetText(root, "expression", 10u, (ptr)expression, (uint32)strlen(expression), FALSE) ||
		!xvoTableSetBool(root, "returnByValue", 13u, TRUE) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "failed to build Runtime.evaluate params");
		return NULL;
	}
	json_text = xrtStringifyJSON(root, 0, &json_len);
	xvoUnref(root);
	(void)json_len;
	if ( json_text == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to stringify Runtime.evaluate params");
	}
	return json_text;
}

static char* xcdp_build_dialog_accept_params(xcdp_conn* conn, int accept, const char* prompt_text)
{
	xvalue root = NULL;
	char* json_text = NULL;
	size_t json_len = 0u;
	root = xvoCreateTable();
	if ( root == NULL ) {
		xcdp_set_conn_error_text(conn, "out of memory while building Page.handleJavaScriptDialog params");
		return NULL;
	}
	if ( !xvoTableSetBool(root, "accept", 6u, accept ? TRUE : FALSE) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "failed to build Page.handleJavaScriptDialog params");
		return NULL;
	}
	if ( prompt_text != NULL ) {
		if ( !xvoTableSetText(root, "promptText", 10u, (ptr)prompt_text, (uint32)strlen(prompt_text), FALSE) ) {
			xvoUnref(root);
			xcdp_set_conn_error_text(conn, "failed to build Page.handleJavaScriptDialog prompt text");
			return NULL;
		}
	}
	json_text = xrtStringifyJSON(root, 0, &json_len);
	xvoUnref(root);
	(void)json_len;
	if ( json_text == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to stringify Page.handleJavaScriptDialog params");
	}
	return json_text;
}

static char* xcdp_build_mouse_event_params(xcdp_conn* conn, const char* type, int x, int y, const char* button, int click_count, int delta_x, int delta_y)
{
	xvalue root = NULL;
	char* json_text = NULL;
	size_t json_len = 0u;
	const char* event_type = (type && type[0]) ? type : "mouseMoved";
	const char* mouse_button = (button && button[0]) ? button : "none";
	root = xvoCreateTable();
	if ( root == NULL ) {
		xcdp_set_conn_error_text(conn, "out of memory while building Input.dispatchMouseEvent params");
		return NULL;
	}
	if ( !xvoTableSetText(root, "type", 4u, (ptr)event_type, (uint32)strlen(event_type), FALSE) ||
		!xvoTableSetInt(root, "x", 1u, x) ||
		!xvoTableSetInt(root, "y", 1u, y) ||
		!xvoTableSetText(root, "button", 6u, (ptr)mouse_button, (uint32)strlen(mouse_button), FALSE) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "failed to build Input.dispatchMouseEvent params");
		return NULL;
	}
	if ( click_count > 0 && !xvoTableSetInt(root, "clickCount", 10u, click_count) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "failed to set Input.dispatchMouseEvent clickCount");
		return NULL;
	}
	if ( strcmp(event_type, "mouseWheel") == 0 ) {
		if ( !xvoTableSetInt(root, "deltaX", 6u, delta_x) ||
			!xvoTableSetInt(root, "deltaY", 6u, delta_y) ) {
			xvoUnref(root);
			xcdp_set_conn_error_text(conn, "failed to set Input.dispatchMouseEvent wheel delta");
			return NULL;
		}
	}
	json_text = xrtStringifyJSON(root, 0, &json_len);
	xvoUnref(root);
	(void)json_len;
	if ( json_text == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to stringify Input.dispatchMouseEvent params");
	}
	return json_text;
}

static char* xcdp_build_screencast_params(xcdp_conn* conn, const char* format, int quality, int every_nth_frame)
{
	xvalue root = NULL;
	char* json_text = NULL;
	size_t json_len = 0u;
	root = xvoCreateTable();
	if ( root == NULL ) {
		xcdp_set_conn_error_text(conn, "out of memory while building Page.startScreencast params");
		return NULL;
	}
	if ( format != NULL && format[0] != '\0' ) {
		if ( !xvoTableSetText(root, "format", 6u, (ptr)format, (uint32)strlen(format), FALSE) ) {
			xvoUnref(root);
			xcdp_set_conn_error_text(conn, "failed to set Page.startScreencast format");
			return NULL;
		}
	}
	if ( quality > 0 ) {
		if ( !xvoTableSetInt(root, "quality", 7u, quality) ) {
			xvoUnref(root);
			xcdp_set_conn_error_text(conn, "failed to set Page.startScreencast quality");
			return NULL;
		}
	}
	if ( every_nth_frame > 0 ) {
		if ( !xvoTableSetInt(root, "everyNthFrame", 13u, every_nth_frame) ) {
			xvoUnref(root);
			xcdp_set_conn_error_text(conn, "failed to set Page.startScreencast everyNthFrame");
			return NULL;
		}
	}
	json_text = xrtStringifyJSON(root, 0, &json_len);
	xvoUnref(root);
	(void)json_len;
	if ( json_text == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to stringify Page.startScreencast params");
	}
	return json_text;
}

static char* xcdp_build_screencast_ack_params(xcdp_conn* conn, int frame_session_id)
{
	xvalue root = NULL;
	char* json_text = NULL;
	size_t json_len = 0u;
	root = xvoCreateTable();
	if ( root == NULL ) {
		xcdp_set_conn_error_text(conn, "out of memory while building Page.screencastFrameAck params");
		return NULL;
	}
	if ( !xvoTableSetInt(root, "sessionId", 9u, frame_session_id) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "failed to build Page.screencastFrameAck params");
		return NULL;
	}
	json_text = xrtStringifyJSON(root, 0, &json_len);
	xvoUnref(root);
	(void)json_len;
	if ( json_text == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to stringify Page.screencastFrameAck params");
	}
	return json_text;
}

static char* xcdp_build_viewport_params(xcdp_conn* conn, int width, int height, int device_scale_factor, int is_mobile)
{
	xvalue root = NULL;
	char* json_text = NULL;
	size_t json_len = 0u;
	if ( width <= 0 || height <= 0 ) {
		xcdp_set_conn_error_text(conn, "viewport width and height must be positive");
		return NULL;
	}
	if ( device_scale_factor <= 0 ) {
		device_scale_factor = 1;
	}
	root = xvoCreateTable();
	if ( root == NULL ) {
		xcdp_set_conn_error_text(conn, "out of memory while building viewport params");
		return NULL;
	}
	if ( !xvoTableSetInt(root, "width", 5u, width) ||
		!xvoTableSetInt(root, "height", 6u, height) ||
		!xvoTableSetInt(root, "deviceScaleFactor", 17u, device_scale_factor) ||
		!xvoTableSetBool(root, "mobile", 6u, is_mobile ? TRUE : FALSE) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "failed to build viewport params");
		return NULL;
	}
	json_text = xrtStringifyJSON(root, 0, &json_len);
	xvoUnref(root);
	(void)json_len;
	if ( json_text == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to stringify viewport params");
	}
	return json_text;
}

static char* xcdp_build_geolocation_params(xcdp_conn* conn, int latitude_e7, int longitude_e7, int accuracy)
{
	xvalue root = NULL;
	char* json_text = NULL;
	size_t json_len = 0u;
	double latitude = ((double)latitude_e7) / 10000000.0;
	double longitude = ((double)longitude_e7) / 10000000.0;
	if ( latitude < -90.0 || latitude > 90.0 || longitude < -180.0 || longitude > 180.0 ) {
		xcdp_set_conn_error_text(conn, "geolocation latitude or longitude is out of range");
		return NULL;
	}
	if ( accuracy < 0 ) {
		accuracy = 0;
	}
	root = xvoCreateTable();
	if ( root == NULL ) {
		xcdp_set_conn_error_text(conn, "out of memory while building geolocation params");
		return NULL;
	}
	if ( !xvoTableSetFloat(root, "latitude", 8u, latitude) ||
		!xvoTableSetFloat(root, "longitude", 9u, longitude) ||
		!xvoTableSetInt(root, "accuracy", 8u, accuracy) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "failed to build geolocation params");
		return NULL;
	}
	json_text = xrtStringifyJSON(root, 0, &json_len);
	xvoUnref(root);
	(void)json_len;
	if ( json_text == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to stringify geolocation params");
	}
	return json_text;
}

static char* xcdp_build_network_conditions_params(xcdp_conn* conn, int offline)
{
	xvalue root = NULL;
	char* json_text = NULL;
	size_t json_len = 0u;
	root = xvoCreateTable();
	if ( root == NULL ) {
		xcdp_set_conn_error_text(conn, "out of memory while building network params");
		return NULL;
	}
	if ( !xvoTableSetBool(root, "offline", 7u, offline ? TRUE : FALSE) ||
		!xvoTableSetInt(root, "latency", 7u, 0) ||
		!xvoTableSetInt(root, "downloadThroughput", 18u, offline ? 0 : -1) ||
		!xvoTableSetInt(root, "uploadThroughput", 16u, offline ? 0 : -1) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "failed to build network params");
		return NULL;
	}
	json_text = xrtStringifyJSON(root, 0, &json_len);
	xvoUnref(root);
	(void)json_len;
	if ( json_text == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to stringify network params");
	}
	return json_text;
}

static char* xcdp_build_cache_disabled_params(xcdp_conn* conn, int disabled)
{
	xvalue root = NULL;
	char* json_text = NULL;
	size_t json_len = 0u;
	root = xvoCreateTable();
	if ( root == NULL ) {
		xcdp_set_conn_error_text(conn, "out of memory while building Network.setCacheDisabled params");
		return NULL;
	}
	if ( !xvoTableSetBool(root, "cacheDisabled", 13u, disabled ? TRUE : FALSE) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "failed to build Network.setCacheDisabled params");
		return NULL;
	}
	json_text = xrtStringifyJSON(root, 0, &json_len);
	xvoUnref(root);
	(void)json_len;
	if ( json_text == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to stringify Network.setCacheDisabled params");
	}
	return json_text;
}

static char* xcdp_build_extra_http_headers_params(xcdp_conn* conn, const char* headers_json)
{
	xvalue root = NULL;
	xvalue headers = NULL;
	char* json_text = NULL;
	size_t json_len = 0u;
	if ( headers_json == NULL || headers_json[0] == '\0' ) {
		xcdp_set_conn_error_text(conn, "extra HTTP headers JSON is empty");
		return NULL;
	}
	headers = xrtParseJSON((str)headers_json, strlen(headers_json));
	if ( !xvoIsTable(headers) ) {
		xvoUnref(headers);
		xcdp_set_conn_error_text(conn, "extra HTTP headers JSON must be an object");
		return NULL;
	}
	root = xvoCreateTable();
	if ( root == NULL ) {
		xvoUnref(headers);
		xcdp_set_conn_error_text(conn, "out of memory while building Network.setExtraHTTPHeaders params");
		return NULL;
	}
	if ( !xvoTableSetValue(root, "headers", 7u, headers, TRUE) ) {
		xvoUnref(root);
		xvoUnref(headers);
		xcdp_set_conn_error_text(conn, "failed to set Network.setExtraHTTPHeaders headers");
		return NULL;
	}
	headers = NULL;
	json_text = xrtStringifyJSON(root, 0, &json_len);
	xvoUnref(root);
	(void)json_len;
	if ( json_text == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to stringify Network.setExtraHTTPHeaders params");
	}
	return json_text;
}

static char* xcdp_build_key_event_params(xcdp_conn* conn, const char* type, const char* key, const char* text)
{
	xvalue root = NULL;
	char* json_text = NULL;
	size_t json_len = 0u;
	if ( type == NULL || type[0] == '\0' ) {
		xcdp_set_conn_error_text(conn, "Input.dispatchKeyEvent type is empty");
		return NULL;
	}
	if ( key == NULL || key[0] == '\0' ) {
		xcdp_set_conn_error_text(conn, "Input.dispatchKeyEvent key is empty");
		return NULL;
	}
	root = xvoCreateTable();
	if ( root == NULL ) {
		xcdp_set_conn_error_text(conn, "out of memory while building Input.dispatchKeyEvent params");
		return NULL;
	}
	if ( !xvoTableSetText(root, "type", 4u, (ptr)type, (uint32)strlen(type), FALSE) ||
		!xvoTableSetText(root, "key", 3u, (ptr)key, (uint32)strlen(key), FALSE) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "failed to build Input.dispatchKeyEvent params");
		return NULL;
	}
	if ( text != NULL && text[0] != '\0' ) {
		if ( !xvoTableSetText(root, "text", 4u, (ptr)text, (uint32)strlen(text), FALSE) ) {
			xvoUnref(root);
			xcdp_set_conn_error_text(conn, "failed to set Input.dispatchKeyEvent text");
			return NULL;
		}
	}
	json_text = xrtStringifyJSON(root, 0, &json_len);
	xvoUnref(root);
	(void)json_len;
	if ( json_text == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to stringify Input.dispatchKeyEvent params");
	}
	return json_text;
}

static char* xcdp_build_insert_text_params(xcdp_conn* conn, const char* text)
{
	xvalue root = NULL;
	char* json_text = NULL;
	size_t json_len = 0u;
	if ( text == NULL ) {
		text = "";
	}
	root = xvoCreateTable();
	if ( root == NULL ) {
		xcdp_set_conn_error_text(conn, "out of memory while building Input.insertText params");
		return NULL;
	}
	if ( !xvoTableSetText(root, "text", 4u, (ptr)text, (uint32)strlen(text), FALSE) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "failed to build Input.insertText params");
		return NULL;
	}
	json_text = xrtStringifyJSON(root, 0, &json_len);
	xvoUnref(root);
	(void)json_len;
	if ( json_text == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to stringify Input.insertText params");
	}
	return json_text;
}

static char* xcdp_build_fetch_enable_params(xcdp_conn* conn, const char* url_pattern)
{
	xvalue root = NULL;
	xvalue patterns = NULL;
	xvalue pattern = NULL;
	char* json_text = NULL;
	size_t json_len = 0u;
	const char* effective_pattern = (url_pattern && url_pattern[0]) ? url_pattern : "*";
	root = xvoCreateTable();
	patterns = xvoCreateArray();
	pattern = xvoCreateTable();
	if ( root == NULL || patterns == NULL || pattern == NULL ) {
		xvoUnref(root);
		xvoUnref(patterns);
		xvoUnref(pattern);
		xcdp_set_conn_error_text(conn, "out of memory while building Fetch.enable params");
		return NULL;
	}
	if ( !xvoTableSetText(pattern, "urlPattern", 10u, (ptr)effective_pattern, (uint32)strlen(effective_pattern), FALSE) ||
		!xvoArrayAppendValue(patterns, pattern, TRUE) ||
		!xvoTableSetValue(root, "patterns", 8u, patterns, TRUE) ) {
		xvoUnref(root);
		xvoUnref(patterns);
		xvoUnref(pattern);
		xcdp_set_conn_error_text(conn, "failed to build Fetch.enable params");
		return NULL;
	}
	pattern = NULL;
	patterns = NULL;
	json_text = xrtStringifyJSON(root, 0, &json_len);
	xvoUnref(root);
	(void)json_len;
	if ( json_text == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to stringify Fetch.enable params");
	}
	return json_text;
}

static char* xcdp_build_fetch_abort_params(xcdp_conn* conn, const char* request_id, const char* error_reason)
{
	xvalue root = NULL;
	char* json_text = NULL;
	size_t json_len = 0u;
	const char* reason = (error_reason && error_reason[0]) ? error_reason : "Failed";
	if ( request_id == NULL || request_id[0] == '\0' ) {
		xcdp_set_conn_error_text(conn, "Fetch requestId is empty");
		return NULL;
	}
	root = xvoCreateTable();
	if ( root == NULL ) {
		xcdp_set_conn_error_text(conn, "out of memory while building Fetch.failRequest params");
		return NULL;
	}
	if ( !xvoTableSetText(root, "requestId", 9u, (ptr)request_id, (uint32)strlen(request_id), FALSE) ||
		!xvoTableSetText(root, "errorReason", 11u, (ptr)reason, (uint32)strlen(reason), FALSE) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "failed to build Fetch.failRequest params");
		return NULL;
	}
	json_text = xrtStringifyJSON(root, 0, &json_len);
	xvoUnref(root);
	(void)json_len;
	if ( json_text == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to stringify Fetch.failRequest params");
	}
	return json_text;
}

static char* xcdp_build_fetch_continue_ex_params(xcdp_conn* conn, const char* request_id, const char* options_json)
{
	xvalue root = NULL;
	char* json_text = NULL;
	size_t json_len = 0u;
	if ( request_id == NULL || request_id[0] == '\0' ) {
		xcdp_set_conn_error_text(conn, "Fetch requestId is empty");
		return NULL;
	}
	if ( options_json != NULL && options_json[0] != '\0' ) {
		root = xrtParseJSON((str)options_json, strlen(options_json));
		if ( !xvoIsTable(root) ) {
			xvoUnref(root);
			xcdp_set_conn_error_text(conn, "Fetch continue options_json must be a JSON object");
			return NULL;
		}
	}
	else {
		root = xvoCreateTable();
	}
	if ( root == NULL ) {
		xcdp_set_conn_error_text(conn, "out of memory while building Fetch.continueRequest params");
		return NULL;
	}
	if ( !xvoTableSetText(root, "requestId", 9u, (ptr)request_id, (uint32)strlen(request_id), FALSE) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "failed to build Fetch.continueRequest params");
		return NULL;
	}
	json_text = xrtStringifyJSON(root, 0, &json_len);
	xvoUnref(root);
	(void)json_len;
	if ( json_text == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to stringify Fetch.continueRequest params");
	}
	return json_text;
}

static char* xcdp_build_fetch_fulfill_params(xcdp_conn* conn, const char* request_id, int status, const char* body_base64)
{
	xvalue root = NULL;
	char* json_text = NULL;
	size_t json_len = 0u;
	const char* body = body_base64 ? body_base64 : "";
	if ( request_id == NULL || request_id[0] == '\0' ) {
		xcdp_set_conn_error_text(conn, "Fetch requestId is empty");
		return NULL;
	}
	if ( status < 100 || status > 599 ) {
		xcdp_set_conn_error_text(conn, "Fetch fulfill status must be between 100 and 599");
		return NULL;
	}
	root = xvoCreateTable();
	if ( root == NULL ) {
		xcdp_set_conn_error_text(conn, "out of memory while building Fetch.fulfillRequest params");
		return NULL;
	}
	if ( !xvoTableSetText(root, "requestId", 9u, (ptr)request_id, (uint32)strlen(request_id), FALSE) ||
		!xvoTableSetInt(root, "responseCode", 12u, status) ||
		!xvoTableSetText(root, "body", 4u, (ptr)body, (uint32)strlen(body), FALSE) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "failed to build Fetch.fulfillRequest params");
		return NULL;
	}
	json_text = xrtStringifyJSON(root, 0, &json_len);
	xvoUnref(root);
	(void)json_len;
	if ( json_text == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to stringify Fetch.fulfillRequest params");
	}
	return json_text;
}

static char* xcdp_build_fetch_fulfill_text_params(xcdp_conn* conn, const char* request_id, int status, const char* content_type, const char* body_text)
{
	xvalue root = NULL;
	xvalue headers = NULL;
	xvalue header = NULL;
	char* body_base64 = NULL;
	char* json_text = NULL;
	size_t json_len = 0u;
	const char* body = body_text ? body_text : "";
	const char* type = content_type ? content_type : "";
	if ( request_id == NULL || request_id[0] == '\0' ) {
		xcdp_set_conn_error_text(conn, "Fetch requestId is empty");
		return NULL;
	}
	if ( status < 100 || status > 599 ) {
		xcdp_set_conn_error_text(conn, "Fetch fulfill status must be between 100 and 599");
		return NULL;
	}
	body_base64 = xrtBase64Encode((ptr)body, strlen(body), NULL);
	if ( body_base64 == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to encode Fetch fulfill body");
		return NULL;
	}
	root = xvoCreateTable();
	if ( root == NULL ) {
		xrtFree(body_base64);
		xcdp_set_conn_error_text(conn, "out of memory while building Fetch.fulfillRequest params");
		return NULL;
	}
	if ( !xvoTableSetText(root, "requestId", 9u, (ptr)request_id, (uint32)strlen(request_id), FALSE) ||
		!xvoTableSetInt(root, "responseCode", 12u, status) ||
		!xvoTableSetText(root, "body", 4u, (ptr)body_base64, (uint32)strlen(body_base64), TRUE) ) {
		xvoUnref(root);
		xrtFree(body_base64);
		xcdp_set_conn_error_text(conn, "failed to build Fetch.fulfillRequest params");
		return NULL;
	}
	xrtFree(body_base64);
	if ( type[0] != '\0' ) {
		headers = xvoCreateArray();
		header = xvoCreateTable();
		if ( headers == NULL || header == NULL ) {
			xvoUnref(root);
			xvoUnref(headers);
			xvoUnref(header);
			xcdp_set_conn_error_text(conn, "out of memory while building Fetch.fulfillRequest headers");
			return NULL;
		}
		if ( !xvoTableSetText(header, "name", 4u, "Content-Type", 12u, FALSE) ||
			!xvoTableSetText(header, "value", 5u, (ptr)type, (uint32)strlen(type), FALSE) ||
			!xvoArrayAppendValue(headers, header, TRUE) ||
			!xvoTableSetValue(root, "responseHeaders", 15u, headers, TRUE) ) {
			xvoUnref(root);
			xvoUnref(headers);
			xvoUnref(header);
			xcdp_set_conn_error_text(conn, "failed to build Fetch.fulfillRequest headers");
			return NULL;
		}
		header = NULL;
		headers = NULL;
	}
	json_text = xrtStringifyJSON(root, 0, &json_len);
	xvoUnref(root);
	(void)json_len;
	if ( json_text == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to stringify Fetch.fulfillRequest params");
	}
	return json_text;
}

static char* xcdp_build_fetch_fulfill_ex_params(xcdp_conn* conn, const char* request_id, const char* options_json)
{
	xvalue root = NULL;
	char* json_text = NULL;
	size_t json_len = 0u;
	if ( request_id == NULL || request_id[0] == '\0' ) {
		xcdp_set_conn_error_text(conn, "Fetch requestId is empty");
		return NULL;
	}
	if ( options_json != NULL && options_json[0] != '\0' ) {
		root = xrtParseJSON((str)options_json, strlen(options_json));
		if ( !xvoIsTable(root) ) {
			xvoUnref(root);
			xcdp_set_conn_error_text(conn, "Fetch fulfill options_json must be a JSON object");
			return NULL;
		}
	}
	else {
		root = xvoCreateTable();
	}
	if ( root == NULL ) {
		xcdp_set_conn_error_text(conn, "out of memory while building Fetch.fulfillRequest params");
		return NULL;
	}
	if ( !xvoTableSetText(root, "requestId", 9u, (ptr)request_id, (uint32)strlen(request_id), FALSE) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "failed to build Fetch.fulfillRequest params");
		return NULL;
	}
	json_text = xrtStringifyJSON(root, 0, &json_len);
	xvoUnref(root);
	(void)json_len;
	if ( json_text == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to stringify Fetch.fulfillRequest params");
	}
	return json_text;
}

static char* xcdp_route_action_options_json(xcdp_conn* conn, xvalue root)
{
	char* json_text = NULL;
	size_t json_len = 0u;
	xvoTableRemove(root, "action", 6u);
	xvoTableRemove(root, "errorReason", 11u);
	json_text = xrtStringifyJSON(root, 0, &json_len);
	(void)json_len;
	if ( json_text == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to stringify route action options");
	}
	return json_text;
}

static const char* xcdp_response_runtime_value(xcdp_conn* conn, const char* response_json)
{
	xvalue root = NULL;
	xvalue result = NULL;
	xvalue remote = NULL;
	xvalue value = NULL;
	char* json_text = NULL;
	char* copy = NULL;
	size_t json_len = 0u;
	if ( response_json == NULL || response_json[0] == '\0' ) {
		return "";
	}
	root = xrtParseJSON((str)response_json, strlen(response_json));
	if ( !xvoIsTable(root) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "invalid CDP response JSON");
		return "";
	}
	result = xvoTableGetValue(root, "result", 6u);
	remote = xvoIsTable(result) ? xvoTableGetValue(result, "result", 6u) : NULL;
	value = xvoIsTable(remote) ? xvoTableGetValue(remote, "value", 5u) : NULL;
	if ( value == NULL || xvoIsNull(value) ) {
		xvoUnref(root);
		return xcdp_set_conn_return_owned(conn, xcdp_strndup_xrt("", 0u));
	}
	if ( xvoIsText(value) ) {
		copy = xcdp_strndup_xrt(xvoGetText(value), xvoGetSize(value));
	} else {
		json_text = xrtStringifyJSON(value, 0, &json_len);
		if ( json_text ) {
			copy = xcdp_strndup_xrt(json_text, json_len);
			xrtFree(json_text);
		}
	}
	xvoUnref(root);
	if ( copy == NULL ) {
		xcdp_set_conn_error_text(conn, "out of memory");
		return "";
	}
	return xcdp_set_conn_return_owned(conn, copy);
}

static size_t xcdp_base64_decoded_len(const char* text, size_t len)
{
	size_t out_len;
	if ( text == NULL || len == 0u ) {
		return 0u;
	}
	out_len = (len / 4u) * 3u;
	if ( len > 0u && text[len - 1u] == '=' && out_len > 0u ) {
		out_len--;
	}
	if ( len > 1u && text[len - 2u] == '=' && out_len > 0u ) {
		out_len--;
	}
	return out_len;
}

static int xcdp_base64_is_valid(const char* text, size_t len)
{
	size_t i;
	int seen_pad = 0;
	if ( text == NULL || len == 0u || (len % 4u) != 0u ) {
		return 0;
	}
	for ( i = 0u; i < len; ++i ) {
		unsigned char ch = (unsigned char)text[i];
		int is_data = (ch >= 'A' && ch <= 'Z') ||
			(ch >= 'a' && ch <= 'z') ||
			(ch >= '0' && ch <= '9') ||
			ch == '+' || ch == '/';
		if ( ch == '=' ) {
			seen_pad = 1;
			if ( i < len - 2u ) {
				return 0;
			}
			continue;
		}
		if ( !is_data || seen_pad ) {
			return 0;
		}
	}
	return 1;
}

static int xcdp_response_save_base64_field(xcdp_conn* conn, const char* response_json, const char* field, uint32_t field_len, const char* path)
{
	xvalue root = NULL;
	xvalue result = NULL;
	xvalue value = NULL;
	const char* data;
	size_t data_len;
	size_t decoded_len;
	ptr decoded = NULL;
	int ok = 0;
	if ( response_json == NULL || response_json[0] == '\0' ) {
		return 0;
	}
	if ( path == NULL || path[0] == '\0' ) {
		xcdp_set_conn_error_text(conn, "output path is empty");
		return 0;
	}
	root = xrtParseJSON((str)response_json, strlen(response_json));
	if ( !xvoIsTable(root) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "invalid CDP response JSON");
		return 0;
	}
	result = xvoTableGetValue(root, "result", 6u);
	if ( !xvoIsTable(result) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "CDP response does not contain result object");
		return 0;
	}
	value = xvoTableGetValue(result, field, field_len);
	if ( !xvoIsText(value) ) {
		xvoUnref(root);
		xcdp_set_conn_errorf(conn, "CDP response result does not contain %s", field ? field : "");
		return 0;
	}
	data = xvoGetText(value);
	data_len = xvoGetSize(value);
	if ( !xcdp_base64_is_valid(data, data_len) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "CDP base64 data is invalid");
		return 0;
	}
	decoded_len = xcdp_base64_decoded_len(data, data_len);
	decoded = xrtBase64Decode((str)data, data_len, NULL);
	if ( decoded == NULL || decoded_len == 0u ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "failed to decode CDP base64 data");
		return 0;
	}
	ok = xrtFilePutAll((str)path, decoded, decoded_len) ? 1 : 0;
	xrtFree(decoded);
	xvoUnref(root);
	if ( !ok ) {
		xcdp_set_conn_error_text(conn, "failed to write CDP data to output path");
	}
	return ok;
}

static void xcdp_conn_close_resources(xcdp_conn* conn)
{
	if ( conn == NULL || conn->magic != XCDP_MAGIC ) {
		return;
	}
	conn->closing = 1;
	conn->open = 0;
	if ( conn->incoming ) {
		xrtMPSCQWaitClose(conn->incoming);
	}
	if ( conn->events ) {
		xrtMPSCQWaitClose(conn->events);
	}
	if ( conn->client ) {
		if ( xrtWsClientIsOpen(conn->client) ) {
			(void)xrtWsClientClose(conn->client, XWS_CLOSE_NORMAL, "x.cdp close");
		}
		xrtWsClientStop(conn->client);
		xrtWsClientDestroy(conn->client);
		conn->client = NULL;
	}
	if ( conn->engine ) {
		xrtNetEngineStop(conn->engine);
	}
	if ( conn->engine ) {
		xrtNetEngineDestroy(conn->engine);
		conn->engine = NULL;
	}
	conn->closed = 1;
}

const char* x_cdp_version(void)
{
	xcdp_set_global_error_text("");
	return XCDP_VERSION;
}

const char* x_cdp_last_error(void)
{
	xcdp_global_init();
	return g_xcdp_error;
}

const char* x_cdp_error(void* handle)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	if ( conn == NULL ) {
		return g_xcdp_error;
	}
	return conn->last_error;
}

const char* x_cdp_version_url(const char* endpoint)
{
	xcdp_set_global_error_text("");
	return xcdp_make_http_url(endpoint, "json/version");
}

const char* x_cdp_list_url(const char* endpoint)
{
	xcdp_set_global_error_text("");
	return xcdp_make_http_url(endpoint, "json/list");
}

const char* x_cdp_protocol_url(const char* endpoint)
{
	xcdp_set_global_error_text("");
	return xcdp_make_http_url(endpoint, "json/protocol");
}

const char* x_cdp_http_get(const char* url, int timeout_ms)
{
	xhttprequest req;
	xhttpresponse* resp = NULL;
	xnetfuture* future = NULL;
	xnetengineconfig engine_cfg;
	xnetengine* engine = NULL;
	xnet_result status = XRT_NET_ERROR;
	xnet_result final_status = XRT_NET_ERROR;
	const char* ret = "";
	uint32_t timeout = xcdp_timeout_or_default(timeout_ms, XCDP_DEFAULT_HTTP_TIMEOUT_MS);
	double deadline;
	int req_inited = 0;
	xcdp_set_global_error_text("");
	if ( url == NULL || url[0] == '\0' ) {
		xcdp_set_global_error_text("HTTP URL is empty");
		return "";
	}
	xrtNetEngineConfigInit(&engine_cfg);
	engine_cfg.iWorkerCount = 1u;
	engine = xrtNetEngineCreate(&engine_cfg);
	if ( engine == NULL || xrtNetEngineStart(engine) != XRT_NET_OK ) {
		if ( engine ) {
			xrtNetEngineDestroy(engine);
		}
		xcdp_set_global_error_text("failed to start xrt HTTP net engine");
		return "";
	}
	xrtHttpRequestInit(&req);
	req_inited = 1;
	if ( !xrtHttpRequestSetMethod(&req, "GET") || !xrtHttpRequestSetURL(&req, url) ) {
		xcdp_set_global_error_text("failed to initialize xrt HTTP request");
		goto cleanup;
	}
	(void)xrtHttpRequestSetHeader(&req, "Connection", "close");
	xrtHttpRequestSetTimeout(&req, timeout);
	xrtHttpRequestSetIdleTimeout(&req, timeout);
	future = xrtHttpExecuteAsync(engine, &req);
	if ( future == NULL ) {
		xcdp_set_global_error_text("failed to start xrt HTTP GET");
		goto cleanup;
	}
	deadline = xrtTimer() + ((double)timeout / 1000.0);
	do {
		status = xrtNetFutureWait(future, 0u);
		if ( status != XRT_NET_AGAIN && status != XRT_NET_TIMEOUT ) {
			break;
		}
		xrtSleep(5u);
	} while ( xrtTimer() < deadline );
	final_status = xrtNetFutureStatus(future);
	resp = (final_status == XRT_NET_OK) ? (xhttpresponse*)xrtNetFutureValue(future) : NULL;
	if ( final_status != XRT_NET_OK || resp == NULL ) {
		xcdp_set_global_errorf("xrt HTTP GET failed: status=%d", (int)final_status);
		goto cleanup;
	}
	if ( resp->iStatusCode < 200u || resp->iStatusCode >= 300u ) {
		xcdp_set_global_errorf("HTTP GET returned status %u", (unsigned)resp->iStatusCode);
		goto cleanup;
	}
	ret = xcdp_set_global_return_copy(resp->pBody ? resp->pBody : "", resp->pBody ? resp->iBodyLen : 0u);
cleanup:
	if ( resp ) {
		xrtHttpResponseDestroy(resp);
		resp = NULL;
	}
	if ( engine ) {
		xrtHttpCloseIdleConnections(engine);
		xrtNetEngineStop(engine);
	}
	if ( future ) {
		xrtNetFutureDestroy(future);
		future = NULL;
	}
	if ( req_inited ) {
		xrtHttpRequestUnit(&req);
	}
	if ( engine ) {
		xrtNetEngineDestroy(engine);
	}
	return ret;
}

static int xcdp_path_join2(char* out, size_t out_size, const char* base, const char* leaf)
{
	size_t len;
	if ( out == NULL || out_size == 0u || base == NULL || base[0] == '\0' || leaf == NULL || leaf[0] == '\0' ) {
		return 0;
	}
	len = strlen(base);
	if ( len > 0u && (base[len - 1u] == '\\' || base[len - 1u] == '/') ) {
		return snprintf(out, out_size, "%s%s", base, leaf) > 0;
	}
	return snprintf(out, out_size, "%s\\%s", base, leaf) > 0;
}

static int xcdp_try_chrome_candidate(char* out, size_t out_size, const char* base, const char* rel)
{
	char path[1024];
	if ( !xcdp_path_join2(path, sizeof(path), base, rel) ) {
		return 0;
	}
	if ( xrtFileExists((str)path) ) {
		snprintf(out, out_size, "%s", path);
		return 1;
	}
	return 0;
}

const char* x_cdp_find_chrome(void)
{
	const char* env_path;
	const char* program_files;
	const char* program_files_x86;
	const char* local_app_data;
	char found[1024];
	xcdp_set_global_error_text("");
	env_path = getenv("CHROME_PATH");
	if ( env_path != NULL && env_path[0] != '\0' && xrtFileExists((str)env_path) ) {
		return xcdp_set_global_return_copy(env_path, strlen(env_path));
	}
	program_files = getenv("PROGRAMFILES");
	if ( xcdp_try_chrome_candidate(found, sizeof(found), program_files, "Google\\Chrome\\Application\\chrome.exe") ) {
		return xcdp_set_global_return_copy(found, strlen(found));
	}
	program_files_x86 = getenv("PROGRAMFILES(X86)");
	if ( xcdp_try_chrome_candidate(found, sizeof(found), program_files_x86, "Google\\Chrome\\Application\\chrome.exe") ) {
		return xcdp_set_global_return_copy(found, strlen(found));
	}
	local_app_data = getenv("LOCALAPPDATA");
	if ( xcdp_try_chrome_candidate(found, sizeof(found), local_app_data, "Google\\Chrome\\Application\\chrome.exe") ) {
		return xcdp_set_global_return_copy(found, strlen(found));
	}
	xcdp_set_global_error_text("Chrome executable not found");
	return "";
}

const char* x_cdp_create_user_data_dir(const char* base_dir)
{
	const char* base = base_dir;
	char path[1024];
	unsigned long stamp;
	if ( base == NULL || base[0] == '\0' ) {
		base = getenv("TEMP");
	}
	if ( base == NULL || base[0] == '\0' ) {
		base = getenv("TMP");
	}
	if ( base == NULL || base[0] == '\0' ) {
		base = ".";
	}
	xcdp_set_global_error_text("");
	if ( !xrtDirCreateAll((str)base) ) {
		xcdp_set_global_error_text("failed to create userDataDir base directory");
		return "";
	}
	stamp = (unsigned long)(time(NULL) ^ (unsigned long)(uintptr_t)&base_dir ^ (unsigned long)(xrtTimer() * 1000000.0));
	snprintf(path, sizeof(path), "%s\\xlang-browser-%lu", base, stamp);
	if ( !xrtDirCreateAll((str)path) ) {
		xcdp_set_global_error_text("failed to create userDataDir directory");
		return "";
	}
	return xcdp_set_global_return_copy(path, strlen(path));
}

void* x_cdp_launch_chrome(const char* executable_path, const char* user_data_dir, int port, int headless)
{
	xprocessconfig cfg;
	str args[9];
	char arg_port[64];
	char arg_user_data_dir[1100];
	uint32 arg_count = 0u;
	xprocess* process;
	xcdp_set_global_error_text("");
	if ( executable_path == NULL || executable_path[0] == '\0' ) {
		xcdp_set_global_error_text("Chrome executable path is empty");
		return NULL;
	}
	if ( user_data_dir == NULL || user_data_dir[0] == '\0' ) {
		xcdp_set_global_error_text("Chrome userDataDir is empty");
		return NULL;
	}
	if ( port <= 0 ) {
		port = 9223;
	}
	snprintf(arg_port, sizeof(arg_port), "--remote-debugging-port=%d", port);
	snprintf(arg_user_data_dir, sizeof(arg_user_data_dir), "--user-data-dir=%s", user_data_dir);
	args[arg_count++] = (str)arg_port;
	args[arg_count++] = (str)"--remote-debugging-address=127.0.0.1";
	args[arg_count++] = (str)arg_user_data_dir;
	args[arg_count++] = (str)"--no-first-run";
	args[arg_count++] = (str)"--no-default-browser-check";
	args[arg_count++] = (str)"about:blank";
	if ( headless ) {
		args[arg_count++] = (str)"--headless=new";
		args[arg_count++] = (str)"--disable-gpu";
	}
	xrtProcessConfigInit(&cfg);
	cfg.iTargetKind = XPROC_TARGET_EXEC;
	cfg.sProgram = (str)executable_path;
	cfg.arrArgs = args;
	cfg.iArgCount = arg_count;
	cfg.bHideWindow = true;
	cfg.Stdin.iMode = XPROC_STDIO_NULL;
	cfg.Stdout.iMode = XPROC_STDIO_PIPE;
	cfg.Stderr.iMode = XPROC_STDIO_PIPE;
	process = xrtProcessSpawn(&cfg);
	if ( process == NULL ) {
		xcdp_set_global_error_text("failed to launch Chrome process");
		return NULL;
	}
	return process;
}

int x_cdp_process_is_running(void* process)
{
	if ( process == NULL ) {
		return 0;
	}
	return xrtProcessIsRunning((xprocess*)process) ? 1 : 0;
}

int x_cdp_process_kill_tree(void* process)
{
	if ( process == NULL ) {
		return 0;
	}
	return xrtProcessKillTree((xprocess*)process) ? 1 : 0;
}

int x_cdp_process_dispose(void* process)
{
	if ( process == NULL ) {
		return 0;
	}
	xrtProcessDestroy((xprocess*)process);
	return 1;
}

const char* x_cdp_read_devtools_active_port(const char* user_data_dir)
{
	char path[1100];
	char endpoint[128];
	char* text;
	char* line_end;
	size_t port_len;
	size_t size = 0u;
	xcdp_set_global_error_text("");
	if ( user_data_dir == NULL || user_data_dir[0] == '\0' ) {
		xcdp_set_global_error_text("userDataDir is empty");
		return "";
	}
	if ( !xcdp_path_join2(path, sizeof(path), user_data_dir, "DevToolsActivePort") ) {
		xcdp_set_global_error_text("failed to build DevToolsActivePort path");
		return "";
	}
	text = (char*)xrtFileGetAll((str)path, &size);
	if ( text == NULL || size == 0u ) {
		if ( text ) {
			xrtFree(text);
		}
		xcdp_set_global_error_text("DevToolsActivePort is not ready");
		return "";
	}
	line_end = (char*)memchr(text, '\n', size);
	port_len = line_end ? (size_t)(line_end - text) : size;
	if ( port_len > 0u && text[port_len - 1u] == '\r' ) {
		port_len--;
	}
	if ( port_len == 0u || port_len > 16u ) {
		xrtFree(text);
		xcdp_set_global_error_text("DevToolsActivePort port is invalid");
		return "";
	}
	snprintf(endpoint, sizeof(endpoint), "http://127.0.0.1:%.*s", (int)port_len, text);
	xrtFree(text);
	return xcdp_set_global_return_copy(endpoint, strlen(endpoint));
}

const char* x_cdp_wait_devtools_active_port(const char* user_data_dir, int timeout_ms)
{
	uint32_t timeout = xcdp_timeout_or_default(timeout_ms, XCDP_DEFAULT_CONNECT_TIMEOUT_MS);
	double deadline = xrtTimer() + ((double)timeout / 1000.0);
	const char* endpoint;
	do {
		endpoint = x_cdp_read_devtools_active_port(user_data_dir);
		if ( endpoint != NULL && endpoint[0] != '\0' ) {
			return endpoint;
		}
		if ( timeout == 0u ) {
			break;
		}
		xrtSleep(25u);
	} while ( xrtTimer() < deadline );
	xcdp_set_global_error_text("DevToolsActivePort wait timeout");
	return "";
}

void* x_cdp_connect_user_data_dir(const char* user_data_dir, int timeout_ms)
{
	const char* endpoint = x_cdp_wait_devtools_active_port(user_data_dir, timeout_ms);
	if ( endpoint == NULL || endpoint[0] == '\0' ) {
		return NULL;
	}
	return x_cdp_connect_http(endpoint, timeout_ms);
}

void* x_cdp_connect(const char* websocket_url, int timeout_ms)
{
	xcdp_conn* conn = NULL;
	xnetengineconfig engine_cfg;
	xwsclientconfig ws_cfg;
	xwsclientevents events;
	xqueue_config queue_cfg;
	uint32_t timeout = xcdp_timeout_or_default(timeout_ms, XCDP_DEFAULT_CONNECT_TIMEOUT_MS);
	double deadline;
	xcdp_set_global_error_text("");
	if ( websocket_url == NULL || websocket_url[0] == '\0' ) {
		xcdp_set_global_error_text("WebSocket URL is empty");
		return NULL;
	}
	conn = (xcdp_conn*)xrtMalloc(sizeof(xcdp_conn));
	if ( conn == NULL ) {
		xcdp_set_global_error_text("out of memory");
		return NULL;
	}
	memset(conn, 0, sizeof(*conn));
	conn->magic = XCDP_MAGIC;
	conn->next_id = 1;
	conn->lock = xrtMutexCreate();
	memset(&queue_cfg, 0, sizeof(queue_cfg));
	queue_cfg.iCapacity = XCDP_QUEUE_CAPACITY;
	conn->incoming = xrtMPSCQWaitCreate(&queue_cfg);
	conn->events = xrtMPSCQWaitCreate(&queue_cfg);
	if ( conn->lock == NULL || conn->incoming == NULL || conn->events == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to create xrt synchronization primitives");
		x_cdp_dispose(conn);
		return NULL;
	}
	xrtNetEngineConfigInit(&engine_cfg);
	engine_cfg.iWorkerCount = 1u;
	conn->engine = xrtNetEngineCreate(&engine_cfg);
	if ( conn->engine == NULL || xrtNetEngineStart(conn->engine) != XRT_NET_OK ) {
		xcdp_set_conn_error_text(conn, "failed to start xrt net engine");
		x_cdp_dispose(conn);
		return NULL;
	}
	xrtWsClientConfigInit(&ws_cfg);
	if ( strlen(websocket_url) >= sizeof(ws_cfg.sURL) ) {
		xcdp_set_conn_error_text(conn, "WebSocket URL is too long");
		x_cdp_dispose(conn);
		return NULL;
	}
	snprintf(ws_cfg.sURL, sizeof(ws_cfg.sURL), "%s", websocket_url);
	ws_cfg.iConnectTimeoutMs = timeout;
	ws_cfg.iRecvLimit = XCDP_RECV_LIMIT;
	ws_cfg.bVerifyPeer = true;
	memset(&events, 0, sizeof(events));
	events.OnOpen = xcdp_ws_on_open;
	events.OnText = xcdp_ws_on_text;
	events.OnClose = xcdp_ws_on_close;
	events.OnError = xcdp_ws_on_error;
	conn->client = xrtWsClientCreate(conn->engine, &ws_cfg, &events, conn);
	if ( conn->client == NULL ) {
		xcdp_set_conn_error_text(conn, "failed to create xrt WebSocket client");
		x_cdp_dispose(conn);
		return NULL;
	}
	if ( xrtWsClientStart(conn->client) != XRT_NET_OK ) {
		xcdp_set_conn_error_text(conn, "failed to start xrt WebSocket client");
		x_cdp_dispose(conn);
		return NULL;
	}
	deadline = xrtTimer() + ((double)timeout / 1000.0);
	while ( xrtTimer() < deadline ) {
		if ( xrtWsClientIsOpen(conn->client) ) {
			conn->open = 1;
			xcdp_set_conn_error_text(conn, "");
			return conn;
		}
		if ( conn->closed && conn->last_error[0] != '\0' ) {
			break;
		}
		xrtSleep(10u);
	}
	xcdp_set_conn_error_text(conn, conn->last_error[0] ? conn->last_error : "WebSocket connect timeout");
	x_cdp_dispose(conn);
	return NULL;
}

void* x_cdp_connect_http(const char* endpoint, int timeout_ms)
{
	const char* version_url;
	const char* body;
	char* ws_copy = NULL;
	xvalue root = NULL;
	xvalue ws_value = NULL;
	void* conn = NULL;
	uint32_t timeout = xcdp_timeout_or_default(timeout_ms, XCDP_DEFAULT_CONNECT_TIMEOUT_MS);
	xcdp_set_global_error_text("");
	version_url = x_cdp_version_url(endpoint);
	if ( version_url == NULL || version_url[0] == '\0' ) {
		return NULL;
	}
	body = x_cdp_http_get(version_url, (int)timeout);
	if ( body == NULL || body[0] == '\0' ) {
		return NULL;
	}
	root = xrtParseJSON((str)body, strlen(body));
	if ( !xvoIsTable(root) ) {
		xvoUnref(root);
		xcdp_set_global_error_text("invalid /json/version response");
		return NULL;
	}
	ws_value = xvoTableGetValue(root, "webSocketDebuggerUrl", 20u);
	if ( !xvoIsText(ws_value) ) {
		xvoUnref(root);
		xcdp_set_global_error_text("/json/version does not contain webSocketDebuggerUrl");
		return NULL;
	}
	ws_copy = xcdp_strndup_xrt(xvoGetText(ws_value), xvoGetSize(ws_value));
	xvoUnref(root);
	if ( ws_copy == NULL ) {
		xcdp_set_global_error_text("out of memory");
		return NULL;
	}
	conn = x_cdp_connect(ws_copy, (int)timeout);
	xrtFree(ws_copy);
	return conn;
}

int x_cdp_is_open(void* handle)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	if ( conn == NULL || conn->client == NULL ) {
		return 0;
	}
	return xrtWsClientIsOpen(conn->client) ? 1 : 0;
}

const char* x_cdp_send_session(void* handle, const char* session_id, const char* method, const char* params_json, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* command = NULL;
	size_t command_len = 0u;
	int target_id;
	uint32_t timeout = xcdp_timeout_or_default(timeout_ms, XCDP_DEFAULT_SEND_TIMEOUT_MS);
	double deadline;
	xcdp_msg* msg = NULL;
	if ( conn == NULL ) {
		return "";
	}
	xcdp_set_conn_error_text(conn, "");
	if ( conn->client == NULL || !xrtWsClientIsOpen(conn->client) ) {
		xcdp_set_conn_error_text(conn, "CDP WebSocket is not open");
		return "";
	}
	if ( conn->lock ) {
		xrtMutexLock(conn->lock);
		target_id = conn->next_id++;
		xrtMutexUnlock(conn->lock);
	} else {
		target_id = conn->next_id++;
	}
	command = xcdp_build_command_json(conn, target_id, session_id, method, params_json, &command_len);
	if ( command == NULL ) {
		return "";
	}
	if ( xrtWsClientSendText(conn->client, command, command_len) != XRT_NET_OK ) {
		xrtFree(command);
		xcdp_set_conn_error_text(conn, "failed to send CDP command over xrt WebSocket");
		return "";
	}
	xrtFree(command);
	msg = xcdp_pending_take_id(conn, target_id);
	if ( msg ) {
		char* owned = msg->text;
		msg->text = NULL;
		xcdp_msg_destroy(msg);
		return xcdp_set_conn_return_owned(conn, owned);
	}
	deadline = xrtTimer() + ((double)timeout / 1000.0);
	while ( xrtTimer() < deadline ) {
		double now = xrtTimer();
		uint32_t wait_ms;
		int msg_id = 0;
		int has_id;
		if ( now >= deadline ) {
			break;
		}
		wait_ms = (uint32_t)((deadline - now) * 1000.0);
		if ( wait_ms > 100u ) {
			wait_ms = 100u;
		}
		if ( wait_ms == 0u ) {
			wait_ms = 1u;
		}
		msg = NULL;
		if ( !xcdp_queue_pop_msg(conn->incoming, &msg, wait_ms) ) {
			continue;
		}
		has_id = xcdp_message_id(msg->text, msg->len, &msg_id);
		if ( has_id && msg_id == target_id ) {
			char* owned = msg->text;
			msg->text = NULL;
			xcdp_msg_destroy(msg);
			return xcdp_set_conn_return_owned(conn, owned);
		}
		if ( has_id ) {
			xcdp_pending_push(conn, msg);
		} else {
			xcdp_queue_push_msg(conn->events, msg);
		}
	}
	xcdp_set_conn_errorf(conn, "CDP command timeout: id=%d method=%s", target_id, method ? method : "");
	return "";
}

const char* x_cdp_send(void* handle, const char* method, const char* params_json, int timeout_ms)
{
	return x_cdp_send_session(handle, "", method, params_json, timeout_ms);
}

int x_cdp_trace_write_action(void* handle, const char* path, const char* action_json)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	return xcdp_append_jsonl_object(conn, path, action_json);
}

int x_cdp_trace_write_event(void* handle, const char* path, const char* event_json)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	return xcdp_append_jsonl_object(conn, path, event_json);
}

int x_cdp_trace_write_manifest(void* handle, const char* path, const char* manifest_json)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	return xcdp_write_json_object(conn, path, manifest_json);
}

const char* x_cdp_trace_default_manifest(void* handle, const char* trace_dir)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	const char* dir;
	char* dir_json = NULL;
	char* json = NULL;
	size_t len;
	if ( conn == NULL ) {
		return "";
	}
	xcdp_set_conn_error_text(conn, "");
	dir = (trace_dir && trace_dir[0]) ? trace_dir : ".";
	dir_json = xcdp_js_string_literal(conn, dir);
	if ( dir_json == NULL ) {
		return "";
	}
	len = strlen(dir_json) + 512u;
	json = (char*)xrtMalloc(len);
	if ( json == NULL ) {
		xrtFree(dir_json);
		xcdp_set_conn_error_text(conn, "out of memory while building trace manifest");
		return "";
	}
	snprintf(json, len, "{\"version\":1,\"root\":%s,\"trace\":\"trace.jsonl\",\"network\":\"network.jsonl\",\"console\":\"console.jsonl\",\"errors\":\"errors.jsonl\",\"snapshots\":\"snapshots/\",\"screenshots\":\"screenshots/\",\"attachments\":\"attachments/\",\"manifest\":\"manifest.json\"}", dir_json);
	xrtFree(dir_json);
	return xcdp_set_conn_return_owned(conn, json);
}

int x_cdp_trace_record_failure(void* handle, const char* session_id, const char* action_path, const char* action_json, const char* screenshot_path, const char* snapshot_path, const char* manifest_path, const char* manifest_json, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	int wrote = 0;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	if ( action_path != NULL && action_path[0] != '\0' && action_json != NULL && action_json[0] != '\0' ) {
		if ( !xcdp_append_jsonl_object(conn, action_path, action_json) ) {
			return 0;
		}
		wrote = 1;
	}
	if ( (screenshot_path != NULL && screenshot_path[0] != '\0') ||
		(snapshot_path != NULL && snapshot_path[0] != '\0') ) {
		if ( !x_cdp_page_failure_artifacts(conn, session_id, screenshot_path, snapshot_path, timeout_ms) ) {
			return 0;
		}
		wrote = 1;
	}
	if ( manifest_path != NULL && manifest_path[0] != '\0' && manifest_json != NULL && manifest_json[0] != '\0' ) {
		if ( !xcdp_write_json_object(conn, manifest_path, manifest_json) ) {
			return 0;
		}
		wrote = 1;
	}
	if ( !wrote ) {
		xcdp_set_conn_error_text(conn, "failure trace outputs are empty");
		return 0;
	}
	return 1;
}

const char* x_cdp_create_target(void* handle, const char* url, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return "";
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_create_target_params(conn, url, NULL);
	if ( params == NULL ) {
		return "";
	}
	response = x_cdp_send(conn, "Target.createTarget", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_result_text(conn, response, "targetId", 8u);
}

const char* x_cdp_create_context(void* handle, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	const char* response;
	if ( conn == NULL ) {
		return "";
	}
	xcdp_set_conn_error_text(conn, "");
	response = x_cdp_send(conn, "Target.createBrowserContext", "{}", timeout_ms);
	return xcdp_response_result_text(conn, response, "browserContextId", 16u);
}

int x_cdp_dispose_context(void* handle, const char* browser_context_id, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_target_params(conn, "browserContextId", 16u, browser_context_id);
	if ( params == NULL ) {
		return 0;
	}
	response = x_cdp_send(conn, "Target.disposeBrowserContext", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_has_result(conn, response);
}

const char* x_cdp_create_target_context(void* handle, const char* browser_context_id, const char* url, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return "";
	}
	xcdp_set_conn_error_text(conn, "");
	if ( browser_context_id == NULL || browser_context_id[0] == '\0' ) {
		xcdp_set_conn_error_text(conn, "CDP browserContextId is empty");
		return "";
	}
	params = xcdp_build_create_target_params(conn, url, browser_context_id);
	if ( params == NULL ) {
		return "";
	}
	response = x_cdp_send(conn, "Target.createTarget", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_result_text(conn, response, "targetId", 8u);
}

const char* x_cdp_attach_target(void* handle, const char* target_id, int flatten, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return "";
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_attach_target_params(conn, target_id, flatten);
	if ( params == NULL ) {
		return "";
	}
	response = x_cdp_send(conn, "Target.attachToTarget", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_result_text(conn, response, "sessionId", 9u);
}

int x_cdp_target_set_discover(void* handle, int discover, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_target_discover_params(conn, discover);
	if ( params == NULL ) {
		return 0;
	}
	response = x_cdp_send(conn, "Target.setDiscoverTargets", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_has_result(conn, response);
}

const char* x_cdp_wait_target_created(void* handle, int timeout_ms)
{
	return x_cdp_wait_event(handle, "", "Target.targetCreated", timeout_ms);
}

int x_cdp_close_target(void* handle, const char* target_id, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_target_params(conn, "targetId", 8u, target_id);
	if ( params == NULL ) {
		return 0;
	}
	response = x_cdp_send(conn, "Target.closeTarget", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_result_bool(conn, response, "success", 7u);
}

int x_cdp_browser_set_download_behavior(void* handle, const char* behavior, const char* download_path, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_download_behavior_params(conn, behavior, download_path);
	if ( params == NULL ) {
		return 0;
	}
	response = x_cdp_send(conn, "Browser.setDownloadBehavior", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_has_result(conn, response);
}

const char* x_cdp_browser_get_version(void* handle, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	const char* response;
	if ( conn == NULL ) {
		return "";
	}
	xcdp_set_conn_error_text(conn, "");
	response = x_cdp_send(conn, "Browser.getVersion", "{}", timeout_ms);
	return xcdp_response_result_json(conn, response);
}

const char* x_cdp_storage_get_cookies(void* handle, const char* browser_context_id, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return "";
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_optional_browser_context_params(conn, browser_context_id);
	if ( params == NULL ) {
		return "";
	}
	response = x_cdp_send(conn, "Storage.getCookies", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_result_json(conn, response);
}

int x_cdp_storage_set_cookies(void* handle, const char* browser_context_id, const char* cookies_json, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_set_cookies_params(conn, browser_context_id, cookies_json);
	if ( params == NULL ) {
		return 0;
	}
	response = x_cdp_send(conn, "Storage.setCookies", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_has_result(conn, response);
}

int x_cdp_storage_clear_cookies(void* handle, const char* browser_context_id, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_optional_browser_context_params(conn, browser_context_id);
	if ( params == NULL ) {
		return 0;
	}
	response = x_cdp_send(conn, "Storage.clearCookies", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_has_result(conn, response);
}

const char* x_cdp_storage_get_local_storage(void* handle, const char* session_id, int timeout_ms)
{
	return x_cdp_runtime_evaluate(handle, session_id, "JSON.stringify((function(s){var o={};for(var i=0;i<s.length;i++){var k=s.key(i);o[k]=s.getItem(k);}return o;})(localStorage))", timeout_ms);
}

int x_cdp_storage_set_local_storage(void* handle, const char* session_id, const char* items_json, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* expr = NULL;
	const char* value;
	int ok;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	expr = xcdp_build_storage_set_expression(conn, "localStorage", items_json);
	if ( expr == NULL ) {
		return 0;
	}
	value = x_cdp_runtime_evaluate(conn, session_id, expr, timeout_ms);
	ok = xcdp_eval_bool(value);
	xrtFree(expr);
	return ok;
}

const char* x_cdp_storage_get_session_storage(void* handle, const char* session_id, int timeout_ms)
{
	return x_cdp_runtime_evaluate(handle, session_id, "JSON.stringify((function(s){var o={};for(var i=0;i<s.length;i++){var k=s.key(i);o[k]=s.getItem(k);}return o;})(sessionStorage))", timeout_ms);
}

int x_cdp_storage_set_session_storage(void* handle, const char* session_id, const char* items_json, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* expr = NULL;
	const char* value;
	int ok;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	expr = xcdp_build_storage_set_expression(conn, "sessionStorage", items_json);
	if ( expr == NULL ) {
		return 0;
	}
	value = x_cdp_runtime_evaluate(conn, session_id, expr, timeout_ms);
	ok = xcdp_eval_bool(value);
	xrtFree(expr);
	return ok;
}

int x_cdp_browser_grant_permissions(void* handle, const char* browser_context_id, const char* permissions_json, const char* origin, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_grant_permissions_params(conn, browser_context_id, permissions_json, origin);
	if ( params == NULL ) {
		return 0;
	}
	response = x_cdp_send(conn, "Browser.grantPermissions", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_has_result(conn, response);
}

int x_cdp_browser_clear_permissions(void* handle, const char* browser_context_id, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_optional_browser_context_params(conn, browser_context_id);
	if ( params == NULL ) {
		return 0;
	}
	response = x_cdp_send(conn, "Browser.resetPermissions", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_has_result(conn, response);
}

const char* x_cdp_wait_download(void* handle, int timeout_ms)
{
	return x_cdp_wait_event(handle, "", "Browser.downloadWillBegin", timeout_ms);
}

const char* x_cdp_wait_download_progress(void* handle, int timeout_ms)
{
	return x_cdp_wait_event(handle, "", "Browser.downloadProgress", timeout_ms);
}

int x_cdp_page_enable(void* handle, const char* session_id, int timeout_ms)
{
	const char* response = x_cdp_send_session(handle, session_id, "Page.enable", "{}", timeout_ms);
	return (response && response[0] != '\0') ? 1 : 0;
}

const char* x_cdp_page_add_init_script(void* handle, const char* session_id, const char* source, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return "";
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_target_params(conn, "source", 6u, source);
	if ( params == NULL ) {
		return "";
	}
	response = x_cdp_send_session(conn, session_id, "Page.addScriptToEvaluateOnNewDocument", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_result_text(conn, response, "identifier", 10u);
}

int x_cdp_page_remove_init_script(void* handle, const char* session_id, const char* identifier, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_target_params(conn, "identifier", 10u, identifier);
	if ( params == NULL ) {
		return 0;
	}
	response = x_cdp_send_session(conn, session_id, "Page.removeScriptToEvaluateOnNewDocument", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_has_result(conn, response);
}

const char* x_cdp_page_navigate(void* handle, const char* session_id, const char* url, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return "";
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_target_params(conn, "url", 3u, url);
	if ( params == NULL ) {
		return "";
	}
	response = x_cdp_send_session(conn, session_id, "Page.navigate", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_result_text(conn, response, "frameId", 7u);
}

const char* x_cdp_runtime_evaluate(void* handle, const char* session_id, const char* expression, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return "";
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_runtime_evaluate_params(conn, expression);
	if ( params == NULL ) {
		return "";
	}
	response = x_cdp_send_session(conn, session_id, "Runtime.evaluate", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_runtime_value(conn, response);
}

const char* x_cdp_page_title(void* handle, const char* session_id, int timeout_ms)
{
	return x_cdp_runtime_evaluate(handle, session_id, "document.title", timeout_ms);
}

const char* x_cdp_page_url(void* handle, const char* session_id, int timeout_ms)
{
	return x_cdp_runtime_evaluate(handle, session_id, "location.href", timeout_ms);
}

int x_cdp_expect_title(void* handle, const char* session_id, const char* expected, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* exp = NULL;
	char* expr = NULL;
	size_t expr_len;
	int ok;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	exp = xcdp_js_string_literal(conn, expected);
	if ( exp == NULL ) {
		return 0;
	}
	expr_len = strlen(exp) + 64u;
	expr = (char*)xrtMalloc(expr_len);
	if ( expr == NULL ) {
		xrtFree(exp);
		xcdp_set_conn_error_text(conn, "out of memory while building title expectation");
		return 0;
	}
	snprintf(expr, expr_len, "document.title===%s", exp);
	ok = xcdp_poll_bool_expression(conn, session_id, expr, timeout_ms);
	xrtFree(expr);
	xrtFree(exp);
	return ok;
}

int x_cdp_expect_url(void* handle, const char* session_id, const char* expected_part, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* part = NULL;
	char* expr = NULL;
	size_t expr_len;
	int ok;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	part = xcdp_js_string_literal(conn, expected_part);
	if ( part == NULL ) {
		return 0;
	}
	expr_len = strlen(part) + 80u;
	expr = (char*)xrtMalloc(expr_len);
	if ( expr == NULL ) {
		xrtFree(part);
		xcdp_set_conn_error_text(conn, "out of memory while building URL expectation");
		return 0;
	}
	snprintf(expr, expr_len, "location.href.indexOf(%s)>=0", part);
	ok = xcdp_poll_bool_expression(conn, session_id, expr, timeout_ms);
	xrtFree(expr);
	xrtFree(part);
	return ok;
}

int x_cdp_expect_visible(void* handle, const char* session_id, const char* selector, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* sel = NULL;
	char* expr = NULL;
	size_t expr_len;
	int ok;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	sel = xcdp_js_string_literal(conn, selector);
	if ( sel == NULL ) {
		return 0;
	}
	expr_len = strlen(sel) + 256u;
	expr = (char*)xrtMalloc(expr_len);
	if ( expr == NULL ) {
		xrtFree(sel);
		xcdp_set_conn_error_text(conn, "out of memory while building visible expectation");
		return 0;
	}
	snprintf(expr, expr_len, "(()=>{const e=document.querySelector(%s);if(!e)return false;const s=getComputedStyle(e);const r=e.getBoundingClientRect();return s.display!=='none'&&s.visibility!=='hidden'&&r.width>0&&r.height>0;})()", sel);
	ok = xcdp_poll_bool_expression(conn, session_id, expr, timeout_ms);
	xrtFree(expr);
	xrtFree(sel);
	return ok;
}

int x_cdp_expect_text(void* handle, const char* session_id, const char* selector, const char* expected, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* sel = NULL;
	char* exp = NULL;
	char* expr = NULL;
	size_t expr_len;
	int ok;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	sel = xcdp_js_string_literal(conn, selector);
	exp = xcdp_js_string_literal(conn, expected);
	if ( sel == NULL || exp == NULL ) {
		xrtFree(sel);
		xrtFree(exp);
		return 0;
	}
	expr_len = strlen(sel) + strlen(exp) + 160u;
	expr = (char*)xrtMalloc(expr_len);
	if ( expr == NULL ) {
		xrtFree(sel);
		xrtFree(exp);
		xcdp_set_conn_error_text(conn, "out of memory while building text expectation");
		return 0;
	}
	snprintf(expr, expr_len, "(()=>{const e=document.querySelector(%s);return !!e&&((e.textContent||'')===%s);})()", sel, exp);
	ok = xcdp_poll_bool_expression(conn, session_id, expr, timeout_ms);
	xrtFree(expr);
	xrtFree(exp);
	xrtFree(sel);
	return ok;
}

int x_cdp_expect_count(void* handle, const char* session_id, const char* selector, int expected_count, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* sel = NULL;
	char* expr = NULL;
	size_t expr_len;
	int ok;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	sel = xcdp_js_string_literal(conn, selector);
	if ( sel == NULL ) {
		return 0;
	}
	expr_len = strlen(sel) + 128u;
	expr = (char*)xrtMalloc(expr_len);
	if ( expr == NULL ) {
		xrtFree(sel);
		xcdp_set_conn_error_text(conn, "out of memory while building count expectation");
		return 0;
	}
	snprintf(expr, expr_len, "document.querySelectorAll(%s).length===%d", sel, expected_count);
	ok = xcdp_poll_bool_expression(conn, session_id, expr, timeout_ms);
	xrtFree(expr);
	xrtFree(sel);
	return ok;
}

int x_cdp_locator_click(void* handle, const char* session_id, const char* selector, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* sel = NULL;
	char* expr = NULL;
	size_t expr_len;
	const char* value;
	int ok = 0;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	sel = xcdp_js_string_literal(conn, selector);
	if ( sel == NULL ) {
		return 0;
	}
	expr_len = strlen(sel) + 160u;
	expr = (char*)xrtMalloc(expr_len);
	if ( expr == NULL ) {
		xrtFree(sel);
		xcdp_set_conn_error_text(conn, "out of memory while building locator click expression");
		return 0;
	}
	snprintf(expr, expr_len, "(function(){var e=document.querySelector(%s);if(!e)return false;e.click();return true;})()", sel);
	value = x_cdp_runtime_evaluate(conn, session_id, expr, timeout_ms);
	ok = xcdp_eval_bool(value);
	xrtFree(expr);
	xrtFree(sel);
	return ok;
}

int x_cdp_locator_fill(void* handle, const char* session_id, const char* selector, const char* text, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* sel = NULL;
	char* val = NULL;
	char* expr = NULL;
	size_t expr_len;
	const char* value;
	int ok = 0;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	sel = xcdp_js_string_literal(conn, selector);
	val = xcdp_js_string_literal(conn, text);
	if ( sel == NULL || val == NULL ) {
		xrtFree(sel);
		xrtFree(val);
		return 0;
	}
	expr_len = strlen(sel) + strlen(val) + 360u;
	expr = (char*)xrtMalloc(expr_len);
	if ( expr == NULL ) {
		xrtFree(sel);
		xrtFree(val);
		xcdp_set_conn_error_text(conn, "out of memory while building locator fill expression");
		return 0;
	}
	snprintf(expr, expr_len, "(function(){var e=document.querySelector(%s);if(!e)return false;e.focus();e.value=%s;e.dispatchEvent(new Event('input',{bubbles:true}));e.dispatchEvent(new Event('change',{bubbles:true}));return true;})()", sel, val);
	value = x_cdp_runtime_evaluate(conn, session_id, expr, timeout_ms);
	ok = xcdp_eval_bool(value);
	xrtFree(expr);
	xrtFree(sel);
	xrtFree(val);
	return ok;
}

int x_cdp_locator_hover(void* handle, const char* session_id, const char* selector, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* sel = NULL;
	char* expr = NULL;
	size_t expr_len;
	const char* value;
	int ok = 0;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	sel = xcdp_js_string_literal(conn, selector);
	if ( sel == NULL ) {
		return 0;
	}
	expr_len = strlen(sel) + 420u;
	expr = (char*)xrtMalloc(expr_len);
	if ( expr == NULL ) {
		xrtFree(sel);
		xcdp_set_conn_error_text(conn, "out of memory while building locator hover expression");
		return 0;
	}
	snprintf(expr, expr_len, "(function(){var e=document.querySelector(%s);if(!e)return false;var r=e.getBoundingClientRect();var o={bubbles:true,clientX:r.left+r.width/2,clientY:r.top+r.height/2};e.dispatchEvent(new MouseEvent('mouseover',o));e.dispatchEvent(new MouseEvent('mouseenter',o));e.dispatchEvent(new MouseEvent('mousemove',o));return true;})()", sel);
	value = x_cdp_runtime_evaluate(conn, session_id, expr, timeout_ms);
	ok = xcdp_eval_bool(value);
	xrtFree(expr);
	xrtFree(sel);
	return ok;
}

int x_cdp_locator_check(void* handle, const char* session_id, const char* selector, int checked, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* sel = NULL;
	char* expr = NULL;
	size_t expr_len;
	const char* value;
	int ok = 0;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	sel = xcdp_js_string_literal(conn, selector);
	if ( sel == NULL ) {
		return 0;
	}
	expr_len = strlen(sel) + 360u;
	expr = (char*)xrtMalloc(expr_len);
	if ( expr == NULL ) {
		xrtFree(sel);
		xcdp_set_conn_error_text(conn, "out of memory while building locator check expression");
		return 0;
	}
	snprintf(expr, expr_len, "(function(){var e=document.querySelector(%s);if(!e||!('checked' in e))return false;if(e.checked!==%s){e.checked=%s;e.dispatchEvent(new Event('input',{bubbles:true}));e.dispatchEvent(new Event('change',{bubbles:true}));}return true;})()", sel, checked ? "true" : "false", checked ? "true" : "false");
	value = x_cdp_runtime_evaluate(conn, session_id, expr, timeout_ms);
	ok = xcdp_eval_bool(value);
	xrtFree(expr);
	xrtFree(sel);
	return ok;
}

int x_cdp_locator_select(void* handle, const char* session_id, const char* selector, const char* value_text, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* sel = NULL;
	char* val = NULL;
	char* expr = NULL;
	size_t expr_len;
	const char* value;
	int ok = 0;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	sel = xcdp_js_string_literal(conn, selector);
	val = xcdp_js_string_literal(conn, value_text);
	if ( sel == NULL || val == NULL ) {
		xrtFree(sel);
		xrtFree(val);
		return 0;
	}
	expr_len = strlen(sel) + strlen(val) + 360u;
	expr = (char*)xrtMalloc(expr_len);
	if ( expr == NULL ) {
		xrtFree(sel);
		xrtFree(val);
		xcdp_set_conn_error_text(conn, "out of memory while building locator select expression");
		return 0;
	}
	snprintf(expr, expr_len, "(function(){var e=document.querySelector(%s);if(!e||e.tagName!=='SELECT')return false;e.value=%s;e.dispatchEvent(new Event('input',{bubbles:true}));e.dispatchEvent(new Event('change',{bubbles:true}));return true;})()", sel, val);
	value = x_cdp_runtime_evaluate(conn, session_id, expr, timeout_ms);
	ok = xcdp_eval_bool(value);
	xrtFree(expr);
	xrtFree(sel);
	xrtFree(val);
	return ok;
}

int x_cdp_locator_press(void* handle, const char* session_id, const char* selector, const char* key, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* sel = NULL;
	char* expr = NULL;
	size_t expr_len;
	const char* value;
	int ok = 0;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	sel = xcdp_js_string_literal(conn, selector);
	if ( sel == NULL ) {
		return 0;
	}
	expr_len = strlen(sel) + 96u;
	expr = (char*)xrtMalloc(expr_len);
	if ( expr == NULL ) {
		xrtFree(sel);
		xcdp_set_conn_error_text(conn, "out of memory while building locator press focus expression");
		return 0;
	}
	snprintf(expr, expr_len, "(function(){var e=document.querySelector(%s);if(!e)return false;e.focus();return true;})()", sel);
	value = x_cdp_runtime_evaluate(conn, session_id, expr, timeout_ms);
	if ( xcdp_eval_bool(value) ) {
		ok = x_cdp_input_dispatch_key_event(conn, session_id, "keyDown", key, "", timeout_ms) &&
			x_cdp_input_dispatch_key_event(conn, session_id, "keyUp", key, "", timeout_ms);
	}
	xrtFree(expr);
	xrtFree(sel);
	return ok;
}

int x_cdp_locator_type(void* handle, const char* session_id, const char* selector, const char* text, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* sel = NULL;
	char* expr = NULL;
	size_t expr_len;
	const char* value;
	int ok = 0;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	sel = xcdp_js_string_literal(conn, selector);
	if ( sel == NULL ) {
		return 0;
	}
	expr_len = strlen(sel) + 96u;
	expr = (char*)xrtMalloc(expr_len);
	if ( expr == NULL ) {
		xrtFree(sel);
		xcdp_set_conn_error_text(conn, "out of memory while building locator type focus expression");
		return 0;
	}
	snprintf(expr, expr_len, "(function(){var e=document.querySelector(%s);if(!e)return false;e.focus();return true;})()", sel);
	value = x_cdp_runtime_evaluate(conn, session_id, expr, timeout_ms);
	if ( xcdp_eval_bool(value) ) {
		ok = x_cdp_input_insert_text(conn, session_id, text, timeout_ms);
	}
	xrtFree(expr);
	xrtFree(sel);
	return ok;
}

const char* x_cdp_locator_text(void* handle, const char* session_id, const char* selector, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* sel = NULL;
	char* expr = NULL;
	size_t expr_len;
	const char* value;
	if ( conn == NULL ) {
		return "";
	}
	xcdp_set_conn_error_text(conn, "");
	sel = xcdp_js_string_literal(conn, selector);
	if ( sel == NULL ) {
		return "";
	}
	expr_len = strlen(sel) + 180u;
	expr = (char*)xrtMalloc(expr_len);
	if ( expr == NULL ) {
		xrtFree(sel);
		xcdp_set_conn_error_text(conn, "out of memory while building locator text expression");
		return "";
	}
	snprintf(expr, expr_len, "(function(){var e=document.querySelector(%s);return e?(e.innerText||e.textContent||''):'';})()", sel);
	value = x_cdp_runtime_evaluate(conn, session_id, expr, timeout_ms);
	xrtFree(expr);
	xrtFree(sel);
	return value;
}

int x_cdp_locator_count(void* handle, const char* session_id, const char* selector, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* sel = NULL;
	char* expr = NULL;
	size_t expr_len;
	const char* value;
	int count = 0;
	if ( conn == NULL ) {
		return -1;
	}
	xcdp_set_conn_error_text(conn, "");
	sel = xcdp_js_string_literal(conn, selector);
	if ( sel == NULL ) {
		return -1;
	}
	expr_len = strlen(sel) + 64u;
	expr = (char*)xrtMalloc(expr_len);
	if ( expr == NULL ) {
		xrtFree(sel);
		xcdp_set_conn_error_text(conn, "out of memory while building locator count expression");
		return -1;
	}
	snprintf(expr, expr_len, "document.querySelectorAll(%s).length", sel);
	value = x_cdp_runtime_evaluate(conn, session_id, expr, timeout_ms);
	if ( value == NULL || value[0] == '\0' ) {
		count = -1;
	} else {
		count = atoi(value);
	}
	xrtFree(expr);
	xrtFree(sel);
	return count;
}

const char* x_cdp_locator_state(void* handle, const char* session_id, const char* selector, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* sel = NULL;
	char* expr = NULL;
	size_t expr_len;
	const char* value;
	if ( conn == NULL ) {
		return "";
	}
	xcdp_set_conn_error_text(conn, "");
	sel = xcdp_js_string_literal(conn, selector);
	if ( sel == NULL ) {
		return "";
	}
	expr_len = strlen(sel) + 900u;
	expr = (char*)xrtMalloc(expr_len);
	if ( expr == NULL ) {
		xrtFree(sel);
		xcdp_set_conn_error_text(conn, "out of memory while building locator state expression");
		return "";
	}
	snprintf(expr, expr_len, "(function(){var e=document.querySelector(%s);if(!e)return {count:0,visible:false,enabled:false,editable:false,receivesEvents:false};var r=e.getBoundingClientRect();var s=getComputedStyle(e);var visible=!!(r.width&&r.height&&s.visibility!=='hidden'&&s.display!=='none');var enabled=!e.disabled;var editable=enabled&&(!e.readOnly&&(e.isContentEditable||/^(INPUT|TEXTAREA|SELECT)$/.test(e.tagName)));var cx=r.left+r.width/2,cy=r.top+r.height/2;var hit=document.elementFromPoint(cx,cy);var receives=!!(hit&&(hit===e||e.contains(hit)));return {count:document.querySelectorAll(%s).length,visible:visible,enabled:enabled,editable:editable,receivesEvents:receives};})()", sel, sel);
	value = x_cdp_runtime_evaluate(conn, session_id, expr, timeout_ms);
	xrtFree(expr);
	xrtFree(sel);
	return value;
}

int x_cdp_expect_ok(void* handle, int ok_value)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	return ok_value ? 1 : 0;
}

const char* x_cdp_page_content(void* handle, const char* session_id, int timeout_ms)
{
	return x_cdp_runtime_evaluate(handle, session_id, "document.documentElement ? document.documentElement.outerHTML : \"\"", timeout_ms);
}

int x_cdp_page_snapshot(void* handle, const char* session_id, const char* path, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	const char* html;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	if ( path == NULL || path[0] == '\0' ) {
		xcdp_set_conn_error_text(conn, "snapshot output path is empty");
		return 0;
	}
	html = x_cdp_page_content(conn, session_id, timeout_ms);
	if ( html == NULL || html[0] == '\0' ) {
		return 0;
	}
	if ( !xrtFilePutAll((str)path, (ptr)html, strlen(html)) ) {
		xcdp_set_conn_error_text(conn, "failed to write DOM snapshot to output path");
		return 0;
	}
	return 1;
}

int x_cdp_page_screenshot(void* handle, const char* session_id, const char* path, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	const char* response;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	response = x_cdp_send_session(conn, session_id, "Page.captureScreenshot", "{\"format\":\"png\",\"fromSurface\":true}", timeout_ms);
	return xcdp_response_save_base64_field(conn, response, "data", 4u, path);
}

int x_cdp_page_pdf(void* handle, const char* session_id, const char* path, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	const char* response;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	response = x_cdp_send_session(conn, session_id, "Page.printToPDF", "{\"printBackground\":true}", timeout_ms);
	return xcdp_response_save_base64_field(conn, response, "data", 4u, path);
}

int x_cdp_page_start_screencast(void* handle, const char* session_id, const char* format, int quality, int every_nth_frame, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_screencast_params(conn, format, quality, every_nth_frame);
	if ( params == NULL ) {
		return 0;
	}
	response = x_cdp_send_session(conn, session_id, "Page.startScreencast", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_has_result(conn, response);
}

int x_cdp_page_stop_screencast(void* handle, const char* session_id, int timeout_ms)
{
	const char* response = x_cdp_send_session(handle, session_id, "Page.stopScreencast", "{}", timeout_ms);
	xcdp_conn* conn = xcdp_checked_conn(handle);
	return xcdp_response_has_result(conn, response);
}

const char* x_cdp_wait_screencast_frame(void* handle, const char* session_id, int timeout_ms)
{
	return x_cdp_wait_event(handle, session_id, "Page.screencastFrame", timeout_ms);
}

int x_cdp_screencast_frame_ack(void* handle, const char* session_id, int frame_session_id, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_screencast_ack_params(conn, frame_session_id);
	if ( params == NULL ) {
		return 0;
	}
	response = x_cdp_send_session(conn, session_id, "Page.screencastFrameAck", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_has_result(conn, response);
}

const char* x_cdp_accessibility_snapshot(void* handle, const char* session_id, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	const char* response;
	if ( conn == NULL ) {
		return "";
	}
	xcdp_set_conn_error_text(conn, "");
	response = x_cdp_send_session(conn, session_id, "Accessibility.getFullAXTree", "{}", timeout_ms);
	return xcdp_response_result_json(conn, response);
}

const char* x_cdp_page_frame_tree(void* handle, const char* session_id, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	const char* response;
	if ( conn == NULL ) {
		return "";
	}
	xcdp_set_conn_error_text(conn, "");
	response = x_cdp_send_session(conn, session_id, "Page.getFrameTree", "{}", timeout_ms);
	return xcdp_response_result_json(conn, response);
}

int x_cdp_page_failure_artifacts(void* handle, const char* session_id, const char* screenshot_path, const char* snapshot_path, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	if ( (screenshot_path == NULL || screenshot_path[0] == '\0') &&
		(snapshot_path == NULL || snapshot_path[0] == '\0') ) {
		xcdp_set_conn_error_text(conn, "failure artifact paths are empty");
		return 0;
	}
	if ( screenshot_path != NULL && screenshot_path[0] != '\0' ) {
		if ( !x_cdp_page_screenshot(conn, session_id, screenshot_path, timeout_ms) ) {
			return 0;
		}
	}
	if ( snapshot_path != NULL && snapshot_path[0] != '\0' ) {
		if ( !x_cdp_page_snapshot(conn, session_id, snapshot_path, timeout_ms) ) {
			return 0;
		}
	}
	return 1;
}

int x_cdp_page_set_viewport(void* handle, const char* session_id, int width, int height, int device_scale_factor, int is_mobile, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_viewport_params(conn, width, height, device_scale_factor, is_mobile);
	if ( params == NULL ) {
		return 0;
	}
	response = x_cdp_send_session(conn, session_id, "Emulation.setDeviceMetricsOverride", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_has_result(conn, response);
}

int x_cdp_page_set_user_agent(void* handle, const char* session_id, const char* user_agent, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_target_params(conn, "userAgent", 9u, user_agent);
	if ( params == NULL ) {
		return 0;
	}
	response = x_cdp_send_session(conn, session_id, "Emulation.setUserAgentOverride", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_has_result(conn, response);
}

int x_cdp_page_set_locale(void* handle, const char* session_id, const char* locale, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_target_params(conn, "locale", 6u, locale);
	if ( params == NULL ) {
		return 0;
	}
	response = x_cdp_send_session(conn, session_id, "Emulation.setLocaleOverride", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_has_result(conn, response);
}

int x_cdp_page_set_timezone(void* handle, const char* session_id, const char* timezone_id, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_target_params(conn, "timezoneId", 10u, timezone_id);
	if ( params == NULL ) {
		return 0;
	}
	response = x_cdp_send_session(conn, session_id, "Emulation.setTimezoneOverride", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_has_result(conn, response);
}

int x_cdp_page_set_geolocation(void* handle, const char* session_id, int latitude_e7, int longitude_e7, int accuracy, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_geolocation_params(conn, latitude_e7, longitude_e7, accuracy);
	if ( params == NULL ) {
		return 0;
	}
	response = x_cdp_send_session(conn, session_id, "Emulation.setGeolocationOverride", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_has_result(conn, response);
}

int x_cdp_page_set_offline(void* handle, const char* session_id, int offline, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_network_conditions_params(conn, offline);
	if ( params == NULL ) {
		return 0;
	}
	response = x_cdp_send_session(conn, session_id, "Network.emulateNetworkConditions", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_has_result(conn, response);
}

int x_cdp_network_set_cache_disabled(void* handle, const char* session_id, int disabled, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_cache_disabled_params(conn, disabled);
	if ( params == NULL ) {
		return 0;
	}
	response = x_cdp_send_session(conn, session_id, "Network.setCacheDisabled", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_has_result(conn, response);
}

int x_cdp_network_set_extra_http_headers(void* handle, const char* session_id, const char* headers_json, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_extra_http_headers_params(conn, headers_json);
	if ( params == NULL ) {
		return 0;
	}
	response = x_cdp_send_session(conn, session_id, "Network.setExtraHTTPHeaders", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_has_result(conn, response);
}

int x_cdp_input_dispatch_key_event(void* handle, const char* session_id, const char* type, const char* key, const char* text, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_key_event_params(conn, type, key, text);
	if ( params == NULL ) {
		return 0;
	}
	response = x_cdp_send_session(conn, session_id, "Input.dispatchKeyEvent", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_has_result(conn, response);
}

int x_cdp_input_insert_text(void* handle, const char* session_id, const char* text, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_insert_text_params(conn, text);
	if ( params == NULL ) {
		return 0;
	}
	response = x_cdp_send_session(conn, session_id, "Input.insertText", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_has_result(conn, response);
}

int x_cdp_input_dispatch_mouse_event(void* handle, const char* session_id, const char* type, int x, int y, const char* button, int click_count, int delta_x, int delta_y, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_mouse_event_params(conn, type, x, y, button, click_count, delta_x, delta_y);
	if ( params == NULL ) {
		return 0;
	}
	response = x_cdp_send_session(conn, session_id, "Input.dispatchMouseEvent", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_has_result(conn, response);
}

int x_cdp_page_mouse_move(void* handle, const char* session_id, int x, int y, int timeout_ms)
{
	return x_cdp_input_dispatch_mouse_event(handle, session_id, "mouseMoved", x, y, "none", 0, 0, 0, timeout_ms);
}

int x_cdp_page_mouse_click(void* handle, const char* session_id, int x, int y, const char* button, int timeout_ms)
{
	if ( !x_cdp_input_dispatch_mouse_event(handle, session_id, "mousePressed", x, y, button, 1, 0, 0, timeout_ms) ) {
		return 0;
	}
	return x_cdp_input_dispatch_mouse_event(handle, session_id, "mouseReleased", x, y, button, 1, 0, 0, timeout_ms);
}

int x_cdp_page_mouse_wheel(void* handle, const char* session_id, int x, int y, int delta_x, int delta_y, int timeout_ms)
{
	return x_cdp_input_dispatch_mouse_event(handle, session_id, "mouseWheel", x, y, "none", 0, delta_x, delta_y, timeout_ms);
}

int x_cdp_network_enable(void* handle, const char* session_id, int timeout_ms)
{
	const char* response = x_cdp_send_session(handle, session_id, "Network.enable", "{}", timeout_ms);
	return (response && response[0] != '\0') ? 1 : 0;
}

int x_cdp_runtime_enable(void* handle, const char* session_id, int timeout_ms)
{
	const char* response = x_cdp_send_session(handle, session_id, "Runtime.enable", "{}", timeout_ms);
	return (response && response[0] != '\0') ? 1 : 0;
}

int x_cdp_log_enable(void* handle, const char* session_id, int timeout_ms)
{
	const char* response = x_cdp_send_session(handle, session_id, "Log.enable", "{}", timeout_ms);
	return (response && response[0] != '\0') ? 1 : 0;
}

const char* x_cdp_next_event(void* handle, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	xcdp_msg* msg = NULL;
	double deadline;
	uint32_t timeout;
	if ( conn == NULL ) {
		return "";
	}
	xcdp_set_conn_error_text(conn, "");
	timeout = timeout_ms < 0 ? XCDP_DEFAULT_SEND_TIMEOUT_MS : (uint32_t)timeout_ms;
	if ( xcdp_queue_pop_msg(conn->events, &msg, 0u) ) {
		char* owned = msg->text;
		msg->text = NULL;
		xcdp_msg_destroy(msg);
		return xcdp_set_conn_return_owned(conn, owned);
	}
	deadline = xrtTimer() + ((double)timeout / 1000.0);
	do {
		int msg_id = 0;
		int has_id;
		uint32_t wait_ms = 0u;
		double now = xrtTimer();
		if ( timeout > 0u ) {
			if ( now >= deadline ) {
				break;
			}
			wait_ms = (uint32_t)((deadline - now) * 1000.0);
			if ( wait_ms > 100u ) {
				wait_ms = 100u;
			}
			if ( wait_ms == 0u ) {
				wait_ms = 1u;
			}
		}
		msg = NULL;
		if ( !xcdp_queue_pop_msg(conn->incoming, &msg, wait_ms) ) {
			if ( timeout == 0u ) {
				break;
			}
			continue;
		}
		has_id = xcdp_message_id(msg->text, msg->len, &msg_id);
		(void)msg_id;
		if ( has_id ) {
			xcdp_pending_push(conn, msg);
			continue;
		}
		{
			char* owned = msg->text;
			msg->text = NULL;
			xcdp_msg_destroy(msg);
			return xcdp_set_conn_return_owned(conn, owned);
		}
	} while ( timeout > 0u );
	return "";
}

const char* x_cdp_wait_event(void* handle, const char* session_id, const char* method, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	xcdp_msg* msg = NULL;
	xcdp_msg* skipped_head = NULL;
	xcdp_msg* skipped_tail = NULL;
	double deadline;
	uint32_t timeout;
	const char* ret = "";
	if ( conn == NULL ) {
		return "";
	}
	xcdp_set_conn_error_text(conn, "");
	timeout = timeout_ms < 0 ? XCDP_DEFAULT_SEND_TIMEOUT_MS : (uint32_t)timeout_ms;
	deadline = xrtTimer() + ((double)timeout / 1000.0);
	while ( xcdp_queue_pop_msg(conn->events, &msg, 0u) ) {
		if ( xcdp_message_event_matches(msg->text, msg->len, session_id, method) ) {
			char* owned = msg->text;
			msg->text = NULL;
			xcdp_msg_destroy(msg);
			ret = xcdp_set_conn_return_owned(conn, owned);
			goto done;
		}
		msg->next = NULL;
		if ( skipped_tail ) {
			skipped_tail->next = msg;
		} else {
			skipped_head = msg;
		}
		skipped_tail = msg;
		msg = NULL;
	}
	do {
		int msg_id = 0;
		int has_id;
		uint32_t wait_ms = 0u;
		double now = xrtTimer();
		if ( timeout > 0u ) {
			if ( now >= deadline ) {
				break;
			}
			wait_ms = (uint32_t)((deadline - now) * 1000.0);
			if ( wait_ms > 100u ) {
				wait_ms = 100u;
			}
			if ( wait_ms == 0u ) {
				wait_ms = 1u;
			}
		}
		if ( !xcdp_queue_pop_msg(conn->incoming, &msg, wait_ms) ) {
			if ( timeout == 0u ) {
				break;
			}
			continue;
		}
		has_id = xcdp_message_id(msg->text, msg->len, &msg_id);
		(void)msg_id;
		if ( has_id ) {
			xcdp_pending_push(conn, msg);
			msg = NULL;
			continue;
		}
		if ( xcdp_message_event_matches(msg->text, msg->len, session_id, method) ) {
			char* owned = msg->text;
			msg->text = NULL;
			xcdp_msg_destroy(msg);
			ret = xcdp_set_conn_return_owned(conn, owned);
			goto done;
		}
		msg->next = NULL;
		if ( skipped_tail ) {
			skipped_tail->next = msg;
		} else {
			skipped_head = msg;
		}
		skipped_tail = msg;
		msg = NULL;
	} while ( timeout > 0u );
	xcdp_set_conn_errorf(conn, "CDP event timeout: method=%s", method ? method : "");
done:
	while ( skipped_head ) {
		xcdp_msg* next = skipped_head->next;
		skipped_head->next = NULL;
		xcdp_queue_push_msg(conn->events, skipped_head);
		skipped_head = next;
	}
	return ret;
}

const char* x_cdp_wait_console(void* handle, const char* session_id, int timeout_ms)
{
	return x_cdp_wait_event(handle, session_id, "Runtime.consoleAPICalled", timeout_ms);
}

const char* x_cdp_wait_exception(void* handle, const char* session_id, int timeout_ms)
{
	return x_cdp_wait_event(handle, session_id, "Runtime.exceptionThrown", timeout_ms);
}

const char* x_cdp_wait_log_entry(void* handle, const char* session_id, int timeout_ms)
{
	return x_cdp_wait_event(handle, session_id, "Log.entryAdded", timeout_ms);
}

const char* x_cdp_wait_dialog(void* handle, const char* session_id, int timeout_ms)
{
	return x_cdp_wait_event(handle, session_id, "Page.javascriptDialogOpening", timeout_ms);
}

static int xcdp_handle_dialog(void* handle, const char* session_id, int accept, const char* prompt_text, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_dialog_accept_params(conn, accept, prompt_text);
	if ( params == NULL ) {
		return 0;
	}
	response = x_cdp_send_session(conn, session_id, "Page.handleJavaScriptDialog", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_has_result(conn, response);
}

int x_cdp_dialog_accept(void* handle, const char* session_id, int timeout_ms)
{
	return xcdp_handle_dialog(handle, session_id, 1, NULL, timeout_ms);
}

int x_cdp_dialog_dismiss(void* handle, const char* session_id, int timeout_ms)
{
	return xcdp_handle_dialog(handle, session_id, 0, NULL, timeout_ms);
}

int x_cdp_dialog_accept_text(void* handle, const char* session_id, const char* prompt_text, int timeout_ms)
{
	return xcdp_handle_dialog(handle, session_id, 1, prompt_text ? prompt_text : "", timeout_ms);
}

const char* x_cdp_wait_network_event(void* handle, const char* session_id, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	xcdp_msg* msg = NULL;
	xcdp_msg* skipped_head = NULL;
	xcdp_msg* skipped_tail = NULL;
	double deadline;
	uint32_t timeout;
	const char* ret = "";
	if ( conn == NULL ) {
		return "";
	}
	xcdp_set_conn_error_text(conn, "");
	timeout = timeout_ms < 0 ? XCDP_DEFAULT_SEND_TIMEOUT_MS : (uint32_t)timeout_ms;
	deadline = xrtTimer() + ((double)timeout / 1000.0);
	while ( xcdp_queue_pop_msg(conn->events, &msg, 0u) ) {
		if ( xcdp_message_event_method_starts_with(msg->text, msg->len, session_id, "Network.") ) {
			char* owned = msg->text;
			msg->text = NULL;
			xcdp_msg_destroy(msg);
			ret = xcdp_set_conn_return_owned(conn, owned);
			goto done;
		}
		msg->next = NULL;
		if ( skipped_tail ) {
			skipped_tail->next = msg;
		} else {
			skipped_head = msg;
		}
		skipped_tail = msg;
		msg = NULL;
	}
	do {
		int msg_id = 0;
		int has_id;
		uint32_t wait_ms = 0u;
		double now = xrtTimer();
		if ( timeout > 0u ) {
			if ( now >= deadline ) {
				break;
			}
			wait_ms = (uint32_t)((deadline - now) * 1000.0);
			if ( wait_ms > 100u ) {
				wait_ms = 100u;
			}
			if ( wait_ms == 0u ) {
				wait_ms = 1u;
			}
		}
		if ( !xcdp_queue_pop_msg(conn->incoming, &msg, wait_ms) ) {
			if ( timeout == 0u ) {
				break;
			}
			continue;
		}
		has_id = xcdp_message_id(msg->text, msg->len, &msg_id);
		(void)msg_id;
		if ( has_id ) {
			xcdp_pending_push(conn, msg);
			msg = NULL;
			continue;
		}
		if ( xcdp_message_event_method_starts_with(msg->text, msg->len, session_id, "Network.") ) {
			char* owned = msg->text;
			msg->text = NULL;
			xcdp_msg_destroy(msg);
			ret = xcdp_set_conn_return_owned(conn, owned);
			goto done;
		}
		msg->next = NULL;
		if ( skipped_tail ) {
			skipped_tail->next = msg;
		} else {
			skipped_head = msg;
		}
		skipped_tail = msg;
		msg = NULL;
	} while ( timeout > 0u );
	xcdp_set_conn_error_text(conn, "CDP network event timeout");
done:
	while ( skipped_head ) {
		xcdp_msg* next = skipped_head->next;
		skipped_head->next = NULL;
		xcdp_queue_push_msg(conn->events, skipped_head);
		skipped_head = next;
	}
	return ret;
}

static const char* xcdp_wait_network_event(void* handle, const char* session_id, const char* method, const char* url_part, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	xcdp_msg* msg = NULL;
	xcdp_msg* skipped_head = NULL;
	xcdp_msg* skipped_tail = NULL;
	double deadline;
	uint32_t timeout;
	const char* ret = "";
	if ( conn == NULL ) {
		return "";
	}
	xcdp_set_conn_error_text(conn, "");
	timeout = timeout_ms < 0 ? XCDP_DEFAULT_SEND_TIMEOUT_MS : (uint32_t)timeout_ms;
	deadline = xrtTimer() + ((double)timeout / 1000.0);
	while ( xcdp_queue_pop_msg(conn->events, &msg, 0u) ) {
		if ( xcdp_message_network_url_matches(msg->text, msg->len, session_id, method, url_part) ) {
			char* owned = msg->text;
			msg->text = NULL;
			xcdp_msg_destroy(msg);
			ret = xcdp_set_conn_return_owned(conn, owned);
			goto done;
		}
		msg->next = NULL;
		if ( skipped_tail ) {
			skipped_tail->next = msg;
		} else {
			skipped_head = msg;
		}
		skipped_tail = msg;
		msg = NULL;
	}
	do {
		int msg_id = 0;
		int has_id;
		uint32_t wait_ms = 0u;
		double now = xrtTimer();
		if ( timeout > 0u ) {
			if ( now >= deadline ) {
				break;
			}
			wait_ms = (uint32_t)((deadline - now) * 1000.0);
			if ( wait_ms > 100u ) {
				wait_ms = 100u;
			}
			if ( wait_ms == 0u ) {
				wait_ms = 1u;
			}
		}
		if ( !xcdp_queue_pop_msg(conn->incoming, &msg, wait_ms) ) {
			if ( timeout == 0u ) {
				break;
			}
			continue;
		}
		has_id = xcdp_message_id(msg->text, msg->len, &msg_id);
		(void)msg_id;
		if ( has_id ) {
			xcdp_pending_push(conn, msg);
			msg = NULL;
			continue;
		}
		if ( xcdp_message_network_url_matches(msg->text, msg->len, session_id, method, url_part) ) {
			char* owned = msg->text;
			msg->text = NULL;
			xcdp_msg_destroy(msg);
			ret = xcdp_set_conn_return_owned(conn, owned);
			goto done;
		}
		msg->next = NULL;
		if ( skipped_tail ) {
			skipped_tail->next = msg;
		} else {
			skipped_head = msg;
		}
		skipped_tail = msg;
		msg = NULL;
	} while ( timeout > 0u );
	xcdp_set_conn_errorf(conn, "CDP network event timeout: method=%s url=%s", method ? method : "", url_part ? url_part : "");
done:
	while ( skipped_head ) {
		xcdp_msg* next = skipped_head->next;
		skipped_head->next = NULL;
		xcdp_queue_push_msg(conn->events, skipped_head);
		skipped_head = next;
	}
	return ret;
}

const char* x_cdp_wait_request(void* handle, const char* session_id, const char* url_part, int timeout_ms)
{
	return xcdp_wait_network_event(handle, session_id, "Network.requestWillBeSent", url_part, timeout_ms);
}

const char* x_cdp_wait_response(void* handle, const char* session_id, const char* url_part, int timeout_ms)
{
	return xcdp_wait_network_event(handle, session_id, "Network.responseReceived", url_part, timeout_ms);
}

const char* x_cdp_wait_route(void* handle, const char* session_id, const char* url_part, int timeout_ms)
{
	return xcdp_wait_network_event(handle, session_id, "Fetch.requestPaused", url_part, timeout_ms);
}

const char* x_cdp_network_get_response_body(void* handle, const char* session_id, const char* request_id, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return "";
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_target_params(conn, "requestId", 9u, request_id);
	if ( params == NULL ) {
		return "";
	}
	response = x_cdp_send_session(conn, session_id, "Network.getResponseBody", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_result_json(conn, response);
}

int x_cdp_fetch_enable(void* handle, const char* session_id, const char* url_pattern, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_fetch_enable_params(conn, url_pattern);
	if ( params == NULL ) {
		return 0;
	}
	response = x_cdp_send_session(conn, session_id, "Fetch.enable", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_has_result(conn, response);
}

int x_cdp_fetch_disable(void* handle, const char* session_id, int timeout_ms)
{
	const char* response = x_cdp_send_session(handle, session_id, "Fetch.disable", "{}", timeout_ms);
	xcdp_conn* conn = xcdp_checked_conn(handle);
	return xcdp_response_has_result(conn, response);
}

int x_cdp_fetch_continue(void* handle, const char* session_id, const char* request_id, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_target_params(conn, "requestId", 9u, request_id);
	if ( params == NULL ) {
		return 0;
	}
	response = x_cdp_send_session(conn, session_id, "Fetch.continueRequest", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_has_result(conn, response);
}

int x_cdp_fetch_continue_ex(void* handle, const char* session_id, const char* request_id, const char* options_json, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_fetch_continue_ex_params(conn, request_id, options_json);
	if ( params == NULL ) {
		return 0;
	}
	response = x_cdp_send_session(conn, session_id, "Fetch.continueRequest", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_has_result(conn, response);
}

int x_cdp_fetch_abort(void* handle, const char* session_id, const char* request_id, const char* error_reason, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_fetch_abort_params(conn, request_id, error_reason);
	if ( params == NULL ) {
		return 0;
	}
	response = x_cdp_send_session(conn, session_id, "Fetch.failRequest", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_has_result(conn, response);
}

int x_cdp_fetch_fulfill(void* handle, const char* session_id, const char* request_id, int status, const char* body_base64, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_fetch_fulfill_params(conn, request_id, status, body_base64);
	if ( params == NULL ) {
		return 0;
	}
	response = x_cdp_send_session(conn, session_id, "Fetch.fulfillRequest", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_has_result(conn, response);
}

int x_cdp_fetch_fulfill_text(void* handle, const char* session_id, const char* request_id, int status, const char* content_type, const char* body_text, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_fetch_fulfill_text_params(conn, request_id, status, content_type, body_text);
	if ( params == NULL ) {
		return 0;
	}
	response = x_cdp_send_session(conn, session_id, "Fetch.fulfillRequest", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_has_result(conn, response);
}

int x_cdp_fetch_fulfill_ex(void* handle, const char* session_id, const char* request_id, const char* options_json, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char* params = NULL;
	const char* response;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	params = xcdp_build_fetch_fulfill_ex_params(conn, request_id, options_json);
	if ( params == NULL ) {
		return 0;
	}
	response = x_cdp_send_session(conn, session_id, "Fetch.fulfillRequest", params, timeout_ms);
	xrtFree(params);
	return xcdp_response_has_result(conn, response);
}

int x_cdp_route_apply(void* handle, const char* session_id, const char* request_id, const char* action_json, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	xvalue root = NULL;
	xvalue action_value = NULL;
	xvalue reason_value = NULL;
	char* action = NULL;
	char* reason = NULL;
	char* options = NULL;
	int ok = 0;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	if ( action_json == NULL || action_json[0] == '\0' ) {
		xcdp_set_conn_error_text(conn, "route action JSON is empty");
		return 0;
	}
	root = xrtParseJSON((str)action_json, strlen(action_json));
	if ( !xvoIsTable(root) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "route action JSON must be an object");
		return 0;
	}
	action_value = xvoTableGetValue(root, "action", 6u);
	if ( !xvoIsText(action_value) ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "route action JSON must contain string action");
		return 0;
	}
	action = xcdp_strndup_xrt(xvoGetText(action_value), xvoGetSize(action_value));
	if ( action == NULL ) {
		xvoUnref(root);
		xcdp_set_conn_error_text(conn, "out of memory while reading route action");
		return 0;
	}
	if ( strcmp(action, "abort") == 0 ) {
		reason_value = xvoTableGetValue(root, "errorReason", 11u);
		if ( xvoIsText(reason_value) ) {
			reason = xcdp_strndup_xrt(xvoGetText(reason_value), xvoGetSize(reason_value));
			if ( reason == NULL ) {
				xcdp_set_conn_error_text(conn, "out of memory while reading route abort reason");
			} else {
				ok = x_cdp_fetch_abort(conn, session_id, request_id, reason, timeout_ms);
			}
		} else {
			ok = x_cdp_fetch_abort(conn, session_id, request_id, "Failed", timeout_ms);
		}
	} else if ( strcmp(action, "continue") == 0 ) {
		options = xcdp_route_action_options_json(conn, root);
		if ( options != NULL ) {
			ok = x_cdp_fetch_continue_ex(conn, session_id, request_id, options, timeout_ms);
		}
	} else if ( strcmp(action, "fulfill") == 0 ) {
		options = xcdp_route_action_options_json(conn, root);
		if ( options != NULL ) {
			ok = x_cdp_fetch_fulfill_ex(conn, session_id, request_id, options, timeout_ms);
		}
	} else {
		xcdp_set_conn_errorf(conn, "unsupported route action: %s", action);
	}
	xvoUnref(root);
	xrtFree(action);
	xrtFree(reason);
	xrtFree(options);
	return ok;
}

int x_cdp_page_reload(void* handle, const char* session_id, int ignore_cache, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	char params[32];
	const char* response;
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	snprintf(params, sizeof(params), "{\"ignoreCache\":%s}", ignore_cache ? "true" : "false");
	response = x_cdp_send_session(conn, session_id, "Page.reload", params, timeout_ms);
	return xcdp_response_has_result(conn, response);
}

int x_cdp_page_bring_to_front(void* handle, const char* session_id, int timeout_ms)
{
	const char* response = x_cdp_send_session(handle, session_id, "Page.bringToFront", "{}", timeout_ms);
	xcdp_conn* conn = xcdp_checked_conn(handle);
	return xcdp_response_has_result(conn, response);
}

int x_cdp_page_wait_dom_content_loaded(void* handle, const char* session_id, int timeout_ms)
{
	const char* event = x_cdp_wait_event(handle, session_id, "Page.domContentEventFired", timeout_ms);
	return (event && event[0] != '\0') ? 1 : 0;
}

int x_cdp_page_wait_load(void* handle, const char* session_id, int timeout_ms)
{
	const char* event = x_cdp_wait_event(handle, session_id, "Page.loadEventFired", timeout_ms);
	return (event && event[0] != '\0') ? 1 : 0;
}

static int xcdp_page_wait_load_state(xcdp_conn* conn, const char* session_id, const char* wait_until, int timeout_ms)
{
	const char* state = (wait_until && wait_until[0]) ? wait_until : "load";
	if ( strcmp(state, "none") == 0 || strcmp(state, "commit") == 0 ) {
		return 1;
	}
	if ( strcmp(state, "load") == 0 ) {
		return x_cdp_page_wait_load(conn, session_id, timeout_ms);
	}
	if ( strcmp(state, "domcontentloaded") == 0 ) {
		return x_cdp_page_wait_dom_content_loaded(conn, session_id, timeout_ms);
	}
	xcdp_set_conn_errorf(conn, "unsupported page load state: %s", state);
	return 0;
}

int x_cdp_page_wait_load_state(void* handle, const char* session_id, const char* wait_until, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_set_conn_error_text(conn, "");
	return xcdp_page_wait_load_state(conn, session_id, wait_until, timeout_ms);
}

const char* x_cdp_page_goto(void* handle, const char* session_id, const char* url, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	const char* frame_id;
	char* frame_copy = NULL;
	if ( conn == NULL ) {
		return "";
	}
	xcdp_set_conn_error_text(conn, "");
	frame_id = x_cdp_page_navigate(conn, session_id, url, timeout_ms);
	if ( frame_id == NULL || frame_id[0] == '\0' ) {
		return "";
	}
	frame_copy = xcdp_strndup_xrt(frame_id, strlen(frame_id));
	if ( frame_copy == NULL ) {
		xcdp_set_conn_error_text(conn, "out of memory while preserving Page.navigate frameId");
		return "";
	}
	if ( !x_cdp_page_wait_load(conn, session_id, timeout_ms) ) {
		xrtFree(frame_copy);
		return "";
	}
	return xcdp_set_conn_return_owned(conn, frame_copy);
}

const char* x_cdp_page_goto_wait(void* handle, const char* session_id, const char* url, const char* wait_until, int timeout_ms)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	const char* frame_id;
	char* frame_copy = NULL;
	if ( conn == NULL ) {
		return "";
	}
	xcdp_set_conn_error_text(conn, "");
	frame_id = x_cdp_page_navigate(conn, session_id, url, timeout_ms);
	if ( frame_id == NULL || frame_id[0] == '\0' ) {
		return "";
	}
	frame_copy = xcdp_strndup_xrt(frame_id, strlen(frame_id));
	if ( frame_copy == NULL ) {
		xcdp_set_conn_error_text(conn, "out of memory while preserving Page.navigate frameId");
		return "";
	}
	if ( !xcdp_page_wait_load_state(conn, session_id, wait_until, timeout_ms) ) {
		xrtFree(frame_copy);
		return "";
	}
	return xcdp_set_conn_return_owned(conn, frame_copy);
}

int x_cdp_close(void* handle)
{
	xcdp_conn* conn = xcdp_checked_conn(handle);
	if ( conn == NULL ) {
		return 0;
	}
	xcdp_conn_close_resources(conn);
	return 1;
}

int x_cdp_dispose(void* handle)
{
	xcdp_conn* conn = (xcdp_conn*)handle;
	if ( conn == NULL || conn->magic != XCDP_MAGIC ) {
		return 0;
	}
	xcdp_conn_close_resources(conn);
	xcdp_queue_drain(conn->incoming);
	xcdp_queue_drain(conn->events);
	xcdp_pending_drain(conn);
	if ( conn->incoming ) {
		xrtMPSCQWaitDestroy(conn->incoming);
	}
	if ( conn->events ) {
		xrtMPSCQWaitDestroy(conn->events);
	}
	if ( conn->last_return ) {
		xrtFree(conn->last_return);
	}
	if ( conn->lock ) {
		xrtMutexDestroy(conn->lock);
	}
	conn->magic = 0u;
	xrtFree(conn);
	return 1;
}
