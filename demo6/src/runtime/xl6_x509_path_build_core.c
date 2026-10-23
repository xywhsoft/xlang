#include "xl6_x509_path_build_internal.h"
static bool add(size_t* total,size_t size)
{
    if(size>SIZE_MAX-*total)return xl6X509FailV1(XERR_RANGE,"path source byte size overflow");
    *total+=size;return true;
}
void* xl6X509PathBuildCreateV1(void* rawTarget,void* const* candidates,size_t count,void* rawTrust,const xx509pathconfig* config,
    int64_t maxCertificates,int64_t maxBytes,int64_t maxCandidates,int64_t maxSourceBytes)
{
    if(xrtGetError())return NULL;
    if(!rawTarget || !rawTrust || (!candidates && count)){
        xl6X509FailV1(XERR_ARGUMENT,"path build needs target, candidate inputs and a stable trust snapshot");return NULL;
    }
    const xx509cert* target=&((const XL6_X509CertificateV1*)rawTarget)->view;
    const XL6_X509StoreVersionV1* trust=rawTrust;
    if(maxCertificates<0 || maxBytes<0 || maxCandidates<0 || maxSourceBytes<0){xl6X509FailV1(XERR_ARGUMENT,"negative path build budget");return NULL;}
    if(!xl6X509PathPolicyValidV1(config) || !xl6X509SpanV1(target->Raw))return NULL;
    size_t sourceCount=xrtX509StoreCount(trust->value),bytes=config->Purpose.Size;
    if(trust->bytes<0 || (uint64_t)trust->bytes>SIZE_MAX){xl6X509FailV1(XERR_STATE,"invalid trust snapshot byte size");return NULL;}
    if(!add(&sourceCount,count) || !xl6X509LimitV1(sourceCount,maxCandidates,"path source exceeds candidate budget") ||
        !add(&bytes,target->Raw.Size) || !add(&bytes,(size_t)trust->bytes))return NULL;
    for(size_t i=0;i<count;++i){
        if(!candidates[i]){xl6X509FailV1(XERR_ARGUMENT,"missing issuer candidate");return NULL;}
        xbytesview der=((const XL6_X509CertificateV1*)candidates[i])->view.Raw;
        if(!xl6X509SpanV1(der) || !add(&bytes,der.Size))return NULL;
    }
    if(!xl6X509LimitV1(bytes,maxSourceBytes,"path source exceeds byte budget"))return NULL;
    if(count==SIZE_MAX){xl6X509FailV1(XERR_RANGE,"path search capacity overflow");return NULL;}
    size_t capacity=count+1;
    if(maxCertificates && (uint64_t)maxCertificates<capacity)capacity=(size_t)maxCertificates;
    if(count>SIZE_MAX-capacity || count+capacity>SIZE_MAX/sizeof(xx509cert*)){
        xl6X509FailV1(XERR_RANGE,"path search index overflow");return NULL;
    }
    const xx509cert** views=xrtCalloc(count+capacity,sizeof(*views));if(!views)return NULL;
    const xx509cert** path=views+count;
    for(size_t i=0;i<count;++i)views[i]=&((const XL6_X509CertificateV1*)candidates[i])->view;
    xx509pathsource source={0};XL6_X509PathOutcomeV1* result=NULL;
    /* With no issuers, do not alias the empty issuer array and search scratch.
     * The native source contract still requires distinct nonempty arrays. */
    bool ok=xl6X509OutcomeV1(xrtX509StoreSource(trust->value,count?views:NULL,count,&source));
    if(ok){result=xrtCalloc(1,sizeof(*result));ok=result!=NULL;}
    if(ok){
        xx509pathconfig policy;XL6_X509PathCallV1 call;xl6X509PathCallBeginV1(&call,config,&policy);
        xx509pathdecision decision={0};xx509pathresult selected={0};
        xx509check check=xrtX509PathBuildCheck(target,&source,&policy,path,capacity,&selected,&decision);
        ok=xl6X509PathCallEndV1(&call,check);
        if(ok && check==X509_CHECK_VALID){
            xx509cert anchor;
            ok=(selected.Count>0 && selected.Count<=capacity && selected.Anchor) ||
                xl6X509FailV1(XERR_STATE,"native builder returned an invalid successful path");
            if(ok)ok=xl6X509OutcomeV1(xrtX509Parse(selected.Anchor->Certificate.Data,selected.Anchor->Certificate.Size,&anchor));
            if(ok){result->path=xl6X509PathSnapshotV1(path,selected.Count,&anchor,config,maxBytes);ok=result->path!=NULL;}
        }
        if(ok)ok=xl6X509PathDecisionV1(result,check,&decision,"x509-path-build");
    }
    xl6X509FreeV1(views);
    if(ok && !xrtGetError())return result;
    xl6X509PathOutcomeDestroyV1(result);return NULL;
}
