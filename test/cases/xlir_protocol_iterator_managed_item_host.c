#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int xlir_module_init(void);
typedef struct xerror xerror;
extern const xerror* xrtGetError(void);
extern const char* xrtErrorMessage(const xerror* error);

void expect_i64(long long actual, long long expected)
{
    if ( actual != expected ) {
        fprintf(stderr, "protocol iterator integer mismatch: %lld != %lld\n",
            actual, expected);
        exit(2);
    }
}

void expect_str(const char* actual, const char* expected)
{
    if ( actual == NULL || expected == NULL || strcmp(actual, expected) != 0 ) {
        fprintf(stderr, "protocol iterator string mismatch: %s != %s\n",
            actual != NULL ? actual : "(null)",
            expected != NULL ? expected : "(null)");
        exit(3);
    }
}

int main(void)
{
    if ( xlir_module_init() ) return 0;
    {
        const xerror* error = xrtGetError();
        fprintf(stderr, "protocol iterator module failure: %s\n",
            error != NULL ? xrtErrorMessage(error) : "(none)");
    }
    return 1;
}
