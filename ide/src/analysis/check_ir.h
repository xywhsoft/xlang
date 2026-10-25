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
#endif
