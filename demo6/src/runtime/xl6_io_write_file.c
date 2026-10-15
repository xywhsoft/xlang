#include "xl6_io_private.h"

static bool xlir_io_write_file(xstrview path, const void* data, size_t actual, int64_t mode, int64_t limit)
{ const char* name; xfile file; xwriter* writer; uint32_t flags; size_t count;
  bool written, closed; xerror* error;
  if (mode < 0 || mode > 2 || limit < -1)
    { xrtSetErrorKind(XERR_RANGE); return false; }
  if (data == NULL && actual != 0)
    { xrtSetErrorKind(XERR_ARGUMENT); return false; }
  count = limit >= 0 && (uint64_t)limit < actual ? (size_t)limit : actual;
  name = xl6IoPath(path); if (name == NULL) return false;
  flags = XFILE_WRITE | XFILE_CREATE | (mode == 0 ? XFILE_EXCLUSIVE :
    mode == 1 ? XFILE_TRUNCATE : XFILE_APPEND);
  file = xrtOpen(name, flags); if (file == NULL) return false;
  writer = xrtWriterTakeFile(&file);
  if (writer == NULL) { error = xrtTakeError(); (void)xrtClose(file);
    xl6IoRestoreFailure(error); return false; }
  written = xrtWriterWriteFull(writer, data, count, NULL) && xrtWriterFlush(writer);
  error = !written ? xrtTakeError() : NULL; closed = xrtWriterDestroy(writer);
  if (!written) { xl6IoRestoreFailure(error); }
  return written && closed; }

XL6_TEXT_API bool xl6_xrt_io_write_file(xstrview path, const xvalue* data, int64_t mode, int64_t limit)
{ xbytesview view = {0}; if (data == NULL || !xrtValueGetBytes(data, &view))
    { xrtSetErrorKind(XERR_TYPE); return false; }
  return xlir_io_write_file(path, view.Data, view.Size, mode, limit); }

XL6_TEXT_API bool xl6_xrt_io_write_text_file(xstrview path, xstrview data, int64_t mode, int64_t limit)
{ return xlir_io_write_file(path, data.Data, data.Size, mode, limit); }

XL6_TEXT_API bool xl6_xrt_io_write_buffer_file(xstrview path, void* data, int64_t mode, int64_t limit)
{ xbytesview view; if (data == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
  view = xrtBufferView((xbuffer*)data); return xlir_io_write_file(path, view.Data, view.Size, mode, limit); }
