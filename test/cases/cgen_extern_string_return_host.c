#include <stdlib.h>
#include <string.h>

const char* native_text(void)
{
    return "native text";
}

const char* native_shared_text(long long which)
{
    static char text[32];
    const char* value = which == 1 ? "first" : "second";
    memcpy(text, value, strlen(value) + 1);
    return text;
}

void* native_text_open(void)
{
    return malloc(1);
}

long long native_text_close(void* self)
{
    free(self);
    return 0;
}

const char* native_text_shared(void* self, long long which)
{
    static char text[32];
    const char* value = which == 1 ? "member first" : "member second";
    (void)self;
    memcpy(text, value, strlen(value) + 1);
    return text;
}

void expect_i64(long long actual, long long expected)
{
    if ( actual != expected ) {
        exit(1);
    }
}
