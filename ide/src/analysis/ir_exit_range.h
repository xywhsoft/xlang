#ifndef XLS_IR_EXIT_RANGE_H
#define XLS_IR_EXIT_RANGE_H
#include "analysis/frontend_ir.h"

typedef struct xls_ir_exit_range xls_ir_exit_range;
typedef enum xls_ir_exit_range_status {
 XLS_EXIT_RANGE_OK, XLS_EXIT_RANGE_INPUT, XLS_EXIT_RANGE_UNAVAILABLE,
 XLS_EXIT_RANGE_MEMORY, XLS_EXIT_RANGE_LIMIT, XLS_EXIT_RANGE_CANCELLED,
 XLS_EXIT_RANGE_PRODUCER
} xls_ir_exit_range_status;
typedef struct xls_ir_exit_range_work {
 size_t closeNodes, planNodes, routePlans;
 int cancelled, limited;
} xls_ir_exit_range_work;
typedef struct xls_ir_exit_range_view {
 size_t first, count, bytes;
 const XL6_IrGeneratorClosePointAbi* close;
 const XL6_IrExitPlan* plans; size_t planCount;
 const XL6_IrCleanupAction* cleanups; size_t cleanupCount;
 const XL6_IrExitStep* steps; size_t stepCount;
} xls_ir_exit_range_view;

/* Fresh native production for [first,first+count), using zero-based node
 * offsets. Requires a valid, immutable, typed pre-lowering native module.
 * Other nodes/tables remain available to original dependency lookups, but
 * only selected nodes enter close-point/plan production and only their
 * plans enter route production. No prior result or eligibility assertion.
 * Input is borrowed for this call only; success owns plain XRT arrays.
 * Native IDs stay in the input namespace; slice offsets are result-local.
 * retainedBytes limits retained capacities, not transient native grow peaks
 * or allocator overhead. Polls occur between selected nodes/plans; one native
 * helper is not preemptible. Failure publishes nothing and never edits input. */
xls_ir_exit_range* xlsIrExitRangeProduce(const XL6_IrModule*, size_t first,
 size_t count, size_t retainedBytes, int(*cancelled)(void*), void*,
 xls_ir_exit_range_status*, xls_ir_exit_range_work*);
int xlsIrExitRangeAvailable(void);
xls_ir_exit_range_view xlsIrExitRangeView(const xls_ir_exit_range*);
void xlsIrExitRangeDestroy(xls_ir_exit_range*);
#endif
