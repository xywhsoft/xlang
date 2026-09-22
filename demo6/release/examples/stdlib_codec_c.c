#define XRT_MODULE_CODEC_HEX
#define XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_CODEC_PERCENT
#define XRT_MODULE_BUFFER_HEX
#define XRT_MODULE_BUFFER_BASE64
#define XRT_MODULE_STRING
#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void require_codec_at(int ok, int line)
{
    if ( !ok ) {
        fprintf(stderr, "codec=fail:%d\n", line);
        exit(1);
    }
}

#define require_codec(Expr) require_codec_at((Expr), __LINE__)

static void require_text(str value, const char* expected)
{
    if ( value == NULL ) {
        fprintf(stderr, "codec=text-null expected=%s\n", expected);
        exit(1);
    }
    if ( strcmp((const char*)value, expected) != 0 ) {
        fprintf(stderr, "codec=text-mismatch actual=%s expected=%s\n",
            (const char*)value, expected);
        exit(1);
    }
    xrtFree(value);
}

static void require_buffer(xbuffer* value)
{
    require_codec(value != NULL);
    require_codec(value->Size == 4u);
    require_codec(value->Data[0] == 0u);
    require_codec(value->Data[1] == 255u);
    require_codec(value->Data[2] == 65u);
    require_codec(value->Data[3] == 90u);
    xrtBufferDestroy(value);
}

int main(void)
{
    unsigned char raw[] = {0u, 255u, 65u, 90u};
    xstrview empty = {NULL, 0u};
    size_t outputSize = 0u;

    require_text(xrtHexEncodeNew("xlang", 5u, XHEX_UPPER), "786C616E67");
    require_text((str)xrtHexDecodeNew(
        xrtStrView("786C616E67"), &outputSize, 0u), "xlang");

    require_text(xrtBase64EncodeNew("xlang", 5u, NULL), "eGxhbmc=");
    require_text((str)xrtBase64DecodeNew(
        "eGxhbmc=", 8u, &outputSize, NULL), "xlang");

    require_text(xrtPercentEncodeNew(
        "alice bob/ok", 12u, empty, &outputSize), "alice%20bob%2Fok");
    require_text((str)xrtPercentDecodeNew(
        xrtStrView("alice%20bob%2Fok"), &outputSize), "alice bob/ok");

    require_text(xrtHexEncodeNew(raw, sizeof(raw), XHEX_UPPER), "00FF415A");
    require_buffer(xrtBufferFromHex(xrtStrView("00FF415A"), 0u));
    require_text(xrtBase64EncodeNew(raw, sizeof(raw), NULL), "AP9BWg==");
    require_buffer(xrtBufferFromBase64(xrtStrView("AP9BWg=="), NULL));

    printf("codec=ok\n");
    return 0;
}
