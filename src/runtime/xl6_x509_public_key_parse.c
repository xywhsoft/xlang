#include "xl6_x509_records_internal.h"
void* xl6X509PublicKeyInfoFromSpkiV1(const xvalue* value,int64_t limit)
{
    xbytesview input;xx509pubkey key;
    if(!xl6X509InputV1(value,&input) || !xl6X509LimitV1(input.Size,limit,"SPKI exceeds input budget") ||
        !xl6X509OutcomeV1(xrtX509PublicKeyParse(input,&key)))return NULL;
    return xl6X509PublicKeyInfoCopyV1(input,&key);
}
