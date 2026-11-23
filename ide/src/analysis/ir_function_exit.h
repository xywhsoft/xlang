#ifndef XLS_IR_FUNCTION_EXIT_H
#define XLS_IR_FUNCTION_EXIT_H
#include "analysis/frontend_ir_memo.h"
typedef struct xls_ir_function_exit_work {
 size_t recorded,reused,fresh,nativeNodes,moduleNodes,fallbackNodes,bytes;
 int attempted,assembled;
} xls_ir_function_exit_work;
/* Experimental construction-bound cache. Counters describe this production,
 * not total raw IR/semantic/verification work or an end-to-end speed claim. */
xls_ir_function_exit_work xlsIrFunctionExitWork(const xls_ir_memo*);
/* Synchronous owner-thread test/resource control; retained cache capacities
 * only, <=8MiB. Zero disables it; transient native outputs/scratch excluded. */
void xlsIrFunctionExitLimit(size_t);
int xlsIrFunctionExitAvailable(void);
#endif
