#include "xl6_crypto_internal.h"

static bool digest(xbytesview key,xbytesview data,uint8 output[16])
{return xrtMd5(data.Data,data.Size,output);}

xvalue* xl6CryptoMd5TextV1(xstrview text,int64_t size)
{
    xbytesview key={0},input={0};uint8 output[16];
    if (!xl6TextSelectRaw(&text,size)) return NULL;
    input=(xbytesview){(const uint8*)text.Data,text.Size};
    bool ok=digest(key,input,output);
    xvalue* result=ok?xl6CryptoCopySecret(output,sizeof(output)):NULL;
    xrtSecureZero(output,sizeof(output));return result;
}
xstrview xl6CryptoMd5TextHexV1(xstrview text,int64_t size)
{
    xbytesview key={0},input={0};uint8 output[16];
    if (!xl6TextSelectRaw(&text,size)) return xl6TextFailure;
    input=(xbytesview){(const uint8*)text.Data,text.Size};
    bool ok=digest(key,input,output);
    xstrview result=ok?xl6CryptoHex(output,sizeof(output)):xl6TextFailure;
    xrtSecureZero(output,sizeof(output));return result;
}
xvalue* xl6CryptoMd5BytesV1(const xvalue* data,int64_t size)
{
    xbytesview key={0},input={0};uint8 output[16];
    if (!xl6BufferBytesInput(data,size,&input)) return NULL;
    bool ok=digest(key,input,output);
    xvalue* result=ok?xl6CryptoCopySecret(output,sizeof(output)):NULL;
    xrtSecureZero(output,sizeof(output));return result;
}
xstrview xl6CryptoMd5BytesHexV1(const xvalue* data,int64_t size)
{
    xbytesview key={0},input={0};uint8 output[16];
    if (!xl6BufferBytesInput(data,size,&input)) return xl6TextFailure;
    bool ok=digest(key,input,output);
    xstrview result=ok?xl6CryptoHex(output,sizeof(output)):xl6TextFailure;
    xrtSecureZero(output,sizeof(output));return result;
}
xvalue* xl6CryptoMd5BufferV1(void* data,int64_t size)
{
    xbytesview key={0},input={0};uint8 output[16];
    if (!xl6BufferInput(data,size,&input)) return NULL;
    bool ok=digest(key,input,output);
    xvalue* result=ok?xl6CryptoCopySecret(output,sizeof(output)):NULL;
    xrtSecureZero(output,sizeof(output));return result;
}
xstrview xl6CryptoMd5BufferHexV1(void* data,int64_t size)
{
    xbytesview key={0},input={0};uint8 output[16];
    if (!xl6BufferInput(data,size,&input)) return xl6TextFailure;
    bool ok=digest(key,input,output);
    xstrview result=ok?xl6CryptoHex(output,sizeof(output)):xl6TextFailure;
    xrtSecureZero(output,sizeof(output));return result;
}

/* Synchronous readonly span: the caller keeps the input owner alive. */
xvalue* xl6CryptoMd5BytesSpanV1(xbytesview data,int64_t size)
{
    xbytesview key={0},input={0};uint8 output[16];
    if (!xl6BufferSpanInput(data,size,&input)) return NULL;
    bool ok=digest(key,input,output);
    xvalue* result=ok?xl6CryptoCopySecret(output,sizeof(output)):NULL;
    xrtSecureZero(output,sizeof(output));return result;
}

/* Synchronous readonly span: the caller keeps the input owner alive. */
xstrview xl6CryptoMd5BytesHexSpanV1(xbytesview data,int64_t size)
{
    xbytesview key={0},input={0};uint8 output[16];
    if (!xl6BufferSpanInput(data,size,&input)) return xl6TextFailure;
    bool ok=digest(key,input,output);
    xstrview result=ok?xl6CryptoHex(output,sizeof(output)):xl6TextFailure;
    xrtSecureZero(output,sizeof(output));return result;
}
