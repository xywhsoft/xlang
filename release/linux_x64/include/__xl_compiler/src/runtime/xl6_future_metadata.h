#ifndef XL6_FUTURE_METADATA_H
#define XL6_FUTURE_METADATA_H
#if !defined(XRT_SINGLE_HEADER) && !defined(XRUNTIME_DECLARATIONS_H)
#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#endif
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_FUTURE_METADATA_API __declspec(dllexport)
#else
#define XL6_FUTURE_METADATA_API
#endif
/* Direct, fallible observation ABI. No side table, Promise reference, callback
 * or generated-module policy is retained. Name input is an exact synchronous
 * borrow; returned string owns a separate XRT allocation. Embedded NUL is data.
 * Times preserve the native first-observation contract, expressed in ms.
 * Pending counts registered public completion Watches, not blocking waiters. */
XL6_FUTURE_METADATA_API bool xl6FutureDebugNameSetV2(xfuture*, xstrview);
XL6_FUTURE_METADATA_API xstrview xl6FutureDebugNameCopyV2(xfuture*);
XL6_FUTURE_METADATA_API int64_t xl6FutureFirstObservedMsV2(xfuture*);
XL6_FUTURE_METADATA_API int64_t xl6FutureFirstTerminalObservedMsV2(xfuture*);
XL6_FUTURE_METADATA_API int64_t xl6FuturePendingWatchesV2(xfuture*);
#endif
