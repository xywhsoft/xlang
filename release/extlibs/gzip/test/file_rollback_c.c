#include "../include/gzip_native.h"
#define NATIVE_CODEC_NAME "gzip"
#define NATIVE_CODEC_PREFIX ".xrt-gzip-"
/* 额外的 MTIME 由编译期补零，不增设运行时包装。 */
#define NATIVE_CODEC_COMPRESS(source, target, level, overwrite, maxInput, maxOutput, callback) \
    xgzip_compress_file(source, target, level, 0, overwrite, maxInput, maxOutput, callback)
#define NATIVE_CODEC_DECOMPRESS xgzip_decompress_file
#include "../../../../test/support/extlib_native_codec_file_rollback.h"
