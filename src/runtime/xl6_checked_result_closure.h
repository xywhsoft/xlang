#ifndef XL6_CHECKED_RESULT_CLOSURE_H
#define XL6_CHECKED_RESULT_CLOSURE_H
#include "xl6_closure.h"
/* Call-only borrow of an exact checked, nullary callback. Invoke has a real
 * bool(Closure*,void*) prototype: its verified representation thunk casts the
 * output address to T* and invokes the exact bool(environment,T*) source ABI.
 * It owns no input and is not an erased function/signature certificate. Native
 * producers must acquire adapterOwner before retaining Invoke, and separately
 * retain the original closure; those code owners need not be the same module.
 * Output is initialized typed-empty and owns any partial result on failure. */
typedef struct XL6_CheckedResultClosureBorrowV1 {
    XL6_Closure* closure;
    bool (*Invoke)(XL6_Closure*,void*);
    const XL6_CodeOwner* adapterOwner;
} XL6_CheckedResultClosureBorrowV1;
#endif
