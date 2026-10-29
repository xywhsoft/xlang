#include <__xl_runtime/xruntime_decl.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int64_t released;

void* extlib_owned_text(xstrview text)
{
    char* copy = malloc(text.Size + 1);
    assert(copy != NULL);
    if (text.Size != 0) memcpy(copy, text.Data, text.Size);
    copy[text.Size] = 0;
    return copy;
}

void* extlib_owned_bytes(xbytesview data)
{
    void* copy = malloc(data.Size != 0 ? data.Size : 1);
    assert(copy != NULL);
    if (data.Size != 0) memcpy(copy, data.Data, data.Size);
    return copy;
}

void extlib_owned_free(void* data)
{
    assert(data != NULL);
    ++released;
    free(data);
}

int64_t extlib_owned_released(void) { return released; }

bool extlib_text_is_pointer(xstrview text, void* expected)
{
    return text.Data == expected;
}
