#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_DIR
#define XRT_MODULE_DIR_TEMP
#define XRT_MODULE_MEMORY_DEBUG
#include "../include/xrt_lz4.h"

#ifdef __TINYC__
#include "../lz4_hash.c"
#include "../third_party/lz4.c"
#include "../third_party/lz4hc.c"
#include "../lz4_frame.c"
#include "../xrt_lz4.c"
#include "../xrt_lz4_file.c"
#endif

#define CODEC_TEST_NAME "lz4"
#define CODEC_TEST_PREFIX ".xrt-lz4-"
#define CODEC_TEST_CLOSED 1
#define CODEC_TEST_COMPRESS(S, D, W, P, U) xrtLz4CompressFile((S), (D), NULL, (W), (P), (U))
#define CODEC_TEST_DECOMPRESS(S, D, W, P, U) xrtLz4DecompressFile((S), (D), NULL, (W), (P), (U))
#include "../../../../test/support/extlib_codec_file_rollback.h"
