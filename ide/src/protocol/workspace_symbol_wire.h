#ifndef XIDE_WORKSPACE_SYMBOL_WIRE_H
#define XIDE_WORKSPACE_SYMBOL_WIRE_H
#include "services/workspace_symbol_index.h"
/* SymbolInformation[] with complete UTF16 selection locations. Does not
 * advertise lazy resolve or assume client support for a missing range.
 * kindMask bit(kind-1);zero uses legacy1..18. Exact JSON-escaped result budget
 * leaves1MiB of the existing8MiB frame for request ID/envelope. Worker only. */
xvalue* xideWorkspaceSymbolWire(const xide_workspace_symbol_result* result,uint32_t kindMask,
 int (*cancelled)(void*),void* user,xide_model_error* error);
#endif
