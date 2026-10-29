#include "src/runtime/xl6_path_build.h"
#include "src/runtime/xl6_array_input.h"
#include "src/api/xl_foreign_error.h"
#include <string.h>
xstrview xl6_xrt_fs_path_build(const xvalue* source,int64_t style)
{
    xstrview result={0}; XL6_ArrayInputV1 input={0}; xstrview* parts=NULL;
    xl6_error_scope scope={0}; bool ok=false;
    if(!xl6ErrorScopeBegin(&scope))return result;
    if(style<0 || style>2){xrtSetErrorKind(XERR_RANGE);goto done;}
    if(!xl6ArrayInputAcquireV1(source,&input))goto done;
    if(input.count>SIZE_MAX/sizeof(*parts)){xrtSetErrorKind(XERR_RANGE);goto done;}
    if(input.count){
        parts=xrtMalloc(input.count*sizeof(*parts));
        if(!parts){if(!xrtGetError())xrtSetErrorKind(XERR_MEMORY);goto done;}
    }
    for(size_t i=0;i<input.count;++i){
        if(!xrtValueGetString(input.items[i],&parts[i])){if(!xrtGetError())xrtSetErrorKind(XERR_TYPE);goto done;}
        if(memchr(parts[i].Data,0,parts[i].Size)){xrtSetErrorKind(XERR_VALUE);goto done;}
    }
    result.Data=xrtPathBuild(parts,input.count,(xpathstyle)style);
    if(!result.Data){if(!xrtGetError())xrtSetErrorKind(XERR_MEMORY);goto done;}
    result.Size=strlen(result.Data);ok=true; /* NUL was explicitly refused. */
done:
    if(!ok)xl6ErrorScopeCaptureFailure(&scope);
    xrtFree(parts);xl6ArrayInputReleaseV1(&input);
    if(xrtGetError())xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok){xrtFree((void*)result.Data);result=(xstrview){0};xl6ErrorScopePropagateFailure(outcome);}
    return result;
}
