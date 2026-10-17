#include "xl6_crypto_internal.h"

static bool digest(xbytesview key,xbytesview data,uint8 output[32])
{return xrtSha512_256(data.Data,data.Size,output);}

xvalue* xl6CryptoSha512_256TextV1(xstrview text,int64_t size)
{
    xbytesview key={0},input={0};uint8 output[32];
    if (!xl6TextSelectRaw(&text,size)) return NULL;
    input=(xbytesview){(const uint8*)text.Data,text.Size};
    bool ok=digest(key,input,output);
    xvalue* result=ok?xl6CryptoCopySecret(output,sizeof(output)):NULL;
    xrtSecureZero(output,sizeof(output));return result;
}
xstrview xl6CryptoSha512_256TextHexV1(xstrview text,int64_t size)
{
    xbytesview key={0},input={0};uint8 output[32];
    if (!xl6TextSelectRaw(&text,size)) return xl6TextFailure;
    input=(xbytesview){(const uint8*)text.Data,text.Size};
    bool ok=digest(key,input,output);
    xstrview result=ok?xl6CryptoHex(output,sizeof(output)):xl6TextFailure;
    xrtSecureZero(output,sizeof(output));return result;
}
xvalue* xl6CryptoSha512_256BytesV1(const xvalue* data,int64_t size)
{
    xbytesview key={0},input={0};uint8 output[32];
    if (!xl6BufferBytesInput(data,size,&input)) return NULL;
    bool ok=digest(key,input,output);
    xvalue* result=ok?xl6CryptoCopySecret(output,sizeof(output)):NULL;
    xrtSecureZero(output,sizeof(output));return result;
}
xstrview xl6CryptoSha512_256BytesHexV1(const xvalue* data,int64_t size)
{
    xbytesview key={0},input={0};uint8 output[32];
    if (!xl6BufferBytesInput(data,size,&input)) return xl6TextFailure;
    bool ok=digest(key,input,output);
    xstrview result=ok?xl6CryptoHex(output,sizeof(output)):xl6TextFailure;
    xrtSecureZero(output,sizeof(output));return result;
}
xvalue* xl6CryptoSha512_256BufferV1(void* data,int64_t size)
{
    xbytesview key={0},input={0};uint8 output[32];
    if (!xl6BufferInput(data,size,&input)) return NULL;
    bool ok=digest(key,input,output);
    xvalue* result=ok?xl6CryptoCopySecret(output,sizeof(output)):NULL;
    xrtSecureZero(output,sizeof(output));return result;
}
xstrview xl6CryptoSha512_256BufferHexV1(void* data,int64_t size)
{
    xbytesview key={0},input={0};uint8 output[32];
    if (!xl6BufferInput(data,size,&input)) return xl6TextFailure;
    bool ok=digest(key,input,output);
    xstrview result=ok?xl6CryptoHex(output,sizeof(output)):xl6TextFailure;
    xrtSecureZero(output,sizeof(output));return result;
}
