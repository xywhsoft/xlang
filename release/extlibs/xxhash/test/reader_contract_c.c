#define XRT_MODULE_MEMORY_DEBUG
#include "../include/xrt_xxhash.h"
#define HASH_TEST_NAME "xxhash"
#define HASH_TEST_VARIANTS 1u
#define HASH_TEST_MEMORY XRT_XXHASH_ERROR_MEMORY
#define HASH_TEST_IO XRT_XXHASH_ERROR_IO
#define HASH_TEST_CANCELLED XRT_XXHASH_ERROR_CANCELLED
#include "../../../../test/support/extlib_hash_reader_contract.h"

static bool test_hash_file(unsigned variant, xreader* reader, hash_output* output, xrtprogressproc progress, ptr user, hash_detail* detail)
{
    xrtxxhasherror error; (void)variant;
    bool ok = xrtXxhashFile64(reader, 0, NULL, &output->Word, progress, user, detail ? &error : NULL);
    if (detail) { detail->Code=error.iCode; detail->Backend=error.iBackendCode; detail->Offset=error.iInputOffset; memcpy(detail->Message,error.sMessage,sizeof(detail->Message)); }
    return ok;
}
static bool test_hash_memory(unsigned variant, const void* data, size_t size, hash_output* output)
{ (void)variant; return xrtXxhash64(data, size, 0, &output->Word, NULL); }

#include "../xrt_xxhash.c"
#include "../third_party/xxhash.c"
