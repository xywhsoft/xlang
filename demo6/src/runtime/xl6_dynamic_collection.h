#ifndef XL6_DYNAMIC_COLLECTION_H
#define XL6_DYNAMIC_COLLECTION_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
typedef struct xvalue xvalue;
typedef enum XL6_DynamicCollectionOpV1 {
    XL6_DYNAMIC_COLLECTION_PUSH=1,
    XL6_DYNAMIC_COLLECTION_INSERT,
    XL6_DYNAMIC_COLLECTION_REMOVE,
    XL6_DYNAMIC_COLLECTION_REMOVE_VALUE,
    XL6_DYNAMIC_COLLECTION_POP,
    XL6_DYNAMIC_COLLECTION_TAKE,
    XL6_DYNAMIC_COLLECTION_CLEAR,
    XL6_DYNAMIC_COLLECTION_RESERVE,
    XL6_DYNAMIC_COLLECTION_SWAP,
    XL6_DYNAMIC_COLLECTION_ADD,
    XL6_DYNAMIC_COLLECTION_CONTAINS,
    XL6_DYNAMIC_COLLECTION_INDEX_OF
} XL6_DynamicCollectionOpV1;
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_DYNAMIC_COLLECTION_API __declspec(dllexport)
#else
#define XL6_DYNAMIC_COLLECTION_API
#endif
/* Borrowed receiver, BOTH arguments consumed on every path (including invalid
 * arity/opcode/receiver). Owned result; NULL is failure, never null data.
 * Native Array dispatch uses the sealed Sequence protocol, not payload casts.
 * Missing map keys return null/false, empty Array pop and bad bounds raise.
 * Destructive operations may commit before a destructor reports failure. */
XL6_DYNAMIC_COLLECTION_API xvalue* xl6DynamicCollectionCallTakeV1(xvalue*,unsigned,xvalue*,xvalue*);
/* Allocation/error transparent dispatcher preflight. Keywords are rejected
 * by the caller before passing positional owned values to CallTake. */
XL6_DYNAMIC_COLLECTION_API bool xl6DynamicCollectionArityV1(unsigned,size_t);
#endif
