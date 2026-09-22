#ifndef XL6_XRT_TRIM_H
#define XL6_XRT_TRIM_H

#include "src/compile/xl6_imports.h"

/* Build the XRT implementation translation unit from the verified artifact
 * linkage plan and explicit package policy. Source text and AST spelling are
 * deliberately outside this boundary. */
int xl6XrtTrimBuildImplementationSourceFromArtifact(
    const XL6_ImportDeps* deps,
    const char* const* modules,
    size_t moduleCount,
    char** outText);

#endif
