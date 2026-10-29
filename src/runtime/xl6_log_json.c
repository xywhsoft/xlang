#include "src/runtime/xl6_log_record.h"
#include <limits.h>

bool xl6LogJsonConfig(xlogjsonconfig* config,int64_t flags,int64_t escape,int64_t fields,int64_t nonfinite,int64_t depth)
{
    if (flags < -1 || (uint64_t)(flags<0 ? 0:flags)>UINT32_MAX || escape<0 || (uint64_t)escape>UINT32_MAX || fields<0 || fields>1 || nonfinite<0 || nonfinite>2 || depth<0 || (uint64_t)depth>SIZE_MAX) {
        xrtSetErrorKind(XERR_RANGE); return false;
    }
    if (!xrtLogJsonConfigInit(config)) return false;
    if (flags>=0) config->Flags=(uint32)flags;
    config->EscapeFlags=(uint32)escape;
    config->FieldStyle=(xlogjsonfieldstyle)fields; config->NonFinite=(xlogjsonnonfinite)nonfinite;
    config->MaxErrorDepth=(size_t)depth;
    return xrtLogJsonConfigValidate(config);
}
bool xl6LogJsonValidateV1(int64_t flags,int64_t escape,int64_t fields,int64_t nonfinite,int64_t depth)
{ xlogjsonconfig config; return xl6LogJsonConfig(&config,flags,escape,fields,nonfinite,depth); }
xstrview xl6LogJsonV1(void* handle,int64_t flags,int64_t escape,int64_t fields,int64_t nonfinite,int64_t depth)
{
    xlogjsonconfig config; xstrview result={0};
    if (!handle) { xrtSetErrorKind(XERR_ARGUMENT); return result; }
    if (!xl6LogJsonConfig(&config,flags,escape,fields,nonfinite,depth)) return result;
    result.Data=xrtLogJson(&((xl6_log_record*)handle)->record,&config,&result.Size);
    return result;
}
int64_t xl6LogJsonBufferV1(void* record,void* buffer,int64_t flags,int64_t escape,int64_t fields,int64_t nonfinite,int64_t depth)
{
    if (!buffer) { xrtSetErrorKind(XERR_ARGUMENT); return -1; }
    xstrview text=xl6LogJsonV1(record,flags,escape,fields,nonfinite,depth);
    if (!text.Data) return -1;
    if (text.Size>INT64_MAX) { xrtFree((void*)text.Data); xrtSetErrorKind(XERR_RANGE); return -1; }
    bool ok=xrtBufferAppend(buffer,(xbytesview){(const uint8*)text.Data,text.Size});
    xrtFree((void*)text.Data);
    return ok ? (int64_t)text.Size : -1;
}
