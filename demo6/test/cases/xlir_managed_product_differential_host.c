#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int xlir_module_init(void);

void expect_str(const char* actual, const char* expected)
{
    if ( actual == NULL || expected == NULL || strcmp(actual, expected) != 0 ) {
        fprintf(stderr, "managed product differential mismatch\n");
        exit(1);
    }
}

int main(void)
{
    return xlir_module_init() == 1 ? 0 : 1;
}
