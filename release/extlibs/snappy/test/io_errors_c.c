#define XRT_MODULE_MEMORY_DEBUG
#include "../include/xrt_snappy.h"
#define CODEC_IO_NAME "snappy"
#define CODEC_IO_API(name) xrtSnappy##name
#define CODEC_IO_ERROR_CODE(name) XRT_SNAPPY_ERROR_##name
#define CODEC_IO_ERROR xrtsnappyerror
#define CODEC_IO_COMPRESSOR xrtsnappycompressor
#define CODEC_IO_DECOMPRESSOR xrtsnappydecompressor
#define CODEC_IO_READER_OUTPUT 10
#define CODEC_IO_FRAME_BYTES {255,6,0,0,'s','N','a','P','p','Y',1,7,0,0,110,87,241,33,'a','b','c'}
#include "../../../../test/support/extlib_codec_io_errors.h"
#ifdef __TINYC__
#include "../third_party/xrt_snappy.c"
#include "../xrt_snappy.c"
#endif
