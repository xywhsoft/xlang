#include "xl6_x509_crl_profiles_internal.h"
static xvalue* number(void* raw,bool delta)
{
    const xx509crl* crl=xl6X509ProfileCrlV1(raw);xbytesview magnitude;if(!crl)return NULL;
    xx509result status=delta?xrtX509CrlDeltaBase(crl,&magnitude):xrtX509CrlNumber(crl,&magnitude);
    if(status==X509_DONE)return xrtValueRetain(xrtValueNull());
    return xl6X509OutcomeV1(status==X509_VALUE)?xl6X509BytesV1(magnitude):NULL;
}
xvalue* xl6X509CrlNumberV1(void* raw){return number(raw,false);}
xvalue* xl6X509CrlDeltaBaseV1(void* raw){return number(raw,true);}
