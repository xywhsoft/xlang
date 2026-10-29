#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_DIR
#define XRT_MODULE_DIR_TEMP
#define XRT_MODULE_MEMORY_DEBUG
#include "../include/xrt_brotli.h"

#define CODEC_TEST_NAME "brotli"
#define CODEC_TEST_PREFIX ".xrt-brotli-"
#define CODEC_TEST_CLOSED 1
#define CODEC_TEST_COMPRESS(S, D, W, P, U) xrtBrotliCompressFile((S), (D), NULL, (W), (P), (U))
#define CODEC_TEST_DECOMPRESS(S, D, W, P, U) xrtBrotliDecompressFile((S), (D), NULL, (W), (P), (U))
#include "../../../../test/support/extlib_codec_file_rollback.h"
