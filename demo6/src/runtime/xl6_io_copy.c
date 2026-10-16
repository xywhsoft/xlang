#include "xl6_io_private.inc"

XL6_TEXT_API int64_t xl6_xrt_io_copy(void* source, void* target, int64_t limit, int64_t mode)
{ xreader* r = xlir_io_reader_get(source); xwriter* w = xlir_io_writer_get(target);
  uint64_t done = 0; bool ok;
  if (r == NULL || w == NULL) return -1;
  if (mode < 0 || mode > 2 || (mode == 0 ? limit != -1 : limit < 0))
    { xrtSetErrorKind(XERR_RANGE); return -1; }
  ok = mode == 1 ? xrtReaderCopyN(r, w, (uint64_t)limit, &done) :
    mode == 2 ? xrtReaderCopyUpTo(r, w, (uint64_t)limit, &done) :
    xrtReaderCopy(r, w, &done);
  return ok ? xlir_io_position(done) : -1; }
