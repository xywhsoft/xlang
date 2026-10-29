#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct NativeBuffer {
    unsigned char* data;
    size_t size;
} NativeBuffer;

typedef struct NativeBufferIterator {
    NativeBuffer* buffer;
    size_t index;
} NativeBufferIterator;

#ifndef XLIR_GENERATED_OWNS_BUFFER_RUNTIME
static int native_buffer_destroy_count = 0;
static int native_iterator_destroy_count = 0;

void* xl6_xrt_buffer_create(void)
{
    return calloc(1, sizeof(NativeBuffer));
}

void xl6_xrt_buffer_destroy(void* handle)
{
    NativeBuffer* buffer = (NativeBuffer*)handle;
    if ( buffer != NULL ) {
        free(buffer->data);
        free(buffer);
        native_buffer_destroy_count += 1;
    }
}

bool xl6_xrt_buffer_append_string(void* handle, const char* text)
{
    NativeBuffer* buffer = (NativeBuffer*)handle;
    size_t size = text != NULL ? strlen(text) : 0;
    unsigned char* data;
    if ( buffer == NULL ) return false;
    data = size != 0 ? (unsigned char*)malloc(size) : NULL;
    if ( size != 0 && data == NULL ) return false;
    if ( size != 0 ) memcpy(data, text, size);
    free(buffer->data);
    buffer->data = data;
    buffer->size = size;
    return true;
}

void* xl6_xrt_buffer_iter_create(void* handle)
{
    NativeBufferIterator* iterator =
        (NativeBufferIterator*)calloc(1, sizeof(NativeBufferIterator));
    if ( iterator != NULL ) iterator->buffer = (NativeBuffer*)handle;
    return iterator;
}

void* xl6_xrt_buffer_iter_wrap_handle(void* handle)
{
    return handle;
}

void xl6_xrt_buffer_iter_destroy(void* handle)
{
    if ( handle != NULL ) {
        free(handle);
        native_iterator_destroy_count += 1;
    }
}

bool xl6_xrt_buffer_iter_has_next(void* handle)
{
    NativeBufferIterator* iterator = (NativeBufferIterator*)handle;
    return iterator != NULL && iterator->buffer != NULL &&
        iterator->index < iterator->buffer->size;
}

int64_t xl6_xrt_buffer_iter_next(void* handle)
{
    NativeBufferIterator* iterator = (NativeBufferIterator*)handle;
    if ( iterator == NULL || iterator->buffer == NULL ||
         iterator->index >= iterator->buffer->size ) return 0;
    return (int64_t)iterator->buffer->data[iterator->index++];
}
#endif

int main(void)
{
    int result = xlang_main(NULL);
    if ( result != 0 ) return result;
#ifdef XLIR_GENERATED_OWNS_BUFFER_RUNTIME
    return 0;
#else
    return native_buffer_destroy_count == 1 &&
           native_iterator_destroy_count == 1 ? 0 : 4;
#endif
}
