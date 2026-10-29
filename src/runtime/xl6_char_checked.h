#ifndef XL6_CHAR_CHECKED_H
#define XL6_CHAR_CHECKED_H
#include <stdint.h>
#include <stdbool.h>
typedef struct xvalue xvalue;
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_CHAR_CHECKED_API __declspec(dllexport)
#else
#define XL6_CHAR_CHECKED_API
#endif
/* Borrowed source, trivial output. Success is separate from scalar value:
 * U+0000 is valid. Failure leaves output unchanged, including allocation-free
 * failed diagnostics. expectedCharId is the compiler's canonical char ID.
 * strict=true asserts CHAR tag/identity; false converts CHAR/INT/UINT only.
 * Integer conversion rejects negatives, surrogates and values above U+10FFFF;
 * bool/float/string/null are TypeError, not numeric/text coercions. */
XL6_CHAR_CHECKED_API bool xl6CharValueCheckedV1(const xvalue*,uint64_t,bool,uint32_t*);
#endif
