#ifndef XLANG_ANALYSIS_FRONTEND_STDLIB_H
#define XLANG_ANALYSIS_FRONTEND_STDLIB_H
#include "src/ast/xl6_ast.h"
#include "src/diag/xl6_diag.h"
#include <stdint.h>
typedef struct xls_stdlib_work { uint64_t nodes,names,bytes,queries,pruned,predicateVisits; } xls_stdlib_work;
/* Private measurement/oracle boundary. Normal expansion uses an 8 MiB
 * spelling-index budget. A failed index always uses the shared predicates. */
int xlsFrontendStdlibExpand(XL6_AstNode* root,XL6_DiagList* diags,size_t bytes,xls_stdlib_work* work);
int xlsFrontendStdlibExpandReference(XL6_AstNode* root,XL6_DiagList* diags,xls_stdlib_work* work);
#endif
