#ifndef XLS_EXIT_SCOPE_H
#define XLS_EXIT_SCOPE_H
#include "analysis/ir_profile.h"
#if XLS_EXPERIMENTAL_IR_EXIT_REUSE
typedef struct xls_exit_pending {xstrbuf key;size_t limit,at,compareSize;const unsigned char* compare;void* build;}xls_exit_pending;
static int xlsExitTry(XL6_IrModule*,xls_exit_pending*);
static void xlsExitFinish(XL6_IrModule*,xls_exit_pending*,int);
#endif
#endif
