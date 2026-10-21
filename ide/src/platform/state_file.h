#ifndef XLANG_IDE_STATE_FILE_H
#define XLANG_IDE_STATE_FILE_H
#include "model/workspace.h"
typedef struct xide_state_file { char* path; xvalue* source; int existed; } xide_state_file;
int xideStateOpen(xide_state_file* state,const char* path,const char* format,xide_model_error* error);
int xideStateWrite(xide_state_file* state,xvalue* root,xide_model_error* error);
void xideStateClear(xide_state_file* state);
#endif