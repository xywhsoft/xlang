#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct xredis_socket {
	SOCKET fd;
} xredis_socket;

static int g_xredis_wsa_started = 0;
static char* g_xredis_recv_buf = NULL;
static int g_xredis_recv_cap = 0;
static char* g_xredis_return_bufs[8];
static int g_xredis_return_slot = 0;

static int xredis_wsa_start(void)
{
	WSADATA data;
	if ( g_xredis_wsa_started ) {
		return 1;
	}
	if ( WSAStartup(MAKEWORD(2, 2), &data) != 0 ) {
		return 0;
	}
	g_xredis_wsa_started = 1;
	return 1;
}

static int xredis_set_timeouts(SOCKET fd, int timeout_ms)
{
	DWORD value;
	if ( timeout_ms <= 0 ) {
		timeout_ms = 5000;
	}
	value = (DWORD)timeout_ms;
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&value, sizeof(value));
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&value, sizeof(value));
	return 1;
}

void* xredis_tcp_connect_native(const char* host, int port, int timeout_ms)
{
	struct addrinfo hints;
	struct addrinfo* list = NULL;
	struct addrinfo* it;
	char service[32];
	SOCKET fd = INVALID_SOCKET;
	xredis_socket* handle;

	if ( host == NULL || host[0] == '\0' || port <= 0 || !xredis_wsa_start() ) {
		return NULL;
	}
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	_snprintf(service, sizeof(service), "%d", port);
	service[sizeof(service) - 1] = '\0';
	if ( getaddrinfo(host, service, &hints, &list) != 0 ) {
		return NULL;
	}
	for ( it = list; it != NULL; it = it->ai_next ) {
		fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
		if ( fd == INVALID_SOCKET ) {
			continue;
		}
		xredis_set_timeouts(fd, timeout_ms);
		if ( connect(fd, it->ai_addr, (int)it->ai_addrlen) == 0 ) {
			break;
		}
		closesocket(fd);
		fd = INVALID_SOCKET;
	}
	freeaddrinfo(list);
	if ( fd == INVALID_SOCKET ) {
		return NULL;
	}
	handle = (xredis_socket*)calloc(1u, sizeof(*handle));
	if ( handle == NULL ) {
		closesocket(fd);
		return NULL;
	}
	handle->fd = fd;
	return handle;
}

int xredis_tcp_close_native(void* socket_handle)
{
	xredis_socket* handle = (xredis_socket*)socket_handle;
	if ( handle == NULL ) {
		return 0;
	}
	if ( handle->fd != INVALID_SOCKET ) {
		closesocket(handle->fd);
		handle->fd = INVALID_SOCKET;
	}
	free(handle);
	return 1;
}

int xredis_tcp_send_native(void* socket_handle, const char* data)
{
	xredis_socket* handle = (xredis_socket*)socket_handle;
	int total = 0;
	int size;
	if ( handle == NULL || handle->fd == INVALID_SOCKET || data == NULL ) {
		return -1;
	}
	size = (int)strlen(data);
	while ( total < size ) {
		int sent = send(handle->fd, data + total, size - total, 0);
		if ( sent <= 0 ) {
			return total > 0 ? total : -1;
		}
		total += sent;
	}
	return total;
}

const char* xredis_tcp_recv_native(void* socket_handle, int max_bytes, int timeout_ms)
{
	xredis_socket* handle = (xredis_socket*)socket_handle;
	int got;
	if ( handle == NULL || handle->fd == INVALID_SOCKET ) {
		return "";
	}
	if ( max_bytes <= 0 ) {
		max_bytes = 4096;
	}
	if ( g_xredis_recv_cap < max_bytes + 1 ) {
		char* next = (char*)realloc(g_xredis_recv_buf, (size_t)max_bytes + 1u);
		if ( next == NULL ) {
			return "";
		}
		g_xredis_recv_buf = next;
		g_xredis_recv_cap = max_bytes + 1;
	}
	xredis_set_timeouts(handle->fd, timeout_ms);
	got = recv(handle->fd, g_xredis_recv_buf, max_bytes, 0);
	if ( got <= 0 ) {
		g_xredis_recv_buf[0] = '\0';
		return g_xredis_recv_buf;
	}
	g_xredis_recv_buf[got] = '\0';
	return g_xredis_recv_buf;
}

