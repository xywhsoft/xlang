#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_DIR
#define XRT_MODULE_DIR_TEMP
#define XRT_MODULE_MEMORY_DEBUG
#include "../include/xrt_bz2.h"

#ifdef __TINYC__
#include "../../_bzip2/third_party/bzip2/blocksort.c"
#include "../../_bzip2/third_party/bzip2/huffman.c"
#include "../../_bzip2/third_party/bzip2/crctable.c"
#include "../../_bzip2/third_party/bzip2/randtable.c"
#include "../../_bzip2/third_party/bzip2/compress.c"
#include "../../_bzip2/third_party/bzip2/decompress.c"
#include "../../_bzip2/third_party/bzip2/bzlib.c"
#include "../xrt_bz2.c"
#include "../xrt_bz2_file.c"
#endif

#define CODEC_TEST_NAME "bz2"
#define CODEC_TEST_PREFIX ".xrt-bz2-"
#define CODEC_TEST_CLOSED 1
#define CODEC_TEST_COMPRESS(S, D, W, P, U) xrtBz2CompressFile((S), (D), NULL, (W), (P), (U))
#define CODEC_TEST_DECOMPRESS(S, D, W, P, U) xrtBz2DecompressFile((S), (D), NULL, (W), (P), (U))
#include "../../../../test/support/extlib_codec_file_rollback.h"
