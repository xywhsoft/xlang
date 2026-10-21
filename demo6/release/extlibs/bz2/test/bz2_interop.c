#ifndef XRT_MODULE_IO_FILE
#define XRT_MODULE_IO_FILE
#endif
#include "../include/xrt_bz2.h"
#include <stdio.h>
#include <string.h>

/* 参数五：压缩为 1..9 级，解压为普通/低内存模式 0/1。 */
int main(int argc, char** argv)
{
    xreader* reader;
    xwriter* writer;
    xrtbz2options options;
    xrtbz2error error;
    bool compress, ok;
    if (argc != 5 || strlen(argv[4]) != 1) return 2;
    compress = strcmp(argv[1], "compress") == 0;
    if (!compress && strcmp(argv[1], "decompress") != 0) return 2;
    if (compress ? argv[4][0] < '1' || argv[4][0] > '9' :
        argv[4][0] != '0' && argv[4][0] != '1') return 2;
    reader = xrtReaderOpen(argv[2]);
    if (!reader) return 3;
    writer = xrtWriterOpen(argv[3]);
    if (!writer) { xrtReaderDestroy(reader); return 4; }
    xrtBz2OptionsInit(&options);
    // 独立互通输入由测试生成；不让启发式倍率限制遮蔽编解码测试。
    options.tLimits.iMaxCompressionRatio = 0;
    if (compress) options.iLevel = argv[4][0] - '0';
    else options.bSmallMemory = argv[4][0] == '1';
    ok = compress ? xrtBz2CompressStream(reader, writer, &options, NULL, NULL, &error) :
        xrtBz2DecompressStream(reader, writer, &options, NULL, NULL, &error);
    if (!ok) fprintf(stderr, "%s\n", error.sMessage);
    if (!xrtWriterDestroy(writer)) ok = false;
    if (!xrtReaderDestroy(reader)) ok = false;
    xrtClearError();
    return ok ? 0 : 1;
}
