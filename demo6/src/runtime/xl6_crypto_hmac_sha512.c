#include "xl6_crypto_internal.h"

static bool digest(xbytesview key,xbytesview data,uint8 output[64])
{return xrtHmacSha512(key.Data,key.Size,data.Data,data.Size,output);}

xvalue* xl6CryptoHmacSha512TextV1(const xvalue* keyValue,xstrview text,int64_t size)
{
    xbytesview key={0},input={0};uint8 output[64];
    if (!xl6CryptoMaterial(keyValue,&key) || !xl6TextSelectRaw(&text,size)) return NULL;
    input=(xbytesview){(const uint8*)text.Data,text.Size};
    bool ok=digest(key,input,output);
    xvalue* result=ok?xl6CryptoCopySecret(output,sizeof(output)):NULL;
    xrtSecureZero(output,sizeof(output));return result;
}
xstrview xl6CryptoHmacSha512TextHexV1(const xvalue* keyValue,xstrview text,int64_t size)
{
    xbytesview key={0},input={0};uint8 output[64];
    if (!xl6CryptoMaterial(keyValue,&key) || !xl6TextSelectRaw(&text,size)) return xl6TextFailure;
    input=(xbytesview){(const uint8*)text.Data,text.Size};
    bool ok=digest(key,input,output);
    xstrview result=ok?xl6CryptoHex(output,sizeof(output)):xl6TextFailure;
    xrtSecureZero(output,sizeof(output));return result;
}
xvalue* xl6CryptoHmacSha512BytesV1(const xvalue* keyValue,const xvalue* data,int64_t size)
{
    xbytesview key={0},input={0};uint8 output[64];
    if (!xl6CryptoMaterial(keyValue,&key) || !xl6BufferBytesInput(data,size,&input)) return NULL;
    bool ok=digest(key,input,output);
    xvalue* result=ok?xl6CryptoCopySecret(output,sizeof(output)):NULL;
    xrtSecureZero(output,sizeof(output));return result;
}
xstrview xl6CryptoHmacSha512BytesHexV1(const xvalue* keyValue,const xvalue* data,int64_t size)
{
    xbytesview key={0},input={0};uint8 output[64];
    if (!xl6CryptoMaterial(keyValue,&key) || !xl6BufferBytesInput(data,size,&input)) return xl6TextFailure;
    bool ok=digest(key,input,output);
    xstrview result=ok?xl6CryptoHex(output,sizeof(output)):xl6TextFailure;
    xrtSecureZero(output,sizeof(output));return result;
}
xvalue* xl6CryptoHmacSha512BufferV1(const xvalue* keyValue,void* data,int64_t size)
{
    xbytesview key={0},input={0};uint8 output[64];
    if (!xl6CryptoMaterial(keyValue,&key) || !xl6BufferInput(data,size,&input)) return NULL;
    bool ok=digest(key,input,output);
    xvalue* result=ok?xl6CryptoCopySecret(output,sizeof(output)):NULL;
    xrtSecureZero(output,sizeof(output));return result;
}
xstrview xl6CryptoHmacSha512BufferHexV1(const xvalue* keyValue,void* data,int64_t size)
{
    xbytesview key={0},input={0};uint8 output[64];
    if (!xl6CryptoMaterial(keyValue,&key) || !xl6BufferInput(data,size,&input)) return xl6TextFailure;
    bool ok=digest(key,input,output);
    xstrview result=ok?xl6CryptoHex(output,sizeof(output)):xl6TextFailure;
    xrtSecureZero(output,sizeof(output));return result;
}

/* Synchronous readonly span: the caller keeps the input owner alive. */
xvalue* xl6CryptoHmacSha512TextSpanV1(xbytesview keyValue,xstrview text,int64_t size)
{
    xbytesview key={0},input={0};uint8 output[64];
    if (!xl6BufferSpanInput(keyValue,-1,&key) || !xl6TextSelectRaw(&text,size)) return NULL;
    input=(xbytesview){(const uint8*)text.Data,text.Size};
    bool ok=digest(key,input,output);
    xvalue* result=ok?xl6CryptoCopySecret(output,sizeof(output)):NULL;
    xrtSecureZero(output,sizeof(output));return result;
}

/* Synchronous readonly span: the caller keeps the input owner alive. */
xstrview xl6CryptoHmacSha512TextHexSpanV1(xbytesview keyValue,xstrview text,int64_t size)
{
    xbytesview key={0},input={0};uint8 output[64];
    if (!xl6BufferSpanInput(keyValue,-1,&key) || !xl6TextSelectRaw(&text,size)) return xl6TextFailure;
    input=(xbytesview){(const uint8*)text.Data,text.Size};
    bool ok=digest(key,input,output);
    xstrview result=ok?xl6CryptoHex(output,sizeof(output)):xl6TextFailure;
    xrtSecureZero(output,sizeof(output));return result;
}

/* Synchronous readonly span: the caller keeps the input owner alive. */
xvalue* xl6CryptoHmacSha512BytesSpanV1(xbytesview keyValue,xbytesview data,int64_t size)
{
    xbytesview key={0},input={0};uint8 output[64];
    if (!xl6BufferSpanInput(keyValue,-1,&key) || !xl6BufferSpanInput(data,size,&input)) return NULL;
    bool ok=digest(key,input,output);
    xvalue* result=ok?xl6CryptoCopySecret(output,sizeof(output)):NULL;
    xrtSecureZero(output,sizeof(output));return result;
}

/* Synchronous readonly span: the caller keeps the input owner alive. */
xstrview xl6CryptoHmacSha512BytesHexSpanV1(xbytesview keyValue,xbytesview data,int64_t size)
{
    xbytesview key={0},input={0};uint8 output[64];
    if (!xl6BufferSpanInput(keyValue,-1,&key) || !xl6BufferSpanInput(data,size,&input)) return xl6TextFailure;
    bool ok=digest(key,input,output);
    xstrview result=ok?xl6CryptoHex(output,sizeof(output)):xl6TextFailure;
    xrtSecureZero(output,sizeof(output));return result;
}

/* Synchronous readonly span: the caller keeps the input owner alive. */
xvalue* xl6CryptoHmacSha512BufferSpanV1(xbytesview keyValue,void* data,int64_t size)
{
    xbytesview key={0},input={0};uint8 output[64];
    if (!xl6BufferSpanInput(keyValue,-1,&key) || !xl6BufferInput(data,size,&input)) return NULL;
    bool ok=digest(key,input,output);
    xvalue* result=ok?xl6CryptoCopySecret(output,sizeof(output)):NULL;
    xrtSecureZero(output,sizeof(output));return result;
}

/* Synchronous readonly span: the caller keeps the input owner alive. */
xstrview xl6CryptoHmacSha512BufferHexSpanV1(xbytesview keyValue,void* data,int64_t size)
{
    xbytesview key={0},input={0};uint8 output[64];
    if (!xl6BufferSpanInput(keyValue,-1,&key) || !xl6BufferInput(data,size,&input)) return xl6TextFailure;
    bool ok=digest(key,input,output);
    xstrview result=ok?xl6CryptoHex(output,sizeof(output)):xl6TextFailure;
    xrtSecureZero(output,sizeof(output));return result;
}
