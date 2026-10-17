#include "xl6_template_internal.h"
#include "xl6_io_handle.h"
typedef struct Writer {xwriter* stream;uint64_t count;} Writer;
static bool write(ptr raw,xstrview text)
{
    Writer* output=raw;size_t written=0;
    if (!xl6TextSelect(&text,-1)) return false;
    bool ok=xrtWriterWriteFull(output->stream,text.Data,text.Size,&written);output->count+=written;return ok;
}
uint64_t xl6TemplateWriteToV1(void* raw,void* target,const xvalue* data,const xvalue* options,XL6_Closure* callback)
{
    xlir_io_writer* output=target;
    if (!output || !output->stream) {xrtSetErrorKind(XERR_STATE);xl6TemplateForeignFailureV1();return 0;}
    Writer writer={output->stream,0};
    if (!xl6TemplateWriteOutputV1(raw,data,options,callback,write,&writer)) xl6TemplateForeignFailureV1();
    return writer.count;
}
