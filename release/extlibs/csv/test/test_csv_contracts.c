#include "../include/xrt_csv.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void dialects(void)
{
    for (unsigned int mode = 0; mode < 9; ++mode) {
        xrtcsvdialect d;
        xrtcsvreader* r;
        xrtcsvwriter* w;
        xrtCsvDialectInit(&d);
        switch (mode) {
        case 0: ++d.iSize; break;
        case 1: ++d.iVersion; break;
        case 2: d.bReserved = true; break;
        case 3: ++d.tLimits.iSize; break;
        case 4: ++d.tLimits.iVersion; break;
        case 5: d.tLimits.iReserved = 1; break;
        case 6: d.tLimits.iMaxDepth = 1; break;
        case 7: d.tLimits.iMaxCompressionRatio = 1; break;
        case 8: d.tLimits.iFlags = 1; break;
        }
        r = xrtCsvReaderOpenMemory("a", 1, &d);
        assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
        xrtCsvReaderDestroy(r);
        w = xrtCsvWriterCreateMemory(&d);
        assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
        xrtCsvWriterDestroy(w);
        xrtClearError();
    }
}

static void cursor_limit(void)
{
    xrtcsvdialect d;
    xrtcsvrow row;
    xreader* input = xrtReaderFromMemory((xbytesview){(cbytes)"prefixa,b\n", 10});
    xrtcsvreader* r;
    assert(input && xrtReaderSeek(input, 6, XSEEK_START, NULL));
    xrtCsvDialectInit(&d);
    d.tLimits.iMaxInputBytes = 4;
    r = xrtCsvReaderCreate(input, true, &d);
    assert(r && !xrtGetError());
    assert(xrtCsvReaderNext(r, &row) && row.iCount == 2);
    assert(!strcmp(row.pFields[0], "a") && !strcmp(row.pFields[1], "b"));
    assert(!xrtCsvReaderNext(r, &row) && xrtCsvReaderEOF(r) && !xrtGetError());
    xrtCsvReaderDestroy(r);
}

static void early_limit(void)
{
    xrtcsvdialect d;
    xrtcsvrow row;
    xrtcsvreader* r;
    xrtCsvDialectInit(&d);
    d.tLimits.iMaxOutputBytes = 2;
    d.tLimits.iMaxItemBytes = 0;
    r = xrtCsvReaderOpenMemory("abcd\n", 5, &d);
    assert(r && !xrtCsvReaderNext(r, &row));
    assert(xrtCsvReaderError(r)->iCode == XRT_CSV_ERROR_LIMIT);
    /* 第三个字段字节就拒绝，不等到换行才知道已经超出输出配额。 */
    assert(xrtCsvReaderError(r)->iOffset == 2);
    xrtCsvReaderDestroy(r);
    xrtClearError();
}

typedef struct {
    xrtcsvreader* Reader;
    xrtcsvwriter* Writer;
    unsigned int Mode, Closed;
    bool Armed;
} callback_state;

static void reenter(callback_state* state)
{
    xrtcsvrow row;
    if (!state->Armed) return;
    state->Armed = false;
    if (state->Reader) {
        if (state->Mode == 0) assert(!xrtCsvReaderNext(state->Reader, &row));
        else if (state->Mode == 1) assert(!xrtCsvReaderClose(state->Reader));
        else xrtCsvReaderDestroy(state->Reader);
    } else {
        if (state->Mode == 0) assert(!xrtCsvWriterWrite(state->Writer, 0, NULL));
        else if (state->Mode == 1) assert(!xrtCsvWriterClose(state->Writer));
        else if (state->Mode == 2) xrtCsvWriterDestroy(state->Writer);
        else assert(xrtCsvWriterTakeBuffer(state->Writer) == NULL);
    }
    assert(xrtErrorKind(xrtGetError()) == XERR_STATE);
    /* 即便用户回调吞掉诊断并返回成功，外层操作仍须失败。 */
    xrtClearError();
}

static bool read_reentry(ptr context, ptr data, size_t request, size_t* count)
{
    callback_state* state = context;
    assert(request);
    reenter(state);
    *(char*)data = '\n';
    *count = 1;
    return true;
}

static bool write_reentry(ptr context, const void* data, size_t request, size_t* count)
{
    (void)data;
    reenter(context);
    *count = request;
    return true;
}

static bool close_callback(ptr context)
{
    callback_state* state = context;
    ++state->Closed;
    reenter(state);
    return true;
}

static bool flush_callback(ptr context)
{
    reenter(context);
    return true;
}

static void reentry(void)
{
    for (unsigned int closing = 0; closing < 2; ++closing) {
        for (unsigned int mode = 0; mode < 3; ++mode) {
            callback_state state = {0};
            xreaderops ops = {0};
            xrtcsvrow row;
            ops.Read = read_reentry; ops.Close = close_callback;
            state.Mode = mode;
            state.Reader = xrtCsvReaderCreate(xrtReaderCreate(&ops, &state), true, NULL);
            assert(state.Reader);
            state.Armed = true;
            assert(!(closing ? xrtCsvReaderClose(state.Reader) : xrtCsvReaderNext(state.Reader, &row)));
            assert(xrtErrorKind(xrtGetError()) == XERR_STATE);
            assert(!xrtCsvReaderNext(state.Reader, &row));
            xrtCsvReaderDestroy(state.Reader);
            assert(state.Closed == 1);
            xrtClearError();
        }
    }
    for (unsigned int stage = 0; stage < 3; ++stage) {
        for (unsigned int mode = 0; mode < 4; ++mode) {
            callback_state state = {0};
            xwriterops ops = {0};
            ops.Write = write_reentry; ops.Close = close_callback;
            if (stage != 2) ops.Flush = flush_callback;
            state.Mode = mode;
            state.Writer = xrtCsvWriterCreate(xrtWriterCreate(&ops, &state), true, NULL);
            assert(state.Writer);
            state.Armed = true;
            assert(!(stage ? xrtCsvWriterClose(state.Writer) : xrtCsvWriterWrite(state.Writer, 0, NULL)));
            assert(xrtErrorKind(xrtGetError()) == XERR_STATE);
            assert(!xrtCsvWriterWrite(state.Writer, 0, NULL));
            xrtCsvWriterDestroy(state.Writer);
            assert(state.Closed == 1);
            xrtClearError();
        }
    }
}

int main(int argc, char** argv)
{
    const char* mode = argc > 1 ? argv[1] : "all";
    if (!strcmp(mode, "all") || !strcmp(mode, "dialect")) dialects();
    if (!strcmp(mode, "all") || !strcmp(mode, "cursor")) cursor_limit();
    if (!strcmp(mode, "all") || !strcmp(mode, "limit")) early_limit();
    if (!strcmp(mode, "all") || !strcmp(mode, "reentry")) reentry();
    puts("csv contracts: dialect version, current cursor, early limits and reentry passed");
    return 0;
}
