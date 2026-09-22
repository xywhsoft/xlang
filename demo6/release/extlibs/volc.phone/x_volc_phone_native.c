#include <stddef.h>
#include <string.h>

#include "xrt.h"

static xmutex* g_xvolc_phone_lock = NULL;
static char* g_xvolc_phone_returns[8];
static size_t g_xvolc_phone_return_slot = 0u;

static void xvolc_phone_init(void)
{
	if ( g_xvolc_phone_lock == NULL ) {
		g_xvolc_phone_lock = xrtMutexCreate();
	}
}

static const char* xvolc_phone_return_copy(const char* text, size_t len)
{
	char* copy;
	size_t slot;
	xvolc_phone_init();
	if ( text == NULL ) {
		text = "";
		len = 0u;
	}
	copy = (char*)xrtMalloc(len + 1u);
	if ( copy == NULL ) {
		return "";
	}
	if ( len > 0u ) {
		memcpy(copy, text, len);
	}
	copy[len] = '\0';
	slot = g_xvolc_phone_return_slot % (sizeof(g_xvolc_phone_returns) / sizeof(g_xvolc_phone_returns[0]));
	g_xvolc_phone_return_slot += 1u;
	if ( g_xvolc_phone_returns[slot] != NULL ) {
		xrtFree(g_xvolc_phone_returns[slot]);
	}
	g_xvolc_phone_returns[slot] = copy;
	return g_xvolc_phone_returns[slot];
}

const char* x_volc_phone_substr(const char* text, int start, int len)
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
	return xvolc_phone_return_copy(text + begin, count);
}
