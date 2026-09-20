#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int xlir_module_init(void);

void expect_i64(int64_t actual, int64_t expected)
{
    if ( actual != expected ) {
        fprintf(stderr, "nullable cast mismatch: %lld != %lld\n",
                (long long)actual, (long long)expected);
        exit(1);
    }
}

void expect_str(const char* actual, const char* expected)
{
    if ( actual == NULL || expected == NULL || strcmp(actual, expected) != 0 ) {
        fprintf(stderr, "nullable cast string mismatch: %s != %s\n",
                actual != NULL ? actual : "(null)",
                expected != NULL ? expected : "(null)");
        exit(1);
    }
}

int main(void)
{
    return xlir_module_init() == 1 ? 0 : 1;
}
