#include "xl6_io_private.h"

static bool xlir_io_reader_close(void* handle)
{
    xlir_io_reader* r=handle;xreader* stream;xrtownershipscope mutation={0};
    if(!r)return true;
    if(!xrtOwnershipMutationBegin(&mutation))abort();
    stream=r->stream;r->stream=NULL;r->contextOwner=(xrtownershipref){0};
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    return xrtReaderDestroy(stream); /* Close callbacks run outside our scope. */
}

XL6_TEXT_API void xl6_xrt_io_reader_destroy(void* handle)
{ xerror* error = xrtTakeError(); (void)xlir_io_reader_close(handle); xrtFree(handle); xrtClearError();
  if (error != NULL) { xrtSetErrorTake(error); } }

XL6_TEXT_API bool xl6_xrt_io_reader_close(void* handle) { return xlir_io_reader_close(handle); }

XL6_TEXT_API void* xl6_xrt_io_reader_handle(void* handle) { return handle; }
/* Native IO and its callback context are uniquely owned, not RC nodes. Trace
 * through that private ownership to the actual shared Process wrapper. */
static bool readerTrace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;
    if(!xrtValueGetHandle(value,&raw,NULL,NULL) || !raw)return false;
    const xlir_io_reader* reader=raw;
    return xl6IoContextTrace(reader->origin,reader->stream,reader->contextOwner,visit,context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6IoReaderHandlePolicyV1Get,xl6_xrt_io_reader_destroy,readerTrace)
