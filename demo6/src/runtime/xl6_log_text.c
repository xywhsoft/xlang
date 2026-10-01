#include "src/runtime/xl6_log_record.h"
#include <limits.h>

bool xl6LogTextConfig(xlogtextconfig* config,int64_t style,int64_t flags,int64_t offset)
{
    if (style<0 || style>2 || flags < -1 || (uint64_t)(flags<0 ? 0:flags)>UINT32_MAX || offset<-86399 || offset>86399) {
        xrtSetErrorKind(XERR_RANGE); return false;
    }
    if (!xrtLogTextConfigInit(config,(xlogtextstyle)style)) return false;
    if (flags>=0) config->Flags=(uint32)flags;
    config->UtcOffset=(int)offset;
    return xrtLogTextConfigValidate(config);
}
bool xl6LogTextValidateV1(int64_t style,int64_t flags,int64_t offset)
{ xlogtextconfig config; return xl6LogTextConfig(&config,style,flags,offset); }
xstrview xl6LogTextV1(void* handle,int64_t style,int64_t flags,int64_t offset)
{
    xlogtextconfig config; xstrview result={0};
    if (!handle) { xrtSetErrorKind(XERR_ARGUMENT); return result; }
    if (!xl6LogTextConfig(&config,style,flags,offset)) return result;
    result.Data=xrtLogText(&((xl6_log_record*)handle)->record,&config,&result.Size);
    return result;
}
int64_t xl6LogTextBufferV1(void* record,void* buffer,int64_t style,int64_t flags,int64_t offset)
{
    if (!buffer) { xrtSetErrorKind(XERR_ARGUMENT); return -1; }
    xstrview text=xl6LogTextV1(record,style,flags,offset);
    if (!text.Data) return -1;
    if (text.Size>INT64_MAX) { xrtFree((void*)text.Data); xrtSetErrorKind(XERR_RANGE); return -1; }
    bool ok=xrtBufferAppend(buffer,(xbytesview){(const uint8*)text.Data,text.Size});
    xrtFree((void*)text.Data);
    return ok ? (int64_t)text.Size : -1;
}
