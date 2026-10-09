#ifndef XL6_RESTORE_CLASS_H
#define XL6_RESTORE_CLASS_H
#include "xl6_persistence_restore_plan.h"
/* Optional exact resident V2/V3 class lifetime admission; no substitute
 * class storage, finalization duty, code lease or new ownership policy. */
XL6_TEXT_API const XL6_RestoreObjectProviderV1* xl6RestoreClassProviderV1(void);
/* Smaller closed class/scalar/Array read domain. Mixed private List graph
 * restoration selects the provider above and its separate native component. */
XL6_TEXT_API const XL6_RestoreObjectProviderV1* xl6RestoreClassClosedProviderV1(void);
#endif
