#ifndef XL6_THREAD_EXIT_H
#define XL6_THREAD_EXIT_H
#include <stdbool.h>
/* Private resident hook, not user registration or a source-module callback.
 * Only the compiled ThreadLocal provider publishes this permanent entry. The
 * ordinary Thread provider does not depend on or link the ThreadLocal library. */
#if defined(__GNUC__) && !defined(_WIN32)
#define XL6_THREAD_EXIT_PRIVATE __attribute__((visibility("hidden")))
#else
#define XL6_THREAD_EXIT_PRIVATE
#endif
typedef bool (*XL6_ThreadLocalExitV1)(void);
XL6_THREAD_EXIT_PRIVATE bool xl6ThreadExitRegisterLocalV1(XL6_ThreadLocalExitV1);
XL6_THREAD_EXIT_PRIVATE bool xl6ThreadExitCurrentV1(void);
#endif