static const char* xredis_return_copy(const char* text, size_t len)
{
	int slot;
	char* copy;
	if ( text == NULL ) {
		text = "";
		len = 0u;
	}
	copy = (char*)malloc(len + 1u);
	if ( copy == NULL ) {
		return "";
	}
	if ( len > 0u ) {
		memcpy(copy, text, len);
	}
	copy[len] = '\0';
	slot = g_xredis_return_slot % (int)(sizeof(g_xredis_return_bufs) / sizeof(g_xredis_return_bufs[0]));
	g_xredis_return_slot += 1;
	if ( g_xredis_return_bufs[slot] != NULL ) {
		free(g_xredis_return_bufs[slot]);
	}
	g_xredis_return_bufs[slot] = copy;
	return g_xredis_return_bufs[slot];
}

const char* xredis_substr_native(const char* text, int start, int len)
{
	size_t text_len;
	size_t begin;
	size_t count;
	if ( text == NULL || start < 0 || len <= 0 ) {
		return "";
	}
	text_len = strlen(text);
	begin = (size_t)start;
	if ( begin >= text_len ) {
		return "";
	}
	count = (size_t)len;
	if ( count > text_len - begin ) {
		count = text_len - begin;
	}
	return xredis_return_copy(text + begin, count);
}

const char* xredis_json_string_native(const char* text)
{
	size_t len;
	size_t i;
	size_t out_len = 0u;
	char* out;
	if ( text == NULL ) text = "";
	len = strlen(text);
	for ( i = 0u; i < len; i += 1u ) {
		unsigned char ch = (unsigned char)text[i];
		if ( ch == '"' || ch == '\\' || ch == '\n' || ch == '\r' || ch == '\t' ) {
			out_len += 2u;
		} else {
			out_len += 1u;
		}
	}
	out = (char*)malloc(out_len + 3u);
	if ( out == NULL ) return "";
	out[0] = '"';
	out_len = 1u;
	for ( i = 0u; i < len; i += 1u ) {
		unsigned char ch = (unsigned char)text[i];
		if ( ch == '"' || ch == '\\' ) {
			out[out_len++] = '\\';
			out[out_len++] = (char)ch;
		} else if ( ch == '\n' ) {
			out[out_len++] = '\\';
			out[out_len++] = 'n';
		} else if ( ch == '\r' ) {
			out[out_len++] = '\\';
			out[out_len++] = 'r';
		} else if ( ch == '\t' ) {
			out[out_len++] = '\\';
			out[out_len++] = 't';
		} else {
			out[out_len++] = (char)ch;
		}
	}
	out[out_len++] = '"';
	out[out_len] = '\0';
	text = xredis_return_copy(out, out_len);
	free(out);
	return text;
}

static const char* xredis_skip_ws(const char* p)
{
	while ( p != NULL && *p != '\0' && isspace((unsigned char)*p) ) {
		p += 1;
	}
	return p;
}

static const char* xredis_json_value_end(const char* p, const char* end)
{
	int depth = 0;
	int in_string = 0;
	int escape = 0;
	char open = '\0';
	char close = '\0';
	p = xredis_skip_ws(p);
	if ( p == NULL ) return NULL;
	if ( end == NULL ) end = p + strlen(p);
	if ( p >= end ) return p;
	if ( *p == '"' ) {
		p += 1;
		while ( p < end ) {
			if ( escape ) {
				escape = 0;
			} else if ( *p == '\\' ) {
				escape = 1;
			} else if ( *p == '"' ) {
				return p + 1;
			}
			p += 1;
		}
		return end;
	}
	if ( *p == '{' || *p == '[' ) {
		open = *p;
		close = (open == '{') ? '}' : ']';
		(void)close;
		while ( p < end ) {
			if ( in_string ) {
				if ( escape ) {
					escape = 0;
				} else if ( *p == '\\' ) {
					escape = 1;
				} else if ( *p == '"' ) {
					in_string = 0;
				}
			} else if ( *p == '"' ) {
				in_string = 1;
			} else if ( *p == '{' || *p == '[' ) {
				depth += 1;
			} else if ( *p == '}' || *p == ']' ) {
				depth -= 1;
				if ( depth == 0 ) {
					return p + 1;
				}
			}
			p += 1;
		}
		return end;
	}
	while ( p < end && *p != ',' && *p != '}' && *p != ']' ) {
		p += 1;
	}
	return p;
}

