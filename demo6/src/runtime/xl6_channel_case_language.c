#include "xl6_text_value.h"
#include "xl6_channel.h"
#include "xl6_channel_case.h"
#include "xl6_channel_language_abi.h"
#include "src/api/xl_foreign_error.h"
#include <limits.h>

static void failed(void)
{ xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); }
void* xl6ChannelReceiveCaseLanguageV1(XL6_SequenceLayoutV1 layout,const xvalue* channel)
{
    return xl6ChannelCaseCreateSchemaV1(layout,channel,NULL,false);
}
void* xl6ChannelSendCaseLanguageV1(XL6_SequenceLayoutV1 layout,const xvalue* channel,const void* value)
{
    return xl6ChannelCaseCreateSchemaV1(layout,channel,value,true);
}
void xl6ChannelSelectionValueLanguageV1(XL6_SequenceLayoutV1 layout,const void* selection,
    const void* expected,const xvalue* channel,void* output)
{
    /* The schema's exact native type proves the result representation. The
     * selected case identity proves which operation supplied the result. */
    if(!xl6ChannelCaseMatchesSchemaV1(expected,channel,layout) ||
        !xl6ChannelSelectionValueV1(selection,expected,
        layout.elementIdentity,layout.type,(XL6_ChannelSlotV1){output,layout.size})) failed();
}
int64_t xl6ChannelSelectionIndexLanguageV1(const void* selection)
{
    XL6_ChannelSelectionV1 info={0};
    if(!xl6ChannelSelectionInfoV1(selection,&info)){failed();return -1;}
    if(info.index==SIZE_MAX)return -1;
    if(info.index>INT64_MAX){xrtSetErrorKind(XERR_RANGE);failed();return -1;}
    return (int64_t)info.index;
}
int64_t xl6ChannelSelectionStatusLanguageV1(const void* selection)
{
    XL6_ChannelSelectionV1 info={0};
    if(!xl6ChannelSelectionInfoV1(selection,&info)){failed();return -1;}
    return (int64_t)info.status;
}
