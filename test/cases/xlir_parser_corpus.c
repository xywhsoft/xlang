#include "src/parser/xl6_parser_api.h"
#include <stdio.h>
#include <string.h>
static unsigned lambdas(const XL6_AstNode* node)
{
    size_t i; unsigned count = node && node->kind == XL6_AST_ANON_FUNC;
    if (node) for (i = 0; i < node->childCount; ++i) count += lambdas(node->children[i]);
    return count;
}
int main(void)
{
    char path[4096];
    while (fgets(path, sizeof(path), stdin)) {
        XL6_ParseResult result; int ok;
        path[strcspn(path, "\r\n")] = 0;
        if (!path[0]) continue;
        xl6ParseResultInit(&result); ok = xl6ParseFile(path, &result);
        printf("%d|%u|%s\n", ok, lambdas(result.root), path);
        xl6ParseResultFree(&result);
    }
    return ferror(stdin) ? 1 : 0;
}
