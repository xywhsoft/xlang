#ifndef XLANG_IDE_PROCESS_H
#define XLANG_IDE_PROCESS_H
#include "app_xrt.h"
#include <stddef.h>
typedef struct xide_process xide_process;
typedef struct xide_process_spec {
    const char *program,*directory,*logPath;
    const char* const* arguments;
    size_t argumentCount;
    const xprocessenv* environment;
    size_t environmentCount;
    int interactive,separateStderr;
    size_t stdoutCapacity; /* 0 = 1 MiB, explicit 16 KiB..16 MiB (extended API: 80 MiB) */
} xide_process_spec;
enum xide_process_state { XIDE_PROCESS_STARTING, XIDE_PROCESS_RUNNING, XIDE_PROCESS_STOPPING, XIDE_PROCESS_FINISHED };
typedef struct xide_process_info {
    enum xide_process_state state;
    xprocessstatus exit;
    unsigned long long id,durationMs,droppedBytes,stderrDroppedBytes;
    int started,cancelled,ioError;
    char error[512];
} xide_process_info;
/* Start deep-copies the request. Public calls are made by one owning thread.
   All spawn/read/write/wait work runs off that thread; no UI API is used here.
   Output defaults to a bounded 1 MiB queue; protocol consumers can request
   up to 16 MiB. Stderr stays 1 MiB. Optional raw logs preserve every byte.
   A deep working directory may use its existing, identity-verified short
   spelling; no junction or project copy is created. Native Windows failures
   retain their system code. A deep directory without a usable short spelling
   still fails and explains the current-directory limit. */
xide_process* xideProcessStart(const xide_process_spec* spec,char* error,size_t capacity);
/* Explicit bounded opt-in for large compiler resource responses (80 MiB). */
xide_process* xideProcessStartExtended(const xide_process_spec* spec,char* error,size_t capacity);
void xideProcessGetInfo(xide_process* process,xide_process_info* info);
size_t xideProcessRead(xide_process* process,void* data,size_t capacity);
size_t xideProcessReadError(xide_process* process,void* data,size_t capacity);
int xideProcessWrite(xide_process* process,const void* data,size_t size);
void xideProcessCloseInput(xide_process* process);
int xideProcessInputOpen(xide_process* process);
void xideProcessCancel(xide_process* process);
/* Cancels and joins workers. Call before disposing the application services. */
void xideProcessDestroy(xide_process* process);
#endif
