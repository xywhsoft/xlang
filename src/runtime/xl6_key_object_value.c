#include "xl6_key_object_internal.h"

bool xl6KeyObjectOutcomeV1(bool ok)
{
    if (!ok && !xrtGetError())
        xl6KeyFormatFailV1(XERR_STATE,XL6_KEY_FORMAT_SCHEMA_V1,"key operation failed without a diagnostic");
    return ok && !xrtGetError();
}
bool xl6KeyObjectInputV1(const xvalue* value,xbytesview* input)
{return xl6KeyObjectOutcomeV1(xl6CryptoMaterial(value,input) && xl6KeyInputV1(*input));}
bool xl6KeyObjectPublicMatchesV1(xbytesview supplied,xbytesview actual)
{
    if (!supplied.Data && !supplied.Size) return true;
    if (!supplied.Data || !actual.Data || supplied.Size!=actual.Size ||
        memcmp(supplied.Data,actual.Data,actual.Size))
        return xl6KeyFormatFailV1(XERR_VALUE,XL6_KEY_FORMAT_MISMATCH_V1,"encoded public key does not match the private key");
    return true;
}
xvalue* xl6KeyObjectOwnedValueV1(xvalue* value)
{
    if (xl6KeyObjectOutcomeV1(value!=NULL)) return value;
    xerror* primary=xrtTakeError();xrtValueRelease(value);
    xrtClearError();xrtSetErrorTake(primary);return NULL;
}
xvalue* xl6KeyObjectValueV1(xbuffer* output,bool ok)
{
    xvalue* value=NULL;bytes data=NULL;size_t size=0,capacity=0;
    if (xl6KeyObjectOutcomeV1(ok)) {
        data=xrtBufferTake(output,&size,&capacity);
        if (xl6KeyObjectOutcomeV1(data!=NULL || size==0)) value=xrtValueBytesTake(&data,size);
    }
    xl6KeyFreeV1(data,capacity);xl6KeyBufferFreeV1(output);return xl6KeyObjectOwnedValueV1(value);
}
