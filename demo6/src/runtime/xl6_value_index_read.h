#ifndef XL6_VALUE_INDEX_READ_H
#define XL6_VALUE_INDEX_READ_H
typedef struct xvalue xvalue;
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_VALUE_INDEX_READ_API __declspec(dllexport)
#else
#define XL6_VALUE_INDEX_READ_API
#endif
/* Open dynamic index: borrow receiver/key, return one owned Value. Integer
 * keys select authenticated Array/List; strings select exact-length objects.
 * Unsupported combinations and absent items return canonical null. Recognized
 * native BUSY/domain/cursor/retirement/projection failures never become misses.
 * Dispatch is resident, not emitted algorithms or speculative HANDLE casts. */
XL6_VALUE_INDEX_READ_API xvalue* xl6ValueIndexReadCopyV1(const xvalue*,const xvalue*);
#endif
