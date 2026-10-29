#include "src/runtime/xl6_text_value.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static void same(xstrview text,const char* expected,size_t size)
{assert(text.Data && text.Size==size && !memcmp(text.Data,expected,size));xrtFree((void*)text.Data);}
static void bytesEqual(xvalue* value,const unsigned char* expected,size_t size)
{xbytesview view;assert(value && xrtValueGetBytes(value,&view) && view.Size==size && !memcmp(view.Data,expected,size));xrtValueRelease(value);}
int main(void)
{
    xstrview raw={"a\0b",3},empty={"",0};
    same(xl6CodecHexTextV1(raw,-1,0),"610062",6);
    same(xl6CodecHexTextV1((xstrview){"\xff",1},-1,1),"FF",2);
    bytesEqual(xl6CodecHexDecodeV1((xstrview){"61 00 ff",8},-1,2),(const unsigned char*)"a\0\xff",3);
    same(xl6CodecBase64TextV1(raw,-1,0,empty,-1),"YQBi",4);
    bytesEqual(xl6CodecBase64DecodeV1((xstrview){"YQBi",4},-1,0,empty,-1),(const unsigned char*)raw.Data,3);
    same(xl6CodecBase64TextV1((xstrview){"\xff",1},-1,3,empty,-1),"_w",2);
    bytesEqual(xl6CodecBase64DecodeV1((xstrview){"_w",2},-1,9,empty,-1),(const unsigned char*)"\xff",1);
    same(xl6CodecPercentTextV1((xstrview){"a +\0",4},-1,empty,false,true,-1),"a%20%2B%00",10);
    same(xl6CodecPercentTextV1((xstrview){"a +\0",4},-1,(xstrview){"+",1},true,true,-1),"a+%2B%00",8);
    bytesEqual(xl6CodecPercentDecodeV1((xstrview){"a+%2B%00",8},-1,true),(const unsigned char*)"a +\0",4);
    same(xl6CodecHexTextV1(raw,99,0),"610062",6);
    assert(!xl6CodecHexDecodeV1((xstrview){"g0",2},-1,0));assert(xrtGetError());xrtClearError();
    assert(!xl6CodecHexTextV1(raw,-2,0).Data);assert(xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
    assert(!xl6CodecBase64DecodeV1((xstrview){"/x==",4},-1,0,empty,-1));assert(xrtGetError());xrtClearError();
    assert(!xl6CodecPercentDecodeV1((xstrview){"%0",2},-1,false));assert(xrtGetError());xrtClearError();
    xvalue* input=xrtValueBytes((xbytesview){(const uint8*)raw.Data,raw.Size});assert(input);
    same(xl6CodecHexBytesV1(input,-1,0),"610062",6);
    same(xl6CodecBase64BytesV1(input,-1,0,empty,-1),"YQBi",4);
    same(xl6CodecPercentBytesV1(input,-1,empty,false,true,-1),"a%00b",5);
    xrtValueRelease(input);
    xstrview alphabet=XRT_STR_LITERAL("ZYXABCDEFGHIJKLMNOPQRSTUVWzyxabcdefghijklmnopqrstuvw0123456789-_");
    assert(alphabet.Size==64);
    xstrview custom=xl6CodecBase64TextV1(raw,-1,0,alphabet,-1);assert(custom.Data);
    bytesEqual(xl6CodecBase64DecodeV1(custom,-1,0,alphabet,-1),(const unsigned char*)raw.Data,raw.Size);
    xrtFree((void*)custom.Data);
    assert(!xl6CodecBase64TextV1(raw,-1,0,XRT_STR_LITERAL("bad"),-1).Data);assert(xrtGetError());xrtClearError();
    assert(!xl6CodecPercentTextV1(raw,-1,(xstrview){"/\0",2},false,true,-1).Data);assert(xrtGetError());xrtClearError();
    same(xl6CodecPercentTextV1((xstrview){"/",1},-1,(xstrview){"/\0",2},false,true,1),"/",1);
    puts("codec resident exact bytes, flags, form, checked errors passed");return 0;
}
