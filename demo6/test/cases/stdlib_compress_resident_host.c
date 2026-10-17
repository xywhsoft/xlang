#include "src/runtime/xl6_text_value.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void equal(xvalue* value,const char* data,size_t size)
{xbytesview view;assert(value && xrtValueGetBytes(value,&view));assert(view.Size==size && !memcmp(view.Data,data,size));xrtValueRelease(value);}
static void error(void) {assert(xrtGetError());xrtClearError();}
static int32 wrongThread(ptr encoder)
{
    assert(!xl6DeflaterDoneV1(encoder));
    assert(xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();return 0;
}
int main(void)
{
    xstrview input={"a\0b\xff\0a",6};
    for (int format=0;format<3;format++) {
        xvalue* zipped=xl6DeflateTextV1(input,-1,format,6,0,67108864,15);assert(zipped);
        equal(xl6InflateBytesV1(zipped,-1,format,67108864,65536,15),input.Data,input.Size);
        if(format<2)equal(xl6InflateBytesV1(zipped,-1,3,67108864,65536,15),input.Data,input.Size);
        assert(!xl6InflateBytesV1(zipped,-1,format,3,65536,15));error();
        xbytesview data;assert(xrtValueGetBytes(zipped,&data));
        assert(!xl6InflateBytesV1(zipped,(int64_t)data.Size-1,format,67108864,65536,15));error();
        xrtValueRelease(zipped);
    }
    assert(!xl6DeflateTextV1(input,-2,2,6,0,-1,15));error();
    assert(!xl6DeflateTextV1(input,-1,3,6,0,-1,15));error();
    assert(!xl6DeflateTextV1(input,-1,2,6,0,1,15));error();
    xvalue* member=xl6DeflateTextV1(XRT_STR_LITERAL("A"),-1,2,6,0,-1,15);assert(member);
    xbytesview memberView;assert(xrtValueGetBytes(member,&memberView) && memberView.Size>8);
    uint8* joined=xrtMalloc(memberView.Size*2+1);assert(joined);
    memcpy(joined,memberView.Data,memberView.Size);memcpy(joined+memberView.Size,memberView.Data,memberView.Size);
    xvalue* concatenated=xrtValueBytes((xbytesview){joined,memberView.Size*2});assert(concatenated);
    equal(xl6InflateBytesV1(concatenated,-1,2,-1,65536,15),"AA",2);xrtValueRelease(concatenated);
    joined[memberView.Size]=0xff;
    xvalue* trailing=xrtValueBytes((xbytesview){joined,memberView.Size+1});assert(trailing);
    assert(!xl6InflateBytesV1(trailing,-1,2,-1,65536,15));error();xrtValueRelease(trailing);
    joined[memberView.Size-8]^=1;
    xvalue* corrupt=xrtValueBytes((xbytesview){joined,memberView.Size});assert(corrupt);
    assert(!xl6InflateBytesV1(corrupt,-1,2,-1,65536,15));error();xrtValueRelease(corrupt);
    xrtFree(joined);xrtValueRelease(member);
    void* encoder=xl6DeflaterCreateV1(2,6,0,-1,15);assert(encoder);
    xthread* worker=xrtThreadCreate(wrongThread,encoder,0);assert(worker);
    assert(xrtThreadWait(worker)==XWAIT_OK);xrtThreadDestroy(worker);assert(!xrtGetError());
    void* decoder=xl6InflaterCreateV1(2,-1,65536,15);assert(decoder);
    xbuffer* zipped=xrtBufferCreate();xbuffer* output=xrtBufferCreate();assert(zipped && output);
    assert(xl6DeflaterWriteTextV1(encoder,(xstrview){input.Data,2},zipped,1,-1));
    assert(xl6DeflaterWriteTextV1(encoder,(xstrview){input.Data+2,4},zipped,0,-1));
    assert(xl6DeflaterFinishV1(encoder,zipped) && xl6DeflaterDoneV1(encoder));
    assert(xl6DeflaterOutputSizeV1(encoder)==zipped->Size);
    assert(!xl6DeflaterFinishV1(encoder,zipped));error();
    assert(!xl6InflaterWriteBufferV1(decoder,zipped,zipped,false,-1));error();
    assert(!xl6InflaterFailedV1(decoder));
    assert(xl6InflaterWriteBufferV1(decoder,zipped,output,false,-1));
    assert(xl6InflaterFinishV1(decoder,output));
    assert(xl6InflaterDoneV1(decoder) && output->Size==input.Size && !memcmp(output->Data,input.Data,input.Size));
    assert(xl6InflaterOutputSizeV1(decoder)==input.Size);
    assert(xl6InflaterResetV1(decoder,2,3,65536,15));
    assert(!xl6InflaterWriteBufferV1(decoder,zipped,output,true,-1));error();
    assert(xl6InflaterFailedV1(decoder));
    assert(!xl6InflaterWriteBufferV1(decoder,zipped,output,true,-1));error();
    assert(xl6InflaterResetV1(decoder,2,-1,65536,15));assert(!xl6InflaterFailedV1(decoder));
    assert(xl6DeflaterResetV1(encoder,2,6,0,-1,15));assert(!xl6DeflaterDoneV1(encoder));
    assert(xl6DeflaterCloseV1(encoder) && xl6DeflaterClosedV1(encoder));
    assert(xl6DeflaterCloseV1(encoder));assert(!xl6DeflaterFinishV1(encoder,zipped));error();
    xl6DeflaterDestroyV1(encoder);xl6InflaterDestroyV1(decoder);xrtBufferDestroy(zipped);xrtBufferDestroy(output);
    assert(!xrtGetError());puts("compress formats, limits, truncation, streaming, alias, reset, close passed");return 0;
}
