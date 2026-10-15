#include "xl6_io_private.h"

XL6_TEXT_API void* xl6_xrt_io_writer_standard(int64_t stream)
{ if (stream < 1 || stream > 3) { xrtSetErrorKind(XERR_RANGE); return NULL; }
  return xlir_io_writer_take(stream == 1 ? xrtWriterStdout() : stream == 2 ? xrtWriterStderr() : xrtWriterDiscard(), NULL, NULL); }
