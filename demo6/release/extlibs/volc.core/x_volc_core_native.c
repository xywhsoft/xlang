#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef XRT_MODULE_ONCE
#define XRT_MODULE_ONCE
#endif
#ifndef XRT_MODULE_THREAD_KEY
#define XRT_MODULE_THREAD_KEY
#endif
#ifndef XRT_MODULE_TIME_TEXT
#define XRT_MODULE_TIME_TEXT
#endif
#ifndef XRT_MODULE_NUMBER_INTEGER
#define XRT_MODULE_NUMBER_INTEGER
#endif
#include "xrt.h"
#include "include/volc_core_native.h"
#include "../xhttp.client/include/xhttp_client_package.h"

#define XVOLC_CORE_VERSION "0.1.0"
#define XVOLC_DEFAULT_TIMEOUT_MS 30000u

typedef struct xvolc_pair {
	char* key;
	char* value;
} xvolc_pair;

typedef struct xvolc_buf {
	char* data;
	size_t len;
	size_t cap;
} xvolc_buf;

typedef struct xvolc_state {
	char* returns[8];
	size_t return_slot;
	char error[512];
	int last_status;
} xvolc_state;

static xonce g_xvolc_once = XRT_ONCE_INIT;
static xthreadkey* g_xvolc_key = NULL;

static void xvolc_state_destroy(ptr data)
{
	xvolc_state* state = (xvolc_state*)data;
	size_t i;
	if ( state == NULL ) return;
	for ( i = 0; i < sizeof(state->returns) / sizeof(state->returns[0]); ++i ) {
		xrtFree(state->returns[i]);
	}
	xrtFree(state);
}

static bool xvolc_key_create(ptr unused)
{
	(void)unused;
	g_xvolc_key = xrtThreadKeyCreate(xvolc_state_destroy);
	return g_xvolc_key != NULL;
}

/* 结果槽和诊断跟随原生线程；线程退出时由 XRT 销毁。 */
static xvolc_state* xvolc_state_get(void)
{
	xvolc_state* state;
	if ( !xrtOnce(&g_xvolc_once, xvolc_key_create, NULL) ) return NULL;
	state = (xvolc_state*)xrtThreadKeyGet(g_xvolc_key);
	if ( state != NULL ) return state;
	state = (xvolc_state*)xrtCalloc(1u, sizeof(*state));
	if ( state == NULL ) return NULL;
	if ( !xrtThreadKeySet(g_xvolc_key, state) ) {
		xvolc_state_destroy(state);
		return NULL;
	}
	return state;
}

static void xvolc_set_error(const char* text)
{
	xvolc_state* state = xvolc_state_get();
	size_t len;
	if ( state == NULL ) return;
	if ( text == NULL ) {
		text = "";
	}
	if ( text[0] == '\0' ) xrtClearError();
	len = strlen(text);
	if ( len >= sizeof(state->error) ) {
		len = sizeof(state->error) - 1u;
	}
	if ( len > 0u ) {
		memcpy(state->error, text, len);
	}
	state->error[len] = '\0';
}

static void xvolc_set_errorf(const char* fmt, ...)
{
	xvolc_state* state = xvolc_state_get();
	va_list ap;
	if ( state == NULL ) return;
	va_start(ap, fmt);
	vsnprintf(state->error, sizeof(state->error), fmt, ap);
	va_end(ap);
	state->error[sizeof(state->error) - 1u] = '\0';
}

static const char* xvolc_return_copy(const char* text, size_t len)
{
	xvolc_state* state = xvolc_state_get();
	char* copy;
	size_t slot;
	if ( state == NULL ) return "";
	if ( text == NULL ) {
		text = "";
		len = 0u;
	}
	if ( len == SIZE_MAX ) {
		xvolc_set_error("result is too large");
		return "";
	}
	copy = (char*)xrtMalloc(len + 1u);
	if ( copy == NULL ) {
		xvolc_set_error("out of memory");
		return "";
	}
	if ( len > 0u ) {
		memcpy(copy, text, len);
	}
	copy[len] = '\0';
	slot = state->return_slot % (sizeof(state->returns) / sizeof(state->returns[0]));
	state->return_slot += 1u;
	if ( state->returns[slot] != NULL ) {
		xrtFree(state->returns[slot]);
	}
	state->returns[slot] = copy;
	return state->returns[slot];
}

static const char* xvolc_return_text(const char* text)
{
	return xvolc_return_copy(text, text ? strlen(text) : 0u);
}

/* XLang 消费结果槽时直接取走分配；静态失败文本则复制一次。 */
void* x_volc_core_take_result(const char* text)
{
	xvolc_state* state = xvolc_state_get();
	size_t i;
	if (state == NULL) return NULL;
	if (text == NULL) text = "";
	for (i = 0u; i < sizeof(state->returns) / sizeof(state->returns[0]); ++i) {
		if (state->returns[i] == text) {
			state->returns[i] = NULL;
			return (void*)text;
		}
	}
	return xrtMemDup(text, strlen(text) + 1u);
}

static void xvolc_buf_init(xvolc_buf* b)
{
	b->data = NULL;
	b->len = 0u;
	b->cap = 0u;
}

static void xvolc_buf_free(xvolc_buf* b)
{
	if ( b->data != NULL ) {
		xrtFree(b->data);
	}
	b->data = NULL;
	b->len = 0u;
	b->cap = 0u;
}

static int xvolc_buf_reserve(xvolc_buf* b, size_t need)
{
	char* next;
	size_t cap;
	if ( need <= b->cap ) {
		return 1;
	}
	cap = b->cap ? b->cap : 256u;
	while ( cap < need ) {
		if ( cap > SIZE_MAX / 2u ) {
			cap = need;
			break;
		}
		cap *= 2u;
	}
	next = (char*)xrtRealloc(b->data, cap);
	if ( next == NULL ) {
		xvolc_set_error("out of memory");
		return 0;
	}
	b->data = next;
	b->cap = cap;
	return 1;
}

static int xvolc_buf_append_n(xvolc_buf* b, const char* text, size_t len)
{
	if ( len == 0u ) {
		return 1;
	}
	if ( len >= SIZE_MAX - b->len ) {
		xvolc_set_error("result is too large");
		return 0;
	}
	if ( !xvolc_buf_reserve(b, b->len + len + 1u) ) {
		return 0;
	}
	memcpy(b->data + b->len, text, len);
	b->len += len;
	b->data[b->len] = '\0';
	return 1;
}

