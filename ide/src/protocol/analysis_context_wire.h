#ifndef XIDE_PROTOCOL_ANALYSIS_CONTEXT_WIRE_H
#define XIDE_PROTOCOL_ANALYSIS_CONTEXT_WIRE_H
#include "analysis/context.h"
#include "app_xrt.h"
/* Current in-process compiler-XRT analysis family, never the selected execution
   compiler. Contexts do not persist across server/binary lifetimes. */
#define XIDE_ANALYSIS_PROFILE "xlang-ls/compiler-xrt-v1"
/* Strict schema1 wire representation of the existing immutable context, not
   another project manifest parser. Read copies into the core's bounded XRT
   allocation; write returns an owned DOM. Both perform no filesystem I/O. */
xls_context* xideAnalysisContextRead(const xvalue* value,char* error,size_t capacity);
xvalue* xideAnalysisContextWrite(const xls_context* context);
#endif
