#ifndef XLANG_IDE_FILE_REBIND_H
#define XLANG_IDE_FILE_REBIND_H
#include "model/workspace.h"
typedef struct xide_file_rebind xide_file_rebind;
/* Inputs are resolved absolute paths (oldPath may be NULL).
   Synchronous UI-thread transaction: do not mutate the workspace between
   prepare and apply. Preparation writes no files; application allocates nothing. */
int xideFileRebindPrepare(xide_workspace* workspace,const char* oldPath,const char* target,xide_file_rebind** out,xide_model_error* error);
void xideFileRebindApply(xide_file_rebind* plan);
void xideFileRebindDestroy(xide_file_rebind* plan);
size_t xideFileRebindProjectCount(const xide_file_rebind* plan);
xide_project* xideFileRebindProjectAt(const xide_file_rebind* plan,size_t index);
#endif