static int xvolc_buf_append(xvolc_buf* b, const char* text)
{
	return xvolc_buf_append_n(b, text ? text : "", text ? strlen(text) : 0u);
}

static int xvolc_buf_appendf(xvolc_buf* b, const char* fmt, ...)
{
	va_list ap;
	va_list ap2;
	int n;
	if ( fmt == NULL ) {
		return 1;
	}
	va_start(ap, fmt);
	va_copy(ap2, ap);
	n = vsnprintf(NULL, 0u, fmt, ap);
	va_end(ap);
	if ( n < 0 ) {
		va_end(ap2);
		xvolc_set_error("format failed");
		return 0;
	}
	if ( (size_t)n >= SIZE_MAX - b->len ) {
		va_end(ap2);
		xvolc_set_error("result is too large");
		return 0;
	}
	if ( !xvolc_buf_reserve(b, b->len + (size_t)n + 1u) ) {
		va_end(ap2);
		return 0;
	}
	vsnprintf(b->data + b->len, (size_t)n + 1u, fmt, ap2);
	va_end(ap2);
	b->len += (size_t)n;
	return 1;
}

static char* xvolc_strdup_n(const char* text, size_t len)
{
	char* copy = (char*)xrtMalloc(len + 1u);
	if ( copy == NULL ) {
		return NULL;
	}
	if ( len > 0u ) {
		memcpy(copy, text, len);
	}
	copy[len] = '\0';
	return copy;
}

static char* xvolc_strdup(const char* text)
{
	return xvolc_strdup_n(text ? text : "", text ? strlen(text) : 0u);
}

static int xvolc_parse_spec_int(const char* text, int default_value,
	const char* error_text, int* result)
{
	int64 value;

	if ( text == NULL || text[0] == '\0' ) {
		*result = default_value;
		return 1;
	}
	xrtClearError();
	if ( !xrtIntParse((xstrview){text, strlen(text)}, 10u, 0u, &value) ) {
		const xerror* error = xrtGetError();
		if ( error && xrtErrorKind(error) == XERR_MEMORY ) {
			xvolc_set_error("out of memory");
		} else {
			xrtClearError();
			xvolc_set_error(error_text);
		}
		return 0;
	}
	if ( value < INT_MIN || value > INT_MAX ) {
		xvolc_set_error(error_text);
		return 0;
	}
	*result = (int)value;
	return 1;
}

static char* xvolc_spec_get(const char* spec, const char* key)
{
	size_t key_len = key ? strlen(key) : 0u;
	const char* p = spec ? spec : "";
	if ( key_len == 0u ) return xvolc_strdup("");
	while ( *p ) {
		const char* line_end = strchr(p, '\n');
		size_t line_len = line_end ? (size_t)(line_end - p) : strlen(p);
		/* CRLF 的 CR 属于分隔符，不属于声明值。 */
		if ( line_len && p[line_len - 1u] == '\r' ) line_len -= 1u;
		if ( line_len > key_len && memcmp(p, key, key_len) == 0 && p[key_len] == '=' ) {
			return xvolc_strdup_n(p + key_len + 1u, line_len - key_len - 1u);
		}
		if ( line_end == NULL ) break;
		p = line_end + 1;
	}
	return xvolc_strdup("");
}

/* spec 是逐行声明：拒绝未知字段和重复字段，避免静默丢失请求数据。 */
enum {
	XVOLC_SPEC_ACEP_REQUEST = 1u,
	XVOLC_SPEC_ACEP_PREVIEW = 2u,
	XVOLC_SPEC_STS_REQUEST = 4u,
	XVOLC_SPEC_STS_PREVIEW = 8u,
	XVOLC_SPEC_ALL = 15u
};

static int xvolc_spec_valid(const char* spec, unsigned variant)
{
	static const struct { const char* name; unsigned variants; } keys[] = {
		{"action", XVOLC_SPEC_ACEP_REQUEST | XVOLC_SPEC_ACEP_PREVIEW},
		{"version", XVOLC_SPEC_ACEP_REQUEST | XVOLC_SPEC_ACEP_PREVIEW},
		{"access_key", XVOLC_SPEC_ALL}, {"secret_key", XVOLC_SPEC_ALL},
		{"session_token", XVOLC_SPEC_ACEP_REQUEST | XVOLC_SPEC_ACEP_PREVIEW},
		{"body", XVOLC_SPEC_ACEP_REQUEST | XVOLC_SPEC_ACEP_PREVIEW},
		{"timeout_ms", XVOLC_SPEC_ACEP_REQUEST | XVOLC_SPEC_STS_REQUEST},
		{"x_date", XVOLC_SPEC_ACEP_PREVIEW | XVOLC_SPEC_STS_PREVIEW},
		{"role_trn", XVOLC_SPEC_STS_REQUEST | XVOLC_SPEC_STS_PREVIEW},
		{"role_session_name", XVOLC_SPEC_STS_REQUEST | XVOLC_SPEC_STS_PREVIEW},
		{"duration_seconds", XVOLC_SPEC_STS_REQUEST | XVOLC_SPEC_STS_PREVIEW}
	};
	const char* p = spec;
	unsigned seen = 0u;
	if ( p == NULL || *p == '\0' ) goto invalid;
	while ( *p ) {
		const char* end = strchr(p, '\n');
		size_t length = end ? (size_t)(end - p) : strlen(p);
		const char* equal;
		size_t i;
		if ( length && p[length - 1u] == '\r' ) --length;
		equal = memchr(p, '=', length);
		if ( equal == NULL || equal == p ) goto invalid;
		for ( i = 0u; i < sizeof(keys) / sizeof(keys[0]); ++i ) {
			if ( (keys[i].variants & variant) &&
				strlen(keys[i].name) == (size_t)(equal - p) &&
				memcmp(p, keys[i].name, (size_t)(equal - p)) == 0 ) break;
		}
		if ( i == sizeof(keys) / sizeof(keys[0]) || (seen & (1u << i)) ) goto invalid;
		seen |= 1u << i;
		if ( end == NULL ) break;
		p = end + 1;
	}
	return 1;
invalid:
	xrtClearError();
	xvolc_set_error("invalid spec");
	return 0;
}

