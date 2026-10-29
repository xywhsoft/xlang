#ifndef XLANG_ANALYSIS_FRONTEND_COMPLETION_PATH_H
#define XLANG_ANALYSIS_FRONTEND_COMPLETION_PATH_H
#include "src/sem/xl6_completion.h"
/* The digest-guarded parser profile binds this AST capability to the matching
 * completion/path API. Current comparison errors must never prove equality. */
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
#include "src/common/xl6_path_identity.h"
#define xlsCompletionPathEqual(a,b) (xl6PathIdentityEqual(a,b)==1)
static inline int xlsCompletionTargetConst(const XL6_CompletionQuery* query,const char* path)
{
    if(!query)return 0;
    /* Original current Target may record an error. A proof over an immutable
     * query uses a private probe; failure declines reuse without publishing it. */
    XL6_CompletionQuery probe=*query;
    return xl6CompletionTarget(&probe,path);
}
#else
#define xlsCompletionPathEqual(a,b) xl6CompletionPathEqual(a,b)
#define xlsCompletionTargetConst(query,path) xl6CompletionTarget(query,path)
#endif
#endif
