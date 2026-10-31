#ifndef XLANG_ANALYSIS_CHECK_IR_H
#define XLANG_ANALYSIS_CHECK_IR_H
#include "analysis/check.h"
/* Creation provenance of optional raw function fragments,not current-query
 * work. Only eligible top-level candidates are counted. Capacities are
 * retained logical cache/index bytes,not full model memory or process RSS. */
typedef struct xls_check_ir_work {
    uint64_t checked,reused,captured,nodesReused,bytes,declined;
    uint64_t textNodes,textBytes,ownerBytes;
} xls_check_ir_work;
xls_check_ir_work xlsSemanticCheckIrWork(const xls_semantic_check* result);
/* Creation provenance of native pure postpass units,not whole final-IR reuse
 * or current-query work. Logical additional bytes are already in raw memo
 * bytes. Exact-result hits keep the producing check's counters. */
typedef struct xls_check_ir_derived_work {
    uint64_t contexts,checked,reused,captured,declined,nodesSkipped,rowsApplied,bytes;
} xls_check_ir_derived_work;
xls_check_ir_derived_work xlsSemanticCheckIrDerivedWork(const xls_semantic_check*);
/* Successful optional-owner creation provenance,not current-query work.
 * Exact-result hits retain it. Fresh/non-owner paths report zero. Metadata
 * bounds exclude model/node/raw payload,transients,allocator overhead andRSS. */
typedef struct xls_check_ir_persistent_work {
    uint64_t built,reused,captured,verified,nodesCopied,tablesShared,keyBytes,metadataBytes,declined,prepared;
} xls_check_ir_persistent_work;
xls_check_ir_persistent_work xlsSemanticCheckIrPersistentWork(const xls_semantic_check*);
typedef struct xls_check_ir_dependency {uint64_t declaration,version;} xls_check_ir_dependency;
size_t xlsSemanticCheckIrDependencies(const xls_semantic_check* result,uint64_t function);
int xlsSemanticCheckIrDependency(const xls_semantic_check* result,uint64_t function,size_t at,xls_check_ir_dependency* out);
#endif
