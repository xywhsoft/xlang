#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int xlir_module_init(void);
typedef struct xerror xerror;
extern const xerror* xrtGetError(void);
extern const char* xrtErrorMessage(const xerror* error);
extern void xrtClearError(void);

void expect_i64(long long actual, long long expected)
{
    if ( actual != expected ) {
        fprintf(stderr, "XLIR dynamic iterator mismatch: %lld != %lld\n",
                actual, expected);
        exit(1);
    }
}

void expect_str(const char* actual, const char* expected)
{
    if ( actual == NULL || expected == NULL || strcmp(actual, expected) != 0 ) {
        fprintf(stderr, "XLIR dynamic iterator string mismatch: %s != %s\n",
                actual != NULL ? actual : "(null)",
                expected != NULL ? expected : "(null)");
        exit(1);
    }
}

int main(void)
{
    xvalue* wrong = xrtValueIntMap();
    xlir_xvalue_iterator* state;
    const xerror* error;
    if ( wrong == NULL ) return 1;
    state = xlir_xvalue_iterator_create_checked(wrong, 9);
    error = xrtGetError();
    if ( state != NULL || error == NULL ||
         strcmp(xrtErrorMessage(error),
                "for in value must be array, list, set or dict") != 0 ) {
        fprintf(stderr,
                "XLIR refined iterator accepted a list as an array\n");
        if ( state != NULL ) xlir_xvalue_iterator_destroy(state);
        xrtValueRelease(wrong);
        return 1;
    }
    xrtClearError();
    xrtValueRelease(wrong);
    if ( xlir_module_init() ) return 0;
    error = xrtGetError();
    if ( error != NULL )
        fprintf(stderr, "XLIR module failure: %s\n", xrtErrorMessage(error));
    return 1;
}
