#include "xl6_io_private.inc"

XL6_TEXT_API void* xl6_xrt_io_writer_open(xstrview path, int64_t mode)
{ const char* name; xfile file; xwriter* stream; xerror* error; uint32_t flags;
  if (mode < 0 || mode > 2) { xrtSetErrorKind(XERR_RANGE); return NULL; }
  name = xl6IoPath(path); if (name == NULL) return NULL;
  flags = XFILE_WRITE | XFILE_CREATE | (mode == 0 ? XFILE_EXCLUSIVE : mode == 1 ? XFILE_TRUNCATE : XFILE_APPEND);
  file = xrtOpen(name, flags); if (file == NULL) return NULL;
  stream = xrtWriterTakeFile(&file);
  if (stream == NULL) { error = xrtTakeError(); (void)xrtClose(file);
    xl6IoRestoreFailure(error); return NULL; }
  return xlir_io_writer_take(stream, NULL, NULL); }
