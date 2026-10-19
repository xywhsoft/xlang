#ifndef XL6_EXECUTION_WORKER_EXIT_H
#define XL6_EXECUTION_WORKER_EXIT_H
#include "xl6_execution_failures.h"
/* Resident callbacks for typed native worker-exit descriptors. Shared ledger
 * is borrowed by each REAL worker; Drop consumes its single pool-owned ref. */
XL6_CODE_LEASE_API void xl6ExecutionWorkerExitV1(void*);
XL6_CODE_LEASE_API void xl6ExecutionWorkerExitDropV1(void*,void*);
#endif
