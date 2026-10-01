#include "src/runtime/xl6_number_internal.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Fault {size_t calls, fail; bool hit;} Fault;
static void* allocate(void* context, size_t size)
{
    Fault* fault = context;
    if (++fault->calls >= fault->fail) {fault->hit = true; return NULL;}
    return malloc(size);
}
static void* resize(void* context, void* pointer, size_t size)
{
    Fault* fault = context;
    if (++fault->calls >= fault->fail) {fault->hit = true; return NULL;}
    return realloc(pointer, size);
}
static void release(void* context, void* pointer) {(void)context; free(pointer);}
static void check(xstrview text, const char* expected)
{
    size_t size = strlen(expected);
    assert(text.Data && text.Size == size && !memcmp(text.Data, expected, size + 1));
    xrtFree((void*)text.Data);
}
static void error(xerrkind kind)
{
    assert(xrtErrorKind(xrtGetError()) == kind); xrtClearError();
}
static void installDiagnostic(void)
{
    char message[8193]; memset(message, 'x', sizeof(message) - 1); message[8192] = 0;
    xerror* cause = xrtErrorCreate(XERR_IO, "xrt.test", 7, "cause"); assert(cause);
    xerrordesc description = {0};
    description.Kind = XERR_PROTOCOL; description.Code = XNUMBER_ERROR_FORMAT;
    description.SystemCode = 9; description.Domain = "xrt.number";
    description.Operation = "int-parse"; description.Message = message;
    description.Data = "byte=3"; description.Cause = cause;
    xerrorlocation location = {"numeric.xl", 4, 2};
    xerror* original = xrtErrorBuildAt(&description, &location); assert(original);
    xrtErrorFree(cause); xrtSetErrorTake(original);
}
static void checkDiagnostic(void)
{
    const xerror* e = xrtGetError();
    assert(xrtErrorKind(e) == XERR_VALUE && xrtErrorCode(e) == XNUMBER_ERROR_FORMAT);
    assert(xrtErrorSystemCode(e) == 9 && !strcmp(xrtErrorDomain(e), "xrt.number"));
    assert(!strcmp(xrtErrorOperation(e), "int-parse") && strlen(xrtErrorMessage(e)) == 8192);
    assert(!strcmp(xrtErrorData(e), "byte=3") && !strcmp(xrtErrorFile(e), "numeric.xl"));
    assert(xrtErrorLine(e) == 4 && xrtErrorColumn(e) == 2);
    assert(xrtErrorKind(xrtErrorCause(e)) == XERR_IO && xrtErrorCode(xrtErrorCause(e)) == 7);
}
int main(int argc, char** argv)
{
    static Fault fault;
    fault.fail = SIZE_MAX;
    xallocator allocator = {&fault, allocate, resize, release};
    assert(xrtSetAllocator(&allocator));
    if (argc == 3) {
        size_t mode = (size_t)strtoull(argv[1], NULL, 10);
        size_t point = (size_t)strtoull(argv[2], NULL, 10);
        xbuffer* buffer = NULL; size_t prefix = 0;
        if (mode >= 3 && mode <= 5) {
            buffer = xrtBufferCreate(); assert(buffer);
            assert(xrtBufferAppend(buffer, (xbytesview){(const uint8*)"prefix", 6}));
            prefix = buffer->Capacity; assert(xrtBufferResize(buffer, prefix));
            memset(buffer->Data, 'p', prefix);
        }
        if (mode == 6) installDiagnostic();
        fault.calls = 0; fault.fail = point;
        xstrview text = {NULL, 0}; bool ok = false;
        if (mode == 0) text = xl6NumberIntStringV1(INT64_MIN, 2, 0);
        else if (mode == 1) text = xl6NumberUIntStringV1(UINT64_MAX, 2, 0);
        else if (mode == 2) text = xl6NumberFloatStringV1(DBL_MAX, 0);
        else if (mode == 3) ok = xl6NumberBufferIntStringV1(buffer, INT64_MIN, 2, 0);
        else if (mode == 4) ok = xl6NumberBufferUIntStringV1(buffer, UINT64_MAX, 2, 0);
        else if (mode == 5) ok = xl6NumberBufferFloatStringV1(buffer, DBL_MAX, 0);
        else if (mode == 6) xl6NumberParseError();
        else if (mode == 7) assert(xl6NumberIntParseV1(XRT_STR_LITERAL("invalid"), 10, 0, -1) == 0);
        else {assert(mode == 8); assert(xl6NumberUInt8ParseV1(XRT_STR_LITERAL("256"), 10, 0, -1) == 0);}
        if (mode < 3) ok = text.Data != NULL;
        if (fault.hit) {
            assert(!ok && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            assert(!text.Data && text.Size == 0);
            if (buffer) {
                assert(buffer->Size == prefix);
                for (size_t i=0; i<prefix; ++i) assert(buffer->Data[i] == 'p');
            }
        } else if (mode < 6) assert(ok);
        else if (mode == 6) checkDiagnostic();
        else assert(xrtErrorKind(xrtGetError()) == (mode == 7 ? XERR_VALUE : XERR_RANGE));
        fault.fail = SIZE_MAX;
        xrtFree((void*)text.Data); xrtBufferDestroy(buffer); xrtClearError();
        assert(xrtMemDebugReset());
        printf("numeric value mode=%zu point=%zu calls=%zu hit=%d balanced\n",mode,point,fault.calls,fault.hit);
        return 0;
    }
    assert(argc == 1);
    size_t allocations = fault.calls;
    for (int i=0; i<10000; ++i) {
        assert(xl6NumberIntParseV1(XRT_STR_LITERAL("-9223372036854775808"), 10, 0, -1) == INT64_MIN);
        assert(xl6NumberUIntParseV1(XRT_STR_LITERAL("ffffffffffffffff"), 16, 0, -1) == UINT64_MAX);
        assert(xl6NumberFloatParseV1(XRT_STR_LITERAL("0.1"), 0, -1) == 0.1);
    }
    assert(fault.calls == allocations); /* Successful scalar parses do not allocate. */
    uint64_t state = UINT64_C(0xbed3e914459fe28a);
    for (int i=0; i<4096; ++i) {
        state ^= state << 13; state ^= state >> 7; state ^= state << 17;
        double value; memcpy(&value, &state, sizeof(value));
        if (!isfinite(value)) continue;
        char data[40]; size_t written;
        assert(xrtNumWrite(value, data, sizeof(data), &written, 0));
        double parsed = xl6NumberFloatParseV1((xstrview){data, written}, 0, -1);
        uint64_t bits; memcpy(&bits, &parsed, sizeof(bits)); assert(bits == state);
    }
    assert(fault.calls == allocations);
    assert(xl6NumberInt8ParseV1(XRT_STR_LITERAL("-128"), 10, 0, -1) == -128);
    assert(xl6NumberInt16ParseV1(XRT_STR_LITERAL("32767"), 10, 0, -1) == 32767);
    assert(xl6NumberInt32ParseV1(XRT_STR_LITERAL("2147483647"), 10, 0, -1) == INT32_MAX);
    assert(xl6NumberInt64ParseV1(XRT_STR_LITERAL("-9223372036854775808"), 10, 0, -1) == INT64_MIN);
    assert(xl6NumberUInt8ParseV1(XRT_STR_LITERAL("255"), 10, 0, -1) == 255);
    assert(xl6NumberUInt16ParseV1(XRT_STR_LITERAL("65535"), 10, 0, -1) == 65535);
    assert(xl6NumberUInt32ParseV1(XRT_STR_LITERAL("4294967295"), 10, 0, -1) == UINT32_MAX);
    assert(xl6NumberIntParseV1(XRT_STR_LITERAL("42\0tail"), 10, 0, 2) == 42);
    assert(xl6NumberIntParseV1(XRT_STR_LITERAL("42"), 10, 0, INT64_MAX) == 42);
    assert(xl6NumberIntParseV1(XRT_STR_LITERAL("0xff"), 0, 2, -1) == 255);
    assert(xl6NumberFloatParseV1(XRT_STR_LITERAL(" 1_2.5 "), 5, -1) == 12.5);
    assert(xl6NumberIntParseV1(XRT_STR_LITERAL("1\0"), 10, 0, -1) == 0); error(XERR_VALUE);
    assert(xl6NumberIntParseV1((xstrview){NULL, 1}, 10, 0, 0) == 0); error(XERR_ARGUMENT);
    assert(xl6NumberIntParseV1(XRT_STR_LITERAL("1"), 10, 0, -2) == 0); error(XERR_RANGE);
    assert(xl6NumberUInt8ParseV1(XRT_STR_LITERAL("256"), 10, 0, -1) == 0); error(XERR_RANGE);
    assert(xl6NumberInt8ParseV1(XRT_STR_LITERAL("-129"), 10, 0, -1) == 0); error(XERR_RANGE);
    assert(xl6NumberIntParseV1(XRT_STR_LITERAL("1"), -1, 0, -1) == 0); error(XERR_VALUE);
    assert(xl6NumberFloatParseV1(XRT_STR_LITERAL("1"), INT64_MAX, -1) == 0); error(XERR_VALUE);
    assert(xl6NumberFloatParseV1(XRT_STR_LITERAL("nan"), 0, -1) == 0); error(XERR_VALUE);
    check(xl6NumberIntStringV1(INT64_MIN, 10, 0), "-9223372036854775808");
    check(xl6NumberUIntStringV1(UINT64_MAX, 16, 3), "0XFFFFFFFFFFFFFFFF");
    check(xl6NumberFloatStringV1(-0.0, 0), "-0.0");
    xbuffer* buffer = xrtBufferCreate(); assert(buffer && xrtBufferReserve(buffer, 4096));
    allocations = fault.calls;
    assert(xl6NumberBufferIntStringV1(buffer, -10, 10, 0));
    assert(xl6NumberBufferUIntStringV1(buffer, UINT64_MAX, 16, 0));
    assert(xl6NumberBufferFloatStringV1(buffer, -0.0, 0));
    assert(fault.calls == allocations && buffer->Size == 23);
    assert(!memcmp(buffer->Data, "-10ffffffffffffffff-0.0", 23));
    assert(!xl6NumberBufferIntStringV1(buffer, 1, 1, 0)); error(XERR_VALUE);
    assert(buffer->Size == 23);
    assert(!xl6NumberBufferFloatStringV1(NULL, 1, 0)); error(XERR_ARGUMENT);
    xrtBufferDestroy(buffer);
    installDiagnostic(); xl6NumberParseError(); checkDiagnostic(); xrtClearError();
    assert(xrtMemDebugReset());
    puts("number values: scalar widths, exact views, allocation-free parse/append, float roundtrip, diagnostics passed");
    return 0;
}
