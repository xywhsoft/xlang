#define XRT_MODULE_MEMORY_DEBUG
#include "../include/xrt_blake3.h"
#define HASH_TEST_NAME "blake3"
#define HASH_TEST_VARIANTS 1u
#define HASH_TEST_MEMORY XRT_BLAKE3_ERROR_MEMORY
#define HASH_TEST_IO XRT_BLAKE3_ERROR_IO
#define HASH_TEST_CANCELLED XRT_BLAKE3_ERROR_CANCELLED
#include "../../../../test/support/extlib_hash_reader_contract.h"

static bool test_hash_file(unsigned variant, xreader* reader, hash_output* output, xrtprogressproc progress, ptr user, hash_detail* detail)
{
    xrtblake3error error; (void)variant;
    bool ok = xrtBlake3File(reader, NULL, output->Bytes, 65, progress, user, detail ? &error : NULL);
    if (detail) { detail->Code=error.iCode; detail->Backend=error.iBackendCode; detail->Offset=error.iInputOffset; memcpy(detail->Message,error.sMessage,sizeof(detail->Message)); }
    return ok;
}
static bool test_hash_memory(unsigned variant, const void* data, size_t size, hash_output* output)
{ (void)variant; return xrtBlake3Hash(data,size,output->Bytes,65,NULL); }

#include "../blake3_portable.c"
#include "../xrt_blake3.c"
