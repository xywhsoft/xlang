#include "xl6_io_private.h"

XL6_TEXT_API void* xl6_xrt_io_reader_stdin(void) { return xlir_io_reader_take(xrtReaderStdin()); }
