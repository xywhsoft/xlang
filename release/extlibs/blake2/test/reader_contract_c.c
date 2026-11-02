#define XRT_MODULE_MEMORY_DEBUG
#include "../include/xrt_blake2.h"
#define HASH_TEST_NAME "blake2"
#define HASH_TEST_VARIANTS 2u
#define HASH_TEST_MEMORY XRT_BLAKE2_ERROR_MEMORY
#define HASH_TEST_IO XRT_BLAKE2_ERROR_IO
#define HASH_TEST_CANCELLED XRT_BLAKE2_ERROR_CANCELLED
#include "../../../../test/support/extlib_hash_reader_contract.h"

static bool test_hash_file(unsigned variant, xreader* reader, hash_output* output, xrtprogressproc progress, ptr user, hash_detail* detail)
{
    xrtblake2error error;
    bool ok = variant == 0 ? xrtBlake2bFile(reader, NULL, 64, output->Bytes, progress, user, detail ? &error : NULL) :
        xrtBlake2sFile(reader, NULL, 32, output->Bytes, progress, user, detail ? &error : NULL);
    if (detail) { detail->Code=error.iCode; detail->Backend=error.iBackendCode; detail->Offset=error.iInputOffset; memcpy(detail->Message,error.sMessage,sizeof(detail->Message)); }
    return ok;
}
static bool test_hash_memory(unsigned variant, const void* data, size_t size, hash_output* output)
{ return variant == 0 ? xrtBlake2bHash(data,size,64,NULL,0,output->Bytes,NULL) : xrtBlake2sHash(data,size,32,NULL,0,output->Bytes,NULL); }

#include "../third_party/blake2.c"
#include "../xrt_blake2.c"
