#include "xl6_io_private.h"

static xvalue* xlir_io_read(void* handle, int64_t max, bool exact)
{ size_t count, done = 0; unsigned char* data = NULL; xvalue* result; xreader* r;
  if (!xlir_io_size(max, &count) || (r = xlir_io_reader_get(handle)) == NULL) return NULL;
  if (count != 0 && (data = (unsigned char*)xrtMalloc(count)) == NULL) return NULL;
  if (!(exact ? xrtReaderReadFull(r, data, count, &done) : xrtReaderRead(r, data, count, &done)))
    { xrtFree(data); return NULL; }
  result = xrtValueBytesTake(&data, done); xrtFree(data); return result; }

XL6_TEXT_API xvalue* xl6_xrt_io_reader_read(void* r, int64_t n) { return xlir_io_read(r, n, false); }

XL6_TEXT_API xvalue* xl6_xrt_io_reader_exact(void* r, int64_t n) { return xlir_io_read(r, n, true); }

XL6_TEXT_API xvalue* xl6_xrt_io_reader_all(void* handle, int64_t limit)
{ xreader* r; xbuffer* buffer; size_t max, size = 0; unsigned char* data; xvalue* value;
  if (!xlir_io_size(limit, &max) || (r = xlir_io_reader_get(handle)) == NULL) return NULL;
  buffer = xrtReaderReadAll(r, max); if (buffer == NULL) return NULL;
  data = xrtBufferTake(buffer, &size, NULL); xrtBufferDestroy(buffer);
  value = xrtValueBytesTake(&data, size); xrtFree(data); return value; }

XL6_TEXT_API int64_t xl6_xrt_io_reader_into(void* handle, void* target, int64_t limit)
{ xreader* r; xbuffer* buffer = (xbuffer*)target; size_t max, old, done = 0; xbytesview view;
  if (!xlir_io_size(limit, &max) || (r = xlir_io_reader_get(handle)) == NULL) return -1;
  if (buffer == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return -1; }
  old = xrtBufferView(buffer).Size;
  if (max > SIZE_MAX - old) { xrtSetErrorKind(XERR_RANGE); return -1; }
  if (!xrtBufferResize(buffer, old + max)) return -1;
  view = xrtBufferView(buffer);
  if (!xrtReaderRead(r, max != 0 ? (unsigned char*)view.Data + old : NULL, max, &done))
    { (void)xrtBufferResize(buffer, old); return -1; }
  (void)xrtBufferResize(buffer, old + done); return (int64_t)done; }
