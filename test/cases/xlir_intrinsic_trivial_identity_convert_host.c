#include <stdlib.h>

static int xlir_identity_point_token;

void* xlir_identity_point_source(void)
{
    return &xlir_identity_point_token;
}

void xlir_expect_same_point(void* actual, void* expected)
{
    if ( actual != expected ) exit(1);
}
