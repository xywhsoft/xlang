#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_native_handle.h"
#include "test/support/ownership_test_thread.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const uint8 polyKey[32]={0x85,0xd6,0xbe,0x78,0x57,0x55,0x6d,0x33,
    0x7f,0x44,0x52,0xfe,0x42,0xd5,0x06,0xa8,0x01,0x03,0x80,0x8a,0xfb,0x0d,
    0xb2,0xfd,0x4a,0xbf,0xf6,0xaf,0x41,0x49,0xf5,0x1b};
static const uint8 polyTag[16]={0xa8,0x06,0x1d,0xc1,0x30,0x51,0x36,0xc6,
    0xc2,0x2b,0x8b,0xaf,0x0c,0x01,0x27,0xa9};
static const char message[]="Cryptographic Forum Research Group";
static void equals(xvalue* value,const void* expected,size_t size)
{
    xbytesview view;assert(value&&xrtValueGetBytes(value,&view)&&view.Size==size);
    assert(!size||!memcmp(view.Data,expected,size));xrtValueRelease(value);
}
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount&&after.LiveBytes==before->LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(after.InvalidFreeCount==before->InvalidFreeCount&&after.DoubleFreeCount==before->DoubleFreeCount&&
        after.UseAfterFreeCount==before->UseAfterFreeCount);
}
static void ordinary(void)
{
    uint8 keyData[32],plain[16],cipher[16];
    for(size_t i=0;i<sizeof(keyData);++i)keyData[i]=(uint8)i;
    for(size_t i=0;i<sizeof(plain);++i)plain[i]=(uint8)(i*17);
    for(size_t size=16;size<=32;size+=8) {
        xvalue* key=xrtValueBytes((xbytesview){keyData,size});assert(key);
        void* aes=xl6CryptoAesBlockCreateV1(key);assert(aes);
        xaes reference;assert(xrtAesInit(&reference,keyData,size)&&xrtAesEncrypt(&reference,plain,cipher));
        xvalue* block=xrtValueBytes((xbytesview){plain,16});assert(block);
        xvalue* encrypted=xl6CryptoAesBlockEncryptV1(aes,block);assert(encrypted);
        equals(xl6CryptoAesBlockDecryptV1(aes,encrypted),plain,16);
        equals(encrypted,cipher,16);equals(xl6CryptoAesBlockEncryptSpanV1(aes,(xbytesview){plain,16}),cipher,16);
        equals(xl6CryptoAesBlockDecryptSpanV1(aes,(xbytesview){cipher,16}),plain,16);
        xbuffer buffer;assert(xrtBufferInit(&buffer)&&xrtBufferAppend(&buffer,(xbytesview){plain,16}));
        equals(xl6CryptoAesBlockEncryptBufferV1(aes,&buffer),cipher,16);
        memcpy(buffer.Data,cipher,16);equals(xl6CryptoAesBlockDecryptBufferV1(aes,&buffer),plain,16);
        xrtBufferUnit(&buffer);xrtValueRelease(block);xrtValueRelease(key);
        xrtSetErrorKind(XERR_RANGE);const xerror* cause=xrtGetError();xl6CryptoAesBlockDestroyV1(aes);
        assert(xrtGetError()==cause);xrtClearError();xrtAesClear(&reference);
    }
    xbytesview key={polyKey,32},input={(const uint8*)message,sizeof(message)-1};
    xvalue* keyValue=xrtValueBytes(key);xvalue* data=xrtValueBytes(input);assert(keyValue&&data);
    xbuffer buffer;assert(xrtBufferInit(&buffer)&&xrtBufferAppend(&buffer,input));
    equals(xl6CryptoPoly1305TextSpanV1(key,(xstrview){message,input.Size},999),polyTag,16);
    equals(xl6CryptoPoly1305BytesSpanV1(key,input,999),polyTag,16);
    equals(xl6CryptoPoly1305BufferSpanV1(key,&buffer,999),polyTag,16);
    equals(xl6CryptoPoly1305TextV1(keyValue,(xstrview){message,input.Size},-1),polyTag,16);
    equals(xl6CryptoPoly1305BytesV1(keyValue,data,-1),polyTag,16);
    equals(xl6CryptoPoly1305BufferV1(keyValue,&buffer,-1),polyTag,16);
    void* stream=xl6CryptoPoly1305CreateV1(keyValue);assert(stream);
    xl6CryptoPoly1305UpdateTextV1(stream,(xstrview){message,14},-1);
    xl6CryptoPoly1305UpdateBytesSpanV1(stream,(xbytesview){(const uint8*)message+14,input.Size-14},-1);
    equals(xl6CryptoPoly1305FinalV1(stream),polyTag,16);
    equals(xl6CryptoPoly1305FinalV1(stream),polyTag,16);
    xl6CryptoPoly1305ResetV1(stream,keyValue);xl6CryptoPoly1305UpdateBytesV1(stream,data,999);
    equals(xl6CryptoPoly1305FinalV1(stream),polyTag,16);
    xl6CryptoPoly1305ResetSpanV1(stream,key);xl6CryptoPoly1305UpdateBufferV1(stream,&buffer,-1);
    equals(xl6CryptoPoly1305FinalV1(stream),polyTag,16);
    xl6CryptoPoly1305ResetSpanV1(stream,(xbytesview){polyKey,31});assert(xrtErrorKind(xrtGetError())==XERR_ARGUMENT);
    xrtClearError();equals(xl6CryptoPoly1305FinalV1(stream),polyTag,16);
    xrtSetErrorKind(XERR_RANGE);const xerror* cause=xrtGetError();xl6CryptoPoly1305DestroyV1(stream);
    assert(xrtGetError()==cause);xrtClearError();xrtBufferUnit(&buffer);xrtValueRelease(data);xrtValueRelease(keyValue);
}
typedef struct Cross {void* stream;void* aes;} Cross;
static void foreignThread(void* raw)
{
    Cross* state=raw;
    assert(!xl6CryptoPoly1305FinalV1(state->stream)&&xrtErrorKind(xrtGetError())==XERR_STATE);
    xrtClearError();uint8 block[16]={0};xvalue* result=xl6CryptoAesBlockEncryptSpanV1(state->aes,(xbytesview){block,16});
    assert(result);xrtValueRelease(result);assert(!xrtGetError());
}
static void fault(int operation,size_t prefix)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    xbytesview key={polyKey,32};void* stream=xl6CryptoPoly1305CreateSpanV1(key);assert(stream);
    void* aes=xl6CryptoAesBlockCreateSpanV1(key);assert(aes);
    xvalue* result=NULL;void* object=NULL;void* slot=NULL;uint8 block[16]={0};
    assert(xrtMemDebugFailAfter(prefix));
    switch(operation) {
    case 0:object=xl6CryptoAesBlockCreateSpanV1(key);break;
    case 1:object=xl6CryptoPoly1305CreateSpanV1(key);break;
    case 2:result=xl6CryptoAesBlockEncryptSpanV1(aes,(xbytesview){block,16});break;
    case 3:result=xl6CryptoAesBlockDecryptSpanV1(aes,(xbytesview){block,16});break;
    case 4:result=xl6CryptoPoly1305BytesSpanV1(key,(xbytesview){block,16},-1);break;
    case 5:result=xl6CryptoPoly1305FinalV1(stream);break;
    case 6:slot=aes;aes=NULL;result=xl6NativeHandleTakeV1(&slot,xl6CryptoAesBlockHandlePolicyV1Get());assert(!slot);break;
    case 7:slot=stream;stream=NULL;result=xl6NativeHandleTakeV1(&slot,xl6CryptoPoly1305HandlePolicyV1Get());assert(!slot);break;
    default:assert(0);
    }
    bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    if(hit){assert(!result&&!object&&xrtGetError());}else assert(result||object);
    xerror* failure=xrtTakeError();xrtValueRelease(result);
    if(operation==0)xl6CryptoAesBlockDestroyV1(object);
    if(operation==1)xl6CryptoPoly1305DestroyV1(object);
    xl6CryptoAesBlockDestroyV1(aes);xl6CryptoPoly1305DestroyV1(stream);
    xrtErrorFree(failure);xrtClearError();balance(&before);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    ordinary();balance(&before);
    Cross cross={xl6CryptoPoly1305CreateSpanV1((xbytesview){polyKey,32}),
        xl6CryptoAesBlockCreateSpanV1((xbytesview){polyKey,32})};assert(cross.stream&&cross.aes);
    xl6OwnershipTestThreadRun(foreignThread,&cross);
    xl6CryptoPoly1305DestroyV1(cross.stream);xl6CryptoAesBlockDestroyV1(cross.aes);balance(&before);
    for(int operation=0;operation<8;++operation)for(size_t prefix=0;prefix<24;++prefix)fault(operation,prefix);
    puts("advanced crypto: normal vectors and ABI paths; 192 OOM prefixes; memory balanced");return 0;
}
