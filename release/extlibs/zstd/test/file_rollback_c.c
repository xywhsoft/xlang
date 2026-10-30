/* 此桥接头使用 xstrview；宿主所需模块须在首次 XRT include 前选择。 */
#define XRT_MODULE_DIR
#ifndef XRT_MODULE_DIR_TEMP
#define XRT_MODULE_DIR_TEMP
#endif
#define XRT_MODULE_IO_FILE
#define XRT_MODULE_MEMORY_DEBUG
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#include "xruntime.h"
#include "../include/zstd_native.h"
#define NATIVE_CODEC_NAME "zstd"
#define NATIVE_CODEC_PREFIX ".xrt-zstd-"
#define NATIVE_CODEC_COMPRESS xzstd_compress_file
#define NATIVE_CODEC_DECOMPRESS xzstd_decompress_file
#include "../../../../test/support/extlib_native_codec_file_rollback.h"
