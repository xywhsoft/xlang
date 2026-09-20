#include <stdint.h>
int xlir_public_apply(int (*callback)(int), int value)
{
    return callback(value);
}
void xlir_public_change(int64_t* value, int64_t* doubled)
{
    ++*value;
    *doubled = *value * 2;
}
