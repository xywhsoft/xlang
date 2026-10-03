#ifndef XL6_STACK_H
#define XL6_STACK_H
#include "src/runtime/xl6_text_value.h"

/* Compiler-private physical ABI. The immutable tuple and type descriptor
 * come from verified XLIR, not caller-controlled language pointers. Output
 * slots are initialized values; this does not define a public byref API. */
XL6_TEXT_API xvalue* xl6StackCreateV1(uint64_t, uint64_t, unsigned int, size_t, const xrttype*);
XL6_TEXT_API bool xl6StackPushV1(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*, const void*);
XL6_TEXT_API bool xl6StackPopOrPeekV1(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*, void*, bool);
XL6_TEXT_API bool xl6StackPeekAtV1(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*, int64_t, void*);
XL6_TEXT_API bool xl6StackReserveV1(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*, int64_t);
XL6_TEXT_API xvalue* xl6StackCloneV1(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*);
XL6_TEXT_API bool xl6StackEqualsV1(const xvalue*, const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*);
XL6_TEXT_API bool xl6StackTrimV1(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*);
XL6_TEXT_API void xl6StackClearV1(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*);
XL6_TEXT_API int64_t xl6StackSizeV1(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*, bool);
/* Resident certificate for BOTH the Value shell and its actual counted
 * wrapper. Caller holds an ownership freeze; unknown children stay refused. */
XL6_TEXT_API const xrtownershipadapterv1* xl6StackOwnershipAdapterV1(xrtownershipref);
#endif
