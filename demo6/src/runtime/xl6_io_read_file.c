#include "xl6_io_private.inc"

XL6_TEXT_API xvalue* xl6_xrt_io_read_file(xstrview path, int64_t limit)
{ const char* name; xreader* reader; xbuffer* buffer; unsigned char* data;
  size_t size = 0; xvalue* result; bool closed; xerror* error;
  if (limit < 0 || (uint64_t)limit > SIZE_MAX)
    { xrtSetErrorKind(XERR_RANGE); return NULL; }
  name = xl6IoPath(path); if (name == NULL) return NULL;
  reader = xrtReaderOpen(name); if (reader == NULL) return NULL;
  buffer = xrtReaderReadAll(reader, (size_t)limit);
  error = buffer == NULL ? xrtTakeError() : NULL;
  closed = xrtReaderDestroy(reader);
  if (buffer == NULL) xl6IoRestoreFailure(error);
  if (buffer == NULL || !closed) { xrtBufferDestroy(buffer); return NULL; }
  data = xrtBufferTake(buffer, &size, NULL); xrtBufferDestroy(buffer);
  result = xrtValueBytesTake(&data, size); if (data != NULL) xrtFree(data);
  return result; }
