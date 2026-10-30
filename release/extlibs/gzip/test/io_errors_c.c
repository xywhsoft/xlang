#define XRT_MODULE_MEMORY_DEBUG
#include "../include/xrt_gzip.h"
#define CODEC_IO_NAME "gzip"
#define CODEC_IO_API(name) xrtGzip##name
#define CODEC_IO_ERROR_CODE(name) XRT_GZIP_ERROR_##name
#define CODEC_IO_ERROR xrtgziperror
#define CODEC_IO_COMPRESSOR xrtgzipcompressor
#define CODEC_IO_DECOMPRESSOR xrtgzipdecompressor
#define CODEC_IO_READER_OUTPUT 0
/* Python gzip 的独立 RFC 1952 帧，明文 abc。 */
#define CODEC_IO_FRAME_BYTES {31,139,8,0,0,0,0,0,2,10,75,76,74,6,0,194,65,36,53,3,0,0,0}
#include "../../../../test/support/extlib_codec_io_errors.h"
#ifdef __TINYC__
#include "../xrt_gzip.c"
#endif
