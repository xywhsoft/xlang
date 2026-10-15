#include "xl6_io_private.h"

static bool xlir_io_writer_close(void* handle)
{
    xlir_io_writer* w=handle;xwriter* stream;xrtownershipscope mutation={0};
    if(!w)return true;
    if(!xrtOwnershipMutationBegin(&mutation))abort();
    stream=w->stream;w->stream=NULL;w->contextOwner=(xrtownershipref){0};
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    return xrtWriterDestroy(stream);
}

XL6_TEXT_API void xl6_xrt_io_writer_destroy(void* handle)
{ xlir_io_writer* w = (xlir_io_writer*)handle; xerror* error = xrtTakeError();
  (void)xlir_io_writer_close(handle);
  if (w != NULL && w->drop != NULL) { w->drop(w->owner); } xrtFree(w); xrtClearError();
  if (error != NULL) { xrtSetErrorTake(error); } }

XL6_TEXT_API bool xl6_xrt_io_writer_close(void* handle) { return xlir_io_writer_close(handle); }

XL6_TEXT_API void* xl6_xrt_io_writer_handle(void* handle) { return handle; }
static bool writerTrace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;
    if(!xrtValueGetHandle(value,&raw,NULL,NULL) || !raw)return false;
    const xlir_io_writer* writer=raw;
    return xl6IoContextTrace(writer->origin,writer->stream,writer->contextOwner,visit,context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6IoWriterHandlePolicyV1Get,xl6_xrt_io_writer_destroy,writerTrace)
