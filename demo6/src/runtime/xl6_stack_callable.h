#ifndef XL6_STACK_CALLABLE_H
#define XL6_STACK_CALLABLE_H
#include "xl6_stack.h"
#include "xl6_closure.h"
/* Compiler-private closure codec. Borrows a signature-admitted boxed slot;
 * constructs its independent closure and code lease before committing a pop.
 * The entry/owner are used by the returned closure, never by Stack metadata.
 * Output is an empty, aligned, disjoint SSA owner. NULL found requests a
 * required item (missing becomes RANGE); otherwise missing succeeds with
 * found=false, independently of a successful nullable function value. */
XL6_TEXT_API bool xl6StackCallableTakeV1(const xvalue*,uint64_t,uint64_t,uint64_t,bool,
    int64_t,bool,const XL6_CodeOwner*,void (*)(void),XL6_Closure**,bool*);
#endif
