#include "xl6_template_internal.h"
#include "xl6_vfs_private.h"
xstrview xl6TemplateReadVfsV1(void* raw,xstrview path,int64_t encoding,int64_t policy,int64_t maxInput)
{
    xvfs filesystem=xl6VfsGet(raw);
    if (!filesystem || !xl6TextSelect(&path,-1)) return xl6TextFailure;
    if (encoding<0 || encoding>XENCODING_UTF32_BE || policy<XUTF_STRICT || policy>XUTF_REPLACE || maxInput<0) {
        xrtSetErrorKind(XERR_RANGE);return xl6TextFailure;
    }
    if (memchr(path.Data,0,path.Size)) {xrtSetErrorKind(XERR_ARGUMENT);return xl6TextFailure;}
    char* name=xrtStrDupView(path);if (!name) return xl6TextFailure;
    size_t size=0;bytes input=xrtVfsReadAllLimit(filesystem,name,(size_t)maxInput,&size);
    xerror* error=xrtTakeError();xrtFree(name);xrtClearError();xrtSetErrorTake(error);
    if (!input) return xl6TextFailure;
    size_t bomSize=0;xbytesview view={input,size};xencoding bom=xrtEncodingBom(view,&bomSize);
    xencoding selected=(xencoding)encoding;
    if (selected!=XENCODING_UNKNOWN && bom!=XENCODING_UNKNOWN && selected!=bom) {
        xrtFree(input);xrtSetErrorKind(XERR_VALUE);return xl6TextFailure;
    }
    if (selected==XENCODING_UNKNOWN) selected=bom!=XENCODING_UNKNOWN?bom:size?xrtEncodingGuess(view).Encoding:XENCODING_UTF8;
    if (selected==XENCODING_UNKNOWN) {xrtFree(input);xrtSetErrorKind(XERR_VALUE);return xl6TextFailure;}
    size_t outputSize=0;
    bytes output=xrtTranscode((xbytesview){input+bomSize,size-bomSize},selected,XENCODING_UTF8,(xutfpolicy)policy,false,&outputSize);
    error=xrtTakeError();xrtFree(input);xrtClearError();xrtSetErrorTake(error);
    return xl6TextOwn((char*)output,outputSize);
}
void* xl6TemplateVfsCreateV1(void* filesystem,xstrview path,const xvalue* options,int64_t encoding,int64_t policy,int64_t maxInput)
{
    xtemplateconfig config;if (!xl6TemplateCompileConfigV1(options,&config)) return NULL;
    xstrview source=xl6TemplateReadVfsV1(filesystem,path,encoding,policy,maxInput);if (!source.Data) return NULL;
    xtemplate* compiled=xrtTemplateCompileConfig(source,&config);
    xerror* error=xrtTakeError();xrtFree((void*)source.Data);xrtClearError();xrtSetErrorTake(error);
    return xl6TemplateTakeV1(compiled,0,false);
}
