#ifndef XLANG_IDE_C_COMMANDS_H
#define XLANG_IDE_C_COMMANDS_H
#include "model/workspace.h"
typedef struct xide_c_compile_profile {
 const char *driver,*target,*prelude;
 const char* const* systemIncludes;
 size_t systemIncludeCount;
} xide_c_compile_profile;
/* C11, active project include paths, C analysis system/forced headers and
   object-like macros, plus explicit C/header entries. The profile
   describes Clang analysis, not a claim of identical TCC preprocessing/ABI.
   Returns an owned standard compilation database, without writing a file. */
xvalue* xideCCommandsCreate(const xide_project* project,const char* configurationId,const xide_c_compile_profile* profile,xide_model_error* error);
/* Add an analysis-only file using an actual existing project's command; the
   source operand is replaced, with flags and working directory preserved. */
int xideCCommandsAddContextFile(xvalue* database,const char* origin,const char* path,xide_model_error* error);
#endif
