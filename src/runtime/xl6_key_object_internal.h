#ifndef XL6_KEY_OBJECT_INTERNAL_H
#define XL6_KEY_OBJECT_INTERNAL_H
#include "xl6_key_format_internal.h"
#include "xl6_crypto_internal.h"
#include "xl6_crypto_key_spans.h"

/* These are immutable resident descriptors, never supplied by the language
 * or retained by a key. All callbacks run synchronously while their actual
 * argument/receiver owner lives. Import/export contracts are separate so a
 * selected writer does not require a parser or unknown callback certificate. */
typedef struct XL6_KeyImportV1 {
    XL6_KeyAlgorithmV1 algorithm;
    void* (*Create)(xbytesview);
    void (*Destroy)(void*);
    xbytesview (*Public)(const void*);
    bool (*Decode)(xbytesview,XL6_KeyAlgorithmV1,bool,xbytesview*,xbytesview*);
} XL6_KeyImportV1;
typedef struct XL6_KeyExportV1 {
    XL6_KeyAlgorithmV1 algorithm;
    xbytesview (*Private)(const void*);
    xbytesview (*Public)(const void*);
    bool (*Encode)(xbytesview,xbytesview,XL6_KeyAlgorithmV1,xbuffer*);
} XL6_KeyExportV1;

bool xl6KeyObjectInputV1(const xvalue*,xbytesview*);
bool xl6KeyObjectOutcomeV1(bool);
bool xl6KeyObjectPublicMatchesV1(xbytesview,xbytesview);
xvalue* xl6KeyObjectOwnedValueV1(xvalue*);
xvalue* xl6KeyObjectValueV1(xbuffer*,bool);
void* xl6KeyObjectFromInnerV1(const XL6_KeyImportV1*,xbytesview,bool,xbytesview);
bool xl6KeyObjectWriteInnerV1(const XL6_KeyExportV1*,void*,bool,xbuffer*);
void* xl6KeyObjectFromPkcs8V1(const XL6_KeyImportV1*,xbytesview);
bool xl6KeyObjectWritePkcs8V1(const XL6_KeyExportV1*,void*,bool,xbuffer*);
bool xl6KeyObjectCurveDecodeV1(xbytesview,XL6_KeyAlgorithmV1,bool,xbytesview*,xbytesview*);
bool xl6KeyObjectCurveEncodeV1(xbytesview,xbytesview,XL6_KeyAlgorithmV1,xbuffer*);

void* xl6KeyObjectFromPemV1(xstrview,const char*,void* (*)(xbytesview),void (*)(void*));
xstrview xl6KeyObjectToPemV1(void*,bool,const char*,bool (*)(void*,bool,xbuffer*));
xstrview xl6KeyObjectBufferPemV1(xbuffer*,bool,const char*);
xvalue* xl6KeyPublicFromSpkiSpanV1(xbytesview,XL6_KeyAlgorithmV1);
xvalue* xl6KeyPublicFromSpkiV1(const xvalue*,XL6_KeyAlgorithmV1);
bool xl6KeyPublicWriteSpkiV1(const xvalue*,XL6_KeyAlgorithmV1,xbuffer*);
xvalue* xl6KeyPublicToSpkiV1(const xvalue*,XL6_KeyAlgorithmV1);
xvalue* xl6KeyPublicFromSpkiPemV1(xstrview,XL6_KeyAlgorithmV1);
xstrview xl6KeyPublicToSpkiPemV1(const xvalue*,XL6_KeyAlgorithmV1);
#endif
