#ifndef XLANG_IDE_C_INPUT_TRACE_H
#define XLANG_IDE_C_INPUT_TRACE_H
#include "platform/file_rename.h"
typedef struct xide_c_input_trace xide_c_input_trace;
xide_c_input_trace* xideCInputTraceCreate(void);
void xideCInputTraceDestroy(xide_c_input_trace* trace);
/* Owner-worker calls. The notification contains actual compiler observations;
   Apply verifies those observations rather than taking a later baseline. */
int xideCInputTraceApply(xide_c_input_trace* trace,const xvalue* notification,
 int (*cancelled)(void*),void* user,xide_model_error* error);
int xideCInputTraceCheck(xide_c_input_trace* trace,int (*cancelled)(void*),void* user,xide_model_error* error);
int xideCInputTraceComplete(const xide_c_input_trace* trace);
/* Cached copy only; safe from the UI thread, with no filesystem I/O. Only
   files whose compiler-read raw bytes were observed are navigation targets. */
int xideCInputTraceSnapshot(xide_c_input_trace* trace,const char* path,xide_file_snapshot* out);
#endif
