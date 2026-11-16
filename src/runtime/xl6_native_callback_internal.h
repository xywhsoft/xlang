#ifndef XL6_NATIVE_CALLBACK_INTERNAL_H
#define XL6_NATIVE_CALLBACK_INTERNAL_H
#include "xl6_native_callback.h"
/* Internal cross-component construction from an already accepted code lease.
 * The caller retains its lease through this call; the record acquires its own
 * reference. NULL explicitly denotes externally resident adapter code.
 * Hide this implementation symbol from ELF export-dynamic roots so ordinary
 * callbacks do not retain this optional constructor when groups are unused.
 * The runtime symbol visitor still supplies its address to embedded TCC. */
#if defined(__GNUC__) && !defined(_WIN32)
__attribute__((visibility("hidden")))
#endif
XL6_NativeCallback* xl6NativeCallbackCreateSharedAdapterV1(XL6_CodeLease*,
    XL6_Closure*,XL6_ForeignView*,uint64_t);
#endif
