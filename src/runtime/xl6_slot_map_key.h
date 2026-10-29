#ifndef XL6_SLOT_MAP_KEY_H
#define XL6_SLOT_MAP_KEY_H
#include "xl6_slot_map.h"
/* Private native record ownership, not source-language API. This component
 * has no dependency on the SlotMap storage algorithm or generated code. */
XL6_TEXT_API XL6_SlotMapInstanceV1* xl6SlotMapInstanceCreateV1(void);
XL6_TEXT_API bool xl6SlotMapInstanceRetainV1(XL6_SlotMapInstanceV1*);
XL6_TEXT_API void xl6SlotMapInstanceDropV1(XL6_SlotMapInstanceV1*);
#endif
