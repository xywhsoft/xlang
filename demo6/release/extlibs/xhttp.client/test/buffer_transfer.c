#include "../src/xlang/xhttp_client_native_internal.h"
#include <assert.h>
#include <string.h>

int main(void)
{
	bytes data = xrtMalloc(6u);
	xbuffer* text;
	xbuffer* binary;
	char* owned;
	bytes previous;

	assert(data != NULL);
	memcpy(data, "hello", 5u);
	text = xrtBufferCreateTake(&data, 5u, 6u);
	binary = xrtBufferFrom((xbytesview){(const uint8*)"a\0b", 3u});
	assert(text != NULL && binary != NULL && data == NULL);
	previous = text->Data;
	owned = (char*)xhttp_client_buffer_take_cstr(text);
	assert((bytes)owned == previous && strcmp(owned, "hello") == 0);
	assert(text->Data == NULL && text->Size == 0u);
	xrtFree(owned);
	xrtBufferDestroy(text);

	previous = binary->Data;
	assert(xhttp_client_buffer_take_cstr(binary) == NULL);
	assert(binary->Data == previous && binary->Size == 3u);
	xrtBufferDestroy(binary);
	return 0;
}
