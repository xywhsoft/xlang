#include "src/sem/xl6_sem_field_attrs.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>

int main(int argc, char** argv)
{
    XL6_AstNode field = { .kind = XL6_AST_FIELD };
    XL6_AstNode skip = { .kind = XL6_AST_NAME, .text = "skip" };
    XL6_AstNode include = { .kind = XL6_AST_NAME, .text = "include" };
    XL6_AstNode* args[] = { &skip };
    XL6_AstNode attr = { .kind = XL6_AST_FIELD_ATTR, .text = "json", .children = args, .childCount = 1 };
    XL6_AstNode* children[] = { NULL, &attr };
    assert(argc == 4);
    assert(xl6SemFieldPersistenceIncluded(&field, "json"));
    assert(xl6SemFieldPersistenceIncluded(&field, "xlon"));
    field.children = children; field.childCount = 2;
    assert(!xl6SemFieldPersistenceIncluded(&field, "json"));
    assert(xl6SemFieldPersistenceIncluded(&field, "xlon"));
    attr.text = "persist";
    assert(!xl6SemFieldPersistenceIncluded(&field, "json"));
    assert(!xl6SemFieldPersistenceIncluded(&field, "xlon"));
    field.flags = XL6_AST_FLAG_PRIVATE; field.childCount = 0;
    assert(!xl6SemFieldPersistenceIncluded(&field, "json"));
    args[0] = &include; field.childCount = 2;
    assert(xl6SemFieldPersistenceIncluded(&field, "json"));
    assert(xl6SemFieldPersistenceIncluded(&field, "xlon"));
    field.flags |= XL6_AST_FLAG_STATIC;
    assert(!xl6SemFieldPersistenceIncluded(&field, "json"));
    assert(!xl6SemFieldPersistenceIncluded(&field, "xlon"));
    assert(!xl6SemFieldPersistenceIncluded(&field, "other"));
    assert(!xl6SemFieldPersistenceIncluded(NULL, "json"));
    for (int i = 1; i < argc; ++i) {
        XL6_ParseResult parse; xl6ParseResultInit(&parse);
        assert(xl6ParseFile(argv[i], &parse) && xl6StdlibExpandModule(parse.root, &parse.diags));
        int ok = xl6SemCheckModule(parse.root, &parse.diags, "field_inclusion_contract");
        assert(i == 1 ? ok : !ok);
        if (i != 1) assert(parse.diags.count);
        xl6ParseResultFree(&parse);
    }
    puts("Field inclusion: 13 direct partitions and 3 actual semantic programs accepted/refused");
    return 0;
}
