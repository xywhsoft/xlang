#include "xl6_io_private.h"

static int64_t xlir_io_write(void* handle, xbytesview view, int64_t limit, bool all)
{ xwriter* w; size_t count, done = 0;
  if (!xlir_io_count(limit, view.Size, &count) || (w = xlir_io_writer_get(handle)) == NULL) return -1;
  if (!(all ? xrtWriterWriteFull(w, view.Data, count, &done) : xrtWriterWrite(w, view.Data, count, &done))) return -1;
  return xlir_io_position(done); }

XL6_TEXT_API int64_t xl6_xrt_io_writer_bytes(void* handle, const xvalue* data, int64_t limit)
{ xbytesview view = {0}; if (!xrtValueGetBytes(data, &view)) { xrtSetErrorKind(XERR_TYPE); return -1; } return xlir_io_write(handle, view, limit, false); }

XL6_TEXT_API bool xl6_xrt_io_writer_all_bytes(void* handle, const xvalue* data, int64_t limit)
{ xbytesview view = {0}; if (!xrtValueGetBytes(data, &view)) { xrtSetErrorKind(XERR_TYPE); return false; } return xlir_io_write(handle, view, limit, true) >= 0; }

XL6_TEXT_API int64_t xl6_xrt_io_writer_text(void* handle, xstrview data, int64_t limit)
{ xbytesview view = { (const unsigned char*)data.Data, data.Size }; return xlir_io_write(handle, view, limit, false); }

XL6_TEXT_API bool xl6_xrt_io_writer_all_text(void* handle, xstrview data, int64_t limit)
{ xbytesview view = { (const unsigned char*)data.Data, data.Size }; return xlir_io_write(handle, view, limit, true) >= 0; }

XL6_TEXT_API int64_t xl6_xrt_io_writer_write_buffer(void* handle, void* data, int64_t limit)
{ xbytesview view; if (data == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return -1; } view = xrtBufferView((xbuffer*)data); return xlir_io_write(handle, view, limit, false); }

XL6_TEXT_API bool xl6_xrt_io_writer_all_buffer(void* handle, void* data, int64_t limit)
{ xbytesview view; if (data == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return false; } view = xrtBufferView((xbuffer*)data); return xlir_io_write(handle, view, limit, true) >= 0; }
