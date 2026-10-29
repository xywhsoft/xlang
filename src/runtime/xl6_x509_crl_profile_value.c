#include "xl6_x509_crl_profiles_internal.h"
bool xl6X509CrlVersionCheckV1(int64_t value,xx509crlversion* output)
{
    if(!output || (value!=X509_CRL_VERSION_1 && value!=X509_CRL_VERSION_2))
        return xl6X509FailV1(XERR_ARGUMENT,"CRL entry context must be version 1 or 2");
    *output=(xx509crlversion)value;return true;
}
const xx509crl* xl6X509ProfileCrlV1(void* raw)
{return xl6X509ProfileCheckedV1(raw)?&((XL6_X509CrlV1*)raw)->view:NULL;}
bool xl6X509CrlProfileExtensionV1(const xx509crl* crl,uint8 last,xbytesview* output)
{
    const uint8 oid[]={0x55,0x1d,last};xx509ext extension;
    if(!xl6X509OutcomeV1(xrtX509ExtensionListFind(crl->Extensions,oid,sizeof(oid),&extension)))return false;
    *output=extension.Value;return true;
}