static const char* xredis_find_key(const char* start, const char* end, const char* key)
{
	size_t key_len;
	const char* p;
	if ( start == NULL || key == NULL ) return NULL;
	if ( end == NULL ) end = start + strlen(start);
	key_len = strlen(key);
	p = start;
	if ( *p == '{' ) p += 1;
	while ( p < end ) {
		p = xredis_skip_ws(p);
		if ( p == NULL || p >= end || *p == '}' ) return NULL;
		if ( *p == '"' ) {
			const char* name_start = p + 1;
			const char* name_end = name_start;
			int escape = 0;
			while ( name_end < end ) {
				if ( escape ) {
					escape = 0;
				} else if ( *name_end == '\\' ) {
					escape = 1;
				} else if ( *name_end == '"' ) {
					break;
				}
				name_end += 1;
			}
			if ( name_end >= end ) return NULL;
			p = xredis_skip_ws(name_end + 1);
			if ( p == NULL || p >= end || *p != ':' ) return NULL;
			if ( (size_t)(name_end - name_start) == key_len && strncmp(name_start, key, key_len) == 0 ) {
				return p + 1;
			}
			p = xredis_json_value_end(p + 1, end);
			if ( p == NULL ) return NULL;
			p = xredis_skip_ws(p);
			if ( p != NULL && p < end && *p == ',' ) {
				p += 1;
				continue;
			}
			continue;
		}
		p += 1;
	}
	return NULL;
}

static int xredis_json_field_range(const char* start, const char* end, const char* key, const char** out_start, const char** out_end)
{
	const char* value = xredis_find_key(start, end, key);
	if ( value == NULL ) return 0;
	value = xredis_skip_ws(value);
	if ( value == NULL ) return 0;
	*out_start = value;
	*out_end = xredis_json_value_end(value, end);
	return *out_end != NULL && *out_end >= *out_start;
}

static const char* xredis_json_string_field(const char* start, const char* end, const char* key)
{
	const char* value;
	const char* stop;
	if ( !xredis_json_field_range(start, end, key, &value, &stop) ) return "";
	value = xredis_skip_ws(value);
	if ( value == NULL || value >= stop ) return "";
	if ( *value == '"' ) {
		const char* text_start = value + 1;
		const char* text_end = stop;
		if ( text_end > text_start && *(text_end - 1) == '"' ) {
			text_end -= 1;
		}
		return xredis_return_copy(text_start, (size_t)(text_end - text_start));
	}
	while ( stop > value && isspace((unsigned char)*(stop - 1)) ) {
		stop -= 1;
	}
	return xredis_return_copy(value, (size_t)(stop - value));
}

static int xredis_json_int_field(const char* start, const char* end, const char* key, int fallback)
{
	const char* value;
	const char* stop;
	char tmp[64];
	size_t len;
	if ( !xredis_json_field_range(start, end, key, &value, &stop) ) return fallback;
	value = xredis_skip_ws(value);
	if ( value == NULL ) return fallback;
	if ( *value == '"' ) value += 1;
	len = (size_t)(stop - value);
	if ( len >= sizeof(tmp) ) len = sizeof(tmp) - 1u;
	memcpy(tmp, value, len);
	tmp[len] = '\0';
	return (int)strtol(tmp, NULL, 10);
}

static int xredis_json_bool_field(const char* start, const char* end, const char* key, int fallback)
{
	const char* value;
	const char* stop;
	if ( !xredis_json_field_range(start, end, key, &value, &stop) ) return fallback;
	(void)stop;
	value = xredis_skip_ws(value);
	if ( value == NULL ) return fallback;
	if ( strncmp(value, "true", 4u) == 0 ) return 1;
	if ( strncmp(value, "false", 5u) == 0 ) return 0;
	return fallback;
}

static int xredis_array_range_for_key(const char* start, const char* end, const char* key, const char** out_start, const char** out_end)
{
	const char* value;
	const char* stop;
	if ( !xredis_json_field_range(start, end, key, &value, &stop) ) return 0;
	value = xredis_skip_ws(value);
	if ( value == NULL || value >= stop || *value != '[' ) return 0;
	*out_start = value;
	*out_end = stop;
	return 1;
}

static int xredis_array_item_range(const char* array_start, const char* array_end, int index, const char** out_start, const char** out_end)
{
	const char* p;
	const char* item_end;
	int i = 0;
	if ( array_start == NULL || *array_start != '[' || index < 0 ) return 0;
	p = array_start + 1;
	while ( p < array_end ) {
		p = xredis_skip_ws(p);
		if ( p == NULL || p >= array_end || *p == ']' ) return 0;
		item_end = xredis_json_value_end(p, array_end);
		if ( item_end == NULL || item_end < p ) return 0;
		if ( i == index ) {
			*out_start = p;
			*out_end = item_end;
			return 1;
		}
		i += 1;
		p = xredis_skip_ws(item_end);
		if ( p != NULL && p < array_end && *p == ',' ) p += 1;
	}
	return 0;
}

