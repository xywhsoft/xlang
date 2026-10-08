#ifndef XL6_ARRAY_OS_ARGV_H
#define XL6_ARRAY_OS_ARGV_H
#include <stdint.h>
typedef struct xvalue xvalue;
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_ARRAY_OS_ARGV_API __declspec(dllexport)
#else
#define XL6_ARRAY_OS_ARGV_API
#endif
/* Borrowed NUL-terminated UTF-16 argument vector -> owned canonical ANY Array.
 * Strict Unicode conversion; invalid surrogate sequences fail, never replace.
 * Also the independently testable Windows OS-adapter conversion boundary. */
XL6_ARRAY_OS_ARGV_API xvalue* xl6ArrayUtf16ArgvCreateV1(uint64_t,uint64_t,int,const uint16_t* const*);
/* For a generated executable's main ONLY. Windows reads the process's Unicode
 * command line with CommandLineToArgvW; the CRT's ACP argv is NOT interpreted
 * as UTF-8. POSIX delegates to the explicit C/POSIX byte-vector factory.
 * Hosts/JIT use EntryAcquire instead, and do not select this OS component. */
XL6_ARRAY_OS_ARGV_API xvalue* xl6ArrayOsArgvCreateV1(uint64_t,uint64_t,int,char* const*);
#endif
