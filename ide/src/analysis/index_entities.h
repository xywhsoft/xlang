#ifndef XLANG_ANALYSIS_INDEX_ENTITIES_H
#define XLANG_ANALYSIS_INDEX_ENTITIES_H
#include "analysis/index.h"
#include "analysis/entity_store.h"
#include "analysis/module_graph.h"
typedef struct xls_entity_view xls_entity_view;
typedef struct xls_entity_view_info {size_t symbols,types,ambiguous,bytes;uint64_t domain;} xls_entity_view_info;
typedef struct xls_entity_fact {
 uint64_t declaredSymbol,symbol,targetSymbol,compoundOperatorSymbol;
 uint64_t declaredType,referencedType,typeId,typeObjectId;
} xls_entity_fact;
/* Optional current checked-compilation binding table. Named module/member
 * declarations and callable header parameters use verified declaration keys.
 * Function-body locals,unknown/generated/builtin and ambiguous identities are0;
 * no reference is inferred by spelling. Compiler IDs remain unchanged.
 * ID stability requires equal context/module/declaration key,not equal offsets.
 * A stable ID NEVER proves unchanged types,bodies,dependencies or current inputs.
 * Validate the checked owner before use. Queries allocate nothing and do no I/O. */
const xls_entity_view* xlsSemanticIndexEntities(const xls_semantic_index* index);
int xlsSemanticIndexEntityFact(const xls_semantic_index* index,size_t at,xls_entity_fact* fact);
xls_entity_view* xlsEntityViewRetain(xls_entity_view* view);
void xlsEntityViewRelease(xls_entity_view* view);
xls_entity_view_info xlsEntityViewInfo(const xls_entity_view* view);
const xls_entity_store* xlsEntityViewStore(const xls_entity_view* view);
const xls_module_graph* xlsEntityViewModules(const xls_entity_view* view);
/* Native IDs below belong ONLY to this view's producing compilation. Retaining
 * a view outlives the checked AST/session/worker;it does not make those IDs
 * meaningful in another compilation or establish freshness of its old facts. */
uint64_t xlsEntityViewSymbol(const xls_entity_view* view,uint32_t symbol);
/* Unique current native binding,0 for missing/ambiguous IDs. The external ID
 * must belong to this view's retained store/domain. No allocation or I/O. */
uint32_t xlsEntityViewNativeSymbol(const xls_entity_view* view,uint64_t identity);
uint64_t xlsEntityViewType(const xls_entity_view* view,uint32_t type);
#endif
