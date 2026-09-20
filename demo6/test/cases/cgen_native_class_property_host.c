#include <stdlib.h>

typedef struct NativePropertyBox {
    long long value;
} NativePropertyBox;

static long long gClosedCount = 0;

void* native_property_open(long long value)
{
    NativePropertyBox* box = (NativePropertyBox*)malloc(sizeof(NativePropertyBox));
    if ( box == 0 ) {
        exit(2);
    }
    box->value = value;
    return box;
}

#if defined(XLIR_NATIVE_PROPERTY_VOID_DESTROY)
void native_property_close(void* self)
#else
long long native_property_close(void* self)
#endif
{
    NativePropertyBox* box = (NativePropertyBox*)self;
    if ( box == 0 ) {
        exit(3);
    }
    gClosedCount += 1;
    free(box);
#if !defined(XLIR_NATIVE_PROPERTY_VOID_DESTROY)
    return 0;
#endif
}

long long native_property_get_value(void* self)
{
    NativePropertyBox* box = (NativePropertyBox*)self;
    if ( box == 0 ) {
        exit(4);
    }
    return box->value;
}

void native_property_set_value(void* self, long long value)
{
    NativePropertyBox* box = (NativePropertyBox*)self;
    if ( box == 0 ) {
        exit(5);
    }
    box->value = value;
}

long long native_property_get_at(void* self, long long index)
{
    NativePropertyBox* box = (NativePropertyBox*)self;
    if ( box == 0 ) {
        exit(6);
    }
    return box->value + index;
}

void native_property_set_at(void* self, long long index, long long value)
{
    NativePropertyBox* box = (NativePropertyBox*)self;
    if ( box == 0 ) {
        exit(7);
    }
    box->value = value + index;
}

long long native_property_get_cell(void* self, long long x, long long y)
{
    NativePropertyBox* box = (NativePropertyBox*)self;
    if ( box == 0 ) {
        exit(8);
    }
    return box->value + x * 10 + y;
}

void native_property_set_cell(void* self, long long x, long long y, long long value)
{
    NativePropertyBox* box = (NativePropertyBox*)self;
    if ( box == 0 ) {
        exit(9);
    }
    box->value = value + x * 10 + y;
}

long long native_property_closed(void)
{
    return gClosedCount;
}
