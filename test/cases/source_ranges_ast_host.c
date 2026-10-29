#include "src/ast/xl6_ast.h"
#include <stdio.h>
#include <stdlib.h>
#define REQUIRE(c) do { if (!(c)) { fprintf(stderr,"ast_range_host:%d: %s\n",__LINE__,#c); exit(1); } } while (0)
int main(void)
{
    XL6_Token token; XL6_AstRange a, b; XL6_AstNode *original, *copy;
    memset(&token, 0, sizeof(token)); token.startOffset=3; token.endOffset=9;
    token.startLine=2; token.endLine=2; token.startColumn=4; token.endColumn=6;
    original=xl6AstNewText(XL6_AST_NAME,"计算",strlen("计算"),&token); REQUIRE(original);
    copy=xl6AstNew(XL6_AST_NAME,NULL); REQUIRE(copy && !copy->range.known);
    xl6AstCopySource(copy,original); /* Both source pointers are NULL. */
    REQUIRE(copy->range.known && copy->range.startOffset==3 && copy->tokenRange.endColumn==6);
    REQUIRE(xl6AstSetSourceTree(original,"真实 Ω.xl")); xl6AstCopySource(copy,original);
    REQUIRE(strcmp(xl6AstSourcePath(copy),"真实 Ω.xl")==0);
    xl6AstFree(original); REQUIRE(strcmp(xl6AstSourcePath(copy),"真实 Ω.xl")==0);
    xl6AstCopySource(copy,NULL); REQUIRE(!copy->range.known && !copy->tokenRange.known && !xl6AstSourcePath(copy));
    a=xl6AstRangeFromToken(&token); token.startOffset=0; token.endOffset=12;
    token.startLine=1; token.startColumn=1; token.endLine=3; token.endColumn=2;
    b=xl6AstRangeFromToken(&token); xl6AstRangeInclude(&a,&b);
    REQUIRE(a.startOffset==0 && a.endOffset==12 && a.startLine==1 && a.endLine==3);
    token.endOffset=0; token.startOffset=1; REQUIRE(!xl6AstRangeFromToken(&token).known);
    REQUIRE(!xl6AstRangeFromToken(NULL).known); xl6AstFree(copy);
    puts("AST range copy/source lifetime: PASS"); return 0;
}
