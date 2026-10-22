#include "xl6_key_object_internal.h"

bool xl6KeyObjectCurveDecodeV1(xbytesview input,XL6_KeyAlgorithmV1 algorithm,
    bool outerCurve,xbytesview* secret,xbytesview* publicKey)
{
    (void)outerCurve;
    if (!xl6KeyCurvePrivateReadV1(input,algorithm,secret)) return false;
    *publicKey=(xbytesview){0};return true;
}
bool xl6KeyObjectCurveEncodeV1(xbytesview secret,xbytesview publicKey,
    XL6_KeyAlgorithmV1 algorithm,xbuffer* output)
{(void)publicKey;return xl6KeyCurvePrivateWriteV1(secret,algorithm,output);}
