#ifndef XL6_X509_IDENTITY_INTERNAL_H
#define XL6_X509_IDENTITY_INTERNAL_H
#include "xl6_x509_internal.h"
#include "xl6_text_internal.h"

/* DONE is an ordinary no-match, not an exception. ERROR must retain the
 * original diagnostic; an impossible status without one is an ABI failure. */
static inline bool xl6X509IdentityResultV1(xx509result result)
{
    return xl6X509OutcomeV1(result==X509_VALUE || result==X509_DONE);
}
#endif
