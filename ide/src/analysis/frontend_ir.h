#ifndef XLANG_ANALYSIS_FRONTEND_IR_H
#define XLANG_ANALYSIS_FRONTEND_IR_H
#include "src/ir/xl6_ir.h"
/* Private attribution/reference boundary. The host has no mode switch and
 * links only the indexed shared compiler entry points. Counters are per
 * calling thread, not protocol-wide or a semantic-incrementality claim. */
typedef struct xls_ir_scan_work {
    uint64_t indexes,candidates,queries,indexedVisits,linearVisits,peakBytes;
} xls_ir_scan_work;
/* One immutable AST build scope. A bounded ordered candidate vector omits
 * non-nominal declarations; the original lookup still matches identity/name.
 * Bytes count the owned pointer vector, not borrowed AST or process RSS. */
typedef struct xls_ir_type_lookup_work {
    uint64_t indexes,rootVisits,declarations,queries,indexedQueries,linearQueries,peakBytes;
} xls_ir_type_lookup_work;
void xlsFrontendIrTypeLookupReset(void);
xls_ir_type_lookup_work xlsFrontendIrTypeLookupWork(void);
int xlsFrontendIrBuildTypeBudget(XL6_IrModule* module,const XL6_AstNode* root,XL6_DiagList* diags,size_t bytes);
void xlsFrontendIrScanReset(void);
xls_ir_scan_work xlsFrontendIrScanWork(void);
typedef struct xls_ir_plan_scan_work {
    uint64_t indexes,records,queries,indexedVisits,linearVisits,peakBytes;
} xls_ir_plan_scan_work;
void xlsFrontendIrPlanScanReset(void);
xls_ir_plan_scan_work xlsFrontendIrPlanScanWork(void);
int xlsFrontendIrVerifyPlanBudget(const XL6_IrModule* module,XL6_DiagList* diags,size_t bytes);
typedef struct xls_ir_scope_proof_work { uint64_t requests,checks,hits,indexes,peakBytes; } xls_ir_scope_proof_work;
void xlsFrontendIrScopeProofReset(void);
xls_ir_scope_proof_work xlsFrontendIrScopeProofWork(void);
typedef struct xls_ir_call_scan_work {
    uint64_t indexes,records,queries,indexedVisits,linearVisits,peakBytes;
} xls_ir_call_scan_work;
void xlsFrontendIrCallScanReset(void);
xls_ir_call_scan_work xlsFrontendIrCallScanWork(void);
int xlsFrontendIrConversionBudget(XL6_IrModule* module,size_t bytes);
int xlsFrontendIrReferenceConversions(XL6_IrModule* module);
int xlsFrontendIrReferenceRefresh(XL6_IrModule* module);
int xlsFrontendIrReferenceVerify(const XL6_IrModule* module,XL6_DiagList* diags);
#endif
