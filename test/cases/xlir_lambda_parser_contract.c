#include "src/parser/xl6_parser_api.h"
#include <assert.h>
#include <stdio.h>
static unsigned lambdas(const XL6_AstNode* node)
{
    size_t i; unsigned count = node && node->kind == XL6_AST_ANON_FUNC;
    if (node) for (i = 0; i < node->childCount; ++i) count += lambdas(node->children[i]);
    return count;
}
int main(void)
{
    static const struct { const char* text; unsigned expected; } cases[] = {
        {"use(func() as int\nreturn 42\nend)\n", 1},
        {"use(func()\nnotify()\nend)\n", 1},
        {"use(func(int n) as int\nreturn n\nend)\n", 1},
        {"use(func() as int => 42)\n", 1},
        {"use(func() as int\nfunc() as int local = func() as int\nreturn 42\nend\nreturn local()\nend)\n", 2},
        {"use(func() as func() as int\nreturn func() as int\nreturn 42\nend\nend)\n", 2},
        {"array<func() as int> values = [func() as int\nreturn 42\nend]\n", 1},
        {"func invoke(func() as int op) as int\nreturn op()\nend\n", 0},
        {"func produce() as (int, func() as int)\nreturn 1, func() as int => 42\nend\n", 1},
        {"use(func() as int\n(func() as int)? missing = null\nreturn 42\nend)\n", 1},
        {"use(func() as int\narray<func() as int> values = []\nreturn 42\nend)\n", 1},
        {"type callbackType = func\nfunc() as int declared\n", 0},
        {"use(func()\nflag = true\nend)\n", 1},
        {"use(func() as int\nflag = true\nreturn 42\nend)\n", 1},
        {"func invoke(func() as int\ncallback) as int\nreturn callback()\nend\n", 0},
        {"func invoke(func() as int\ncallback = func() as int\nreturn 42\nend) as int\nreturn callback()\nend\n", 1},
        {"use(func()\nflag = true\nuse(func()\nother = false\nend)\nend)\n", 2},
        {"use(func()\nflag = true\nif flag then\nnotify()\nend\nend)\n", 1},
        {"use(func() as func() as int\nflag = true\nreturn func() as int\nreturn 42\nend\nend)\n", 2},
        {"class Holder\ncreate(func() as int\ncallback = func() as int\nreturn 42\nend)\nend\nend\n", 1},
        {"use(func()\nflag = true\ntry\nnotify()\nfinally\nnotify()\nend\nend)\n", 1},
        {"use(func()\nflag = true\ndo\nnotify()\nwhile flag end\nend)\n", 1}
    };
    static const char* invalid[] = {
        "use(func()\nflag = true)\n",
        "use(func() as int\nflag = true\nreturn 42)\n",
        "use(func()\nflag = true\nuse(func()\nother = false\nend))\n",
        "func invoke(func() as int\ncallback = func() as int\nreturn 42\nend) as int\nreturn callback()\n"
    };
    unsigned cycle; size_t i; unsigned failed = 0;
    for (cycle = 0; cycle < 20; ++cycle) {
        for (i = 0; i < sizeof(cases)/sizeof(cases[0]); ++i) {
            XL6_ParseResult result; xl6ParseResultInit(&result);
            if (!xl6ParseSource(cases[i].text, "lambda-parser", &result) ||
                lambdas(result.root) != cases[i].expected) {
                fprintf(stderr, "lambda parser case %zu failed, anonymous nodes=%u expected=%u\n", i, lambdas(result.root), cases[i].expected);
                ++failed;
            }
            xl6ParseResultFree(&result);
        }
        for (i = 0; i < sizeof(invalid)/sizeof(invalid[0]); ++i) {
            XL6_ParseResult result; xl6ParseResultInit(&result);
            if (xl6ParseSource(invalid[i], "lambda-parser-invalid", &result)) {
                fprintf(stderr, "lambda parser accepted unterminated case %zu\n", i); ++failed;
            }
            xl6ParseResultFree(&result);
        }
    }
    printf("lambda parser: 22 contexts + 4 invalid cases x 20 rounds, failures=%u\n", failed);
    return failed ? 1 : 0;
}
