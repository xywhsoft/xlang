#ifndef XL6_HOST_XRT_H
#define XL6_HOST_XRT_H
#ifndef XL6_HOST_XRT_API
#if defined(_WIN32) && (defined(XL_BUILD_DLL) || defined(XL6_BUILD_SHARED))
#define XL6_HOST_XRT_API __declspec(dllexport)
#elif defined(__GNUC__) && !defined(_WIN32)
#define XL6_HOST_XRT_API __attribute__((visibility("default")))
#else
#define XL6_HOST_XRT_API
#endif
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef void (*XL6_HostXrtFunctionV1)(void);
/* Cold immutable lookup in this actual host's native implementation catalog.
 * A TinyCC module can have a different import thunk for the same function;
 * invoking this resident resolver returns the real, stable implementation
 * address instead. Names are exact terminated C identifiers. NULL/unknown
 * names return NULL without allocation, initialization or diagnostic changes.
 * Returned code remains borrowed from the host runtime; users must keep that
 * host resident until all consumers of its addresses have been unloaded. */
XL6_HOST_XRT_API XL6_HostXrtFunctionV1 xl6HostXrtResolveV1(const char* name);
#ifdef __cplusplus
}
#endif
#endif
