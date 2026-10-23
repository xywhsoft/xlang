#include "../src/xlang/xhttp_native_internal.h"
#include <assert.h>
#include <string.h>

int main(void)
{
	xbuffer* text = xrtBufferFrom((xbytesview){(const uint8*)"hello", 5u});
	xbuffer* binary = xrtBufferFrom((xbytesview){(const uint8*)"a\0b", 3u});
	xbuffer* spare;
	char* owned;
	bytes previous;
	bytes allocation = xrtMalloc(6u);

	assert(text != NULL && binary != NULL && allocation != NULL);
	memcpy(allocation, "hello", 5u);
	spare = xrtBufferCreateTake(&allocation, 5u, 6u);
	assert(spare != NULL && allocation == NULL);
	previous = spare->Data;
	owned = (char*)xhttp_buffer_take_cstr(spare);
	assert((bytes)owned == previous && strcmp(owned, "hello") == 0);
	xrtFree(owned);
	xrtBufferDestroy(spare);

	owned = (char*)xhttp_buffer_take_cstr(text);
	assert(owned != NULL && strcmp(owned, "hello") == 0);
	assert(text->Data == NULL && text->Size == 0u);
	xrtFree(owned);
	xrtBufferDestroy(text);

	previous = binary->Data;
	assert(xhttp_buffer_take_cstr(binary) == NULL);
	assert(binary->Data == previous && binary->Size == 3u);
	xrtBufferDestroy(binary);
	return 0;
}
