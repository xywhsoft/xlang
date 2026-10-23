#include "xl6_x509_records_internal.h"

static bool compare(void* raw,void* other,bool within)
{
    const XL6_X509NameV1* self=xl6X509NameCheckedV1(raw);const XL6_X509NameV1* base=xl6X509NameCheckedV1(other);
    if(!self || !base)return false;
    xx509result result=within?xrtX509NameWithin(self->raw,base->raw):xrtX509NameEqual(self->raw,base->raw);
    return xl6X509OutcomeV1(result!=X509_ERROR) && result==X509_VALUE;
}
bool xl6X509NameEqualV1(void* raw,void* other){return compare(raw,other,false);}
bool xl6X509NameWithinV1(void* raw,void* other){return compare(raw,other,true);}
