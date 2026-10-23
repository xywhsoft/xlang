#include "xl6_der_internal.h"
int64_t xl6X509DerValueTimeV1(void* raw)
{
    const XL6_DerValue* self=xl6DerChecked(raw);xtime result=0;
    return self && xrtX509TimeParse(self->value.Raw,&result)?result:0;
}
