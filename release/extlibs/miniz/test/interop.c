/* 标准输入/输出只交付完整成功结果，供 Python zlib 独立验证。 */
#include "../include/xrt_miniz.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

int main(int argc, char **argv) {
    bool encode = argc >= 2 && !strcmp(argv[1], "encode");
    bool decode = argc == 2 && !strcmp(argv[1], "decode");
    xrtminizoptions options; xrtminizerror error;
    xbuffer *input = NULL, *output = NULL;
    unsigned char chunk[65536]; size_t size;
    int code = 2;
    xrtMinizOptionsInit(&options);
    if (encode) {
        char *end; long level;
        if (argc != 3) return 2;
        level = strtol(argv[2], &end, 10);
        if (*end || level < 0 || level > 10) return 2;
        options.iLevel = (int)level;
    } else if (!decode) return 2;
    /* 独立格式检查不限解压比；安全配额由包内其他测试覆盖。 */
    options.tLimits.iMaxCompressionRatio = 0;
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    input = xrtBufferCreate();
    if (!input) return 2;
    while ((size = fread(chunk, 1, sizeof(chunk), stdin)) != 0)
        if (!xrtBufferAppend(input, (xbytesview){chunk, size})) goto cleanup;
    if (ferror(stdin)) goto cleanup;
    bool ok = encode ? xrtMinizCompress(input->Data, input->Size, &options, &output, &error) :
        xrtMinizDecompress(input->Data, input->Size, &options, &output, &error);
    if (!ok) {
        fprintf(stderr, "miniz: %s (code=%d)\n", error.sMessage, error.iCode);
        code = 1;
    } else if (!output->Size || fwrite(output->Data, 1, output->Size, stdout) == output->Size) code = 0;
cleanup:
    xrtBufferDestroy(output); xrtBufferDestroy(input);
    return code;
}
