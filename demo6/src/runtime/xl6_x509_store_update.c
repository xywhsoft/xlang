#include "xl6_x509_store_internal.h"
#include "xl6_text_internal.h"

typedef struct Write {
    XL6_X509StoreV1* store;
    XL6_X509StoreVersionV1* version;
    size_t before;
} Write;
static bool begin(void* raw,Write* write)
{
    if(xrtGetError())return false;
    if(!raw)return xl6X509FailV1(XERR_ARGUMENT,"missing trust store");
    write->store=raw;
    if(!xl6X509OutcomeV1(xrtMutexLock(&write->store->lock)))return false;
    write->version=write->store->current;write->before=xrtX509StoreCount(write->version->value);return true;
}
static bool bounded(Write* write,uint64_t upper)
{
    return upper<=(uint64_t)(INT64_MAX-write->version->bytes) ||
        xl6X509FailV1(XERR_RANGE,"trust store input exceeds representable total bytes");
}
static bool writable(Write* write)
{
    if(xrtAtomic64Load(&write->version->references,XMEMORY_ACQUIRE)==1)return true;
    xx509store* copy=xrtX509StoreClone(write->version->value);if(!copy)return false;
    XL6_X509StoreVersionV1* next=xl6X509StoreVersionTakeV1(&copy,write->version->bytes);
    if(!next)return false;
    write->version=next;return true;
}
static int64_t finish(Write* write,bool ok)
{
    ok=xl6X509OutcomeV1(ok);
    XL6_X509StoreVersionV1* drop=NULL;int64_t added=0;
    if(ok){
        size_t count=xrtX509StoreCount(write->version->value);
        added=(int64_t)(count-write->before);
        /* Each accepted DER is nonempty. Before mutation, the input byte
         * upper bound was checked; PEM DER cannot exceed its input text,
         * and a merge's new DER is a subset of its immutable source. */
        for(size_t i=write->before;i<count;++i)
            write->version->bytes+=(int64_t)xrtX509StoreCertificate(write->version->value,i)->Raw.Size;
        if(write->version!=write->store->current){drop=write->store->current;write->store->current=write->version;}
    }else if(write->version!=write->store->current)drop=write->version;
    xerror* error=xrtTakeError();bool unlocked=xl6X509OutcomeV1(xrtMutexUnlock(&write->store->lock));
    if(error){xrtClearError();xrtSetErrorTake(error);}
    xl6X509StoreSnapshotDestroyV1(drop);
    return ok && unlocked && !xrtGetError()?added:0;
}
bool xl6X509StoreAddV1(void* raw,void* certificate)
{
    if(!certificate)return xl6X509FailV1(XERR_ARGUMENT,"missing trust anchor certificate");
    xbytesview der=((const XL6_X509CertificateV1*)certificate)->view.Raw;
    if(!xl6X509SpanV1(der))return false;
    Write write={0};if(!begin(raw,&write))return false;
    bool ok=bounded(&write,der.Size) && writable(&write) &&
        xrtX509StoreAdd(write.version->value,der.Data,der.Size)!=X509_ERROR;
    return finish(&write,ok)!=0;
}
int64_t xl6X509StoreAddPemV1(void* raw,xstrview text,int64_t size)
{
    if(!xl6TextSelectRaw(&text,size))return 0;
    Write write={0};if(!begin(raw,&write))return 0;
    bool ok=bounded(&write,text.Size) && writable(&write) &&
        xrtX509StoreAddPem(write.version->value,text.Data,text.Size,NULL);
    return finish(&write,ok);
}
int64_t xl6X509StoreMergeV1(void* raw,void* source)
{
    if(!source){xl6X509FailV1(XERR_ARGUMENT,"missing immutable trust store source");return 0;}
    const XL6_X509StoreVersionV1* input=source;Write write={0};if(!begin(raw,&write))return 0;
    if(input==write.version)return finish(&write,true);
    bool ok=bounded(&write,(uint64_t)input->bytes) && writable(&write) &&
        xrtX509StoreMerge(write.version->value,input->value,NULL);
    return finish(&write,ok);
}
void xl6X509StoreClearV1(void* raw)
{
    if(xrtGetError())return;
    if(!raw){xl6X509FailV1(XERR_ARGUMENT,"missing trust store");return;}
    xx509store* value=xrtX509StoreCreate();if(!value)return;
    XL6_X509StoreVersionV1* next=xl6X509StoreVersionTakeV1(&value,0);if(!next)return;
    Write write={0};
    if(!begin(raw,&write)){xl6X509StoreSnapshotDestroyV1(next);return;}
    XL6_X509StoreVersionV1* previous=write.store->current;write.store->current=next;
    xerror* error=xrtTakeError();(void)xl6X509OutcomeV1(xrtMutexUnlock(&write.store->lock));
    if(error){xrtClearError();xrtSetErrorTake(error);}
    xl6X509StoreSnapshotDestroyV1(previous);
}
