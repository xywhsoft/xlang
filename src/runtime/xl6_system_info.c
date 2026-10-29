#include "xl6_text_internal.h"
#include "xl6_foundation_abi.h"
#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#include <errno.h>
#endif

xstrview xl6SystemPlatformV1(void)
{
#if defined(_WIN32)
    return xl6TextCopy(XRT_STR_LITERAL("windows"));
#elif defined(__APPLE__)
    return xl6TextCopy(XRT_STR_LITERAL("macos"));
#elif defined(__linux__)
    return xl6TextCopy(XRT_STR_LITERAL("linux"));
#elif defined(__FreeBSD__)
    return xl6TextCopy(XRT_STR_LITERAL("freebsd"));
#else
    return xl6TextCopy(XRT_STR_LITERAL("unknown"));
#endif
}
xstrview xl6SystemArchitectureV1(void)
{
#if defined(__aarch64__) || defined(_M_ARM64)
    return xl6TextCopy(XRT_STR_LITERAL("arm64"));
#elif defined(__x86_64__) || defined(_M_X64)
    return xl6TextCopy(XRT_STR_LITERAL("x86_64"));
#elif defined(__i386__) || defined(_M_IX86)
    return xl6TextCopy(XRT_STR_LITERAL("x86"));
#elif defined(__arm__) || defined(_M_ARM)
    return xl6TextCopy(XRT_STR_LITERAL("arm"));
#elif defined(__riscv) && __riscv_xlen == 64
    return xl6TextCopy(XRT_STR_LITERAL("riscv64"));
#elif defined(__powerpc64__)
    return xl6TextCopy(XRT_STR_LITERAL("ppc64"));
#elif defined(__s390x__)
    return xl6TextCopy(XRT_STR_LITERAL("s390x"));
#else
    return xl6TextCopy(XRT_STR_LITERAL("unknown"));
#endif
}
int64_t xl6SystemPointerBitsV1(void) { return sizeof(void*) * CHAR_BIT; }
bool xl6SystemLittleEndianV1(void)
{ const uint16_t one = 1; return *(const unsigned char*)&one == 1; }
int64_t xl6SystemLogicalProcessorCountV1(void)
{
    int32_t systemCode=0;
#if defined(_WIN32)
    DWORD count = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    if (count) return count;
    systemCode=(int32_t)GetLastError();
#elif defined(_SC_NPROCESSORS_ONLN)
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    if (count > 0) return count;
    systemCode=errno;
#else
    xrtSetErrorKind(XERR_UNSUPPORTED);
    return 0;
#endif
    xerrordesc desc={.Kind=XERR_IO,.SystemCode=systemCode,.Domain="xlang.os",
        .Operation="logicalProcessorCount",.Message="logical processor count query failed"};
    xerror* error=xrtErrorBuild(&desc);
    if (error) xrtSetErrorTake(error);
    else if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
    return 0;
}
