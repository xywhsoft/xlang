#ifndef XL6_CALLBACK_FAILURE_H
#define XL6_CALLBACK_FAILURE_H
#include <stdbool.h>
#include <stdint.h>
typedef struct xerror xerror;
/* Shared two-field error carrier; identical to xl_foreign_error.h. Keep this
 * host channel usable from an independently staged private runtime SDK. */
#ifndef XL6_ERROR_RESULT_ABI_DEFINED
#define XL6_ERROR_RESULT_ABI_DEFINED
typedef struct xl6_foreign_error_result {bool ok;xerror* error;} xl6_foreign_error_result;
typedef xl6_foreign_error_result xl6_error_result;
#endif
#ifndef XL6_FOREIGN_API
#if defined(_WIN32) && (defined(XL_BUILD_DLL) || defined(XL6_BUILD_SHARED))
#define XL6_FOREIGN_API __declspec(dllexport)
#elif defined(__GNUC__) && !defined(_WIN32)
#define XL6_FOREIGN_API __attribute__((visibility("default")))
#else
#define XL6_FOREIGN_API
#endif
#endif
#ifdef __cplusplus
extern "C" {
#endif

/* Resident fallback for a verified native callback without a resource owner
 * or an enclosing language error scope. Consumes a FAILED result, including
 * {false,NULL} under OOM. Successful callbacks do not call this interface.
 * The first error is retained by transfer; later errors are released outside
 * the mailbox lock. Counters saturate rather than wrap. */
XL6_FOREIGN_API void xl6CallbackFailureRecordV1(xl6_error_result);
/* Checked host channel. A successful Take atomically detaches the current
 * batch. output.ok distinguishes no failure from failure without a diagnostic;
 * output.error is owned by the caller. count is the number in that batch.
 * False means invalid output storage, with both outputs unchanged. Take does
 * not raise, poll callbacks or execute generated code. */
XL6_FOREIGN_API bool xl6CallbackFailureTakeV1(xl6_error_result* output,uint64_t* count);
/* Consistent snapshot of pending and lifetime totals; does not acknowledge.
 * Two distinct aligned output slots are required, unchanged on failure. */
XL6_FOREIGN_API bool xl6CallbackFailureCountV1(uint64_t* pending,uint64_t* total);
#ifdef __cplusplus
}
#endif
#endif
