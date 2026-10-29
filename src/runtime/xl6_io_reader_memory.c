#include "xl6_io_private.inc"

static void* xlir_io_reader_snapshot(xbytesview view)
{ xbuffer* buffer = xrtBufferFrom(view); xreader* stream; xerror* error;
  if (buffer == NULL) return NULL;
  stream = xrtReaderTakeBuffer(&buffer);
  if (stream == NULL) { error = xrtTakeError(); xrtBufferDestroy(buffer);
    xl6IoRestoreFailure(error); return NULL; }
  return xlir_io_reader_take(stream); }

XL6_TEXT_API void* xl6_xrt_io_reader_bytes(const xvalue* data, int64_t limit)
{ xbytesview view = {0}; if (!xrtValueGetBytes(data, &view)) { xrtSetErrorKind(XERR_TYPE); return NULL; }
  if (!xlir_io_count(limit, view.Size, &view.Size)) return NULL;
  return xlir_io_reader_snapshot(view); }

XL6_TEXT_API void* xl6_xrt_io_reader_buffer(void* data, int64_t limit)
{ if (data == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
  xbytesview view = xrtBufferView((xbuffer*)data);
  if (!xlir_io_count(limit, view.Size, &view.Size)) return NULL;
  return xlir_io_reader_snapshot(view); }
