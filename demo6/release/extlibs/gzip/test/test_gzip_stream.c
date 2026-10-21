#include "../include/xrt_gzip.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    xbytesview Data;
    size_t Position;
    unsigned int Closed;
    bool Fail;
    xrtgzipcompressor* Reenter;
} io_state;

static bool close_io(ptr user) { ((io_state*)user)->Closed++; return true; }

static bool read_io(ptr user, ptr buffer, size_t request, size_t* count)
{
    io_state* s = user;
    if (s->Fail) {
        xrtSetErrorInfo(XERR_IO, "test.reader", 123, "reader marker");
        return false;
    }
    *count = s->Data.Size - s->Position;
    if (*count > 7) *count = 7;
    if (*count > request) *count = request;
    if (*count) memcpy(buffer, s->Data.Data + s->Position, *count);
    s->Position += *count;
    return true;
}

static bool write_io(ptr user, const void* data, size_t request, size_t* count)
{
    io_state* s = user;
    (void)data;
    if (s->Fail) {
        xrtSetErrorInfo(XERR_IO, "test.writer", 456, "writer marker");
        return false;
    }
    if (s->Reenter) {
        // 不能在借用回调中释放正被调用的对象，即使回调随后清空错误。
        xrtGzipCompressorDestroy(s->Reenter);
        xrtClearError();
    }
    *count = request > 3 ? 3 : request;
    s->Position += *count;
    return true;
}

typedef struct {
    uint64 Input, Output, Total;
    unsigned int Calls, Finals;
    bool Known, Cancel, CancelFinal, Fail;
} progress_state;

static bool progress(const xrtprogress* value, ptr user)
{
    progress_state* s = user;
    assert(value->iSize == sizeof(*value));
    assert(value->iInputBytes >= s->Input && value->iOutputBytes >= s->Output);
    assert(!!(value->iFlags & XRT_PROGRESS_TOTAL_KNOWN) == s->Known);
    assert(value->iTotalInputBytes == s->Total);
    s->Input = value->iInputBytes;
    s->Output = value->iOutputBytes;
    s->Calls++;
    if (value->iFlags & XRT_PROGRESS_FINAL) s->Finals++;
    if (s->Fail) {
        xrtSetErrorInfo(XERR_ARGUMENT, "test.progress", 789, "callback marker");
        return false;
    }
    return !s->Cancel && !(s->CancelFinal && s->Finals);
}

static void streams(void)
{
    const uint8 text[] = "short reader and writer boundaries";
    xreaderops rops = {0};
    xwriterops wops = {0};
    io_state input = {0}, output = {0};
    progress_state p = {0};
    xrtgziperror error;
    xreader* reader;
    xwriter* writer;
    rops.Read = read_io; rops.Close = close_io;
    wops.Write = write_io; wops.Close = close_io;
    input.Data = (xbytesview){text, sizeof(text) - 1};
    reader = xrtReaderCreate(&rops, &input);
    writer = xrtWriterCreate(&wops, &output);
    assert(reader && writer && xrtGzipCompressStream(reader, writer, NULL, progress, &p, &error));
    assert(p.Calls > 1 && p.Finals == 1 && p.Input == input.Data.Size && p.Output == output.Position);
    assert(input.Closed == 0 && output.Closed == 0);
    assert(xrtReaderDestroy(reader) && xrtWriterDestroy(writer));
    assert(input.Closed == 1 && output.Closed == 1);

    input.Position = 0; input.Fail = true;
    reader = xrtReaderCreate(&rops, &input);
    writer = xrtWriterCreate(&wops, &output);
    assert(!xrtGzipCompressStream(reader, writer, NULL, NULL, NULL, &error));
    assert(error.iBackendCode == 123 && strcmp(xrtErrorMessage(xrtGetError()), "reader marker") == 0);
    assert(xrtReaderDestroy(reader) && xrtWriterDestroy(writer));

    input.Position = 0; input.Fail = false; output.Fail = true;
    reader = xrtReaderCreate(&rops, &input);
    writer = xrtWriterCreate(&wops, &output);
    assert(!xrtGzipCompressStream(reader, writer, NULL, NULL, NULL, &error));
    assert(error.iBackendCode == 456 && strcmp(xrtErrorMessage(xrtGetError()), "writer marker") == 0);
    assert(xrtReaderDestroy(reader) && xrtWriterDestroy(writer));

    // 已知总长取剩余区间，取消和最终取消都返回失败。
    {
        unsigned int mode;
        for (mode = 0; mode < 4; ++mode) {
            xbuffer* packed = xrtBufferCreate();
            memset(&p, 0, sizeof(p));
            p.Known = true; p.Total = sizeof(text) - 4;
            p.Cancel = mode == 1; p.CancelFinal = mode == 2; p.Fail = mode == 3;
            reader = xrtReaderFromMemory((xbytesview){text, sizeof(text) - 1});
            writer = xrtWriterFromBuffer(packed);
            assert(reader && writer && xrtReaderSeek(reader, 3, XSEEK_START, NULL));
            assert(xrtGzipCompressStream(reader, writer, NULL, progress, &p, &error) == (mode == 0));
            if (mode == 1 || mode == 2) assert(error.iCode == XRT_GZIP_ERROR_CANCELLED);
            if (mode == 3) assert(error.iBackendCode == 789 && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
            if (mode == 0) {
                xbuffer* plain = xrtBufferCreate();
                xreader* r = xrtReaderFromBuffer(packed);
                xwriter* w = xrtWriterFromBuffer(plain);
                memset(&p, 0, sizeof(p)); p.Known = true; p.Total = packed->Size;
                assert(xrtGzipDecompressStream(r, w, NULL, progress, &p, &error));
                assert(p.Finals == 1 && p.Output == sizeof(text) - 4);
                assert(plain->Size == sizeof(text) - 4 && memcmp(plain->Data, text + 3, plain->Size) == 0);
                assert(xrtReaderDestroy(r) && xrtWriterDestroy(w));
                xrtBufferDestroy(plain);
            }
            assert(xrtReaderDestroy(reader) && xrtWriterDestroy(writer));
            xrtBufferDestroy(packed);
        }
    }
    // 重入会终止流，但不提前释放对象；外层回来后才可正常销毁。
    output.Fail = false; output.Reenter = xrtGzipCompressorCreate(NULL);
    writer = xrtWriterCreate(&wops, &output);
    assert(output.Reenter && writer && xrtGzipCompressorFinish(output.Reenter));
    assert(!xrtGzipCompressorDrain(output.Reenter, writer));
    assert(xrtGzipCompressorError(output.Reenter)->iCode == XRT_GZIP_ERROR_STATE);
    assert(xrtGetError() != NULL && !xrtGzipCompressorFinished(output.Reenter));
    assert(!xrtGzipCompressorDrainBuffer(output.Reenter));
    xrtGzipCompressorDestroy(output.Reenter);
    assert(xrtWriterDestroy(writer));
    xrtClearError();
}

int main(void)
{
    streams();
    puts("gzip C streams: progress, cancellation, ownership, IO and reentry passed");
    return 0;
}
