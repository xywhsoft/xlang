#include "src/runtime/xl6_text_value.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Exercise the actual DLL allocation paths, not a duplicate native provider.
 * Every triggered failure must leave no partial result and no live bytes. */
static void fault(int operation,size_t point)
{
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    uint8 keyData[32]={0},nonceData[12]={0};
    xvalue* key=xrtValueBytes((xbytesview){keyData,sizeof(keyData)});
    xvalue* nonce=xrtValueBytes((xbytesview){nonceData,sizeof(nonceData)});
    xvalue* data=xrtValueBytes((xbytesview){(const uint8*)"a\0b",3});
    xvalue* empty=xrtValueBytes((xbytesview){NULL,0});assert(key && nonce && data && empty);
    void* aes=xl6CryptoAesGcmCreateV1(key);assert(aes);
    void* chacha=xl6CryptoChaChaAeadCreateV1(key);assert(chacha);
    void* stream=xl6CryptoHmacSha256CreateV1(key);assert(stream);
    xvalue* encrypted=xl6CryptoAesGcmSealV1(aes,data,nonce,empty,-1,-1);assert(encrypted);
    xvalue* result=NULL;void* object=NULL;xstrview text={0};
    assert(xrtMemDebugFailAfter(point));
    switch (operation) {
    case 0:result=xl6CryptoSha256BytesV1(data,-1);break;
    case 1:text=xl6CryptoSha256TextHexV1((xstrview){"a\0b",3},-1);break;
    case 2:object=xl6CryptoHmacSha256CreateV1(key);break;
    case 3:object=xl6CryptoHmacSha256CloneV1(stream);break;
    case 4:result=xl6CryptoHmacSha256FinalV1(stream);break;
    case 5:result=xl6CryptoPbkdf2Sha256V1(data,key,2,64);break;
    case 6:result=xl6CryptoHkdfSha512V1(key,empty,data,64);break;
    case 7:object=xl6CryptoAesGcmCreateV1(key);break;
    case 8:result=xl6CryptoAesGcmSealV1(aes,data,nonce,empty,-1,-1);break;
    case 9:result=xl6CryptoAesGcmOpenV1(aes,encrypted,nonce,empty,-1,-1);break;
    case 10:object=xl6CryptoChaChaAeadCreateV1(key);break;
    case 11:result=xl6CryptoChaChaAeadSealV1(chacha,data,nonce,empty,-1,-1);break;
    default:assert(0);
    }
    bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    if (hit) {assert(!result && !object && !text.Data);assert(xrtGetError());}
    else assert(result || object || text.Data);
    xerror* failure=xrtTakeError();xrtValueRelease(result);xrtFree((void*)text.Data);
    if (operation==2 || operation==3) xl6CryptoHmacSha256DestroyV1(object);
    if (operation==7) xl6CryptoAesGcmDestroyV1(object);
    if (operation==10) xl6CryptoChaChaAeadDestroyV1(object);
    xl6CryptoHmacSha256DestroyV1(stream);xl6CryptoAesGcmDestroyV1(aes);xl6CryptoChaChaAeadDestroyV1(chacha);
    xrtValueRelease(key);xrtValueRelease(nonce);xrtValueRelease(data);xrtValueRelease(empty);xrtValueRelease(encrypted);
    xrtErrorFree(failure);xrtClearError();xrtMemDebugSnapshot(&after);
    if (before.LiveCount!=after.LiveCount || before.LiveBytes!=after.LiveBytes) {
        fprintf(stderr,"operation=%d point=%zu live count=%lld bytes=%lld\n",operation,point,
            (long long)(after.LiveCount-before.LiveCount),(long long)(after.LiveBytes-before.LiveBytes));assert(0);
    }
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    for (int operation=0;operation<12;++operation) for (size_t point=0;point<32;++point) fault(operation,point);
    puts("crypto 384 allocation prefixes balanced");return 0;
}
