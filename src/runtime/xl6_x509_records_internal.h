#ifndef XL6_X509_RECORDS_INTERNAL_H
#define XL6_X509_RECORDS_INTERNAL_H
#include "xl6_x509_internal.h"
#include <stddef.h>

/* Every record owns only its encoded fragment. Lists additionally own a
 * parsed index in the same allocation. A small child never pins a whole
 * certificate/CRL, and no interior view crosses the language boundary. */
typedef struct XL6_X509NameV1 {size_t count;xbytesview raw;xx509nameattr* items;} XL6_X509NameV1;
typedef struct XL6_X509AttributeV1 {xx509nameattr value;} XL6_X509AttributeV1;
typedef struct XL6_X509ExtensionsV1 {size_t count;xbytesview raw;xx509ext* items;} XL6_X509ExtensionsV1;
typedef struct XL6_X509ExtensionV1 {xx509ext value;} XL6_X509ExtensionV1;
typedef struct XL6_X509AlgorithmV1 {xx509algorithm value;} XL6_X509AlgorithmV1;
typedef struct XL6_X509SignatureInfoV1 {xx509signature value;} XL6_X509SignatureInfoV1;
typedef struct XL6_X509PublicKeyInfoV1 {xx509pubkey value;xbytesview raw;} XL6_X509PublicKeyInfoV1;

void* xl6X509RecordAllocV1(size_t,size_t,size_t,size_t,xbytesview,void**,xbytesview*);
bool xl6X509RebaseV1(xbytesview,xbytesview,xbytesview,xbytesview*);
bool xl6X509IndexV1(int64_t,size_t,size_t*);
void xl6X509BufferFreeV1(xbuffer*);
bool xl6X509OidInputV1(const xvalue*,xbytesview*);
bool xl6X509OidEncodeV1(xstrview,xbuffer*);
xstrview xl6X509OidTextV1(xbytesview);
XL6_X509NameV1* xl6X509NameCopyV1(xbytesview,int64_t);
XL6_X509AttributeV1* xl6X509AttributeCopyV1(const xx509nameattr*);
bool xl6X509AttributeValueSpanV1(void*,xbytesview*);
XL6_X509ExtensionsV1* xl6X509ExtensionsCopyV1(xbytesview,int64_t);
XL6_X509ExtensionV1* xl6X509ExtensionCopyV1(const xx509ext*);
XL6_X509AlgorithmV1* xl6X509AlgorithmCopyV1(const xx509algorithm*);
XL6_X509PublicKeyInfoV1* xl6X509PublicKeyInfoCopyV1(xbytesview,const xx509pubkey*);

#define XL6_X509_RECORD_CHECK(Type,Name) \
    static inline const Type* Name(void* raw) { \
        if(!raw)xl6X509FailV1(XERR_ARGUMENT,"missing X.509 record receiver"); \
        return raw; \
    }
XL6_X509_RECORD_CHECK(XL6_X509NameV1,xl6X509NameCheckedV1)
XL6_X509_RECORD_CHECK(XL6_X509AttributeV1,xl6X509AttributeCheckedV1)
XL6_X509_RECORD_CHECK(XL6_X509ExtensionsV1,xl6X509ExtensionsCheckedV1)
XL6_X509_RECORD_CHECK(XL6_X509ExtensionV1,xl6X509ExtensionCheckedV1)
XL6_X509_RECORD_CHECK(XL6_X509AlgorithmV1,xl6X509AlgorithmCheckedV1)
XL6_X509_RECORD_CHECK(XL6_X509SignatureInfoV1,xl6X509SignatureInfoCheckedV1)
XL6_X509_RECORD_CHECK(XL6_X509PublicKeyInfoV1,xl6X509PublicKeyInfoCheckedV1)
#undef XL6_X509_RECORD_CHECK
#endif
