#include "src/runtime/xl6_text_value.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    const char raw[] = {'a',0,'b'};
    xstrview text = {raw,sizeof(raw)};
    uint32 expected = xrtCrc32(raw,sizeof(raw));
    assert(xl6Crc32StringV1(0,text,-1)==expected);
    assert(xl6Crc32StringV1(0,text,99)==expected);
    assert(xl6Crc32StringV1(0,text,1)==xrtCrc32(raw,1));
    assert(xl6Crc32StringV1(UINT32_MAX,text,0)==UINT32_MAX);
    xrtSetErrorKind(XERR_STATE);
    const xerror* ambient=xrtGetError();
    assert(xl6Crc32StringV1(0,text,-1)==expected && xrtGetError()==ambient);
    xrtClearError();
    assert(xl6Crc32StringV1(UINT64_C(0x100000000),text,0)==0);
    assert(xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
    assert(xl6Crc32StringV1(0,text,-2)==0);
    assert(xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
    xstrview invalid={NULL,1};
    assert(xl6Crc32StringV1(0,invalid,-1)==0);
    assert(xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();
    assert(xl6Crc32BytesV1(0,NULL,-1)==0);
    assert(xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();
    xvalue* value=xrtValueBytes((xbytesview){(const uint8*)raw,sizeof(raw)});
    assert(value && xl6Crc32BytesV1(0,value,-1)==expected);
    xrtValueRelease(value);
    assert(xl6Crc32BufferV1(0,NULL,-1)==0);
    assert(xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();
    xbuffer buffer;
    assert(xrtBufferInit(&buffer));
    assert(xrtBufferAppend(&buffer,(xbytesview){(const uint8*)raw,sizeof(raw)}));
    assert(xl6Crc32BufferV1(0,&buffer,-1)==expected);
    buffer.Size=buffer.Capacity+1;
    assert(xl6Crc32BufferV1(0,&buffer,-1)==0);
    assert(xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();
    buffer.Size=sizeof(raw);xrtBufferDestroy(&buffer);
    puts("CRC32 native product contracts ok");
    return 0;
}
