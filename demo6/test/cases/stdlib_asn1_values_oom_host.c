#include "src/runtime/xl6_text_value.h"
#include <assert.h>
#include <stdio.h>
static void fault(int operation,size_t point)
{
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    const uint8 der[]={0x30,6,2,1,1,2,1,2};
    xvalue* input=xrtValueBytes((xbytesview){der,sizeof(der)});assert(input);
    void* value=xl6DerParseV1(input,-1,1024);assert(value);
    void* reader=xl6DerReaderCreateV1(input,-1,1024);assert(reader);
    void* builder=xl6DerBuilderCreateV1(1024);assert(builder);
    xstrview pem=xl6PemEncodeV1(XRT_STR_LITERAL("TEST"),input,-1);assert(pem.Data);
    void* block=xl6PemFindV1(pem,XRT_STR_LITERAL(""),-1,1024);assert(block);
    void* pemReader=xl6PemReaderCreateV1(pem,-1,1024);assert(pemReader);
    xvalue* result=NULL;void* object=NULL;xstrview text={0};
    assert(xrtMemDebugFailAfter(point));
    switch(operation) {
    case 0:object=xl6DerParseV1(input,-1,1024);break;
    case 1:object=xl6DerReaderCreateV1(input,-1,1024);break;
    case 2:object=xl6DerReaderReadV1(reader);break;
    case 3:object=xl6DerValueReaderV1(value);break;
    case 4:object=xl6DerBuilderCreateV1(1024);break;
    case 5:xl6DerBuilderOidV1(builder,XRT_STR_LITERAL("1.2.840.113549"),-1);break;
    case 6:text=xl6PemEncodeV1(XRT_STR_LITERAL("TEST"),input,-1);break;
    case 7:object=xl6PemFindV1(pem,XRT_STR_LITERAL("TEST"),-1,1024);break;
    case 8:result=xl6PemBlockDecodeV1(block);break;
    case 9:object=xl6PemReaderCreateV1(pem,-1,1024);break;
    case 10:object=xl6PemReaderReadV1(pemReader);break;
    default:assert(0);
    }
    bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    if(hit) assert(!result && !object && !text.Data);
    else if(operation!=5) assert(result || object || text.Data);
    xerror* failure=xrtTakeError();
    if(hit && operation==2) assert(xl6DerReaderRemainingV1(reader)==(int64_t)sizeof(der));
    if(hit && operation==10) assert(!xl6PemReaderDoneV1(pemReader));
    if(hit && operation==5) {
        xvalue* state=xl6DerBuilderFinishV1(builder);xbytesview bytes;
        assert(state && xrtValueGetBytes(state,&bytes) && bytes.Size==0);xrtValueRelease(state);
    }
    xrtValueRelease(result);xrtFree((void*)text.Data);
    if(operation==0 || operation==2) xl6DerValueDestroyV1(object);
    if(operation==1 || operation==3) xl6DerReaderDestroyV1(object);
    if(operation==4) xl6DerBuilderDestroyV1(object);
    if(operation==7 || operation==10) xl6PemBlockDestroyV1(object);
    if(operation==9) xl6PemReaderDestroyV1(object);
    xl6DerValueDestroyV1(value);xl6DerReaderDestroyV1(reader);xl6DerBuilderDestroyV1(builder);
    xl6PemBlockDestroyV1(block);xl6PemReaderDestroyV1(pemReader);xrtValueRelease(input);xrtFree((void*)pem.Data);
    xrtErrorFree(failure);xrtClearError();xrtMemDebugSnapshot(&after);
    if(before.LiveCount!=after.LiveCount || before.LiveBytes!=after.LiveBytes) {
        fprintf(stderr,"operation=%d point=%zu leak count=%lld bytes=%lld\n",operation,point,
            (long long)(after.LiveCount-before.LiveCount),(long long)(after.LiveBytes-before.LiveBytes));assert(0);
    }
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    for(int operation=0;operation<11;++operation) for(size_t point=0;point<32;++point) fault(operation,point);
    puts("ASN.1/PEM 352 allocation prefixes balanced; readers do not advance on failed allocation");return 0;
}
