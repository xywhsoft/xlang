#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct xerror xerror;
extern const xerror* xrtGetError(void);
extern const char* xrtErrorMessage(const xerror* error);

int64_t native_add_i64(int64_t left, int64_t right)
{
    return left + right;
}

void expect_i64(int64_t actual, int64_t expected)
{
    if ( actual != expected ) {
        fprintf(stderr, "XLIR xcall mismatch: %lld != %lld\n",
                (long long)actual, (long long)expected);
        exit(1);
    }
}

int main(void)
{
    if ( xlir_module_init() ) return 0;
    {
        const xerror* error = xrtGetError();
        if ( error != NULL )
            fprintf(stderr, "XLIR xcall module failure: %s\n",
                    xrtErrorMessage(error));
    }
    return 1;
}
