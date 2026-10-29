#include <stdint.h>
int xlir_public_apply(int (*callback)(int), int value)
{
    return callback(value);
}
int64_t xlir_public_increment(int64_t value)
{
    return value + 1;
}
int64_t xlir_public_double(int64_t value) { return value * 2; }
