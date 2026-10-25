#ifndef XLANG_ANALYSIS_MODULE_GRAPH_H
#define XLANG_ANALYSIS_MODULE_GRAPH_H
#include "analysis/context.h"
typedef struct xls_module_graph xls_module_graph;
/* Immutable compilation ownership/dependencies, independent of compiler-local
 * SymbolId,TypeId and source ordinal. IDs belong to the retained shared module
 * store and preserve its exact logical UTF-8 labels,not physical alias merging.
 * Original import expansion supplies direct edges;these are resolved SOURCE
 * dependencies,not lexical imports,negative probes or an entire workspace.
 * Manifest/source report rows keep their original source ordinal separately.
 * A graph never proves current freshness or permits skipping compiler rules. */
enum xls_module_role {XLS_MODULE_SOURCE=1,XLS_MODULE_MANIFEST=2,XLS_MODULE_ENTRY=4,XLS_MODULE_DECLARED_MEMBER=8};
typedef struct xls_module_node {uint64_t identity;const char* path;unsigned roles,source;} xls_module_node;
typedef struct xls_module_edge {uint64_t importer,imported;} xls_module_edge;
typedef struct xls_module_graph_info {size_t modules,edges,sources,bytes;uint64_t entry;} xls_module_graph_info;
xls_module_graph* xlsModuleGraphRetain(xls_module_graph* graph);
void xlsModuleGraphRelease(xls_module_graph* graph);
xls_module_graph_info xlsModuleGraphInfo(const xls_module_graph* graph);
const xls_context* xlsModuleGraphContext(const xls_module_graph* graph);
struct xls_module_store;
const struct xls_module_store* xlsModuleGraphStore(const xls_module_graph* graph);
int xlsModuleGraphNode(const xls_module_graph* graph,size_t index,xls_module_node* node);
int xlsModuleGraphEdge(const xls_module_graph* graph,size_t index,xls_module_edge* edge);
/* Original report ordinal (1 based),zero for absent/unavailable. No I/O,
 * allocation or path matching. Borrow graph while accessing any returned view. */
uint64_t xlsModuleGraphSource(const xls_module_graph* graph,size_t source);
struct xls_semantic_check;struct xls_semantic_completion;
/* Borrowed from a validated owner;NULL when optional capture was unavailable.
 * Retain the graph explicitly to outlive its producing result/worker/store. */
const xls_module_graph* xlsSemanticCheckModules(const struct xls_semantic_check* result);
const xls_module_graph* xlsSemanticCompletionModules(const struct xls_semantic_completion* result);
#endif
