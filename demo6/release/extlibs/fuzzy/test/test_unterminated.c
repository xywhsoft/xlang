#define XRT_MODULE_UNICODE_DISTANCE
#define XRT_MODULE_STRING
#include "xrt.h"
#include "../include/fuzzy_native.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "fuzzy bounded scan failed at line %d: %s\n", \
        __LINE__, #condition); \
    return 1; \
} } while (0)

static int check_input(size_t size, int kind)
{
    char* text = (char*)malloc(size);
    int valid;
    CHECK(text != NULL);
    memset(text, 'x', size);
    if (kind == 0)
        valid = xfuzzy_levenshtein(text, "") == -1;
    else if (kind == 1)
        valid = xfuzzy_ratio("", text) == -1;
    else if (kind == 2)
        valid = xfuzzy_match("x", text) == -2;
    else
        valid = xfuzzy_best("x", text) == NULL;
    valid = valid && xrtGetError() != NULL &&
        xrtErrorKind(xrtGetError()) == XERR_RANGE;
    free(text);
    return valid;
}

int main(void)
{
    CHECK(check_input(4u * 4096u + 1u, 0));
    CHECK(check_input(4u * 4096u + 1u, 1));
    CHECK(check_input(1024u * 1024u + 1u, 2));
    CHECK(check_input(8u * 1024u * 1024u + 1u, 3));
    CHECK(xfuzzy_match("x", "x") >= 0 && xrtGetError() == NULL);
    puts("fuzzy bounded scans: PASS");
    return 0;
}
