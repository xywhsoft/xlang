#ifdef __TINYC__
#include "../fuzzy_native.c"
#else
#define XRT_MODULE_UNICODE_DISTANCE
#define XRT_MODULE_STRING
#include "xrt.h"
#include "../include/fuzzy_native.h"
#endif
#include <inttypes.h>
#include <stdio.h>

int main(void)
{
    const char* candidates = "src/main.c\nsrc/utils.c\nlib/parser.c\nlib/lexer.c";
    char* best = (char*)xfuzzy_best("pc", candidates);
    if (best == NULL) return 1;
    printf("levenshtein(kitten, sitting): %" PRId64 "\n",
        xfuzzy_levenshtein("kitten", "sitting"));
    printf("ratio(hello, hallo): %" PRId64 "\n", xfuzzy_ratio("hello", "hallo"));
    printf("score(fb, foobar): %" PRId64 "\n", xfuzzy_match("fb", "foobar"));
    printf("isMatch(xyz, foobar): %s\n",
        xfuzzy_match("xyz", "foobar") >= 0 ? "true" : "false");
    printf("best(pc, ...): %s\n", best);
    xrtFree(best);
    best = (char*)xfuzzy_best("abc", "a_b_c\nabc");
    if (best == NULL) return 1;
    printf("best(abc, ...): %s\n", best);
    puts("fuzzy example: PASS");
    xrtFree(best);
    return 0;
}
