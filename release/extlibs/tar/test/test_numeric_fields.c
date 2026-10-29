#include "../include/xrt_tar.h"
#include <stdio.h>
#include <string.h>

static void checksum(unsigned char* header)
{
    unsigned sum = 0;
    size_t i;
    memset(header + 148u, ' ', 8u);
    for (i = 0; i < 512u; ++i) sum += header[i];
    snprintf((char*)header + 148u, 8u, "%06o", sum);
    header[154u] = 0;
    header[155u] = ' ';
}

static xbuffer* archive(uint64 uid, uint64 gid)
{
    xrttarwriter* writer = xrtTarWriterCreateMemory(NULL);
    xrttarentry entry;
    xbuffer* result = NULL;
    xrtTarEntryInit(&entry);
    entry.sName = "numeric.txt";
    entry.iMode = 0644u;
    entry.iUID = uid;
    entry.iGID = gid;
    if (writer != NULL && xrtTarWriterAdd(writer, &entry, NULL, 0) &&
        xrtTarWriterClose(writer)) result = xrtTarWriterTakeBuffer(writer);
    xrtTarWriterDestroy(writer);
    return result;
}

static int matrix(void)
{
    /* 12 字节负数的高位全一还不够；低 64 位也必须保持负号。 */
    static const struct {
        size_t Offset, Width;
        bool Negative, Valid;
        uint64 Low;
        int64 Time;
        uint32 Mode;
    } cases[] = {
        {136, 12, true,  true,  UINT64_MAX, -1, 0644},
        {136, 12, true,  true,  (uint64)INT64_MIN, INT64_MIN, 0644},
        {136, 12, true,  false, (uint64)INT64_MAX, 0, 0},
        {136, 12, true,  false, 0, 0, 0},
        {136, 12, true,  false, 1, 0, 0},
        {136, 12, false, true,  (uint64)INT64_MAX, INT64_MAX, 0644},
        {136, 12, false, true,  0, 0, 0644},
        {136, 12, false, false, (uint64)INT64_MAX + 1u, 0, 0},
        {100, 8,  false, true,  UINT32_MAX, 0, UINT32_MAX},
        {100, 8,  false, false, (uint64)UINT32_MAX + 1u, 0, 0},
        {100, 8,  false, false, (uint64)UINT32_MAX + 0645u, 0, 0},
        {100, 8,  true,  false, UINT64_MAX, 0, 0},
        {108, 8,  true,  false, UINT64_MAX, 0, 0},
        {116, 8,  true,  false, UINT64_MAX, 0, 0},
        {124, 12, true,  false, UINT64_MAX, 0, 0},
        {329, 8,  false, false, (uint64)UINT32_MAX + 1u, 0, 0},
        {337, 8,  false, false, (uint64)UINT32_MAX + 1u, 0, 0}
    };
    size_t i;
    int ok = 1;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        xbuffer* packed = archive(0, 0);
        xrttarreader* reader;
        xrttarentry entry;
        bool next;
        size_t j;
        if (packed == NULL) return 0;
        memset(packed->Data + cases[i].Offset, cases[i].Negative ? 0xff : 0,
            cases[i].Width);
        for (j = 0; j < 8u; ++j) {
            packed->Data[cases[i].Offset + cases[i].Width - 1u - j] =
                (unsigned char)(cases[i].Low >> (j * 8u));
        }
        packed->Data[cases[i].Offset] |= 0x80u;
        checksum(packed->Data);
        reader = xrtTarReaderOpenMemory(packed->Data, packed->Size, NULL);
        next = reader != NULL && xrtTarReaderNext(reader, &entry);
        if (reader == NULL || (cases[i].Valid ?
            !next || entry.iMTime != cases[i].Time || entry.iMode != cases[i].Mode :
            next || xrtTarReaderError(reader)->iCode != XRT_TAR_ERROR_FORMAT)) {
            fprintf(stderr, "TAR numeric case %zu: next=%d time=%lld mode=%u error=%d\n",
                i, (int)next, next ? (long long)entry.iMTime : 0,
                next ? (unsigned)entry.iMode : 0,
                reader != NULL ? xrtTarReaderError(reader)->iCode : -1);
            ok = 0;
        }
        if (next && cases[i].Valid && (xrtTarReaderNext(reader, &entry) ||
                !xrtTarReaderEOF(reader))) ok = 0;
        xrtTarReaderDestroy(reader);
        xrtBufferDestroy(packed);
    }
    /* PAX 十进制 ID 使用完整 uint64，不受 GNU 八字节字段的 63 位限制。 */
    {
        xbuffer* packed = archive(UINT64_MAX, (uint64)INT64_MAX + 1u);
        xrttarreader* reader = packed != NULL ?
            xrtTarReaderOpenMemory(packed->Data, packed->Size, NULL) : NULL;
        xrttarentry entry;
        if (reader == NULL || !xrtTarReaderNext(reader, &entry) ||
            entry.iUID != UINT64_MAX || entry.iGID != (uint64)INT64_MAX + 1u) ok = 0;
        xrtTarReaderDestroy(reader);
        xrtBufferDestroy(packed);
    }
    return ok;
}

static int inspect(cstr path)
{
    xrttarreader* reader;
    xrttarentry entry;
    int ok = 1;
    reader = xrtTarReaderOpenFile(path, NULL);
    if (reader == NULL) return 1;
    if (xrtTarReaderNext(reader, &entry)) {
        printf("mtime=%lld;mode=%u;uid=%llu;gid=%llu;major=%u;minor=%u\n",
            (long long)entry.iMTime, (unsigned)entry.iMode,
            (unsigned long long)entry.iUID, (unsigned long long)entry.iGID,
            (unsigned)entry.iDeviceMajor, (unsigned)entry.iDeviceMinor);
        ok = !xrtTarReaderNext(reader, &entry) && xrtTarReaderEOF(reader);
    } else if (xrtTarReaderError(reader)->iCode == XRT_TAR_ERROR_FORMAT) {
        puts("invalid:3");
    } else ok = 0;
    xrtTarReaderDestroy(reader);
    return ok ? 0 : 1;
}

/* 批量文件探针让 C/JIT/AOT 只启动一次，而不是为每个数值重复编译。 */
int main(int argc, char** argv)
{
    int i;
    if (argc == 1) {
        if (!matrix()) return 1;
        puts("TAR numeric C matrix: PASS");
        return 0;
    }
    for (i = 1; i < argc; ++i) if (inspect(argv[i]) != 0) return 1;
    return 0;
}
