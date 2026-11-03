#include <stdlib.h>
#include <stdint.h>

typedef struct protocol_native_counter {
    int64_t Value;
} protocol_native_counter;

void* protocol_native_counter_open(int64_t value)
{
    protocol_native_counter* self =
        (protocol_native_counter*)malloc(sizeof(protocol_native_counter));

    if ( self != NULL ) {
        self->Value = value;
    }
    return self;
}

int64_t protocol_native_counter_close(void* handle)
{
    free(handle);
    return 0;
}

int64_t protocol_native_counter_value(void* handle)
{
    protocol_native_counter* self = (protocol_native_counter*)handle;
    return self != NULL ? self->Value : 0;
}
