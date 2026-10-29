#include "xl6_io_private.inc"

XL6_TEXT_API bool xl6_xrt_io_writer_flush(void* handle)
{ xwriter* w = xlir_io_writer_get(handle); return w != NULL && xrtWriterFlush(w); }

XL6_TEXT_API int64_t xl6_xrt_io_writer_tell(void* handle)
{ xwriter* w = xlir_io_writer_get(handle); uint64_t value = 0;
  return w != NULL && xrtWriterTell(w, &value) ? xlir_io_position(value) : -1; }

XL6_TEXT_API int64_t xl6_xrt_io_writer_size(void* handle)
{ xwriter* w = xlir_io_writer_get(handle); uint64_t value = 0;
  return w != NULL && xrtWriterSize(w, &value) ? xlir_io_position(value) : -1; }

XL6_TEXT_API int64_t xl6_xrt_io_writer_seek(void* handle, int64_t offset, int64_t origin)
{ xwriter* w = xlir_io_writer_get(handle); uint64_t value = 0;
  if (origin < 0 || origin > 2) { xrtSetErrorKind(XERR_RANGE); return -1; }
  return w != NULL && xrtWriterSeek(w, offset, (xseek)origin, &value) ? xlir_io_position(value) : -1; }

XL6_TEXT_API bool xl6_xrt_io_writer_can_seek(void* handle) { return xlir_io_writer_get(handle) != NULL && xrtWriterCanSeek(((xlir_io_writer*)handle)->stream); }

XL6_TEXT_API bool xl6_xrt_io_writer_can_size(void* handle) { return xlir_io_writer_get(handle) != NULL && xrtWriterCanSize(((xlir_io_writer*)handle)->stream); }

XL6_TEXT_API bool xl6_xrt_io_writer_closed(void* handle) { return handle == NULL || ((xlir_io_writer*)handle)->stream == NULL; }
