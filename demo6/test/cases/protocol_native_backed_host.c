#include <stdlib.h>

typedef struct protocol_native_counter {
    long long Value;
} protocol_native_counter;

void* protocol_native_counter_open(long long value)
{
    protocol_native_counter* self =
        (protocol_native_counter*)malloc(sizeof(protocol_native_counter));

    if ( self != NULL ) {
        self->Value = value;
    }
    return self;
}

long long protocol_native_counter_close(void* handle)
{
    free(handle);
    return 0;
}

long long protocol_native_counter_value(void* handle)
{
    protocol_native_counter* self = (protocol_native_counter*)handle;
    return self != NULL ? self->Value : 0;
}
