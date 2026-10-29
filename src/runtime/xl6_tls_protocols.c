#include "xl6_tls_role_context_internal.h"
#include <string.h>
bool xl6TlsProtocolsCopyV1(const XL6_TlsProtocolsV1* source,XL6_TlsProtocolsV1** output)
{
    if(xrtGetError())return false;
    if(!output || *output)return xl6TlsFailV1(XERR_ARGUMENT,"TLS protocol copy needs an empty output");
    if(!source)return true;
    size_t prefix=sizeof(*source)+source->count*sizeof(*source->views);
    XL6_TlsProtocolsV1* copy=xrtMalloc(prefix+source->bytes);if(!copy)return false;
    copy->count=source->count;copy->bytes=source->bytes;char* data=(char*)copy+prefix;
    for(size_t i=0;i<source->count;++i){
        copy->views[i]=(xstrview){data,source->views[i].Size};
        memcpy(data,source->views[i].Data,source->views[i].Size);data+=source->views[i].Size;
    }
    *output=copy;return true;
}
xvalue* xl6TlsProtocolsAtV1(const XL6_TlsProtocolsV1* protocols,int64_t index)
{
    if(xrtGetError())return NULL;
    if(!protocols || index<0 || (uint64_t)index>=protocols->count){
        xl6TlsFailV1(XERR_RANGE,"TLS protocol index is outside its ordered list");return NULL;
    }
    xstrview view=protocols->views[index];
    return xrtValueBytes((xbytesview){(const uint8*)view.Data,view.Size});
}
