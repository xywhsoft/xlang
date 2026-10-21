#ifndef XLANG_IDE_RUNTIME_FAILURE_H
#define XLANG_IDE_RUNTIME_FAILURE_H
#include <stddef.h>
enum xide_failure_kind {
    XIDE_FAILURE_NONE, XIDE_FAILURE_ARGUMENTS, XIDE_FAILURE_WINDOW,
    XIDE_FAILURE_STARTUP, XIDE_FAILURE_WORKSPACE, XIDE_FAILURE_DOCUMENT,
    XIDE_FAILURE_COMMAND, XIDE_FAILURE_RENDER
};
enum xide_failure_recovery {
    XIDE_FAILURE_RECOVERY_UNAVAILABLE, XIDE_FAILURE_RECOVERY_DISABLED,
    XIDE_FAILURE_RECOVERY_SAVED, XIDE_FAILURE_RECOVERY_FAILED
};
typedef struct xide_runtime_failure {
    int kind,code,recovery;
    char domain[32],detail[1024],operation[256],recoveryDetail[512];
} xide_runtime_failure;
typedef struct xide_failure_report {
    char path[32768];
    unsigned long error;
    int fallback;
} xide_failure_report;
#define XIDE_DIAGNOSTIC_MAX_FILES 64u
#define XIDE_DIAGNOSTIC_MAX_BYTES (512u * 1024u)
#define XIDE_DIAGNOSTIC_MAX_REPORT 8192u
/* Preserve the first cause. Recovery and report failures never replace it. */
void xideFailureSet(xide_runtime_failure* failure,int kind,const char* domain,int code,const char* detail,const char* operation);
void xideFailureRecovery(xide_runtime_failure* failure,int recovery,const char* detail);
/* Before SDK initialization, and after SDK teardown, this uses Win32 only.
   An explicit root is isolated: it never falls back to another directory. */
int xideFailureWrite(const xide_runtime_failure* failure,const char* root,int frames,xide_failure_report* report);
int xideFailureMessage(const xide_runtime_failure* failure,const xide_failure_report* report,wchar_t* message,size_t capacity);
void xideFailureShow(const xide_runtime_failure* failure,const xide_failure_report* report);
#endif