static void xvolc_hex_lower(const unsigned char* bytes, size_t len, char* out)
{
	static const char* hex = "0123456789abcdef";
	size_t i;
	for ( i = 0u; i < len; ++i ) {
		out[i * 2u] = hex[(bytes[i] >> 4) & 0x0f];
		out[i * 2u + 1u] = hex[bytes[i] & 0x0f];
	}
	out[len * 2u] = '\0';
}

static int xvolc_hex_value(int ch)
{
	if ( ch >= '0' && ch <= '9' ) return ch - '0';
	if ( ch >= 'a' && ch <= 'f' ) return ch - 'a' + 10;
	if ( ch >= 'A' && ch <= 'F' ) return ch - 'A' + 10;
	return -1;
}

static int xvolc_hex_to_bytes(const char* hex, unsigned char* out, size_t out_len)
{
	size_t i;
	if ( hex == NULL || strlen(hex) != out_len * 2u ) {
		return 0;
	}
	for ( i = 0u; i < out_len; ++i ) {
		int hi = xvolc_hex_value((unsigned char)hex[i * 2u]);
		int lo = xvolc_hex_value((unsigned char)hex[i * 2u + 1u]);
		if ( hi < 0 || lo < 0 ) {
			return 0;
		}
		out[i] = (unsigned char)((hi << 4) | lo);
	}
	return 1;
}

static int xvolc_sha256_hex_into(const char* text, char out_hex[65])
{
	unsigned char digest[32];
	xrtSha256(text ? text : "", text ? strlen(text) : 0u, digest);
	xvolc_hex_lower(digest, sizeof(digest), out_hex);
	return 1;
}

static int xvolc_hmac_sha256_raw(const unsigned char* key, size_t key_len, const char* text, unsigned char out[32])
{
	xrtHmacSha256(key, key_len, text ? text : "", text ? strlen(text) : 0u, out);
	return 1;
}

static int xvolc_hmac_sha256_hex_into(const unsigned char* key, size_t key_len, const char* text, char out_hex[65])
{
	unsigned char digest[32];
	xvolc_hmac_sha256_raw(key, key_len, text, digest);
	xvolc_hex_lower(digest, sizeof(digest), out_hex);
	return 1;
}

static int xvolc_is_unreserved(unsigned char ch)
{
	return (ch >= 'A' && ch <= 'Z') ||
		(ch >= 'a' && ch <= 'z') ||
		(ch >= '0' && ch <= '9') ||
		ch == '-' || ch == '_' || ch == '.' || ch == '~';
}

static int xvolc_url_encode_append(xvolc_buf* b, const char* text, int preserve_slash)
{
	static const char* hex = "0123456789ABCDEF";
	const unsigned char* p = (const unsigned char*)(text ? text : "");
	while ( *p != 0u ) {
		if ( xvolc_is_unreserved(*p) || (preserve_slash && *p == '/') ) {
			if ( !xvolc_buf_append_n(b, (const char*)p, 1u) ) return 0;
		} else {
			char esc[3];
			esc[0] = '%';
			esc[1] = hex[(*p >> 4) & 0x0f];
			esc[2] = hex[*p & 0x0f];
			if ( !xvolc_buf_append_n(b, esc, sizeof(esc)) ) return 0;
		}
		++p;
	}
	return 1;
}

static int xvolc_pair_cmp(const void* left, const void* right)
{
	const xvolc_pair* a = (const xvolc_pair*)left;
	const xvolc_pair* b = (const xvolc_pair*)right;
	int k = strcmp(a->key, b->key);
	if ( k != 0 ) {
		return k;
	}
	return strcmp(a->value, b->value);
}

static void xvolc_pairs_free(xvolc_pair* pairs, size_t count)
{
	size_t i;
	if ( pairs == NULL ) {
		return;
	}
	for ( i = 0u; i < count; ++i ) {
		if ( pairs[i].key ) xrtFree(pairs[i].key);
		if ( pairs[i].value ) xrtFree(pairs[i].value);
	}
	xrtFree(pairs);
}

static int xvolc_pairs_add(xvolc_pair** pairs, size_t* count, size_t* cap, const char* key, const char* value)
{
	xvolc_pair* next;
	char* key_copy;
	char* value_copy;
	if ( key == NULL || key[0] == '\0' ) {
		return 1;
	}
	if ( *count >= *cap ) {
		size_t new_cap = *cap ? (*cap * 2u) : 8u;
		if ( new_cap < *cap || new_cap > SIZE_MAX / sizeof(xvolc_pair) ) {
			xvolc_set_error("query has too many parameters");
			return 0;
		}
		next = (xvolc_pair*)xrtRealloc(*pairs, new_cap * sizeof(xvolc_pair));
		if ( next == NULL ) {
			xvolc_set_error("out of memory");
			return 0;
		}
		*pairs = next;
		*cap = new_cap;
	}
	(*pairs)[*count].key = NULL;
	(*pairs)[*count].value = NULL;
	key_copy = xvolc_strdup(key);
	if ( key_copy == NULL ) {
		xvolc_set_error("out of memory");
		return 0;
	}
	value_copy = xvolc_strdup(value ? value : "");
	if ( value_copy == NULL ) {
		xrtFree(key_copy);
		xvolc_set_error("out of memory");
		return 0;
	}
	(*pairs)[*count].key = key_copy;
	(*pairs)[*count].value = value_copy;
	++(*count);
	return 1;
}

static int xvolc_parse_extra_pairs(const char* text, xvolc_pair** pairs, size_t* count, size_t* cap)
{
	const char* p = text ? text : "";
	while ( *p != '\0' ) {
		const char* line_start = p;
		const char* line_end;
		const char* eq;
		char* key;
		char* value;
		while ( *p != '\0' && *p != '\n' && *p != '\r' ) {
			++p;
		}
		line_end = p;
		while ( *p == '\n' || *p == '\r' ) {
			++p;
		}
		if ( line_end == line_start ) {
			continue;
		}
		eq = (const char*)memchr(line_start, '=', (size_t)(line_end - line_start));
		if ( eq == NULL ) {
			key = xvolc_strdup_n(line_start, (size_t)(line_end - line_start));
			value = xvolc_strdup("");
		} else {
			key = xvolc_strdup_n(line_start, (size_t)(eq - line_start));
			value = xvolc_strdup_n(eq + 1, (size_t)(line_end - eq - 1));
		}
		if ( key == NULL || value == NULL ) {
			if ( key ) xrtFree(key);
			if ( value ) xrtFree(value);
			xvolc_set_error("out of memory");
			return 0;
		}
		if ( !xvolc_pairs_add(pairs, count, cap, key, value) ) {
			xrtFree(key);
			xrtFree(value);
			return 0;
		}
		xrtFree(key);
		xrtFree(value);
	}
	return 1;
}

