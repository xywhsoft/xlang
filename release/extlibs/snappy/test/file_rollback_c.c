/* 只绑定真实文件 ABI；Snappy 没有压缩级别，不引入运行时适配层。 */
#include "../include/snappy_native.h"
#define NATIVE_CODEC_NAME "snappy"
#define NATIVE_CODEC_PREFIX ".xrt-snappy-"
#define NATIVE_CODEC_COMPRESS(source, target, level, overwrite, input, output, callback) \
    xsnappy_compress_file(source, target, overwrite, input, output, callback)
#define NATIVE_CODEC_DECOMPRESS xsnappy_decompress_file
#include "../../../../test/support/extlib_native_codec_file_rollback.h"
