#ifndef XRT_MODULE_IO_FILE
#define XRT_MODULE_IO_FILE
#endif
#include "../include/xrt_lz4.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 独立工具驱动：第五参数为压缩加速因子，解压时传 1。 */
int main(int argc, char** argv)
{
    xreader* reader;
    xwriter* writer;
    xrtlz4options options;
    xrtlz4error error;
    char* end;
    long acceleration;
    bool compress, ok;
    if (argc != 5) return 2;
    compress = strcmp(argv[1], "compress") == 0;
    if (!compress && strcmp(argv[1], "decompress") != 0) return 2;
    acceleration = strtol(argv[4], &end, 10);
    if (*end || acceleration < 1 || acceleration > 65537) return 2;
    reader = xrtReaderOpen(argv[2]);
    if (!reader) return 3;
    writer = xrtWriterOpen(argv[3]);
    if (!writer) { xrtReaderDestroy(reader); return 4; }
    xrtLz4OptionsInit(&options);
    // 此驱动验证标准格式互通，资源限制由边界测试单独验证。
    options.tLimits.iMaxCompressionRatio = 0;
    options.iAcceleration = (int)acceleration;
    ok = compress ? xrtLz4CompressStream(reader, writer, &options, NULL, NULL, &error) :
        xrtLz4DecompressStream(reader, writer, &options, NULL, NULL, &error);
    if (!ok) fprintf(stderr, "%s\n", error.sMessage);
    if (!xrtWriterDestroy(writer)) ok = false;
    if (!xrtReaderDestroy(reader)) ok = false;
    xrtClearError();
    return ok ? 0 : 1;
}