static int xredis_array_length(const char* array_start, const char* array_end)
{
	const char* p;
	const char* item_end;
	int count = 0;
	if ( array_start == NULL || *array_start != '[' ) return 0;
	p = array_start + 1;
	while ( p < array_end ) {
		p = xredis_skip_ws(p);
		if ( p == NULL || p >= array_end || *p == ']' ) return count;
		item_end = xredis_json_value_end(p, array_end);
		if ( item_end == NULL || item_end < p ) return count;
		count += 1;
		p = xredis_skip_ws(item_end);
		if ( p != NULL && p < array_end && *p == ',' ) p += 1;
	}
	return count;
}

const char* xredis_response_type_native(const char* response_json)
{
	return xredis_json_string_field(response_json, NULL, "type");
}

const char* xredis_response_value_native(const char* response_json)
{
	return xredis_json_string_field(response_json, NULL, "value");
}

int xredis_response_int_native(const char* response_json)
{
	return xredis_json_int_field(response_json, NULL, "value", -1);
}

int xredis_response_bool_native(const char* response_json)
{
	return xredis_json_bool_field(response_json, NULL, "value", 0);
}

int xredis_response_nil_native(const char* response_json)
{
	return xredis_json_bool_field(response_json, NULL, "nil", 0);
}

const char* xredis_response_array_value_native(const char* response_json, int index)
{
	const char* array_start;
	const char* array_end;
	const char* item_start;
	const char* item_end;
	if ( !xredis_array_range_for_key(response_json, NULL, "items", &array_start, &array_end) ) return "";
	if ( !xredis_array_item_range(array_start, array_end, index, &item_start, &item_end) ) return "";
	return xredis_json_string_field(item_start, item_end, "value");
}

int xredis_response_array_length_native(const char* response_json)
{
	const char* array_start;
	const char* array_end;
	if ( !xredis_array_range_for_key(response_json, NULL, "items", &array_start, &array_end) ) return 0;
	return xredis_array_length(array_start, array_end);
}

int xredis_response_map_length_native(const char* response_json)
{
	const char* array_start;
	const char* array_end;
	if ( !xredis_array_range_for_key(response_json, NULL, "pairs", &array_start, &array_end) ) return 0;
	return xredis_array_length(array_start, array_end);
}

const char* xredis_response_map_key_value_native(const char* response_json, int index)
{
	const char* array_start;
	const char* array_end;
	const char* pair_start;
	const char* pair_end;
	const char* key_start;
	const char* key_end;
	if ( !xredis_array_range_for_key(response_json, NULL, "pairs", &array_start, &array_end) ) return "";
	if ( !xredis_array_item_range(array_start, array_end, index, &pair_start, &pair_end) ) return "";
	if ( !xredis_json_field_range(pair_start, pair_end, "key", &key_start, &key_end) ) return "";
	return xredis_json_string_field(key_start, key_end, "value");
}

const char* xredis_response_map_value_value_native(const char* response_json, int index)
{
	const char* array_start;
	const char* array_end;
	const char* pair_start;
	const char* pair_end;
	const char* value_start;
	const char* value_end;
	if ( !xredis_array_range_for_key(response_json, NULL, "pairs", &array_start, &array_end) ) return "";
	if ( !xredis_array_item_range(array_start, array_end, index, &pair_start, &pair_end) ) return "";
	if ( !xredis_json_field_range(pair_start, pair_end, "value", &value_start, &value_end) ) return "";
	return xredis_json_string_field(value_start, value_end, "value");
}

const char* xredis_response_map_value_native(const char* response_json, const char* key_text)
{
	const char* array_start;
	const char* array_end;
	int count;
	int i;
	if ( key_text == NULL ) key_text = "";
	if ( !xredis_array_range_for_key(response_json, NULL, "pairs", &array_start, &array_end) ) return "";
	count = xredis_array_length(array_start, array_end);
	for ( i = 0; i < count; i += 1 ) {
		const char* key = xredis_response_map_key_value_native(response_json, i);
		if ( strcmp(key, key_text) == 0 ) {
			return xredis_response_map_value_value_native(response_json, i);
		}
	}
	return "";
}

