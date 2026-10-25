#include "src/runtime/xl6_text_value.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void one(int operation,size_t prefix,size_t* failures,size_t* successes)
{
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    uint8 keyData[32]={0},nonceData[12]={0};
    xbytesview key={keyData,32},nonce={nonceData,12},data={(const uint8*)"a\0b",3};
    xvalue* k=xrtValueBytes(key);xvalue* n=xrtValueBytes(nonce);xvalue* d=xrtValueBytes(data);
    xbuffer* b=xrtBufferCreate();assert(k && n && d && b && xrtBufferAppend(b,data));
    xstrview text={"a\0b",3};xvalue* result=NULL;
    assert(xrtMemDebugFailAfter(prefix));
    switch (operation) {
    case 0:result=xl6CryptoChaCha20TextV1(k,n,text,1,-1);break;
    case 1:result=xl6CryptoChaCha20BytesV1(k,n,d,1,-1);break;
    case 2:result=xl6CryptoChaCha20BufferV1(k,n,b,1,-1);break;
    case 3:result=xl6CryptoChaCha20TextSpanV1(key,nonce,text,1,-1);break;
    case 4:result=xl6CryptoChaCha20BytesSpanV1(key,nonce,data,1,-1);break;
    case 5:result=xl6CryptoChaCha20BufferSpanV1(key,nonce,b,1,-1);break;
    default:assert(0);
    }
    bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    if (hit) {assert(!result && xrtGetError());++*failures;}
    else {
        assert(result);xbytesview actual;uint8 expected[3];
        assert(xrtChaCha20(key.Data,nonce.Data,1,data.Data,expected,3));
        assert(xrtValueGetBytes(result,&actual) && actual.Size==3 && !memcmp(actual.Data,expected,3));
        ++*successes;
    }
    assert(b->Size==3 && !memcmp(b->Data,data.Data,3));
    xrtValueRelease(result);xrtValueRelease(k);xrtValueRelease(n);xrtValueRelease(d);
    xrtBufferDestroy(b);xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));size_t failures=0,successes=0;
    for (int operation=0;operation<6;++operation) {
        size_t f=failures,s=successes;
        for (size_t prefix=0;prefix<8;++prefix) one(operation,prefix,&failures,&successes);
        assert(failures>f && successes>s);
    }
    uint8 keyData[32]={0},nonceData[12]={0},data[65]={0};
    xbytesview key={keyData,32},nonce={nonceData,12},message={data,65};
    assert(!xl6CryptoChaCha20BytesSpanV1(key,nonce,message,UINT32_MAX,-1));
    assert(xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
    xvalue* last=xl6CryptoChaCha20BytesSpanV1(key,nonce,message,UINT32_MAX,64);assert(last);
    xrtValueRelease(last);xrtClearError();
    printf("chacha20 48 allocation prefixes: %zu failures, %zu completions; counter boundary balanced\n",failures,successes);
    return 0;
}
