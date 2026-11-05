#ifndef XL6_COLLECTION_COUNT_H
#define XL6_COLLECTION_COUNT_H
#include <stdbool.h>
#include <stdint.h>
typedef struct xvalue xvalue;

/* A leaf generated-code ABI: do not pull the native storage, XRT declarations
 * or implementation-only types into otherwise self-contained modules. */
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_COLLECTION_COUNT_API __declspec(dllexport)
#else
#define XL6_COLLECTION_COUNT_API
#endif

/* Compiler-private collection length. Wire Array/List/Dict/Set and sealed,
 * projection-bound native ARRAY are accepted; other handles are not containers.
 * Native access uses the actual owner's BUSY pin and never projects any slot.
 * Failure is exceptional (XRT error); ordinary Count returns zero on failure.
 * IfPresent returns false for absence OR failure; the caller's error edge
 * distinguishes them. It only writes the initialized expression result on
 * success. These output slots are not language byref parameters.
 * OrNull returns one owned null/integer, or NULL on failure. */
XL6_COLLECTION_COUNT_API int64_t xl6CollectionCountV1(const xvalue*);
XL6_COLLECTION_COUNT_API bool xl6CollectionCountIfPresentV1(const xvalue*, int64_t*);
XL6_COLLECTION_COUNT_API xvalue* xl6CollectionCountOrNullV1(const xvalue*);
#endif
