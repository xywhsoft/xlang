#ifndef XLANG_IDE_RESOURCE_INPUTS_H
#define XLANG_IDE_RESOURCE_INPUTS_H
#include "model/workspace.h"
/* Project only icon/manifest path strings from captured resource bytes. Other
   XLON values remain opaque: the configured compiler validates their semantics.
   Does not read files, load another XRT, or modify the resource configuration.
   On failure the caller's list is unchanged; returned strings use free(). */
int xideResourceInputPaths(const unsigned char* bytes,size_t size,const char* configPath,xide_strings* paths,xide_model_error* error);
void xideResourceInputPathsClear(xide_strings* paths);
#endif
