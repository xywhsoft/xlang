#ifndef XL6_STACK_H
#define XL6_STACK_H
#include "xl6_text_value.h"

/* Compiler-private physical ABI. The immutable tuple and type descriptor
 * come from verified XLIR, not caller-controlled language pointers. Output
 * slots are initialized values; this does not define a public byref API. */
XL6_TEXT_API xvalue* xl6StackCreateV1(uint64_t, uint64_t, unsigned int, size_t, const xrttype*);
XL6_TEXT_API bool xl6StackPushV1(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*, const void*);
/* Output is one empty initialized SSA owner of the exact payload. Failure
 * preserves it and sets the thread error; success owns the moved/copied item. */
XL6_TEXT_API bool xl6StackTakeV1(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*, int64_t, void*, bool);
/* Return value is operation success, never element presence. A successful
 * missing depth writes found=false and preserves target; a successful item
 * writes found=true. Any failure writes found=false and preserves target.
 * Both outputs are private compiler slots, not language byref arguments. */
XL6_TEXT_API bool xl6StackTryTakeV2(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*, int64_t, void*, bool, bool*);
XL6_TEXT_API bool xl6StackReserveV1(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*, int64_t);
XL6_TEXT_API xvalue* xl6StackCloneV1(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*);
XL6_TEXT_API bool xl6StackEqualsV1(const xvalue*, const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*);
XL6_TEXT_API bool xl6StackTrimV1(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*);
/* Explicit may-raise operation: committed emptying finishes every Drop and
 * preserves the first error. This is not an implicit destructor transaction. */
XL6_TEXT_API bool xl6StackClearV1(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*);
XL6_TEXT_API int64_t xl6StackSizeV1(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*, bool);
#endif
