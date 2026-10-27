#ifndef XLANG_ANALYSIS_FUNCTION_UNIT_INTERNAL_H
#define XLANG_ANALYSIS_FUNCTION_UNIT_INTERNAL_H
#include "analysis/function_unit.h"
#include "analysis/index_entities.h"
#include "analysis/frontend_index.h"
#include "analysis/ast_clone.h"
#include "analysis/check.h"
#include "analysis/frontend_internal.h"
typedef struct xls_function_origin xls_function_origin;
/* Borrow immutable captured rows only during a retained check. This report
 * VALUE excludes mutable validation/error fields;final owner validation still
 * decides publication. Never retain this shallow report in a unit. */
int xlsFrontendCheckFunctionInputs(const xls_semantic_check*,XL6_SourceReport*);
static inline int xlsFrontendFunctionDefinition(const XL6_AstNode* node)
{return node && (node->kind==XL6_AST_FUNC || node->kind==XL6_AST_ASYNC_FUNC || node->kind==XL6_AST_EXPORT_FUNC || node->kind==XL6_AST_ANON_FUNC);}
xls_function_origin* xlsFunctionOriginCapture(const XL6_SourceReport*,xls_context*,
    xls_module_graph*,const xls_entity_view*,int (*cancelled)(void*),void* user);
void xlsFunctionOriginRelease(xls_function_origin*);
xls_function_unit* xlsFrontendFunctionUnitCapture(const XL6_AstNode*,const xls_semantic_index*,
    xls_function_origin*,size_t bytes,int (*cancelled)(void*),void* user);
/* Independent value clone only. This does NOT rebind native IDs,registry
 * declarations or inputs into a different compilation. Keep the original
 * checker/rebinding path until complete current-domain proofs are implemented. */
XL6_AstNode* xlsFrontendFunctionUnitMaterialize(const xls_function_unit*,size_t bytes,
    int (*cancelled)(void*),void* user);
const XL6_AstNode* xlsFrontendFunctionUnitRoot(const xls_function_unit*);
xls_function_set* xlsFrontendFunctionSetCapture(const XL6_AstNode*,const xls_semantic_index*,
    xls_function_origin*,size_t bytes,int (*cancelled)(void*),void* user);
#endif
