#include "xl6_template_internal.h"
xstrview xl6TemplateReadFileV1(xstrview path,int64_t encoding,int64_t policy,int64_t maxInput)
{
    if (!xl6TextSelect(&path,-1)) return xl6TextFailure;
    if (encoding<0 || encoding>XENCODING_UTF32_BE || policy<XUTF_STRICT || policy>XUTF_REPLACE || maxInput<0) {
        xrtSetErrorKind(XERR_RANGE);return xl6TextFailure;
    }
    if (memchr(path.Data,0,path.Size)) {xrtSetErrorKind(XERR_ARGUMENT);return xl6TextFailure;}
    char* name=xrtStrDupView(path);if (!name) return xl6TextFailure;
    size_t size=0;char* text=xrtFileReadTextLimit(name,(xencoding)encoding,(xutfpolicy)policy,(size_t)maxInput,&size);
    xerror* error=xrtTakeError();xrtFree(name);xrtClearError();xrtSetErrorTake(error);
    return xl6TextOwn(text,size);
}
void* xl6TemplateFileCreateV1(xstrview path,const xvalue* options,int64_t encoding,int64_t policy,int64_t maxInput)
{
    xtemplateconfig config;if (!xl6TemplateCompileConfigV1(options,&config)) return NULL;
    xstrview source=xl6TemplateReadFileV1(path,encoding,policy,maxInput);if (!source.Data) return NULL;
    xtemplate* compiled=xrtTemplateCompileConfig(source,&config);
    xerror* error=xrtTakeError();xrtFree((void*)source.Data);xrtClearError();xrtSetErrorTake(error);
    return xl6TemplateTakeV1(compiled,0,false);
}
