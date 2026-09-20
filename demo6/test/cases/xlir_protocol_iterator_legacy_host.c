#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct xvalue xvalue;
int xlang_main(xvalue* arg);

void expect_i64(long long actual, long long expected)
{
    if ( actual != expected ) {
        fprintf(stderr, "legacy protocol iterator integer mismatch: %lld != %lld\n",
            actual, expected);
        exit(2);
    }
}

void expect_str(const char* actual, const char* expected)
{
    if ( actual == NULL || expected == NULL || strcmp(actual, expected) != 0 ) {
        fprintf(stderr, "legacy protocol iterator string mismatch: %s != %s\n",
            actual != NULL ? actual : "(null)",
            expected != NULL ? expected : "(null)");
        exit(3);
    }
}

int main(void)
{
    return xlang_main(NULL);
}
