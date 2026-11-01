/* 用完整预期字节验证U+FEFF数据，不把它误当输入开头的编码标记。 */
#define XRT_MODULE_MEMORY_DEBUG
#include "../include/xrt_csv.h"
#include <stdio.h>
#include <string.h>

static unsigned cases, failures, faults;
static bool records;
static const char* values[] = {"", "plain", "\xef\xbb\xbf", "\xef\xbb\xbf" "data",
    "data\xef\xbb\xbf", "\xef\xbb\xbf,part", "\xef\xbb\xbf\"part"};

static void require(bool ok, const char* message)
{
    if (!ok) {
        if (failures < 8u) fprintf(stderr, "CSV BOM case %u: %s\n", cases, message);
        ++failures;
    }
}

static bool balanced(xmemdebugsnapshot a)
{
    xmemdebugsnapshot b;
    xrtClearError(); xrtMemDebugSnapshot(&b);
    return a.LiveCount == b.LiveCount && a.LiveBytes == b.LiveBytes &&
        a.InvalidFreeCount == b.InvalidFreeCount && a.DoubleFreeCount == b.DoubleFreeCount &&
        a.OverflowCount == b.OverflowCount && a.UnderflowCount == b.UnderflowCount &&
        a.UseAfterFreeCount == b.UseAfterFreeCount;
}

/* 预期编码由已输出的字节决定首部歧义，不读取被测writer的内部状态。 */
static void expected_row(char* out, size_t* used, const xrtcsvdialect* d, unsigned count, const str* fields)
{
    for (unsigned i = 0; i < count; ++i) {
        const char* value = fields[i];
        bool quote = d->bQuoteAll || (count == 1u && *value == 0);
        if (i != 0u) out[(*used)++] = d->iDelimiter;
        if (*used == 0u && strncmp(value, "\xef\xbb\xbf", 3u) == 0) quote = true;
        for (const char* p = value; *p; ++p)
            if (*p == d->iDelimiter || *p == d->iQuote || *p == '\r' || *p == '\n') quote = true;
        if (quote) out[(*used)++] = d->iQuote;
        for (; *value; ++value) {
            out[(*used)++] = *value;
            if (quote && *value == d->iQuote) out[(*used)++] = *value;
        }
        if (quote) out[(*used)++] = d->iQuote;
    }
    if (d->iNewline == XRT_CSV_NEWLINE_CRLF) out[(*used)++] = '\r';
    out[(*used)++] = '\n';
}

static xbuffer* produce(const xrtcsvdialect* d, unsigned position, const char* value)
{
    const str prior[] = {"head"}, fields[] = {position == 1u ? "head" : (str)value, (str)value};
    xrtcsvwriter* writer = xrtCsvWriterCreateMemory(d);
    xbuffer* output = NULL;
    bool ok = writer != NULL && xrtCsvWriterError(writer)->iCode == XRT_CSV_OK;
    if (ok && position == 2u) ok = xrtCsvWriterWrite(writer, 1u, prior);
    if (ok && position == 3u) ok = xrtCsvWriterWrite(writer, 0u, NULL);
    if (ok) ok = xrtCsvWriterWrite(writer, position == 1u ? 2u : 1u, fields);
    if (ok) ok = xrtCsvWriterClose(writer);
    if (ok) output = xrtCsvWriterTakeBuffer(writer);
    xrtCsvWriterDestroy(writer);
    return output;
}

static void one(xrtcsvdialect d, unsigned position, unsigned valueIndex)
{
    const char* value = values[valueIndex];
    const str prior[] = {"head"}, fields[] = {position == 1u ? "head" : (str)value, (str)value};
    char expected[256]; size_t size = 0;
    xmemdebugsnapshot before;
    xrtClearError(); xrtMemDebugSnapshot(&before);
    if (d.bWriteBOM) { memcpy(expected, "\xef\xbb\xbf", 3u); size = 3u; }
    if (position == 2u) expected_row(expected, &size, &d, 1u, prior);
    if (position == 3u) expected_row(expected, &size, &d, 0u, NULL);
    expected_row(expected, &size, &d, position == 1u ? 2u : 1u, fields);
    xbuffer* output = produce(&d, position, value);
    require(output != NULL && xrtGetError() == NULL, "produce failed");
    if (output != NULL && records) {
        printf("%u\t%u\t%u\t%d\t%d\t%d\t%u\t%u\t", cases, (unsigned char)d.iDelimiter,
            (unsigned char)d.iQuote, d.iNewline, (int)d.bQuoteAll, (int)d.bWriteBOM, position, valueIndex);
        for (size_t i = 0; i < output->Size; ++i) printf("%02x", output->Data[i]);
        putchar('\n');
    }
    if (output != NULL && !records) {
        require(output->Size == size && memcmp(output->Data, expected, size) == 0, "wire bytes differ");
        xrtcsvreader* reader = xrtCsvReaderOpenMemory(output->Data, output->Size, &d);
        xrtcsvrow row;
        require(reader != NULL && xrtCsvReaderError(reader)->iCode == XRT_CSV_OK, "reader creation");
        if (position >= 2u) {
            require(xrtCsvReaderNext(reader, &row) && row.iCount == (position == 2u ? 1u : 0u), "prior row");
            if (position == 2u && row.iCount == 1u) require(strcmp(row.pFields[0], "head") == 0, "prior field");
        }
        require(xrtCsvReaderNext(reader, &row) && row.iCount == (position == 1u ? 2u : 1u), "data row");
        if (row.iCount == (position == 1u ? 2u : 1u)) {
            require(strcmp(row.pFields[row.iCount - 1u], value) == 0, "U+FEFF data lost");
            if (position == 1u) require(strcmp(row.pFields[0], "head") == 0, "leading field");
        }
        require(!xrtCsvReaderNext(reader, &row) && xrtCsvReaderEOF(reader) && xrtGetError() == NULL, "EOF");
        xrtCsvReaderDestroy(reader);
    }
    xrtBufferDestroy(output);
    if (!records) {
        d.tLimits.iMaxOutputBytes = size;
        output = produce(&d, position, value);
        require(output != NULL && output->Size == size && memcmp(output->Data, expected, size) == 0, "exact output quota");
        xrtBufferDestroy(output);
        d.tLimits.iMaxOutputBytes = size - 1u;
        output = produce(&d, position, value);
        require(output == NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE, "short output quota");
        xrtBufferDestroy(output);
    }
    require(balanced(before), "seven memory counters");
    ++cases;
}

