#ifndef XLANG_ANALYSIS_INDEX_MODULES_H
#define XLANG_ANALYSIS_INDEX_MODULES_H
#include "analysis/index.h"
#include "analysis/module_graph.h"
/* Borrowed from the validated checked index owner. SourceId stays compilation
 * local;this map binds it to the shared store's persistent logical module ID.
 * NULL/0 means optional graph unavailable,never an invented identity. */
const xls_module_graph* xlsSemanticIndexModules(const xls_semantic_index* index);
uint64_t xlsSemanticIndexModuleIdentity(const xls_semantic_index* index,size_t source);
#endif
