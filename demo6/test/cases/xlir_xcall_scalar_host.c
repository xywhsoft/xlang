#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct xvalue xvalue;
typedef struct xerror xerror;
extern bool xrtValueGetBool(const xvalue*, bool*);
extern bool xrtValueGetFloat(const xvalue*, double*);
extern const xerror* xrtGetError(void);
extern const char* xrtErrorMessage(const xerror*);

void expect_xvalue_bool(xvalue* actual, bool expected)
{
    bool value = false;
    if ( !xrtValueGetBool(actual, &value) || value != expected ) {
        fprintf(stderr, "XLIR scalar xcall bool mismatch\n");
        exit(1);
    }
}

void expect_xvalue_float(xvalue* actual, double expected)
{
    double value = 0.0;
    if ( !xrtValueGetFloat(actual, &value) || value != expected ) {
        fprintf(stderr, "XLIR scalar xcall float mismatch\n");
        exit(1);
    }
}

int main(void)
{
    if ( xlir_module_init() ) return 0;
    {
        const xerror* error = xrtGetError();
        if ( error != NULL )
            fprintf(stderr, "XLIR scalar xcall module failure: %s\n",
                    xrtErrorMessage(error));
    }
    return 1;
}
