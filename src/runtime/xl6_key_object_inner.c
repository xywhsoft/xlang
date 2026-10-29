#include "xl6_key_object_internal.h"

void* xl6KeyObjectFromInnerV1(const XL6_KeyImportV1* spec,xbytesview input,
    bool outerCurve,xbytesview outerPublic)
{
    xbytesview secret,innerPublic;void* object=NULL;
    if (!xl6KeyObjectOutcomeV1(spec->Decode(input,spec->algorithm,outerCurve,&secret,&innerPublic))) return NULL;
    object=spec->Create(secret);
    if (xl6KeyObjectOutcomeV1(object!=NULL)) {
        xbytesview actual=spec->Public(object);
        if (xl6KeyObjectOutcomeV1(xl6KeyPublicShapeV1(spec->algorithm,actual) &&
                xl6KeyObjectPublicMatchesV1(innerPublic,actual) && xl6KeyObjectPublicMatchesV1(outerPublic,actual))) return object;
    }
    spec->Destroy(object);return NULL;
}
bool xl6KeyObjectWriteInnerV1(const XL6_KeyExportV1* spec,void* object,bool withPublic,xbuffer* output)
{
    if (!object) return xl6KeyFormatFailV1(XERR_ARGUMENT,XL6_KEY_FORMAT_MATERIAL_V1,"missing key receiver");
    xbytesview secret=spec->Private(object),publicKey={0};
    if (withPublic) publicKey=spec->Public(object);
    return xl6KeyObjectOutcomeV1(!xrtGetError() && spec->Encode(secret,publicKey,spec->algorithm,output));
}
