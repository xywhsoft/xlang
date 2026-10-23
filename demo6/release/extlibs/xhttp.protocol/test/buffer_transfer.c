#include "../src/xlang/xhttp_protocol_native_internal.h"
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
	fprintf(stderr, "xhttp.protocol buffer transfer line %d: %s\n", __LINE__, #condition); \
	return 1; } } while (0)

int main(void)
{
	bytes data = xrtMalloc(6u);
	xbuffer* text;
	xbuffer* binary;
	char* owned;
	bytes previous;

	CHECK(data != NULL);
	memcpy(data, "hello", 5u);
	text = xrtBufferCreateTake(&data, 5u, 6u);
	binary = xrtBufferFrom((xbytesview){(const uint8*)"a\0b", 3u});
	CHECK(text != NULL && binary != NULL && data == NULL);
	previous = text->Data;
	owned = (char*)xhttp_protocol_buffer_take_cstr(text);
	CHECK((bytes)owned == previous && strcmp(owned, "hello") == 0);
	CHECK(text->Data == NULL && text->Size == 0u);
	xrtFree(owned);
	xrtBufferDestroy(text);

	previous = binary->Data;
	CHECK(xhttp_protocol_buffer_take_cstr(binary) == NULL);
	CHECK(binary->Data == previous && binary->Size == 3u);
	xrtBufferDestroy(binary);
	return 0;
}
