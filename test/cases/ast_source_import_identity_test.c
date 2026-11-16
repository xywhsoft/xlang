#include "src/ast/xl6_ast.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    xmemdebugsnapshot baseline,after,steady;
    assert(xrtMemDebugEnabled());
    xrtMemDebugSnapshot(&baseline);
    XL6_AstNode* original=xl6AstNew(XL6_AST_MODULE,NULL);
    XL6_AstNode* copy=xl6AstNew(XL6_AST_MODULE,NULL);
    assert(original && copy && xl6AstSetSourceTree(original,"same-file.xl"));
    assert(!original->source->importNamespace);
    assert(xrtMemDebugFailAfter(0));
    assert(!xl6AstBindSourceNamespace(original,"package"));
    assert(xrtMemDebugFailTriggered());xrtMemDebugFailClear();xrtClearError();
    assert(!original->source->importNamespace);
    assert(xl6AstBindSourceNamespace(original,"package"));
    xl6AstCopySource(copy,original);
    assert(copy->source==original->source && copy->source->references==2);
    xrtMemDebugSnapshot(&steady);
    assert(xl6AstBindSourceNamespace(copy,"package"));
    assert(!xl6AstBindSourceNamespace(copy,"other-instance"));
    xrtMemDebugSnapshot(&after);
    assert(after.AllocCount==steady.AllocCount && after.ReallocCount==steady.ReallocCount);
    xl6AstFree(original);
    assert(copy->source->references==1 && !strcmp(copy->source->path,"same-file.xl"));
    assert(!strcmp(copy->source->importNamespace,"package"));
    // Replacing the last source also releases its immutable namespace.
    assert(xl6AstSetSourceTree(copy,"new-file.xl"));
    assert(!copy->source->importNamespace);
    xl6AstFree(copy);
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==baseline.LiveCount && after.LiveBytes==baseline.LiveBytes);
    puts("AST_IMPORT_SOURCE_IDENTITY_OK");
    return 0;
}
