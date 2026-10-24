#ifndef XL6_RUNTIME_EXPORT_H
#define XL6_RUNTIME_EXPORT_H
/* Shared declaration visibility, usable by physical ABI headers without
 * including the complete text/runtime declaration registry or a cyclic include. */
#ifndef XL6_TEXT_API
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_TEXT_API __declspec(dllexport)
#else
#define XL6_TEXT_API
#endif
#endif
#endif