static char* xvolc_build_canonical_query(const char* action, const char* version, const char* extra_pairs)
{
	xvolc_pair* pairs = NULL;
	size_t count = 0u;
	size_t cap = 0u;
	size_t i;
	xvolc_buf b;
	char* ret;
	xvolc_buf_init(&b);
	if ( action && action[0] && !xvolc_pairs_add(&pairs, &count, &cap, "Action", action) ) goto fail;
	if ( version && version[0] && !xvolc_pairs_add(&pairs, &count, &cap, "Version", version) ) goto fail;
	if ( !xvolc_parse_extra_pairs(extra_pairs, &pairs, &count, &cap) ) goto fail;
	qsort(pairs, count, sizeof(xvolc_pair), xvolc_pair_cmp);
	for ( i = 0u; i < count; ++i ) {
		if ( i > 0u && !xvolc_buf_append(&b, "&") ) goto fail;
		if ( !xvolc_url_encode_append(&b, pairs[i].key, 0) ) goto fail;
		if ( !xvolc_buf_append(&b, "=") ) goto fail;
		if ( !xvolc_url_encode_append(&b, pairs[i].value, 0) ) goto fail;
	}
	ret = xvolc_strdup(b.data ? b.data : "");
	xvolc_buf_free(&b);
	xvolc_pairs_free(pairs, count);
	if ( ret == NULL ) {
		xvolc_set_error("out of memory");
	}
	return ret;
fail:
	xvolc_buf_free(&b);
	xvolc_pairs_free(pairs, count);
	return NULL;
}

static int xvolc_utc_now(char x_date[17], char date_stamp[9])
{
	if ( xrtTimeWrite(x_date, 17u, xrtNow(), 0,
		XRT_STR_LITERAL("%Y%m%dT%H%M%SZ")) != 16u ) {
		xvolc_set_error("failed to format current UTC time");
		return 0;
	}
	memcpy(date_stamp, x_date, 8u);
	date_stamp[8] = '\0';
	return 1;
}

static int xvolc_signed_field_valid(const char* text)
{
	const unsigned char* p = (const unsigned char*)(text ? text : "");
	while ( *p != 0u ) {
		if ( *p < 0x20u || *p == 0x7fu ) return 0;
		++p;
	}
	return 1;
}

/* 路径按片段编码，斜杠保留；签名与发出的 URL 必须使用相同字节。 */
static char* xvolc_normalize_path(const char* path)
{
	xvolc_buf encoded;
	const char* text = path && path[0] ? path : "/";
	if ( text[0] != '/' || !xvolc_signed_field_valid(text) ) {
		xvolc_set_error("invalid path");
		return NULL;
	}
	xvolc_buf_init(&encoded);
	if ( !xvolc_url_encode_append(&encoded, text, 1) ) {
		xvolc_buf_free(&encoded);
		return NULL;
	}
	return encoded.data;
}

