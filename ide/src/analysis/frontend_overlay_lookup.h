#ifndef XLS_FRONTEND_OVERLAY_LOOKUP_H
#define XLS_FRONTEND_OVERLAY_LOOKUP_H
#include "src/compile/xl6_source_overlay.h"
/* Match the shared current API: 1 hit,0 miss,-1 failed identity/report.
 * Historical AST builds retain the old API's existing failure observation. */
static inline int xlsFrontendOverlayLookup(const XL6_SourceOverlay* overlay,
    const char* path,const XL6_SourceOverlayItem** item)
{
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
    return xl6SourceOverlayLookup(overlay,path,item);
#else
    if(!item)return -1;
    *item=xl6SourceOverlayFind(overlay,path);return *item ? 1 : 0;
#endif
}
#endif