static void allocations(void)
{
    xmemdebugsnapshot initial;
    xrtClearError(); xrtMemDebugSnapshot(&initial);
    xrtcsvwriter* probe = xrtCsvWriterCreateMemory(NULL);
    xbuffer* empty = probe != NULL && xrtCsvWriterClose(probe) ? xrtCsvWriterTakeBuffer(probe) : NULL;
    size_t capacity = empty != NULL ? empty->Capacity : 0u;
    xrtCsvWriterDestroy(probe); xrtBufferDestroy(empty);
    require(capacity >= 4096u && capacity <= 1024u * 1024u, "initial capacity probe");
    if (capacity < 4096u || capacity > 1024u * 1024u) return;
    size_t length = capacity - 2u;
    char* large = xrtMalloc(length + 1u);
    if (large == NULL) { require(false, "capacity fixture allocation"); return; }
    memset(large, 'x', length); memcpy(large, "\xef\xbb\xbf", 3u); large[length] = 0;
    for (unsigned mode = 0; mode < 4u; ++mode) {
        const char* value = mode == 3u ? large : values[3];
        bool complete = false, injected = false;
        for (unsigned budget = 0; budget < 64u; ++budget) {
            xrtcsvdialect d;
            xmemdebugsnapshot before;
            xrtCsvDialectInit(&d);
            d.bWriteBOM = mode == 1u;
            if (mode == 2u) d.tLimits.iMaxOutputBytes = 2u;
            xrtClearError(); xrtMemDebugSnapshot(&before);
            require(xrtMemDebugFailAfter(budget), "inject allocation failure");
            xbuffer* output = produce(&d, 0u, value);
            bool triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (triggered) {
                ++faults; injected = true;
                require(output == NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY, "OOM classification");
            } else require(mode == 2u ? (output == NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE) :
                (output != NULL && xrtGetError() == NULL), "no-fault outcome");
            if (output != NULL && mode == 3u) require(output->Size == capacity + 1u && output->Capacity > capacity && output->Data[0] == '"' &&
                output->Data[length + 1u] == '"' && output->Data[length + 2u] == '\n' && memcmp(output->Data + 1u, large, length) == 0,
                "capacity boundary bytes");
            xrtBufferDestroy(output);
            require(balanced(before), "OOM memory balance");
            /* 故障后的独立调用必须仍能成功，旧输入不用重建。 */
            d.tLimits.iMaxOutputBytes = 0u;
            output = produce(&d, 0u, value);
            require(output != NULL && xrtGetError() == NULL, "immediate recovery");
            xrtBufferDestroy(output);
            require(balanced(before), "recovery memory balance");
            if (!triggered) { complete = true; break; }
        }
        require(complete && injected, "fault scan incomplete");
    }
    xrtFree(large);
    require(balanced(initial), "capacity fixture memory balance");
    printf("CSV BOM capacity: %zu -> %zu output bytes\n", capacity, capacity + 1u);
}

int main(int argc, char** argv)
{
    static const char delimiters[] = {',', ';', '\t'}, quotes[] = {'"', '\'', ' '};
    records = argc == 2 && strcmp(argv[1], "--records") == 0;
    if ((argc != 1 && !records) || !xrtMemDebugEnabled()) return 2;
    xrtSetErrorInfo(XERR_VALUE, "csv.bom-test", 0, "warm diagnostics"); xrtClearError();
    for (size_t sep = 0; sep < sizeof(delimiters); ++sep)
    for (size_t quote = 0; quote < sizeof(quotes); ++quote)
    for (unsigned newline = 0; newline < 2u; ++newline)
    for (unsigned all = 0; all < 2u; ++all)
    for (unsigned bom = 0; bom < 2u; ++bom)
    for (unsigned position = 0; position < 4u; ++position)
    for (unsigned value = 0; value < sizeof(values)/sizeof(*values); ++value) {
        xrtcsvdialect d;
        xrtCsvDialectInit(&d);
        d.iDelimiter = delimiters[sep]; d.iQuote = quotes[quote];
        d.iNewline = (int)newline; d.bQuoteAll = all != 0u; d.bWriteBOM = bom != 0u;
        one(d, position, value);
    }
    if (!records) {
        allocations();
        printf("CSV BOM values: %u cases, %u OOM, %u failures\n", cases, faults, failures);
    }
    return failures != 0u || ferror(stdout);
}

#ifdef __TINYC__
#include "../xrt_csv_common.c"
#include "../xrt_csv_reader.c"
#include "../xrt_csv_writer.c"
#endif
