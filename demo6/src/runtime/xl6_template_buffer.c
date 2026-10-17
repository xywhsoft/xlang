#include "xl6_template_internal.h"
#include "xl6_buffer_internal.h"
int64_t xl6TemplateAppendV1(void* raw,void* target,const xvalue* data,const xvalue* options,XL6_Closure* callback)
{
    xbuffer* output=xl6BufferChecked(target);if (!output) {xl6TemplateForeignFailureV1();return 0;}
    xstrview text=xl6TemplateRenderV1(raw,data,options,callback);if (!text.Data) {xl6TemplateForeignFailureV1();return 0;}
    bool ok=text.Size<=(size_t)INT64_MAX-output->Size;
    if (!ok) xrtSetErrorKind(XERR_RANGE);
    else ok=xrtBufferAppend(output,(xbytesview){(const uint8*)text.Data,text.Size});
    xerror* error=xrtTakeError();xrtFree((void*)text.Data);xrtClearError();xrtSetErrorTake(error);
    if (!ok) xl6TemplateForeignFailureV1();
    return ok?(int64_t)text.Size:0;
}