static int xvolc_sign(const char* method,
	const char* host,
	const char* path,
	const char* service,
	const char* region,
	const char* access_key,
	const char* secret_key,
	const char* session_token,
	const char* content_type,
	const char* canonical_query,
	const char* body,
	const char* x_date_arg,
	char** out_x_date,
	char** out_payload_hash,
	char** out_signed_headers,
	char** out_authorization)
{
	char x_date[17];
	char date_stamp[9];
	char payload_hash[65];
	char canonical_hash[65];
	char signature[65];
	unsigned char k_date[32], k_region[32], k_service[32], k_signing[32];
	xvolc_buf canonical;
	xvolc_buf string_to_sign;
	xvolc_buf auth;
	char* signed_headers = NULL;
	char* authorization = NULL;
	char* x_date_copy = NULL;
	char* payload_hash_copy = NULL;
	const char* p_method = method && method[0] ? method : "POST";
	const char* p_path = path && path[0] ? path : "/";
	const char* p_service = service ? service : "";
	const char* p_region = region ? region : "";
	const char* p_access_key = access_key ? access_key : "";
	const char* p_secret_key = secret_key ? secret_key : "";
	const char* p_content_type = content_type && content_type[0] ? content_type : "application/json";
	int has_token = session_token != NULL && session_token[0] != '\0';

	xvolc_buf_init(&canonical);
	xvolc_buf_init(&string_to_sign);
	xvolc_buf_init(&auth);
	if ( p_access_key[0] == '\0' || p_secret_key[0] == '\0' || host == NULL || host[0] == '\0' ) {
		xvolc_set_error("missing host/access_key/secret_key");
		return 0;
	}
	/* 这些值直接进入签名行或 HTTP 头；请求体和查询值由各自的编码路径处理。 */
	if ( !xvolc_signed_field_valid(p_method) || !xvolc_signed_field_valid(host) ||
		!xvolc_signed_field_valid(p_path) || !xvolc_signed_field_valid(p_service) ||
		!xvolc_signed_field_valid(p_region) || !xvolc_signed_field_valid(p_access_key) ||
		!xvolc_signed_field_valid(session_token) || !xvolc_signed_field_valid(p_content_type) ) {
		xvolc_set_error("invalid signed field");
		return 0;
	}
	if ( x_date_arg != NULL && x_date_arg[0] != '\0' ) {
		xtime parsed;
		if ( strlen(x_date_arg) != 16u ) {
			xvolc_set_error("invalid x_date");
			return 0;
		}
		if ( !xrtTimeParse((xstrview){ x_date_arg, 16u },
				XRT_STR_LITERAL("%Y%m%dT%H%M%SZ"), &parsed) ) {
			/* 本接口通过返回值和 last_error 报错，不泄漏时间解析器的线程错误。 */
			xrtClearError();
			xvolc_set_error("invalid x_date");
			return 0;
		}
		memcpy(x_date, x_date_arg, 17u);
		memcpy(date_stamp, x_date, 8u);
		date_stamp[8] = '\0';
	} else if ( !xvolc_utc_now(x_date, date_stamp) ) {
		return 0;
	}
	xvolc_sha256_hex_into(body ? body : "", payload_hash);
	signed_headers = xvolc_strdup(has_token
		? "content-type;host;x-content-sha256;x-date;x-security-token"
		: "content-type;host;x-content-sha256;x-date");
	if ( signed_headers == NULL ) {
		xvolc_set_error("out of memory");
		goto fail;
	}
	if ( !xvolc_buf_appendf(&canonical,
		"%s\n%s\n%s\ncontent-type:%s\nhost:%s\nx-content-sha256:%s\nx-date:%s\n",
		p_method, p_path, canonical_query ? canonical_query : "", p_content_type, host, payload_hash, x_date) ) goto fail;
	if ( has_token && !xvolc_buf_appendf(&canonical, "x-security-token:%s\n", session_token) ) goto fail;
	if ( !xvolc_buf_appendf(&canonical, "\n%s\n%s", signed_headers, payload_hash) ) goto fail;
	xvolc_sha256_hex_into(canonical.data ? canonical.data : "", canonical_hash);
	if ( !xvolc_buf_appendf(&string_to_sign, "HMAC-SHA256\n%s\n%s/%s/%s/request\n%s",
		x_date, date_stamp, p_region, p_service, canonical_hash) ) goto fail;
	xvolc_hmac_sha256_raw((const unsigned char*)p_secret_key, strlen(p_secret_key), date_stamp, k_date);
	xvolc_hmac_sha256_raw(k_date, sizeof(k_date), p_region, k_region);
	xvolc_hmac_sha256_raw(k_region, sizeof(k_region), p_service, k_service);
	xvolc_hmac_sha256_raw(k_service, sizeof(k_service), "request", k_signing);
	xvolc_hmac_sha256_hex_into(k_signing, sizeof(k_signing), string_to_sign.data ? string_to_sign.data : "", signature);
	if ( !xvolc_buf_appendf(&auth,
		"HMAC-SHA256 Credential=%s/%s/%s/%s/request, SignedHeaders=%s, Signature=%s",
		p_access_key, date_stamp, p_region, p_service, signed_headers, signature) ) goto fail;
	authorization = xvolc_strdup(auth.data ? auth.data : "");
	x_date_copy = xvolc_strdup(x_date);
	payload_hash_copy = xvolc_strdup(payload_hash);
	if ( authorization == NULL || x_date_copy == NULL || payload_hash_copy == NULL ) {
		xvolc_set_error("out of memory");
		goto fail;
	}
	*out_x_date = x_date_copy;
	*out_payload_hash = payload_hash_copy;
	*out_signed_headers = signed_headers;
	*out_authorization = authorization;
	xvolc_buf_free(&canonical);
	xvolc_buf_free(&string_to_sign);
	xvolc_buf_free(&auth);
	return 1;
fail:
	if ( signed_headers ) xrtFree(signed_headers);
	if ( authorization ) xrtFree(authorization);
	if ( x_date_copy ) xrtFree(x_date_copy);
	if ( payload_hash_copy ) xrtFree(payload_hash_copy);
	xvolc_buf_free(&canonical);
	xvolc_buf_free(&string_to_sign);
	xvolc_buf_free(&auth);
	return 0;
}

static int xvolc_json_escape_append(xvolc_buf* b, const char* text)
{
	const unsigned char* p = (const unsigned char*)(text ? text : "");
	if ( !xvolc_buf_append(b, "\"") ) return 0;
	while ( *p != 0u ) {
		char tmp[8];
		switch ( *p ) {
		case '\\': if ( !xvolc_buf_append(b, "\\\\") ) return 0; break;
		case '"': if ( !xvolc_buf_append(b, "\\\"") ) return 0; break;
		case '\b': if ( !xvolc_buf_append(b, "\\b") ) return 0; break;
		case '\f': if ( !xvolc_buf_append(b, "\\f") ) return 0; break;
		case '\n': if ( !xvolc_buf_append(b, "\\n") ) return 0; break;
		case '\r': if ( !xvolc_buf_append(b, "\\r") ) return 0; break;
		case '\t': if ( !xvolc_buf_append(b, "\\t") ) return 0; break;
		default:
			if ( *p < 0x20u ) {
				snprintf(tmp, sizeof(tmp), "\\u%04x", (unsigned int)*p);
				if ( !xvolc_buf_append(b, tmp) ) return 0;
			} else if ( !xvolc_buf_append_n(b, (const char*)p, 1u) ) {
				return 0;
			}
			break;
		}
		++p;
	}
	return xvolc_buf_append(b, "\"");
}

int x_volc_core_available(void)
{
	xvolc_set_error("");
	return 1;
}

const char* x_volc_core_version(void)
{
	xvolc_set_error("");
	return XVOLC_CORE_VERSION;
}

const char* x_volc_core_last_error(void)
{
	xvolc_state* state = xvolc_state_get();
	return state != NULL ? state->error : "thread state unavailable";
}

int x_volc_core_last_status(void)
{
	xvolc_state* state = xvolc_state_get();
	return state != NULL ? state->last_status : 0;
}

const char* x_volc_core_sha256_hex(const char* text)
{
	char out[65];
	xvolc_set_error("");
	xvolc_sha256_hex_into(text ? text : "", out);
	return xvolc_return_text(out);
}

const char* x_volc_core_hmac_sha256_hex(const char* key, const char* text)
{
	char out[65];
	xvolc_set_error("");
	xvolc_hmac_sha256_hex_into((const unsigned char*)(key ? key : ""), key ? strlen(key) : 0u, text ? text : "", out);
	return xvolc_return_text(out);
}

const char* x_volc_core_hmac_sha256_hex_key(const char* hex_key, const char* text)
{
	unsigned char key[32];
	char out[65];
	xvolc_set_error("");
	if ( !xvolc_hex_to_bytes(hex_key, key, sizeof(key)) ) {
		xvolc_set_error("hex key must be 64 hex characters");
		return "";
	}
	xvolc_hmac_sha256_hex_into(key, sizeof(key), text ? text : "", out);
	return xvolc_return_text(out);
}

