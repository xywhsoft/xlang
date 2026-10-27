#ifndef XLANG_ANALYSIS_FUNCTION_UNIT_H
#define XLANG_ANALYSIS_FUNCTION_UNIT_H
#include "analysis/index.h"
#include "analysis/input_view.h"
#include <stdint.h>

typedef struct xls_function_unit xls_function_unit;
typedef struct xls_function_set xls_function_set;
struct xls_semantic_check;
struct xls_context;
struct xls_entity_view;
struct xls_module_graph;

typedef struct xls_function_info {
    const char *name,*path;
    size_t nodes,facts,treeBytes,ownedBytes,originBytes;
    uint64_t entityDomain;
    uint32_t symbol;
} xls_function_info;
/* Complete final checked AST values. Node IDs are one based in THIS unit,
 * zero means absent. Children retain their slot order. Type edges have their own
 * node IDs. Native symbol/type/region IDs remain in the PRODUCING compilation;
 * neither their value nor entity identity proves validity in another result.
 * All returned strings/literal bytes borrow the explicitly retained unit. */
typedef struct xls_function_node {
    const char *kindName,*text,*doc,*sourcePath;
    const unsigned char* literalData;
    size_t literalSize,children,completionStart,completionEnd;
    uint32_t id,parent,inferredType,declaredType,kind,flags,literalScalar;
    uint32_t type,symbol,declaredSymbol,targetSymbol,compoundOperatorSymbol;
    uint64_t typeAbi;
    uint32_t scope,targetScope,region,targetRegion;
    int literalDecoded,call,lvalue,ownership,exit,line,column,hasSource,completionQuery;
    xls_index_range range,tokenRange;
} xls_function_node;
/* Origin metadata copies no source bodies or producing result. Retained
 * existing stores can independently retain/evict ordinary syntax and text.
 * This is a historical origin stamp, NEVER automatic freshness validation.
 * Consumers must establish current inputs and complete dependency/domain
 * proofs before using units for another analysis or publishing results. */
typedef struct xls_function_source {
    const char *path,*origin;
    size_t size,rawSize;
    int kind;
    unsigned char textSha256[32],rawSha256[32];
} xls_function_source;

/* Explicit, demand-only capture from a successful check. Functions, async,
 * export functions, methods and nested/anonymous functions are enumerated in
 * AST child preorder; inferred/declared-type forests do not add functions.
 * Count has no I/O/allocation and rejects known-invalid/error results.
 * Capture validates the owner before/after construction and is cancellable.
 * The producer is retained only during capture,never by a returned unit.
 * No snapshot is constructed on ordinary check/completion/signature paths.
 * byteLimit bounds cloned AST plus known unit vectors/canonical strings;
 * max64MiB. Shared origin metadata has a separate max16MiB allowance.
 * Retained context/module/entity stores, allocator and transient clone maps
 * are outside these local quotas, NOT a whole-process memory budget. */
int xlsSemanticCheckFunctionCount(const struct xls_semantic_check*,size_t* count);
xls_function_unit* xlsSemanticCheckFunctionCapture(struct xls_semantic_check*,size_t ordinal,
    size_t byteLimit,int (*cancelled)(void*),void* user,char* error,size_t capacity);
typedef struct xls_function_set_info {size_t functions,ownedBytes,originBytes;uint64_t entityDomain;} xls_function_set_info;
/* Explicit whole-function collection,with ONE shared immutable origin stamp.
 * max256MiB known collection+unit+origin data;individual trees still<=64MiB.
 * No partial collection is published on cancellation/resource failure.
 * This owns checked functions,not the complete module/type registry/final IR.
 * Ordinary analysis never constructs a collection. Returned units retain
 * independently and survive set/check/worker destruction. */
xls_function_set* xlsSemanticCheckFunctionsCapture(struct xls_semantic_check*,size_t byteLimit,
    int (*cancelled)(void*),void* user,char* error,size_t capacity);
xls_function_set* xlsFunctionSetRetain(xls_function_set*);
void xlsFunctionSetRelease(xls_function_set*);
xls_function_set_info xlsFunctionSetInfo(const xls_function_set*);
/* One owned reference,or NULL for an invalid ordinal. No allocation or I/O. */
xls_function_unit* xlsFunctionSetUnit(const xls_function_set*,size_t ordinal);
xls_function_unit* xlsFunctionUnitRetain(xls_function_unit*);
void xlsFunctionUnitRelease(xls_function_unit*);
xls_function_info xlsFunctionUnitInfo(const xls_function_unit*);
int xlsFunctionUnitNode(const xls_function_unit*,uint32_t id,xls_function_node*);
int xlsFunctionUnitChild(const xls_function_unit*,uint32_t id,size_t child,uint32_t* target);
/* Exact ORIGINAL collector rows, not recollected against a miniature module.
 * id/parent/source ordinals remain in the producing compilation. In particular
 * the root fact's parent may be outside this function. This preserves global
 * alias/type and selection rules. No invented local ID remapping. */
int xlsFunctionUnitFact(const xls_function_unit*,size_t at,xls_index_fact*);
int xlsFunctionUnitInputs(const xls_function_unit*,xls_inputs*);
int xlsFunctionUnitSource(const xls_function_unit*,size_t at,xls_function_source*);
int xlsFunctionUnitProbe(const xls_function_unit*,size_t at,xls_input_probe*);
const struct xls_context* xlsFunctionUnitContext(const xls_function_unit*);
const struct xls_entity_view* xlsFunctionUnitEntities(const xls_function_unit*);
const struct xls_module_graph* xlsFunctionUnitModules(const xls_function_unit*);
#endif
