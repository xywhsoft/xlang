#ifndef XL6_TASK_RESULT_CLOSURE_H
#define XL6_TASK_RESULT_CLOSURE_H
#include "xl6_checked_result_closure.h"
#include "xl6_future_value.h"
/* Private shared frame with one caller owner. Acquires the adapter's code
 * lease before retaining Invoke and independently retains the original
 * closure. No computation-pool dependency. Submission rejection leaves the
 * caller responsible for discarding this private frame. */
XL6_CODE_LEASE_API void* xl6TaskResultClosurePrepareV1(
    XL6_FutureValueProfileV1,XL6_CheckedResultClosureBorrowV1);
#endif
