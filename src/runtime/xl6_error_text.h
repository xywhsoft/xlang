#ifndef XL6_ERROR_TEXT_H
#define XL6_ERROR_TEXT_H
#include "xl6_text_value.h"

/* Kind is a terminated language error identifier. Text is an exact borrowed
 * span, copied synchronously by immutable XRT Error; no input is retained.
 * Returns an owned error, including the actual construction failure error. */
XL6_TEXT_API xerror* xl6ErrorCreateV1(const char* kind, xstrview text, int32_t line, int32_t column);
/* Exact owned string copies; {NULL,0} on failure, current error preserved.
 * NULL Error observes an empty text, matching the native accessor contract. */
XL6_TEXT_API xstrview xl6ErrorMessageCopyV1(const xerror*);
XL6_TEXT_API xstrview xl6ErrorDomainCopyV1(const xerror*);
XL6_TEXT_API xstrview xl6ErrorOperationCopyV1(const xerror*);
XL6_TEXT_API xstrview xl6ErrorDataCopyV1(const xerror*);
XL6_TEXT_API xstrview xl6ErrorFileCopyV1(const xerror*);
XL6_TEXT_API xstrview xl6ErrorKindCopyV1(const xerror*);
/* Borrowed stable terminated identifier; these observations never allocate. */
XL6_TEXT_API const char* xl6ErrorKindNameV1(const xerror*);
XL6_TEXT_API bool xl6ErrorKindIsV1(const xerror*, const char* expected);
/* Consume the current error into an AsyncError cause, preserving every text
 * byte. On construction failure, retain that failure, never a success/null. */
XL6_TEXT_API void xl6ErrorWrapAsyncV1(const char* fallback);
#endif
