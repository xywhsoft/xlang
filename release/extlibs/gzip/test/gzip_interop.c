#ifndef XRT_MODULE_IO_FILE
#define XRT_MODULE_IO_FILE
#endif
#include "../include/xrt_gzip.h"
#include <stdio.h>
#include <string.h>

/* 独立实现互通测试驱动；路径全部由测试调用者指定，不用于业务 API。 */
int main(int argc, char** argv)
{
    xreader* reader;
    xwriter* writer;
    xrtgziperror error;
    xrtgzipoptions options;
    bool ok;
    int status;
    if (argc != 4 || (strcmp(argv[1], "compress") != 0 && strcmp(argv[1], "decompress") != 0)) return 2;
    reader = xrtReaderOpen(argv[2]);
    if (!reader) return 3;
    writer = xrtWriterOpen(argv[3]);
    if (!writer) { xrtReaderDestroy(reader); return 4; }
    xrtGzipOptionsInit(&options);
    options.tLimits.iMaxCompressionRatio = 0;
    ok = strcmp(argv[1], "compress") == 0 ?
        xrtGzipCompressStream(reader, writer, &options, NULL, NULL, &error) :
        xrtGzipDecompressStream(reader, writer, &options, NULL, NULL, &error);
    if (!ok) fprintf(stderr, "%s\n", error.sMessage);
    /* 坏帧负例只接受格式/截断失败，不把 OOM、IO 或进程崩溃算作通过。 */
    status = ok ? 0 : (error.iCode == XRT_GZIP_ERROR_DATA || error.iCode == XRT_GZIP_ERROR_TRUNCATED ? 1 : 5);
    if (!xrtWriterDestroy(writer)) status = 6;
    if (!xrtReaderDestroy(reader)) status = 7;
    xrtClearError();
    return status;
}
