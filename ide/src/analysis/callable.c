#include "analysis/callable.h"
#include "analysis/frontend_internal.h"
#include "analysis/frontend_index.h"
xls_callable_relations* xlsFrontendCallableRelations(const xls_semantic_index*,size_t,
 int (*)(void*),void*,enum xls_callable_error*);
xls_callable_relations* xlsSemanticCheckCallableRelations(const xls_semantic_check* check,size_t bytes,
 int (*cancelled)(void*),void* user,enum xls_callable_error* error)
{return xlsFrontendCallableRelations(xlsSemanticCheckTypedIndex(check),bytes,cancelled,user,error);}
