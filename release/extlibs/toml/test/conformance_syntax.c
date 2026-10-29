/* 检查 TOML 语法；可选输出解析后的写回结果，供独立解析器比对。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/xrt_toml.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char** argv)
{
    xrttomlerror error;
    size_t size = 0u;
    bytes data;
    xvalue* value;
    const bool roundtrip = argc == 3 && strcmp(argv[1], "--roundtrip") == 0;
    const char* path = roundtrip ? argv[2] : argc == 2 ? argv[1] : NULL;
    if (path == NULL) return 2;
    data = xrtFileReadAllLimit(path, 64u * 1024u * 1024u, &size);
    if (data == NULL) return 2;
    value = xrtTomlParseMemory(data, size, NULL, &error);
    xrtFree(data);
    if (value != NULL) {
        if (roundtrip) {
            xbuffer* output = xrtTomlStringify(value, NULL, &error);
            if (output == NULL) {
                fprintf(stderr, "%s: stringify code=%d %s\n", path,
                        error.iCode, error.sMessage);
                xrtValueRelease(value);
                return 2;
            }
            if (fwrite(output->Data, 1u, output->Size, stdout) != output->Size) {
                xrtBufferDestroy(output);
                xrtValueRelease(value);
                return 2;
            }
            xrtBufferDestroy(output);
        }
        xrtValueRelease(value);
        return 0;
    }
    if (error.iCode == XRT_TOML_ERROR_FORMAT) return 1;
    fprintf(stderr, "%s: code=%d %s\n", path, error.iCode, error.sMessage);
    return 2;
}

#include "../../_tomlc17/tomlc17_xrt.c"
#include "../xrt_toml_types.c"
#include "../xrt_toml_parser.c"
#include "../xrt_toml_writer.c"
