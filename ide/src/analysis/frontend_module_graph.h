#ifndef XLANG_ANALYSIS_FRONTEND_MODULE_GRAPH_H
#define XLANG_ANALYSIS_FRONTEND_MODULE_GRAPH_H
#include "analysis/module_graph.h"
#include "analysis/module_store.h"
#include "src/compile/xl6_source_report.h"
typedef struct xls_module_graph_scope {struct xls_module_graph_scope* previous;void* state;} xls_module_graph_scope;
/* Optional synchronous capture;disabled/nested scopes shadow outer captures.
 * Defaults/caller clamp8MiB known completed and staging data,65536 nodes and
 * 262144 distinct edges. XRT map metadata/allocator/transient overhead extra.
 * Every failure declines this optional graph without changing language errors.
 * Identity admission is monotonic and may remain after a cancelled capture. */
void xlsModuleGraphBegin(xls_module_graph_scope* scope,xls_module_store* store,xls_context* context,size_t bytes);
/* Called only after ORIGINAL activeSources push,with that exact original stack.
 * No second resolver or extra path normalization. Called at the original root
 * exit BEFORE pop. Failed/unfinished expansion never yields a complete graph. */
void xlsModuleGraphImport(char* const* stack,size_t count);
/* Keep full original expansion when a dependency has no owned graph proof. */
void xlsModuleGraphDecline(void);
void xlsModuleGraphExpanded(char* const* stack,size_t count,int accepted);
xls_module_graph* xlsModuleGraphEnd(xls_module_graph_scope* scope,const XL6_SourceReport* report);
struct xls_semantic_index;
void xlsFrontendIndexAttachModules(struct xls_semantic_index* index,xls_module_graph* graph);
#endif
