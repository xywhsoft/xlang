#include "../src/xlang/xhttp_native_internal.h"
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
	fprintf(stderr, "xhttp buffer transfer line %d: %s\n", __LINE__, #condition); \
	return 1; } } while (0)

int main(void)
{
	xbuffer* text = xrtBufferFrom((xbytesview){(const uint8*)"hello", 5u});
	xbuffer* binary = xrtBufferFrom((xbytesview){(const uint8*)"a\0b", 3u});
	xbuffer* spare;
	char* owned;
	bytes previous;
	bytes allocation = xrtMalloc(6u);

	CHECK(text != NULL && binary != NULL && allocation != NULL);
	memcpy(allocation, "hello", 5u);
	spare = xrtBufferCreateTake(&allocation, 5u, 6u);
	CHECK(spare != NULL && allocation == NULL);
	previous = spare->Data;
	owned = (char*)xhttp_buffer_take_cstr(spare);
	CHECK((bytes)owned == previous && strcmp(owned, "hello") == 0);
	xrtFree(owned);
	xrtBufferDestroy(spare);

	owned = (char*)xhttp_buffer_take_cstr(text);
	CHECK(owned != NULL && strcmp(owned, "hello") == 0);
	CHECK(text->Data == NULL && text->Size == 0u);
	xrtFree(owned);
	xrtBufferDestroy(text);

	previous = binary->Data;
	CHECK(xhttp_buffer_take_cstr(binary) == NULL);
	CHECK(binary->Data == previous && binary->Size == 3u);
	xrtBufferDestroy(binary);
	return 0;
}