const char* x_volc_core_canonical_query(const char* action, const char* version, const char* extra_pairs)
{
	char* query;
	const char* ret;
	xvolc_set_error("");
	query = xvolc_build_canonical_query(action, version, extra_pairs);
	if ( query == NULL ) {
		return "";
	}
	ret = xvolc_return_text(query);
	xrtFree(query);
	return ret;
}

const char* x_volc_core_json_string(const char* text)
{
	xvolc_buf b;
	const char* ret;
	xvolc_set_error("");
	xvolc_buf_init(&b);
	if ( !xvolc_json_escape_append(&b, text ? text : "") ) {
		xvolc_buf_free(&b);
		return "";
	}
	ret = xvolc_return_text(b.data ? b.data : "\"\"");
	xvolc_buf_free(&b);
	return ret;
}

const char* x_volc_core_sign_preview(const char* method,
	const char* host,
	const char* path,
	const char* service,
	const char* region,
	const char* action,
	const char* version,
	const char* access_key,
	const char* secret_key,
	const char* session_token,
	const char* content_type,
	const char* extra_pairs,
	const char* body,
	const char* x_date)
{
	char* query = NULL;
	char* normalized_path = NULL;
	char* signed_x_date = NULL;
	char* payload_hash = NULL;
	char* signed_headers = NULL;
	char* authorization = NULL;
	xvolc_buf json;
	const char* ret = "";
	xvolc_set_error("");
	xvolc_buf_init(&json);
	query = xvolc_build_canonical_query(action, version, extra_pairs);
	if ( query == NULL ) goto done;
	normalized_path = xvolc_normalize_path(path);
	if ( normalized_path == NULL ) goto done;
	if ( !xvolc_sign(method, host, normalized_path, service, region, access_key, secret_key, session_token,
		content_type, query, body, x_date, &signed_x_date, &payload_hash, &signed_headers, &authorization) ) goto done;
	if ( !xvolc_buf_append(&json, "{") ) goto done;
	if ( !xvolc_buf_append(&json, "\"query\":") || !xvolc_json_escape_append(&json, query) ) goto done;
	if ( !xvolc_buf_append(&json, ",\"x_date\":") || !xvolc_json_escape_append(&json, signed_x_date) ) goto done;
	if ( !xvolc_buf_append(&json, ",\"x_content_sha256\":") || !xvolc_json_escape_append(&json, payload_hash) ) goto done;
	if ( !xvolc_buf_append(&json, ",\"signed_headers\":") || !xvolc_json_escape_append(&json, signed_headers) ) goto done;
	if ( !xvolc_buf_append(&json, ",\"authorization\":") || !xvolc_json_escape_append(&json, authorization) ) goto done;
	if ( session_token && session_token[0] ) {
		if ( !xvolc_buf_append(&json, ",\"x_security_token\":") || !xvolc_json_escape_append(&json, session_token) ) goto done;
	}
	if ( !xvolc_buf_append(&json, "}") ) goto done;
	ret = xvolc_return_text(json.data ? json.data : "{}");
done:
	if ( query ) xrtFree(query);
	if ( normalized_path ) xrtFree(normalized_path);
	if ( signed_x_date ) xrtFree(signed_x_date);
	if ( payload_hash ) xrtFree(payload_hash);
	if ( signed_headers ) xrtFree(signed_headers);
	if ( authorization ) xrtFree(authorization);
	xvolc_buf_free(&json);
	return ret;
}

