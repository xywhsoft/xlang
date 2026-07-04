#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void require_codec(int ok)
{
    if ( !ok ) {
        fprintf(stderr, "codec=fail\n");
        exit(1);
    }
}

static void require_text(str value, const char* expected)
{
    require_codec(value != NULL);
    require_codec(strcmp((const char*)value, expected) == 0);
    xrtFree(value);
}

static void require_buffer(xbuffer value)
{
    require_codec(value != NULL);
    require_codec(value->Length == 4u);
    require_codec(((unsigned char*)value->Buffer)[0] == 0u);
    require_codec(((unsigned char*)value->Buffer)[1] == 255u);
    require_codec(((unsigned char*)value->Buffer)[2] == 65u);
    require_codec(((unsigned char*)value->Buffer)[3] == 90u);
    xrtBufferDestroy(value);
}

int main(void)
{
    unsigned char raw[] = {0u, 255u, 65u, 90u};

    xrtInit();

    require_text(xrtHexEncode((ptr)"xlang", 5u), "786C616E67");
    require_text((str)xrtHexDecode("786C616E67", 10u), "xlang");

    require_text(xrtBase64Encode((ptr)"xlang", 5u, NULL), "eGxhbmc=");
    require_text((str)xrtBase64Decode("eGxhbmc=", 8u, NULL), "xlang");

    require_text(xrtPercentEncode("alice bob/ok", 12u, FALSE, NULL), "alice%20bob%2Fok");
    require_text(xrtPercentDecode("alice%20bob%2Fok", 16u, FALSE, NULL), "alice bob/ok");
    require_text(xrtPercentEncode("alice bob/ok", 12u, TRUE, NULL), "alice+bob%2Fok");
    require_text(xrtPercentDecode("alice+bob%2Fok", 14u, TRUE, NULL), "alice bob/ok");

    require_text(xrtFormUrlEncode("alice bob/ok", 12u, NULL), "alice+bob%2Fok");
    require_text(xrtFormUrlDecode("alice+bob%2Fok", 14u, NULL), "alice bob/ok");

    require_text(xrtHexEncode((ptr)raw, sizeof(raw)), "00FF415A");
    require_buffer(xrtBufferFromHex((str)"00FF415A", 8u));
    require_text(xrtBase64Encode((ptr)raw, sizeof(raw), NULL), "AP9BWg==");
    require_buffer(xrtBufferFromBase64((str)"AP9BWg==", 8u, NULL));

    printf("codec=ok\n");
    xrtUnit();
    return 0;
}
