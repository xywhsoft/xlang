#ifndef XLS_IR_NATIVE_CALL_CACHE_H
#define XLS_IR_NATIVE_CALL_CACHE_H
#include "analysis/ir_native_pipeline.h"
typedef struct xls_ir_call_cache xls_ir_call_cache;
typedef struct xls_ir_call_cache_work {
 uint64_t checked,reused,captured,declined,nativeNodes,restoredSites,restoredArguments;
 size_t bytes;
} xls_ir_call_cache_work;
/* Reviewed native call-table phase only. Previous is an immutable call borrow;
 * the new cache owns its context and independent immutable units,never an
 * AST,module,old cache or result chain. Calls require exclusive pipeline use.
 * Ready is idempotent and captures nothing. Start no later than phase26;
 * that phase must not already be partial. Other partial phases can finish.
 * *out must be NULL. Only a completed,fully verified pipeline publishes it.
 * limit<=16MiB bounds requested retained cache bytes,including shared units;
 * phase scratch is separately bounded by the same limit,not output/RSS.
 * Zero disables reuse/capture. Optional metadata failure uses native production;
 * required native output allocation failure/cancellation retires the pipeline.
 * Counts describe ordered declaration ranges,not complete final-function IR.
 * Exact native IDs are required;structural/type/context changes may miss. */
int xlsIrNativePipelineFinishCachedCalls(xls_ir_native_pipeline*,XL6_DiagList*,
 const xls_ir_call_cache* previous,size_t limit,xls_ir_call_cache** out,
 xls_ir_call_cache_work*,xls_ir_pipeline_status*);
size_t xlsIrCallCacheBytes(const xls_ir_call_cache*);
void xlsIrCallCacheDestroy(xls_ir_call_cache*);
#endif
