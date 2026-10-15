#include "xl6_io_private.h"

static bool xlir_io_reader_close(void* handle)
{ xlir_io_reader* r = (xlir_io_reader*)handle; xreader* stream;
  if (r == NULL || r->stream == NULL) return true;
  stream = r->stream; r->stream = NULL; return xrtReaderDestroy(stream); }

XL6_TEXT_API void xl6_xrt_io_reader_destroy(void* handle)
{ xerror* error = xrtTakeError(); (void)xlir_io_reader_close(handle); xrtFree(handle); xrtClearError();
  if (error != NULL) { xrtSetErrorTake(error); } }

XL6_TEXT_API bool xl6_xrt_io_reader_close(void* handle) { return xlir_io_reader_close(handle); }

XL6_TEXT_API void* xl6_xrt_io_reader_handle(void* handle) { return handle; }
/* Builtin factories own native file/buffer snapshots or standard streams.
 * Process streams have an additional owning graph and are refused here. */
static bool readerTrace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;(void)context;
    return visit && xrtValueGetHandle(value,&raw,NULL,NULL) && raw &&
        ((const xlir_io_reader*)raw)->origin==XL6_IO_ORIGIN_BUILTIN;
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6IoReaderHandlePolicyV1Get,xl6_xrt_io_reader_destroy,readerTrace)
