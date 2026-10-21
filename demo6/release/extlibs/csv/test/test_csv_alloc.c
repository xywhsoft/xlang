#define XRT_MODULE_MEMORY_DEBUG
#include "../include/csv_native.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void unchanged(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    if (after.LiveBytes != before->LiveBytes || after.LiveCount != before->LiveCount)
        fprintf(stderr, "allocation balance: %llu/%llu -> %llu/%llu\n",
            (unsigned long long)before->LiveCount, (unsigned long long)before->LiveBytes,
            (unsigned long long)after.LiveCount, (unsigned long long)after.LiveBytes);
    assert(after.LiveBytes == before->LiveBytes && after.LiveCount == before->LiveCount);
    assert(after.InvalidFreeCount == before->InvalidFreeCount && after.DoubleFreeCount == before->DoubleFreeCount);
    assert(after.OverflowCount == before->OverflowCount && after.UnderflowCount == before->UnderflowCount);
}

/* 同时遍历正常结果和错误对象构造的分配点，不能只在成功路径上检查释放。 */
static unsigned int reader_failures(bool malformed)
{
    char text[2048];
    xmemdebugsnapshot before;
    unsigned int rejected = 0, complete = 0;
    memset(text, 'a', 700);
    strcpy(text + 700, ",b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r\n");
    if (malformed) strcpy(text, "\"broken\"x");
    xrtMemDebugSnapshot(&before);
    for (unsigned int i = 0; i < 512 && complete < 2; ++i) {
        void* reader;
        str field = NULL;
        bool ok = false, injected;
        assert(xrtMemDebugFailAfter(i));
        reader = xcsv_reader_open_memory(text, strlen(text), ",", "\"", true, 0, 0, 0, 0, 0);
        if (reader && xcsv_reader_next(reader)) {
            field = xcsv_reader_field(reader, 0);
            ok = field != NULL;
        }
        injected = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (injected) {
            if (ok || xrtErrorKind(xrtGetError()) != XERR_MEMORY)
                fprintf(stderr, "reader malformed=%d allocation=%u kind=%d\n", malformed, i, xrtErrorKind(xrtGetError()));
            assert(!ok && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            ++rejected;
        } else {
            if (malformed) {
                assert(!ok && !strcmp(xrtErrorData(xrtGetError()), "CsvError"));
                assert(xrtErrorCode(xrtErrorCause(xrtGetError())) == XRT_CSV_ERROR_FORMAT);
            } else {
                assert(ok && strlen(field) == 700 && !xrtGetError());
            }
            ++complete;
        }
        xcsv_reader_destroy(reader);
        xrtFree(field);
        unchanged(&before);
    }
    assert(complete == 2 && rejected > 0);
    return rejected;
}

static unsigned int writer_failures(bool malformed)
{
    xmemdebugsnapshot before;
    unsigned int rejected = 0, complete = 0;
    xrtMemDebugSnapshot(&before);
    for (unsigned int i = 0; i < 512 && complete < 2; ++i) {
        void* writer;
        xbuffer* output = NULL;
        bool ok, injected;
        assert(xrtMemDebugFailAfter(i));
        writer = xcsv_writer_create_memory(",", "\"", 0, false, true, 0, 0, 0, 0, 0);
        ok = writer != NULL && xcsv_writer_begin(writer);
        for (unsigned int j = 0; ok && j < 40; ++j)
            ok = xcsv_writer_field(writer, malformed ? "\xc0" : "word,\"\r\n");
        if (ok) ok = xcsv_writer_end(writer);
        if (ok) { output = xcsv_writer_take_buffer(writer); ok = output != NULL; }
        injected = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (injected) {
            if (ok || xrtErrorKind(xrtGetError()) != XERR_MEMORY)
                fprintf(stderr, "writer malformed=%d allocation=%u kind=%d\n", malformed, i, xrtErrorKind(xrtGetError()));
            assert(!ok && output == NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            ++rejected;
        } else {
            if (malformed) assert(!ok && !strcmp(xrtErrorData(xrtGetError()), "CsvError"));
            else assert(ok && output->Size > 3 && !xrtGetError());
            ++complete;
        }
        xcsv_writer_destroy(writer);
        xrtBufferDestroy(output);
        unchanged(&before);
    }
    assert(complete == 2 && rejected > 0);
    return rejected;
}

static bool empty_read(ptr context, ptr data, size_t request, size_t* count)
{
    (void)context; (void)data; (void)request;
    *count = 0;
    return true;
}

static bool empty_write(ptr context, const void* data, size_t request, size_t* count)
{
    (void)context; (void)data;
    *count = request;
    return true;
}

static bool close_count(ptr context)
{
    ++*(unsigned int*)context;
    return true;
}

/* 拥有型流在构造失败时也只释放一次；借用流始终由调用者释放。 */
static unsigned int construction_ownership(bool writing, bool take)
{
    unsigned int rejected = 0, complete = 0;
    xmemdebugsnapshot before;
    xreaderops rops = {0};
    xwriterops wops = {0};
    rops.Read = empty_read; rops.Close = close_count;
    wops.Write = empty_write; wops.Close = close_count;
    xrtMemDebugSnapshot(&before);
    for (unsigned int i = 0; i < 64 && complete < 2; ++i) {
        unsigned int closed = 0;
        xreader* input = writing ? NULL : xrtReaderCreate(&rops, &closed);
        xwriter* output = writing ? xrtWriterCreate(&wops, &closed) : NULL;
        xrtcsvreader* reader = NULL;
        xrtcsvwriter* writer = NULL;
        bool injected;
        assert(input || output);
        assert(xrtMemDebugFailAfter(i));
        if (writing) writer = xrtCsvWriterCreate(output, take, NULL);
        else reader = xrtCsvReaderCreate(input, take, NULL);
        injected = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (injected) { assert(xrtErrorKind(xrtGetError()) == XERR_MEMORY); ++rejected; }
        else { assert((reader || writer) && !xrtGetError()); ++complete; }
        xrtCsvReaderDestroy(reader); xrtCsvWriterDestroy(writer);
        assert(closed == (take ? 1u : 0u));
        if (!take) { assert(xrtReaderDestroy(input) && xrtWriterDestroy(output)); }
        assert(closed == 1);
        unchanged(&before);
    }
    assert(rejected > 0 && complete == 2);
    return rejected;
}

int main(void)
{
    xmemdebugsnapshot before;
    unsigned int count;
    assert(xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
    count = reader_failures(false) + reader_failures(true);
    count += writer_failures(false) + writer_failures(true);
    count += construction_ownership(false, false) + construction_ownership(false, true);
    count += construction_ownership(true, false) + construction_ownership(true, true);
    unchanged(&before);
    printf("csv allocation: %u failure points, recovery and ownership passed\n", count);
    return 0;
}
