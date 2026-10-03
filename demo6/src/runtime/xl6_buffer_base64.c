#include "src/runtime/xl6_buffer_internal.h"

static bool base64Config(int64_t flags, xstrview alphabet, int64_t size,
    xbase64config* config, char storage[65])
{
    if (flags < 0 || (uint64_t)flags > UINT32_MAX) {
        xrtSetErrorKind(XERR_RANGE); return false;
    }
    if (!xl6TextSelectRaw(&alphabet, size)) return false;
    if (alphabet.Size && alphabet.Size != 64) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    config->Flags = (uint32_t)flags;
    config->Alphabet = NULL;
    if (alphabet.Size) {
        memcpy(storage, alphabet.Data, 64); storage[64] = '\0';
        config->Alphabet = storage;
    }
    /* XRT validates the 64 exact ASCII bytes, uniqueness and flag conflicts.
     * A fixed stack sentinel bridges its C config ABI without strlen/heap. */
    return true;
}
void* xl6BufferFromBase64V1(xstrview text, int64_t flags, xstrview alphabet,
    int64_t size, int64_t alphabetSize)
{
    xbase64config config; char storage[65];
    if (!xl6TextSelectRaw(&text, size) || !base64Config(flags, alphabet, alphabetSize, &config, storage)) return NULL;
    xbuffer* result = xrtBufferFromBase64(text, &config);
    xl6BufferAllocation(result != NULL);
    return result;
}
xstrview xl6BufferBase64V1(void* raw, int64_t flags, xstrview alphabet,
    int64_t size, int64_t alphabetSize)
{
    xbase64config config; char storage[65]; xbytesview input; size_t output = 0;
    if (!xl6BufferInput(raw, size, &input) || !base64Config(flags, alphabet, alphabetSize, &config, storage) ||
        !xrtBase64Encode(input.Data, input.Size, NULL, 0, &output, &config)) return xl6TextFailure;
    if (output > INT64_MAX || output == SIZE_MAX) { xrtSetErrorKind(XERR_RANGE); return xl6TextFailure; }
    char* text = xrtMalloc(output + 1);
    if (!text) return xl6TextOwn(NULL, 0);
    if (!xrtBase64Encode(input.Data, input.Size, text, output + 1, &output, &config)) {
        xerror* failure = xrtTakeError();
        xrtFree(text); xrtClearError(); xrtSetErrorTake(failure);
        return xl6TextFailure;
    }
    return (xstrview){text, output};
}
