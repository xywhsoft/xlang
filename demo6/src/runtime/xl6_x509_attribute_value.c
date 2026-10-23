#include "xl6_x509_records_internal.h"
#include "xl6_der_internal.h"

void* xl6X509AttributeValueV1(void* raw)
{
    xbytesview input;if(!xl6X509AttributeValueSpanV1(raw,&input))return NULL;
    XL6_DerValue* result=xl6DerCopyValue(input);
    if(xl6X509OutcomeV1(result!=NULL))return result;
    xl6DerValueDestroyV1(result);return NULL;
}
