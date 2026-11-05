#ifndef XL6_TASK_SUBMIT_CLOSURE_H
#define XL6_TASK_SUBMIT_CLOSURE_H
#include "xl6_checked_result_closure.h"
#include "xl6_future_value.h"
struct XL6_TaskPool;
struct xcancel;
struct xfuture;
/* Exact compiler-verified result profile and callback ABI. On success owns the
 * independent Future; callback/adapter are borrowed for this call only. No
 * default-pool redirection, generated result destructor or copied T result.
 * False/NULL resource failures use the ordinary outer checked error scope. */
XL6_CODE_LEASE_API struct xfuture* xl6TaskSubmitClosureV1(struct XL6_TaskPool*,
    XL6_FutureValueProfileV1,XL6_CheckedResultClosureBorrowV1,struct xcancel*);
#endif
