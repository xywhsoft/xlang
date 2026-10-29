#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* The suite emits this translation unit immediately before compiling the
 * host.  Inclusion keeps the intentionally private module initializer in
 * scope while the functions below provide its declared C imports. */
#include "../../build/test/cgen_call_var_arg_exprs_xlir_generated.c"

static int64_t xlir_recorded_value = 0;

void expect_i64(int64_t actual, int64_t expected)
{
    if ( actual != expected ) {
        fprintf(stderr, "XLIR call argument mismatch: actual=%lld expected=%lld\n",
                (long long)actual, (long long)expected);
        exit(1);
    }
}

void record_xvalue(xvalue* value)
{
    int64_t decoded = 0;
    if ( value == NULL || !xrtValueGetInt(value, &decoded) ) {
        fputs("XLIR call argument result is not an integer xvalue\n", stderr);
        exit(1);
    }
    xlir_recorded_value = decoded;
}

int64_t recorded_xvalue(void)
{
    return xlir_recorded_value;
}

int main(void)
{
    return xlir_module_init() == 1 ? 0 : 1;
}
