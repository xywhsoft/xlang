#ifndef XLS_ANALYSIS_CALLABLE_H
#define XLS_ANALYSIS_CALLABLE_H
#include "analysis/check.h"
typedef struct xls_callable_relations xls_callable_relations;
enum xls_callable_kind {XLS_CALLABLE_OVERLOAD=4,XLS_CALLABLE_PROPERTY=8};
enum xls_callable_error {XLS_CALLABLE_OK,XLS_CALLABLE_INVALID,XLS_CALLABLE_LIMIT,XLS_CALLABLE_MEMORY,XLS_CALLABLE_CANCELLED};
typedef struct xls_callable_relation {uint32_t declaration,implementation;unsigned kind;} xls_callable_relation;
/* Demand-only same-lexical-owner declaration associations. The original native
 * callable/member classifiers and overload predicate remain authoritative.
 * GET and LET of the same native property name are associated for source
 * rename, including indexed accessors. Never bind a reference by this name.
 * IDs belong to the producing compilation; this is not complete workspace or
 * generic closure, edit permission, a reference index or freshness proof.
 * No AST/check/index is retained. Default/max32MiB charges concurrent scratch
 * and owned result, excluding borrowed index/AST and allocator/sort internals.
 * Poll between bounded rows; bounded library sorting is not preemptible. */
xls_callable_relations* xlsSemanticCheckCallableRelations(const xls_semantic_check*,size_t,
 int (*cancelled)(void*),void*,enum xls_callable_error*);
void xlsCallableRelationsDestroy(xls_callable_relations*);
size_t xlsCallableRelationsBytes(const xls_callable_relations*);
size_t xlsCallableRelationsCount(const xls_callable_relations*);
int xlsCallableRelationsAt(const xls_callable_relations*,size_t,xls_callable_relation*);
#endif
