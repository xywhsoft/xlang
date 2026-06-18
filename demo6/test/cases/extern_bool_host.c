#include <stdbool.h>

void exit(int code);

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
