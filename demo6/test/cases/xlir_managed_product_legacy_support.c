#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void expect_str(char* actual, char* expected)
{
    if ( actual == NULL || expected == NULL || strcmp(actual, expected) != 0 ) {
        fprintf(stderr, "legacy managed product mismatch\n");
        exit(1);
    }
}