const char* x_volc_core_request(const char* method,
	const char* host,
	const char* path,
	const char* service,
	const char* region,
	const char* action,
	const char* version,
	const char* access_key,
	const char* secret_key,
	const char* session_token,
	const char* content_type,
	const char* extra_pairs,
	const char* body,
	int timeout_ms)
{
	char* query = NULL;
	char* normalized_path = NULL;
	char* signed_x_date = NULL;
	char* payload_hash = NULL;
	char* signed_headers = NULL;
	char* authorization = NULL;
	xvolc_buf url;
	xhttprequest* req = NULL;
	const xhttpresponse* resp = NULL;
	xhttpresult* result = NULL;
	xnetengineconfig engine_cfg;
	xnetengine* engine = NULL;
	xhttpclientconfig client_cfg;
	xhttpclient* client = NULL;
	xhttpcalloptions call_options;
	uint64 timeout = (timeout_ms > 0 ? (uint64)timeout_ms : (uint64)XVOLC_DEFAULT_TIMEOUT_MS) * UINT64_C(1000);
	const char* ret = "";
	const char* p_method = method && method[0] ? method : "POST";
	const char* p_path = path && path[0] ? path : "/";
	const char* p_content_type = content_type && content_type[0] ? content_type : "application/json";
	const char* p_body = body ? body : "";
	xvolc_state* state = xvolc_state_get();
	if ( state == NULL ) return "";
	xvolc_set_error("");
	state->last_status = 0;
	xvolc_buf_init(&url);
	query = xvolc_build_canonical_query(action, version, extra_pairs);
	if ( query == NULL ) goto done;
	normalized_path = xvolc_normalize_path(p_path);
	if ( normalized_path == NULL ) goto done;
	if ( !xvolc_sign(p_method, host, normalized_path, service, region, access_key, secret_key, session_token,
		p_content_type, query, p_body, NULL, &signed_x_date, &payload_hash, &signed_headers, &authorization) ) goto done;
	if ( !xvolc_buf_appendf(&url, "https://%s%s?%s", host ? host : "", normalized_path, query) ) goto done;
	xrtNetEngineConfigInit(&engine_cfg);
	engine_cfg.Workers = 1u;
	engine = xrtNetEngineCreate(&engine_cfg);
	if ( engine == NULL || !xrtNetEngineStart(engine) ) {
		xvolc_set_error("failed to start xrt HTTP net engine");
		goto done;
	}
	xrtHttpClientConfigInit(&client_cfg);
	client_cfg.Timeout = timeout;
	client_cfg.IdleTimeout = timeout;
	client = xrtHttpClientCreate(engine, &client_cfg);
	if ( client == NULL ) {
		xvolc_set_error("failed to create xrt HTTP client");
		goto done;
	}
	req = xrtHttpRequestCreate(
		(xstrview){ p_method, strlen(p_method) },
		(xstrview){ url.data ? url.data : "", url.data ? url.len : 0u }
	);
	if ( req == NULL ) {
		xvolc_set_error("failed to initialize xrt HTTP request");
		goto done;
	}
	if ( !xrtHttpRequestSetHeader(req, XRT_STR_LITERAL("Host"), (xstrview){ host ? host : "", host ? strlen(host) : 0u }) ||
		!xrtHttpRequestSetHeader(req, XRT_STR_LITERAL("X-Date"), (xstrview){ signed_x_date, strlen(signed_x_date) }) ||
		!xrtHttpRequestSetHeader(req, XRT_STR_LITERAL("X-Content-Sha256"), (xstrview){ payload_hash, strlen(payload_hash) }) ||
		!xrtHttpRequestSetHeader(req, XRT_STR_LITERAL("Authorization"), (xstrview){ authorization, strlen(authorization) }) ) {
		xvolc_set_error("failed to set xrt HTTP headers");
		goto done;
	}
	if ( session_token && session_token[0] ) {
		if ( !xrtHttpRequestSetHeader(req, XRT_STR_LITERAL("X-Security-Token"), (xstrview){ session_token, strlen(session_token) }) ) {
			xvolc_set_error("failed to set xrt security token");
			goto done;
		}
	}
	if ( strcmp(p_method, "GET") != 0 && strcmp(p_method, "get") != 0 ) {
		if ( !xrtHttpRequestSetBytes(req, (xbytesview){ (const unsigned char*)p_body, strlen(p_body) }, (xstrview){ p_content_type, strlen(p_content_type) }) ) {
			xvolc_set_error("failed to set xrt HTTP request body");
			goto done;
		}
	} else {
		if ( !xrtHttpRequestSetHeader(req, XRT_STR_LITERAL("Content-Type"), (xstrview){ p_content_type, strlen(p_content_type) }) ) {
			xvolc_set_error("failed to set xrt content type");
			goto done;
		}
	}
	xrtHttpCallOptionsInit(&call_options);
	call_options.Timeout = timeout;
	call_options.IdleTimeout = timeout;
	result = xrtHttpClientDoSync(client, req, &call_options);
	if ( result == NULL || (resp = xrtHttpResultResponse(result)) == NULL ) {
		xvolc_set_error("xrt HTTP request failed");
		goto done;
	}
	state->last_status = (int)xrtHttpResponseStatus(resp);
	{
		xbytesview response_body = xrtHttpResponseBody(resp);
		ret = xvolc_return_copy((const char*)response_body.Data, response_body.Size);
	}
	if ( state->last_status < 200 || state->last_status >= 300 ) {
		xvolc_set_errorf("HTTP returned status %d", state->last_status);
	}
done:
	if ( result ) xrtHttpResultDestroy(result);
	if ( req ) xrtHttpRequestDestroy(req);
	if ( client ) xrtHttpClientDestroy(client);
	if ( engine ) {
		(void)xrtNetEngineStop(engine);
		(void)xrtNetEngineDestroy(engine);
	}
	if ( query ) xrtFree(query);
	if ( normalized_path ) xrtFree(normalized_path);
	if ( signed_x_date ) xrtFree(signed_x_date);
	if ( payload_hash ) xrtFree(payload_hash);
	if ( signed_headers ) xrtFree(signed_headers);
	if ( authorization ) xrtFree(authorization);
	xvolc_buf_free(&url);
	return ret;
}

const char* x_volc_core_request_acep(const char* action,
	const char* version,
	const char* access_key,
	const char* secret_key,
	const char* session_token,
	const char* body,
	int timeout_ms)
{
	return x_volc_core_request("POST", "open.volcengineapi.com", "/", "ACEP", "cn-north-1",
		action, version, access_key, secret_key, session_token, "application/json", "", body, timeout_ms);
}

const char* x_volc_core_sign_preview_acep(const char* action,
	const char* version,
	const char* access_key,
	const char* secret_key,
	const char* session_token,
	const char* body,
	const char* x_date)
{
	return x_volc_core_sign_preview("POST", "open.volcengineapi.com", "/", "ACEP", "cn-north-1",
		action, version, access_key, secret_key, session_token, "application/json", "", body, x_date);
}

static int xvolc_sts_role_fields_valid(const char* role_trn, const char* role_session_name)
{
	if ( (role_trn && strpbrk(role_trn, "\r\n")) ||
		(role_session_name && strpbrk(role_session_name, "\r\n")) ) {
		xvolc_set_error("invalid STS role field");
		return 0;
	}
	return 1;
}

const char* x_volc_core_request_sts(const char* access_key,
	const char* secret_key,
	const char* role_trn,
	const char* role_session_name,
	int duration_seconds,
	int timeout_ms)
{
	xvolc_buf pairs;
	const char* ret = "";
	xvolc_set_error("");
	if ( !xvolc_sts_role_fields_valid(role_trn, role_session_name) ) return ret;
	xvolc_buf_init(&pairs);
	if ( !xvolc_buf_appendf(&pairs, "RoleTrn=%s\nRoleSessionName=%s\nDurationSeconds=%d",
		role_trn ? role_trn : "", role_session_name ? role_session_name : "", duration_seconds) ) goto done;
	ret = x_volc_core_request("GET", "sts.volcengineapi.com", "/", "sts", "cn-north-1",
		"AssumeRole", "2018-01-01", access_key, secret_key, "", "application/x-www-form-urlencoded", pairs.data ? pairs.data : "", "", timeout_ms);
done:
	xvolc_buf_free(&pairs);
	return ret;
}

const char* x_volc_core_sign_preview_sts(const char* access_key,
	const char* secret_key,
	const char* role_trn,
	const char* role_session_name,
	int duration_seconds,
	const char* x_date)
{
	xvolc_buf pairs;
	const char* ret = "";
	xvolc_set_error("");
	if ( !xvolc_sts_role_fields_valid(role_trn, role_session_name) ) return ret;
	xvolc_buf_init(&pairs);
	if ( !xvolc_buf_appendf(&pairs, "RoleTrn=%s\nRoleSessionName=%s\nDurationSeconds=%d",
		role_trn ? role_trn : "", role_session_name ? role_session_name : "", duration_seconds) ) goto done;
	ret = x_volc_core_sign_preview("GET", "sts.volcengineapi.com", "/", "sts", "cn-north-1",
		"AssumeRole", "2018-01-01", access_key, secret_key, "", "application/x-www-form-urlencoded", pairs.data ? pairs.data : "", "", x_date);
done:
	xvolc_buf_free(&pairs);
	return ret;
}

