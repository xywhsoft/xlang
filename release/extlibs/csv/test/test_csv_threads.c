#define XRT_MODULE_THREAD
#define XRT_MODULE_ATOMIC
#include "../include/xrt_csv.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct { xatomic32* start; int id; } worker;

static int run(void* input)
{
    worker* state = input;
    const str fields[] = {"left,right", "line\r\nbreak", "\xe4\xbd\xa0\xe5\xa5\xbd", ""};
    while (!xrtAtomic32Load(state->start, XMEMORY_ACQUIRE)) xrtThreadYield();
    for (int round = 0; round < 250; ++round) {
        xrtcsvwriter* writer = xrtCsvWriterCreateMemory(NULL);
        assert(writer && xrtCsvWriterWrite(writer, 4, fields));
        assert(xrtCsvWriterClose(writer));
        xbuffer* bytes = xrtCsvWriterTakeBuffer(writer);
        assert(bytes);
        xrtCsvWriterDestroy(writer);
        xrtcsvreader* reader = xrtCsvReaderOpenMemory(bytes->Data, bytes->Size, NULL);
        xrtcsvrow row;
        assert(reader && xrtCsvReaderNext(reader, &row) && row.iCount == 4);
        for (uint32 i = 0; i < row.iCount; ++i) assert(strcmp(row.pFields[i], fields[i]) == 0);
        assert(!xrtCsvReaderNext(reader, &row) && xrtCsvReaderEOF(reader));
        xrtCsvReaderDestroy(reader);
        xrtBufferDestroy(bytes);
        // 不同实例可并行，错误槽也不能被另一个线程的 CSV 调用清空。
        xrtSetErrorInfo(XERR_STATE, "csv.thread", state->id, "marker");
        xrtThreadYield();
        assert(xrtErrorCode(xrtGetError()) == state->id);
    }
    xrtClearError();
    return 0;
}

int main(void)
{
    xatomic32 start;
    worker workers[4];
    xthread* threads[4];
    xrtAtomic32Init(&start, 0);
    for (int i = 0; i < 4; ++i) {
        workers[i] = (worker){&start, i + 1};
        threads[i] = xrtThreadCreate(run, &workers[i], 0);
        assert(threads[i]);
    }
    xrtAtomic32Store(&start, 1, XMEMORY_RELEASE);
    for (int i = 0; i < 4; ++i) {
        assert(xrtThreadWait(threads[i]) == XWAIT_OK && xrtThreadExitCode(threads[i]) == 0);
        xrtThreadDestroy(threads[i]);
    }
    puts("CSV threads: four workers, 1000 independent roundtrips and TLS PASS");
    return 0;
}
