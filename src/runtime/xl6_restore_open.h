#ifndef XL6_RESTORE_OPEN_H
#define XL6_RESTORE_OPEN_H
#include "xl6_persistence_restore_plan.h"
/* Optional exact resident Class + DynamicFields preparation/admission.
 * Does not grant clone rights to arbitrary classes or resource handles. */
XL6_TEXT_API const XL6_RestoreObjectProviderV1* xl6RestoreOpenProviderV1(void);
#endif
