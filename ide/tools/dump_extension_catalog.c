/* 复用正式前端，只导出声明；不执行源码、不解析C、不修改编译器。 */
#include "src/parser/xl6_parser_api.h"
#include "src/ast/xl6_ast_dump.h"
#include <stdio.h>

static void discard_bodies(XL6_AstNode* node)
{
    if (!node) return;
    if (node->kind == XL6_AST_BLOCK) {
        for (size_t i = 0; i < node->childCount; ++i) xl6AstFree(node->children[i]);
        node->childCount = 0;
        return;
    }
    for (size_t i = 0; i < node->childCount; ++i) discard_bodies(node->children[i]);
}

static XL6_AstNode* members(XL6_AstNode* node)
{
    XL6_AstNode* result = NULL;
    if (node->kind != XL6_AST_CLASS && node->kind != XL6_AST_PROTOCOL &&
        node->kind != XL6_AST_UNION && node->kind != XL6_AST_ENUM) return NULL;
    for (size_t i = 0; i < node->childCount; ++i) {
        if (node->children[i]->kind == XL6_AST_LIST) result = node->children[i];
    }
    /* class/protocol 和带载荷 enum 的首个列表不是成员，成员始终为末个列表。 */
    return result;
}

static int emit(XL6_Output* out, XL6_AstNode* node, long parent, long* count)
{
    XL6_AstNode* list = members(node);
    size_t size = list ? list->childCount : 0;
    long index = (*count)++;
    if (index >= 50000) return 0;
    if (index) xl6OutputPuts(",\n", out);
    xl6OutputPrintf(out, "{\"parent\":%ld,\"syntax\":", parent);
    /* 每个声明独立限额；大类成员稍后逐条输出，不重复序列化整棵大树。 */
    if (list) list->childCount = 0;
    int ok = xl6AstDumpJsonRanges(out, node);
    if (list) list->childCount = size;
    if (!ok) return 0;
    xl6OutputPuts("}", out);
    for (size_t i = 0; i < size; ++i)
        if (!emit(out, list->children[i], index, count)) return 0;
    return !xl6OutputHasFailed(out);
}

int main(int argc, char** argv)
{
    XL6_ParseResult parsed;
    XL6_Output out;
    long count = 0;
    int ok;
    if (argc != 3) return 2;
    xl6ParseResultInit(&parsed);
    if (!xl6ParseFile(argv[1], &parsed)) {
        xl6DiagPrintText(&parsed.diags, argv[1]);
        xl6ParseResultFree(&parsed);
        return 1;
    }
    if (!xl6OutputOpenFile(&out, argv[2])) {
        xl6ParseResultFree(&parsed);
        return 1;
    }
    discard_bodies(parsed.root);
    xl6OutputPuts("[", &out);
    ok = 1;
    for (size_t i = 0; i < parsed.root->childCount && ok; ++i)
        ok = emit(&out, parsed.root->children[i], -1, &count);
    xl6OutputPuts("]\n", &out);
    if (xl6OutputHasFailed(&out)) ok = 0;
    xl6OutputClose(&out);
    if (xl6OutputHasFailed(&out)) ok = 0;
    xl6ParseResultFree(&parsed);
    if (!ok) fputs("Declaration export exceeded limits or output failed\n", stderr);
    return ok ? 0 : 1;
}
