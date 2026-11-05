#ifndef XL6_STACK_ERASED_METHODS_H
#define XL6_STACK_ERASED_METHODS_H
#include "xl6_sequence.h"
typedef enum XL6_StackErasedMethodV1 {
    XL6_STACK_ERASED_PUSH_BATCH=1,
    XL6_STACK_ERASED_POP_BATCH,
    XL6_STACK_ERASED_PEEK_BATCH,
    XL6_STACK_ERASED_PEEK_BATCH_AT,
    XL6_STACK_ERASED_EQUALS
} XL6_StackErasedMethodV1;
/* Borrowed inputs; one owned result. Complete identities/admission metadata
 * are resident, not caller-invented layout matches. Failed preparation leaves
 * source contents unchanged. Committed mutation may report a Drop failure. */
XL6_TEXT_API xvalue* xl6StackErasedMethodV1(const xvalue*,unsigned,const xvalue*,const xvalue*);
#endif
