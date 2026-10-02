#ifndef XL6_PROCESS_INTERNAL_H
#define XL6_PROCESS_INTERNAL_H
#include "src/runtime/xl6_text_thread.h"
#include "src/runtime/xl6_process_text.h"

/* Owned configuration. XRT borrows its views only until Spawn/Run returns.
 * No script pointers, managed graph edges or OS handles are hidden here. */
typedef struct XL6_ProcessCommand {
    XL6_TextThreadOwner owner;
    xprocessconfig config;
    size_t envCapacity;
} XL6_ProcessCommand;
typedef struct XL6_ProcessRunOptions {
    xprocessrunoptions options;
    int64_t timeoutMs;
} XL6_ProcessRunOptions;
static inline XL6_ProcessCommand* xl6ProcessCommandCheck(void* handle)
{
    if (!handle) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    XL6_ProcessCommand* command=handle;
    return xl6TextThreadCheck(&command->owner) ? command : NULL;
}
static inline bool xl6ProcessDuration(int64_t milliseconds,uint64* duration)
{
    if (milliseconds<0 || (uint64_t)milliseconds>UINT64_MAX/1000u) {
        xrtSetErrorKind(XERR_RANGE);return false;
    }
    *duration=(uint64_t)milliseconds*1000u;return true;
}
#endif
