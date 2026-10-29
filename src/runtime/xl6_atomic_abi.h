#ifndef XL6_ATOMIC_ABI_H
#define XL6_ATOMIC_ABI_H
#include "xl6_product_abi.h"
/* Resident atomic values use checked, scalar-only contracts. */
XL6_TEXT_API void* xl6AtomicInt32CreateV1(int64_t initial);
XL6_TEXT_API void xl6AtomicInt32DestroyV1(void* raw);
XL6_TEXT_API int64_t xl6AtomicInt32LoadV1(void* raw, int64_t order);
XL6_TEXT_API void xl6AtomicInt32StoreV1(void* raw, int64_t value, int64_t order);
XL6_TEXT_API int64_t xl6AtomicInt32ExchangeV1(void* raw, int64_t value, int64_t order);
XL6_TEXT_API XL6_ProductBoolI32V1 xl6AtomicInt32CompareExchangeV1(void* raw, int64_t expected, int64_t desired, int64_t successOrder, int64_t failureOrder);
XL6_TEXT_API int64_t xl6AtomicInt32FetchAddV1(void* raw, int64_t value, int64_t order);
XL6_TEXT_API int64_t xl6AtomicInt32FetchSubV1(void* raw, int64_t value, int64_t order);
XL6_TEXT_API int64_t xl6AtomicInt32FetchAndV1(void* raw, int64_t value, int64_t order);
XL6_TEXT_API int64_t xl6AtomicInt32FetchOrV1(void* raw, int64_t value, int64_t order);
XL6_TEXT_API int64_t xl6AtomicInt32FetchXorV1(void* raw, int64_t value, int64_t order);
XL6_TEXT_API void* xl6AtomicUInt32CreateV1(int64_t initial);
XL6_TEXT_API void xl6AtomicUInt32DestroyV1(void* raw);
XL6_TEXT_API int64_t xl6AtomicUInt32LoadV1(void* raw, int64_t order);
XL6_TEXT_API void xl6AtomicUInt32StoreV1(void* raw, int64_t value, int64_t order);
XL6_TEXT_API int64_t xl6AtomicUInt32ExchangeV1(void* raw, int64_t value, int64_t order);
XL6_TEXT_API XL6_ProductBoolU32V1 xl6AtomicUInt32CompareExchangeV1(void* raw, int64_t expected, int64_t desired, int64_t successOrder, int64_t failureOrder);
XL6_TEXT_API int64_t xl6AtomicUInt32FetchAddV1(void* raw, int64_t value, int64_t order);
XL6_TEXT_API int64_t xl6AtomicUInt32FetchSubV1(void* raw, int64_t value, int64_t order);
XL6_TEXT_API int64_t xl6AtomicUInt32FetchAndV1(void* raw, int64_t value, int64_t order);
XL6_TEXT_API int64_t xl6AtomicUInt32FetchOrV1(void* raw, int64_t value, int64_t order);
XL6_TEXT_API int64_t xl6AtomicUInt32FetchXorV1(void* raw, int64_t value, int64_t order);
XL6_TEXT_API void* xl6AtomicInt64CreateV1(int64_t initial);
XL6_TEXT_API void xl6AtomicInt64DestroyV1(void* raw);
XL6_TEXT_API int64_t xl6AtomicInt64LoadV1(void* raw, int64_t order);
XL6_TEXT_API void xl6AtomicInt64StoreV1(void* raw, int64_t value, int64_t order);
XL6_TEXT_API int64_t xl6AtomicInt64ExchangeV1(void* raw, int64_t value, int64_t order);
XL6_TEXT_API XL6_ProductBoolI64V1 xl6AtomicInt64CompareExchangeV1(void* raw, int64_t expected, int64_t desired, int64_t successOrder, int64_t failureOrder);
XL6_TEXT_API int64_t xl6AtomicInt64FetchAddV1(void* raw, int64_t value, int64_t order);
XL6_TEXT_API int64_t xl6AtomicInt64FetchSubV1(void* raw, int64_t value, int64_t order);
XL6_TEXT_API int64_t xl6AtomicInt64FetchAndV1(void* raw, int64_t value, int64_t order);
XL6_TEXT_API int64_t xl6AtomicInt64FetchOrV1(void* raw, int64_t value, int64_t order);
XL6_TEXT_API int64_t xl6AtomicInt64FetchXorV1(void* raw, int64_t value, int64_t order);
XL6_TEXT_API void* xl6AtomicUInt64CreateV1(uint64_t initial);
XL6_TEXT_API void xl6AtomicUInt64DestroyV1(void* raw);
XL6_TEXT_API uint64_t xl6AtomicUInt64LoadV1(void* raw, int64_t order);
XL6_TEXT_API void xl6AtomicUInt64StoreV1(void* raw, uint64_t value, int64_t order);
XL6_TEXT_API uint64_t xl6AtomicUInt64ExchangeV1(void* raw, uint64_t value, int64_t order);
XL6_TEXT_API XL6_ProductBoolU64V1 xl6AtomicUInt64CompareExchangeV1(void* raw, uint64_t expected, uint64_t desired, int64_t successOrder, int64_t failureOrder);
XL6_TEXT_API uint64_t xl6AtomicUInt64FetchAddV1(void* raw, uint64_t value, int64_t order);
XL6_TEXT_API uint64_t xl6AtomicUInt64FetchSubV1(void* raw, uint64_t value, int64_t order);
XL6_TEXT_API uint64_t xl6AtomicUInt64FetchAndV1(void* raw, uint64_t value, int64_t order);
XL6_TEXT_API uint64_t xl6AtomicUInt64FetchOrV1(void* raw, uint64_t value, int64_t order);
XL6_TEXT_API uint64_t xl6AtomicUInt64FetchXorV1(void* raw, uint64_t value, int64_t order);
XL6_TEXT_API void* xl6AtomicBoolCreateV1(bool initial);
XL6_TEXT_API void xl6AtomicBoolDestroyV1(void* raw);
XL6_TEXT_API bool xl6AtomicBoolLoadV1(void* raw, int64_t order);
XL6_TEXT_API void xl6AtomicBoolStoreV1(void* raw, bool value, int64_t order);
XL6_TEXT_API bool xl6AtomicBoolExchangeV1(void* raw, bool value, int64_t order);
XL6_TEXT_API XL6_ProductBoolBoolV1 xl6AtomicBoolCompareExchangeV1(void* raw, bool expected, bool desired, int64_t successOrder, int64_t failureOrder);
#endif
