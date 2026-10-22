#include "xl6_key_object_internal.h"

void* xl6KeyObjectFromPkcs8V1(const XL6_KeyImportV1* spec,xbytesview input)
{
    xbytesview secret,publicKey;
    if (!xl6KeyObjectOutcomeV1(xl6KeyPkcs8ReadV1(input,spec->algorithm,&secret,&publicKey))) return NULL;
    return xl6KeyObjectFromInnerV1(spec,secret,true,publicKey);
}
bool xl6KeyObjectWritePkcs8V1(const XL6_KeyExportV1* spec,void* object,bool withPublic,xbuffer* output)
{
    xbuffer encoded={0};xbytesview publicKey={0};
    /* RFC5915's inner EC public field is retained. withPublic selects the
     * separate RFC5958 outer field/version, not an undocumented curve guess. */
    bool ok=xl6KeyObjectWriteInnerV1(spec,object,true,&encoded);
    if (ok && withPublic) {publicKey=spec->Public(object);ok=xl6KeyObjectOutcomeV1(!xrtGetError());}
    if (ok) ok=xl6KeyPkcs8WriteV1(xrtBufferView(&encoded),publicKey,spec->algorithm,output);
    xl6KeyBufferFreeV1(&encoded);return xl6KeyObjectOutcomeV1(ok);
}
