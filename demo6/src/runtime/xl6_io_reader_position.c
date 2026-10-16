#include "xl6_io_private.inc"

XL6_TEXT_API int64_t xl6_xrt_io_reader_tell(void* handle)
{ xreader* r = xlir_io_reader_get(handle); uint64_t value = 0;
  return r != NULL && xrtReaderTell(r, &value) ? xlir_io_position(value) : -1; }

XL6_TEXT_API int64_t xl6_xrt_io_reader_size(void* handle)
{ xreader* r = xlir_io_reader_get(handle); uint64_t value = 0;
  return r != NULL && xrtReaderSize(r, &value) ? xlir_io_position(value) : -1; }

XL6_TEXT_API int64_t xl6_xrt_io_reader_seek(void* handle, int64_t offset, int64_t origin)
{ xreader* r = xlir_io_reader_get(handle); uint64_t value = 0;
  if (origin < 0 || origin > 2) { xrtSetErrorKind(XERR_RANGE); return -1; }
  return r != NULL && xrtReaderSeek(r, offset, (xseek)origin, &value) ? xlir_io_position(value) : -1; }

XL6_TEXT_API bool xl6_xrt_io_reader_eof(void* handle) { return xlir_io_reader_get(handle) != NULL && xrtReaderEOF(((xlir_io_reader*)handle)->stream); }

XL6_TEXT_API bool xl6_xrt_io_reader_can_seek(void* handle) { return xlir_io_reader_get(handle) != NULL && xrtReaderCanSeek(((xlir_io_reader*)handle)->stream); }

XL6_TEXT_API bool xl6_xrt_io_reader_can_size(void* handle) { return xlir_io_reader_get(handle) != NULL && xrtReaderCanSize(((xlir_io_reader*)handle)->stream); }

XL6_TEXT_API bool xl6_xrt_io_reader_closed(void* handle) { return handle == NULL || ((xlir_io_reader*)handle)->stream == NULL; }