int xredis_response_map_has_key_native(const char* response_json, const char* key_text)
{
	const char* array_start;
	const char* array_end;
	int count;
	int i;
	if ( key_text == NULL ) key_text = "";
	if ( !xredis_array_range_for_key(response_json, NULL, "pairs", &array_start, &array_end) ) return 0;
	count = xredis_array_length(array_start, array_end);
	for ( i = 0; i < count; i += 1 ) {
		const char* key = xredis_response_map_key_value_native(response_json, i);
		if ( strcmp(key, key_text) == 0 ) {
			return 1;
		}
	}
	return 0;
}

const char* xredis_cluster_slots_node_host_native(const char* slots_response_json, int slot)
{
	const char* slots_start;
	const char* slots_end;
	int slots_count;
	int i;
	if ( !xredis_array_range_for_key(slots_response_json, NULL, "items", &slots_start, &slots_end) ) return "";
	slots_count = xredis_array_length(slots_start, slots_end);
	for ( i = 0; i < slots_count; i += 1 ) {
		const char* range_start;
		const char* range_end;
		const char* items_start;
		const char* items_end;
		const char* start_item;
		const char* start_item_end;
		const char* end_item;
		const char* end_item_end;
		int start_slot;
		int end_slot;
		if ( !xredis_array_item_range(slots_start, slots_end, i, &range_start, &range_end) ) continue;
		if ( !xredis_array_range_for_key(range_start, range_end, "items", &items_start, &items_end) ) continue;
		if ( !xredis_array_item_range(items_start, items_end, 0, &start_item, &start_item_end) ) continue;
		if ( !xredis_array_item_range(items_start, items_end, 1, &end_item, &end_item_end) ) continue;
		start_slot = xredis_json_int_field(start_item, start_item_end, "value", -1);
		end_slot = xredis_json_int_field(end_item, end_item_end, "value", -1);
		if ( slot >= start_slot && slot <= end_slot ) {
			const char* node_item;
			const char* node_item_end;
			const char* node_items;
			const char* node_items_end;
			const char* host_item;
			const char* host_item_end;
			if ( !xredis_array_item_range(items_start, items_end, 2, &node_item, &node_item_end) ) return "";
			if ( !xredis_array_range_for_key(node_item, node_item_end, "items", &node_items, &node_items_end) ) return "";
			if ( !xredis_array_item_range(node_items, node_items_end, 0, &host_item, &host_item_end) ) return "";
			return xredis_json_string_field(host_item, host_item_end, "value");
		}
	}
	return "";
}

int xredis_cluster_slots_node_port_native(const char* slots_response_json, int slot)
{
	const char* slots_start;
	const char* slots_end;
	int slots_count;
	int i;
	if ( !xredis_array_range_for_key(slots_response_json, NULL, "items", &slots_start, &slots_end) ) return -1;
	slots_count = xredis_array_length(slots_start, slots_end);
	for ( i = 0; i < slots_count; i += 1 ) {
		const char* range_start;
		const char* range_end;
		const char* items_start;
		const char* items_end;
		const char* start_item;
		const char* start_item_end;
		const char* end_item;
		const char* end_item_end;
		int start_slot;
		int end_slot;
		if ( !xredis_array_item_range(slots_start, slots_end, i, &range_start, &range_end) ) continue;
		if ( !xredis_array_range_for_key(range_start, range_end, "items", &items_start, &items_end) ) continue;
		if ( !xredis_array_item_range(items_start, items_end, 0, &start_item, &start_item_end) ) continue;
		if ( !xredis_array_item_range(items_start, items_end, 1, &end_item, &end_item_end) ) continue;
		start_slot = xredis_json_int_field(start_item, start_item_end, "value", -1);
		end_slot = xredis_json_int_field(end_item, end_item_end, "value", -1);
		if ( slot >= start_slot && slot <= end_slot ) {
			const char* node_item;
			const char* node_item_end;
			const char* node_items;
			const char* node_items_end;
			const char* port_item;
			const char* port_item_end;
			if ( !xredis_array_item_range(items_start, items_end, 2, &node_item, &node_item_end) ) return -1;
			if ( !xredis_array_range_for_key(node_item, node_item_end, "items", &node_items, &node_items_end) ) return -1;
			if ( !xredis_array_item_range(node_items, node_items_end, 1, &port_item, &port_item_end) ) return -1;
			return xredis_json_int_field(port_item, port_item_end, "value", -1);
		}
	}
	return -1;
}
