#include "xl6_io_private.h"

XL6_TEXT_API void* xl6_xrt_io_reader_open(xstrview path)
{ const char* name = xl6IoPath(path); return name != NULL ? xlir_io_reader_take(xrtReaderOpen(name)) : NULL; }
