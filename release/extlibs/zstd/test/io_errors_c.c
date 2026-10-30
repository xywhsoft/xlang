#define XRT_MODULE_MEMORY_DEBUG
#include "../include/xrt_zstd.h"
#define CODEC_IO_NAME "zstd"
#define CODEC_IO_API(name) xrtZstd##name
#define CODEC_IO_ERROR_CODE(name) XRT_ZSTD_ERROR_##name
#define CODEC_IO_ERROR xrtzstderror
#define CODEC_IO_COMPRESSOR xrtzstdcompressor
#define CODEC_IO_DECOMPRESSOR xrtzstddecompressor
#define CODEC_IO_READER_OUTPUT 0
/* 单段 raw block 的标准帧，明文 abc，无需用本包编码器自证。 */
#define CODEC_IO_FRAME_BYTES {0x28,0xb5,0x2f,0xfd,0x20,3,0x19,0,0,'a','b','c'}
#include "../../../../test/support/extlib_codec_io_errors.h"
#ifdef __TINYC__
#include "../third_party/zstd.c"
#include "../xrt_zstd.c"
#endif
