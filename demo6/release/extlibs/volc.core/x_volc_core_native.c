#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "xrt.h"

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

static xmutex g_xvolc_lock = NULL;
static char* g_xvolc_returns[8];
static size_t g_xvolc_return_slot = 0u;
static char g_xvolc_error[512] = "";
static int g_xvolc_last_status = 0;

static void xvolc_init(void)
{
	if ( g_xvolc_lock == NULL ) {
		g_xvolc_lock = xrtMutexCreate();
	}
}

static void xvolc_set_error(const char* text)
{
	size_t len;
	xvolc_init();
	if ( text == NULL ) {
		text = "";
	}
	len = strlen(text);
	if ( len >= sizeof(g_xvolc_error) ) {
		len = sizeof(g_xvolc_error) - 1u;
	}
	if ( len > 0u ) {
		memcpy(g_xvolc_error, text, len);
	}
	g_xvolc_error[len] = '\0';
}

static void xvolc_set_errorf(const char* fmt, ...)
{
	va_list ap;
	xvolc_init();
	va_start(ap, fmt);
	vsnprintf(g_xvolc_error, sizeof(g_xvolc_error), fmt, ap);
	va_end(ap);
	g_xvolc_error[sizeof(g_xvolc_error) - 1u] = '\0';
}

static const char* xvolc_return_copy(const char* text, size_t len)
{
	char* copy;
	size_t slot;
	xvolc_init();
	if ( text == NULL ) {
		text = "";
		len = 0u;
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
	slot = g_xvolc_return_slot % (sizeof(g_xvolc_returns) / sizeof(g_xvolc_returns[0]));
	g_xvolc_return_slot += 1u;
	if ( g_xvolc_returns[slot] != NULL ) {
		xrtFree(g_xvolc_returns[slot]);
	}
	g_xvolc_returns[slot] = copy;
	return g_xvolc_returns[slot];
}

static const char* xvolc_return_text(const char* text)
{
	return xvolc_return_copy(text, text ? strlen(text) : 0u);
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

static int xvolc_parse_int_default(const char* text, int default_value)
{
	char* end;
	long value;

	if ( text == NULL || text[0] == '\0' ) {
		return default_value;
	}
	value = strtol(text, &end, 10);
	if ( end == text ) {
		return default_value;
	}
	while ( *end != '\0' ) {
		if ( !isspace((unsigned char)*end) ) {
			return default_value;
		}
		end += 1;
	}
	if ( value > (long)INT_MAX ) {
		return INT_MAX;
	}
	if ( value < (long)INT_MIN ) {
		return INT_MIN;
	}
	return (int)value;
}

static char* xvolc_spec_get(const char* spec, const char* key)
{
	size_t key_len = key ? strlen(key) : 0u;
	const char* p = spec ? spec : "";
	if ( key_len == 0u ) return xvolc_strdup("");
	while ( *p ) {
		const char* line_end = strchr(p, '\n');
		size_t line_len = line_end ? (size_t)(line_end - p) : strlen(p);
		if ( line_len > key_len && memcmp(p, key, key_len) == 0 && p[key_len] == '=' ) {
			return xvolc_strdup_n(p + key_len + 1u, line_len - key_len - 1u);
		}
		if ( line_end == NULL ) break;
		p = line_end + 1;
	}
	return xvolc_strdup("");
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
	xrtSHA256((const ptr)(text ? text : ""), text ? strlen(text) : 0u, digest);
	xvolc_hex_lower(digest, sizeof(digest), out_hex);
	return 1;
}

static int xvolc_hmac_sha256_raw(const unsigned char* key, size_t key_len, const char* text, unsigned char out[32])
{
	xrtHMAC_SHA256(key, key_len, (const uint8*)(text ? text : ""), text ? strlen(text) : 0u, out);
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

static int xvolc_url_encode_append(xvolc_buf* b, const char* text)
{
	static const char* hex = "0123456789ABCDEF";
	const unsigned char* p = (const unsigned char*)(text ? text : "");
	while ( *p != 0u ) {
		if ( xvolc_is_unreserved(*p) ) {
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
	if ( key == NULL || key[0] == '\0' ) {
		return 1;
	}
	if ( *count >= *cap ) {
		size_t new_cap = *cap ? (*cap * 2u) : 8u;
		next = (xvolc_pair*)xrtRealloc(*pairs, new_cap * sizeof(xvolc_pair));
		if ( next == NULL ) {
			xvolc_set_error("out of memory");
			return 0;
		}
		*pairs = next;
		*cap = new_cap;
	}
	(*pairs)[*count].key = xvolc_strdup(key);
	(*pairs)[*count].value = xvolc_strdup(value ? value : "");
	if ( (*pairs)[*count].key == NULL || (*pairs)[*count].value == NULL ) {
		xvolc_set_error("out of memory");
		return 0;
	}
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
		if ( !xvolc_url_encode_append(&b, pairs[i].key) ) goto fail;
		if ( !xvolc_buf_append(&b, "=") ) goto fail;
		if ( !xvolc_url_encode_append(&b, pairs[i].value) ) goto fail;
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

static void xvolc_utc_now(char x_date[17], char date_stamp[9])
{
	time_t t = time(NULL);
	struct tm tmv;
	struct tm* ptm = gmtime(&t);
	if ( ptm != NULL ) {
		tmv = *ptm;
	} else {
		memset(&tmv, 0, sizeof(tmv));
	}
	strftime(x_date, 17u, "%Y%m%dT%H%M%SZ", &tmv);
	memcpy(date_stamp, x_date, 8u);
	date_stamp[8] = '\0';
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
	if ( x_date_arg != NULL && strlen(x_date_arg) == 16u ) {
		memcpy(x_date, x_date_arg, 17u);
		memcpy(date_stamp, x_date, 8u);
		date_stamp[8] = '\0';
	} else {
		xvolc_utc_now(x_date, date_stamp);
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
	xvolc_init();
	return g_xvolc_error;
}

int x_volc_core_last_status(void)
{
	return g_xvolc_last_status;
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
	if ( !xvolc_sign(method, host, path, service, region, access_key, secret_key, session_token,
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
	char* signed_x_date = NULL;
	char* payload_hash = NULL;
	char* signed_headers = NULL;
	char* authorization = NULL;
	xvolc_buf url;
	xhttprequest req;
	xhttpresponse* resp = NULL;
	xnetengineconfig engine_cfg;
	xnetengine* engine = NULL;
	xnet_result status = XRT_NET_ERROR;
	uint32 timeout = timeout_ms > 0 ? (uint32)timeout_ms : XVOLC_DEFAULT_TIMEOUT_MS;
	const char* ret = "";
	const char* p_method = method && method[0] ? method : "POST";
	const char* p_path = path && path[0] ? path : "/";
	const char* p_content_type = content_type && content_type[0] ? content_type : "application/json";
	const char* p_body = body ? body : "";
	xvolc_set_error("");
	g_xvolc_last_status = 0;
	xvolc_buf_init(&url);
	xrtHttpRequestInit(&req);
	query = xvolc_build_canonical_query(action, version, extra_pairs);
	if ( query == NULL ) goto done;
	if ( !xvolc_sign(p_method, host, p_path, service, region, access_key, secret_key, session_token,
		p_content_type, query, p_body, NULL, &signed_x_date, &payload_hash, &signed_headers, &authorization) ) goto done;
	if ( !xvolc_buf_appendf(&url, "https://%s%s?%s", host ? host : "", p_path, query) ) goto done;
	xrtNetEngineConfigInit(&engine_cfg);
	engine_cfg.iWorkerCount = 1u;
	engine = xrtNetEngineCreate(&engine_cfg);
	if ( engine == NULL || xrtNetEngineStart(engine) != XRT_NET_OK ) {
		xvolc_set_error("failed to start xrt HTTP net engine");
		goto done;
	}
	if ( !xrtHttpRequestSetMethod(&req, p_method) || !xrtHttpRequestSetURL(&req, url.data ? url.data : "") ) {
		xvolc_set_error("failed to initialize xrt HTTP request");
		goto done;
	}
	xrtHttpRequestSetTimeout(&req, timeout);
	xrtHttpRequestSetIdleTimeout(&req, timeout);
	xrtHttpRequestSetVerifyPeer(&req, true);
	xrtHttpRequestSetHeader(&req, "Host", host ? host : "");
	xrtHttpRequestSetHeader(&req, "X-Date", signed_x_date);
	xrtHttpRequestSetHeader(&req, "X-Content-Sha256", payload_hash);
	xrtHttpRequestSetHeader(&req, "Authorization", authorization);
	if ( session_token && session_token[0] ) {
		xrtHttpRequestSetHeader(&req, "X-Security-Token", session_token);
	}
	if ( strcmp(p_method, "GET") != 0 && strcmp(p_method, "get") != 0 ) {
		xrtHttpRequestSetBodyCopy(&req, p_body, strlen(p_body), p_content_type);
	} else {
		xrtHttpRequestSetHeader(&req, "Content-Type", p_content_type);
	}
	resp = xrtHttpExecuteSync(engine, &req, &status);
	if ( status != XRT_NET_OK || resp == NULL ) {
		xvolc_set_errorf("xrt HTTP request failed: status=%d", (int)status);
		goto done;
	}
	g_xvolc_last_status = (int)resp->iStatusCode;
	ret = xvolc_return_copy(resp->pBody ? resp->pBody : "", resp->pBody ? resp->iBodyLen : 0u);
	if ( resp->iStatusCode < 200u || resp->iStatusCode >= 300u ) {
		xvolc_set_errorf("HTTP returned status %u", (unsigned)resp->iStatusCode);
	}
done:
	if ( resp ) xrtHttpResponseDestroy(resp);
	xrtHttpRequestUnit(&req);
	if ( engine ) {
		xrtNetEngineStop(engine);
		xrtNetEngineDestroy(engine);
	}
	if ( query ) xrtFree(query);
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

const char* x_volc_core_request_sts(const char* access_key,
	const char* secret_key,
	const char* role_trn,
	const char* role_session_name,
	int duration_seconds,
	int timeout_ms)
{
	xvolc_buf pairs;
	const char* ret = "";
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
	const char* ret = x_volc_core_request("POST", "open.volcengineapi.com", "/", "ACEP", "cn-north-1",
		action, version, access_key, secret_key, session_token, "application/json", "", body,
		xvolc_parse_int_default(timeout_text, (int)XVOLC_DEFAULT_TIMEOUT_MS));
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
	const char* ret = x_volc_core_sign_preview("POST", "open.volcengineapi.com", "/", "ACEP", "cn-north-1",
		action, version, access_key, secret_key, session_token, "application/json", "", body, x_date);
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
	const char* ret = x_volc_core_request_sts(access_key, secret_key, role_trn, role_session_name,
		xvolc_parse_int_default(duration_text, 3600),
		xvolc_parse_int_default(timeout_text, (int)XVOLC_DEFAULT_TIMEOUT_MS));
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
	const char* ret = x_volc_core_sign_preview_sts(access_key, secret_key, role_trn, role_session_name,
		xvolc_parse_int_default(duration_text, 3600), x_date);
	if ( access_key ) xrtFree(access_key);
	if ( secret_key ) xrtFree(secret_key);
	if ( role_trn ) xrtFree(role_trn);
	if ( role_session_name ) xrtFree(role_session_name);
	if ( duration_text ) xrtFree(duration_text);
	if ( x_date ) xrtFree(x_date);
	return ret;
}
