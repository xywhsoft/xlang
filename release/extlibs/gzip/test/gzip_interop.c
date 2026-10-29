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
    if (!xrtWriterDestroy(writer)) ok = false;
    if (!xrtReaderDestroy(reader)) ok = false;
    xrtClearError();
    return ok ? 0 : 1;
}
