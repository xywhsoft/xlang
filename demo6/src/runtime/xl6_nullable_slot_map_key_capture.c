#include "xl6_nullable_slot_map_key_capture.h"
#include "xl6_nullable_slot_map_key.h"
#include "src/type/xl6_native_value_ids.h"
#include <stdlib.h>
static void detach_cleanup(void* raw,unsigned char* live)
{
    XL6_NullableSlotMapKeyV1* slot=raw;
    XL6_NullableSlotMapKeyV1 owned;
    xrtownershipscope mutation={0};
    if(!xrtOwnershipMutationBegin(&mutation))abort();
    owned=*slot;*slot=(XL6_NullableSlotMapKeyV1){0};if(live)*live=0;
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    xl6NullableSlotMapKeyDropValueV1(owned);
}
const XL6_NativeCaptureProfileV1* xl6NullableSlotMapKeyCaptureProfileV1Get(void)
{
    static const XL6_NativeCaptureProfileV1 profile={sizeof(profile),
        XL6_NULLABLE_SLOT_MAP_KEY_LANGUAGE_VALUE_ID,xl6NullableSlotMapKeyTypeV1,detach_cleanup};
    return &profile;
}
