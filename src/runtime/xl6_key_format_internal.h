#ifndef XL6_KEY_FORMAT_INTERNAL_H
#define XL6_KEY_FORMAT_INTERNAL_H
#include "include/xruntime_decl.h"
#include <string.h>

/* Resident wire-format support only. No private scalar arithmetic, dispatch
 * from guessed key widths, borrowed language result or new ownership family.
 * Views borrow the caller's complete input for this synchronous call. Writers
 * append exactly one DER value, atomically, to an independent output buffer. */
enum { XL6_KEY_DER_LIMIT_V1 = 16777216, XL6_KEY_TEXT_LIMIT_V1 = 33554432 };
typedef enum XL6_KeyAlgorithmV1 {
    XL6_KEY_RSA_V1 = 1, XL6_KEY_P256_V1, XL6_KEY_P384_V1,
    XL6_KEY_ED25519_V1, XL6_KEY_X25519_V1, XL6_KEY_X448_V1
} XL6_KeyAlgorithmV1;
typedef struct XL6_KeySpecV1 {
    xbytesview algorithm, parameters;
    size_t privateSize, publicSize;
    bool nullParameters, ecPoint;
} XL6_KeySpecV1;
typedef enum XL6_KeyFormatErrorV1 {
    XL6_KEY_FORMAT_SCHEMA_V1 = 1, XL6_KEY_FORMAT_ALGORITHM_V1,
    XL6_KEY_FORMAT_VERSION_V1, XL6_KEY_FORMAT_MISMATCH_V1,
    XL6_KEY_FORMAT_LIMIT_V1, XL6_KEY_FORMAT_MATERIAL_V1
} XL6_KeyFormatErrorV1;

const XL6_KeySpecV1* xl6KeySpecV1Get(XL6_KeyAlgorithmV1);
bool xl6KeyFormatFailV1(xerrkind,XL6_KeyFormatErrorV1,const char*);
bool xl6KeyInputV1(xbytesview);
bool xl6KeyOpenV1(xbytesview,xdercursor*);
bool xl6KeyDoneV1(const xdercursor*);
bool xl6KeyReadAlgorithmV1(xdercursor*,XL6_KeyAlgorithmV1);
bool xl6KeyWriteAlgorithmV1(xbuffer*,XL6_KeyAlgorithmV1);
bool xl6KeyPublicShapeV1(XL6_KeyAlgorithmV1,xbytesview);
void xl6KeyFreeV1(void*,size_t);
void xl6KeyBufferFreeV1(xbuffer*);

bool xl6KeySpkiReadV1(xbytesview,XL6_KeyAlgorithmV1,xbytesview*);
bool xl6KeySpkiWriteV1(xbytesview,XL6_KeyAlgorithmV1,xbuffer*);
/* PKCS#8/RFC5958 DER wrapper. Key-specific inner encoding is handled by
 * RSA/SEC1/RFC8410 adapters. An optional public key is borrowed separately,
 * so the managed factory MUST compare it with the derived actual key. */
bool xl6KeyPkcs8ReadV1(xbytesview,XL6_KeyAlgorithmV1,xbytesview*,xbytesview*);
bool xl6KeyPkcs8WriteV1(xbytesview,xbytesview,XL6_KeyAlgorithmV1,xbuffer*);
bool xl6KeyRsaPublicReadV1(xbytesview,xbytesview[2]);
bool xl6KeyRsaPublicWriteV1(const xbytesview[2],xbuffer*);
bool xl6KeyRsaPrivateReadV1(xbytesview,xbytesview[8]);
bool xl6KeyRsaPrivateWriteV1(const xbytesview[8],xbuffer*);
bool xl6KeySec1ReadV1(xbytesview,XL6_KeyAlgorithmV1,bool,xbytesview*,xbytesview*);
bool xl6KeySec1WriteV1(xbytesview,xbytesview,XL6_KeyAlgorithmV1,xbuffer*);
bool xl6KeyCurvePrivateReadV1(xbytesview,XL6_KeyAlgorithmV1,xbytesview*);
bool xl6KeyCurvePrivateWriteV1(xbytesview,XL6_KeyAlgorithmV1,xbuffer*);

/* Strict single PEM object. Plaintext DER/text remains owned by the caller;
 * private intermediates must be wiped using the actual allocation capacity. */
bool xl6KeyPemDecodeV1(xstrview,const char*,bytes*,size_t*);
bool xl6KeyPemEncodeV1(xbytesview,const char*,str*,size_t*);
#endif
