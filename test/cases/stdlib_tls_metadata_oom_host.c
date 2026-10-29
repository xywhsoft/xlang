/* Synthetic immutable snapshot storage, real resident accessors/XRT only.
 * This leaf/OOM unit does not replace the actual-source loopback acceptance. */
#include "src/runtime/xl6_tls_metadata_internal.h"
#include "src/runtime/xl6_x509_abi.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint8_t protocol[]={'a',0,'b'};
static const char message[]={'e',0,'r'};
static XL6_TlsInfoV1* sample(const uint8_t* der,size_t count)
{
    XL6_TlsInfoV1* value=xrtCalloc(1,sizeof(*value)+sizeof(xbytesview)+sizeof(protocol)+sizeof(message)+count);
    assert(value);value->references=1;value->role=XTLS_CLIENT;value->ready=true;
    value->version=0x0304;value->cipher=0x1301;value->hasSelection=true;
    value->certificateCount=1;
    uint8_t* data=(uint8_t*)(value->certificates+1);
    memcpy(data,protocol,sizeof(protocol));value->protocol=(xbytesview){data,sizeof(protocol)};
    data+=sizeof(protocol);memcpy(data,message,sizeof(message));value->errorMessage=(xstrview){(const char*)data,sizeof(message)};
    data+=sizeof(message);memcpy(data,der,count);value->certificates[0]=(xbytesview){data,count};
    return value;
}
static void check(int operation,size_t point,const uint8_t* der,size_t count)
{
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    XL6_TlsInfoV1* value=sample(der,count);void* certificate=NULL;xvalue* bytes=NULL;xstrview text={0};
    assert(xl6TlsInfoRefV1(value)==value);xl6TlsInfoDestroyV1(value);
    assert(xl6TlsInfoReadyV1(value)&&xl6TlsInfoVersionV1(value)==0x0304);
    assert(xrtMemDebugFailAfter(point));
    if(operation==0)bytes=xl6TlsInfoProtocolV1(value);
    else if(operation==1)text=xl6TlsInfoErrorMessageV1(value);
    else certificate=xl6TlsInfoCertificateAtV1(value,0);
    bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    if(hit)assert(!bytes&&!text.Data&&!certificate&&xrtGetError());
    else if(operation==0){
        xbytesview view;assert(bytes&&xrtValueGetBytes(bytes,&view)&&view.Size==sizeof(protocol));
        assert(!memcmp(view.Data,protocol,sizeof(protocol))&&view.Data!=value->protocol.Data);
    }else if(operation==1)assert(text.Data&&text.Size==sizeof(message)&&!memcmp(text.Data,message,sizeof(message))&&text.Data[text.Size]==0&&text.Data!=value->errorMessage.Data);
    else assert(certificate);
    xerror* error=xrtTakeError();xl6TlsInfoDestroyV1(value);
    if(bytes){xbytesview view;assert(xrtValueGetBytes(bytes,&view)&&!memcmp(view.Data,protocol,sizeof(protocol)));}
    if(text.Data)assert(!memcmp(text.Data,message,sizeof(message))&&text.Data[text.Size]==0);
    if(certificate){
        xvalue* copy=xl6X509CertificateToDerV1(certificate);xbytesview view;
        assert(copy&&xrtValueGetBytes(copy,&view)&&view.Size==count&&!memcmp(view.Data,der,count));
        xrtValueRelease(copy);xl6X509CertificateDestroyV1(certificate);
    }
    xrtValueRelease(bytes);xrtFree((void*)text.Data);xrtErrorFree(error);xrtClearError();
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&before.UseAfterFreeCount==after.UseAfterFreeCount);
}
int main(int argc,char** argv)
{
    assert(argc==2&&xrtMemDebugEnable(true));
    /* Same complete domain list used by real module initialization. Catch
     * duplicate registrations before the more expensive language matrix. */
    const XL6_OwnershipDomainV1* domains[]={
#define XL6_OWNERSHIP_DOMAIN_ENTRY(Producer,Component,Getter) Getter(),
#include "src/runtime/xl6_ownership_domains.inc"
#undef XL6_OWNERSHIP_DOMAIN_ENTRY
    };
    size_t domainCount=sizeof(domains)/sizeof(*domains);
    for(size_t i=0;i<domainCount;i++)for(size_t j=0;j<i;j++)assert(domains[i]!=domains[j]);
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,domainCount);assert(catalog);
    xl6OwnershipCatalogDestroyV1(catalog);
    FILE* file=fopen(argv[1],"rb");assert(file&&fseek(file,0,SEEK_END)==0);
    long length=ftell(file);assert(length>0&&fseek(file,0,SEEK_SET)==0);
    uint8_t* der=malloc((size_t)length);assert(der&&fread(der,1,(size_t)length,file)==(size_t)length);fclose(file);
    check(2,SIZE_MAX,der,(size_t)length); /* Initialize shared parsers first. */
    for(int operation=0;operation<3;operation++)for(size_t point=0;point<32;point++)check(operation,point,der,(size_t)length);
    free(der);puts("TLS metadata leaf: 96 allocation prefixes balanced; detached NUL bytes and certificates survive snapshot release");return 0;
}