const char* x_volc_core_request_acep_spec(const char* spec)
{
	char* action = xvolc_spec_get(spec, "action");
	char* version = xvolc_spec_get(spec, "version");
	char* access_key = xvolc_spec_get(spec, "access_key");
	char* secret_key = xvolc_spec_get(spec, "secret_key");
	char* session_token = xvolc_spec_get(spec, "session_token");
	char* body = xvolc_spec_get(spec, "body");
	char* timeout_text = xvolc_spec_get(spec, "timeout_ms");
	int timeout_ms;
	const char* ret = "";
	if ( !action || !version || !access_key || !secret_key || !session_token ||
		!body || !timeout_text ) {
		xvolc_set_error("out of memory");
		goto done;
	}
	if ( !xvolc_spec_valid(spec, XVOLC_SPEC_ACEP_REQUEST) ) goto done;
	if ( !xvolc_parse_spec_int(timeout_text, (int)XVOLC_DEFAULT_TIMEOUT_MS,
		"invalid timeout_ms", &timeout_ms) ) goto done;
	ret = x_volc_core_request("POST", "open.volcengineapi.com", "/", "ACEP", "cn-north-1",
		action, version, access_key, secret_key, session_token, "application/json", "", body,
		timeout_ms);
done:
	if ( action ) xrtFree(action);
	if ( version ) xrtFree(version);
	if ( access_key ) xrtFree(access_key);
	if ( secret_key ) xrtFree(secret_key);
	if ( session_token ) xrtFree(session_token);
	if ( body ) xrtFree(body);
	if ( timeout_text ) xrtFree(timeout_text);
	return ret;
}

const char* x_volc_core_sign_preview_acep_spec(const char* spec)
{
	char* action = xvolc_spec_get(spec, "action");
	char* version = xvolc_spec_get(spec, "version");
	char* access_key = xvolc_spec_get(spec, "access_key");
	char* secret_key = xvolc_spec_get(spec, "secret_key");
	char* session_token = xvolc_spec_get(spec, "session_token");
	char* body = xvolc_spec_get(spec, "body");
	char* x_date = xvolc_spec_get(spec, "x_date");
	const char* ret = "";
	if ( !action || !version || !access_key || !secret_key || !session_token ||
		!body || !x_date ) {
		xvolc_set_error("out of memory");
		goto done;
	}
	if ( !xvolc_spec_valid(spec, XVOLC_SPEC_ACEP_PREVIEW) ) goto done;
	ret = x_volc_core_sign_preview("POST", "open.volcengineapi.com", "/", "ACEP", "cn-north-1",
		action, version, access_key, secret_key, session_token, "application/json", "", body, x_date);
done:
	if ( action ) xrtFree(action);
	if ( version ) xrtFree(version);
	if ( access_key ) xrtFree(access_key);
	if ( secret_key ) xrtFree(secret_key);
	if ( session_token ) xrtFree(session_token);
	if ( body ) xrtFree(body);
	if ( x_date ) xrtFree(x_date);
	return ret;
}

const char* x_volc_core_request_sts_spec(const char* spec)
{
	char* access_key = xvolc_spec_get(spec, "access_key");
	char* secret_key = xvolc_spec_get(spec, "secret_key");
	char* role_trn = xvolc_spec_get(spec, "role_trn");
	char* role_session_name = xvolc_spec_get(spec, "role_session_name");
	char* duration_text = xvolc_spec_get(spec, "duration_seconds");
	char* timeout_text = xvolc_spec_get(spec, "timeout_ms");
	int duration_seconds, timeout_ms;
	const char* ret = "";
	if ( !access_key || !secret_key || !role_trn || !role_session_name ||
		!duration_text || !timeout_text ) {
		xvolc_set_error("out of memory");
		goto done;
	}
	if ( !xvolc_spec_valid(spec, XVOLC_SPEC_STS_REQUEST) ) goto done;
	if ( !xvolc_parse_spec_int(duration_text, 3600,
		"invalid duration_seconds", &duration_seconds) ||
		!xvolc_parse_spec_int(timeout_text, (int)XVOLC_DEFAULT_TIMEOUT_MS,
		"invalid timeout_ms", &timeout_ms) ) goto done;
	ret = x_volc_core_request_sts(access_key, secret_key, role_trn, role_session_name,
		duration_seconds, timeout_ms);
done:
	if ( access_key ) xrtFree(access_key);
	if ( secret_key ) xrtFree(secret_key);
	if ( role_trn ) xrtFree(role_trn);
	if ( role_session_name ) xrtFree(role_session_name);
	if ( duration_text ) xrtFree(duration_text);
	if ( timeout_text ) xrtFree(timeout_text);
	return ret;
}

const char* x_volc_core_sign_preview_sts_spec(const char* spec)
{
	char* access_key = xvolc_spec_get(spec, "access_key");
	char* secret_key = xvolc_spec_get(spec, "secret_key");
	char* role_trn = xvolc_spec_get(spec, "role_trn");
	char* role_session_name = xvolc_spec_get(spec, "role_session_name");
	char* duration_text = xvolc_spec_get(spec, "duration_seconds");
	char* x_date = xvolc_spec_get(spec, "x_date");
	int duration_seconds;
	const char* ret = "";
	if ( !access_key || !secret_key || !role_trn || !role_session_name ||
		!duration_text || !x_date ) {
		xvolc_set_error("out of memory");
		goto done;
	}
	if ( !xvolc_spec_valid(spec, XVOLC_SPEC_STS_PREVIEW) ) goto done;
	if ( !xvolc_parse_spec_int(duration_text, 3600,
		"invalid duration_seconds", &duration_seconds) ) goto done;
	ret = x_volc_core_sign_preview_sts(access_key, secret_key, role_trn, role_session_name,
		duration_seconds, x_date);
done:
	if ( access_key ) xrtFree(access_key);
	if ( secret_key ) xrtFree(secret_key);
	if ( role_trn ) xrtFree(role_trn);
	if ( role_session_name ) xrtFree(role_session_name);
	if ( duration_text ) xrtFree(duration_text);
	if ( x_date ) xrtFree(x_date);
	return ret;
}
