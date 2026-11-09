#ifndef XIDE_WORKSPACE_SYMBOL_SELECTION_H
#define XIDE_WORKSPACE_SYMBOL_SELECTION_H
#include <xrt.h>
/* Bounded complete SymbolInformation row;no allocation,disk or body hashing. */
int xideWorkspaceSymbolSelectionValid(const xvalue* selection);
#endif
