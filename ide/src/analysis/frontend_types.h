#ifndef XLANG_ANALYSIS_FRONTEND_TYPES_H
#define XLANG_ANALYSIS_FRONTEND_TYPES_H
#include "src/ast/xl6_ast.h"
/* Same-thread private native scope. Caller owns an immutable root until End;
 * never keep a scope across semantic AST mutation. Roots borrow every node;
 * only the pointer vector is owned. */
typedef struct xls_type_alias_scope {
    struct xls_type_alias_scope* previous;
    const XL6_AstNode* root;
    XL6_AstNode** sourceChildren;
    size_t sourceCount;
    XL6_AstNode filtered;
    XL6_AstNode** items;
    int ready;
} xls_type_alias_scope;
typedef struct xls_type_alias_work {
    uint64_t indexes,rootVisits,aliases,queries,indexedQueries,linearQueries,peakBytes;
} xls_type_alias_work;
/* Attribution counts only lookups inside an explicit scope, including a
 * disabled reference scope. Unscoped semantic/cursor queries are not counted. */
void xlsFrontendTypeAliasBegin(xls_type_alias_scope* scope,const XL6_AstNode* root,size_t bytes);
void xlsFrontendTypeAliasEnd(xls_type_alias_scope* scope);
void xlsFrontendTypeAliasReset(void);
xls_type_alias_work xlsFrontendTypeAliasWork(void);
#endif
