#ifndef XLS_TYPED_MODULE_H
#define XLS_TYPED_MODULE_H
typedef struct xls_typed_module xls_typed_module;
typedef struct xls_typed_function xls_typed_function;
/* One finalized semantic tree,independent of check/report/cache lifetimes.
 * Immutable after construction. No previous module,check,report,or result is
 * retained;payload lifetimes follow the original AST/pool/intern contracts.
 * This value is not an input-freshness proof or a current-query domain. */
xls_typed_module* xlsTypedModuleRetain(xls_typed_module*);
void xlsTypedModuleRelease(xls_typed_module*);
/* Final native function ownership,without cloning or retaining its module,
 * check,report or source text. A retained function is immutable. The owner
 * serializes original AST/source/pool teardown shared by sibling functions;
 * reads need no lock. This is NOT a current-input or native-ID rebind proof. */
xls_typed_function* xlsTypedFunctionRetain(xls_typed_function*);
void xlsTypedFunctionRelease(xls_typed_function*);
#endif
