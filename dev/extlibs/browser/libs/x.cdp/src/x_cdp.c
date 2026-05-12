#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
			xrtWsClientStop(conn->client);
			xrtWsClientDestroy(conn->client);
			conn->client = NULL;
		}
	}
	if ( conn->engine ) {
		xrtNetEngineStop(conn->engine);
	}
	if ( conn->client ) {
		xrtWsClientDestroy(conn->client);
		conn->client = NULL;
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
	xnetengineconfig engine_cfg;
	xnetengine* engine = NULL;
	xnet_result status = XRT_NET_ERROR;
	const char* ret = "";
	uint32_t timeout = xcdp_timeout_or_default(timeout_ms, XCDP_DEFAULT_HTTP_TIMEOUT_MS);
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
	if ( !xrtHttpRequestSetMethod(&req, "GET") || !xrtHttpRequestSetURL(&req, url) ) {
		xrtHttpRequestUnit(&req);
		xrtNetEngineStop(engine);
		xrtNetEngineDestroy(engine);
		xcdp_set_global_error_text("failed to initialize xrt HTTP request");
		return "";
	}
	xrtHttpRequestSetTimeout(&req, timeout);
	xrtHttpRequestSetIdleTimeout(&req, timeout);
	resp = xrtHttpExecuteSync(engine, &req, &status);
	if ( status != XRT_NET_OK || resp == NULL ) {
		xcdp_set_global_errorf("xrt HTTP GET failed: status=%d", (int)status);
		xrtHttpRequestUnit(&req);
		xrtNetEngineStop(engine);
		xrtNetEngineDestroy(engine);
		return "";
	}
	if ( resp->iStatusCode < 200u || resp->iStatusCode >= 300u ) {
		xcdp_set_global_errorf("HTTP GET returned status %u", (unsigned)resp->iStatusCode);
		xrtHttpResponseDestroy(resp);
		xrtHttpRequestUnit(&req);
		xrtNetEngineStop(engine);
		xrtNetEngineDestroy(engine);
		return "";
	}
	ret = xcdp_set_global_return_copy(resp->pBody ? resp->pBody : "", resp->pBody ? resp->iBodyLen : 0u);
	xrtHttpResponseDestroy(resp);
	xrtHttpRequestUnit(&req);
	xrtNetEngineStop(engine);
	xrtNetEngineDestroy(engine);
	return ret;
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
