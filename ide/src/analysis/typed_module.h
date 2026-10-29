#ifndef XLS_TYPED_MODULE_H
#define XLS_TYPED_MODULE_H
typedef struct xls_typed_module xls_typed_module;
/* One finalized semantic tree,independent of check/report/cache lifetimes.
 * Immutable after construction. No previous module,check,report,or result is
 * retained;payload lifetimes follow the original AST/pool/intern contracts.
 * This value is not an input-freshness proof or a current-query domain. */
xls_typed_module* xlsTypedModuleRetain(xls_typed_module*);
void xlsTypedModuleRelease(xls_typed_module*);
#endif
