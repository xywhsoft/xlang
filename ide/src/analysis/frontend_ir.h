#ifndef XLANG_ANALYSIS_FRONTEND_IR_H
#define XLANG_ANALYSIS_FRONTEND_IR_H
#include "src/ir/xl6_ir.h"
/* Private attribution/reference boundary. The host has no mode switch and
 * links only the indexed shared compiler entry points. Counters are per
 * calling thread, not protocol-wide or a semantic-incrementality claim. */
typedef struct xls_ir_scan_work {
    uint64_t indexes,candidates,queries,indexedVisits,linearVisits,peakBytes;
} xls_ir_scan_work;
void xlsFrontendIrScanReset(void);
xls_ir_scan_work xlsFrontendIrScanWork(void);
int xlsFrontendIrReferenceRefresh(XL6_IrModule* module);
int xlsFrontendIrReferenceVerify(const XL6_IrModule* module,XL6_DiagList* diags);
#endif
