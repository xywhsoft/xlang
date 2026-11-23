#ifndef XLS_IR_NATIVE_PIPELINE_H
#define XLS_IR_NATIVE_PIPELINE_H
#include "src/ir/xl6_ir.h"

typedef struct xls_ir_native_pipeline xls_ir_native_pipeline;
typedef enum xls_ir_pipeline_state {
 XLS_PIPELINE_EMPTY,XLS_PIPELINE_PREPARED,XLS_PIPELINE_READY,
 XLS_PIPELINE_FAILED,XLS_PIPELINE_MOVED
} xls_ir_pipeline_state;
typedef enum xls_ir_pipeline_status {
 XLS_PIPELINE_OK,XLS_PIPELINE_INPUT,XLS_PIPELINE_MEMORY,
 XLS_PIPELINE_CANCELLED,XLS_PIPELINE_NATIVE_ERROR,XLS_PIPELINE_UNAVAILABLE
} xls_ir_pipeline_status;

/* Native production component,not a cache or semantic admission proof.
 * The reviewed source cohort and XLS_NATIVE_IR_PIPELINE_COMPONENT=1 are
 * required. Ordinary service builds retain their original producer.
 * Single owner;calls may interleave independently owned builders on a thread.
 * Create allocates only the reusable XRT-owned state. */
int xlsIrNativePipelineAvailable(void);
xls_ir_native_pipeline* xlsIrNativePipelineCreate(xls_ir_pipeline_status*);
/* Requires an accepted immutable typed AST and an empty diagnostics list.
 * AST is borrowed until Ready/Failed/Reset/Destroy. No AST/check/previous
 * result is retained or owned. Prepare invokes the actual native prelude,
 * stopping before all final module passes. The raw module is not validated
 * final IR. Only Empty/Moved/Failed accepts a new input;Reset is explicit.
 * lookupBytes limits the optional type/alias indexes (max8MiB),not native
 * output,scratch,allocator overhead or RSS. Raw memo/payload borrowing is
 * disabled inside this independent producer;outer TLS state is restored. */
int xlsIrNativePipelinePrepare(xls_ir_native_pipeline*,const XL6_AstNode*,
 XL6_DiagList*,size_t lookupBytes,xls_ir_pipeline_status*);
/* Advance executes one original phase;the final step computes statistics
 * and runs the complete native verifier. Finish executes all remaining
 * phases inside one common type/alias/resource scope. No result can be
 * taken before Ready. Failure/cancellation retires the partial native IR.
 * Ready calls are idempotent;invalid arguments do not discard prior state. */
int xlsIrNativePipelineAdvance(xls_ir_native_pipeline*,XL6_DiagList*,xls_ir_pipeline_status*);
int xlsIrNativePipelineFinish(xls_ir_native_pipeline*,XL6_DiagList*,xls_ir_pipeline_status*);
/* For reviewed single-scan phases,produce only [NodeCursor,end) in native
 * node order. end must exceed the cursor and not exceed nodeCount. The
 * phase advances only after its last node and mandatory phase finalizer.
 * Advance/Finish may complete the remaining suffix. No input is skipped,
 * and incomplete phases cannot produce a Ready result. Other phases and
 * invalid ranges return INPUT without discarding state. */
int xlsIrNativePipelinePhaseHasNodeRanges(size_t phase);
size_t xlsIrNativePipelineNodeCursor(const xls_ir_native_pipeline*);
int xlsIrNativePipelineAdvanceNodes(xls_ir_native_pipeline*,size_t end,
 XL6_DiagList*,xls_ir_pipeline_status*);
/* Read-only borrow,valid until the next mutating call. Empty/Failed/Moved
 * returns NULL. Native APIs requiring a fully verified module may be used
 * only at Ready. Never free this borrow or modify its arrays/payloads. */
const XL6_IrModule* xlsIrNativePipelineModule(const xls_ir_native_pipeline*);
xls_ir_pipeline_state xlsIrNativePipelineState(const xls_ir_native_pipeline*);
size_t xlsIrNativePipelinePhase(const xls_ir_native_pipeline*);
size_t xlsIrNativePipelinePhaseCount(void);
const char* xlsIrNativePipelinePhaseName(size_t);
/* Move the completed native module descriptor and its actual allocations;
 * no node/table/payload copy. Target is a valid independently owned native
 * module (including an initialized empty module);its old content is freed.
 * Do not pass graph images or borrowed result modules. */
int xlsIrNativePipelineTake(xls_ir_native_pipeline*,XL6_IrModule*);
void xlsIrNativePipelineReset(xls_ir_native_pipeline*);
void xlsIrNativePipelineDestroy(xls_ir_native_pipeline*);
#endif
