#include <stdbool.h>
#include <stdlib.h>

bool native_flip_bool(bool value)
{
    return !value;
}

void expect_bool(bool actual, bool expected)
{
    if ( actual != expected ) {
        exit(1);
    }
}
