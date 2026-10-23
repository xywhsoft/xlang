#include "xl6_x509_path_internal.h"
void* xl6X509PathValidateCreateV1(void* const* certificates,size_t count,void* anchor,const xx509pathconfig* config,
    int64_t maxCertificates,int64_t maxBytes)
{
    if(xrtGetError())return NULL;
    if(!certificates || !count || !anchor){xl6X509FailV1(XERR_ARGUMENT,"path needs ordered certificates and an independent explicit anchor");return NULL;}
    if(!xl6X509LimitV1(count,maxCertificates,"path exceeds certificate budget") ||
        !xl6X509LimitV1(config?config->Purpose.Size:0,maxBytes,"path purpose exceeds byte budget") ||
        !xl6X509PathPolicyValidV1(config))return NULL;
    const xx509cert* local[16];const xx509cert** views=local;
    if(count>sizeof(local)/sizeof(*local)){
        if(count>SIZE_MAX/sizeof(*views)){xl6X509FailV1(XERR_RANGE,"path input index overflow");return NULL;}
        views=xrtCalloc(count,sizeof(*views));if(!views)return NULL;
    }
    for(size_t i=0;i<count;++i)views[i]=certificates[i]?&((const XL6_X509CertificateV1*)certificates[i])->view:NULL;
    XL6_X509PathOutcomeV1* result=xrtCalloc(1,sizeof(*result));
    if(result)result->path=xl6X509PathSnapshotV1(views,count,&((const XL6_X509AnchorV1*)anchor)->certificate,config,maxBytes);
    if(views!=local)xl6X509FreeV1(views);
    if(!result || !result->path){xl6X509PathOutcomeDestroyV1(result);return NULL;}
    const xx509cert** index=(const xx509cert**)(result->path->certificates+count);
    xx509pathconfig policy;XL6_X509PathCallV1 call;
    xl6X509PathCallBeginV1(&call,config,&policy);
    policy.Purpose=result->path->policy.Purpose;
    xx509pathdecision decision={0};xx509check check=xrtX509PathCheck(index,count,&result->path->anchor,&policy,&decision);
    if(xl6X509PathCallEndV1(&call,check) && xl6X509PathDecisionV1(result,check,&decision,"x509-path-check"))return result;
    xl6X509PathOutcomeDestroyV1(result);return NULL;
}
