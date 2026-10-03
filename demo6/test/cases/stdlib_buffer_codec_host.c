#include "src/runtime/xl6_text_value.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void textEqual(xstrview result, xstrview expected)
{
    assert(result.Data && result.Size == expected.Size && result.Data[result.Size] == 0);
    assert(!result.Size || !memcmp(result.Data, expected.Data, result.Size));
    xrtFree((void*)result.Data);
}
static void bytesEqual(xbuffer* result, const unsigned char* expected, size_t size)
{
    assert(result && result->Size == size && (!size || !memcmp(result->Data, expected, size)));
    xrtBufferDestroy(result);
}
static void refusal(xerrkind kind, int64_t code)
{
    if (xrtErrorKind(xrtGetError()) != kind || (code && xrtErrorCode(xrtGetError()) != code))
        fprintf(stderr, "refusal expected kind=%d code=%lld, actual kind=%d code=%d\n",
            (int)kind, (long long)code, (int)xrtErrorKind(xrtGetError()), xrtErrorCode(xrtGetError()));
    assert(xrtErrorKind(xrtGetError()) == kind);
    if (code) assert(xrtErrorCode(xrtGetError()) == code);
    xrtClearError();
}
static void memoryFault(const char* operation, size_t point)
{
    assert(xrtMemDebugEnable(true));
    xmemdebugsnapshot before, after;
    xrtMemDebugSnapshot(&before);
    const unsigned char data[] = {0, 255, 65, 90};
    xbuffer* source = xrtBufferCreate(); assert(source && xrtBufferAssign(source, (xbytesview){data,sizeof(data)}));
    const unsigned char* previous = source->Data;
    size_t capacity = source->Capacity;
    assert(xrtMemDebugFailAfter(point));
    xstrview text = {NULL, 0};
    xbuffer* result = NULL;
    if (!strcmp(operation, "hex")) text = xl6BufferHexV1(source, 0, -1);
    else if (!strcmp(operation, "base64")) text = xl6BufferBase64V1(source, 0, (xstrview){NULL, 0}, -1, -1);
    else if (!strcmp(operation, "from-hex")) result = xl6BufferFromHexV1(XRT_STR_LITERAL("00ff415a"), 0, -1);
    else if (!strcmp(operation, "from-base64")) result = xl6BufferFromBase64V1(XRT_STR_LITERAL("AP9BWg=="), 0, (xstrview){NULL, 0}, -1, -1);
    else abort();
    bool hit = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    assert(hit ? !result && !text.Data && xrtErrorKind(xrtGetError()) == XERR_MEMORY
               : (result || text.Data) && !xrtGetError());
    assert(source->Data == previous && source->Capacity == capacity && source->Size == sizeof(data));
    assert(!memcmp(source->Data, data, sizeof(data)));
    xerror* failure = xrtTakeError();
    xrtBufferDestroy(result); xrtFree((void*)text.Data); xrtBufferDestroy(source);
    xrtErrorFree(failure); xrtClearError();
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
    assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
    assert(after.InvalidFreeCount == before.InvalidFreeCount && after.DoubleFreeCount == before.DoubleFreeCount);
    assert(after.UseAfterFreeCount == before.UseAfterFreeCount);
    printf("buffer codec %s point=%zu hit=%d balanced\n", operation, point, hit);
}
int main(int argc, char** argv)
{
    if (argc == 3) { memoryFault(argv[1], (size_t)strtoull(argv[2], NULL, 10)); return 0; }
    assert(argc == 1);
    const unsigned char data[] = {0, 255, 65, 90};
    xbuffer* source = xrtBufferCreate(); assert(source && xrtBufferAssign(source, (xbytesview){data,sizeof(data)}));
    textEqual(xl6BufferHexV1(source, 0, -1), XRT_STR_LITERAL("00ff415a"));
    textEqual(xl6BufferHexV1(source, 1, 1), XRT_STR_LITERAL("00"));
    textEqual(xl6BufferBase64V1(source, 0, (xstrview){NULL, 0}, 99, -1), XRT_STR_LITERAL("AP9BWg=="));
    /* No sentinel: public xlang views are exact bytes, not C strings. */
    char hex[] = {'0','0','f','f','4','1','5','a'};
    char encoded[] = {'A','P','9','B','W','g','=','='};
    bytesEqual(xl6BufferFromHexV1((xstrview){hex,sizeof(hex)}, 0, 99), data, sizeof(data));
    bytesEqual(xl6BufferFromBase64V1((xstrview){encoded,sizeof(encoded)}, 0, (xstrview){NULL,0}, -1, -1), data, sizeof(data));
    char alphabet[64]; memcpy(alphabet, "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz+/", sizeof(alphabet));
    xstrview custom = xl6BufferBase64V1(source, 0, (xstrview){alphabet,sizeof(alphabet)}, -1, 99);
    assert(custom.Data);
    bytesEqual(xl6BufferFromBase64V1(custom, 0, (xstrview){alphabet,sizeof(alphabet)}, -1, -1), data, sizeof(data));
    xrtFree((void*)custom.Data);
    assert(!xl6BufferFromHexV1(XRT_STR_LITERAL("00\0"), 0, -1)); refusal(XERR_PROTOCOL, 902);
    assert(!xl6BufferFromBase64V1(XRT_STR_LITERAL("QR=="), 0, (xstrview){NULL,0}, -1, -1)); refusal(XERR_PROTOCOL, 1002);
    assert(!xl6BufferFromHexV1((xstrview){NULL,2}, 0, 0)); refusal(XERR_ARGUMENT, 0);
    assert(!xl6BufferFromHexV1(XRT_STR_LITERAL("00"), (int64_t)UINT32_MAX+1, -1)); refusal(XERR_RANGE, 0);
    assert(!xl6BufferHexV1(source, 2, -1).Data); refusal(XERR_VALUE, 901);
    assert(!xl6BufferBase64V1(source, 4, (xstrview){NULL,0}, -1, -1).Data); refusal(XERR_VALUE, 1001);
    assert(!xl6BufferBase64V1(source, 0, (xstrview){NULL,64}, -1, -1).Data); refusal(XERR_ARGUMENT, 0);
    alphabet[0] = 0;
    assert(!xl6BufferBase64V1(source, 0, (xstrview){alphabet,64}, -1, -1).Data); refusal(XERR_VALUE, 1001);
    alphabet[0] = alphabet[1];
    assert(!xl6BufferBase64V1(source, 0, (xstrview){alphabet,64}, -1, -1).Data); refusal(XERR_VALUE, 1001);
    alphabet[0] = '0';
    assert(!xl6BufferBase64V1(source, 1, (xstrview){alphabet,64}, -1, -1).Data); refusal(XERR_VALUE, 1001);
    assert(!xl6BufferBase64V1(source, 0, (xstrview){alphabet,64}, -1, -2).Data); refusal(XERR_RANGE, 0);
    xbuffer malformed = {0}; malformed.Capacity = 8;
    assert(!xl6BufferHexV1(&malformed, 0, -1).Data); refusal(XERR_ARGUMENT, 0);
    assert(!memcmp(source->Data, data, sizeof(data)) && source->Size == sizeof(data));
    xrtBufferDestroy(source); assert(xrtMemDebugReset());
    puts("Buffer exact codec native vectors/refusals ok");
    return 0;
}
