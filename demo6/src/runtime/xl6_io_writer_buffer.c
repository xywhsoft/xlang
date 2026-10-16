#include "xl6_io_private.inc"

static void xlir_io_buffer_drop(void* owner) { xrtBufferDestroy((xbuffer*)owner); }
XL6_TEXT_API void* xl6_xrt_io_writer_buffer(void)
{ xbuffer* buffer = xrtBufferCreate(); if (buffer == NULL) return NULL;
  return xlir_io_writer_take(xrtWriterFromBuffer(buffer), buffer, xlir_io_buffer_drop); }

XL6_TEXT_API xvalue* xl6_xrt_io_writer_to_bytes(void* handle)
{ xlir_io_writer* w = (xlir_io_writer*)handle;
  if (w == NULL || w->owner == NULL || w->drop == NULL) { xrtSetErrorKind(XERR_STATE); return NULL; }
  return xrtValueBytes(xrtBufferView((xbuffer*)w->owner)); }
