#ifndef XL6_TASK_POOL_EXIT_H
#define XL6_TASK_POOL_EXIT_H
#include "xl6_task.h"
#include "xl6_execution_failures.h"
struct xtaskpoolworkerexitownershipv1;
/* Managed factory retains its own real ledger ref. No worker is created until
 * Start/Submit. Close then Wait includes actual workers/TLS, unlike raw pools. */
XL6_CODE_LEASE_API XL6_TaskPool* xl6TaskPoolCreateManagedV1(const XL6_TaskPoolConfig*,XL6_ExecutionFailuresV1*);
XL6_CODE_LEASE_API const struct xtaskpoolworkerexitownershipv1* xl6TaskPoolWorkerExitPolicyV1Get(void);
#endif
