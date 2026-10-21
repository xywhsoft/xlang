#ifndef XLANG_IDE_LIBRARY_INFO_H
#define XLANG_IDE_LIBRARY_INFO_H
#include "services/command_catalog.h"
enum xide_library_state { XIDE_LIBRARY_NOT_FOUND, XIDE_LIBRARY_FOUND, XIDE_LIBRARY_INCOMPLETE, XIDE_LIBRARY_INVALID, XIDE_LIBRARY_UNSUPPORTED };
typedef struct xide_library_info {
    enum xide_library_state state;
    char *manifest,*version,*abiVersion;
    size_t declaredFiles,missingFiles;
    char message[512];
} xide_library_info;
void xideLibraryInfoClear(xide_library_info* info);
size_t xideLibraryCatalogEntry(const xide_command_catalog* catalog,const char* name);
/* Read-only disk inspection of the first existing candidate. This does not
   run the compiler, prove dependency resolution, or import/link a reference.
   packagePath is the inherited XLANG_PACKAGE_PATH; relative roots use the
   project manifest directory, as in the build service's process directory. */
int xideLibraryInspect(xide_library_info* info,const xide_project* project,const char* kind,const char* name,const char* compiler,const char* packagePath,xide_model_error* error);
#endif
